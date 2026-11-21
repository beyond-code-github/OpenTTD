# BRIEF H14-D403: legs of one node exempt from the spine; H14 joins Shape's promise

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Budget
45 min wall total. No rerun loops; one gate run. Builds `-j 4`, JT_JOBS=4.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel` (reuse it).
- Ruling D403 in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md`: two lines of one channel that are legs of the same node (they end at one merge or start at one split) are exempt from H14 and the spine rows; then H14 joins the promise.

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_shape_layer.cpp` lines 155-205 (`SpinePairs`), 815-895 (`CheckH14`, `H14_IN_PROMISE`), 1000-1030, 1370-1380 (spine rows).
- `src/tests/junction/planar/planar_shape_layer.cpp` lines 1755-1830 and 1850-1910.
- `src/junction/planar/CLAUDE.md`, the "Spines (H14, E3-S2)" bullet.

## 3. Tests (red first; record the red output)
- Outer (hidden `[.][planarpsg197]`, existing "planar shape: H14 on PSG 197 (measured)"): add `CHECK(different + differ == 0);` for the label "V" as it already exists for "Shape". Red before the change: V's channel 61 (D23c and D12c, legs of merge D123) differ; record the WARN lines.
- The existing hidden "planar shape: V's shape of PSG 197 through Shape's checker (E1h)" must stay as green as it is now after H14 joins the promise; run it before and after and report both outcomes.

## 4. Changes
1. `SpinePairs`: a pair is formed per channel for each member m >= 1 with the FIRST earlier member u (in `members` order) such that u and members[m] are not legs of one node; when every earlier member is such a leg, members[m] gets no pair. Two tracks are legs of one node when some node of `map.expanded.nodes` has both in its `in`, or both in its `out`. Update the function's doc comment (one sentence, citing D403).
2. `H14_IN_PROMISE = true`. Update its comment and `CheckH14`'s comment ("measured, not yet in the promise" goes).
3. `src/junction/planar/CLAUDE.md`: the Spines bullet states the D403 exemption and that H14 is part of `CheckPlanarShapePromise`; remove "it is not yet part of `CheckPlanarShapePromise` (V's PSG 197 diverges on channel 61)".

## 5. Do not
- Do not change the spine rows' form, the H14 messages, the node-group crossing exemption, or any other check.
- Do not exempt anything beyond legs of one node (the toggle candidate in D403 is not this step).

## 6. Stop conditions
- A fast `[planarshape]` test or the `[slow]` H14 measured test goes red with the flag on: report the messages; do not fix.
- Shape's time on T2/T3/LL2 (the slow H14 test's lines, or `BuildPlanarShape` times printed by existing tests) rises by more than 20 %: report the numbers before committing.

## 7. Gates
- Before and after: `tools/junction/jt fast build-rel "[planarshape]" 10`; `build-rel/openttd_test "[planarpsg197]" > /tmp/claude-1000/h14-197.txt 2>&1` (hidden; report the H14 197 lines and every failed CHECK; the known reds there are the 197 structure test's 14 `gap:` CHECKs, by design); the `[slow]` H14 measured test by name: `build-rel/openttd_test "planar shape layer: H14 every channel's lanes follow one spine (measured)" > /tmp/claude-1000/h14-slow.txt 2>&1`.
- `tools/junction/jt gate build-rel` once at the end.
- One commit: `Change: Junction Shape: legs of one node leave the spine; H14 joins the promise (D403)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report (short)
- References read; Commits; Red evidence; Members changed (names only); Suite results (count and time per gate, and the H14 lines of T2/T3/LL2 and 197 before and after, with Shape times); Stops; State (`git status --porcelain`, `git log -1 --oneline`).

When you have reported, stop.
