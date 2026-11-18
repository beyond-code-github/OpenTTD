# Probe A2: prio coverage over a doubled bridge, near lanes' row reservation dropped (D347)

Throwaway probe: nothing committed in ng1. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. One heavy job at a time (no parallel test runs). Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/probe-pb2/`; read code with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule. Do not start background work that outlives your report.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `0958786f36`. Apply `/home/pete/OpenTTD/docs/junction-tool/wip/probe-prio-bridge.patch` (probe A: G9c copy-switch exemption with `IsCopySplit`, near lanes' coverage summed along the strand, all behind `PLANAR_PRIOBRIDGE=1`). Read `wip/PROBE-prio-bridge.md` (probe A's result: every variant "a clash is not separable": the near lane's prio row reservation `prio:<strand>`, a fixed straight 2P strand tied 2P before the merge, cannot clear the joiner's diagonal that now crosses inside the coverage).

## Change (behind the same env)
1. In the 2-lane `Join` and the n-lane `Join` (`src/junction/planar/planar_components.cpp`, `M.prio_rows.push_back` ~406-407 and ~1015), do not push the prio row for the near lanes whose coverage probe A changed (2-lane: `S_near`; n-lane: `S[k]` for k >= 1). The far lane's row and Block227's rows stay. Rationale (D342): the row may leave the main line between joins; the signal pass places the row by search (D345) and the checker (G9b) judges; the reservation here only kept room.
2. Nothing else.

## Runs (sequential; baselines are in probe A's scratchpad `probe-pb/{T2,T3}-base.txt`, do not rerun them)
- T2 and T3 mapped full sets (the two `planar components: T2/T3 from its topology text (mapped)...` tests) with `PLANAR_PRIOBRIDGE=1`: per variant area/size or failure text; best variant.
- If any variant solves: the end-to-end plan test that runs signals and `CheckJunctionRules` on mapped T2/T3 (`src/tests/junction/planar/planar_plan.cpp`, as probe A named) with the env: checker clean or each violation text; prio placement (straight pattern, search, or "does not fit"). If none solves: `PLANAR_ONLY=o1t3f1s0 PLANAR_IIS=1 PLANAR_DEBUG=1` on T3 and report the IIS rows; add no rule.

## Report (findings under 30 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-prio-bridge2.md`, committed alone on feature/junction-tool in `/home/pete/OpenTTD`, message "Doc: Junction probe A2 of prio over a doubled bridge", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
Then in ng1: `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-prio-bridge2.patch` (commit with the findings), `git checkout -- .`, remove any untracked file you added, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
