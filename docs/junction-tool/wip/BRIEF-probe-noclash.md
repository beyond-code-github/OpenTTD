# Probe: retire the clash loop: rough solve without clash separation, ladders until stable

Throwaway probe: nothing committed. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/noclash/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Ladders are in: `SolveMappedVariant` = rough `SolveLazy` (lazy clash loop + area pass) then up to two ladder passes (rungs from the accepted solution, new composer, warm start), lazy loop still on. Read `src/junction/planar/CLAUDE.md`, `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §7-§9, and in `planar_components.cpp`: `SolveLazy1` (what a round detects: `bad` clashes and `under_bad` under-deck; how it separates), `SolveLazy`, `SolveMappedVariant`, `LadderRungs`, `RungsMoved`.
- Question: can ladders replace the clash loop? The rough solve then may overlap, and rungs are read from it (which segment faces which point). Do re-read passes converge to a legal layout, at what area and time?

## Change (env `PLANAR_NOCLASH=1`, probe only)
- In `SolveLazy1`, when set: detect clashes each round exactly as today but do not add clash separations (`bad`); still handle `under_bad` exactly as today. Return the solution after the under-deck handling settles, with the number of clashes it still has (`bad.size()` of the final round) recorded in the result (a new `LazyResult` field is fine for the probe).
- In `SolveMappedVariant`, when set: the rough solve and every ladder pass run this way; ladder passes repeat (rungs from the latest solution) while any rung point moved off its span OR the latest solution still has clashes, at most 6 passes. Keep the area pass as is (it runs inside `SolveLazy`).
- Count per variant: rough clashes, per pass clashes and moved points and ms, final clashes, final area, whether any rung row was violated on the solution it came from (expected now, since the rough may overlap: report counts, do not stop for it), and any infeasible ladder solve (with `PLANAR_IIS=1` core, first 30 lines, for the first two such).

## Runs
- T2 o0t2f1s1, T3 o1t2f1s1, LL² o1t3f0s0 first; then the full T2 and T3 sets and LL² on the 201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt` (`/home/pete/OpenTTD/tools/junction/pv run ... -j 6`, plus a stderr log per variant for the counts).
- Per hub against today (no env): variants legal at the end (final clashes 0) vs solved today; area better/same/worse (worse ones with deltas); passes used (histogram); median/max ms; infeasible ladder solves; the clash pairs left on variants that end with clashes (first 20).

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-noclash.md`)
Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-noclash.patch`, `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
