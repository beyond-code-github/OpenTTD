# BRIEF H2: the heading program, and the closure probe

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-shape, head HEAD_HASH. Builds: build (Debug), build-rel (Release), current. Logs: /tmp/claude-1000/-home-pete-OpenTTD/315c4ccd-306d-453c-be89-5dda9a8e71c5/scratchpad/h2/.
- Implements: docs/junction-tool/wip/SPEC-shape.md §2 and §2.1 (every rule except Capacity, which is H3), §7 step H2 (riskiest first, D373(1)); DESIGN-tracks §17.2. The oracle `CheckPlanarShapePromise` (H1, same file) is the judge.

## Read first (once each, with offset/limit)
1. docs/junction-tool/wip/SPEC-shape.md (whole).
2. src/junction/planar/planar_shape_layer.h and .cpp (whole: the types, and the oracle whose rules you turn into rows; reuse its anonymous-namespace helpers).
3. src/tests/junction/planar/planar_shape_layer.cpp (the t1 and fray hand cases and helpers).
4. src/junction/placement/junction_milp.h (whole; short).
5. src/junction/planar/planar_model.h lines 40-50 (`STEP`).
6. CODINGSTYLE.md (naming, doc comments).

## The program (one `JunctionMilp` per variant)
Points: per track t, point 0 = its start end, point i (1..n) = its event events[t][i-1], point n+1 = its end end. Stretch k joins points k and k+1.
- **Variables:** H[t][i] integer in [-24, 32] for every track and point (lifted heading). Helper integers q (in [-8, 8]) wherever a row below says "+ 8q"; each gets its own variable.
- **Ports:** a track starting at a port: H[t][0] = the port's heading (fixed, 0..7). A track ending at a port: H[t][n+1] = heading + 8q.
- **Splits** (site with split true, in track I, out legs L): H[L][0] = H[I][end] + d_L * σ(L), d_L binary, d_L fixed 0 for the straight leg. σ as the oracle's H3 (Side against the straight leg, else the site's side; at a fork against the other leg).
- **Merges** (out track O, in legs L): straight leg S: H[S][end] = H[O][0]. Every other leg: H[L][end] + d_L * σ(L) - H[O][0] = 8q, d_L binary.
- **Crossings** (map crossing c, separator p, crosser q as the oracle's H4; s as the oracle's H4): H[q][at c] - H[p][at c] - s * a_c = 8 q_c, a_c integer in [1, 3]; separator axis: H[p][at c] = 2 z (z integer). A separator x separator crossing: both rows from each side (a_c = 2 then follows).
- **Structures:** for consecutive crossings c, c' of one structure: H[sep][at c] = H[sep][at c'].
- **Beside tracks** (Copy or Row, parent = straight leg of its split node's site, as the oracle's H8): H[b][i] = H[parent][i] for every point i.
- **Faces:** for every face of `map.faces`, the same walk as the oracle's H7: Σ walked stretch turns + Σ vertex terms + box terms = -8, where a forward stretch's walked turn is H[k+1] - H[k], a backward one's -(H[k+1] - H[k]); each vertex term is a new integer v in [-4, 3] with v = (next walked start) - (previous walked end) - 8q (walked heading of a backward stretch = H + 4); each box insert adds v1 = box(A) - arriving - 8q, v1 in [-4, 3], the constant -2 per corner, and v2 = leaving - box(B) - 8q, v2 in [-4, 3]. Refactor the oracle's H7 walk into one anonymous-namespace function that lists, per face, its terms (stretch and direction; vertex between two stretch ends; box insert with sides A and B), used by both the oracle and the program; the oracle's behaviour must not change (its tests stay green).
- **No U-turn:** per route of `map.expanded`, every path from its entry port track to an exit port track following, at each track's end node, the out legs whose line carries the route (rows, copies and parts included; enumerate by depth-first search): cum at each point of each track on the path = Σ over earlier tracks (H[end] - H[0]) + Σ the d * σ of each node transition passed (splits: d_L σ(L); merges: d_L σ(L) of the leg, 0 for the straight leg) + (H[t][i] - H[t][0]); -3 <= cum <= 3 at every point. If a path's consecutive tracks do not meet at a node, stop and report it.
- **Monotone stretches** need no row: a stretch's pieces are generated monotone from its end headings (below).
- **Objective:** minimise 1000 * Σ |H[k+1] - H[k]| over the stretches of Line tracks (an auxiliary u >= ±turn each) + Σ |a_c - 2| over crossings (auxiliary). Time limit 5 s per variant.

