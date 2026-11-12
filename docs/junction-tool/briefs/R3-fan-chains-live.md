# R3: in and out edges placed by their chains (Fan elements live), their row_pts relations removed (DESIGN-G1 §16 R3; RECON-row-chain §4 step 3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-r3 <HEAD>` (given at launch). Never use git stash. Another agent may work in .claude/nodes2: never touch it.
- R2 merged: every row edge has an Elem chain (in: [Tile(port), Fan, Tile(node)]; out: [Tile(node), Fan, Tile(port)]; legs as chains; doubled copies with HeadsPair), tied by `eq` to row_pts and checked equal on every solve (`row_chain_mismatches`). Row edges are skipped by `refresh` and the chain-relations loop.
- This step makes the in and out edges' chains carry their own relations (the generic chain-relations loop with the R1 floor table and the Fan kind's shape relation), and removes the in/out fan relations from the row blocks (fif/fil/fof/fol and their runs). Legs and doubled legs keep row_pts (R4, R5).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DESIGN-G1.md §16; RECON-row-chain.md §1 items 4-5, §4.
- src/junction_topology.cpp: the row blocks (in fan, out fan, out port run), the chain-relations loop, refresh, the R2 row chains and their check, fan_item, best_offset/set_port_rows, the D215 objective's kind-1 handling, the occupancy and facts enumerations for fans.

## Tests (record each red)
1. **Outer:** the R2 check (`row_chain_mismatches` empty) and every existing test and sweep, unchanged numbers, on T, LL_RR and the 44 cells: the fans are now placed by their chains and must land exactly where row_pts put them. Red: with the row-block fan relations removed and the chain relations not yet live, the placement differs (record what moves).
2. **`[topology]` "topology: a fan edge may hold more than its fan"**: a program for an in edge of [Fan, SBend] (built in the test by editing the T's programs: an extra SBend after the fan on one in edge, hand toward the node row) places without "not ruled yet", the R2 check still passes for the other rows, and the checker is clean or names only that edge (report which). Red: fan_item's "exactly one token" condition refuses it.
- Also: move the slowest `[topoproj]` test (measure the five slowest in Debug) to `[topoprojsweep]` so `[topoproj]` is back under 5 s; say which.

## Changes
1. In and out edges are no longer skipped by `refresh` and the chain-relations loop: their Fan elements get the shape relation (size m from the port row, as fan_m today: exact `m - 1` across, first at the port row, last at the item row), and their runs come from JunctionRunFloor (Port-Fan, Fan-Node, Node-Fan, Fan-Port, PrioNode-Fan as R1 names them).
2. Remove from the row blocks: the in fan's `fif`/`fil` relations and runs, the out fan's `fof`/`fol` relations and runs, and their `eq` R2 ties (the chain points are now the fan points; the row_pts fan members become aliases of the chain points or are removed — remove them if nothing else reads them, else alias, say which).
3. `fan_item` no longer requires the program to be exactly [Fan]: the fan is the program's first token on an in edge and last on an out edge when it is a Fan; best_offset reads it the same way.
4. The D215 objective's kind-1 entries read the chain's Fan element.
5. TESTS.md rows.

## Do not
- No change to legs, doubled legs, the prio relations, the families, facts, the levels solve, programs or the render.

## Stop conditions
- Any seed or sweep number changes; a fan feature the generic chain relations cannot express (report it); anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` (diff their output lines against a HEAD run: identical except timings); circuit byte-identical; `tools/junction/jt gate build-rel` (447 before; report the count).
- One commit: `Codechange: Junction in and out edges placed by their chains (R3)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus what was removed or aliased, test 2's outcome, the moved `[topoproj]` test, timing. When you have reported, stop.
