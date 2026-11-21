# BRIEF E5-KA: pairwise separation probe in the drawing program (D406)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Budget
Total 90 min wall: implementation and fast tests about 60 min, measurements at most 25 min (caps below). Each solve has its own cap; no rerun loops (run each measurement once; a rerun only after a code fix, at most once). Builds `-j 4`. One heavy job at a time.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Release build dir `/home/pete/OpenTTD/.claude/ng1/build-rel` (valid, reuse it; `cmake --build build-rel --target openttd_test -j 4 > <file> 2>&1`).
- Ruling: D406 in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (last row). Card: `/home/pete/OpenTTD/docs/junction-tool/wip/CARD-E5-keepapart.md` (option B). Background: `/home/pete/OpenTTD/docs/junction-tool/wip/probes/e5land/NOTES.md` Round 2 (faces 16, 17 infeasible with named landings; 61, 99 undecided).

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_draw.h` (all) and `src/junction/planar/planar_draw.cpp` (all; 447 lines; the clearance loop is lines 322-424).
- `src/junction/planar/planar_shape_layer.h` lines 40-60 and 110-131; `src/junction/planar/planar_shape_layer.cpp` lines 1125-1155 (`PlanarShapeCellWalls`).
- `src/tests/junction/planar/planar_draw.cpp` (all; the 197 test at lines 136-170 is the model for the new hidden test).
- `src/junction/planar/CLAUDE.md`, section "Drawing program (`planar_draw`) (E5)".

## 3. Tests, outermost first (red before green; record each red message)
T-outer (hidden, tag `[.][planarkeepapart]`), in `src/tests/junction/planar/planar_draw.cpp`: "planar draw: PSG 197 pairwise, faces 16 and 17 alone are drawable". Load 197 exactly as the existing 197 test does (topology, structure file, `ShapePlanarStructureVariant`). For each face f in {16, 17}: `CheckPlanarDrawable(shaped, 15.0, {.clearance = true, .clear_rows = true, .pairwise = true, .only_faces = {f}})`; WARN a line `KA face <f>: feasible, status, pairs, binaries, rows, columns, ms` and up to 20 conflict tags; `CHECK(r.feasible)`. Red expected before the code: with the members added but ignored (step 4a below), the program writes named landings for all faces and is infeasible; record the status and the conflict families.

T-inner-1 (fast, tags `[junction][planar][planardraw]`): "planar draw: face walls are the cells' walls when there are no dummies". `DrawCase c = ShapeFirst(PlanarTText(1), 1); c.v.dummies.clear();` then the list of walls of `PlanarShapeFaceWalls(c.v)` (each entry's `walls`, in order) equals `PlanarShapeCellWalls(c.v)` element by element (compare track, piece, heading), and the faces are strictly increasing. Red: with the stub returning an empty vector, the sizes differ.

T-inner-2 (fast, same tags): "planar draw: pairwise mode writes separations and no landings". `DrawCase c = ShapeFirst(PlanarTText(1), 1);` `r = CheckPlanarDrawable(c.v, 5.0, {.pairwise = true})`: `CHECK(r.pairs > 0)`, `CHECK(r.binaries >= 2 * r.pairs)` (every pair has at least two axis sides) and `CHECK(r.binaries <= 4 * r.pairs)`. WARN the status and counts (feasibility is not asserted: T1 waits for the corridor family). Red: with the members added but not filled, `pairs` is 0.

## 4. Changes
### 4a. Declarations first (then run the three tests red)
- `planar_shape_layer.h`, after `PlanarShapeCellWalls`:
  ```cpp
  /** One inner face's walk, unsplit by dummies. */
  struct PlanarFaceWalls {
  	uint32_t face; ///< The face's index in the map's faces.
  	std::vector<PlanarCellWall> walls; ///< Its walls in walk order, the face on the left.
  };
  /**
   * Every inner face's walls as PlanarShapeCellWalls walks them, without the dummies' splits: the same faces (usable, winding -8), in the map's order.
   * @param v The variant; its tracks must pass H1.
   * @return One entry per face.
   */
  std::vector<PlanarFaceWalls> PlanarShapeFaceWalls(const PlanarShapeVariant &v);
  ```
  Stub in the .cpp returning `{}`.
- `planar_draw.h`, `PlanarDrawOptions` gains:
  - `bool pairwise = false; ///< With clearance (D406 probe): no dummies and no landings; a face convex without dummies keeps the clearance rows, every other face gets pairwise separation of its walls (`sep`).`
  - `std::vector<uint32_t> only_faces; ///< With pairwise: write the face rows (clearance or separation) only for these faces (map face indices); empty is every face. A measurement aid.`
