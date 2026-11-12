# P3: port fans lean toward their node in the solve (D212; DESIGN-G1 §13.3, §13.8 item 6)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

**Source rule (D213):** which classes lean comes from the graph input's tags (port edges and their first/last bend token), never from shapes in a layout.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-p3 <HEAD>` (given at launch). Never use git stash.
- P1-P2b (merged): the template is built from the graph input (JunctionNodeInputOfGraph; per-piece JunctionTrackPieceTag); SolveJunctionNodes' graph overload renders tagged routes and signals them by tokens; on flat facts LL_RR solves to its seed.
- D212 (DECISIONS.md; read it and D213, D214): a lane keeps its port row as long as it can; the row change sits next to the node that needs it. Engineer's rig finding: LL_RR's W in-lanes fan right after the port because the seed's X solve takes least x. In projection the solve snaps every class to the value nearest the seed (SolveJunctionNodes' `snap`), so it keeps that.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row); DESIGN-G1.md §13.3 and §13.8 item 6; DECISIONS.md D212.
- src/junction_nodes.h: JunctionNodeTemplate (all members), DiffJunctionNodeTemplates; junction_nodes.cpp: ReadPoints (how classes are formed from points), BuildJunctionNodeTemplate, SolveJunctionNodes' snap lambda, its Y snap and X snap loops ("5. Snap each X class" and the Y counterpart).
- src/junction_topology.h JunctionTrackPieceTag; src/tests/junction_nodes.cpp "the template comparator finds every member"; src/tests/junction_topology.cpp P1's oracle test and P2b's flat-solve tests.

## The rule (settled)
For each graph input path (route): its **in bend** = the pieces of the first token (tag.token != RUN, not a bridge piece) on the route's FIRST edge; its **out bend** = the pieces of the last such token on the route's LAST edge. A bend that lies wholly inside one class C on the axis of its edge's travel at the port (X for a W/E port, Y for an S/N port) makes C a lean candidate with direction: in bend: the travel sign along that axis (later = toward its node); out bend: minus the travel sign (earlier = toward its node). C leans only if (a) every point in C is a piece of that same bend (no node piece, no port piece, no other edge's piece: a node must never slide), and (b) every route that puts C up as a candidate gives the same direction. Otherwise C does not lean. If a bend spans several classes on that axis (a free diagonal), the candidate is the class of the bend's first piece (in) or last piece (out) and (a) applies to it.

## Tests (tag `[topology]`; record each red)
1. **Outer, "topology: LL_RR's port fans keep their port rows as long as they can" (D212):** the graph pipeline on LL_RR on flat facts, solved by the graph overload. For every lean class c (tpl.lean[c] != 0): with the solved values fixed for every other class, c cannot move one more step in its lean direction (rebuild the positions as the solve does up to its snap, fix all others, and RestrictRange(c, v + lean, v + lean) fails; or an equivalent check the solve exposes — if no check is possible without new API, STOP and report). And at least one W-side in-port class (an in bend on a route whose first edge starts at a W port slot) has values[c] != tpl.flat[c]. And CheckJunctionRules(layout, spec, topology) is empty. Red: before the lean, the W-side classes sit at flat (record).
2. **"topology: lean classes come from the port bends"**: for the T and LL_RR: tpl.lean has one entry per class; every nonzero entry's class satisfies the rule's (a); for LL_RR, the classes of the W-side in bends lean +1 (x grows toward the exit split). Red: member missing (compile) — for this guard test only.
- Edits allowed: P1's oracle test compares the templates with `lean` copied from the traced template into the graph one before diffing (the traced input has no tags, so no leans); the comparator test adds VEC(lean); P2b's flat-solve tests: their track/signal EQUALITY to the seed is dropped for LL_RR and the T where a class leans (they keep "no failure" and "checker clean"); the inner routes-at-flat test stays as is. No other existing test edits.

## Changes
1. JunctionNodeTemplate gains `std::vector<int8_t> lean; ///< Per class (X then Y): +1 snap to its greatest value, -1 to its least, 0 to the value nearest flat; nonzero only for a port bend's class (D212, DESIGN-G1 §13.3).` Built in BuildJunctionNodeTemplate from input.tags by the rule (all 0 when input.tags is empty). Comparator compares it.
2. SolveJunctionNodes' snap (both the legacy and the graph path share it; legacy templates have no leans so nothing changes there): in the Y snap and in the X snap, first snap every class with lean 0 as today; then the classes with lean != 0 in ascending class order, each fixed at `positions.Up(c)` (lean +1) or `positions.Lo(c)` (lean -1) — then accept as the existing snap's accept does; if accept rejects, step one value back toward flat and retry, as far as flat.
3. The LL_RR rig circuit test (`[.circuitllrr]`, src/tests/junction_design.cpp): its layout comes from the graph solve on flat facts (as test 1) instead of the seed render, so the rig shows the leaned fans; everything else in that test unchanged. Regenerate tools/ottd/layouts/circuit-llrr-TL4.json with the Release build (`JUNCTION_CIRCUIT=$PWD/tools/ottd/layouts/circuit-llrr-TL4.json build-rel/openttd_test "[.circuitllrr]"`) and commit it. If the circuit test then fails (a loop has no route, checker violations), STOP and report.

## Do not
- No lean for any class but a port bend's; do not change the template's constraints, the seed placement, or the legacy (traced) behaviour.

## Stop conditions
- A lean class contains a node or port piece by the rule's (a) but the rule says it leans (a contradiction); test 1 checker violations (report each + RenderJunctionAscii to docs/junction-tool/wip/p3-llrr.txt); an existing test fails beyond the allowed edits; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Add: Junction port fans keep their port rows as long as they can (D212)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "P3" and the count.

## Report
The standard report, plus per lean class: its route(s), direction, flat value and solved value; RenderJunctionAscii of LL_RR before (seed) and after (solved) to docs/junction-tool/wip/p3-llrr.txt. When you have reported, stop.

## Amendment 1 (orchestrator, after the stop: rule (a) left every LL_RR class unleaning)
1. Rule (a) relaxed: every point in C is a piece of the bend, or a RUN piece (token == RUN) of the bend's own edge and copy (the axis piece rigidly joined to a half-piece bend). Node pieces, port pieces and other edges' pieces still bar a lean.
2. A class whose lean-side bound is still the domain limit after the other classes are snapped does not lean (nearest-flat snap).
3. If no value from the extreme back to flat is accepted, the normal nearest-flat snap applies; only its failure is a solve failure.
4. (Amendment 2, after the second stop: a fan spans several X classes tied by the diagonal's fixed extent.) Every class on the edge's travel axis that holds a piece of the port bend leans, in the same direction; rule (a) per class, all-or-nothing per bend. A class shared by the bend's end and the next run's start is in the bend.
