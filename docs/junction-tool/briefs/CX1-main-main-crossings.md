# CX1: main x main and joiner x joiner crossings get their over/under from D223 ("a branch crossing without one joiner is not ruled yet")

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-cx1 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes2 (placement facts recon) and .claude/ng1 (tunnel render): never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- D223 (DECISIONS.md: read it with D190, D80, D174, D188): where two main lanes cross, the lane that goes straight through is on the bridge and the turning leg goes under; where two branch joiners cross, the one farther from the main line goes over. MJ1 (71d7cbc112): LL_RR x LL_RR (any, keep) and L_R x LL_RR pass the order and the joiner check and stop in StructureJunctionTrackGraph's bridges section: "a branch crossing without one joiner is not ruled yet" (~:797 at 7f5bc41e06; re-locate). Today the structure decides each crossing's bridge from "exactly one joiner" (the joiner, i.e. the branch edge, is carried over / the main line under? read and state which, citing D190).

## Step 0 (findings, no code change)
For LL_RR x LL_RR (any) and (keep): list every crossing the structure meets that has zero or two joiners: the two edges, their kinds (main through lane, main turning leg, branch joiner, branch split leg...), and what D223 says goes over. Confirm "straight through" and "turning" can be read from the graph (the edge's lane path turn, structure.turns, or the movement's in/out groups) and "farther from the main line" from the order/levels. If a crossing fits neither D223 case (e.g. two turning legs, or a turning leg against a joiner), list it and stop with the cases (that needs the engineer).

## Change (after step 0)
In the bridges section: a crossing with exactly one joiner keeps today's rule; a main x main crossing is carried by the through lane over the turning leg; a joiner x joiner crossing by the joiner farther from the main line; doubled/single as today's rules for the carrying edge (D80/D174/D188: a main-line bridge is doubled). Each with a comment citing D223.

## Tests (record each red)
1. Outer identity: `tools/junction/jt identity build-rel`; the five sweep tags' outputs equal to launch HEAD's (times stripped); LL_RR circuit byte-identical.
2. `[topologysweep]` (extend MJ1's test): LL_RR x LL_RR (any, keep) pass the bridges section; for each D223 crossing, the carried edge is the one D223 names; report the next stage's refusal or, if it seeds, render + signals + CheckJunctionRules (clean or first violation, box, ms). Red: today's refusal.
3. NM1 rerun (local patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/of2/nm1-local.patch; never committed): 729 runs, 0 crashes/timeouts; counts per pairing vs MJ1's.
4. TESTS.md row.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Change: Junction main x main and joiner x joiner crossings take their over and under from D223`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0, NM1 counts and LL_RR x LL_RR's next refusal or result. When you have reported, stop.
