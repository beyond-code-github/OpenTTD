# Probe: one-lane merge exit steps as always-present diagonals allowed zero length

Throwaway probe: nothing is committed. Time box: 30 minutes of work; if you hit the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only the `openttd_test` target in `build-rel`, never `openttd`. Keep context small: outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/zerostep/`, code read with offset/limit, command output to files.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release build dir `build-rel`.
- Question: if every one-lane merge exit carries a diagonal step that is always present but may have length 0 (so the solver decides how far it steps, with no switch and no rule), what do LL² areas and times look like? The curve rows (`Model::PieceCurves`) still count its two bends even at length 0; that over-strictness is what we are measuring.
- Background (skim): `/home/pete/OpenTTD/docs/junction-tool/wip/ANALYSIS-merge-steps.md`.

## Read first
- `src/junction/planar/planar_components.cpp`: `Join` (~941-1007, the `exit_step` piece at ~994 is `M.AddSeg(o, Turn(h, exit_step), 2, ...)`), the per-node merge call (~1398, `PlanarMergeExitStep` / `PlanarInnerMergeExitStep`).
- `src/junction/planar/planar_model.h` `AddSeg`/`AddPiece` (~367-430) and `Model::PieceCurves` in `planar_model.cpp` (~84): confirm a required segment with `lo` 0 is accepted and still counted as a curve. If a zero-length segment is refused anywhere or breaks the solve outright, report that and stop.

## Change (an env knob, uncommitted)
- `PLANAR_ZEROSTEP` set: at the per-node merge call (~1398) pass exit step `+1` for every one-lane merge (port and inner), ignoring the two rule functions; and `Join` gives that step piece a least length of 0 instead of 2 (add one `int step_lo = 2` parameter to `Join`, passed as 0 from that call site only when the knob is set). Everything else unchanged (the `join exit` rows stay as for a non-zero step).

## Measure
1. `/home/pete/OpenTTD/tools/junction/pv run build-rel LL2 --tags /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt -j 8 --out <dir>/zs.txt` with `PLANAR_ZEROSTEP=1`.
2. `pv diff` against both baselines in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/2e2/`: `before.txt` (old build, every merge step optional: the free solver) and `after.txt` (HEAD, today's rule).
3. Report: solved counts; best area and tag for each of the three; median and max per-variant ms; median area change vs each baseline; counts better / worse by more than 0.5% vs each.
4. For the best zerostep variant and for o49t0f1s0 and o0t4f0s0: which merge steps came out at length 0 vs > 0 (print the step segment lengths from the solution, temporary print is fine).
5. If more than 5 variants that solve in `after.txt` fail under the knob: `PLANAR_IIS=1` core (first 30 lines) for up to two of them.

## Output
- Findings under 40 lines: `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-zerostep.md` (mark verified vs inferred).
- The probe diff: `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-zerostep.patch`.
- Restore: `git checkout -- .`, confirm `git status --porcelain` empty, rebuild `openttd_test`.
- Report in under 15 lines. When you have reported, stop.
