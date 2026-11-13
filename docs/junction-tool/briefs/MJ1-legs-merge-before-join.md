# MJ1: branch legs bound for one main lane merge before joining it (D224; LL_RR x LL_RR, pairing any: "more than one joiner on a lane is not ruled yet")

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-mj1 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes2 (junction_levels.cpp) and .claude/ng1 (tunnel tokens: structure bridges and programs in junction_topology.cpp): never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- D224 (DECISIONS.md; read it with D187, D204, D221): with a multi-lane branch under pairing any, every main lane receives a leg from each branch lane; those legs merge with each other first and join the main lane as ONE joiner, mirroring D187's exit side (legs merge before the port).
- OF2 (b0fb98bc79): LL_RR x LL_RR (any) and L_R x LL_RR pass the order and stop in StructureJunctionTrackGraph: "more than one joiner on a lane is not ruled yet" (K1, ~:608 at 7f5bc41e06; re-locate). The track graph (G1b ii-a; the merge trees per D204/D187, built in the graph stage: JunctionTreeBuilder or its successor) decides which legs merge where.

## Step 0 (findings, no code change)
Where the graph builds the join-side merge tree today (file:line) and what it produces for LL_RR x LL_RR (any) at one main lane: the legs arriving, their merge nodes, and why two joiners reach the lane. Where the exit side builds D187's "legs merge before the port" (the pattern to mirror). If merging the legs first needs anything beyond the graph's tree building (a new node kind, a structure kind), stop and report what.

## Change (after step 0)
The graph's join-side tree merges the legs bound for the same main lane (from different branch lanes) with each other first, then the merged leg joins the main lane: one joiner per lane. Single-lane branches unchanged (identity).

## Tests (record each red)
1. Outer identity: `tools/junction/jt identity build-rel`; the five sweep tags' outputs equal to launch HEAD's (times stripped); LL_RR circuit byte-identical.
2. `[topologysweep]` (extend OF2's test): LL_RR x LL_RR (any) and L_R x LL_RR pass the structure stage's joiner check; every main lane has exactly one joiner; report the next refusal or, if it seeds, render + signals + CheckJunctionRules (clean or the first violation, box size, seed ms). Red: today's refusal.
3. NM1 rerun (local patch at /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/of2/nm1-local.patch, never committed): 729 runs, 0 crashes/timeouts; counts per pairing vs OF2's.
4. TESTS.md row.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Change: Junction branch legs bound for one main lane merge before joining it`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0, the NM1 counts and LL_RR x LL_RR's next refusal or result. When you have reported, stop.

## Amendment 1 (orchestrator, after the stop)
Option (a), the general one: the order stage learns in-band merges. Keep your level-pass change (a main-slot merge node whose paths all come from non-main groups moves to the band below its lane, 2*row_of + 2). In OrderJunctionTrackGraph's sweep, when an item to expand is a merge node whose feeding legs are both already items in its own band and adjacent there (nothing between them, or only items those legs do not cross), the two leg items are replaced by ONE item (the merge node's out edge) at the position they held; if they are not adjacent, the existing pending-crossing machinery (OF1's rules A/B) applies first; if they still cannot be made adjacent, fail "the legs of a merge cannot be brought together" (no special case). The split side's mirror (a split node in its own band whose two out legs are adjacent) only if the same LL_RR x LL_RR needs it; say if so. Then the tests, NM1 rerun and gates as the brief says. D204's split levels stay unchanged.
