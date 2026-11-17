# S2b finish: measure and commit (the machine crashed mid-run)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test`, never `openttd`. The machine crashed (likely out of memory from many parallel sanitizer runs): run at most ONE heavy job at a time (one build, or one pv run at `-j 6`, or one gate run), never in parallel with each other. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/S2bf/` (the old scratch was wiped by the reboot).

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, at d632dc357c with S2b's changes UNCOMMITTED in the tree (also saved as `/home/pete/OpenTTD/docs/junction-tool/wip/S2b-wip.patch`). Read `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-S2b-shape-rules.md` (the step; its Gates and commit message apply) and `git diff --stat`. The code and tests were done and passed: `[planarshape]` incl. slow, the new planar_regions crossing-side test (1836 comparisons, 0 mismatches), the wrong-side refusal test (red evidence seen), T2/T3 mapped slow tests.
- Lost in the reboot: the LL² tag list. Recreate it durably: the built LL² tags from `PLANAR_DRY=1` (see `/home/pete/OpenTTD/tools/junction/pv` docstring for how LL2 tags are found) into `/home/pete/OpenTTD/docs/junction-tool/wip/ll2-tags.txt` (expect 201 lines; report the count).

## Steps
1. Build `build-rel` from the tree; `jt gate build-rel` once on the quiet machine (report time and failures; if a test fails, rerun the gate once; report both).
2. Before binary: `git apply -R` the patch, build, copy `build-rel/openttd_test` to scratch `before-bin/`, `git apply` the patch back, rebuild; confirm `git diff` equals the saved patch.
3. pv, one run at a time, `-j 6`: before and after, default mode (scoring both shapes) and `PLANAR_SHAPE=program`, on T2, T3, LL² (the tag file). Per hub and mode: solved, best area, better/same/worse, median/max ms; in program mode list variants still failing with reasons (count by reason).
4. Stop before committing if default-mode best area gets worse on any hub or a variant that solved now fails. Otherwise commit with the brief's message, the tables in the body.

## Report
Tag count; gate time and failures; pv tables; commit or stop; `git status --porcelain`, `git log -1`. Under 20 lines. When you have reported, stop.
