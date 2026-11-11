# FABLE-nodes-perf: performance review of DESIGN-nodes revision 2 (2026-11-11)

Read: D168-D182, DESIGN-nodes r2, RECON-nodes, PERF2, DIAG-house; code: junction_skeleton.cpp (whole), junction_line_router.h, junction_terrain.h, MakeJunctionTerrain / GenerateJunctionNear (junction_game.cpp:325, :416), the flat-T and height loop in ConstructJunctionTMap (junction_generator.cpp:895-1160).
"ASSUMPTION" marks what I did not verify in code.

## 0. Summary
The proposal is the right shape for speed: positions are a tiny integer system, structure is the only search. Speed at scale depends on three things:
(a) how few branches the structure search makes;
(b) whether terrain facts reach the solve as cheap bound and domain updates rather than as branches;
(c) how much work is shared across calls.
The main finding: almost every terrain fact, **once the structure is fixed**, is a *min-closed and max-closed* constraint. That means the position layer has a least and a greatest solution, found by monotone propagation with **no branching at all**. So the discrete search branches only on true structure: which row an edge is on, where S-bends, ramps, bridges and tunnels sit, how many bays a leg has. It never branches on "before or after the house".

## 1. Performance of the proposal

### 1.1 The continuous layer: propagation, not search
Today SolveJunctionSkeleton runs a full Bellman-Ford (FindCycle), then for every variable a full Bounds pass (V rounds x E constraints), then appends two constraints (junction_skeleton.cpp:41-112). That is O(V^2 E). At 10 variables this costs microseconds. With LL_RR or multi-line it is about 100 variables and about 400 constraints, so about 4M relaxations per solve. Inside a structure search that visits hundreds of nodes, it becomes milliseconds per hover. (Estimate, not measured.)

Replace it with:
- **Incremental difference propagation with a trail.** Keep `lo` (the least solution) and `up` (the greatest). Adding `x_a - x_b >= k` costs O(1) if `lo[a] >= lo[b] + k` already holds. Otherwise push `a` onto a FIFO and propagate along out-edges, touching only the variables that move. If the propagation raises `lo[b]` again, that is a negative cycle, so the constraint is infeasible. Record each change on a trail so a backtrack undoes it in O(changes). This is the standard difference-logic method in SMT solvers (Cotton-Maler style); deterministic with FIFO order.
- **Unary domains as bitsets.** A node whose footprint (switch tile, prio coverage, prio row, signal tile, bridge-head pair) is legal only at some x gets a domain bitset `D_v` over x. Propagation becomes `lo[v] = next_set_bit(D_v, max(lo[v], lo[b] + k))`. This stays monotone. Componentwise min of two solutions is again a solution: for `x_a >= x_b + k`, min keeps the inequality, and min of two members of `D_v` is a member. So the least solution exists whenever any solution exists, and the fixpoint finds it. The same holds for max and the greatest solution.
- **"Edge [x_a, x_b] on row r must not cover column c"** (the obstacle condition of §2, with the structure fixed so the edge is known) is also min-closed and max-closed, since x_a <= x_b. Least-solution rule: if `lo[a] <= c <= lo[b]`, raise `lo[a]` to `c+1`. Greatest-solution rule: if `up[a] <= c <= up[b]`, lower `up[b]` to `c-1`. No branch is needed for "pass before or after".
- **D144's push order** (each variable fixed at its extreme in turn) becomes one incremental add per variable. If every variable pushes Down, the answer is simply `lo`.

What is *not* min-closed, and so must be discrete (part of the structure, not of the positions):
- D176 whole bays, `x_b - x_a in {TL, 2TL, 3TL}`. Make the bay count a parameter of the edge type. Once it is chosen, the length is an exact difference equality.
- D148a's e_phase parity. Fix it per structure, as today.
- Anything with coefficient != 1. ASSUMPTION: §3.4's depth-as-a-variable gives constraints `x_end - x_start - y >= k`, which are difference constraints only if a diagonal changes one row per tile of x. If a diagonal is counted in half-units with coefficient 2, keep depth discrete: an outer loop of at most a few values, pruned by the lower bound in 1.3.

Measure: count solver relaxations and µs per structure on PERF2 a-f. On the house case (e), expect < 1 ms of total solve once the structure is right.

