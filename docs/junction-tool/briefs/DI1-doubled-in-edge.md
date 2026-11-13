# DI1: a doubled bridge on a row's in edge, structure and programs (DESIGN-G2 §G2.4 "In edge", D229, RECON-crossed-in-edge)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Replace the refusal "a crossed lane in edge is not ruled yet" with the mirror of the doubled out edge, so that LLL_RRR x LLL_RRR and wider hubs get past structure and programs. Placement may still refuse them. That is expected and reported, not fixed.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-di1 <HEAD given at launch>`. Its builds exist (configured with HiGHS); rebuild `openttd_test` before the first run. The untracked files in it stay untouched.
- Another agent works in `.claude/nodes2`, in placement: never touch it.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/di1/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G2-general-graph.md` §G2.4;
- `docs/junction-tool/RECON-crossed-in-edge.md`;
- `docs/junction-tool/DECISIONS.md` row D229;
- `src/junction/topology/junction_topology.cpp`:
  - 1055-1070 (the refusal and `row_doubled`);
  - 1125-1160 (the row edges' programs, including the doubled out edge branch);
  - 1238-1262 (the edge ends);
  - `in_item` just below;
- `src/junction/topology/CLAUDE.md`;
- the W1 tests in `src/tests/junction/topology/junction_topology.cpp` (grep "a diverging joiner that crosses" and "wide hubs: how far each gets").

## Rules (from DESIGN-G2 §G2.4 "In edge", not open)
1. **Structure.** Delete the refusal. Only the leg (a row edge that starts and ends at a node) sets `row_doubled[r]`. Write the condition as `graph.edges[e].from.node && graph.edges[e].to.node`. An in edge's doubled bridge does not make the leg doubled.
2. **Programs.** Add a branch for an in edge (`edge.to.node`, not `edge.from.node`) with `doubled_bridge(e)`, placed before the plain `else if (edge.to.node)` branch:
   - B = `row_doubled[r] ? row_items[r].north : row_items[r].lane` (the item the in edge's node sits on);
   - A = whichever of `row_items[r].lane` and `row_items[r].copy` is not B (D241). When the leg is not doubled, A is Copy and B is Lane. When the leg is doubled, B is north, which is the Copy item, and A is Lane;
   - if lane, copy or B is `NO_ROW`, fail "a row without its side tracks is not ruled yet";
   - if B is neither the lane nor the copy item, fail "a crossed in edge whose node sits off its lane and copy items is not ruled yet";
   - hand = `((B > A) == east[r]) ? +1 : -1` (the out edge's rule for a step from item `from` to item `to`, with from = A and to = B);
   - `edges[e]` = `Fan(A)`, then `Over(x)` for each crossing, then `SBend(hand, B, LaneMerge)`;
   - `copies[e]` = `SBend(hand, B, LaneSplit)`, then `Over(x)` for each crossing.
3. **Edge ends and node shapes.** These need no change: the first token is the Fan, no token ends on the split, and `in_item` reads B from the LaneMerge S-bend. If the node shape's item for the in edge's node is not B, STOP.

## Tests (`src/tests/junction/topology/junction_topology.cpp`, `[junction][topology]`), red first
0. A first attempt's patch, `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/di1/di1-uncommitted.patch`, holds the tests and most of the change. You may apply it and then correct it to this brief. Its `assert(c != b)` goes: the A/B rule above replaces it. Record the red evidence again from your own run.
1. **"programs: a crossed in edge carries a doubled bridge"** (outer). LLL_RRR x LLL_RRR under pairing any:
   - programs succeed;
   - for the in edge the recon names (e3, W-in 1 to node 3), `programs.edges` and `programs.copies` are exactly the token lists of rule 2, with each Over naming e54, e57 and e60 in the order's crossing order;
   - `programs.nodes[3].item` is B.
   - Red: it fails today with the refusal text; record it.
2. **"structure: a crossed in edge does not double the leg"** (inner). Also check LL_RR x LL_RR's e5 (leg doubled): its programs follow rule 2 with A = Lane and B = north. For LLL_RRR x LLL_RRR, the leg of e3's row has no LaneSplit or LaneMerge token unless that row's leg is itself crossed. State which it is in the report.
3. **The W1 characterisation test** ("wide hubs: how far each gets", `[topologysweep]`): run it, and report its lines before and after.

## Changes
Only the two sites above and the tests. In `src/junction/topology/CLAUDE.md`, with no D-ids or design, brief or recon references:
- replace ", a crossed in edge is refused as not ruled" in the out-edge line with ".";
- add "- A crossed in edge carries its doubled bridge as the mirror: its fan arrives on whichever of the lane and copy items the lane's first node does not sit on, copy 1 S-bends to the node's item before the bridge, copy 0 after it; an in edge's bridge does not double the leg."

## Do not
- Touch placement, render or signals, or the refusal "a bridge over several band crossings".
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Identity or any existing test changes, except one. LL_RR x LL_RR has a crossed in edge (e5, row 2, with a doubled leg; a first attempt found it). So the `[topologysweep]` test "topology: LL_RR x LL_RR under each lane pairing seeds or names a reason" may now reach a later stage. Report its lines before and after. Change its expectations only if it pins the old refusal text, and say exactly what you changed.
- The node item is not B.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`. Another agent builds and gates in parallel, so if identity is over budget, re-run it once and report both times.

One commit on wt-di1: `Add: Junction doubled bridge on a crossed in edge`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- test 2's answer;
- the characterisation lines, before and after;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
