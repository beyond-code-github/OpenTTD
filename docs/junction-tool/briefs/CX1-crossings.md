# CX1: structure crossings without one joiner (D223, D227 C1 and C3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-cx1 <head>` with the head given in your launch prompt. Builds: `build` (Debug) and `build-rel` (Release), both configured. Ignore docs/junction-tool/wip/. Never use git stash.
- Rulings: D223 and D227 in docs/junction-tool/DECISIONS.md; the cards they close are docs/junction-tool/CARDS-crossings.md (C1, C2, C3).
- Today `StructureJunctionTrackGraph` (src/junction/topology/junction_topology.cpp, the "Bridges." block, around line 894) handles a crossing of two non-row edges only when exactly one edge is a joiner (all its paths come from branch slots): the joiner goes over on a single bridge. Every other pair fails with "a branch crossing without one joiner is not ruled yet". LL_RR x LL_RR under pairing any (the default) is refused there.

## Probe facts (LL_RR x LL_RR, pairing any; measured by the orchestrator)
Slots are named side, in/out, lane, M (main) or B (branch); e.g. `W in 0 M` is the W-side main in slot lane 0. Rows: `W in 0 M` row 0, `W in 1 M` row 1, `E in 0 M` row 2, `E in 1 M` row 3. The branch is on the S side, below the last row. The non-row crossings that today fail, by the paths of their two edges, with the edge that must go over under this brief:

| Rule | Edge A paths | Edge B paths | Over |
|---|---|---|---|
| C1 | W in 0 M > S out 0 B | W in 1 M > S out 1 B | A (row 0 < row 1) |
| C1 | E in 0 M > S out 1 B | E in 1 M > S out 0 B | A (row 2 < row 3) |
| C1 | E in 0 M > S out 0 B, E in 1 M > S out 0 B | W in 0 M > S out 1 B, W in 1 M > S out 1 B | B (rows {0,1} < {2,3}) |
| C3 | S in 0 B > E out 0 M, S in 0 B > E out 1 M (levels 8 -> 4) | S in 1 B > W out 0 M, S in 1 B > W out 1 M (8 -> 8) | B (near end 8 > 4) |
| C3 | S in 0 B > W out 0 M (8 -> 6) | S in 1 B > W out 1 M (8 -> 8) | B (8 > 6) |
| C3 | S in 0 B > E out 1 M (4 -> 4) | S in 1 B > E out 0 M (4 -> 2) | A (4 > 2) |

Under pairing keep, an In edge from a main port carries both a through path and a turning path before its split (C2). This brief refuses C2 by name; it needs a doubled bridge off the row, which is a later step.

## Read first
src/junction/CLAUDE.md, src/junction/topology/CLAUDE.md, src/tests/junction/CLAUDE.md, CODINGSTYLE.md (only the naming and comment sections), DECISIONS D80, D174, D223, D227, CARDS-crossings.md, junction_topology.cpp lines 817-920 (StructureJunctionTrackGraph: its `group_of`, `row_of`, `end_level`, `is_row_edge`, and the "Bridges." block), and the MJ1 test in src/tests/junction/topology/junction_topology.cpp ("branch legs bound for one main lane merge before joining it") for how a test builds paths, graph, order and structure.

