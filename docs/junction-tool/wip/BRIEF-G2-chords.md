# BRIEF G2: keep-apart chords on T2 (probe, Opus)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (G1g: G1's rows audited against their own geometry and fixed). Release `build-rel`.
- Implements /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-slots-lengths.md §9 (keep-apart chords, D381) on G1's model (§8; code in src/tests/junction/planar/planar_slots_probe.cpp, `SolveG1`). Read §8, §9 and DECISIONS D380, D381 once.

## Goal
Find out whether chords from reflex vertices to their front (§9.3), written before the solve, make G1's solutions clash-free on T2's top 3 variants, and at what cost in time and feasibility. You are trusted with the reasoning inside §9; §9.3's front rule is the one open item. If a case shows the rule is wrong, do not invent another rule: report the face, the vertex, its turn sequence and what the rule picked, and stop at a clean state.

## Changes (planar_slots_probe.cpp only)
1. **Face walks at piece level** (§9.1): from `WalkFace`'s order (planar_shape_layer.cpp lines 85-122; copy the walk, it is in an anonymous namespace; do not export it) build per face the list of wall pieces (track, piece, direction, heading as walked, the expressions for its start point and length in `SolveG1`'s variables, step diagonals and stubs included as pieces that exist conditionally) and the box-edge pieces, with each vertex's turn.
2. **Chords** (§9.2-9.4): per reflex vertex its front by §9.3; rows of §9.4 with `RungSpacing` (planar_model.h) for the gap; conditional chords relaxed by 1000 (1 - z). `off` bit 16384 removes the chords. Count `chords`, `conditional`, `front_missing` (flagged faces) per variant.
3. **Output:** the G1 line gains `chords=<n> conditional=<n> front_missing=<n>`. When a run has clashes, for each G1C line add the face of the pair if any and, when both pieces lie in one face, whether a chord in that face ends on either piece.
4. **Test:** a new test `"planar probe: G2 chords on T2"` (`[.probe]`): T2's first 3 variants; per variant the full solve with chords, the full solve without (bit 16384), and with beside rows off (bit 1) both ways (T2's known beside-versus-clear conflict). Print G1 and G1C lines as G1 does, tagged `G2`.
5. **Oracle check:** for each solved run with chords, verify on the geometry that each chord's landing point lies on its front piece and that L >= gap (an audit line `G2 AUDIT ...` per breach, as G1g's audit does).
- Commit: `Add: Junction planar probe: G2, keep-apart chords from reflex vertices (D381)`.

## Run (Release, one at a time)
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: G2 chords on T2" > g2-run.txt 2>&1; grep '^G2' g2-run.txt > g2.txt`
- `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`

## Do not
- Add rows of any other kind; read a solution to decide a row; loop solves; change G1's rows; touch any file but the probe.

## Stop conditions
- §9.3's rule gives a front that the geometry contradicts in a way you can name (above): report and stop.
- A face whose walk cannot be built at piece level (a stretch with no Shape pieces, an unusable walk): report the face and stop.

## Report (short)
- Commit; per variant and run: solved, ms, steps, W x H, clashes (same_channel/face/noface), curve, chords/conditional/front_missing; the first 5 G1C lines of each solved chord run with their chord annotation; audit lines; gate (count, time); your reading in at most 5 lines (does the front rule hold, where it fails); `git status --porcelain`, `git log -1`.
When you have reported, stop.
