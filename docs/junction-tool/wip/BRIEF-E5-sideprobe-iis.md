# BRIEF: E5 side probe, the IIS from the joint drawing (read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 30 minutes in total. Each solve has a 15 s cap. Run the hidden hub test once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Previous step 40be4d43c8 added "faces free" (binaries on whole faces) and "faces hard" (sep hard on its drawing's sides, IIS decoded) to the `side_probe` block. "faces free" found no incumbent in 15 s on T3 and LL2, so "faces hard" never ran. This step runs the same "faces hard" LP from the "joint free" drawing instead (the joint solve with binaries on just the stretched pairs and every insert free, which is Optimal).

## 2. Read first
- `git show 40be4d43c8 a9ed91bf0b -- src/junction/planar/planar_draw.cpp` (the `side_probe` block: the joint solves loop and the faces free / faces hard block).

## 3. Changes (planar_draw.cpp, `side_probe` block only)
1. Keep the "joint free" solve's result: declare a `PlanarDrawResult joint_free;` before the joint loop and copy `r` into it when `free_inserts` is true.
2. Move the "faces hard" LP out of the `if (r.status == "Optimal")` of "faces free" into a small lambda taking a source result and a label, used twice:
   - after "faces free", when it is Optimal: source = its result, label `faces hard` (today's behaviour, same log text);
   - then always, when `joint_free.status == "Optimal"`: source = `joint_free`, label `joint hard`. Its log lines read `side probe joint hard: ...` and `side probe joint hard conflict <tag>[ = decode]`, otherwise identical to "faces hard".
- The LP itself is unchanged: commit-stage options, `curve_flex_from = nullptr`, `node_gap_from = &source.lengths`, `sides_from = &source.points`, `elastic_families = {"curve"}`, 15 s, every conflict tag logged (no cap) with the `sep` decode.
- Skip "faces free" entirely when `std::getenv("PLANAR_PROBE_NO_FACES") != nullptr` (it costs 15 s per hub and found nothing). This getenv is in planar_draw.cpp next to the block, with a one-line comment; it is the only new switch.

## 4. Do not
- Do not change any default or any other path. Do not run PSG 197. Do not interpret beyond tabulating.

## 5. Stop conditions
- "joint hard" is Optimal on T3 or LL2 (then the joint drawing's sides hold everywhere with sep hard, contradicting step 3's slack): stop after the run and report it with the joint free slack lines.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- The hidden test once with `PLANAR_PROBE_SIDES=1 PLANAR_PROBE_NO_FACES=1`: `build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/sideprobe-iis.log 2>&1`. Copy the log to `docs/junction-tool/wip/probes/e5land/sideprobe-iis-<short head hash before your commit>.log` in the same commit.
- One commit: `Add: Junction: side probe IIS from the joint drawing (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed.
- Suite results (count and time per gate).
- For T3 and LL2: the "joint hard" line; then the conflict lines (join Catch's 80-column wraps first): family counts (first word of each tag), and every `sep`, `node`, `leg`, `bay`, `corridor`, `pin`, `end` and `port`-like line verbatim. If a hub has 60 or fewer conflict lines, give them all verbatim.
- Stops and anything left undone.
- State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
