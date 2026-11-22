# BRIEF: E5 V's residual slack on PSG 197, decoded against V's real coordinates (read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 45 minutes in total. Each solve keeps the 60 s cap the test already uses. Run the hidden test once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Context: in the test "PSG 197 clearance rows evaluated on V's drawing" (src/tests/junction/planar/planar_draw.cpp), the section "sepV allfaces W1" (~line 2606) holds every track point within 1 half tile of V's real drawing (`vref`, `fix_window = 1`) and takes each pair's side from `full.points` (a base-rows drawing near V, not V itself). Its elastic slack is 57.667: sep 46.667 in 9 rows, node 8.333 (3 rows), cross 2.333 (2), end 0.333 (1). Log: docs/junction-tool/wip/probes/e5land/inward-197v-ed29dcf5d3.log lines 1184-1240. The "geo reference" base run already has slack 5.0, and "sepV W0" (window 0) is Infeasible on `piece L100 0`. The question is whether V's real drawing breaks our rows, or our rows measure V wrongly. This step prints the facts; the orchestrator interprets.

## 2. Read first (once each)
- The test above from `LoadPinned197`/`vref` construction (~2320-2400) to the end of the "sepV allfaces W1" block; `WarnSepWalls` (~1806).
- `src/junction/planar/planar_draw.h`: `sides_from`, `margin_tags`, `sep_margins` (format `"c<k> s(x,y) spacing <n> from i|j margin <m>"` joined by `"; "`), `stretched`, `points`, `fix_points`, `fix_window`.
- `PlanarShapeFaceWalls` / `PlanarFaceWalls` / `PlanarCellWall` (the wall's track, piece, heading).

## 3. Change: one new section in that test, behind env `PLANAR_PROBE_VRES`, placed after the "sepV allfaces W1" block. No production code change.
1. **Side source check.** Re-run the "sepV allfaces W1" options exactly, but with `sides_from = &vref` (V's own points). WARN `vres sides V: status <s>, total slack <x>, short_sides <n>` and the same family lines as the block prints. (Same options otherwise, including `fix_window = 1`.)
2. **Window check.** The same as step 1 with `fix_window = 0`; WARN status, slack, and on Infeasible the conflict tags (up to 40).
3. **Per stretched row of the step-1 run** (all families, up to 30, largest first):
   - For `sep <f> <i> <j>` rows: both walls as `<track>#<piece> h<heading>`; each wall's two endpoints on V (`vref[track][piece]` and `vref[track][piece + 1]`) and on the run's `r.points`; every candidate's margin on V (a separate run with `sides_from = &vref` and `margin_tags = {tag}` is fine; one extra solve per row is allowed within the cap, or collect all tags into one run's `margin_tags`, preferred); and the face walk from i to j and from j to i: each wall in between as `<track>#<piece> h<heading> len <V length in half tiles>` (length = max(|dx|, |dy|) of its V endpoints). Also WARN whether walls i and j share an endpoint on V (equal points) or their tracks meet at a node (same `from`/`to` node in the map), and the piece counts of both tracks.
   - For `node`, `cross`, `end` rows: the row tag, its slack, and the V points of every track the decoder (as in the "A real W1 stretched" lines, which print `node in L51 in L50 out L38`) names, for the pieces adjacent to that node or crossing (the point at the node/crossing and one point either side).
4. WARN one line per track pair summing slack (as `WarnSepGroups` does) for the step-1 run.

## 4. Do not
- No production change, no default change, no interpretation, no fix.

## 5. Stop conditions
- `vref` cannot be passed as `sides_from` (type or meaning differs): report what differs.
- A row family's tag cannot be decoded to tracks without production code: print the raw tag and say so (no stop).
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`.
- `PLANAR_PROBE_VRES=1 build-rel/openttd_test "PSG 197 clearance rows evaluated on V's drawing" > build-rel/vres.log 2>&1` (use the exact test name from the file). Copy to `docs/junction-tool/wip/probes/e5land/vres-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: V's residual slack on PSG 197 decoded, a read-out (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed.
- Suite result.
- Steps 1 and 2 lines verbatim; step 4 lines verbatim; for each of the 9 largest step-3 rows, its lines verbatim (join Catch's 80-column wraps first). The rest stays in the log.
- Stops and anything left undone. State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
