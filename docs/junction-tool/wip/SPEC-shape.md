# SPEC: the Shape layer's hand-off (draft, 2026-11-20, orchestrator; for the engineer)

Second SSS hand-off spec (D368). Implements src/junction/planar/CLAUDE.md "Architecture" (Shape) and DESIGN-tracks §17.2. Written from the contracts, the track map and wip/SPEC-structure.md, not from the code; code is mapped onto it afterwards (D368). Rulings it rests on: D340 (shape program: faces turn once, least turning), D342 (crossers may bend), D349 (a row follows its lane as an offset copy through bends), D351 (parallelogram), D360 (no fork, merge or spread diagonals; Slots owns sideways moves), D362, D364-D372, D228a, D346. Game facts: `GetTunnelBridgeLength` counts middle tiles for both kinds; `CheckBridgeAvailability` caps a bridge at min(type max_length, max_bridge_length); `CmdBuildTunnel` caps middles at max_tunnel_length.

## 0. The first questions, answered from the contracts
- **Rows (and copies): yes, Shape outputs them, but decides nothing for them.** Slots needs every physical track headed (D366: it reads only Shape's model), and a structure's capacity counts them (they are crossers, S2c1 ruling). Their headings are derived: a row or copy has its parent's headings over its range (D349 for rows; a copy sits on its separator's axis piece, D351). A mirrored crossing sits on the same piece of the crosser as the original. So Shape solves on the **base tracks** only (topology lines and their parts) and copies headings to beside tracks: the model stays the size of the topology.
- **Faces and sides: yes, Shape reads both; Structure already promises them (P1, P2).** Sides give: the arrival side s of each crossing (the crossing rule below), each non-straight leg's turn side at a node, and the order of parallel legs (which Slots keeps). Faces give the face rule: every inner face winds exactly once (D340), the one heading-only condition that stops Lengths from meeting a shape that cannot close. Shape does not decide sides or faces; it never changes them (D367).

## 1. Input
- One Structure variant (SPEC-structure §3, §6): the track map (tracks, ends, events in travel order, faces, sides per stretch pair, the expanded topology with port headings), sites (straight leg and side), structures (separator, crossings, crossers, allowed kinds, copy), Structure cost.
- **Added to Structure's output (D366, orchestrator default):** each structure's limit per allowed kind in middle tiles: bridge min(longest bridge, and for a main-line separator 2d + tl - 2 from D228a, head to head B = middles + 2); tunnel min(max_tunnel_length, the same D228a bound for a main line, portal to portal). Structure computes these for P5 already; Shape must not re-read the spec's rule fields (they are Structure's input).
Nothing else. No lengths or positions exist yet.

