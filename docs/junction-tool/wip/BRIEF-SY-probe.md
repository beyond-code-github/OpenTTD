# Brief SY (probe, throwaway): force T3's EF split low and see what refuses

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Run `git switch -c probe-sy probe-fl` (probe-fl is at {HEAD}); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes. Solve runs `timeout -s KILL 30`; one run at a time. Do NOT run any IIS (`PLANAR_IIS`): the orchestrator runs those.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Background: `wip/ANALYSIS-PD.md` and `wip/PROBE-FL.md`. With `PLANAR_LATE=rcg` (prio fray rise may be 0, E's core may climb after its bridges, fray gaps at least) T3 o1t3f1s0 still climbs all three EF lanes right after the west port and splits them high (rows 9, 11, 13 at tiles x 124-132 in the built frame), where the engineer split them low (rows 14, 20, 21) and climbed E after its bridges. We want the rule that keeps the split high.

## Change (planar_components.cpp, in `Fray`, after the `fray gap` rows)
When `PLANAR_SPLITLOW=<K>` is set (integer, half-tile units) and the fray is a prio fray: for each lane k, one row under `TagScope("split low")`: `Ge(Dot(r, sp[k]) - Dot(r, sp_ref), K)` is NOT what we want; instead use the y axis directly: the split point's y minus the y of the end point of the strand `E1` (the E channel's first line's end, `M.End("E1")`) is at least K: `Ge(sp[k][1] - M.End("E1")[1], K)`. If `E1` has no strand when `Fray` runs, add the rows at the end of `Build` instead (keep the split points in a member `std::vector<Point> split_pts; ///< Prio fray split points (probe).`), and say which you did.

## Measure (T3 test `"planar components: T3 from its topology text (mapped) at the hand composition's area"`)
`PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 PLANAR_LATE=rcg <env> timeout -s KILL 30 build-rel/openttd_test "<T3 test>" -s > <S>/sy-<name>.txt 2>&1`
1. `PLANAR_LATE=rcg` alone: 1813.25 expected; record the solved `sp[k][1] - E1end[1]` for k = 0..2 (print them under `PLANAR_TIMES` as `split dy <k> <value>`).
2. `PLANAR_SPLITLOW` = that minimum + 2, + 6, + 10, + 14 (half units): area, size, track, or failure with its reason (`base infeasible`, ladder failure, clash not separable, budget).
3. Without `PLANAR_LATE` at the first `PLANAR_SPLITLOW` value that fails in 2: result.

## Do not
- Run `PLANAR_IIS`. Change anything else. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-sy: `Probe: Junction prio fray split held low (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Reply with the measurements, the exact command line of the first failing run, the commit hash, `git status --porcelain`. When you have reported, stop.
