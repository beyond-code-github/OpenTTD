# AUDIT of rulings D380..D430 against the goal (2026-11-23, Sonnet draft)

Goal used: D427 (working drawing for every hub, today's compactness, stop chasing V's compactness or geometry, no loosening); D428-D430 (geometry rules found and changed only against the domain suite); the drawing program is the Slots v2 prototype; layers decide by construction (D361, D381: no repair loops, no generate-and-check).
Sources read: DECISIONS D380-D430, STATUS top, HANDOVER-2026-11-22b, CARD-domain-rules, planar CLAUDE.md "Drawing program", planar_draw.h defaults (grep).

Classes: KEEP (serves goal, game fact, settled architecture), RETIRE (chased something D427 dropped, superseded, dead probe), RETEST (geometry or row rule the domain suite should judge), CONFLICT (contradicts D427, D428 or by-construction, or another ruling).

| id | title | class | reason |
|---|---|---|---|
| D380 | Split/merge on diagonal trunk, axis legs | RETEST | Stub of 0 or 1 unit and grid rows are tile-edge geometry; judge on a node-legs configuration. |
| D381 | Slots writes keep-apart rows | KEEP | The by-construction rule itself; but its "no solution inspected" clause is broken by D418 (see CONFLICT). |
| D382 | Tile grid in Slots | RETEST | Grid parity: D423 deferred it, yet the domain is tile geometry; the suite must say which parities are needed. |
| D383 | Flex points (optional bends) | RETEST | Flex is a loosening of the model (insert of 0 = no bend); its legality and curve fit belong to the suite, not hub runs. |
| D384 | Prio exactly its coverage long | KEEP | Game fact about the prio element. |
| D385 | Faces Slots can draw (cells, dummies, bend pairs) | RETIRE | Convex cells and dummies dead (handover: "convex cells ... dead"); superseded by D388, D416-D418. |
| D386 | Shape certifies its refinement (H13) | RETIRE | D388 (b) removed the runtime certificate; cells retired. |
| D387 | Litmus test: 197 representable | KEEP | Representability, not compactness; D427 keeps feasibility on 197. Reading it as "match V" would conflict. |
| D388 | SSS v2 bundle-level | KEEP | Settled architecture (bundles as skeleton); parts on gadgets/cells amended by D401. |
| D389 | 197 pinned post-Structure | KEEP | Answer-key method, still used. |
| D390 | E1 defaults (crossings at nodes, tunnel separators) | KEEP | Game-geometry defaults, settled. |
| D391 | 197's lane mixer, D24 stays | KEEP | Narrow pinned-hub exception; D24 untouched. |
| D392 | 197's doubled crosser left out | KEEP | Scope cut, consistent with D427. |
| D393 | What E1's gap list measures | KEEP | Definition only. |
| D394 | Promise oracle on a written-down hub | KEEP | Oracle scope for pinned hubs; harmless. |
| D395 | Steps on spines are Shape's | RETIRE | Superseded by D397 (no step by choice) and D402/D420. |
| D396 | E1x defaults (197 shape data, H11 runs) | KEEP | Data and capacity rows; H11 runs are a Shape rule, check later if a span suite is built. |
| D397 | Steps two-sided by rule | RETEST | Flex eligibility rule, built from V's 197 steps; judge legality in the suite. |
| D398 | Gadget granularity | RETIRE | Superseded by D400 and D401 (no catalogue); trident part lives on in D400. |
| D399 | Keep D113 (tile between trident and bridge head) | KEEP | Game/signal fact. |
| D400 | Nodes are rules; trident is a length choice | KEEP | Node rule architecture; its legality is what node_lead tests (D429). |
| D401 | No gadget catalogue | KEEP | Settled; matches "rules, not special pieces". |
| D402 | Flex on port and node legs | RETEST | Reason was V's 41 of 50 steps at ports; eligibility to be judged by the suite. Superseded in default by D420. |
| D403 | Node legs exempt from one spine | RETEST | Adopted on V's evidence ("if V builds it"); a loosening of H14, toggle candidate; test as a rule. |
| D404 | No cusps, non-straight leg takes turned heading | RETEST | Rests on the tile fact that the diverging leg is a curve piece; node-legs suite should confirm both leg headings. |
| D405 | Dummy landings independent of lengths | RETIRE | Landings and dummies belong to the retired cell model (D416-D418). |
| D406 | Keep-apart pairwise probe B, kill criterion | RETIRE | Pairwise became the model via D416-D418; the 197 kill criterion is a V chase. |
| D407 | Flex step needs no dummy | RETIRE | Cell clearance by sides; cells retired. |
| D408 | Cell vertices stay on their side of dummies | RETIRE | Same, cell-only rule. |
| D409 | K-E5 kill criterion, elastic mode | RETIRE | K-E5 on 197 superseded by D427; the elastic mode remains a diagnosis tool, not a rule. |
| D410 | Port lane positions along the box side | KEEP | Input, optional, no model change; corpus authority. |
| D411 | Ignore V's prio and pathfinder traps | KEEP | Extractor scope; consistent with D427. |
| D412 | Port lane may have length 0 before first node | RETEST | Zero-length pieces are where keep-apart leaked (D429 FINDING: `port_stretch` broader than the rule); judge on the port/node configuration. |
| D413 | Flex on every piece (NOT ADOPTED) | RETIRE | Never adopted, reverted. |
| D414 | Crosser may cross at its piece end | RETEST | A zero-length crossing piece makes the track cross at another angle over two tiles (cross_piece probe, STATUS step 8-10); suite says "other shape" excuses only rejections. Corpus-only evidence. |
| D415 | Deck gap 2 parallel / 1 otherwise | RETEST | Deck/crosser spacing rule from V's counts; deck family not yet judged by the suite (STATUS: span/diamond groups unowned). |
| D416 | K-E5 hybrid keep-apart (cells + pairwise) | RETIRE | Superseded by D417 and D418. |
| D417 | Pairwise directions: monotone-walk probe | RETIRE | Gate failed; superseded by D418. (Its monotone-walk rule was by construction; D418 replaced it with drawing-read sides.) |
| D418 | D381 amended: directions from earlier drawings | CONFLICT | Sides read from up to 6 earlier drawings is generate-and-check: breaks D381/D361, and D426 says "never read from a drawing". Engineer: "why do we need a repair?" |
| D419 | E5 oracle = LP, integers later in Slots | CONFLICT | Verdict is an off-grid LP, not a tile drawing, against D427 (working drawing) and D428 (tile-rule truth); also inherits D418's drawing-read sides. |
| D420 | Flex C default, D414 for bridges, bays first | CONFLICT | Flex C adopted because D402 misses 8 of V's 50 steps: chases V's geometry (D427) and widens optional bends (no loosening). Bays and D414-for-bridges parts stand (retest). |
| D421 | Corridor family (beside track = offset curve) | RETEST | By-construction in spirit and consistent with D426; opening/closing steps and miter are geometry for the copy/prio configuration of the suite. |
| D422 | Optional flex meets curve spacing: ratchet pipeline | CONFLICT | Discovery/commit/prune rounds are a repair loop (D381, D361); defaults "to tune against V (~50 steps, 101x124)" is D427's dropped goal. Integer-final clause already amended by D423. |
| D423 | Whole-tile integrality leaves E5 pipeline | CONFLICT | The pipeline's verdict is an off-grid LP ("no gate"), yet D427 wants a working drawing and the domain suite (D428) judges tile geometry; also D423 restores D419. Grid is needed for any real drawing. |
| D424 | Node gaps on by default | RETEST | Node-to-node leg 0 or >= 2 half tiles, measured on V; legality belongs to the node-legs suite (D400 trident). |
| D425 | Curve-aware discovery on by default | CONFLICT | Extra discovery rounds with damped memory = more of D422's loop; evidence is V's step counts. Keep only if reframed as by-construction curve rows. |
| D426 | Bundle-internal faces rigid by rule | KEEP | The by-construction answer to side choice (sides from the map and headings, never a drawing); code default off (see flags). |
| D427 | Working drawing first | KEEP | The goal. |
| D428 | Domain suite; rules changed only against it | KEEP | The goal. |
| D429 | Node-leg rule A (node_lead) | KEEP | Found through the suite; default pending, step "probes as defaults". |
| D430 | Rule A narrowed | KEEP | Same, from the suite's 18 too-strict cases. |