## 2. What Shape decides (and nothing else)
Per variant, one shape (variants live only in Structure, D362; D340's "several shapes per variant" is superseded):
1. **Headings:** each base track's absolute heading at every point (its ends and each event), as a lifted integer (eighths, not reduced mod 8). A stretch turns **monotonically** from its start heading to its end heading, one eighth per piece. A turn and turn back between two events is a sideways move, so it is never Shape's: it is an S-bend, which belongs to Slots (D360, CLAUDE "makes no sideways moves").
2. **Crossing angles:** each crossing's angle a in 1..3 eighths, which follows from the two headings at the event.
3. **Each structure's axis:** the one axis heading its separator holds from first to last crossing.
Derived, not decided: beside tracks' headings; per structure the least middle tiles at the chosen angles, and the straight runs its ends need (§3).

### 2.1 Rules (the model)
- **Ports:** a port track's heading at its port end is the port's heading.
- **Nodes:** at a split with in-heading h, the straight leg starts at h; every other leg starts at h or h + its side (one eighth toward its side, never away), its side read against the straight leg (a row's split and merge, and the three-leg merge where a prio lane, its row and the joiner end, give several other legs). A fork's legs each start at h or h + their own side; which one keeps the in line's slot is Slots' (D360). Merges mirror this at their end. Legs that start parallel are ordered by their sides; Slots separates them.
- **Crossings:** at a crossing of separator p and crosser c arriving from side s: H_c - H_p = s*a + 8q, a in 1..3 (D340's rule, with separator for "over"). The separator's heading is an axis heading. Inside a structure, the separator's stretches between its crossings have turn 0. Two separators crossing (a stack, or a bridge over a tunnel line, P3/P4) are both axis there, so a = 2.
- **Mirrored crossings:** a copy's or row's k-th crossing mirrors its parent's k-th crossing (Structure's contract: beside tracks' crossings follow by rule from their parent's structure, so they are in no structure's list). It takes the parent's role: beside a separator it is a parallel deck of that structure (as a copy is); beside a crosser it is spanned by the same separator. Its angle equals the original's, and the crosser sits on one piece across both crossings; except under a copy's deck (D376, 2026-11-20): a crossing on a structure's copy is a second span, with its own angle and crossing rule against the copy, and the crosser may bend between the two decks.
- **Faces:** each inner face's walk winds exactly once (its stretch turns plus its corner turns sum to 8 eighths; a corner between parallel legs at a node is a cusp, 4 eighths, oriented by the sides); the outer face winds once the other way.
- **No U-turn:** along every route path the lifted heading stays within 3 eighths of the entry port's heading.
- **Capacity (per structure, for every kind in its allowed set):** the middles needed at the chosen angles fit the kind's limit. Width along the separator (from track pieces): k crossers in a row at 90 degrees take k tiles; k consecutive crossers at the same 45 or 135 degree angle take k + 1 tiles (a diagonal crosses an axis row in two half pieces, and two neighbouring diagonals share a tile); a change of angle between consecutive crossers starts a new run. Structure's P5 guarantees every structure fits at 90 degrees, so a capacity row can always be met.
- **Ends (tunnels on paper, D365):** beyond its outermost crossings the separator holds its axis for the heads: a bridge's head tile each end; a tunnel's portal and ramp tile each end (it dives one level on flat ground; crossers stay at grade and may bend over it, D342). An "either" structure reserves the tunnel's ends (D366). No node or other crossing lies on a head, portal or ramp. A crossing of a bridge separator with a tunnel separator lies on middle tiles of both.
- **Cost:** least total turning (sum of |turn| over stretches, D340); no angle preference among equal turning (corpus, RECON-corpus-crossings §3: under bridges 61% of crossed tracks pass diagonally as S-bends and 37% perpendicular; over tunnels 67% right angle), then a stated deterministic order. (Corrected 2026-11-20 at the engineer's prompt; the first draft preferred 90 degrees. H2 still carries the |a - 2| term; H3 removes it.) Shape's cost is added below Structure's for ranking (D366).

## 3. Output model (each variant), refining Structure's (D367)
- Structure's variant unchanged (map, sites, structures, kinds, cost).
- Per track (beside tracks included): heading at each point; hence its piece list (stretch by stretch) and the piece each event sits on.
- Per crossing: its angle.
- Per structure: its axis; least middle tiles per allowed kind at the chosen angles; end runs per allowed kind (bridge: head; tunnel: portal + ramp); its no-event zone as an event range on the separator.
- Shape cost (turning).
- **Hand-on to Slots (named now so its spec starts from it):** (1) cusps: legs that start or end parallel at a node, which only a sideways move separates; (2) each structure's no-step zone (span, heads, and for a tunnel the ramps); (3) under each structure, the crossers' spread across its separator is an upper bound Slots must keep (crossers' positions along a separator are their slots in their own bundle), so their summed spacing stays within the limit; (4) diagonal crossing pieces move a crosser sideways by their length: a known move Slots accounts for, not a step it adds. (5) Crossers' S-bends under a deck are the coop norm (corpus: 61% of tracks under bridges, 76% of crossing bridges), so "no step inside a deck" binds the separator only; a crosser may step under the span, and its widened footprint counts against the limit in (3). (6) Between structures in series on one separator (a split long bridge or tunnel), the separator and its copy may step on the landing (D375); the landing is at least the 1 signal tile, odd lengths preferred (a Lengths matter).

## 4. Promise (each a test at Shape's output, D367)
- H1 Headings are continuous: a stretch turns monotonically one eighth per piece; each event's heading is the same on both stretches it joins.
- H2 Port headings as authored.
- H3 Node rule: straight leg keeps h; the other leg (each fork leg) h or one eighth to its side; merges mirrored.
- H4 Crossing rule: separator axis at each crossing; crosser not parallel; arrival side as the map's sides; separator x separator at 90 degrees.
- H5 Structures: each separator straight on one axis piece from its first to its last crossing, with its end runs per allowed kind free of other events.
- H6 Capacity: per structure and per allowed kind, the least middles at the chosen angles fit the limit Structure handed down.
- H7 Faces wind once; no U-turn bound on every route path.
- H8 Beside tracks: a copy's or row's headings equal its parent's over its range; mirrored crossings on the crosser's same piece.
- H9 Refinement: Structure's output is unchanged, and P1-P8 still hold on it.
- H11 (D376, per run since D396): a span's runs are its maximal groups of consecutive crossers (in deck order) with headings equal mod 4; a run of k crossers is k tiles wide at 90 degrees and k + 1 on a diagonal; a span's least middles are n plus one per diagonal run (H6); the gap between runs is a Slots length.
- H10 Every variant handed down is shaped. A variant with no shape is a bug: either in Shape, or a condition missing from Structure's promise, which is then added there (never a skip, D361).

## 5. Settled (D373)
- Q1 **Closure:** Tamassia's theorem (valid angles imply a drawing) covers orthogonal drawings only; for 8 directions with positive lengths it is not guaranteed. Shape owns winding (H7); whether a winding-valid shape always closes once Slots opens the cusps is probed first (§7 H2) with a lengths-only LP oracle (lower bounds 1, no clash rows). A missing condition goes into Shape's model, not a later layer.
- Q2 **Ranking:** Structure cost, then Shape cost (lexicographic).
- Q3 **Rows:** a row takes its lane's headings over its whole range. D342's other freedoms are not Shape's: a gap between row and lane is for Slots' spec; a row through a tunnel or over a bridge is Structure's. Toggle candidate: row headings of its own between joins.

## 6. Output types (target; code is mapped onto these, D368)
New header src/junction/planar/planar_shape_layer.h (name to settle against today's planar_shape.h when mapping):
- `PlanarStructure` gains `std::array<int, 2> limit;` (max middle tiles, index 0 bridge, 1 tunnel; -1 when that kind is not allowed). A Structure step, briefed against SPEC-structure §3.
- `PlanarShapeTrack { std::vector<int> pieces; std::vector<uint32_t> event_piece; }`: the track's heading list (one lifted heading per piece, eighths clockwise from north, consecutive pieces one eighth apart) and, per entry of the map's `events[track]`, the piece that crossing sits on (CLAUDE Terms: Shape = heading list + crossing pieces). The heading at a point is the heading of the piece it sits on; stretch k runs from piece event_piece[k-1] (0 for k = 0) to event_piece[k] (the last piece for the last stretch).
- `PlanarShapeStructure { uint8_t axis; std::array<int, 2> middles; std::array<int, 2> end_run; std::array<int, 2> copy_middles; }`: least middles and end run tiles per kind (-1 not allowed); `copy_middles` the copy deck's (D376).
- `PlanarShapeVariant { PlanarStructureVariant structure; std::vector<PlanarShapeTrack> tracks; std::vector<uint8_t> angles; std::vector<PlanarShapeStructure> structures; int cost; }` (angles per map crossing).
- `PlanarShapeResult { std::vector<PlanarShapeVariant> variants; std::string why; }`, ranked per D373(2).
- `PlanarShapeResult BuildPlanarShape(PlanarStructureResult structure)`.
- `std::vector<std::string> CheckPlanarShapePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarShapeVariant &v)`: H1-H9 as a test oracle, one message per breach (topology and spec only for H9's P1-P7 and H6's limits; the oracle is a test tool, not a layer). H10 is a property of `BuildPlanarShape`, tested in H4.
Until tunnels are built (D365): kinds always bridge; the tunnel rows and end runs have hand-structure tests written when D365's build starts.

## 7. Build order (each step one brief, test-first at Shape's output)
- H1 **Oracle:** `CheckPlanarShapePromise` driven by two hand variants (two lines crossing; one fray), one failing test per promise (each mutation breaks only that promise). No characterisation of today's code: `BuildPlanarLineHeadings` needs channel paths (`BuildPlanarPaths`), which Structure's variants do not carry (D366), so it cannot run on them; H2's ILP is judged by the oracle instead, and today's `SolvePlanarShape` is mapped onto H2 as its starting code.
- H2 **Riskiest first (Q1), time-boxed:** the heading ILP of §2.1 without capacity on every handed-down variant: count shaped, unshaped (H10), and closure under the lengths-only LP with cusps opened by one slot; time per variant.
- H3 Capacity rows and end runs, Structure's `limit`, cost and ranking; `BuildPlanarShape`.
- H4 **Outer test:** `BuildPlanarShape` on T2, T3, LL2 (keep 16): every variant shaped, H1-H10 kept, ranking order, time (milliseconds per variant; the seed budget is 5 s, D297).
- **Built (2026-11-20):** H1 oracle (c1f326dfd8, 38c46d9d21); H2 heading program and closure probe (7b0e4255d5); H2b parent chain, probe opens cusps (bdff7e9913, b69d8e57cf); H3 Structure `limit` (8e194c4c5b), capacity rows and `BuildPlanarShape` with the outer test (cb19e03bd5). Findings on the way: the parser needs nodes for any crossing (no port-to-port line); a row's parent is its lane's part chain; the first closure LP failed only because it put parallel legs on one point (Slots' cusp). Result: every T2, T3, LL2 variant (keep 16) shapes, keeps H1-H9 and closes.

## 8. Faces Slots can draw: the guarantee behind D361 (DRAFT, orchestrator, 2026-11-21 night; for the engineer)

**Scope under SSS v2 (D388):** this section now applies to free faces only (faces between bundles: spines' outer lanes, gadget hulls, the box). Faces inside bundles and gadgets are rigid (lanes as offset curves, catalogue gadgets) and get no cells; a dummy lands only on a physical wall, never on another dummy (DESIGN-sss-v2 §3.2, H16). §8.7's runtime certificate (D386 (1)) is superseded: the feasibility program is a test oracle (D388 b).

**Why this section.** Slots (D379) decides every length on Shape's headings, and D361 asks that its output be valid by construction. That needs two statements, proved before Slots' rows are built (D368):
- **G-planar:** any lengths that satisfy Slots' rows give a planar drawing of Structure's map with tile clearances (no clash).
- **G-feasible:** Slots' rows always have a solution on a variant Shape hands down (Slots never discovers infeasibility, D361).

H7 (each face winds once) is TSM's validity condition. In orthogonal drawing it is enough: every valid orthogonal representation refines into rectangles and compacts (Tamassia). In eight directions it is not: G2 showed a ray's landing wall depends on lengths, and faces with fixed headings can fold or collapse. Slots G1-G2 tried to close the gap from below with row families read off clash counts; that is the generate-and-check D361 rules out. This section closes it from above.

### 8.1 Proposed promise: convex cells (H12)
Shape refines every inner face into **convex cells** by dummy edges and hands them down:
- A **dummy edge** runs from a reflex vertex into the face along a heading Shape chooses (one of the headings that splits the reflex angle into two convex ones) and ends on a wall **Shape names**. Choosing the landing wall is a combinatorial decision on headings only, the analogue of TSM's rectangularization, so it is Shape's (it owns headings and decides without lengths). Step and flex bends are Slots' and are not reflex in Shape's model (they live inside a piece); see 8.4.
- **H12:** every cell's walk turns one way only (each corner turns <= 0 with the cell on the left) and winds once.

### 8.2 Slots' rows per cell (complete, no choice left)
- **Closure** of each cell (its walls' lengths times their headings sum to zero), the dummy edge's landing point on its named wall (0 <= u <= the wall's length).
- **Clearance:** for each track wall e of the cell and each track wall f whose heading faces e (3 to 5 eighths from e's, walked), the distance between their lines along e's normal is the sum of the lengths of the walls between them times their normal components: linear, with no choice, because in a convex cell the order of walls fixes which faces which. Row: that distance >= the clearance for the pair's headings (`RungSpacing`). Exempt: e and f adjacent through a corner where they legitimately meet (a node, a crossing).

### 8.3 Proof sketch of G-planar
1. A closed walk whose heading turns monotonically through exactly one turn, with lengths >= 0, is a convex polygon (possibly with zero-length walls): it cannot self-intersect.
2. The cells of a face tile it: every dummy edge is a segment inside the face from a vertex to a point on a named wall (its landing rows), so the cells' union is the face and their interiors are disjoint.
3. Faces tile the box (the map is planar and every face winds once, H7, with the outer face the other way).
4. Clearance rows keep facing walls of each cell apart; walls of different cells are separated by the cells between them. Hence no two track pieces share a tile except where the map says they meet.
- **Open in the proof:** (a) whether "facing walls" (3-5 eighths) covers every pair the tile clash rule cares about, including pairs meeting at 45 degrees across a short wall; (b) degenerate cells (a wall of length 0 making two cells touch).
- **Checks for the paper proof (from the engineer's Gemini review, 2026-11-21):** (c) no dummy edge may land on a zero-length insert (flex or step piece) alone: its landing wall must be a physical wall, or collapsing the insert to 0 pinches the walls either side; (d) per interior corner angle of a convex cell (45, 90, 135 degrees), show that facing-wall separation plus the leg minimums implies the tile clash spacing (1-2 half tiles) on all four normals (x, y, x+y, x-y), not only the facing wall's own.

### 8.4 What this does to steps, flex and beside tracks
- A Slots step or flex insert adds bends inside one of Shape's pieces. With Shape's cells fixed, such bends make the cell non-convex. Two readings to decide: (i) steps and flex inserts stay inside frames whose cells are synchronized corridors (beside offset curves, SPEC-slots-lengths §11), where the corridor is a pair of parallel walls whose convexity holds piecewise; (ii) Shape places the bends (a flex point or a step is a pair of Shape headings with length >= 0, D383's "Shape marks flex points" read literally), so every bend is in Shape's model and convexity covers it.
- Beside tracks: SPEC-slots-lengths §11 (offset curves) is a clearance construction for the thin cell between a track and its row or copy; it fits 8.2 as the corridor case.

### 8.5 G-feasible (the harder half, open)
Every Slots row above is homogeneous (closure, landing, clearance as ">= c" with c > 0 scales away), except the **exact quantities**: port pins and side gaps, deck gaps and spans, beside offsets (one lane), exact prio length (D384), node and join shapes. Scaling all lengths of a drawing up keeps every homogeneous row and every lower bound, so G-feasible reduces to SPEC-slots §0.3's question: **the exact quantities sit in rigid zones, and every cut between two rigid zones crosses pieces that can stretch** (axis pieces along the cut's normal, flex points on diagonals). That is a statement about Shape's cells and Structure's zones; to be written as a promise once 8.1-8.4 are settled.

### 8.6 Decisions for the engineer (card)
- Shape gains a decision (dummy edges and their landing walls) and a promise (H12, convex cells).
- 8.4: steps and flex inserts as Slots-only bends inside corridors (i), or as Shape-placed bend pairs (ii).
- Order: prove 8.3's open points and 8.5 on paper (T2 by hand, then the corpus) before any Slots code.

### 8.7 H12b: how Shape chooses dummies, and why realizability is the test (DRAFT, orchestrator, 2026-11-21 night)
**Correction to 8.1's framing.** A dummy edge need not "land on its wall for all lengths". As in TSM, it is a refinement that the next layer enforces (Slots writes its landing rows, which restrict lengths). What must hold is that the refined representation can still be drawn, with Slots' exact quantities: that is G-feasible (8.5), and it is a property of Shape's choice.

**Facts (geometry, not probe).**
- A convex cell with fixed headings and k walls is drawable on its own and has k - 2 degrees of freedom (scale included).
- Triangles (k = 3) are rigid: their side ratios are fixed by the headings. In orthogonal TSM every cell is a rectangle (2 degrees), and a rectangular subdivision is always drawable. In eight directions a cycle of glued triangles (or other low-freedom cells) can force contradictory scales, so convex cells alone do NOT imply G-feasible.
- Whether a refinement is drawable is decided exactly by a linear system: per cell closure (sum of lengths times headings = 0), every wall length >= 1 (homogeneous rows scale, so >= 1 loses nothing), each dummy's landing point within its wall, plus the exact quantities Structure and Shape already know (port pins and side gaps, deck gaps and spans, beside offsets of one lane, D384's exact prio length, node and join shapes).

**Proposal (H12b, H13).**
- **H12b rule:** at each reflex bend (in heading h, out h + 1, face on the left), candidates are d in {h, h-1, h-2, h-3} (the four headings that split 225 degrees into two convex angles), each with the walls its ray can face in the cell (c = wall heading - d in [-3, -1], walked); a dummy that lands on a box edge or on a wall of a longer free run is preferred, triangles avoided where a candidate gives a cell of 4+ walls. Shape picks the first candidate in that order.
- **H13 (new promise, its test): the refined representation is realizable** by the linear system above. Shape checks it with one LP per variant (no lengths are handed down; the LP is Shape's certificate of its own decision, tested at its output per D367). If it fails, Shape tries the next candidate at the vertices the infeasible rows name (a finite search inside Shape's own decision); if none certifies, the variant is refused with the named rows, which is a missing Structure promise (D361, never a skip).
- RULED D386 (yes to both card items). Rule order made exact for H12b: d in the order h, h-3, h-1, h-2 (lifted); landing = the first wall after v in the cell walk with c in [-3, -1]; take the first candidate whose two cells both have 4+ walls, else the first valid one. On the fray this picks d = east onto the E box edge (two 4-wall cells), the hand dummy of H12a.
- First parity check (engineer's Gemini review): on T2, whether any rigid cell (triangle) touches an exact one-lane offset; if yes, H13 carries the forced parities.
- Open: (a) integrality and grid parity (D382) of a realization: the LP is over the reals; scaling makes it rational and integral, but parity needs its own argument (likely: scale by 2 then shift odd as D382's forced parities allow); (b) clearance (8.3 c, d) is a separate per-cell condition.

**Card for the engineer.**
1. May Shape run an LP to certify its own refinement (H13)? It hands no lengths down; Slots still decides every length. Reading of D366/D367: a layer testing its own promise. Alternative: no LP, only combinatorial rules (avoid rigid cells by rule), with no proof of G-feasible.
2. The fallback search (try other dummy candidates where the LP names rows) is a search inside Shape's own decision; acceptable under D361, or must the first rule be proved sufficient?