### 1.2 Facts: one table, queried in O(1)
Today MakeJunctionTerrain re-reads the map tile by tile for every hover. GenerateJunctionNear then copies the whole snapshot for each offset (`JunctionTerrain shifted = terrain;`, junction_game.cpp:438). On top of that, every edge query walks tiles. Build **per-tile facts once per terrain snapshot**, and at network scale once per map with incremental updates (see 3.1).
- Per tile, per axis (x-travel, y-travel), about 16 bits:
  - Straight edge heights (entry and exit z), straight on the incline, flat or level-foundation (switch-legal, D115a/G6), signal-legal.
  - Curve-legal per curve kind, span-ok bands, portal-legal (D155), class.
  - All derived from GetRailFoundation's rules (rail_cmd.cpp:311) as the design says.
- Per row and axis, derived indexes:
  - `run_id[x]`: maximal runs where consecutive straight pieces meet at matching edge heights. "Straight edge [a,b] on row r is buildable" is then `run_id[a] == run_id[b]`, in O(1).
  - `next_bad[x]` and `prev_bad[x]` per fact class (switch, signal, any track). These turn a node footprint or obstacle into a bound in O(1). They are exactly the `next_set_bit` used in 1.1.
- **Rigid shapes by sliding window.** A->S's curve-diagonal-curve descent and N->A's rise are rigid once their rows are fixed and the diagonal has its minimum length. For each shape, precompute its tile mask relative to its anchor node once per spec. Then, per snapshot, AND the mask against the fact rows to get the bitset of legal anchor x. Cost: O(width x mask tiles), about 80 x 30 per shape, microseconds. These bitsets are the `D_v` of 1.1. Branch edges, crossings (D172 span plus head heights equal plus clearance) and port approaches (D179) all become domains, not checks after the fact.
- **Do not leave optional extra diagonal length free** (§4 "diagonal (>= rows changed)"). A diagonal of variable length sweeps a region that depends on two variables, which brings back late discovery. Default it to the minimum. Extra length comes from an inserted straight edge, whose facts are 1D and indexed.

### 1.3 The discrete layer: conflict-directed, best-first, decomposed
- **Start state.** The precompiled flat structure for the spec (3.2), with its flat solution as the cost lower bound.
- **Expand only on a failure explanation.** When propagation empties a domain or crosses `lo > up`, the trail names the constraint chain, as FindCycle's CycleRules already does for cycles. Extend that to domain wipe-outs and record which fact (tile, fact class) sits in the chain. The children are only the repairs for that fact. Look the repairs up in a per-spec table keyed by where the fact sits:
  - in a straight edge on row r: S-bend to r±1 before it, then shift back after it or stay shifted (D182);
  - in a node footprint: nothing; the domain handles it (1.1);
  - in a crossing window: other realisation (D171), tunnel or the other doubling;
  - under a building: tunnel edge (D175).
  This is the "obstacles add structure" ruling turned into a branching rule. Because the trigger is a named fact, there is no blind enumeration.
- **Order.** Best-first on `bound = flat cost + sum of each added edge's minimum cost` (S-bend minimum straight lengths, tunnel portal pair, and so on). Ties break on a canonical structure key, for determinism. Most-constrained line first: the main lines carry prio coverage and crossings.
- **Nogoods.** When a subtree fails for (line, local structure between two nodes, fact), store it. Siblings that differ elsewhere skip it. This is cheap, and it is what stops the product blow-up across lines.
- **Group obstacles by a rule-derived gap, not by outlines** (§7 open item). On a row, merge two forbidden intervals when the gap between them is shorter than the shortest shift-and-return (S-bend + N straight + S-bend). A line cannot get back between them anyway. The gap is derived from the edge rule table, so it is not padding. Bounding boxes over-block and outlines branch for no gain; per-row merged intervals are exact and 1D.
- **Bundle parallel lanes** (B's two lanes now; LL_RR's 2+2 next). A lane can only shift if its sibling shifts with it or out of its way. Give the discrete layer a *bundle* edge type: k parallel lanes on adjacent rows, one S-bend that moves the whole bundle, and one lane-split or lane-merge node. Otherwise the search explores lane combinations that are infeasible by construction. Branching then grows with the number of bundles, not lanes.
- **Hard bounds, stated as rules, not budgets.** At most m added structural edges per line, with m from the box geometry: rows available / one row per S-bend. Prune any node whose bound exceeds the incumbent. The search has a fixed maximum depth, so worst-case time is bounded and deterministic.

