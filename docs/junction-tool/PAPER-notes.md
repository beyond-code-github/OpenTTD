# PAPER notes: claims and evidence (no prose)

A running list of claims that could go in a paper, each with its evidence: commit, numbers and where the data lives. A claim is added when a result lands, in the same commit as the card or STATUS update. Open claims stay marked open until measured. Archaeology items (from before 2026-11-22) carry a pointer to dig from.

## Working title
Drawing expert rail hubs: a layered octilinear layout model checked against expert builds.

## Prior art to cite
- Octilinear metro maps as a MIP: Nöllenburg & Wolff.
- Topology-shape-metrics: Tamassia; our Structure -> Shape -> Slots echoes it.
- Compaction:
  - Klau & Mutzel, "Optimal compaction of orthogonal grid drawings";
  - Eiglsperger & Kaufmann, fast compaction with visibility representations (constraint-graph compaction).

- Hardness and tractability (verify): Patrignani 2001 (orthogonal compaction NP-complete); Bridgeman et al. 2000 (turn-regular faces: polynomial). No octilinear analogue known (per Gemini, verify).

## Claims

### C1. Expert builds as an answer key find over-strict rules (method)
- **Claim:** pinning an expert's real build (track ends and crossing points within a window, with one common shift) to the constraint model, then solving an elastic LP (one slack per row, cost 1), names the rule families that are stricter than the expert. Fixing each family generically drives the slack to 0.
- **Evidence (PSG 197, 324 pins, 2026-11-22):**
  - Base rows with pins at window 8 (half tiles): LP slack 0.5 on one row, crossing L40 x L57. The crosser bends at the crossing, over a tunnel. -> D414 (49b9751f39) -> slack 0; the integer program is feasible in 3.2 s.
  - Window 4: slack 10 on deck rows. The expert puts axis and diagonal crossers half a tile apart on a tunnel deck (26 of 26 such pairs; parallel pairs are always >= 1 tile). -> D415 (0130ebab2a) -> slack 0.
  - Window 2: slack 26 -> 14.
    - The rest is port gaps, now fixed in the extractor (968bc1072f): the expert's port lanes are spaced unevenly.
    - Also tunnel end runs: the expert tunnels through slopes with no ramp tile. That is a terrain difference, not a rule error.
  - Extractor artefacts found the same way:
    - crossings listed on a tunnel portal tile (ad1dd2e94f): tunnels match the hand model 50 of 50, was 46;
    - port positions (968bc1072f).
- **Side lesson:** the "integer infeasibility" that first appeared was an LP one (all six variable groups made continuous: still infeasible). The elastic LP named it at once.
  - Probes: 5c265a9e53 (variable groups), 48f7cb8e4f (base-LP conflict).
- **Data:** wip/CARD-E5-flexstep.md, last bullets; scratchpad outputs are not durable, so the numbers are copied into the card.

### C2. Optional bends without binaries, and their limit with convex cells
- **Claim:**
  - Flex pairs (bend pairs h -> h+-1 -> h with lengths >= 0) let steps appear or not with no binaries.
  - Convex-cell clearance against wall lines is exact only when no flex step is used. Once a step is used, the cell is not convex, and the line beyond the step is a ghost wall.
- **Evidence (PSG 197, pins window 8, 2026-11-22):**
  - D402 flexes one-piece stretches only, giving 344 flex marks. Cells: slack 208, largest clear row 3.0.
  - Option C flexes the end pieces of multi-piece free stretches too, giving 646 marks; it is needed for 8 of the expert's 50 steps. Cells: slack 948.
  - Of C's 380 stretched clear rows, 316 (512 of 634 slack) have a flex insert at the vertex and a target wall next to an insert (3335b3c5ab).
  - Pairwise separation with the same flex goes from 102 to 66 (554650a522).
- **D416 hybrid measured (23e8ae21c6):** flex groups touch nearly every wall: flex-incident pairs 9559 of 9682 (D402) and 28,179 of 28,179 (C), 38k and 113k binaries, no 120 s solve. So exactness for optional bends cannot be bought by local binaries around flex: a negative result. Next: 4-axis constraint-graph compaction.

