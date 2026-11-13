# OC1 (recon): what LL_RR x LL_RR needs at the order stage ("a crossing between edges that are not adjacent is not ruled yet") and at placement (per direction: "bridge heads hug crossers")

Read-mostly recon: you may add temporary prints/tests in your worktree; commit nothing; save any experiment as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/oc1.patch and leave the worktree clean. Keep your context small. Never use git stash. Never start a game client or touch the engineer's game or rig servers. Mark anything not read or measured as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-oc1 <HEAD>` (given at launch). Another agent works in /home/pete/OpenTTD/.claude/nodes2: never touch it. Use absolute paths.
- LP1 (f9b4a6488f): lane pairing (D221). LL_RR x LL_RR now passes the lane expansion; with pairing any/keep it stops in OrderJunctionTrackGraph: "a crossing between edges that are not adjacent is not ruled yet" (src/junction_topology.cpp ~:577); with per direction it seeds through order/structure/programs and stops in placement: "the placement rules contradict each other: bridge heads hug crossers (edge 14, row 2) <- ...". The test "LL_RR x LL_RR under each lane pairing seeds or names a reason" (`[topologysweep]`) reproduces both. NM1 under each pairing: docs/junction-tool/RECON-notation-matrix.md and LP1's numbers (any: 12 O non-adjacent; keep: 21).
- DESIGN-G1 §7-§9 (order by a sweep from the S side, crossings, rows) and §11 (placement) are the design; D182 (every crossing is a node? read it), D203, D209.

## Questions
1. The order stage: what it does with crossings (the sweep, `pending`, why a crossing between non-adjacent edges cannot be inserted), with file:line. For LL_RR x LL_RR (any): list the lane paths, the graph's crossings, and the first crossing that fails: which two edges, why they are not adjacent at that level, and what lies between them.
2. What general rule would place it: e.g. a crossing between non-adjacent edges swaps the in-between edges too (a block move), or the sweep reorders by a different key, or a bridge spans several (D203/D209 series bridges). Prior art in the code or DESIGN-G1. Say whether it is a pure order-stage change or needs a structure/programs change, and whether it needs an engineer ruling (state the question as a decision card: context, options with consequences, recommendation).
3. Per direction's placement contradiction: the reason chain in full; which rule pairs contradict and why (geometry), and what general relation is missing or wrong.
4. After a hypothetical fix of (2): what is the next stage likely to refuse (INFERRED from the NM1 groups G "more than two legs at one split/merge": does LL_RR x LL_RR hit it?).

## Output
docs/junction-tool/RECON-nonadjacent-crossing.md (untracked, ~60 lines) and the patch path if any.

## Report
10 lines: the failing crossing and why, the proposed general rule (and whether it needs the engineer), the per-direction contradiction, and the next blocker. `git -C /home/pete/OpenTTD/.claude/nodes status --porcelain` (clean). When you have reported, stop.
