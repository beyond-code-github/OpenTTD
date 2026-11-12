# T1d: the D215 objective: every row change pushed toward the feature it serves, after the least solution (DESIGN-G1 §14.5; D212, D215, D216, D217)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-t1d <HEAD>` (given at launch). Never use git stash.
- D215 (DECISIONS.md): every row change sits next to the feature it serves; the free run lies on its other side. §14.5 / D216: this is an objective, not an exact relation: after the least solution, with the other points fixed, each row-change token is pushed toward its feature as far as the relations and facts allow, in a fixed order. Today two exact relations stand in for it (the doubled leg's lane split exactly 2 before its first near head; the out port exactly 2 after the out fan), and T1c-1 made k_out a lower bound, so on LL_RR the joiner merges now sit 4-8 columns before their out fans (the tests marked `// D215 objective restores (T1d)`).
- The least solution puts every point at its smallest column. A token whose feature lies toward smaller columns is therefore already next to it; only tokens whose feature lies toward larger columns move.
- P3b's lean kinds (briefs/P3b-every-row-change-leans.md, "The rule"): kind 1 a Fan on a route's first edge leans +travel (toward its node), on its last edge -travel; kind 2 a copies SBend (lane split) +travel; kind 3 the lane merge SBend of copy 0 -travel; kind 4 any other SBend by its neighbours. The same kinds apply here, read from programs and the graph (tokens, not routes).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DECISIONS.md D212, D215, D216, D217; DESIGN-G1.md §14.5; briefs/P3b-every-row-change-leans.md (the rule and Amendment 1).
- src/junction_topology.cpp the placement helper: the row chains (in fan, lane split ~:1535, out fan ~:1610-1620), the chains of other edges (SBend elements), `solve`, the facts block (the objective runs after it); src/junction_positions.h (Mark, Undo, RestrictRange, Up).
- src/tests: every line marked `D215 objective restores (T1d)`; "topology: a doubled leg's lane split sits next to its bridge"; P3/P3b's `[topoproj]` lean tests (what they measure).

## Tests (record each red)
1. **Outer `[topology]` "topology: every row change sits next to its feature" (D215)**: T and LL_RR at the test spec and every `[topologysweep]` cell: for every row-change token of kinds 1-4 with a lean direction, the run between it and its feature (its node, or for kind 2 the first near head, kind 3 the last far head) is the rule minimum (the relation's k), unless a relation or the box bound stops it (then INFO which reason). Red: today's LL_RR out fans (k 11 vs 5) and in fans next to their ports.
2. **Every line marked `// D215 objective restores (T1d)`** returns to its value before T1c-1 (the marker says the old value; remove the marker). If one does not, STOP and report it with the reason chain of the point that stops it.
3. **`[topoprojsweep]` "topoproj: the objective gives way to a house"**: LL_RR, a house on the run between an in fan and its exit split (the middle tile of that run at the flat solve): the facts solve succeeds with the fan before the house, checker clean.
- `[topologysweep]`, `[topologystretch]`, `[placeoracle]` (heights 0, violations 0; solved count reported), `[realterrainsweep]` if merged: green. The box size of every sweep cell must not change.

## Changes
1. **Relations:** the lane split's `along(heading, rp.nh0[0], rp.lane_split, 2, true)` and the out port's `along(heading, pout, rp.fol, 2, true)` become lower bounds (`false`). Shape relations stay exact (fan width m - 1, the S-bend's own span, the head stagger, nh1 one after sb1_last).
2. **The objective (after the facts, in the X phase, both overloads):** the tokens of kinds 1-4 in a fixed order (edges ascending, then tokens ascending, copies after edges). For each token whose feature lies toward larger columns: take Mark; RestrictRange every point not in the token's rigid group (the points tied to the token's anchor by exact relations: compute the group once by union over `eq` pairs) to its current Lo; read Up of the anchor; Undo; then RestrictRange(anchor, that Up, that Up) with reason "next to feature" (edge, row) and propagate. Tokens whose feature lies toward smaller columns are left at Lo. Y is untouched.
3. Remove the markers and restore the marked test values (test 2). Regenerate the circuit file (commit it; say what moved).

## Do not
- No new relations or facts; no change to the families, the rounds, the checker or the node path; do not touch P3/P3b's node-path leans.

## Stop conditions
- A marked value that does not return; a box size change in any cell; a solved placement with a violation in any sweep; an existing test fails other than the marked lines and pins the objective is meant to move (an in-fan's column: update those pins only if the move is toward the fan's node and the checker is clean, listing old/new); anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[topoprojsweep]"`, `"[placeoracle]"`, `"[realterrainsweep]"` if present (to files); `tools/junction/jt gate build-rel` (report the count).
- One commit: `Change: Junction placement pushes every row change toward its feature after the least solution (D215 objective)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus: per kind, how many tokens moved and by how much (LL_RR test spec); the restored values; pins moved (old/new); sweep solved counts; mean Release micros per facts placement before/after. When you have reported, stop.