### C3. Fixing pairwise sides from a reference drawing turns keep-apart into an LP
- **Claim:** pairwise separation has one disjunction per wall pair. Taking each pair's side from a reference drawing (the candidate with the largest least margin) gives an LP. Re-picking sides from the LP's own drawing converges.
- **Evidence:**
  - 02df480ad1 (`sides_from`).
  - 3dc1b90fee (fixed point): 9682 pairs, 0 binaries; slack 624 -> 118 -> 110 -> 108 -> 102 -> 102 at window 8.
  - Pairwise with free binaries: about 38,700 binaries, no solve in 120 s.
- **Open:** is the fixed point the global optimum over sides? (Local search; no proof.)

### C4. A layered by-construction pipeline (architecture)
- **Claim:** Structure -> Shape -> Slots -> Heights -> Signals, each layer keeping a stated promise by construction and never repairing an earlier one. Infeasibility in the oracle names the upstream promise that broke.
- **Archaeology:**
  - D361, D362, D365, D366, D367, D368, D379, D381, D388;
  - src/junction/planar/CLAUDE.md "Architecture";
  - DESIGN-sss-v2.md.
- Evidence still to gather: counts of promise bugs caught per layer (git log of the oracle tests).

### C5. Corpus of expert hubs
- **Claim (aim):** an extractor turns an expert's save into the model's inputs (topology, structure, shape, pins), and the model draws the corpus.
- **Evidence so far:**
  - PSG 197 extracted, matching the hand model: 122 of 122 crossings, 50 of 50 tunnels, ports.
  - PSG 186 (Brainmelter) extracted, with W, E and S matching the engineer's counts.
- **Archaeology:** corpus-growth goal 2026-11-21; X8 test a893acac0d; wip/corpus/.
- **Needed for a paper:** many hubs drawn with full rules, with solve times.

## Archaeology to do
- Earlier over-strict findings before the answer key:
  - D407 (flex inserts join their side);
  - D408 (dummies are no track);
  - §8.10 (a length-0 step imposes no spacing);
  - D410 (port positions);
  - D412 (port lane length 0).
  Collect their before/after numbers from git and the cards.
- The pocket rule (§8.9): a negative result, the "longitudinal" exemption measured and rejected (FS-d).
- The hand T2/T3/LL² numbers from the v1 component solver, as a baseline for solve time and area.

### C6. Optional steps are the hard core; deciding them upstream makes lengths exact (open)
- **Claim:** keep-apart with optional steps is a disjunction per wall pair (C2, D416 gate: 38k-113k binaries, no solve), in line with Patrignani. With the used steps fixed upstream and cells refined around them, the lengths solve is exact and binary-free (convex cells, fixed landings).
- **Evidence so far (75f61cc2aa):** NOT yet supported: with the expert's steps fixed, cells elastic W8 579 vs 189 without steps; dummies 78 -> 162. Suspected probe artefacts (flank least lengths, dummy landing choice) under diagnosis.

### C7. On the expert's real geometry, pairwise separation fits and convex cells do not
- **Claim:** with the expert's real drawing (piece lengths from the save; 91 of 101 tracks held within 1 half tile), pairwise separation with sides taken from that drawing needs slack 108 (sep 84 over 19 rows, mostly on tracks not held to the expert), while convex-cell clearance needs 1884-2094 even with dummy landings steered by the expert's geometry.
- **Evidence:** 6d6de658d9, ea261847a4, c3f316f1d0 (probe "planar draw: PSG 197 clearance rows evaluated on V's drawing"); vref built from tools/junction/corpus/out/psg197/lengths.txt.
- **Further (9781bccf5d, pairwise integer probe):** a monotone face-walk rule fixes 616 of 1449 pair directions but 66 are short of spacing in the expert's drawing (it fixes the half-plane, not the spacing); letting the MIP choose all directions (5796 binaries) does not solve in 120 s. So the direction choice is the remaining hard core.
- **Method note:** an elastic drawing is not a stand-in for the expert's geometry (no objective between pins): only the save's piece lengths are (CORRECTION 045429a251).

