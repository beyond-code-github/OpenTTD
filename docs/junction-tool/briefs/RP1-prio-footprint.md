# RP1: the prio footprint as tiles relative to the merge (read-only recon for the prio-row template rule; D193, AUDIT item 2)

You do read-only research and write one file. Do not change code, build, or commit. Keep your context small: grep, then read with offset/limit. Mark anything reasoned rather than read as INFERRED.

## Goal
The node template must reserve what the signal pass's prio needs, so the pass never fails ("missing prio") and the checker's Prio/PrioReach always pass (D193). State that need as a footprint: a set of tiles (and edge-contact conditions) relative to the merge tile, the ML's direction and the prio side, with p = prio_tiles.

## Read (once each)
`src/junction_signal_pass.cpp`: PlacePrio (~:306) and everything it calls to choose the side, the row and the tiles it lays (grep, offset/limit); `src/junction_rules.cpp`: CheckPrio (:1054-1363) and CheckPrioReach (:1535-1630) — only what they require of the prio's tiles and their neighbours; `docs/junction-tool/AUDIT-rule-coverage.md` rows Prio, PrioReach; `docs/junction-tool/DESIGN-N3.md` §7 (occupancy chain: elements per row); DECISIONS rows D75, D87, D107, D108, D110, D111, D116, D148, D148a (grep by id); the flat T dump `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n3a/t2-dump.txt` section (a) and `docs/junction-tool/RECON-turn-programs.md` §1 (where the two prios are in today's T: A's on row 0, x 24..38; the W line's merge at (6,4)).

## Write `docs/junction-tool/RECON-prio-footprint.md` (under 100 lines)
1. How PlacePrio picks the side and the row, and which tiles it lays (with file:line), and when it returns nothing.
2. The footprint, per prio merge: the tiles (row, x range as a function of the merge x, d and p) that must be free of other tracks, the tiles where the joiner itself may run (is the joiner part of the prio? today's W prio vs N->W's run on row 5), and the edge-contact conditions (which neighbours must not touch it).
3. For today's T: both prio merges, their footprint tiles listed, and what currently sits next to them.
4. How the footprint maps onto DESIGN-N3 §7's occupancy chain (reserved elements on a row with an owner set), or why it cannot.
5. Numbered open questions.

## Report
File path, line count, the footprint formula, open questions. When you have reported, stop.