- `PlanarDrawResult` gains `uint32_t pairs = 0; ///< Wall pairs separated (pairwise mode).` and `uint32_t binaries = 0; ///< Their axis-side binaries.`

### 4b. `PlanarShapeFaceWalls`
The loop of `PlanarShapeCellWalls` (same `Oracle`, `WalkFace`, `usable`, `CellWalls`, `piece_total != -8` skips), without the dummy splitting: push `{face.face, walls of first}` with the heading normalised as there. Do not change `PlanarShapeCellWalls`.

### 4c. `CheckPlanarDrawable`, pairwise mode (only when `options.clearance && options.pairwise`; with `pairwise` false the program is byte for byte what it is now)
1. **No dummies:** `nd` is 0 in pairwise mode (so no dummy and no `land` rows).
2. **May-be-zero pieces:** keep each track's `free_piece` vector (today local to the points loop) in a `std::vector<std::vector<bool>> may_zero(nt)` so the face code can read it.
3. **Refactor, no behaviour change:** move the body of today's per-cell loop (the `Part` computation through the `clear` rows, lines 358-423) into a lambda `clear_walls(const std::vector<PlanarCellWall> &cell, size_t c)` returning false where today's code returns `unsupported("cell {} names a wall the variant lacks")` (the caller returns that same `unsupported`). The non-pairwise path calls it for each cell exactly as today (same tags).
4. **Faces:** in pairwise mode, for each `PlanarFaceWalls f` of `PlanarShapeFaceWalls(v)` with `only_faces` empty or containing `f.face`:
   - turn at corner k (between wall k and wall k+1, cyclic) = `norm(h[k+1] - h[k])` with `norm(x) = ((x % 8) + 12) % 8 - 4` (range -4..3). A face is **convex** when no corner turns > 0 (the reflex test `RefinePlanarShapeCells` uses).
   - convex: `clear_walls(f.walls, f.face)` (tags read `clear <face> <vertex> <wall>`).
   - otherwise: the pairwise rows below.
