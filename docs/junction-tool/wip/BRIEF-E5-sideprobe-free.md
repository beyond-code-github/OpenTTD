# BRIEF: E5 side probe, inserts free, with conflict tags (read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 40 minutes in total. Each LP has a 15 s cap. Run the hidden hub test once (a second time only if the first crashed or printed nothing from the probe). No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Previous step: commit 7372d911eb added `PlanarDrawPipelineOptions::side_probe` (per stretched sep pair, four LPs, one candidate side hard as a "sephard" row), brief `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-E5-sideprobe.md`. Its LPs copied the commit-stage options, so every insert the commit did not keep was held at 0. This step repeats the four LPs with every insert free, and logs the conflicting rows of every infeasible LP in both modes.

## 2. Read first (once each)
- `src/junction/planar/planar_draw.cpp`: the `side_probe` block in `DrawPlanarPipeline` (added in 7372d911eb; `git show 7372d911eb -- src/junction/planar/planar_draw.cpp` shows it), and lines ~370-385 (how `curve_flex_from` holds inserts).
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions::curve_flex_from`, `PlanarDrawResult::conflict`.

## 3. Changes (planar_draw.cpp only, inside the existing `side_probe` block)
1. In the existing per-tag, per-`c` loop: when the LP's status is not "Optimal", log one more line: `side probe <tag> c<c> conflict: <the result's conflict tags joined by " | ">` (the first 40 tags; append ` (+N more)` when there are more).
2. After that loop, for each stretched tag, for `c` = 0..3, run a second LP: the same options as the first loop's LP except `.curve_flex_from = nullptr` (and therefore no `curve_flex_above` effect): every insert is free (>= 0) with no curve rows on it, as in discovery round 1, and no flex price (leave `flex_price` and `flex_weights` at their defaults). Log `side probe free <tag> c<c>: <status> slack <total, 3 decimals> sep <sep sum> curve <curve sum> ms <ms>`, and on a non-Optimal status the conflict line as in 1, prefixed `side probe free`.
- Append to `out.log` directly, as the first loop does; do not touch `out.rounds` or `log_round`.

No other change: no new member, no option, no test edit (the hidden test already sets `side_probe` from `PLANAR_PROBE_SIDES` and prints `r.log`).

## 4. Do not
- Do not change any default or any existing path. Do not run PSG 197. Do not interpret beyond tabulating.

## 5. Stop conditions
- `CheckPlanarDrawable` with `curve_flex_from = nullptr` rejects the options or fails differently from Optimal/Infeasible (e.g. an exception or "Error"): stop and report.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor" and "joint: a port fan's size follows the rows"; rerun alone and report.
- The hidden test once with `PLANAR_PROBE_SIDES=1`: `build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/sideprobe-free.log 2>&1`. Copy the log to `docs/junction-tool/wip/probes/e5land/sideprobe-free-<short head hash before your commit>.log` in this worktree, in the same commit.
- One commit: `Add: Junction: side probe with free inserts and conflict tags (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed.
- Suite results (count and time per gate).
- For T3 and LL2: one table, a row per stretched tag: walls (decode text), committed-mode c0..c3 (status, sep slack), free-mode c0..c3 (status, sep slack).
- Every conflict line verbatim (both modes), grouped by tag. Catch wraps lines at 80 columns: join the wrapped lines before quoting.
- Stops and anything left undone.
- State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
