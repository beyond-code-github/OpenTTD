# Probe A4: A3 with the prio join's lanes spread one line wider (option b of wip/PROBE-prio-row.md)

Throwaway probe: nothing committed in ng1. Time box: 60 minutes. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`. One heavy job at a time. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/probe-ps/`; read code with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule. Do not start background work that outlives your report.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `4887288d92` (includes the hard anchor). Apply `/home/pete/OpenTTD/docs/junction-tool/wip/probe-prio-row.patch` (probes A, A2, A3 behind `PLANAR_PRIOBRIDGE=1`). If it does not apply, stop and report.
- Read `wip/PROBE-prio-row.md` including the orchestrator reading (the line between two prio lanes holds the outer lane's row and the inner lane's bridge copy).

## Change (behind the same env, plus `PLANAR_SPREAD=<1|2>`)
`JoinEntry(n)` (`src/junction/planar/planar_components.cpp` ~1140, the join-entry lane offsets of a prio join's straight channel, today 0, 1, 3, 5 ...): with `PLANAR_SPREAD=1` return 0 then `3 * j - 2` for j >= 1 (0, 1, 4, 7: every gap after the first one line wider); with `PLANAR_SPREAD=2` return `3 * j - 1` for j >= 1 after 0 (0, 2, 5, 8: every gap one line wider). Nothing else.

## Runs (sequential)
- T2 and T3 mapped full sets (`/home/pete/OpenTTD/tools/junction/pv run build-rel <hub>` as earlier probes) with `PLANAR_PRIOBRIDGE=1` and each of `PLANAR_SPREAD=1`, `2`: per variant area/size or reason; best.
- For any setting where a variant solves: the mapped-dump test (plan + signals + `CheckJunctionRules`) on that hub's best with the same env: checker clean or violation texts; prio placement or "does not fit" with its tile.
- If nothing solves under a setting: `PLANAR_ONLY=o1t3f1s0 PLANAR_IIS=1` on T3 for it; report the IIS rows; add no rule.
- Baselines for comparison (today's, with the anchor): T2 862.25, T3 1933.25.

## Report (findings under 25 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-prio-spread.md`, committed alone on feature/junction-tool in `/home/pete/OpenTTD`, message "Doc: Junction probe A4 prio lanes spread", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
Then in ng1: `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-prio-spread.patch` (commit with the findings), `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 10 lines. When you have reported, stop.
