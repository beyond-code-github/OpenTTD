# SF1: fans on S-side branch port edges (D230, D203(1), D38)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-sf1 <head>` with the head given in your launch prompt. Builds `build` (Debug) and `build-rel` (Release) are configured. Ignore docs/junction-tool/wip/. Never use git stash.
- Rule: D230 (read it in docs/junction-tool/DECISIONS.md). Cause: docs/junction-tool/RECON-llrr-placement-contradiction.md sections 0, 1.2 and 3. Today L_R x LL_RR (pairing any) fails placement with a contradiction whose text includes "S port spacing": the S ports are 1 column apart and every S port edge is a straight vertical run, so the two S in lanes cannot open up.

## Read first
src/junction/CLAUDE.md, src/junction/topology/CLAUDE.md, src/junction/placement/CLAUDE.md, src/tests/junction/CLAUDE.md, CODINGSTYLE.md (naming and comments), D230, the recon file, and:
- src/junction/topology/junction_topology.cpp: `ProgramJunctionTrackGraph` from its start (about line 966) to its end (about line 1195), in particular the band-edge part "a. Start" to "d. End".
- src/junction/placement/junction_placement.cpp: the `Elem` struct (about 199-225); the band-edge chain builder (about 239-285); `refresh` (about 482-515); the bends list at about 784; the chain relations loop from about 1442 to the "bridge over S-bend" block (about 1565); the facts group at about 1850; the rounds (about 2028-2125); the result spans for band edges (about 2242-2255).
- src/junction/placement/junction_render.cpp about lines 100-170 (how a Fan token renders: an empty one is skipped, either hand is matched).
- src/tests/junction/topology/junction_topology.cpp: `PlacementOf` (about line 840) and one test that calls it (about line 903).

## Tests (red first; tags `[junction][topology]`; in src/tests/junction/topology/junction_topology.cpp)
Topologies: `MakeJunctionHubTopology(ParseLineNotation("L_R").spec, ParseLineNotation("LL_RR").spec)` (L_R x LL_RR) and the same with `"LL_RR"` twice (LL_RR x LL_RR), pairing default (any).
1. "placement: S branch lanes open up by fans inside the box (D230)": L_R x LL_RR through `PlacementOf(t, 64, &error)`. Assert: `error` is empty; the S in ports of the branch group lie 1 column apart on one row (notation spacing kept); for each S in slot, the node at the far end of the edge from that port lies at or west of the port's column for the west slot of the group and at or east for the east slot; and at least one of the two differs from its port's column (a fan of at least one column was needed). Red: before the change the error contains "S port spacing". Record it.
2. "programs: an S branch port edge of a group with several lanes carries a fan (D230)": LL_RR x LL_RR, build paths, graph, order, structure and programs as `PlacementOf` does. For every graph edge with a port end on the S side: if the edge starts at the port, its first token is `Fan`; if it ends at the port, its last token is `Fan`; the token's `index` is `JunctionTrackStep::NO_ROW`; its `hand` is -1 when the port slot is the first (west) slot of its two-slot group and +1 for the second. Also: `JunctionTTopology()` (one lane per S group) has no Fan token on any edge with a port end on the S side. Red: the first assertion fails (no Fan token). Record it.
3. Report-only probe (not committed): run L_R x LL_RR and LL_RR x LL_RR (any) through `MakeJunctionGraphSeed` with `MakeTestJunctionSpec()`; report each one's error text or "seeded". It must not crash; a crash is a stop.

## Change
### Programs (junction_topology.cpp, `ProgramJunctionTrackGraph`, the band-edge part)
Add one lambda next to `s_side_slot`:
- `south_fan_hand(slot)`: 0 if the slot's group is not on the S side or is a main group; else with `k` = the number of slots of the slot's group and `i` = the number of slots `s < slot` with `slots[s].group == slots[slot].group`: -1 if `2 * i + 1 < k`, +1 if `2 * i + 1 > k`, 0 if `2 * i + 1 == k` (the middle lane of an odd group). A group of one lane gives 0.
In the band-edge loop (the edges that are not row edges):
- Before "a. Start": if `!edge.from.node` and `south_fan_hand(edge.from.index) != 0`, `program.push_back(step(StepKind::Fan, south_fan_hand(edge.from.index), Step::NO_ROW));`.
- After "d. End": if `!edge.to.node` and `south_fan_hand(edge.to.index) != 0`, `program.push_back(step(StepKind::Fan, south_fan_hand(edge.to.index), Step::NO_ROW));`.
- Comment above the first: "An S branch port of a group with several lanes opens its lane inside the box by a fan, west half west and east half east, and keeps the notation spacing at the port (D230, D203, D38)."
- Hand meaning: the first curve, as an SBend's (+1 right, -1 left). Heading north from an in port, -1 moves west; heading south to an out port, -1 moves east, so the node stays west of the port for both. Do not change any row-edge program.
- Update the `Kind::Fan` doc comment in junction_topology.h to: "The S-bend between a port row and a stack row on a row edge, or none when they coincide; on an S branch port edge, a vertical S-bend sized by placement, 0 allowed (D230)." and the `index` comment's "Fan: the stack item at its inner end" to "Fan: the stack item at its inner end, or NO_ROW at an S branch port".