5. **Pairwise rows of one face** (walls w[0..n-1], headings h[i], and the end points `a_i`, `b_i` of each wall: `point[track][piece]` and `point[track][piece + 1]`; a box edge (`track == UINT32_MAX`) is no wall here, as in clearance today). For each pair i < j, skip it when:
   - adjacent in the walk (j == i + 1, or i == 0 and j == n - 1);
   - either wall is a span piece (`is_span`) (stated limit: a span piece's end vertices get no separation in this mode);
   - both walls are the same (track, piece);
   - same track and every piece strictly between the two pieces (by index, either order) is in `may_zero` (no piece between counts as all);
   - any end point of one shares a node with any end point of the other (`find(x var)` equal, as clearance uses today);
   - the two pieces cross each other: some value `c` occurs in both `map.events[t_i]` and `map.events[t_j]` with `v.tracks[t_i].event_piece[k_i] == p_i` and `v.tracks[t_j].event_piece[k_j] == p_j` at those positions.
   Otherwise the pair is separated (`result.pairs++`). With `n(h) = STEP[(h + 6) % 8]` (the clearance rows' normal) and `M = 4 * COORD_MAX`:
   - axes: `n(h_i)` with spacing `RungSpacing(h_i, h_j)`; when `h_i % 4 != h_j % 4` also `n(h_j)` with spacing `RungSpacing(h_j, h_i)`; when `h_i % 4 == h_j % 4` instead the direction `STEP[h_i % 8]` with spacing 2 (RungSpacing(h, h) is 2 for every h).
   - per axis s and side (+: wall j beyond wall i along s; -: wall i beyond wall j along s) one binary `b = var(0, 1, 0)` (`result.binaries++`); for side + and every p in {a_i, b_i}, q in {a_j, b_j}: `row(dot(q - p, s) terms + {b, -M}, spacing - M, INF, "sep <face> <i> <j>")`; side - is the same with i and j swapped.
   - `row(sum of the pair's binaries, 1, 1, "sepdir <face> <i> <j>")`.
   Worked call site (pair i, j, axis s, side +):
   ```cpp
   const uint32_t b = var(0, 1, 0);
   result.binaries++;
   bs.push_back(b);
   for (const auto &p : {part_i.a, part_i.b}) {
   	for (const auto &q : {part_j.a, part_j.b}) {
   		Terms t{{b, -M}};
   		for (int axis = 0; axis < 2; axis++) {
   			t.emplace_back(q[axis], s[axis]);
   			t.emplace_back(p[axis], -s[axis]);
   		}
   		row(std::move(t), spacing - M, JunctionMilp::INF, fmt::format("sep {} {} {}", f.face, i, j));
   	}
   }
   ```
   Separation rows come after the clearance block's position in the program (no other row order changes).
6. Doc comment of `CheckPlanarDrawable` gains the `sep`/`sepdir` families and the pairwise option in one sentence each.

## 5. Measurements (after the tests are green; Release; each command's output to a file under `/tmp/claude-1000/` named `ka-*.txt`)
Add two more hidden tests in the same file, tag `[.][planarkeepapart]`, each WARNing one `KA ...` line per solve (face or whole, feasible, status, pairs, binaries, rows, columns, ms, and for an infeasible one its conflict families with counts plus the first 10 tags):
- M1 "planar draw: PSG 197 pairwise, every face alone": every face of `PlanarShapeFaceWalls(shaped)` alone, 5 s cap each, faces in order; at the end one `KA sweep` line with counts feasible / infeasible / limit, and the list of infeasible and limit faces. Stop the sweep (and say so) once 15 min wall have passed.
- M2 "planar draw: PSG 197 pairwise, whole": `only_faces` empty, 15 s; then the same at 60 s (information only).
Run them directly: `build-rel/openttd_test "[planarkeepapart]" > /tmp/claude-1000/ka-run.txt 2>&1` (hidden probe tests have no jt mode); grep the `KA` lines.

## 6. Do not
- Do not change Shape, `PlanarShapeCellWalls`, H12b, or the non-pairwise program.
- Do not prune further, tighten M, add more axes, or change any skip rule to make a measurement pass; an infeasible result is data. Report it with its tags.
- Do not add an objective term, an option, a field or a test beyond those named.
- Do not run the full gate more than once.

## 7. Stop conditions
- A premise is false (for example `PlanarShapeFaceWalls` cannot match the cells' walls on T1 without dummies, or `event_piece` does not map crossings as described).
- T-outer stays red after the code: report the face, status and conflict tags; do not fix it (that is the measurement).
- Over budget on any fast tag or the total budget.

## 8. Gates
- `tools/junction/jt fast build-rel "[planardraw]" 10` and `tools/junction/jt fast build-rel "[planarshape]" 10`; then `tools/junction/jt gate build-rel` once (JT_JOBS=4; known red: none expected in the gate; report any).
- `src/junction/planar/CLAUDE.md`, "Drawing program" section: one bullet stating the pairwise mode as built (flag, convex faces keep clearance, every other face's non-incident wall pairs separated along their normals or their shared direction, one binary per axis side, `sep`/`sepdir`, the skips, the stated span limit), and the measured numbers belong in the report, not the CLAUDE.md.
- One commit: `Add: Junction drawing program: pairwise separation in refined faces, a probe behind a flag (D406)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report (short, this format)
- References read
- Commits: hash and subject
- Red evidence: each new test and its failure message before the code
- Types and members added or removed: names only
- Suite results (count and time per gate), naming any failure you did not cause
- Measurements: every `KA` line of T-outer, M2, and M1's `KA sweep` line plus the infeasible/limit faces' lines
- Stops, and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
