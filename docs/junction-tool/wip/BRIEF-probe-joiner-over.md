# Probe B: joiner over, main-line lanes S-bend under, in the n-lane prio join (D347b), T3/T2

Throwaway probe: nothing committed in ng1. Time box: 75 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. One heavy job at a time (no parallel test runs). Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/probe-jo/`; read code with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule. Do not start background work that outlives your report.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `0958786f36`, Release `build-rel`.
- Read first: `/home/pete/OpenTTD/docs/junction-tool/wip/RECON-prio-under.md` (sections 2, 3, 5), `wip/PROBE-prio-stagger.md`, `wip/PROBE-prio-bridge2.md`; DECISIONS rows D342, D347, D347b; `src/junction/planar/CLAUDE.md` "Component solver".

## Why
Today in a prio join each near main-line lane S_j doubles over every joiner J_k (k < j) that crosses it, and the checker's G9c then forces S_j's coverage to start after that doubled bridge, so the lanes' coverages chain (T3's length). D347b: the other line may go over instead. Here J_k bridges straight (axis) over the nearer lanes, which step sideways (diagonal) under its deck; S_j has no doubled bridge, so the merges need not chain.

## Change (only in `MappedComposer`'s n-lane `Join`, `src/junction/planar/planar_components.cpp` ~971-1020, only when `prio` and env `PLANAR_JOINER_OVER` is set; unset = today exactly)
Lanes far 0 .. near n-1, joiner side sigma, as the function already names them. Replace the prio branch's joiner and near-lane construction by:
1. Joiners, for k = 0 .. n-1: `AddSeg(J[k], h, 2)`; if k < n-1 (it crosses nearer lanes): `dk = AddSeg(J[k], h, 2)` (the axis piece that will carry its deck); then the landing diagonal `js[k] = AddSeg(J[k], Turn(h, -sigma), 4)` (lo 4, no hi). For k = n-1 keep today's `(h, 2)` and `Turn(h, -sigma)` with today's lo/hi. Tie `End(J[k])` to `End(S[k])` as today.
2. Far lane S[0]: unchanged (`AddSeg(S[0], h, 2 * P + 2)`).
3. Near lanes, for j = 1 .. n-1 (instead of the `Double`/`PairDecks` loop): `AddSeg(S[j], h, 2)`; `out_j = AddSeg(S[j], Turn(h, sigma), 2)` (the S-bend out, under the decks); `AddSeg(S[j], h, 2)`; `AddSeg(S[j], Turn(h, -sigma), 2, std::nullopt, 1.0, true)` (optional return); `AddSeg(S[j], h, 2 * P + 2)` (coverage unchanged).
4. Crossings and decks: for each k < n-1, `xs_k` = `M.Cross(dk, out_j, 4)` for every j > k; then `M.Deck(dk, xs_k)` with default arguments. Joiner lines on the T are half share, so no joiner `Double`.
5. In `build_core` (~1279-1284), when the env is set, the join-entry offsets of a prio join's straight lanes are `2 * j` instead of `JoinEntry(n)` (the step-out parallelogram is gone).
6. Everything else unchanged (prio rows, exit steps, channel level, frays, the 2-lane hand `Join`).

## Runs (sequential; T3 and T2 baselines are in `scratchpad/probe-pb/{T2,T3}-base.txt`, do not rerun them)
- T3 and T2 mapped full sets with the env (`/home/pete/OpenTTD/tools/junction/pv` as probe A2 did, `-j 1`): per variant area/size or failure text; best.
- If any variant solves: the mapped-dump test probe A2 used (plan + signals + `CheckJunctionRules`) on T3 and T2 best: checker clean or each violation text; prio placement (pattern, search, or "does not fit").
- If none solves: `PLANAR_ONLY=o1t3f1s0 PLANAR_IIS=1 PLANAR_DEBUG=1` on T3, report the IIS rows (strands, segments, row tags); add no rule.
- If the tests can print an ASCII picture without new code, include T3 best's; do not add a printer.

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-joiner-over.md`, committed alone on feature/junction-tool in `/home/pete/OpenTTD`, message "Doc: Junction probe B joiner over prio lanes", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
Then in ng1: `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-joiner-over.patch` (commit with the findings), `git checkout -- .`, remove untracked files you added, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
