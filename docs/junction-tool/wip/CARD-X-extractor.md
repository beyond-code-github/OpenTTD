# CARD X: the hub extractor, save -> the solver's files (orchestrator, 2026-11-21)

Goal (Pete, 2026-11-21): a repeatable extractor as the first step to cataloguing every hub ever built (facts, measurements, throughput, author), style profiles (the goals and toggles that reproduce an MFB or V453000 hub) and a timeline of gadgets and styles. Input: wip/RECON-extractor.md (how PSG 197 was made: 14 stages, mostly by hand). Formats: authored topology (D389), structure.txt (D389), Shape projection + steps (D396). Nothing below Shape until E3/E5.

## Principle
Every manual stage of 197 becomes a rule, or a named human checkpoint recorded in the hub's data (never a silent choice). PSG 197's hand-made files are the acceptance test: the extractor must reproduce them up to line naming, and every difference is explained (an extractor error, or a hand error found).

## Stages (Python under tools/junction/corpus/, `python3 -I`; validation through the C++ loaders)
| # | Stage | Rule that replaces 197's hand work | Checkpoint |
|---|---|---|---|
| X0 | Hub list | `tools/junction/corpus/hubs.tsv`: id, game, hub name, author, style, box, source (Junctionary BBH list, coop wiki) | **Pete:** confirms each hub's box once (from the wiki picture or the game) |
| X1 | Export | `map_export` on the box, headless (exists) | none |
| X2 | Track graph | pieces, tunnels and bridges (heads with far ends), signals; direction: signal pieces as signalled; every free piece oriented by the routes (the direction in which it is reachable from an in lane and reaches an out lane, iterated to a fixpoint; probe x1c: 3 rounds, 0 ambiguous on 197; the save's own inference disagrees on 198 pieces and must not be used) | ambiguous pieces listed by tile |
| X3 | Ports | lanes crossing the box edge, side and reading order (planar CLAUDE "Topology") | none |
| X4 | Routes, lines, nodes | a **node** is a switch (a piece with two or more successors or predecessors); a **line** is the run of pieces between nodes; each line's route set (reachable from the lane and reaching the port) labels it, and a split is a fray when its legs' sets differ, a lane split when equal (probe x1c on 197: 62 of 62 hand nodes matched, 89 of 100 lines one-to-one; extras are the hand file's dropped D1+D2 pair ladder and D3 x D3); a three-way split on one tile is written as a cascade in the geometry's leg order (D398's trident); line names generated from routes | short parallel pairs and other unrepresentable bits listed as gaps (D393) |
| X5 | Crossings | line pairs whose geometry crosses (a tunnel under ground track included), order along each line from the geometry, marks by marks.py's rule (D389) | parity and face-map failures stop the hub |
| X6 | Structure | tunnels and bridges from the export: separator = the line through it, crossers = lines crossing its span; groups = adjacent parallel tunnels; straight leg = the leg whose first piece keeps the trunk's heading | undetermined straight legs listed |
| X7 | Shape | per line its headings and crossing pieces from the pieces; split into projection + steps (split_shape.py) | none |
| X8 | Validation | `ParsePlanarTopology` (parity), face map (catches mirrored leg orders), `LoadPlanarStructureVariant` (gaps, differs), `LoadPlanarShapeVariant` + `CheckPlanarShapePromise`, run by a hidden test on a hub directory | the report is part of the hub's data |
| X9 | Record | metadata (game, name, author, date where known); metrics (box, tiles, track pieces, bridges, tunnels, signals, turning, crossing-angle mix); decisions vs our defaults (the `differs:` lines); gadget tally once the catalogue exists | none |

Data: the first hubs pinned in the repo (tools/junction/topologies/<hub>/) for tests; the bulk corpus on OneDrive (S/corpus/<hub>/).

## Steps (each a brief, test-first, own worktree from feature: ng2)
1. X0-X4 on 197: topology without crossings; acceptance: lines, nodes and ports isomorphic to psg197.txt's.
2. X5: 197's 122 crossings, orders and marks equal psg197.txt's (up to names).
3. X6: equal psg197.structure.txt (with D392, D396's D12g, tunnel #49 named as a gap).
4. X7: equal psg197.shape.txt and psg197.steps.txt.
5. X8-X9 and the next four hubs.

## Decision for Pete: the first hubs after 197
- Proposed (one per builder or style, all in the Junctionary BBH list): PSG 227 "Simple" LLRR (mfb), PSG 248 Furball (V453000), PSG 186 Brainmelter, PSG 188 "Bridge split" (Vitus).
- Reversible: yes.

## Known limits
- Throughput: no structured source (no vehicles in the export, no counts in the manifest); a later step (vehicle export or the wiki's figures).
- Hub location: census v2 boxes are candidates only (43 of 1418 classify); boxes stay a checkpoint until a finder is trusted.