## Output
- New function in planar_shape_layer.h/.cpp: `bool ShapePlanarStructureVariant(const PlanarStructureVariant &v, PlanarShapeVariant &out, std::string &why);` doc-commented from SPEC-shape §2. On a non-optimal solve: false, `why` = "no shape: <status>", plus the tags of the infeasible rows when the status is Infeasible (tag each row by kind and object: "port t", "split node", "merge node", "cross c", "axis c", "struct i", "beside b", "face f", "uturn route r", so `InfeasibleRows` reads).
- On Optimal: `out.structure = v`; per track: pieces = for each stretch k, the headings from H[k] to H[k+1] one step at a time (the first piece of stretch k+1 is the last of stretch k, not repeated), event_piece[i] = the index of the piece holding point i+1; `angles[c] = a_c`; per structure: axis = separator heading at its crossings mod 8, middles and end_run as the oracle's H5 and H6 compute them (no capacity row in this step); cost = Σ turns of Line tracks.

## Closure LP (probe only, in the test file, not in the product)
For a shaped variant: one `JunctionMilp` with continuous variables: per point of every track (x, y); per piece j of every track a length L_j >= 1; box width W >= 1 and height Hb >= 1; every point within [0, W] x [0, Hb]. Rows: along each stretch, point k+1 = point k + Σ over its pieces L_j * STEP[h_j mod 8] (both coordinates); tracks meeting at a node share its point (in end = every out leg's start; every in leg's end = out start); the two tracks' points at a crossing are equal; a port end on side W has x = 0, E x = W, N y = 0, S y = Hb. Objective W + Hb. Closes = Optimal. (No clash, no spacing, no slot rows: it asks only whether the headings can close with positive lengths.)

## Tests, in this order (red before green)
1. **t1 shaped (inner, fast tag):** `ShapePlanarStructureVariant` on the t1 hand case's variant returns true, the oracle returns empty, and cost == 4 (the least turning, derived: F1 must reach the axis 4 at crossing 1 from at most 3, A1 must end on 4 from at most 5, C1 must end on 6 or 7 from 0, D1 must cross B1 on axis 0 and end on 1 or 2). Red: the function declared and returning false.
2. **fray shaped (inner, fast tag):** returns true, oracle empty, cost == 1.
3. **Probe (outer, characterisation, tag `[planarshape][slow]`):** for T2, T3, LL2 (`PlanarTText(2)`, `PlanarTText(3)`, `PlanarLL2Text()`), `BuildPlanarStructure(topology, MakeTestJunctionSpec(), 16)`; per variant: shape it; on success run the oracle and the closure LP. Print per hub: variants, shaped, unshaped (each tag with its why and IIS tags), oracle breaches by id (H6 expected possible: no capacity rows yet; any other id is a finding), closure feasible / infeasible (tags), turning cost min and max, time per variant (max, mean) for the program and for the LP. REQUIRE only that every variant was attempted.

## Do not
- Do not add capacity rows (H3), `BuildPlanarShape`, `limit`, ranking, or anything for Slots.
- Do not change any file outside planar_shape_layer.{h,cpp}, its test file, and nothing in CMake (already registered).
- Do not relax a row to make a variant shape; an unshaped variant is the probe's answer, not a bug to hide.

## Stop conditions
- The t1 or fray test fails after the program is built (give the oracle messages or the why with IIS tags): the derivation is the orchestrator's to fix.
- A route path's consecutive tracks do not meet at a node.
- Any rule above cannot be written from the fields named.
- The build fails outside your files.

## Gates
- Release build of openttd_test (log to file; zero warnings in your files).
- `tools/junction/jt fast build '[planarshape]' 5` (Debug), then `tools/junction/jt gate build-rel` within its budget (it stood at 26.9 s of 30 s: if it goes over, report the time, do not tag anything slow to hide it).
- Run the probe once in Release by its test name, output to a log file.
- Commit: `Add: Junction planar Shape heading program and closure probe (H2)`, trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Update src/junction/planar/CLAUDE.md's "Shape layer" section in the same commit with the facts this step makes true (the program's rules in one bullet; no probe numbers, no open questions).

## Report
- References read
- Commit hash and subject
- Red evidence per test
- Functions added (names only)
- The probe's printed lines, copied
- Gate results and times
- Stops, and anything left undone
- State: `git status --porcelain`, `git log -1 --oneline`

When you have reported, stop.
