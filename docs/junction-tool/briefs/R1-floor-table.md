# R1: one floor table for every run constant in placement (DESIGN-G1 §16.1-§16.3 R1; D217; RECON-row-chain §1.5, §4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-r1 <HEAD>` (given at launch). Never use git stash.
- DESIGN-G1 §16 (read it): every edge becomes one Elem chain; first, every run constant moves into one floor table keyed by neighbouring element kinds, each entry citing its rule. This step is a pure refactor: seeds identical.
- RECON-row-chain.md (read it whole) lists the constants: the chain run 2 between elements (`chain run`), in-fan runs (`fif >= pin + 2`, `nex >= fil + 2`), out-fan runs (`fof >= njm + 2`, `pout = fol + 2` exact), the prio element floors (`njm >= nex + even_prio + 1`; doubled `njm >= lane_merge + even_prio + 2`), the doubled leg's lane split/merge and head floors (2, 1), STUB, the D145 floors, the waiting room (tl + 1).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DESIGN-G1.md §16; RECON-row-chain.md; DECISIONS.md D145, D206, D217.
- src/junction_topology.cpp the placement helper: every `along`/`ge`/`eq` whose constant is not derived from a shape (span, m, h) — the recon's list.

## Tests (record each red)
1. **`[topology]` "topology: every run floor comes from the floor table"**: the table (Changes 1) lists each entry with a non-empty rule citation; at the test spec on T and LL_RR, every non-shape lower-bound relation placement adds has a reason whose rule names a table entry (expose nothing new: assert through `why`-style reasons on a forced failure per entry is NOT required; instead the test calls the table's lookup for each (kind, kind) pair the recon lists and checks the values equal today's constants). Red: the table does not exist (compile) — record as the red.
- Every existing test stays green unedited; SamePlacement over all 44 `[topologysweep]` cells against stored columns (the T1c-1 golden table) holds; circuit file byte-identical.

## Changes
1. In junction_topology.cpp (file-static): `enum class FloorKind : uint8_t { Port, Fan, Tile, Corner, SBend, Heads, Node, PrioNode, LaneSplit, LaneMerge };` and `static int JunctionRunFloor(FloorKind before, FloorKind after, const JunctionSpec &spec, int even_prio)` returning the least run in tiles between the two, with a comment per case citing its rule (decision id or DESIGN section); unknown pairs return 2 (the chain run, D217 comment). Every constant the recon lists that is a run floor between two elements is replaced by a call naming its two kinds. Shape constants (spans, m - 1, the head stagger 1, exact port alignment) stay where they are, each with a one-line comment saying why it is a shape, not a floor.
2. The reason names stay as they are.

## Do not
- No Elem kinds, no row chains, no relation changes, no new families. Values must not change.

## Stop conditions
- Any seed changes; a constant you cannot classify as floor or shape (list it); anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"` (to files); circuit regeneration (`JUNCTION_CIRCUIT=$PWD/tools/ottd/layouts/circuit-llrr-TL4.json build-rel/openttd_test "[.circuitllrr]"`, must be byte-identical); `tools/junction/jt gate build-rel` (report the count).
- One commit: `Codechange: Junction placement run floors from one table (D217)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the table (pairs, values, citations) and the constants left as shapes. When you have reported, stop.