### 1.4 Box depth, heights and offsets: fold the outer loops in
PERF2 (e) spends 11.85 s in route over 33 heights x 17 offsets. DESIGN §3.4 already removes the height loop.
- Do the same for offsets. The hub x offset is a translation shared by every node on the axis: add it as one variable `t` with `|t| <= offset limit`. The x part of the offset search is then a single solve, and `[lo[t], up[t]]` is the set of x offsets that work, returned for free.
- The y offsets stay discrete. ASSUMPTION: JunctionNearOffsets is 17 offsets on a small 2D pattern (DIAG-house says "17 nearby spots"; PERF2 found offsets like (-4,-4) and (-2,2)). That gives about 5 structure searches per hover instead of 17 full generations.
- **Warm start across cursor moves and rotations.** Keep the last solved structure per (spec, orientation). On the next hover, try it first: re-run propagation on the new domains, which takes microseconds. Search only if it fails, and then seed best-first with its nogoods. Cursor jitter of one tile almost always keeps the structure.
- **Rotations:** build the fact table in map coordinates with both axes. An orientation is then an index transform (swap axis bits, mirror), not a new snapshot.

## 2. What will not scale or brings back old problems
1. **Lay-out that can still fail.** §2 rules that a router failure on a laid-out edge is a model bug. That is only true if every fact the layout touches went into the solve. Watch for:
   - (a) prio rows: not terrain-checked in the line-router path today (RECON "surprising facts"; finish with terrain nullptr, junction_generator.cpp:1692);
   - (b) signal slots: the signal pass decides adjacency after routing (D148a e_phase);
   - (c) bridge head heights and clearance on terrain;
   - (d) the 2D sweep of diagonals.
   Fix: every one of these is part of a node or edge type's footprint, enters as a domain (1.2), and has a test-only assertion that layout re-derives the same facts. Signals: give each edge type its signal slots, so block boundaries are known in the solve. Derive e_phase per spec at precompile time.
2. **Inferred nodes.** The S merge and N split (N4, N5), the X3 crossing (N12) and B's shift-back (N10) must be explicit nodes in the chain with their own types. Otherwise they come back as "plain pieces of a routed raster" (RECON §3) and the 14 dependencies return.
3. **Retry loops in disguise.** NW1's `d = 0..tl` retry (RECON N10) and D179a's widen-and-reroute are both loops over a quantity the solve should own. k_sb + d is just `x_nw - x_sb >= k_sb` with no upper bound, and the ports are variables bounded by the frame. Neither may survive as an outer loop.
4. **Per-structure full rule check.** The full checker costs 8-20 ms per success (PERF2 "check"), which is a fifth of the budget. Run it in tests and under a debug flag. On the hot path, check only what the model does not encode: ideally nothing.
5. **Non-incremental solver** (1.1). This is fine today and quadratic later.
6. **Search that branches on facts.** If "obstacle in straight edge" were split into "S-bend before" and "S-bend after" as separate structures, the search would grow with the number of obstacles times the number of nodes. 1.1 makes before/after a position question. Keep it out of the structure key.

## 3. Network scale (thousands of calls, LLM-driven)
### 3.1 Persistent world facts
- One fact table for the map: 512x1024 = 524k tiles x about 4 bytes (2 axes x 16 bits) = about 2 MB, plus per-row run and next-bad indexes. It is updated incrementally from tile-change events (commands that build or clear, terraform, town growth). Mark rows dirty and rebuild their indexes lazily.
- A generation call reads an immutable epoch of it: copy-on-write per 64-row band. Calls can then run on worker threads in parallel and stay deterministic, because each sees one epoch.
- Keep a rolling hash per row band, so each call can cheaply hash the window under its footprint.

