# DB1a: programs for a doubled bridge on a row's out edge (D229, DESIGN-G2 §G2.4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-db1a <head>` with the head given in your launch prompt. Builds `build` (Debug) and `build-rel` (Release) are configured. Ignore docs/junction-tool/wip/. Never use git stash.
- The rule is DESIGN-G2-general-graph.md §G2.4 and D229. The recon is docs/junction-tool/RECON-doubled-row-edges.md (sections 1 and 3a).
- Today `ProgramJunctionTrackGraph` (src/junction/topology/junction_topology.cpp, "Bridges." block about line 1050) refuses a doubled bridge on any row edge other than the leg: "a crossed lane in or out edge is not ruled yet". LL_RR x LL_RR (pairing any) reaches it: row 1's out edge (joiner merge to the E out port, lane 1) is crossed by lane 0's exit legs.

## Read first
src/junction/CLAUDE.md, src/junction/topology/CLAUDE.md, src/tests/junction/CLAUDE.md, DESIGN-G2 §G2.4, the recon file, junction_topology.cpp lines 170-200 (`FindRowItems`), 965-1130 (`ProgramJunctionTrackGraph` up to the row-edge programs), and junction_topology.h around line 200-250 (`JunctionTrackStep`, `RowItems`).

## Tests (red first; tags `[junction][topology]`; in src/tests/junction/topology/junction_topology.cpp)
1. "programs: a crossed out edge carries a doubled bridge (D229, G2 §G2.4)": LL_RR x LL_RR, `JunctionLanePairing::AnyToAny`; build paths, graph, order, structure and programs as the MJ1 test does. Assert:
   - `ProgramJunctionTrackGraph` returns no error;
   - at least one row's out edge (a row edge whose `to` is a port) has a doubled bridge in `structure.bridges`; for every such edge `e` with row `r`, crossings `X = order.edge_crossings[e]`, Copy item `c = FindRowItems(structure.stack, r).copy`, and the stack's Lane item `l`:
     - `programs.edges[e]` is exactly `Over(x)` for each x in X in order, then `SBend(hand, c, copy 0, LaneMerge)`, then `Fan(0, c)`;
     - `programs.copies[e]` is exactly `SBend(hand, c, copy 0, LaneSplit)`, then `Over(x)` for each x in X;
     - `hand` is +1 when (`c > l`) equals `east` of the row (the row runs W to E when its in slot is on the W side), else -1.
   - Red: before the change, the error is "a crossed lane in or out edge is not ruled yet". Record it.
2. "programs: FindRowItems names a row's copy item on either side of the lane": build a stack by hand with, for row 0, `[Copy, Lane, Approach]`, and for row 1, `[Approach, Lane, PrioAndCopy]`. Assert `copy` is the Copy item for row 0 and the PrioAndCopy item for row 1; `north` is unchanged from today (row 0: the Copy item; row 1: NONE). Red: does not compile until `copy` exists; then record the first assertion failure with the member defaulted to NONE.
3. Report-only probe (not committed): run LL_RR x LL_RR (any) through `MakeJunctionGraphSeed` with `MakeTestJunctionSpec()` and report the error text and, if the placement why channel names a row or edge, that text. It must not crash; a crash is a stop.

## Change
1. `RowItems` (junction_topology.h) gains `size_t copy; ///< The row's Copy or PrioAndCopy item, on either side of the lane, or NONE.` `FindRowItems` sets it for a stack item of the row with kind `Copy` or `PrioAndCopy`. Update every aggregate initialisation of `RowItems` (e.g. `RowItems items{RowItems::NONE, ...}`) to include it; nothing else uses it in this step.
2. In the "Bridges." gate loop: keep `row_doubled[r] = true` only for the leg. For a doubled row edge that is not the leg:
   - an in edge (`!edge.from.node`): `return fail("a crossed lane in edge is not ruled yet");`
   - an out edge (`!edge.to.node`): allowed; do not set `row_doubled`.
3. In "The programs.", the row-edge branch for an out edge (`else` branch, today `Fan(0, lane)`): when `doubled_bridge(e)`:
   - `c = row_items[r].copy`; if NONE, `return fail("a row without its side tracks is not ruled yet");`
   - `hand = ((c > row_items[r].lane) == east[r]) ? +1 : -1`;
   - `programs.edges[e]`: `Over(0, x)` for each `x` in `crossings`, then `step(SBend, hand, c, 0, LaneMerge)`, then `step(Fan, 0, c)`;
   - `programs.copies[e]`: `step(SBend, hand, c, 0, LaneSplit)`, then `Over(0, x)` for each `x` in `crossings`.
   - Otherwise unchanged (`Fan(0, lane)`).
   Comment above it: "A doubled out edge (D229, DESIGN-G2 §G2.4): copy 1 S-bends from the lane to the copy item before the bridge, copy 0 after it; the fan leaves from the copy item."
4. Do not change the leg's programs or the in edge's fan rule.

## Do not
- Do not change placement, render, signals or levels. If placement crashes on the new programs (assert, out-of-range index), stop and report where; do not guard it.
- Do not change any existing test.

## CLAUDE.md (same commit)
In src/junction/topology/CLAUDE.md, after the line beginning "Over and under: a crossed row edge", add: "A crossed out edge (the joiner merged before the crossing, D229) carries its doubled bridge like the leg with the items swapped: copy 1 S-bends from the lane to the row's copy item before the bridge, copy 0 after it, and the fan leaves from the copy item; a crossed in edge is refused as not ruled (DESIGN-G2 §G2.4)."

## Stop conditions
- Any existing test or identity number changes.
- The out-edge crossings are not all crossings of a doubled bridge on that edge, or a row with a doubled out edge has no Copy item.
- A crash anywhere in the probe.

## Gates
- Before the change: `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root, for a baseline.
- After: Debug and Release build with no new warnings (two known: `unused variable 'n'` in junction_topology.cpp and `unused function 'TravelY'` in the test file); gate passes with 2 cases added; identity unchanged; `tools/junction/jt fast build '[topology]'` within budget.
- One commit: `Add: Junction programs carry a doubled bridge on a crossed row out edge (D229)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash and subject; red evidence for both tests; baseline and after counts; the probe's error text; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
