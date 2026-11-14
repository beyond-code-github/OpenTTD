# G4 2b2b0: item rows reach past a row's end nodes (D273; RECON-O3 finding 7)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The item-row recorder (2b2a2/2b2a3a) assumes each row's travel order starts and ends with a node: `assert(last >= 1 && travel[r][0].node && travel[r][last].node)` in `junction_placement.cpp` (grep `travel[r][0].node`). At LL_RR x LL_RR (LL²) rows 1-3 end with crossing edges after the row's last node (order `ROW 1: n2 e24 e26 n24 e14 e15`), so recording LL² at a point aborts. Those crossers cross the row's out edge (east of its merge) or, before the first node, its in edge.

The rule this step makes true (D273): on a row's travel order, the split is the first node item and the merge the last node item; a crosser before the first node crosses the row's in edge, one between them the leg, one after the last node the out edge; every rank that today assumes the split is at position 0 or the merge at `size - 1` uses that node's travel position instead. At the three seeds every row starts and ends with a node, so nothing there moves.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4c2`: run `git -C /home/pete/OpenTTD/.claude/o4c2 checkout -B wt-2b2b0 <HEAD given at launch>`. Leave untracked files alone.
- `build-rel` (Release) exists there; it holds the recon's probe objects, which your first build replaces. Delete stale `.pch` files if the build complains.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2b2b0/`.
- Build only `openttd_test`. Never start a game client.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `docs/junction-tool/DECISIONS.md`, row D273.
- `docs/junction-tool/RECON-O3-bands.md` finding 7.
- `docs/junction-tool/wip/recon-o3-probe.patch`: how the recon recorded LL² at form 100100000 at its joint optimum (the point and lengths it passed to `RecordAt`), and the bypass it used. Use the same recording path in the test; do not apply the patch.
- `src/junction/placement/junction_placement.cpp`: 1386-1420 (`travel`, `travel_pos`, `bridge_span`); 1540-1560 (`prio_rank`); 1560-1700 (the crossers' candidate block: the loop `for (int pos = 1; pos < last; pos++)` and the Under tokens it matches against `leg_edge[r]`); 1674-1760 (the occupant ranking: the assert, `split`, `merge`, `at_node`, the `node band` rank `0 : 100 * last`). Grep for any other use of `travel[r][0]`, `travel[r][last]`, `travel[r].size()` and list them in the report.
- `src/tests/junction/placement/junction_joint.cpp` 1360-1430 (`ChoicesWithForms`, `Ll2Start`, the "LL² at form 100100000" test).
- `src/tests/junction/topology/junction_topology.cpp`: the item-row tests (grep `item rows`).

## Test (red first)
In `src/tests/junction/placement/junction_joint.cpp`, after the "LL² at form 100100000 ..." test, tag `[junction][milp][.sweep]`:

**"LL² item rows reach past their end nodes"**
- Record LL² at form 100100000 with `port_offset = 24` as the existing test does, solve its joint (Optimal), then record again at the joint's point and lengths, the recon probe's way.
- Assert: `system.failed` is empty; for rows 1, 2 and 3, every edge of `order.rows[r]` that is not a node and lies before the row's first node or after its last node in travel order has, on each of that row's stack item rows, exactly one recorded candidate piece whose `chosen` holds at the recorded point (the same count the existing "crossers: one piece covers ..." test makes at the seeds); the recorder's uncovered count (`crosser_uncovered`) is 0.
- Expected red: the process aborts on the assert at the item-row recorder (record the message). If it fails for another reason, stop and report.

Keep the three seeds' existing item-row and crosser tests unchanged; they must stay green with no re-pin.

## Changes (`src/junction/placement/junction_placement.cpp` only)
1. Where `travel` is built (record only), compute per row `first_node[r]` and `last_node[r]`: the travel positions of the first and last node items (each row has at least one node; assert that).
2. The candidate loop: iterate every travel position that is not a node (not only `1 .. last - 1`). The row edge the crosser passes under is `in_edge[r]` before `first_node[r]`, `leg_edge[r]` between, `out_edge[r]` after `last_node[r]`; match its Under tokens against that edge instead of `leg_edge[r]`. Everything else in the block is unchanged.
3. The ranking block: replace the assert with `assert(first_node[r] < last_node[r])` only if a row with one node cannot reach it, otherwise stop and report; `split` and `merge` are the items at `first_node[r]` and `last_node[r]`; the `node band` rank is `100 * first_node[r]` for the split's band edges and `100 * last_node[r]` for the merge's.
4. `prio_rank` is `100 * last_node[r] - 25`.
5. Any other use listed under "Read first" that assumes the ends are nodes: change it the same way only if it is one of the four above in kind; otherwise stop and report it.

## CLAUDE.md
`src/junction/placement/CLAUDE.md`, the item-row fact (begins "A recording lists each stack item row's occupants"): add one sentence: "A row's split and merge are its first and last nodes in travel order; a crosser before the split crosses the row's in edge, one after the merge its out edge."

## Do not
- Do not change the seeds' recordings, the joint model, O2, or any test's pinned value.
- Do not read the recorded rows in the solver.
- Do not apply or commit the recon probe patch; do not touch `docs/junction-tool/wip/`.

## Stop conditions
- The red is not the assert abort.
- A row with crossers past an end node has a crosser whose Under tokens match none of in/leg/out edge.
- Any seed test changes value.
- `crosser_uncovered` is not 0 at LL² after the change: report the edges and rows, do not work around it.

## Gates
- While working: `tools/junction/jt fast placement` (the jt runner; see `docs/junction-tool/TESTS.md`), output to a file.
- Before the commit: `tools/junction/jt gate`, then `tools/junction/jt sweep` once (your new test is a sweep case). Report count and time of each; budgets in TESTS.md (sweep is near 60 s; report the new case's own time).
- One commit: `Fix: Junction item rows reach past a row's end nodes`, body naming LL² rows 1-3, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. The CLAUDE.md edit goes in the same commit.

## Report
References read; commits (hash, subject); red evidence (test name and the failure seen); types and members added or removed (names only); suite results (count, time, any failure you did not cause); the other uses of the travel ends you found; stops and anything left undone; state: `git status --porcelain` and `git log -1` of the worktree. When you have reported, stop.
