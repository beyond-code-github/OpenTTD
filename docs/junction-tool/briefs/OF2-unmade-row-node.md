# OF2: a row node reached only by row edges is placed by the order (LL_RR x LL_RR, pairing any: "a row edge to a node the row has not made is not ruled yet")

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-of2 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes2 (junction_levels.cpp) and .claude/ng1 (tunnel tokens in junction_topology.cpp: programs/structure): never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- OF1 (5db93a991d) fixed the non-adjacent crossings; LL_RR x LL_RR (pairing any) now stops in OrderJunctionTrackGraph: "a row edge to a node the row has not made is not ruled yet" (CR1's refusal, raised when `row_position` returns -2). OC1 (docs/junction-tool/RECON-nonadjacent-crossing.md §4): node N10 (the E main split, e10 = S8 -> N10) is reached only by row edges, so no swept item creates it in the row; INFERRED rule: the node takes its place in its row between its row neighbours (between the port and N11). CR1 (a7eb3cc377) refused the same shape for L_R main + multi-lane branch (36 NM1 pairs under pairing any now stop here).

## Step 0 (findings, no code change)
For LL_RR x LL_RR (any) and for one L_R x LL_RR pair: list the row's items as the sweep builds them, the unmade node, the row edges that touch it and their other ends' positions. Is the node's position determined by the row (both its row edges' other ends are in the row and on either side), so that inserting it there is forced? If its position is ambiguous (several legal places), stop and report the options.

## Change (after step 0)
In OrderJunctionTrackGraph, after a row's sweep, every node that a row edge of that row touches but the sweep did not place is inserted at the position its row edges determine (between its row neighbours in travel order), as a node item; then the crossings pass as today. The CR1 refusal stays for any node whose place the row does not determine.

## Tests (record each red)
1. Outer identity: `tools/junction/jt identity build-rel`; the five sweep tags' outputs equal to launch HEAD's (times stripped); LL_RR circuit byte-identical.
2. `[topologysweep]` (extend OF1's test): LL_RR x LL_RR (any) passes the order stage; report the next stage's refusal (or a seed: then render + signals + CheckJunctionRules: report clean or the first violation). Red: today's refusal.
3. NM1 rerun as OF1 did (local nm1 patch with NM1_PAIRING/NM1_TSV, never committed; reuse OF1's copy in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/of1/ if present): 729 runs, 0 crashes/timeouts; first-failure counts per pairing vs OF1's.
4. TESTS.md row.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Change: Junction order places a row node reached only by row edges`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0, the NM1 counts and LL_RR x LL_RR's next refusal. When you have reported, stop.
