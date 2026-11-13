# S2c: a row's in and out edge meet a main port (REVIEW-port-fans F8), then S2b

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes, branch wt-s2b at 2e77db1366, with an UNCOMMITTED change: S2 (the S fans) plus the two placing tests re-hidden; saved diff /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/s2b-wip.patch. Check `git diff` equals it before you start; if not, stop. Never git stash. A directory `build-asan` (Debug, AddressSanitizer, `openttd_test` built) exists in the worktree; use it for the red evidence below. Do not commit it (it is untracked; leave it).
- Rebase first: save `git diff` again, `git checkout -- .`, `git checkout -B wt-s2c <head>` (head in your launch prompt; it adds only doc commits, among them the standalone CLAUDE.md rewrite), re-apply the diff (by hand where the CLAUDE.md hunks no longer apply: then write those lines without any decision, design, brief or review reference, see below).
- The crash: with S2, "topology: LL_RR x LL_RR under each lane pairing seeds or names a reason" ([topologysweep], src/tests/junction/topology/junction_topology.cpp ~4522) crashes. AddressSanitizer: heap-buffer-overflow read at src/junction/placement/junction_placement.cpp:1329 `item_pt[node_item(out_edge[r])]` with `node_item` = NO_ROW. Cause: `JunctionIsInEdge` / `JunctionIsOutEdge` (src/junction/topology/junction_topology.cpp ~211-221) call an edge a row's in or out edge when it joins a port and an odd-level node and its first (last) token is a Fan; an exit leg from a row node straight to an S port now ends in an S fan (index NO_ROW), so it is taken as the row's out edge.

## Rule
A row's in edge runs from a main port to a node on a row; its out edge from a node on a row to a main port. The token kind does not decide it.

## Change
1. `JunctionIsInEdge` and `JunctionIsOutEdge` gain a first parameter `const JunctionTopology &topology` and require the port end's group to be main: build the slots with `JunctionPortSlots(topology)` and test `topology.groups[slots[port].group].main`. Keep every other condition as it is. Update the declarations in junction_topology.h (doc comments say "from a main port" / "to a main port") and every caller (placement lambdas at ~142-143 have `topology` in scope; the test at src/tests/junction/topology/junction_topology.cpp ~749 passes its topology; it is the only existing test you may touch, by adding the argument).
2. Nothing else.

## Tests
- Red: run the crashing test in `build-asan` before change 1 (with S2 applied) and record the ASan first line. After change 1: it passes in build-asan and in build-rel.
- New, `[junction][topology]`: "programs: an exit leg to an S port is no row in or out edge": LL_RR x LL_RR (any) built to programs as `PlacementOf` does; for every edge with a port end on the S side, `JunctionIsInEdge` and `JunctionIsOutEdge` are false; and every row r still has exactly one in edge and one out edge by the helpers. Red: before change 1 the first part fails (record which edge).

## Then
Commit this as `Fix: Junction row in and out edges meet a main port` after the gates below pass, with the S2 diff still uncommitted? No: commit order is (1) S2 with its tests hidden: `Add: Junction S branch lanes open up by fans inside the box`, including change 1 and the new test, since S2 cannot pass the gate without it; one commit. Then (2) carry on with Part 2 of docs/junction-tool/briefs/S2b-sbend-roles.md (read it), exactly as written there, as its own commit, except: CLAUDE.md lines carry no reference to decisions, design sections, briefs, recons or reviews (drop the bracketed ids from the lines S2b gives you, keep the facts); test names may keep their references.

## Gates (before each commit)
Debug and Release build with no new warnings; `tools/junction/jt gate build-rel` passes (baseline at the head: record it first; expect 500 cases), identity unchanged (4 cases, 255 assertions), `tools/junction/jt fast build '[topology]'` within budget; the crashing test passes in build-asan.
Both commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Stop conditions
As S2b's, plus: any other test crashes or fails in the gate after change 1.

## Report
Commit hashes and subjects; red evidence (ASan line, the new test, S2b's two tests); counts per commit; S2b's probe results; stops; `git status --porcelain` and `git log -3`.

When you have reported, stop.
