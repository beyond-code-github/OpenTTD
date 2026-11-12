# CS1: curve gap from an in fan to its exit split (G3; DESIGN-G1 §11.8 "Curve gaps")

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-cs1 <HEAD>` (given at launch). You are the only writer there. Never use git stash (shared stack).
- After ST2 the LL_RR seed (render + signal tokens + prio elements) has one checker violation: rule 3 at (42,8), "curve speed limit 111 below 160". Measured (orchestrator diag): the w exit route (lane path 6) has an in-fan S-bend of 2 half pieces at (44,7)/(44,8) (curve c0 left, then c1 right), one axis piece (43,8), then the exit split's diverging curve c2 (left) at (42,8). JunctionCurveGaps (junction_chain.cpp:25) wants c0 -> c2 (two curves apart) >= min(2 TL - 1, 2 CL) = min(7, 10) = 7 units; it has 2 + 2 = 4. Placement's in chain only asks exit split >= fan end + 2 tiles (junction_topology.cpp, `along(heading, nex, rp.fil, 2, false)` after the "In: port, fan, exit split." block).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]` row).
- src/junction_chain.cpp :20-45 (JunctionCurveGaps).
- src/junction_topology.cpp: :965-985 (cl, h, k, prio constants), the X-phase in chain (grep `/* In: port, fan, exit split. */`, ~25 lines), and where fan_m is computed (grep `fan_m[e] =`). Find how the exit leg of row r is known there (in_edge, leg_edge, the exit edge used by the exit Corner); read only what you need.
- src/junction_topology.h JunctionTrackStep (hand convention: +1 right, -1 left, as JunctionTrackStructure::turns).
- src/tests/junction_topology.cpp: the placement and ST1/ST2 tests (from "the T's placement" to the end).

## Design (settled)
A fan of m = m_in > 0 rows is an S-bend of 2m half pieces (2m units) between its first curve c0 and its closing curve c1; d = the along-distance in tiles from the fan's last tile (fil) to the exit split (nex), so the d - 1 axis pieces between give 2(d - 1) units from c1 to the split's curve c2. With G1 = min(2 TL - 1, max(2, cl)) (consecutive, same hand) and G2 = min(2 TL - 1, 2 cl) (two curves apart), using placement's `cl`:
- c0 -> c2 >= G2: d >= 1 + ceil((G2 - 2m) / 2);
- c1 -> c2 >= 2 when c1 and c2 have opposite hands, >= G1 when the same: d >= 1 + ceil(G1 / 2) in the same-hand case;
- and d >= 2 as today.
Replace the constant 2 in `along(heading, nex, rp.fil, 2, false)` by `d_min` = the largest of these, computed per row r when m_in > 0 (m_in == 0: keep 2). Hands: the split's curve c2 has the hand of the exit path's first turn (the first token of the exit leg's program, a Corner; take its `hand`; if it is not a Corner, STOP and report the row and token); the fan's first curve c0 turns toward the stack row (hand +1 when the stack row lies to the right of the heading, -1 when to the left; right of heading h is heading h + 1 mod 4 in the 0 N, 1 E, 2 S, 3 W numbering), and c1 has the opposite hand. Add a two-line comment citing JunctionCurveGaps and this brief's derivation. No other constraint changes; the out side is not changed (no violation there; a finding if the new test shows one).

## Tests (tag `[topology]`; record each red)
1. **Outer: "topology: the LL_RR seed is clean"**: the SignalledOf pipeline on LL_RR (render, signal tokens, prio elements); `CheckJunctionRules(layout, spec, JunctionLLRRSidelineTopology())` is empty (INFO each violation). Red today: the one rule 3 violation at (42,8) (record it).
2. **Placement tests:** the T must not change (if any T number changes, STOP). LL_RR's pinned numbers that move (e.g. size.x in "LL_RR series bridges", any pinned x in "LL_RR with an L_R sideline: placement"): update them to the new solve and report each old -> new; if any y value or size.y changes, or X_e1 - X_e2 changes, STOP.
3. The ST2 hidden LL_RR findings test may stay as is.
- Every other existing test stays green and unedited.

## Do not
- Do not add curve gaps anywhere else, change cl/h/k, the render, the signal tokens or the checker.

## Stop conditions
- Test 1 still has violations after the change: report them, RenderJunctionAscii to a file you name, and the route's pieces around each tile.
- The T changes; a y/size.y/X_e1 - X_e2 change in LL_RR; anything not covered.

## Gates
- `tools/junction/jt fast .claude/nodes/build '[topology]' 5`; `tools/junction/jt gate .claude/nodes/build-rel` green (387 before; report the count).
- One commit: `Fix: Junction seed curve gap from an in fan to its exit split (G3)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "CS1" and the new test count.

## Report
The standard report, plus d_min per row of LL_RR, every changed number old -> new, and the new LL_RR box size. When you have reported, stop.
