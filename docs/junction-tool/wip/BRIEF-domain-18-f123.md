# BRIEF domain-18: fix F1-F3 (the lead rule's heading, D412's reach, turns in contracted lines)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. One Release run per configuration (X1, X2, M1, K1) with `span+lead+endnode+crosspiece+grid`. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- STATUS findings (domain-17b reports in docs/junction-tool/wip/probes/domain/, `*_grid.txt`):
  - **F1, too strict (M1, `[lead]`):** a merge leg arriving on an S-bend toward its sibling with no flank forms a Y, which is legal because its heading differs from the sibling's. At X1's split, the toward step put B1 on D1's heading (overlap). The lead rule's real condition: the step's heading equals the sibling leg's heading at the node.
  - **F2, too loose (X1 78, K1 168, `[grid | node | off grid: ...]`):** the grid rows do not reach node-to-out-port lines (X1 C1; K1 A1, C1). Cause: `port_stretch` (planar_draw.cpp, near line 386, D412) also covers node-to-OUT-port lines, so their pieces may be 0 and their runs are never seeded. D412's ruling covers only an in-port line before its first node (DECISIONS.md D412; `PlanarNodeLeadRows` already uses the ruling's form).
  - **F3, too loose (M1 6, `[turn]`):** a contracted line can hold two consecutive pieces more than one eighth apart (NE then SE, their flank removed at length 0). Under D432 a contracted line keeps the flank between two used inserts: consecutive contracted pieces are at most one eighth apart.

## 2. Changes
1. **F1** (planar_draw.cpp `PlanarNodeLeadRows`, flex and steps paths): a toward step gets its row only when the insert's heading (mod 8) equals the sibling leg's heading at the node (mod 8): the sibling's first piece heading at a split, its last at a merge; with several siblings (a trident), any of them. Keep `sib == 0` (fork, no site) as now. Doc comment and the CLAUDE.md lead bullet: "binds when the S-bend's heading at the node equals a sibling leg's heading there (it would run over it)".
2. **F2** (planar_draw.cpp, `port_stretch`): only an in-port line (`from.port && !to.port`) with no event before its first node, as D412 says; node-to-out-port lines keep their least lengths. Comment cites D412's wording. This changes drawings of real hubs: run the gate; any previously passing test that now fails is a stop (report it).
3. **F3** (harness `ContractedVariant`): a writing whose consecutive contracted pieces differ by more than one eighth (lifted headings) is not a writing Slots can hold; if no writing remains, the polyline is "rows reject". Use the flank-first search: try the next representation (cap 50) before giving up. Report how many polylines this affected.
   - Record in the report header, and add to STATUS under Slots items: "Slots keeps the flank between two used S-bend inserts (consecutive contracted pieces at most one eighth apart)".

## 3. Tests (fast)
- F1: M1's too-strict example (A1.0=6 A1.1=2 A1.2=2 A1.3=2 A1.4=0) has no failing lead row; X1 S1's overlap example still fails `lead B1 start 0`.
- F2: X1's first F2 too-loose example breaks a grid row (an `integer` break in the program check) after the change; record red.
- F3: M1's turn example (A1.0=10 A1.1=1 A1.2=0 A1.3=1 A1.4=0) is not accepted by the rows.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (production change).
- Release once each: X1, X2, M1, K1, `span+lead+endnode+crosspiece+grid` (overwrite).
- One commit: `Fix: Junction: lead rows bind on the sibling's heading, D412 only for in-port lines, contracted lines turn one eighth at a time (D428, D432)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; red evidence per fix; per configuration the classes with group keys, counts and first examples; gate result; state. When you have reported, stop.
