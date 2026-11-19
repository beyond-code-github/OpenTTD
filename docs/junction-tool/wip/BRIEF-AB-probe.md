# Brief AB (probe, throwaway): slot steps absorbed into the lanes' own diagonals

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, now on `probe-et` (= probe-fl2 plus box caps). Run `git switch -c probe-ab probe-et` (probe-et is at {HEAD}); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. Single-variant runs `timeout -s KILL 30`; pv `timeout -s KILL 600`; end to end `timeout -s KILL 60`. One run at a time. No `PLANAR_IIS`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
The engineer, on T3 with `PLANAR_LATE=rfg` (built from `<S>/fl2-T3.json`): "the blue tracks all rise and then fall again when they start the main diagonal". The per-line dump `<S>/fl2-lines.txt` shows it: route E2 runs `EF2:E x17, NE x10, E x6, E2:E x8, NE x2, E x7, NE x8, E x3, SE x2, E x3, SE x10, ...` and E3 likewise: a one-slot S-bend (NE x2 ... ) just before the late climb (NE x8) and another (SE x2) just before the descent (SE x10). These are slot steps (`SlotBend`, an axis piece then a 45-degree piece; "a slot step on a diagonal piece is not composed"), made beside the lane's own diagonal instead of in it; and the fray late climb (`f`) ties all lanes' climb diagonals equal (`fl_climb`, `Eq(L - L0, 0)`), so a lane cannot climb one slot more or less than another.

## Read first
- `planar_components.cpp` on probe-et: `Fray` (prio case, `fl_climb`, the `fray late climb` tie), `SlotBend`, every `SlotBend` call site, and which call emits E2's and E3's `NE x2` right after their fray legs and their `SE x2` before the descent (find them by running `PLANAR_LATE=rfg PLANAR_ONLY=o1t3f1s0 PLANAR_LINEDUMP=...` once and matching segment indices; record the call sites in the report).
- `src/junction/planar/CLAUDE.md`, "Slot layer" (prio fray straight legs off a non-far lane fixed one slot toward the far lane; the step back after doubled crossings).

## Changes (behind a new letter `a` of `PLANAR_LATE`)
1. In `Fray`, with `a` and `f`: drop the equal-length tie of the late climb diagonals (each lane's climb diagonal is free, >= 0).
2. With `a`: at the `SlotBend` call that emits a lane's slot step immediately before or after a diagonal segment of that same lane whose length is free (the late climb of 1, or an existing free-length diagonal that follows directly), do not emit the S-bend; instead require the lane's offset after that diagonal (the `Dot(Right(h), point)` relation the S-bend would have produced, relative to lane 0) to equal the target offset. If a call site's target offset cannot be expressed that way without new geometry, leave that site unchanged and list it.
If a change needs a new type, member or a change to the slot layer, stop and report.

## Measure (T3 test `"planar components: T3 from its topology text (mapped) at the hand composition's area"`, T2 test `"planar components: T2 from its topology text (mapped) at the probe's area"`)
1. `PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 PLANAR_LATE=<x>` for x = `rfg` (baseline 1692.25) and `rfga`: area, size, track, ms or failure; for `rfga`, E1-3 heading runs as above from a line dump.
2. `pv run build-rel T3` and `pv run build-rel T2` with `PLANAR_LATE=rfga` (`--out <S>/ab-pv-<hub>.txt`): built/failed with reasons, best variant and area, max ms; side by side with `<S>/fl2-pv-T3.txt` / `<S>/fl2-pv-T2.txt`.
3. End to end T3 with `PLANAR_LATE=rfga PLANAR_ONLY=o1t3f1s0 PLANAR_LINEDUMP=<S>/ab-lines.txt JUNCTION_COMPONENTS_TOPOLOGY=T3 JUNCTION_COMPONENTS_OUT=<S>/ab-T3.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/ab-e2e-T3.txt 2>&1`: plan result, `MAPPED`, `VIOLATION` lines grouped by rule.

## Do not
- Change the slot layer, paths, signal pass, checker, or composer rules other than the two above. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-ab: `Probe: Junction slot steps absorbed into free diagonals (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Reply with the call sites found, the sites changed and left, the measurements, the commit hash, `git status --porcelain`. When you have reported, stop.