## Tests (red first; tags `[junction][topology]`; in src/tests/junction/topology/junction_topology.cpp)
1. "structure: crossings without one joiner follow D223 and D227 (C1, C3)": LL_RR x LL_RR, `JunctionLanePairing::AnyToAny`. Build paths, graph, order and structure as the MJ1 test does. Assert:
   - `StructureJunctionTrackGraph` returns no error;
   - for each of the six table rows, find edge A and edge B by their exact path sets (write a small local helper that matches an edge's paths against a list of (from slot, to slot) pairs, slots named by side, in, lane and main); exactly one bridge has `edge` = the Over edge and the other edge in `crossed`, with `doubled == false` and `tunnel == false`; and no bridge carries the other edge over the Over edge.
   - Red: before the change, the structure error is "a branch crossing without one joiner is not ruled yet". Record it.
2. "structure: a through edge off its row that crosses is refused (C2 not yet built)": LL_RR x LL_RR, `JunctionLanePairing::LaneKeepsLane`. Assert the structure error is exactly "a crossing of a through edge off its row is not ruled yet". Red: before the change the error is "a branch crossing without one joiner is not ruled yet". Record it.
3. Report-only probe, not a committed assertion: run LL_RR x LL_RR (any) through `MakeJunctionGraphSeed` with `MakeTestJunctionSpec()` once and report the first refusing stage's error text. G2b′ (row shape) and G2c (several bridges on a band edge, F3) are expected blockers.

## Change
In `StructureJunctionTrackGraph`, in the "Bridges." loop over `graph.crossings` (pairs where neither edge is a row edge), replace the single `if (bi == bj) return fail(...)` line with these rules, in this order. Keep `from_branch` as it is. Add one lambda next to it:
- `carries_through(e)`: true when any path of edge `e` has both its from and its to group main (a through movement).

Per crossing `(i, j)`:
1. If `carries_through(i) || carries_through(j)`: `return fail("a crossing of a through edge off its row is not ruled yet");` (C2, D227: it goes over, on a doubled bridge, not built yet).
2. If `bi != bj`: unchanged; the joiner goes over: `structure.bridges.push_back({bi ? i : j, {bi ? j : i}, false});`.
3. If both are joiners (`bi && bj`), C3: an edge's near end is `std::min(end_level(edge.from), end_level(edge.to))`. The edge whose near end has the larger level goes over. Equal: `return fail("two joiners that meet the main line at one level are not ruled yet");`.
4. Else (both carry only main-to-branch turning paths), C1: an edge's main row is the smallest `row_of[paths[p].from]` over its paths. The edge with the smaller main row goes over (it comes from the far side of the main line; the branch is on the S side, G1 §8). Equal: `return fail("two turning legs from one main row are not ruled yet");`.
5. For 3 and 4: `structure.bridges.push_back({over, {under}, false});` A bridge carrying branch-bound traffic is single (D174).

Comments: one line above the block: "Crossings of two non-row edges (D223, D227): through goes over (C2, not built), a joiner goes over a turning leg, of two joiners the one meeting the main line farther from it goes over (C3), of two turning legs the one from the far side of the main line goes over (C1)." Keep the existing `std::stable_sort` after the loop.

## Do not
- Do not change order, programs, placement, render, signals or the row-edge bridge rule.
- Do not merge several bridges of one edge into one; several entries for one edge are allowed here (placement refuses more than one on a band edge today, F3; G2c handles that).
- Do not change any existing test.

## CLAUDE.md (same commit)
In src/junction/topology/CLAUDE.md, replace the line beginning "A crossing of two non-row edges needs exactly one joiner" with:
"A crossing of two non-row edges: a joiner goes over a turning leg; of two joiners the one whose near end lies farther from the main line goes over (C3); of two turning legs the one from the far side of the main line goes over (C1); each on a single bridge (D174, D223, D227). A through edge off its row in a crossing is refused as not ruled until doubled bridges off the row exist (C2)."

## Stop conditions
- Any existing test or identity number changes.
- A table row's edges cannot be found by their path sets, or the over edge differs from the table.
- The first test still fails after the change for a reason other than the rules above.

## Gates
- Before the change: `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root, for a baseline.
- After the change:
  - Debug and Release build, with no new warnings.
  - The gate passes, with 2 cases added.
  - Identity is unchanged.
  - `tools/junction/jt fast build '[topology]'` passes within budget.
- One commit: `Add: Junction structure rules crossings of two joiners and of two turning legs (D223, D227)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read.
- Commit hash and subject.
- Red evidence for both tests.
- Baseline and after counts (gate cases and assertions, identity).
- The probe's first refusing stage and its error.
- Stops.
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
