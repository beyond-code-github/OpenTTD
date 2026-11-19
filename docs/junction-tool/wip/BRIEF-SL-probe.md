# Brief SL (probe, throwaway): which rules hold T2's faces open

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Run `git switch -c probe-sl probe-cm` (probe-cm is at {HEAD}); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes. Single-variant runs `timeout -s KILL 30`. One run at a time.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad` (`mkdir -p` it if missing).
- Background: `wip/PROBE-CM.md`. Charging face slack (`PLANAR_FACEW`) barely moves it: T2 o1t3f1s0 slack 536.5 tiles with no weight, 463 at w=30, with area worse. So rules, not the objective, hold the faces open. We want those rules named.

## Read first
- On probe-cm: `RungSpec`, `RungExpr`, `RungCon`, `LadderRungs`, the ladder pass (`PLANAR_FACEW`, the `slack` print), `LadderFace` and where `ladder_faces` is filled in `Build`; the `PLANAR_IIS` print and `ExplainInfeasible` call sites.

## Changes
1. `RungSpec` gets `size_t face = 0; ///< Index of the ladder face the rung came from.`, set in `LadderRungs` from the face loop index.
2. Under `PLANAR_TIMES`, after the `slack` line, print per face with a non-zero slack, sorted by slack descending, at most 8 lines: `face <i> slack <tiles> rungs <n> strands <distinct strand names of its rungs' pt and tgt, sorted, comma-joined>`.
3. When `PLANAR_SLACKCAP` is set (double, tiles; `std::from_chars`), in the ladder pass after the rung rows: one row `Ge(2 * cap - sum over rungs of (RungExpr - k), 0)` under `TagScope("slackcap")`. When `PLANAR_SLACKFACE` is also set (integer face index), the sum runs over that face's rungs only.

## Measure
Command: `PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 <env> timeout -s KILL 30 build-rel/openttd_test "planar components: T2 from its topology text (mapped) at the probe's area" -s > <S>/sl-<name>.txt 2>&1`.
1. No env: 932.25 75x31, slack 536.5; record the face lines.
2. Whole sum: `PLANAR_SLACKCAP` = 500, 460, 420, 380, 340 until no variant solves or the ladder pass fails (the variant then keeps the rough result: tell which from the output). Per step: area, size, slack, or failure. At the first cap whose ladder pass is infeasible, rerun with `PLANAR_IIS=1` and save the `IIS` lines sorted unique to `<S>/sl-iis-all.txt`. If the IIS print does not run for a ladder pass failure, stop and report (do not add a call).
3. Per face: for each of the three largest faces from 1, `PLANAR_SLACKFACE=<i>` with `PLANAR_SLACKCAP` = that face's slack x 0.8, then x 0.6, then x 0.4, until it fails; IIS of the first failure to `<S>/sl-iis-f<i>.txt`.

## Do not
- Change anything else (rungs' rows, objective, clash loop, slot layer, signal pass, checker).
- Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-sl: `Probe: Junction face slack caps for rule diagnosis (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-SL.md` (do not commit): the face table, the cap tables, and per IIS file the rows grouped by tag (with the tag's source location if the tag carries one) with counts and the distinct strand names.
- Reply with those, the commit hash, `git status --porcelain`.

When you have reported, stop.