### C8. Directions from earlier drawings give a whole expert hub in seconds (method, D418)
- **Claim:** pairwise keep-apart with each pair's direction re-picked from the previous drawing (an LP per round, slack only on separation rows, a small length tie-break), then one integer solve with directions fixed, draws the whole PSG 197 (1535 pairs) in 2 rounds plus 6.4 s, all in HiGHS (MIT), where the same program with direction binaries (6140) does not solve in 120 s.
- **Evidence:** cdca61376f (probe "planar draw: PSG 197 option (1), directions from earlier drawings"), variant A (the expert's steps as bends), no pins.
- **Caveats:**
  - The seed must be separated: a tidy drawing with no keep-apart gives noise (246422c311).
  - Elastic drawings need a length tie-break, or unpinned points drift (3f9ca7a809).
  - With optional flex (flex C: 28.5k pairs) the LP rounds reach slack 0 in 2 rounds. The final must take the directions the last round proved (that round's input drawing), not re-read them from its rounded, tidy-packed output: re-read, even the all-continuous final is Infeasible (an artefact, earlier read as integrality). With the proved directions: LP 6.4 s, integer points with continuous axis lengths 40 s, fully integer (whole-tile axis pieces) no incumbent in 60 s. So on optional flex the integer step is slow, not infeasible (`PLANAR_PROBE_FINAL_SIDES`, `PLANAR_PROBE_CONTINUOUS`, 38fa75019a).
- **Contrast:** offline CP-SAT (ortools 9.15, native disjunctions or big-M, 2 workers, 60 s) finds no solution on the same program, including the case (1) draws in seconds; HiGHS with 6140 direction binaries finds none in 120 s. Directions read from geometry beat generic MIP/CP on this problem.

### C9. Optional steps decided by discover, commit and prune give a curve-legal whole expert hub (method, D422 probe)
- **Claim:** optional sideways steps make curve spacing conditional, which no LP can express (Gemini; metro-map MIPs and VLSI jog insertion use binaries). Deciding step use inside the lengths layer instead keeps every program binary-free:
  - discovery LP rounds with iteratively reweighted L1 insert prices and a box price;
  - commit the used inserts (curve rows on their bends, the rest held at 0);
  - re-pick keep-apart directions;
  - at most one prune.
  With these, PSG 197 draws with keep-apart, bays and curve spacing on every bend, and the whole-tile integer final solves in 17 s. Without IRL1 and the box price, no integer drawing was found in 60 s.
- **Evidence:** card CARD-E5-flexstep, last bullets (5578e8c9bd); probe "planar draw: PSG 197 option (1), directions from earlier drawings" with PLANAR_PROBE_RATCHET, PLANAR_PROBE_IRL1=0.1, PLANAR_PROBE_BOX=0.01.
- **Caveats:**
  - 10 steps and a 121x188 box against the expert's ~50 steps and 101x124: the trade between steps and length is set by the prices.
  - Bays are in, but coverage, node legs and the full corridor family (copies with decks) are not yet on 197.
  - 17 s is over the 15 s solve gate.
  - The integer final is off the game's tile grid (N/S piece ends x even 180 / odd 50; E/W y even 102 / odd 178), so the curve-legal drawing is a half-tile relaxation; with grid rows HiGHS finds no drawing in 15 s (D423, CARD-E5-flexstep). Evidence for the claim is the LP rounds.
  - Unpriced discovery commits 173 jogs and grows 2.4x.

### C10. Keep-apart sides inside bundles: what headings decide and what they do not (probe, D426; the "room is short" reading is a suspicion, not proven)
- Claim: in the pairwise keep-apart LP, a wall pair whose face walk is monotone both ways (each wall in the other's inner half-plane, read from Shape's headings alone) can take its side by construction; on T2, T3, LL2 and PSG 197 every such pair (all anti-parallel: 8, 18, 17, 6) holds in the last round. Sibling lanes as exact offset curves between channel crossings (D388 spine) also hold where written.
- Negative: on T3 and LL2 the remaining slack (13, 10) sits entirely on corner pairs at bends where neither inward normal is certain from headings; freeing those pairs' sides by binaries leaves 12 and 7, and an IIS with sides re-picked from that drawing is local to them. Sides were not the cause; the room between facing bends of neighbouring tracks is short.
- Evidence: feature 948356b4e1 (ng1 7372d911eb..625835dbd3); logs docs/junction-tool/wip/probes/e5land/sideprobe-*.log, siblings-*.log, facing-*.log; card wip/CARD-E5-bundle-ends.md.

### C11. Exact local certificates separate drawing failures from upstream gaps (E5, 2026-11-22)
- Claim: restricting pairwise keep-apart to a few faces, every pair binary, inserts free and a tight coordinate bound (big-M 1024 instead of 16384) turns an undecidable whole-hub question into sub-second exact answers: T3's crowded faces are drawable (Optimal, 0.3-3 s), so its slack is the pipeline's; LL2's face 33 is not (Infeasible, 0.8 s), and deletion filters over pairs and rows give a three-row core: two keep-apart pairs and the closing step of a copy, i.e. a copy rejoining its parent inside a face where two other tracks' bends face each other.
- Method: the LP relaxation was feasible (no IIS), so the core came from deletion filtering (skip_pairs, skip_rows), each solve under a second.
- Evidence: feature (ng1 856ca51153, 35bc8cedfd, a973e7081f); logs docs/junction-tool/wip/probes/e5land/cert-625835dbd3.log, face33-856ca51153.log, face33-rows-35bc8cedfd.log.

### C12. A solver's own constraint set is not a legality check: exemptions hide clashes (E5, 2026-11-22)
- **Claim:** the drawing program's keep-apart model skips some wall pairs (adjacent walls, span pieces, walls that touch once may-be-zero pieces vanish). An independent geometric check of its "clean" drawings found overlapping tracks on three hubs (T1 1, T2 7, PSG 197 6). Every one was a skipped pair; none was a written row that failed. With the exemptions narrowed (span pieces exempt only against their own crossers; touching pairs kept, with only the touching ends exempt; S-bends kept off crossing points), T1 and T2 drew clash-free on the tile grid, and so did 197 given 27 s.
- **Evidence:** cdbf85467b (clash check, outputs probes/e5land/clash-d0d1276432/), aa9cfbc75b and the strict*/flanks* logs, e0c5ca0707, dc79ad55c7.
- **Caveats:** the check covers overlaps and off-crossing touches, not spacing. Each narrowed exemption was tuned on one hub, and one (the crossing-flank rule) traded T2 against T3. Hence D428: a legality predicate from the game's tile rule, and an enumerated local suite against it, before more rule changes.

### C13. Contracted rows judged on fixed points are exact on eight local configurations (domain suite, 2026-11-23)
- **Claim:** under D432 (rows over contracted lines) plus the suite-found rules (node lead D429/D430, end_node, cross_piece, grid, D434 port line 0 or ordinary), the drawing program's rows agree with the game's tile rule on every enumerated geometry of eight configurations (X1, X2, M1, K1, L1, Y1, P1, BR1: tunnel and bridge crossings, a 45-degree crosser bending at its crossing, S-bend merges, a cascade and trident, lanes through a corner, a copy, a prio row): 0 too strict, 0 too loose, with port and box rows judged and interface breaks separated by counterfactual.
- **Method notes:** (1) the turn rule must hold along route paths through nodes, not only within a track (F6: contraction drops a zero first flank and can turn two eighths at a switch); zero-length tracks are transparent. (2) An elimination-based fixed-point checker misattributes blame when a variable is solved from a row other than its definition (a grid half from a rewritten port row); solving each auxiliary from its defining row restores correct blame. (3) "Off interface" is decided by counterfactual (the rows without interface rows accept), not by failing tags, because derived values carry interface breaks into other rows.
- **Evidence:** feature 6ad1f4c541 (domain-22..32); reports docs/junction-tool/wip/probes/domain/encoding-*-span_lead_endnode_crosspiece_grid_portzero.txt; D434, D435.
- **Caveats:** coverage is local (neighbourhoods of k = 3 pieces around events); the prio row's own pieces are never varied and D384's exact length has no row yet; off-interface leaks (domain-illegal geometry only interface rows reject) are not counted.
