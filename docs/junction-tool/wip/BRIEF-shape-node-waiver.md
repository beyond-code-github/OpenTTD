# BRIEF shape-node-waiver: a Line leg always turns at its node, also at copy and row nodes

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Finding (`docs/junction-tool/wip/probes/slots/flank-lock.md`, last section): on T2 (Shape variant 0), node 10 is the prio join of D1 into E1. Its in lines are prio:E1 (Row), E1 (Line, straight) and D1 (Line, the joiner from the right); its out line is ED1. Shape gives D1 the trunk heading h2 at the node, the same as E1. The game cannot build two legs meeting at one point on one heading. The judge's node-turn check (F8) refuses every drawing of it, so Slots can never draw T2.
- Cause: the node rule (the straight leg at the trunk heading, every other leg one eighth toward its side, no cusps; D404) is waived for EVERY leg at a node holding any Copy or Row track:
  - program side, `src/junction/planar/planar_shape_layer.cpp` about line 1457: `topology` is true only when every track at the node is a Line, else each non-straight leg gets a binary `d`;
  - oracle side, the same file, about line 585: the cusp is reported only at such a "topology" node.
- The rule intended (H8): only a BESIDE track (a Copy or a Row) may keep its parent's heading. A Line leg that is not the straight leg always turns.

## 2. Read first (once, with offset/limit)
- `CODINGSTYLE.md`.
- `src/junction/planar/planar_shape_layer.cpp` lines 540-600 (`CheckH3`) and 1425-1500 (the node rows in `ShapePlanarStructureVariant`).
- `src/junction/planar/CLAUDE.md`, the Shape layer bullets on H3/H8 and on `ShapePlanarStructureVariant`'s node rule.
- Existing Shape tests: `src/tests/junction/planar/planar_shape_layer.cpp` (find the test that checks the promise on T2, T3 and LL2 over the kept variants, and its tag).

## 3. Tests, outermost first (red before green)
In `src/tests/junction/planar/planar_shape_layer.cpp`, a new test `"planar shape: a Line leg turns at its node, also where a copy or row meets it (no cusps, D404)"`, with the same tags as the existing T2/T3/LL2 promise test. Over every kept variant of T2, T3 and LL2 (the same build as that test):
- for every site, for every leg that is not the straight leg and whose track kind is Line, CHECK that its heading at the node differs from the trunk's (mod 8), with a message naming the hub, variant tag, node and leg.
- Red first: with today's code this fails on T2, at node 10, leg D1 (and possibly elsewhere: record every failure line). Record the failure text.

## 4. The change
In both places, replace the node-wide "topology" test with a per-leg test:
- program: `d` is `NONE` (the leg turns, d = 1) when `map.tracks[leg].kind == PlanarTrack::Kind::Line`; a binary only when the leg is a Copy or a Row. Keep the rest of the rows as they are. Update the comment.
- oracle (`CheckH3`): report the cusp when the leg's kind is Line, whatever else sits at the node; a Copy or Row leg may keep the trunk heading. Update the comment.
- Update `src/junction/planar/CLAUDE.md` in the same commit: in the `CheckPlanarShapePromise` bullet, "(at a topology node, one where every track is a Line; a copy or row node may keep the trunk heading, H8...)" becomes the per-leg rule. In the `ShapePlanarStructureVariant` bullet, "at a copy or row node a binary d keeps the beside track on its parent's headings" becomes "a beside leg (Copy or Row) has a binary d...; a Line leg always turns".

## 5. Do not
- Do not change Structure, flex placement, the drawing program or any other rule. Do not relax the oracle anywhere else.
- If a variant becomes unshaped (H10) or the promise test fails elsewhere, do not work around it: stop and report (variant, why).

## 6. Stop conditions
- Any T2/T3/LL2 kept variant becomes unshaped, or `CheckPlanarShapePromise` reports anything new on any of them.
- The red test passes before the change.
- Anything not covered here.

## 7. Gates
- Debug `build`: `tools/junction/jt fast build '[planar]'`. The pre-existing time is 6.03 s; stop above 6.5 s. Run the Shape tests' own tag with `jt fast` too, at its TESTS.md budget.
- `tools/junction/pv check build-rel T2`, then T3, then LL2 (Release). Changed variants are EXPECTED where a joiner's heading changes. List every changed variant with its old and new area or reason. A variant that changes from shaped to failing is a stop.
- Release `build-rel`: `tools/junction/jt gate build-rel`. Give the count and time. A known intermittent SIGSEGV/SIGILL under parallel load: rerun that test alone, and report it.
- Then rerun the T2 probe that found it, once: `JUNCTION_MILP_INCUMBENT=1 PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 PLANAR_SLOTS_HUB=T2 PLANAR_STEP2B_OUT=docs/junction-tool/wip/probes/slots/node-waiver.md build-rel/openttd_test '[slotsstep2]'`. Report its stage A and node-turn lines; T2's stage A may still find nothing in 15 s, which is a finding, not a stop.
- Commit: `Fix: Junction: a Line leg turns at its node also where a copy or row meets it (D404)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report (short)
References read; commit; red evidence (every failure line); the pv changes per hub; jt times; gate count; the T2 probe lines; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
