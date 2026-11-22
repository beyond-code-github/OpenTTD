# BRIEF: E5 side probe, the stretched pairs together (step c, read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 40 minutes in total. Each solve has a 15 s cap. Run the hidden hub test once (a second time only if the first crashed or printed nothing from the probe). No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Previous steps: 7372d911eb (`side_probe`: per stretched pair four LPs, one side hard), 1f7d6f187d (the same with every insert free, `curve_flex_from = nullptr`, and conflict lines). Result: with inserts free every stretched pair on T3 and LL2 has a side that holds alone. This step asks whether they hold together: one solve with the binary side choice on exactly the stretched pairs.

## 2. Read first (once each)
- `src/junction/planar/planar_draw.cpp`: the `free_sides` branch of the pairwise `sides_from` block (search `options.free_sides`), the `along` lambda (it now takes the axis), and the `side_probe` block in `DrawPlanarPipeline` (`git show 7372d911eb 1f7d6f187d -- src/junction/planar/planar_draw.cpp`); the `free_stretched_sides` call (its option list: `elastic_integer`, `continuous`).

## 3. Why the branch changes
The `free_sides` branch writes, per candidate, a big-M row for each of the four endpoint pairs. In elastic mode each row has its own slack, so a stretched pair's slack is counted up to four times, and the earlier free-sides probe was not comparable with the fixed-side rounds. With `sep_extremal` the fixed-side path writes one row per pair (the endpoints the `along` rule picks, exact for lengths >= 0). The binary branch must do the same.

## 4. Changes (planar_draw.cpp only)
1. In the `free_sides` branch: when `options.sep_extremal`, per candidate (axis, side) write one big-M row, between `from[along(&from, &axis) > 0 ? 1 : 0]` and `to[along(&to, &axis) > 0 ? 0 : 1]` (the same choice the fixed path makes for that axis), instead of the four. Without `sep_extremal` the branch is unchanged. Adapt the call to `along`'s actual signature; the endpoint choice must be exactly the fixed path's.
2. In the `side_probe` block, after the free-inserts loop, two joint solves:
   - "joint committed": the commit-stage options (`commit_options()`), `node_gap_from = &last_input_lengths`, `sides_from = &last_input`, `free_sides = tags` (all stretched tags), `elastic_integer = true`, `continuous = {"point", "axis", "diagonal", "param", "cross"}` (as the free-sides round), 15 s.
   - "joint free": the same with `curve_flex_from = nullptr`.
   - Log per solve: `side probe joint <committed|free>: <status> slack <total, 3 decimals> sep <sep sum> curve <curve sum> binaries <result.binaries> sides_free <result.sides_free> ms <ms>`, then one line per stretched row: `side probe joint <mode> stretched <tag> <amount, 3 decimals>`, and the conflict line on a non-Optimal status (as the existing `conflict_line`).
   - Append to `out.log` directly; do not touch `out.rounds` or `log_round`.

## 5. Do not
- Do not change any default or any path other than the `free_sides` branch under `sep_extremal`. Do not run PSG 197. Do not interpret beyond tabulating.

## 6. Stop conditions
- `along` cannot be applied to a candidate in the binary branch without changing its meaning.
- A joint solve returns something other than Optimal, Infeasible or a time-limit status: report it.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 7. Gates
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- The hidden test once with `PLANAR_PROBE_SIDES=1` and `PLANAR_PROBE_FREE_SIDES=1` (the free-sides round now uses the extremal rows too): `build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/sideprobe-joint.log 2>&1`. Copy the log to `docs/junction-tool/wip/probes/e5land/sideprobe-joint-<short head hash before your commit>.log` in the same commit.
- One commit: `Add: Junction: joint side probe on the stretched pairs, extremal rows in the free-sides branch (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 8. Report (short, fixed format)
- Commits: hash and subject; files changed.
- Suite results (count and time per gate).
- For T1, T2, T3 and LL2: the last-round slack, the "free sides" line (status, slack, stretched rows), and both joint lines with their stretched rows. Join Catch's 80-column wraps before quoting.
- Stops and anything left undone.
- State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
