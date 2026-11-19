# Brief SH (probe, throwaway): what holds T2 o1t3f1s0's box open

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Run `git switch -c probe-sh probe-pr` (probe-pr is at {HEAD}); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes. Every run: `timeout -s KILL 30`. One run at a time.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
T2 o1t3f1s0 solves at 932.25 (75x31) with the prio row strands, 834.25 (67x31) when rows are exempt from clashes (`PLANAR_ROWFREE=1`, invalid). We want the constraints that stop the box getting narrower or lower.

## Read first
- `src/junction/planar/planar_components.cpp` on probe-pr around 1840-1900 (box `M.X0`, `M.X`, `M.Y0`, `M.Y`, port sides, anchor) and the `PLANAR_IIS` print (grep `PLANAR_IIS`); `PLANAR_TIMES` prints.

## Change (one place, next to the port side rows)
Under `TagScope ts("cap")`: when `PLANAR_MAXDX` is set, `M.Add(Ge(<value> - (M.X - M.X0), 0))`; when `PLANAR_MAXDY` is set, likewise with `M.Y - M.Y0`. Values are integers in the model's half-tile units. Under `PLANAR_TIMES`, after each variant's final solve, print one stderr line `box dx <M.X - M.X0> dy <M.Y - M.Y0>` from the accepted solution. If reading the solved value of `M.X` needs anything but the existing evaluation helper (`Ev` or the like) used elsewhere in the file, stop and report.

## Measure
Command: `PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 <env> timeout -s KILL 30 build-rel/openttd_test "planar components: T2 from its topology text (mapped) at the probe's area" -s > <S>/sh-<name>.txt 2>&1`; take area and size from the `variant o1t3f1s0 area ...` line, dx dy from the `box` line, failure from `no variant solves` and its reason.
1. Baseline (no cap): must be 932.25 75x31; record dx0, dy0.
2. Width: `PLANAR_MAXDX` = dx0 - 2, dx0 - 4, ... until no variant solves (at most 10 steps). Per step: area, size, ms, or failure. At the first failure rerun with `PLANAR_IIS=1` and save the `IIS` lines sorted unique to `<S>/sh-iis-dx.txt`.
3. Height: same with `PLANAR_MAXDY` from dy0 (save `<S>/sh-iis-dy.txt`).
4. With `PLANAR_ROWFREE=1`: baseline dx dy, then step 2 only (save `<S>/sh-iis-dx-free.txt`).
If a failure is "budget" rather than infeasible, record it as such and do not run the IIS for it.

## Do not
- Change anything else in the composer, the slot layer, the signal pass or the checker.
- Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-sh: `Probe: Junction box caps for constraint diagnosis (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-SH.md` (do not commit): the table per measurement and, for each IIS file, the rows grouped by tag with counts plus the full list of distinct strand names they mention.
- Reply with the tables, the IIS summaries, the commit hash, `git status --porcelain`.

When you have reported, stop.
