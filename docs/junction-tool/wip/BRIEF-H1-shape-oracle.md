# BRIEF H1: Shape's promise oracle

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-shape, head HEAD_HASH. Builds: build (Debug) and build-rel (Release), both current up to the previous code commit (incremental). Logs: /tmp/claude-1000/-home-pete-OpenTTD/315c4ccd-306d-453c-be89-5dda9a8e71c5/scratchpad/h1/.
- Implements: docs/junction-tool/wip/SPEC-shape.md §2.1 (rules), §4 (promise H1-H9), §6 (output types), §7 step H1; DESIGN-tracks §17.2; rulings D340, D349, D351, D365, D369, D373. The spec is the source of truth (D368). No Shape solver exists in this step.

## Read first (once each, with offset/limit)
1. docs/junction-tool/wip/SPEC-shape.md (whole; short).
2. src/junction/planar/CLAUDE.md lines 1-61 ("Architecture", "Terms").
3. CODINGSTYLE.md (naming and doc-comment rules).
4. src/junction/planar/planar_structure.h (whole: the types this step embeds) and src/junction/planar/planar_structure.cpp: only `CheckPlanarStructurePromise` (its P5 limits and its P7 route-path walk are the patterns to follow).
5. src/junction/planar/planar_regions.h lines 25-36 (`PlanarWallRun`, `PlanarFace`) and 89-112 (`PlanarTrack`, `PlanarTrackMap`); src/junction/planar/planar_topology.h lines 20-90 (headings, ports, lines with their ends, nodes, routes).
6. src/tests/junction/planar/planar_structure.cpp: its hand-case helpers (`HandCase`, `RequireOnly`, `Ids`) and the P7 test (topology text pattern); and how the file is registered in src/tests/junction/planar/CMakeLists.txt.

## Files
- New: src/junction/planar/planar_shape_layer.h and planar_shape_layer.cpp (registered in the planar CMakeLists like planar_structure.cpp); src/tests/junction/planar/planar_shape_layer.cpp (registered like planar_structure.cpp), tags `[junction][planar][planarshape]`.
- The header declares exactly SPEC-shape §6's types `PlanarShapeTrack`, `PlanarShapeStructure`, `PlanarShapeVariant`, `PlanarShapeResult`, and `CheckPlanarShapePromise(topology, spec, v)`, with the members §6 gives. Do NOT declare `BuildPlanarShape`, and do NOT add `limit` to `PlanarStructure` (later steps). Doc-comment every type and member from the spec's wording. Kind index: 0 bridge, 1 tunnel; -1 in `middles`/`end_run` for a kind the structure does not allow.

