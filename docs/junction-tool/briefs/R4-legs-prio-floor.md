# R4: undoubled legs on their chains, the prio element as a node floor, and the curve family reading row chains (DESIGN-G1 §16 R4; RECON-row-chain §1 item 6, §4 step 4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-r4 <HEAD>` (given at launch). Never use git stash. Another agent may work in .claude/nodes2: never touch it.
- R3 merged (on top of Z2e's joint level solve): in/out edges are placed by their chains (Fan elements live; rp.fif/fil/fof/fol alias the chain points). R3's test "a fan edge may hold more than its fan" showed a hole: the curve-gap family's symbolic routes (`sym_routes`) take the row fans from the row blocks, so an S-bend after a fan on an in edge is not in the family (CurveSpeed at 35,4).
- This step: (1) undoubled legs placed by their chains (empty chain Tile..Tile between the exit split and the joiner merge); (2) the prio element as a node floor ("prio before" the joiner merge: even_prio + 1 after a straight, + 2 after a bend, from JunctionRunFloor's PrioNode entries) instead of the row-block relations; (3) `sym_routes` reads every row edge's chain (fans and any further tokens), so the hole closes. Doubled legs keep row_pts (R5).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DESIGN-G1.md §16; RECON-row-chain.md §1 (items 5-6), §4; RECON-prio-element.md §3-4.
- src/junction_topology.cpp: the undoubled leg relations and the prio element relations in the row blocks, JunctionRunFloor's PrioNode entries, sym_routes (the row fan part), R3's in/out chain handling, the R2 check.

## Tests (record each red)
1. **Outer**: every existing test and sweep with identical numbers (seeds, sweeps, circuit byte-identical) — the legs and prio element now come from chains and the node floor. Red: removing the row-block leg/prio relations before the chain ones are live breaks placement (record).
2. **`[topology]` "a fan edge may hold more than its fan"** (R3's test): now REQUIRE the checker clean (the family covers the extra S-bend). Red: today's CurveSpeed at 35,4.
3. **Budget:** move the slowest `[topology]` test (measure the five slowest in Debug) to `[topologysweep]` so `[topology]` is under 5 s Debug with margin; say which.
- First of all, run `tools/junction/jt gate build-rel` at the launch HEAD and record the count (R3 and Z2e were merged together; this is the baseline).

## Changes
1. Undoubled leg edges are no longer skipped by refresh / the chain loop; their relations come from the chain (runs from JunctionRunFloor). Remove the undoubled leg's row-block relations; keep aliases only where other code still reads row points (say which).
2. The prio element: a node floor on the joiner merge node of every row (both undoubled and doubled rows: the doubled one's `njm >= lane_merge + even_prio + 2` becomes the same floor with the LaneMerge-PrioNode entry), replacing the row-block relations, reason "prio element" (edge, node, row) as today.
3. `sym_routes` walks the row edges' chains for every token (fans, S-bends, any later token), not the row blocks.
4. TESTS.md rows.

## Do not
- No change to doubled legs' heads/lane pair (R5), programs (R6), the families' rules, facts, levels, render.

## Stop conditions
- Any seed or sweep number changes; test 2 still violating (report); anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` (diff against a launch-HEAD run: identical except timings); circuit byte-identical; `tools/junction/jt gate build-rel` (report before/after counts).
- One commit: `Codechange: Junction legs and the prio element on chains; the curve family reads row chains (R4)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus what was removed or aliased, the moved test, timing. When you have reported, stop.