### Placement (junction_placement.cpp)
1. `Elem` gains `bool fan = false; ///< A band edge's fan at an S branch port (D230): a vertical S-bend sized from the columns that may be empty (m 0, no tiles).`
2. Band-edge chain builder: a `StepKind::Fan` token builds an `Elem` exactly as an SBend token does (`k = Elem::K::SBend`, `hand = step.hand`, `index = NO_ROW`, `hout = cur`, points as for an SBend, `free_m = true`) with `fan = true` and `m = 0`. Change the builder's condition to cover the Fan token; nothing else in it.
3. Chain relations, SBend branch, vertical case (`el.hin == 0 || el.hin == 2`): for `el.fan`:
   - X: with `fixed` as today but with `el.m` (may be 0); without `fixed`, `ge` with 0 instead of `el.m`.
   - Y: if `el.m == 0`, `eq(1, el.last, el.first, 0)`; else as today.
   Comment: "A fan at an S branch port: any size toward its side, 0 allowed (D230)."
4. Chain runs: in the same loop, where the run from `el` to `next` is added: an empty fan joins its port tile and the run on its node side stays. So if (`i == 0`, `next.fan`, `next.m == 0`) or (`el.fan`, `el.m == 0`, `i + 2 == elems.size()`): `why("fan size", e)`, `eq(0, next.first, el.last, 0)`, `eq(1, next.first, el.last, 0)`, and `continue` without the run. Every other run is unchanged (a run to or from a non-empty fan takes the table's default floor 2, the same as the port run today).
5. Rounds and fixed choices: the three places that set a free S-bend's size (`el.m = std::max(1, ...)` with `fixed->m` and with the columns, and the change check in the rounds loop) use the size without `std::max(1, ...)` when `el.fan`.
6. Empty bends: every site that reads an SBend element's tiles as a curve skips an element with `el.fan && el.m == 0`: the bends list at about line 784, `bend_heights` and the group loop at about line 1854. The result spans already take the Fan branch by token kind; check that they do and change nothing there.
7. The "bridge over S-bend" search for the crossed S-bend skips `o.fan` elements (a fan is no crossed S-bend).
Change nothing in render, signals or levels. If render or signals fail on the probe, report where and stop; do not guard it.

## Do not
- Do not change the S port spacing relation, the run floor table, the bridge-over-S-bend pin, or any row-edge fan code (the `Elem::K::Fan` paths).
- Do not add an objective that prefers empty fans.
- Do not change any existing test.

## CLAUDE.md (same commit)
- src/junction/topology/CLAUDE.md, after the line beginning "Rows: lanes spread inside the box": add "An S branch port edge of a group with several lanes starts (in) or ends (out) with a Fan token: the west half of the group opens west, the east half east, the middle lane of an odd group has none (D230)."
- src/junction/placement/CLAUDE.md, after the line "Stub length at a branch port is two tiles": add "An S branch port's fan is a vertical S-bend sized by the solve, 0 columns allowed (then it lies on the port's run); the ports keep the notation spacing (D230, D38)."

## Stop conditions
- Any existing test or identity number changes (every topology placed today has one-lane S groups, so none may move).
- Test 1 fails after the change for a reason other than the fan (report the error text).
- A crash anywhere, or an assert in placement, render or signals in the probe.

## Gates
- Before the change: `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root, for a baseline.
- After: Debug and Release build with no new warnings (known: `unused variable 'n'` in junction_topology.cpp, `unused function 'TravelY'` in the test file, 10 `-Wmissing-field-initializers` in legacy/junction_nodes.cpp); gate passes with 2 cases added; identity unchanged; `tools/junction/jt fast build '[topology]'` within budget.
- One commit: `Add: Junction S branch lanes open up by fans inside the box (D230)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash and subject; red evidence for both tests; members added; baseline and after counts (gate cases and assertions, identity); the probe results; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