### 3.2 Precompile per junction type and spec (terrain-free)
Store:
- the node graph and typed edge chains;
- the constraint template with spec constants (prio_tiles, TL, nmax, curve spacing, k_* of D145) resolved;
- the flat structure and its solution;
- every rigid shape mask;
- the repair table (1.3);
- e_phase and bay alternatives;
- the minimum core footprint.
This replaces the flat-T run in the hot path. PERF2 shows 21-33 ms for "flat" in every row. The GUI caches it (junction_gui.cpp:674-676), but the bench and any non-GUI caller pay it. ASSUMPTION: an LLM batch API would too, unless this is precompiled. Precompile all common specs at startup or lazily, keyed by spec hash.

### 3.3 Batch and site queries
- **Site map.** For (type, spec, orientation) over a region, run the cheap necessary tests as sliding windows over the fact table: port approaches (D179), core footprint, at least one legal x in each node's domain. The result is a bitmap of candidate anchors. One call answers "where along this line can an L_R T go?" with a ranked list, so callers do not probe 17 x N cursors.
- **`GenerateMany(type, spec, [anchor, orientation]...)`.** It shares the epoch, the precompile and the warm-start cache. Memoise results by (type, spec hash, orientation, anchor, footprint-window hash) in an LRU, so identical LLM retries are free.

### 3.4 Results built so the caller does not call again
Return:
- nodes with positions **and slack** (`lo..up` per node, including the offset variable `t`);
- edge types per line;
- ports with tile, direction, height and the straight run required before them, so the line painter can connect without guessing;
- cost and footprint.
On failure, return the **explanation chain** from 1.3: which fact (tile, class) defeated which node or edge rule, plus the feasible offsets if any. "Move the cursor" (D179) becomes "move 3 east" with the reason. An LLM can act on that in one step.

## 4. Where the A* line router belongs
- **Between junctions (the line painter):** yes. It is the right tool for long, free-shape track. Make it use the same fact table (3.1). Bound it with a corridor: run a coarse A* on 8x8 macro-tiles from the facts, then a fine A* inside the corridor band. Size the expansion budget from the corridor area rather than the global 200000 (junction_line_router.h:76). Return the relaxed-reachability verdict (JunctionRelaxedReachable) before any fine search.
- **Inside a junction:** not as a search. Lay-out of a typed edge between fixed ends is construction. Keep the router as a **test oracle**: in tests, route each laid-out edge with the router under the same facts. Any disagreement is the model bug §2 talks about.
- **One exception worth keeping open:** a long branch leg out to a port across rough ground. Model it as a line-painter edge that leaves the junction at a port node, outside the junction's solve, rather than routing it inside the junction.

## 5. Top 5 by expected payoff
1. **Monotone propagation layer** (incremental difference constraints + domain bitsets + min/max-closed "avoid" constraints, with trail undo). It removes all branching on positions. Measure: structures explored and µs per solve on PERF2 a-f; target case (e) < 5 ms total.
2. **Per-tile fact table with per-row run and next-bad indexes and sliding-window shape domains**, map-coordinate, both axes, incremental. Measure: snapshot build µs, fact queries per hover, PERF2 (c)/(f) route time, which should drop to about 0 since nothing is routed.
3. **Per-spec precompile** (flat structure, constraint template, masks, repair table) and **hot-path check reduced to tests only**. Measure: PERF2 "flat" (21-33 ms) and "check" (8-20 ms) columns should go to about 0.
4. **Fold offsets, heights and ports into the solve** (translation variable `t`, depth as a variable or a tiny pruned loop, ports as frame-bounded variables) **plus warm start from the last hover**. Measure: full generations per hover (17 x heights today, target <= 5) and p99 hover time over a cursor sweep of a cluttered region.
5. **Conflict-directed best-first structure search with nogoods, bundle edges for parallel lanes and rule-derived obstacle grouping.** Measure: distribution (p50/p99/max) of structures explored over a corpus of about 1000 random anchors on a real 512x1024 save, for L_R now and LL_RR when it lands. It must stay flat as clutter grows.

## 6. On the rulings (§2)
I see no ruling that is wrong for performance. Typed edges, no detour pairs, obstacles add structure, no segment-failure outcome and heights as tile facts are exactly what make 1.1 and 1.2 possible. One caution sits within the rulings, not against them: §4's "diagonal (>= rows changed)" with optional extra length is the one edge type whose facts are 2D in two variables. Keep it rigid (1.2) and get extra length from adjacent straight edges.
