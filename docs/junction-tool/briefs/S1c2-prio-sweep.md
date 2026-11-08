# S1c2: prio timing: every ML arrival time, not just the coverage edge

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD`, branch `feature/junction-tool`, head given in your launch prompt. You are the only writer.
- S1c (`briefs/S1c-prio-timing.md`, commit 173c444302) added `JunctionPrioTimings` / `SimulatePrioTiming` in `src/junction_throughput.cpp`.

## Why (the S1c model was wrong; this is settled)
S1c started the ML train only at the coverage edge. Delay is not monotonic in the ML train's start: an ML train just outside the coverage reaches the merge block before the slow joiner, so it is not delayed; an ML train further back arrives while the joiner is in the merge block, and is delayed. Any ML start at or beyond the coverage edge is possible (the Entry does not see it, so the joiner gets green). On the generated junction (`prio_tl` = 1, coverage 4 tiles) S1c reported no delay only because it tested the one start that wins the race.

## Corrected model (implement exactly this)
- The joiner's run does not depend on the ML train (depth 1; the ML train ahead of the joiner can only hold the joiner, which is not an ML delay). So run the joiner once, exactly as S1c does now (same start, stepping, body, blocks, virtual blocks, end condition, tick cap), and record for each block the **held interval** `[first tick, last tick]` in which the joiner body occupies it. If a block is held in more than one disjoint run of ticks, keep each interval.
- The ML train runs at constant `design_speed` (stepped with `StepJunctionTrain` from speed `design_speed`, as now). Precompute its distance after n ticks, `d(n)`, for n up to the joiner's end tick.
- ML starts are positions `x <= c`, where `c` = m's progress at the merge piece minus the coverage (no clamp at 0; negative positions are upstream of the layout, where there are no joiner-held blocks). For a signal on m's path (real, or virtual beyond the exit) at position `s` with block `B` beyond it, the ML head crosses it at the first tick `n` with `x + d(n) >= s` (crossing rule as in S1c: before at tick n−1, at or past at tick n).
- **Delayed** if some ML start `x <= c` crosses some signal at a tick `n` inside a held interval of its block `B`. Compute this exactly from the intervals (for each signal and interval, the set of starts that cross it inside the interval is a contiguous range of `x`; delayed if that range meets `x <= c`). Do not sample `x`.
- **margin_ticks** (redefined): when not delayed, the coverage slack: the smallest amount, in ML ticks (`ceil(distance / progress per tick at design_speed)`), by which `c` would have to move towards the merge for a delaying range to meet `x <= c`. If no delaying range exists at all, `margin_ticks` = `UINT32_MAX`. When delayed, 0.
- **Add** `int64_t worst_start;` to `JunctionPrioTiming` after `margin_ticks`: when delayed, the delaying start nearest the merge (as progress on m's path, may be negative); otherwise `c`. Document it in one line.

## Code changes
- Rewrite `SimulatePrioTiming` to the model above. Keep `JunctionPrioTimings`' signature, identification and ordering. Update the doc comments (`JunctionPrioTiming::margin_ticks`, the new member, `JunctionPrioTimings`).
- File-local statics only, each documented in one line.

## Tests (`src/tests/junction_throughput.cpp`, `[junction][throughput]`)
1. **New, write first, see it fail:** "Prio timing: a later ML train is delayed when the coverage is short". The hand-made fixture of "Prio timing: coverage from JunctionPrioLength is enough at the merge", with `prio_tl` = 1. Assert `ml_delayed` true and `worst_start` < `c`. Red: the current S1c code reports not delayed (record that output).
2. "coverage from JunctionPrioLength is enough": keep `ml_delayed` false; replace any old margin value assertion with `margin_ticks` > 0 and < `UINT32_MAX`.
3. "a distant Entry signal delays the ML": unchanged expectation (delayed).
4. "margin grows with coverage": keep, with the new meaning (one more TL of coverage gives a larger margin).
5. **Outer test** ("EvaluateJunctionThroughput on the generated test junction"): keep `CHECK` that no entry is `ml_delayed`.
   - If it now fails (expected with coverage 4 tiles), **do not change the generator or the spec.** Report each delayed prio: ML and joiner movement indices, merge tile, `entry_to_merge`, coverage (tiles), `worst_start` (and its distance in tiles before the merge), the signal and block where the ML is stopped, and the coverage that `JunctionPrioLength` gives for the test's train. Then stop after the gates. That is a D119 finding for the orchestrator.

## Do not
- Change the generator, rules, signal pass, S1a/S1b functions or `JunctionPrioLength`.
- Model the joiner being held by the ML train, curves, braking or path signals.

## Gates
1. Build `build-rel` `openttd_test` and Debug `build` `openttd_test` and `openttd`, output to files. No errors, no new warnings.
2. `[throughput]` in both builds.
3. Full `build-rel/openttd_test`, output to a file.
4. Commit: `Fix: Junction prio timing checks every ML arrival time`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence for test 1
- Types, functions and members added or removed: names only
- The generated junction's full `prio_timings` list (indices, merge tile, `entry_to_merge`, delayed, margin_ticks, worst_start)
- Suite results
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
