# R7: the Shift edit: an S-bend pair inserted into a row edge, its shifted stretch on a new stack item (DESIGN-G1 §14.6, §16 R7; D182, D195, D213, D216)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-r7 <HEAD>` (given at launch). Never use git stash. Another agent may work in .claude/nodes2: never touch it.
- R1-R6 merged: every edge is an Elem chain; programs carry roles (SBend Role::Shift exists, unused); a row edge's program may hold any token sequence. D182/D195: a Shift is an S-bend pair around an obstacle on a row (or "staying shifted" to the port: one S-bend; out of scope here). §14.6: a Shift restarts at programs + placement. The search (T4b) will produce Shifts from failures; this step makes one Shift edit placeable and correct.

## The edit (settled)
- `struct JunctionShiftEdit { uint32_t edge; uint8_t copy; int8_t side; auto operator<=>(const JunctionShiftEdit &) const = default; };` (junction_topology.h): on edge `edge` (copy `copy`), insert two SBend tokens with Role::Shift, the first stepping to `side` (+1 = toward larger rows... define it as the edge's right of travel, as JunctionDetourEdit's side), the second stepping back; inserted after the edge's existing leading tokens (after a Fan on an in edge; at the start of a leg) so the pair lies on the edge's own row stretch. The shifted stretch between them sits on a NEW stack item adjacent to the edge's row on that side.
- `JunctionGraphSeed ApplyJunctionShifts(const JunctionGraphSeed &seed, const std::vector<JunctionShiftEdit> &edits, const JunctionSpec &spec, std::string *why);` returns the seed with programs edited and structure.stack gaining one item per edit (kind: a new `JunctionTrackStackItem::Kind::Shift` value; row = the edge's lane row; placed in the stack next to the edge's row item on that side), placement and routes recomputed (cut, place on flat, render); empty routes and `why` on failure. Refused (why) on a doubled leg's copy pair stretch between its lane split and merge, on a prio coverage (the stretch within even_prio + 1 of a joiner merge), and on an edge with no row stretch.
- Placement of Shift S-bends: the generic chain machinery (horizontal SBend sized by rows: first to the Shift item's row, second back), the floor table (SBend-SBend opposite hands: the curve family covers gaps), occupancy, facts, D215 (the pair's feature is the obstacle it passes: no objective entry for Role::Shift; say so in a comment).

## Tests (record each red)
1. **Outer `[topoproj]` "topoproj: a house mid-row on LL_RR builds by a Shift"**: take the LL_RR seed; a house at the middle tile of its longest free undoubled row run outside prio coverage (search all row edges; if none, STOP and report); the facts placement (seed's choices) fails naming that tile; ApplyJunctionShifts with one edit on that edge (side +1, and if that fails, -1: report both) then the facts placement (the edited seed's own choices) succeeds; no track on the house; render + signals + CheckJunctionRules clean; T3 heights on flat ok. Red: ApplyJunctionShifts stub returns the seed unchanged (placement still fails).
2. **`[topology]` "topology: a Shift on flat ground"**: the T and LL_RR with one Shift on each eligible row edge in turn (not under the refusal rules): placement succeeds and is checker clean, the box grows by at most the new item's row (+1 in height) and the shifted stretch's columns. Red: stub.
3. **`[topology]` "topology: a Shift is refused where it cannot stand"**: on a doubled leg's lane stretch and inside a prio coverage: `why` names the reason.
- Every existing test and sweep green and unchanged.

## Changes
1. JunctionShiftEdit, JunctionTrackStackItem::Kind::Shift, ApplyJunctionShifts as above.
2. Whatever the stack/order/structure code needs to accept a Shift item (Y relations: one row from its lane row on its side; band gaps as for other items). If the stack code cannot take an item that belongs to an edge rather than a lane, STOP and report what it needs.
3. TESTS.md rows.

## Do not
- No search, no other edit kinds, no GUI. No change to existing seeds.

## Stop conditions
- No eligible house run on LL_RR; the stack cannot take a Shift item; any existing number changes; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` (to files); circuit byte-identical; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Add: Junction Shift edit: an S-bend pair on a row edge with its own stack item`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus test 1's house, side used, box before/after, Release time of ApplyJunctionShifts + placement. When you have reported, stop.

## Note (orchestrator, from R6's report)
The result-spans loop for doubled legs looks up an S-bend's chain element with an unchecked find_if; a Shift S-bend with no chain element would be undefined behaviour, and on an undoubled leg with tokens it leaves an empty span. Building the Shift S-bends as chain elements (this step) must cover both loops; add a check that every SBend token has its chain element (a placement failure naming the token otherwise).
