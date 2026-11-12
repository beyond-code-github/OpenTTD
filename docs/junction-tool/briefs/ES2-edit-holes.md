# ES2: close the two placement holes ES1's edit sweep found, and commit the sweep (D193, D217)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-es2 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes2: never touch it. Never start a game client. Use absolute paths.
- ES1 (brief briefs/ES1-edit-soundness-sweep.md) wrote the sweep "topology: every placement edit fails with a reason or is checker-clean" (patch: /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/es1-test.patch; counterexamples: es1-ce.txt in the same folder). LL_RR is clean at HEAD; the T has 92 dirty edits, all from two atoms:
  - **H1 port below the box:** port_offset 1 -> -1 places a port tile at y -1 (rule 1 "unexpected exit", tile 0,-1). The placement accepted a point above the box's first row.
  - **H2 corner shrink:** T `corner[7][0]` 5 -> 4 yields rule 5 "tile shared with another bridge" (e.g. tile 19,5 at tl 3 v 100). Some relation that keeps two bridges apart does not read the corner's span.
- D217: no special cases; every fix is a general relation family read from the graph and the choices, not a test on this edge or token.

## Read first (once each, offset/limit)
CODINGSTYLE.md (C++ part); the ES1 patch and es1-ce.txt; src/junction_topology.cpp: the Y phase (bounds, port rows, port_offset), the corner token's placement, the bridge middle clearance family and the occupancy family (as L1 left them); src/junction_rules.cpp rule 5 (what "shared with another bridge" checks).

## Step 0 (findings first, no code change)
- H1: where port rows get their row and why no lower bound of 0 applies to them (file:line); whether any other Y point lacks it.
- H2: on T tl 3 v 100 with corner[7][0] = 4: which two bridges (graph edges, tokens) share tile (19,5), which tiles each covers (head, middle, ramp), and which existing relation should have separated them and why it does not see the corner's span (file:line). If the separation needs a relation the graph does not have, or a choice between approaches, STOP here and report.

## Changes (after step 0)
1. H1: every Y point has the box's lower bound (row >= 0), with a reason naming the point (e.g. "box"), so a port offset that would place a port above the box fails with that reason. If the box's first row is not 0 by construction, stop and report.
2. H2: the relation step 0 names, extended to read the corner's span as it reads other spans, in its general form (every bridge pair, every corner), with a reason naming both bridges.
3. Apply the ES1 test patch as is.

## Tests (record each red)
1. **Outer:** the ES1 sweep: dirty == 0 on every spec (red: the 92 counterexamples at launch HEAD). Report the new EDITSOUND lines (failed/clean move; say which edits moved from dirty to failed and that the reasons name the box or the bridges).
2. Every existing test and sweep green with identical numbers (`[topologysweep]`, `[topologystretch]`, `[placeoracle]`, `[realterrainsweep]`, `[topoprojsweep]`, circuit byte-identical). If an unedited seed moves: STOP and report.
3. `[topology]` "topology: a port above the box fails with its reason" (T, port_offset -1) and "topology: a corner edit keeps bridges apart" (T tl 3 v 100, corner[7][0] = 4: fails with the bridge relation's reason, or places a layout CheckJunctionRules accepts: say which and why). Red: both accepted with a checker violation at launch HEAD.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; the sweeps above in build-rel (diff against launch HEAD; report `[topologysweep]` time); `tools/junction/jt gate build-rel` (report the count). TESTS.md rows (ES1's sweep row and the two tests).
- One commit: `Fix: Junction placement bounds ports by the box and keeps bridges apart across corner edits`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus step 0's findings and the EDITSOUND lines. When you have reported, stop.

## Amendment 1 (orchestrator, after step 0)
- H1: as step 0 found: check `port_row[s] >= 0` right after `set_port_rows` (fixed branch and wherever else set_port_rows runs), failing with reason "box" naming the port slot and its row.
- H2: option (a). A head whose flat side is 0 (on the bridge's row, inside its span) is a contradiction of the bridge middle clearance family: the placement fails with that family's reason naming both edges and the tile (sound and general; the flat state is already illegal, and a later separation would be negotiation's job, DESIGN-G1 §17). No side-picking rule.
- Then changes 3, tests 1-3, gates and the commit as the brief says. Test 3's corner case expects the failure with the clearance reason.
