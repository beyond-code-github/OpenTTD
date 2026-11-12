# RC16 (read-only recon): the per-row and per-kind code that keeps DESIGN-G1 §16.1 unmet

Read-only: change no file except the output. Keep your context small: Grep with scope, Read with offset/limit, each file once. Mark anything not read in code as INFERRED. Use the clangd LSP if available for call graphs.

## State
- Main repo /home/pete/OpenTTD at feature/junction-tool (read there; do not build). Never touch /home/pete/OpenTTD/.claude/nodes or .claude/nodes2 or any game.
- DESIGN-G1 §16.1 says "placement has no per-shape row code": every edge is an Elem chain (R1-R6 did that), and rows come from the stack by general relations. The review (docs/junction-tool/REVIEW-opus-since-terrain-placement.md, Q1 (d) and Q3) found it is not met: per-row `in_edge[r]`/`out_edge[r]`/`leg_edge[r]`, `lane_item`/`north_item`, `doubled_row(r)` drive the Y relations (junction_topology.cpp ~:2181-2186) and the doubled-leg block (~:2240-2262). R7b (Shift edit) failed on it: a stack item inserted between a Lane item and its Approach item contradicts because the joiner's arrival S-bend is fixed at exactly one row (`eq(1, last, first, VY)`) with its Corner on the Approach item; and the prio element is drawn by the signal pass beside the merge tile (~:3495-3567), not placed on the Prio item's row, so a Prio item can be left empty (rule 10).

## Read first
DESIGN-G1.md §10 (programs), §11 (placement), §14.2 (per-state choices), §16 (general row chain, R1-R8); src/junction_topology.h (stack item kinds, programs, Elem); src/junction_topology.cpp: StructureJunctionTrackGraph (stack building), the Y phase of PlaceJunctionTrackGraph, the Elem chain building, the signal pass's prio element.

## Questions
1. Inventory: every place where placement or signal code branches on a stack item kind (Lane, Copy, Approach, Prio, PrioAndCopy, ...), on a row role (in/out/leg/lane/north/doubled), or on a token at a specific position, rather than reading a generic chain/relation. For each: file:line, what it decides, and which graph or chain element it stands in for.
2. Approach items: what an Approach item is in the graph (which edge, which tokens), why its S-bend is exactly one row, and what general relation would let it span k >= 1 rows (the fan/S-bend relations elsewhere already size by rows: cite them).
3. The prio element: what graph element (if any) it belongs to; where its tiles are decided; what it would take for it to be an Elem on the Prio item's row placed by the general machinery (occupancy, facts, levels already treat its tiles: cite C5b/H3/Z2f code).
4. For each inventory item, the general replacement in one line, and the order in which they can be removed with seeds unchanged (identity tests in the sweeps). Flag any that would change a seed (INFERRED with reason).

## Output
docs/junction-tool/RECON-per-row-code.md (untracked, <= 100 lines).

## Report
10 lines: inventory count by kind, the Approach and prio findings, the proposed removal order, and which steps change seeds. When you have reported, stop.
