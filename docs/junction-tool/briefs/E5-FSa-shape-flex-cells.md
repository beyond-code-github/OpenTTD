# Brief E5-FS-a: Shape refines cells with flex pairs at size 0 (D407 (1))

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Work and commit only there.
- Plan: `docs/junction-tool/wip/CARD-E5-flexstep.md` (step FS-a); ruling D407 (1) in `docs/junction-tool/DECISIONS.md`; proof `docs/junction-tool/wip/SPEC-shape.md` §8.8 ("Definitions": reduced walk; "The rule": H12 amended).

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `docs/junction-tool/wip/SPEC-shape.md` §8.8 only (from the heading "### 8.8" to the end of the file).
- `src/junction/planar/planar_shape_layer.cpp`: `struct Wall` (~line 225), `SplitCell` (~253), `Oracle::CheckH12` (~701), `H14_IN_PROMISE` (~909, the pattern for the flag), `RefinePlanarShapeCells` (~1045-1142).
- `src/tests/junction/planar/planar_shape_layer.cpp`: `ShapedWithFlex` and the test after it (~1587-1650).
- `src/junction/planar/CLAUDE.md`, section "Shape" bullet on refinement and "Shape layer" bullet on flex points.

## 3. Tests (outside-in), in `src/tests/junction/planar/planar_shape_layer.cpp`, tag `[junction][planar][planarshape]`
1. **Outer: `planar shape layer: no dummy at a flex bend`.** For each hub T2 (`PlanarTText(2)`), T3 (`PlanarTText(3)`), LL2 (`PlanarLL2Text()`), Structure with keep 16, shape every variant (as `ShapedWithFlex` does). Count dummies at a flex bend: a dummy `d` with some `m` in `shaped.tracks[d.track].flex` and (`d.piece + 1 == m` or `d.piece == m`) (the bend before or after the insert; `d.piece` is the bend between piece `d.piece` and `d.piece + 1`). Assert the total is 0, and assert `CheckPlanarShapePromise(topology, spec, shaped)` is empty for every shaped variant. Also `WARN` one line per hub: variants shaped, total dummies, total flex middles.
   - Red expected today: the flex-bend count is > 0 (record the count and the failure text). **If the count is already 0 today on all three hubs, stop and report** (the premise is false).
2. **Inner: `planar shape layer: H12 ignores a flex bend`.** Take `ShapedWithFlex`'s variant. Assert `CheckPlanarShapeCells(shaped)` is empty. Then remove every dummy whose start bend is a flex bend (same predicate as test 1) from a copy and assert `CheckPlanarShapeCells(copy)` is still empty (today it reports "turns both ways" at the flex bend: that is the red, record it). Note: after the change test 1 makes this copy equal to the original; keep the test anyway, it pins H12's side.

## 4. Changes, all in `src/junction/planar/planar_shape_layer.cpp`
1. A flag next to the `Wall` struct (after it, before `struct Oracle;`), same style as `H14_IN_PROMISE`:
   ```cpp
   /** D407 (1): cells are refined and checked (H12, H12b) with every flex pair at size 0: a flex insert is no wall of the walk and its bends are no corners. */
   static constexpr bool FLEX_CELLS_AT_ZERO = true;
   ```
2. A free function after `SplitCell`, before `struct Oracle` (it needs only the variant):
   ```cpp
   /**
    * A cell walk with every flex insert removed (SPEC-shape §8.8, reduced walk): an insert's turn is added to the wall before it, so the walls either side meet with turn 0.
    * @param v The variant; `PlanarShapeTrack::flex` names the inserts.
    * @param walls The walk, in order, cyclic.
    * @return The walk without inserts.
    */
   static std::vector<Wall> ReducedWalls(const PlanarShapeVariant &v, const std::vector<Wall> &walls)
   ```
   A wall is an insert when `w.track < v.tracks.size()` and `w.piece` is in `v.tracks[w.track].flex`. Keep the order; add each removed wall's `turn` to the previous kept wall's `turn`, cyclically (an insert at index 0 adds to the last kept wall). If every wall were an insert, return `walls` unchanged (cannot happen; no extra handling).
3. `RefinePlanarShapeCells`: right after the `if (piece_total != -8) continue;` line, add `if constexpr (FLEX_CELLS_AT_ZERO) first = ReducedWalls(v, first);`. Nothing else in it changes: a reduced walk has no turn > 0 at a flex bend, so no dummy starts there and none lands on an insert.
4. `Oracle::CheckH12`: the same line right after its `if (piece_total != -8) continue;` (use `this->v`). The dummies split the reduced walk exactly as they split the full one (they start at non-flex bends and land on non-insert walls).
5. Do not change `PlanarShapeCellWalls` or `PlanarShapeCellFaces`: they keep the inserts as walls (the drawing program needs them; step FS-b groups them into sides).
6. `src/junction/planar/CLAUDE.md`, "Shape" refinement bullet: add one sentence: "Refinement and H12 run on the reduced walk (D407): every flex insert removed and its turn added to the wall before it, so a flex bend is no corner and gets no dummy, and no dummy lands on an insert (`FLEX_CELLS_AT_ZERO`); `PlanarShapeCellWalls` keeps the inserts as walls." Update the promise sentence "every face's cells, split by its dummies, turn one way and wind once (H12)" to "... turn one way and wind once once their flex inserts are removed (H12, D407)".

## 5. Do not
- Do not touch `planar_draw.cpp` or any drawing-program test (that is step FS-b).
- Do not change `PlacePlanarShapeFlex`, the candidate order, D405's monotone rule, or `SplitCell`.
- Do not add a runtime option; the flag is the constexpr above.
- Do not edit a pinned number in an existing test unless rule 6 below allows it.

## 6. Stop conditions
- Test 1 is not red today (premise false).
- Any existing test fails after the change. Exception: a test that pins a dummy count or a specific dummy may be updated **only** if every removed dummy started at a flex bend; list each such test, the old and new values, and say so in the report. Any other failure: stop, leave the change uncommitted in the tree (WIP commit named `WIP FS-a` is fine), report the test and its assertion text.
- `[planardraw]` failures after the change are a stop too (report them; FS-b will address the drawing side).

## 7. Gates
- Debug build in `build` (`cmake --build build -j 4 --target openttd_test > /tmp/claude-1000/fsa-build.log 2>&1`, grep for `error`).
- `tools/junction/jt fast build '[planarshape]'` and `tools/junction/jt fast build '[planardraw]'`.
- Release build in `build-rel` (-j 4) then `JT_JOBS=4 tools/junction/jt gate build-rel`. Known: about 31 s, jt prints OVER BUDGET at 30 s (not new); `FAILED:` lines inside passing cases (junction_joint, planar_structure:686, junction_real_terrain:397) are not new.
- Measurement (hidden, Release): `build-rel/openttd_test "planar draw: PSG 197 (Shape's own shape)" > /tmp/claude-1000/fsa-197.log 2>&1`; report the line starting `197 dummies` (before this change it was 878 dummies; expected about 48). The draw lines in that test are not meaningful until FS-b; report them but do not act on them.
- One commit: `Change: Junction Shape refines cells with flex pairs at size 0 (D407)`, body naming the flag and the two tests; end with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report (short, this format)
- References read
- Commits: hash and subject
- Red evidence: each new test and the failure text seen before the change
- Types and members added or removed: names only
- Suite results: per gate, count and time; any failure you did not cause
- 197: the `197 dummies` line before (878) and after
- Stops, and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