## CONFLICT rows
- D418: sides read from earlier drawings (up to ~6 rounds); breaks D381/D361 and D426's "never read from a drawing". Two-ruling clash D418 vs D426.
- D419: LP-only, off-grid verdict; conflicts D427, D428, and carries D418.
- D420: Flex C chosen to cover V's steps; D427 (stop chasing V) and no loosening.
- D422: ratchet rounds and prune = repair loop; tuned against V.
- D423: "integer final is no gate" leaves the pipeline with no tile drawing; conflicts D427 and the domain suite's premise.
- D425: more discovery rounds on V's step evidence; same family as D422.

## RETIRE rows
- D385, D386, D405, D407, D408: convex cells, dummies, landings, certificate (cells dead; D388 (b), D416-D418).
- D395: superseded by D397, D402, D420.
- D398: superseded by D400, D401.
- D406, D409: pairwise probe and K-E5 kill criterion on 197, overtaken by D416-D418 and D427 (elastic mode stays as a tool).
- D413: not adopted.
- D416, D417: superseded by D418 (and D418 itself is in CONFLICT).

## RETEST rows by family
- Flex and steps: D383, D397, D402, D403, D420 (flex-C default), D422 (price defaults), D425.
- Node legs, leads and gaps: D380, D404, D412, D424 (D429, D430 are the suite's own rules).
- Crossing, deck and crosser rules: D414, D415, D396 (H11 runs).
- Grid parity: D382 (D423).
- Corridor, copies and prio rows: D421, D426 (sibling lanes).
- Bays and curve spacing (inside D420, D422): bays on by default since D420 are not suite-judged; add a bay/curve configuration.

## Code defaults versus rulings (planar_draw.h, CLAUDE.md)
- D426 is adopted but `sibling_lanes` is off by default in both PlanarDrawOptions and the pipeline (a probe); the ruling says sibling lanes are rigid by rule. Either flip it or amend D426.
- D421: `corridor` is false in PlanarDrawOptions, true in the pipeline; matches the ruling's "option, on in the pipeline".
- D429/D430: `node_lead` false; STATUS NEXT (2) plans the default flip. `end_node` and `cross_piece` (D428) likewise off.
- D422 text says the integer final is part of the pipeline; D423 amends it and `final_grid` is false: consistent with D423, not D422.
- D424 `node_gaps` true (pipeline), false in a single draw: as ruled.
- D420 flex C: `PlacePlanarShapeFlex(..., end_pieces = true)`: as ruled.
- D427's probes `strict_span`, `strict_zero`, `crossing_flanks` still off: as D428 says. Note strict_zero is "not needed and never works" (STATUS), so it can be retired as code.
- D382 grid: off by default (D423); every default drawing is off the game grid.
