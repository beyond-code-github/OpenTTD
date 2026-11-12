# P3b: every row change leans toward the feature it serves (D215; generalises P3/D212)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

**Source rule (D213):** candidates come from the graph input's tags and the programs (token kinds and positions), never from shapes.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-p3b <HEAD>` (given at launch). Never use git stash.
- P3 (merged): NodeBuilder::ReadLeans makes the classes of each route's port bends lean (in bend +travel, out bend -travel), rule (a) per class (bend pieces or RUN pieces of the bend's own edge and copy), all-or-nothing per bend, conflicting directions bar; the snap puts lean classes last at their extreme. Read briefs/P3-port-fan-lean.md with its amendments.
- D215 (DECISIONS.md): every row change sits next to the feature it serves. Engineer's rig finding: a doubled leg's lane split sits right after the exit split, far before its bridge heads (seed floors are minima: junction_topology.cpp ~:1300-1312).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md (`[topoproj]` row); DECISIONS.md D212, D215; briefs/P3-port-fan-lean.md.
- src/junction_nodes.cpp NodeBuilder::ReadLeans and the lean snap; src/junction_topology.h JunctionTrackStep (kinds), JunctionTrackPrograms (edges vs copies), JunctionTrackPieceTag.
- src/tests/junction_topology.cpp P3's tests (now `[topoproj]`).

## The rule (settled): bends and their lean direction (along the bend's own travel axis)
A bend = the pieces tagged with one emitting token on one route (tag.token != RUN; not bridge pieces). Kinds:
1. Fan on the route's first edge: +travel. Fan on the route's last edge: -travel. (P3, unchanged.)
2. SBend of programs.copies[e] (a doubled leg's lane split, copy 1): +travel (toward its bridge heads).
3. SBend of programs.edges[e] where programs.copies[e] is non-empty (the lane merge, copy 0): -travel.
4. Any other SBend on edge e: let prev = the previous emitting token of e on this route, or e's from-node if none (or the port if e is the route's first edge); next likewise toward e's to-node (or port). If next is e's to-node and prev is not e's from-node: +travel. If prev is e's from-node and next is not e's to-node: -travel. Otherwise no lean.
Corners and Over/Under tokens never lean. Rule (a), all-or-nothing per bend, and conflict barring apply to every bend as in P3.

## Tests (tag `[topoproj]`; record each red)
1. **Outer, "topoproj: LL_RR's doubled legs split next to their bridges" (D215):** graph solve on flat facts. For each doubled edge (programs.copies non-empty): in the solved routes, the copy-1 route's lane split piece (first piece of its copies SBend) is at most as far (along travel) from that route's first bridge head as in the seed routes, and for at least one doubled edge strictly nearer; every lean class cannot step one more (P3's check); CheckJunctionRules clean. Red: before the change the distances equal the seed's (record).
2. **"topoproj: lean kinds"**: on LL_RR, tpl.lean is nonzero for the classes of each doubled leg's two SBends (unless rule (a) bars them: then INFO which point barred it and fail — report it, it is a finding) with the directions of kinds 2-3.
- P3's tests stay green unedited. Other existing tests unchanged.

## Changes
- Extend ReadLeans to collect the bends of kinds 2-4 alongside kind 1 (same rule (a), all-or-nothing, conflicts). Update the `lean` member's comment: "nonzero only for a bend's class (D212, D215)".
- Regenerate tools/ottd/layouts/circuit-llrr-TL4.json with the Release build (`JUNCTION_CIRCUIT=$PWD/tools/ottd/layouts/circuit-llrr-TL4.json build-rel/openttd_test "[.circuitllrr]"`) and commit it with the code; if the circuit test fails, STOP and report.

## Do not
- No other lean kinds; no change to the snap, constraints, seed placement, or legacy behaviour.

## Stop conditions
- Test 1 checker violations (report each + RenderJunctionAscii seed and solved to docs/junction-tool/wip/p3b-llrr.txt); a doubled leg's SBend barred by rule (a); an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topoproj]' 5`; `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Add: Junction every row change leans toward the feature it serves (D215)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topoproj]` row: add "P3b" and the count.

## Report
The standard report, plus per new lean class: kind, edge, direction, flat and solved value; and p3b-llrr.txt. When you have reported, stop.

## Amendment 1 (orchestrator, after the stop: ReadLeans sees no token kinds)
The classification moves to where the graph is known. JunctionNodeInput gains `std::vector<std::vector<int8_t>> lean; ///< Per path and piece: the lean of the bend the piece belongs to (+1 with travel, -1 against; D212, D215), 0 otherwise; graph input only.` JunctionNodeInputOfGraph fills it by the rule's kinds 1-4 (it has graph and programs; kind 1 moves here from ReadLeans). ReadLeans then reads it: a bend = the pieces of one (edge, token, copy) with nonzero lean on one route; rule (a), all-or-nothing and conflict barring as before; the lean axis is the bend's travel axis. With input.lean empty (traced input) nothing leans. P3's tests must stay green unedited.