## Conventions the oracle uses (derived; do not change them)
- Headings: eighths clockwise from north, y down (planar_topology.h). Pieces are lifted integers (not reduced mod 8). `norm(x)` below means x reduced into [-4, 3].
- Stretch k of track t (k = 0 .. events[t].size()): from piece `first(k)` = (k == 0 ? 0 : event_piece[k-1]) to piece `last(k)` = (k == events.size() ? pieces.size()-1 : event_piece[k]). Heading at the crossing events[t][k] = pieces[event_piece[k]].
- A side +1 is right of travel = a clockwise (+1) turn.
- Separator of a map crossing c: the `separator` of the structure in `v.structure.structures` whose `crossings` lists c. A crossing listed by two structures (each track the other's separator) is a separator x separator crossing.

## The oracle: one message per breach, each starting with its id ("H3: ...")
- **H1:** for every track, `pieces` is non-empty, consecutive pieces differ by exactly 1 in absolute value, `event_piece` has one entry per `events[t]` entry and is non-decreasing and within range; within every stretch the pieces are monotone (never both a +1 and a -1 step).
- **H2:** a track whose start is a port end: pieces.front() ≡ the port's heading (mod 8); whose end is a port end: pieces.back() ≡ the port's heading (mod 8).
- **H3:** for every site s of `v.structure.sites`, with the node's in and out tracks from `map.expanded`: at a split, h = the in track's last piece; every out leg L: its first piece ≡ h + d·σ(L) (mod 8) with d in {0, 1}, and d = 0 for the straight leg. At a merge, h = the out track's first piece; every in leg L: its last piece ≡ h - d·σ(L), d = 0 for the straight leg. σ(L) = `map.Side(L, its stretch at the node, S, S's stretch at the node)` with S the straight leg (stretch 0 at a split, the last stretch at a merge), or `s.side` when that is 0. At a fork (straight UINT32_MAX, two legs) σ(L) = `map.Side(L, ., other leg, .)` at the node.
- **H4:** for every crossing c with separator p and crosser q (each the other's, for a separator x separator crossing): k = index of c in events[q], j = index of c in events[p]; s = -map.Side(q, k, p, j), or -map.Side(q, k, p, j+1) when that is 0 (s = +1: q arrives from p's left); a = ((H_q - H_p) * s) mod 8 into 0..7; breach unless a in 1..3 and `v.angles[c] == a`; for a separator x separator crossing also a == 2. (If both Sides are 0, that is a breach naming "no side".)
- **H5:** every structure: all its crossings sit on the same piece of its separator (same pieces index), and that piece's heading is even (an axis). `end_run` is {1, -1} for kinds bridge, {-1, 2} for tunnel, {1, 2} for either.
- **H6:** every structure: walk its crossers in order with each crossing's angle a as H4 computes it from the headings (not `v.angles`, so a wrong stored angle is H4's alone); split into runs of consecutive equal angle; a run of k crossers takes k tiles at angle 2, k + 1 tiles at angle 1 or 3; the sum is the least middles; breach unless `middles[kind]` equals it for each allowed kind (and is -1 for a kind not allowed), and it fits: bridge `<= spec.max_bridge_middle`, tunnel `<= spec.max_tunnel_length`, and with a copy `middles + 2 <= 2 * spec.sd + spec.tl` (the same limits as P5).
- **H7 (faces):** for every face of `map.faces` (every face but the outer; the walk keeps the face on its left): walk its runs in order; a forward run walks its stretches along travel (walked headings = pieces), a backward run against it (walked headings = pieces + 4, stretches in reverse, pieces within each stretch reversed). Total = sum of the walked turns inside stretches + norm(next walked start heading - previous walked end heading) at every vertex between consecutive stretches (cyclically). Where the walk leaves the box at a port end on side A and re-enters at a port end on side B, insert the box: walked box heading for side N, E, S, W is 6, 0, 2, 4; add norm(box(A) - arriving heading), then -2 for each box corner passed, (index(A) - index(B)) mod 4 corners with N, E, S, W = 0, 1, 2, 3, then norm(leaving heading - box(B)). Breach unless the total is -8.
- **H7 (no U-turn):** walk every route path of `map.expanded` (as P7 walks route paths); cumulative turn = sum of consecutive piece differences within tracks plus norm(next track's first piece - previous track's last piece) at each node; breach when it leaves [-3, 3].
- **H8:** for each Copy or Row track b: its parent is the straight leg of b's split node's site; b's pieces equal the parent's pieces and b's event_piece equals the parent's event_piece (entry by entry); for the k-th crossings of b and of the parent, the crosser is the same track x, and x's event_piece at both crossings is equal.
- **H9:** each message of `CheckPlanarStructurePromise(topology, spec, v.structure)`, prefixed "H9: ".

## Tests, in this order (red before green: write all tests against a stub returning {"not implemented"}, record the failures, then implement)
Hand cases, each `BuildPlanarStructure(topology, MakeTestJunctionSpec(), 1)` variant 0 as `v.structure`; assert its tag before using it.
- **t1:** `PlanarTText(1)`, tag `o3t0f0m0` (dump: /tmp/claude-1000/-home-pete-OpenTTD/315c4ccd-306d-453c-be89-5dda9a8e71c5/scratchpad/h1/t1-dump.txt). Valid shape, by track index (pieces; event_piece): 0 {2}; 1 {6}; 2 {2}; 3 {6}; 4 {0}; 5 {4}; 6 {5, 4}; 7 {6}, [0, 0]; 8 {0, -1}; 9 {0, 1}, [0, 0]; 10 {2}; 11 {3, 4}, [0, 1]; 12 {6}; 13 {2}; 14 {6}; 15 {2}. Angles: crossing 0 → 2, 1 → 2, 2 → 3. Structures (in `v.structure.structures` order): separator 9: axis 0, middles {3, -1}, end_run {1, -1}; separator 11: axis 4, middles {1, -1}, end_run {1, -1}. (Orchestrator's hand check: every face winds -8; every route stays within ±2.)
- **fray:** `"topology fray\nport W in E: AB1\nport E out E: A1\nport S out S: B1\nnode AB1 -> A1, B1\n"`, tag `o0t0f0m0`. Valid shape: AB1 {2}, A1 {2}, B1 {3, 4}.
Tests:
1. Both valid cases: the oracle returns empty.
2. One mutation per promise, each asserting every message carries that one id (as `RequireOnly`):
   - H1 (fray): B1 pieces {3, 4, 3, 4}.
   - H2 (fray): A1 pieces {2, 1}.
   - H3 (fray): B1 pieces {4}.
   - H4 (t1): angle of crossing 1 set to 1.
   - H5 (t1): track 9 event_piece [0, 1] (crossing 2 on its diagonal piece), with angle of crossing 2 set to 2 and separator 9's middles {2, -1}, so only H5 breaks.
   - H6 (t1): separator 9's middles {5, -1}.
   - H7 (fray): A1 pieces {2, 3, 4, 5, 6, 7, 8, 9, 10} (winding and U-turn both break; all messages H7).
   - H8 (t1): row 15 pieces {2, 3}.
   - H9 (t1): push one entry into `v.structure.map.conflicts` (P2).
Check with `jt` timings: the tag stays inside the `[planar]` fast budget.

## Do not
- Do not change any existing source file except the two CMakeLists registrations.
- Do not write a solver, a builder or a characterisation of today's code.
- Do not make any check lenient so a valid case passes: if a valid case breaches, that is a stop.

## Stop conditions
- A case does not build or its variant 0 has another tag (give `why` or the tag).
- A valid case breaches a promise (give the message): the derivation above or the spec is wrong, which is the orchestrator's to fix.
- A mutation breaks more than one promise.
- A check cannot be written from the fields named (say which).
- The build fails outside your files.

## Gates
- Release build of openttd_test (log to file; zero warnings in your files).
- `tools/junction/jt fast build '[planarshape]' 5`, then `tools/junction/jt gate build-rel` green within its budget.
- Commit: `Add: Junction planar Shape promise oracle (H1)`, trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read
- Commit hash and subject
- Red evidence per test (the failure line before the code)
- Types and functions added (names only)
- Gate results and times
- Stops, and what was left unchecked (copies in H8 at least: no T1 variant within keep 16 has one)
- State: `git status --porcelain`, `git log -1 --oneline`

When you have reported, stop.
