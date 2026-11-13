# S0: band chains start on the edge's real heading; row-node relations get their own label (REVIEW-port-fans F1, F4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-s0 <head>` with the head given in your launch prompt. Builds `build` (Debug) and `build-rel` (Release) are configured. Ignore docs/junction-tool/wip/. Never use git stash.
- Source: docs/junction-tool/REVIEW-port-fans.md, findings F1 and F4 and Q4 step S0. The SF1 attempt that exposed this is saved as /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/sf1-stopped.patch; do NOT apply it, it is evidence only.
- The bug: in `PlaceTrackGraph` (src/junction/placement/junction_placement.cpp, band-edge chain builder, line 245) a band edge's start heading is taken from the edge kind: `cur = In ? north(0) : Out ? south(2) : row node ? row heading : north`. An In edge that starts at a row node (an exit split on a main row going down into the band) then starts north instead of on its row heading, an exit-side band edge carrying main-to-branch paths starts north instead of south, and a joiner-side Out edge (branch-to-main paths, a band merge up to a joiner merge) starts south instead of north. The programs assume the real heading (G1 §10.2).

## Read first
src/junction/CLAUDE.md, src/junction/placement/CLAUDE.md, src/junction/topology/CLAUDE.md, src/tests/junction/CLAUDE.md, CODINGSTYLE.md (naming, comments), REVIEW-port-fans.md (F1, F4, Q4), and:
- junction_placement.cpp lines 120-125 (`VX`, `VY`: heading 0 N, 1 E, 2 S, 3 W), 239-285 (band-edge chain builder), 1300-1325 (ports, "S port spacing", row nodes).
- junction_topology.cpp `ProgramJunctionTrackGraph`, lines 1136-1175 (`heads_south` and its two uses).
- src/tests/junction/topology/junction_topology.cpp: `PlacementOf` (about line 840) and the test that calls it at about line 903.

## The rule (both places)
A band edge's heading comes from where it starts and what it carries:
- it starts at a row node (`is_row_node(edge.from)`): the row's heading (`east[row] ? 1 : 3`), as today;
- else, its paths come from a main slot (`group_of(paths[edge.paths[0]].from).main`): south (2);
- else (its paths come from the S side): north (0).
A band edge carries paths of one kind only (all exit paths from main slots, or all joiner paths from S slots); if an edge's paths disagree on this, `return fail("a band edge carrying exit and joiner paths is not ruled yet");` in placement (and treat it the same way in programs with the programs' `fail`).

## Tests (red first; tags `[junction][topology]`; in src/tests/junction/topology/junction_topology.cpp)
1. "placement: a band edge from a main row starts on its row heading and runs south (REVIEW-port-fans F1)": topology `MakeJunctionHubTopology(ParseLineNotation("L_R").spec, b)` where `b` is a branch notation with ONE S in lane and TWO S out lanes (try `"L_RR"` and `"LL_R"`, check with `ExpandJunctionLanes`/the topology's groups which one gives one S in lane and two S out lanes, and use that one; say which in the report). Through `PlacementOf(t, 64, &error)`:
   - Before the change: record the error text (expected: it names a "Corner size" or "S-bend size" of a band edge; record whatever it is).
   - After the change: assert `error` is empty, and for every band edge (`from` a row node, `to` a band node) whose program starts with a Corner, the Corner's result span (`placement.edges[e][0]`) ends south of (larger y than) the edge's start node; and for every band edge carrying exit paths whose program has an `SBend` with `hand` +1, its span ends west of (smaller x than) its start, -1 east (heading south, +1 is west).
   - If after the change placement still refuses this topology, stop and report the error text verbatim; do not add any other change.
2. "programs: heads_south follows the edge's paths (REVIEW-port-fans F1)": no new test if no existing topology changes a program; instead report whether any `Under` copy order changed in the gate and identity runs (it must not).

## Change
1. junction_placement.cpp, band-edge chain builder (line 245): replace the `cur` expression by the rule above. Comment: "A band edge starts on its row's heading at a row node, else south when it carries exit paths from the main line and north when it carries joiner paths from the S side (G1 §10.2, REVIEW-port-fans F1)." `edge.kind` stays unused for the heading.
2. junction_topology.cpp, `heads_south` (line 1138): `heads_south` = the edge's paths come from a main slot (same test as above; for an edge from a row node this is true for exit paths, which is where it heads after its Corner). Same comment reference.
3. junction_placement.cpp, the row-node relations (about line 1314, "The rows' nodes: the exit split lies on the exit item's row..."): add `why("row nodes");` before them so they no longer print as "S port spacing" (F4).

## Do not
- Do not change programs other than `heads_south`, the S port spacing, the "S edge" relation, the run floor table, or any row-edge code.
- Do not add fans or apply the SF1 patch.
- Do not change any existing test.

## CLAUDE.md (same commit)
In src/junction/placement/CLAUDE.md, after the line beginning "Every edge is one chain of elements": add "A band edge's chain starts on its row's heading when it leaves a row node, else south when it carries exit paths and north when it carries joiner paths (G1 §10.2)."

## Stop conditions
- Any existing test or identity number changes (the review states every shape placed today keeps its heading; if identity moves, stop and list the cases).
- Test 1's topology cannot be found with the notations above, or still fails after the change.
- A band edge whose paths disagree on main/S origin appears in any existing test.

## Gates
- Before: `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root (baseline: gate 498 cases, 3042017 assertions; identity 4 cases, 255 assertions).
- After: Debug and Release build with no new warnings (known: `unused variable 'n'` in junction_topology.cpp, `unused function 'TravelY'` in the test file, 10 `-Wmissing-field-initializers` in legacy/junction_nodes.cpp); gate passes with 1 case added; identity unchanged; `tools/junction/jt fast build '[topology]'` within budget.
- One commit: `Fix: Junction band chains start on the edge's real heading`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash and subject; the notation chosen for test 1; red evidence; baseline and after counts; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
