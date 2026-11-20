# BRIEF G2d: neighbour rows in frames (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD}. Release build dir `/home/pete/OpenTTD/.claude/ng1/build-rel` (already built at that head). One file changes: /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_slots_probe.cpp.
- Implements D381's first half (parallel neighbours one lane apart, "valid by construction": rows read from the track map and Shape's headings before the solve; no solution is read, no loop) inside SPEC-slots §2.1's frames. Read once: /home/pete/OpenTTD/docs/junction-tool/DECISIONS.md rows D381 and D383 (grep `^| D381 ` and `^| D383 `); /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-slots-lengths.md §9 and §10; /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-slots.md lines 63-68 (§2.1 Frames).

## What exists (read in the probe, do not change its behaviour)
- `SolveG1Raw`'s G2 block (search `13. G2 (SPEC-slots-lengths 9`): per face it builds `wall`, a vector of `WallPiece` (kind Part/Stub/Step/Cusp/Flex/Box, `track`, `elem`, `stretch`, `start` (two `Lin`), `length`, lifted walked `heading`), with the face on the left of every wall. `lifted(idx)`, `node_of(p)` (with off bit 268435456 it uses `leg_start_node`/`leg_end_node`), `piece_text(idx)` and `row(Lin, lower, upper, tag)` exist there. Elements: `te[track].elems[elem].piece` is the Shape piece index of a wall element.
- The test `"planar probe: G2 chords on T2"` (search it) runs T2's first variant with the `runs` array.

## Changes
1. **New off bit 2147483648U (neighbour rows ON when set; default off so older runs are unchanged).** Inside the per-face loop of the G2 block, after the chord loop, when `(off & 2147483648U) != 0`:
   - **Sections.** For each index i in the wall (cyclic, 0..nw-1), find a section: a connector of either form, with the wall pieces just before and just after it both belonging to tracks (not Box):
     - **Box section:** wall[i] is Box. Near piece A = the last non-Box wall before i (walking backwards over consecutive Box pieces), near piece B = the first non-Box wall after i (walking forwards over consecutive Box pieces). Only if A.track != B.track.
     - **Deck section:** wall[i] is a Part of track S, and wall[i-1] and wall[i+1] belong to other tracks A.track and B.track (each != S, A.track != B.track), and the corners i-1 -> i and i -> i+1 are crossing corners (both tracks cross S there: check `map.crossings` for a crossing between A.track and S and between B.track and S). Near pieces A = wall[i-1], B = wall[i+1].
   - **Groups.** Group consecutive walls of one track by Shape piece: a group is a maximal run of walls with the same `track` and the same `te[track].elems[elem].piece`; its heading is the heading of its first Part element (Shape's piece heading as walked; steps, cusps, flex, stubs never decide the heading). Walking outward from the section: A's groups backwards from A (indices decreasing), B's groups forwards from B (indices increasing). Stop when either side leaves its track (track changes) or reaches the other side's group (the two walks meet).
   - **Pairing.** Pair A-group k with B-group k (k = 0, 1, ...) while `lifted heading(B-group) - lifted heading(A-group)` is congruent to 4 mod 8 (antiparallel). Stop at the first k that is not; count it in `frame_open`.
   - **Rows per pair.** Let h = A-group's heading mod 8 and n = `PlanarModel::STEP[(h + 6) % 8]` (A's left normal, into the face). Let A_near = the point where A-group ends as walked (the end of its last wall: that wall's `start` plus its `length` times `STEP[heading]`, written with the existing Lin helpers), A_far = A-group's start (its first wall's `start`); B_near = B-group's first wall's `start`, B_far = B-group's end. Write `Dot(B_near - A_near, n) >= 2` (tag `nbr f<face> <count> near`) and `Dot(B_far - A_far, n) >= 2` (tag `nbr f<face> <count> far`), unconditional.
   - **Exempt (count `nbr_exempt`):** the far row of a pair when `node_of` of A-group's far wall and of B-group's far wall are the same node (two legs meeting at a node), and every row whose A and B tracks are the same track.
   - Count `nbr_pairs` (pairs written), `nbr_rows`, `nbr_exempt`, `frame_open` in `G1Result` (add these four `size_t` members next to `chords`, with one doc comment line).
2. **Output.** The G2 test line prints `nbr_pairs= nbr_rows= nbr_exempt= frame_open=` after `between_reflex=`.
3. **Test runs.** Replace the `runs` array of `"planar probe: G2 chords on T2"` with exactly: `{"chain+allkinds", 268435456U | 1073741824U}, {"chain+allkinds+nbr", 268435456U | 1073741824U | 2147483648U}, {"nbr only", 33554432U | 2147483648U}`. (33554432 = no chords; the neighbour rows must then still be written: if the G2 block is skipped when 33554432 is set, move only the neighbour part so it runs whenever 2147483648 is set, and say so in the report.)
4. **Commit:** `Add: Junction planar probe: G2d, neighbour rows in frames (D381)`.

## Red / evidence
- This is a probe: no red test. Evidence is the run lines. Before the commit, run the test once with env `JUNCTION_G2_DIAG=1` and save the output to a file; if any run is infeasible, report the first 10 `nbr` rows in its conflict (`why=`) and the face walk of their face (`G2 DIAG FACE` lines print faces only for chord conflicts: add one line per conflicting `nbr` face in the same format, nothing more).

## Run (Release, one at a time)
- `cmake --build /home/pete/OpenTTD/.claude/ng1/build-rel --target openttd_test -j8 > /home/pete/OpenTTD/.claude/ng1/build-rel/g2d-build.txt 2>&1`, grep it for `error`.
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 200 ./openttd_test "planar probe: G2 chords on T2" > g2d-run.txt 2>&1; grep '^G2' g2d-run.txt > g2d.txt`
- `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`

## Do not
- Change any existing row, chord or default; read a solution to decide a row; add a loop of solves; use `std::stoi`, `std::isdigit` or other functions `safeguards.h` forbids (the build fails on them); touch any other file.

## Stop conditions
- A face walk where a section's near pieces cannot be found as described, or a group with no Part element: report the face (`piece_text` of its walk) and stop.
- The neighbour rows make a run infeasible: do NOT change the rule; report as in "Red / evidence" and stop after committing.

## Time box
- Total about 1 hour. Each solve has the probe's 15 s cap. No rerun loops: at most one diagnostic run beyond the planned run.

## Report (short)
- Commit hash; per run: solved, ms, steps, W x H, clashes (same_channel/face/noface), curve, chords, nbr_pairs/nbr_rows/nbr_exempt/frame_open; any conflict rows with their face walk; gate (count, time); `git status --porcelain`, `git log -1`.
When you have reported, stop.
