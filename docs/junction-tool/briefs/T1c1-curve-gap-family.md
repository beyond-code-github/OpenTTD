# T1c-1: the curve-gap relation family on every route replaces the site floors (D217; DESIGN-G1 §14.3, §14.7 T1c)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-t1c1 <HEAD>` (given at launch). Never use git stash.
- D217 (DECISIONS.md, read it): placement relations come in rule families applied everywhere, never per site. Today the curve rule (JunctionCurveGaps: adjacent same hand g1, adjacent opposite 2, i..j apart min(2tl-1, (j-i) cl)) is enforced only by site floors: TL1b's `g3` terms at k_out (A) and d_min (D), `JunctionRunBetween` between chain elements (B, C, F), the unpinned S-bend's last run (E); SF1b's k_out; CS1's d_min. RECON-stretch C4 found a pair none covers (a doubled leg's lane-merge S-bend vs the out fan after a row stretch).
- Rows are fixed when X is solved (Y first), so along a lane path the units between two curves are a fixed constant plus 2 x the column difference of their anchor points, as long as every horizontal stretch between them has one heading (true for T and LL_RR paths; if not, STOP).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DECISIONS.md D217; DESIGN-G1.md §14.3; RECON-train-length.md §2-§3; RECON-stretch.md C4.
- src/junction_chain.cpp JunctionCurveGaps; src/junction_topology.cpp the placement helper: Elem, chains per edge, row chains (RowPts), copies, `edge_tiles`, bend_tiles, the TL1b floors (search `g3`, `JunctionRunBetween`, `k_out`, `d_min`), the reasons; RasterizeJunctionRoute (how a route's pieces and units come from the chain); the test "topology: curve spans in the seed routes".

## Tests (record each red)
1. **Outer, `[topologysweep]` "topology: the curve family alone keeps every seed clean"**: with the site curve floors removed (Changes 3), every cell of the TL1b sweep is checker clean (as the existing sweep test; it stays and must stay green), and per cell the placement equals the old seed except where a run is now shorter: for each such difference INFO the run and the site floor that was stricter than the rule. A run that is LONGER than before is a failure (the family must never add length the site floors did not). Red: with the site floors removed and no family, the sweep violates (record the count).
2. **`[topology]` "topology: symbolic curves match the rendered routes"**: at the test spec on T and LL_RR: for every lane path and copy, the symbolic curve list (Changes 1) has the same count, hands and unit positions as the curves of the rendered route (as the curve-spans test reads them). Red: stub returns empty.
3. **`[topologystretch]` stays green**; add Y stretches (the brief of T1b-1 forbade them; now: for every stack item s in {1, 2}, row gap +s via the existing seam pattern, i.e. a Y floor `item_pt[i] - item_pt[i-1] >= old + s` added through a new `y_floors` parameter mirroring x_floors) and REQUIRE no CurveSpeed (rule 3) violation in any solved Y-stretch run (other rules: count and INFO only; they are T1c-2's). Red: RECON-stretch C4 (tl 6 v196 cell or the test spec, whichever shows it; record).

## Changes
1. **Symbolic curves (file-static in junction_topology.cpp):** per lane path and copy, in travel order, each curve as (anchor point, unit offset from that point's column along the path's horizontal heading, hand, the element's reason), built from the chains the way RasterizeJunctionRoute walks them; vertical stretches contribute their fixed units (rows known in X).
2. **The family (X phase, after the relations, before facts):** for each path and copy, for every pair (i, j) JunctionCurveGaps returns for the path's hands: one difference `x_anchor_j - x_anchor_i >= ceil((min_gap - (off_j - off_i) - fixed_units_between) / 2)` oriented by heading, reason "curve gap" with the element of curve j. Pairs whose two curves share an anchor are checked as constants (a violation there is a placement failure naming both elements). Deduplicate identical relations.
3. **Remove the site curve floors:** the `g3` terms, `JunctionRunBetween` (back to the rule's run minimum of 2 tiles between elements), the k_out curve terms (keep k_out's non-curve minimum, if any; say what remains), the curve terms of d_min. Keep the S-bend unpin (E) and the waiting room (G): they are not curve floors. Keep the prio and doubled-leg floors.
4. **y_floors:** the fixed-choices overload gains `const std::vector<std::pair<int, int>> &y_floors = {}` after x_floors: (point, least row), reason "y floor". Test seam until the row-gap edit.
5. TESTS.md rows.

## Do not
- No occupancy, bridge or prio change (T1c-2). No change to the checker, JunctionCurveGaps, facts, the node path.

## Stop conditions
- A path with two horizontal headings; test 2 differs; test 1 finds a longer run or a violation; an existing test fails (other than a pinned number that becomes SHORTER with a clean checker: then record old/new and the site floor, and update only that number); anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test "[topologysweep]"`, `"[topologystretch]"`, `"[topoprojsweep]"`, `"[placeoracle]"` (to files; placeoracle heights 0 violations 0, solved >= 122); `tools/junction/jt gate build-rel` (426 before; report the count). Regenerate the circuit file; if it changes, commit it and say what moved.
- One commit: `Change: Junction curve-gap relations on every route replace the site floors (D217)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus: relations added per phase at the test spec (LL_RR); each changed seed number (old, new, which site floor was stricter); the Y-stretch rule-3 count before/after and the other rules' counts after; placeoracle line. When you have reported, stop.
