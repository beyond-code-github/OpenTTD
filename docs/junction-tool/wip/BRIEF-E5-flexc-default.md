# BRIEF E5-flexc: flex C as Shape's default flex rule (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head: given in the launch message. Release build `build-rel`, `-j 4` only.
- Source: DECISIONS D420 (1): flex C is the default flex rule: flex on every one-piece free stretch (D402) and on the first and last piece of every multi-piece free stretch. D402 stays as the toggle (`end_pieces = false`). D420's reason: C hurt only convex cells, which D416-D418 retired as the keep-apart model.
- Measured before this brief (orchestrator): flipping only the default of `PlacePlanarShapeFlex(..., bool end_pieces = false)` to true makes 7 gate cases fail, all in `src/tests/junction/planar/planar_shape_layer.cpp` (lines 331, 463, 490, 504), e.g. "the one-lane T is shaped at the least turning": "15 messages, first: H1: track CD1 flex at piece 1 is not on a free stretch".

## 2. Read first (once each, offset/limit)
- `src/junction/planar/planar_shape_layer.cpp` lines 940-1000 (`FreeOnePieceStretch`, `PlacePlanarShapeFlex`) and lines 480-520 (the H1 flex check in the promise checker, with `First`, `Last`, `FlexRemoved`; read their definitions too).
- `src/junction/planar/planar_shape_layer.h` lines 150-162.
- `src/tests/junction/planar/planar_shape_layer.cpp` lines 296-340 (the D405 test).
- `src/junction/planar/CLAUDE.md` line 193 (the flex bullet).

## 3. Tests (red before green)
1. **Outer, existing:** the 7 failing cases above go red when only the default is flipped (step 4 a first); record the failing case names and first messages. They are the red evidence.
2. **New, inner, `[junction][planar][planarshape]`:** "planar shape layer: H1 accepts flex on the end pieces of a free stretch (D420)": shape the one-lane T (`PlanarTText(1)`, as the least-turning test does) with the new default; REQUIRE at least one track with a flex middle on a stretch of two or more kept pieces (if none, STOP and report); CHECK `CheckPlanarShapePromise(...)` has no message starting with "H1:". Red: with 4 a done and 4 b not yet, the H1 message appears.

## 4. Changes
a. `planar_shape_layer.h`: the default `end_pieces = true`; its @param: "Flex C (D420, the default): a free stretch of two or more pieces also gets the pairs, on its first and its last piece only; false is D402 (one-piece stretches only), the toggle."
b. H1 (planar_shape_layer.cpp, the `free` computation): a flex at middle i is on a free stretch when, for the stretch k that holds it, `FreeOnePieceStretch(v, t, k)` and either kept == 1 (as today) or i's original piece is the first or the last kept piece of the stretch, i.e. every j in [First(t, k), i - 1) is FlexRemoved or every j in (i + 1, Last(t, k)] is FlexRemoved. Check the FlexRemoved semantics you read: if the original piece of a flex group is not i - 1 (the kept piece), STOP and report what it is. Keep the message text. Accepting both rules (C is a superset of D402) is intended.
c. The D405 test: in the PSG 197 part, change the per-dummy `CHECK(monotone)` for faces 16 and 17 to a WARN-only measurement (keep the existing WARN line; remove the CHECK and the `checked` assertion that depends on it, if any), with a comment: "Measurement since D420: flex C breaks the monotone landing on 197 faces 16 and 17 (D413); convex cells are not the keep-apart model (D416-D418)." The T2/T3 part of the test is unchanged.
d. `src/junction/planar/CLAUDE.md` line 193: say the default is flex C (D420): every free one-piece stretch, and the first and last piece of every free multi-piece stretch; D402 is the toggle (`end_pieces = false`).

## 5. Do not
- Do not change `PlacePlanarShapeFlex`'s placement logic, `FreeOnePieceStretch`, cells refinement, or any other promise.
- Do not touch the drawing program or its tests, except to run them.
- Do not turn any other CHECK into a WARN.

## 6. Stop conditions
- After 4 a-c, any gate failure other than the known flake "planar components: T2 mapped lands in its box at the anchor" (rerun that alone and report).
- FlexRemoved semantics differ from 4 b's reading.
- The new test finds no multi-piece flex on the one-lane T.

## 7. Gates
- `tools/junction/jt fast build-rel '[planarshape]' 5`; `tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Over budget: STOP.
- One commit: `Change: Junction: flex C is Shape's default flex rule (D420)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 8. Report (short, fixed format)
- References read
- Commits: hash and subject
- Red evidence: the 7 cases' names and first messages after 4 a; the new test's failure before 4 b
- Types and members added or removed: names only
- Suite results, naming any failure you did not cause
- Stops and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
