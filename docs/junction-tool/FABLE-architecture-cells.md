# FABLE-architecture-cells: review of the planar solver architecture, SSS with convex cells (at feature/junction-tool 3c3952cf4b; ng1 wt-cells 0506a740f2)

## Verdict
Keep the SSS layering. Convex cells are the right kind of guarantee: a face cut into cells that each close and turn one way is planar (SPEC-shape §8.3 steps 1-3 hold, since two convex cells lie on opposite sides of their dummy's line). But they are applied at the wrong grain. Every physical track is a wall, so the machinery is spent on faces coop never treats as free: lane/lane, lane/row, separator/copy and the crossing lattice. The rigid and degenerate cells found on T2 so far sit in such faces or at frays and prio joins (F1, F2). The headline "every variant convex" is measured on a model that leaves out the bends that break convexity (F1). H13 as planned does not certify what Slots needs (F3), and the clearance rows have a hole at dummies (F4).

**The engineer's question, answered: yes, there is a simpler route, and it is more coop-like: draw bundles, not tracks.**
- Shape heads channels: every lane of a channel gets one heading list, as beside tracks already do (H8).
- Bundle-internal faces become rigid corridors, drawn by the offset rule (SPEC-slots-lengths §11, generalised).
- Coop components become fixed-shape gadgets at nodes and crossings, each checked once per lane count by the checker: fray, join with prio row, doubling parallelogram, crossing block.
- Convex cells apply only to the faces between bundles.

This is the MappedComposer's skeleton (bundles plus atomic primitives, D310 and D330), which produced checker-clean T2 (862.25, ROADMAP item 6) and clash-free ladders on all 201 tags (DESIGN-tracks §8, probe 5). SSS's guarantee replaces its lazy loop.

Neither approach passes D387 (PSG 197) today. The blockers are mostly shared and sit upstream of cells: authored crossings, tunnel over tunnel, a portal over a tunnel, prio per merge. One rule of the cells plan refuses PSG 197 outright: H11 (F8).

## Findings

**F1 [wrong] "Every T2/T3/LL2 variant convex" is measured before the bends that make faces non-convex.**
- What the code does: CheckH12 flags only turns > 0 (planar_shape_layer.cpp:646). `Norm` folds a cusp into -4 (:28-35), so a cusp counts as a convex corner.
- Counterexample: T2 faces 35 `[B2 H11] <-4> [prio:B2 H7]` and 36 (E1 with prio:E1) pass as 2-wall cells (wip/PROOF-T2-cells.md; STATUS "2-wall 2"). Faces 29, 30 and 32 are triangles closed by a -4 corner.
- A 2-wall cell winding -8 has two -4 corners. Its walls are therefore antiparallel and collinear: zero width in every drawing. It cannot be drawn as two tracks without a sideways move, and Shape does not hold that move.
- The same applies elsewhere: cusp steps, port fans and close-ups, landing steps and flex inserts (D383) are all outside Shape's pieces today. Each one adds a reflex bend to one of its faces.
- Fix: count H12 green only for cells with no -4 corner. Until D385(ii) is built, the outer test should assert the number of cusp-closed cells, not zero messages.

**F2 [wrong] The H12b preference counts walls, not corners, so rigid triangles are undercounted, and two of them carry prio rows.**
- What the code does: `is_wide` asks for two cells of 4+ walls (:864). The first candidate, d = h (:850), continues the incoming piece, so it always makes a 0 corner (:211-216). Collinear walls add more 0 corners: node straight-throughs, and lane to row to lane.
- Counterexample: I applied the code's rule by hand to the probe face list. INFERRED that the real walls match it; H12a found the same reflex faces.
  - Face 1 gives {EF2 p1, E2 p0, F1 p1, dummy}: 4 walls, but only 3 non-zero corners (-3, -2, -3). That is a right isosceles triangle.
  - Face 2 gives {CB2 p0, prio:B2, B2 s4 p1, F2, dummy}: 5 walls, a triangle.
  - Face 4 gives {CB1 p0, prio:B1, C2, dummy}: a triangle.
- So T2 top has at least four dummy-made triangles, not the one STATUS reports (face 13).
- Faces 2 and 4 each put a prio row on a side. D384 fixes a row's length to its coverage, so it can fix the triangle's scale. STATUS's "the parity worry does not bite on T2" was therefore decided on the wrong count.
- Fix: rank candidates by non-zero corners (degrees of freedom = corners - 2), and redo the parity check on that count.

**F3 [wrong] H13 as planned does not certify that Slots can solve.**
- (a) The "lengths >= 1" premise does not hold. SPEC-shape §8.7 says "every wall >= 1 (homogeneous rows scale, so >= 1 loses nothing)". That is true only when nothing is exact. Slots' lower bounds (SPEC-slots-lengths §8.1: clear, deck gap, cross, node_leg, curve g, bay 2TL+4, landing) do not scale against D384's exact coverage, the pins or the spans.
  - Counterexample: prio:B1's coverage holds B1's crossings with D1 and F1 (G2f trace in STATUS: "cross 3 B1 D1, cross 5 B1 F1"). With the slider set short, so that the coverage is below the sum of clear, deck gaps and cross over those parts, an H13 with >= 1 certifies and Slots is infeasible. The D361 breach just moves into the certificate.
- (b) Two row sets are missing from the planned H13: the clearance rows (§8.2) and the dummy rows of F4.
- (c) Parity: §8.7 open (a) proposes scaling by 2 and then offsetting the odd values. Exact quantities cannot be scaled, so that argument is void.
- (d) A complete H13 will break H10 on T3. T3 o7t2f0m3270 is infeasible because of the crossing lattice inside spans (SPEC-slots-lengths §10.4). A complete H13 refuses it whatever dummies the fallback tries.
- Fix:
  - H13 = Slots' full row set without the objective: every lower bound, clearance, landing and dummy row, and curve rows over Shape's bends.
  - Run it as an integer program with D382's forced parities.
  - A refusal names a missing Structure or Shape promise. The fallback search over dummies cannot repair such a refusal.

**F4 [collision] Clearance rows stop at dummies.**
- What the spec does: §8.2 writes a clearance row only between track walls that face each other (3 to 5 eighths apart) inside one cell. A dummy is not a track, which leaves two holes.
- Hole (i): a dummy with d = h landing on a wall at c = -1.
  - In cell b, the incoming wall (h) and the landing wall e (h-1) are one eighth apart. In cell a, the outgoing wall (h+1) and e are two eighths apart. Neither cell writes a row.
  - The only rows left are 0 <= u <= len(e), so the dummy length L may be 0, which puts the bend on track e.
- Hole (ii): two tracks that face each other only across a dummy, one in each cell, get no row.
- Therefore §8.3 step 4 ("separated by the cells between them") is false as written.
- Not hit on T2 top by my hand application: its d = h dummies land at c = -2 or -3, or at -1 on box edges (faces 10 and 11). T3 and LL2 are not checked.
- This also answers §8.3 (a) and (d): the facing set is not enough.
- Fix:
  - Every dummy gets L >= RungSpacing(start track, landing track), as the old §9.4 chord rows had.
  - For each pair facing across dummy D: dist_a(f, D) + dist_b(D, g) >= spacing. In a convex refinement both terms are linear and involve no choice.

**F5 [missing] A dummy may land on another dummy, and the hand-off cannot say which.**
- The landing scan (:852-856) accepts any wall with c in [-3, -1], earlier dummies included (they are {UINT32_MAX, UINT32_MAX}).
- `PlanarShapeDummy` (planar_shape_layer.h:43-44) can only name a track piece or a box side.
- `SplitCell` matches the first wall with an equal (track, piece) (:194), which is any dummy.
- Slots cannot write R = start(e) + u·dir(e) for such a landing. On T2, faces 12, 13, 24 and 34 each hold two dummies.
- Fix: give dummies an index and a landing-dummy field, or skip dummy walls as landing candidates (§8.3 c: "a physical wall").

**F6 [missing] Bend pairs (D385 ii) will multiply dummies, need sides that Shape cannot know, and leave curve speed to an oracle.**
- (a) Count: SPEC-slots-lengths §10.1 marks flex on every diagonal outside spans. G2c skipped 116 flex vertices on T2 top. Each bend is reflex for one side, so roughly 116 more dummies on top of today's 17 (INFERRED). Each one ties an often unused insert to the extent of its landing wall.
- (b) Sides: G1x's residual offsets (EF1 +8, ED1 +8, C1 -8, E1 +8, AB1 -3, CD1 -5) take their sign from lengths elsewhere. A bend pair has one side. Shape must either place both sides (a dogleg of 4 bends) or decide a sideways move. Planar CLAUDE.md ("It makes no sideways moves") and SPEC-shape §2 item 1 forbid the second.
- (c) Curves: D383 writes no curve rows on inserts. A curve row between a pair's bends binds only when the insert is above 0. That needs a binary (what D340 removed) or an oracle (G1v-G1x counted 15-47 breaches). The main line's curve speed is then not guaranteed by construction.
- Fix:
  - Shape places a pair only where a rule names it, with a known side and size >= 1: cusps; fans and close-ups from the ring order (D378); landings (D375). For these pairs the curve rows are unconditional.
  - No blanket flex. Flex goes only where §10.1's own cut rule proves a cut has no stretchable piece.

**F7 [missing] Nothing ties the lanes of a channel together.**
- Shape's integer program has beside rows (H8) and span rows (H11) only. `planar_shape_layer.cpp` has no channel rows (grep).
- At equal turning, two lanes of one channel may turn on different stretches (SPEC-slots §0.1 finding b). That makes thin faces between lanes with reflex bends, which cells must then split with dummies.
- Coop bundles bend as one: `coop/files/800px-227_bbh03.png`; RECON-corpus-anatomy summary 4 ("plain 3-7 lane bundles run 40+ tiles").
- Fix: extend H8's equality to every lane of a channel over its shared stretches.

**F8 [collision] H11, P4 and Shape's Ends rule refuse PSG 197 (D387).**
- Mixed angles under one span (wip/topologies/psg197-decisions.md):
  - Tunnel #2 (A1c, (95,158)-(95,172), 13 middle tiles) is crossed by B2b and B2c over 2 tiles each (diagonal) and by F1, F1b, F3 and F3b over 1 tile each (right angle).
  - H11 forbids mixed angles under one span. D376 accepted H11 only "as long as it just affects the one or two tiles under the span", and this span is 13 tiles.
- Tunnel over tunnel (bbh.json):
  - Tunnels #23 and #26 (F3, F3b, (95,169)-(99,169), portals at heights 3/4) pass x 96-97.
  - There, tunnels #3 and #4 (A1d, A1e, portals at heights 2/3) run from y 158 to 172.
  - P4 forbids tunnel over tunnel and D365 defers it.
- Portal over a tunnel: F3's portal sits on #2's middle tile (95,169), which SPEC-shape §2.1 Ends forbids.
- The sidecar misses these crossings: its evidence rule (a track piece on a middle tile) cannot see a tunnel crossing a tunnel.
- Fix:
  - H11 per run of one angle. Since D379, the gap between runs is a Slots length inside the span, so D376's reason is gone.
  - Tunnel × tunnel at two depths and a portal over a tunnel go into Structure's model and into the sidecar.

**F9 [wording] The docs still describe the pre-D385 Shape.**
- Planar CLAUDE.md says Shape "makes no sideways moves", and SPEC-shape §2 item 1 says such moves are "never Shape's". D385(ii) moves where steps sit into Shape.
- "Two cells of 4+ walls" should read "corners" (F2).

## Q1. Will SSS with convex cells give coop-like layouts?

**What it guarantees, once F3 and F4 are fixed:**
- a planar drawing with tile clearance;
- every exact coop quantity: pins, parallelograms, exact prio length (D384), deck limits;
- refusals named before Slots runs.

**What it does not guarantee:**
- Curve speed at bend pairs (F6).
- Compactness. Each dummy fixes, by wall order (:852-856), which wall each reflex bend faces, and the fallback fires only on infeasibility, never on area. The cost is unmeasured; compare against the composer's T2 at 862.25.

**Coop forms it admits:**
- S-bends under a deck. 93% have the deck parallel to the crossed heading (RECON-corpus-sbend-crossings Part B, psg167_33). Least turning produces these naturally, because H4 forces a parallel crosser to turn at least one eighth each way.
- A corner's diagonal under a deck (45%).
- Doubled diagonal pairs under one deck (13%; H6 width k + 1).

**What it excludes or puts at risk:**
- bundles bending as one (F7);
- mixed-angle crossers under long tunnels (F8; RECON-corpus-crossings §6: 17% of tunnels carry 3+ tracks);
- S-bends with the deck across the heading (7% of S-bends): they cost 2 more turning and never win;
- tunnel over tunnel and portals on hill terraces (PSG 197).

## Q2. A simpler overall approach?

**What has been tried, and why it stopped:**

| Approach | Result | Why it stopped |
|---|---|---|
| Hand Composer | per-hub compositions | D326 ("why was that ever something we did") |
| MappedComposer | bundles and primitives; T2 checker clean at 862.25; T3 1941 at 0.64 s; ladders cleared every round-0 clash on 201 tags | generate-and-check (D361, D381 rejected option C); construction rules with no ruling behind them (D355, D356); LL² needed a 13-bit search at 4-10 s per variant (D363) |
| TrackComposer | LL² unsolvable by any fork or merge mask | D360 |
| SSS at track level | after two days, no clash-free Slots run (G2: 60-78 clashes) | still current |
| Templates per pattern | not tried | fail D326 and coop's woven boxes (RECON-corpus-anatomy 4) |
| Whole-hub grid search | did not finish in 18 min | DESIGN-tracks §8, not adopted |

**What I would do: bundle-level SSS.**
- **Structure:** unchanged. Channels already exist (planar_channels).
- **Shape:**
  - Ties every channel's lanes together (F7).
  - Classes the bundle-internal faces (lane/lane, lane/row, separator/copy, the crossing lattice) as rigid corridors, drawn by offset curves with miter rows (SPEC-slots-lengths §11). The lattice is rigid by rule (G1w).
  - Treats nodes and crossings as fixed-footprint gadgets from DESIGN-components C2-C7, kept atomic as D330 asks.
  - Runs H12b only on the faces between bundles.
- **Slots:** solves the lengths of the channels and gadgets.

Why it is simpler:
- The lattice cells (faces 14-19, 21, 26, 28, 31) and the lane/row faces (29, 30, 32, 35, 36) vanish into gadgets. Face 13 (the AB fray) and the triangles of F2 sit at frays and prio joins, so they too fall inside node gadgets (INFERRED).
- Dummies drop to the faces between bundles (INFERRED; measure it).
- Separation inside a bundle holds by construction, not by rows.

Why it is more coop-like: it is the engineer's own D310 ("this is absolutely how I thought we were approaching things") and the element-composition rule.

What it costs:
- a gadget catalogue with tests per lane count;
- a lane can no longer bend on its own inside a bundle (coop's steps next to a split, 19% of steps in RECON-corpus-steps, sit 4-6 tiles from it, inside the lane-split gadget);
- partial crossings of a bundle need gadget forms (PSG 197 below).

Honest note: H13 plus its fallback search is the same kind of mechanism as the rejected ladders. Both pick a face phase and then verify it with a solve. The holistic review (§2) already named a bounded iteration as "the known price". So moving to bundles gives up nothing D361 still forbids.

## PSG 197, the litmus test (D387)

**SSS with convex cells, as planned:**
- **Notation:** it refuses the hub. 8 must-cross path pairs have no line pair (psg197-gaps §1), and 7 crossings are same-route crossings (§2). Authored crossings (D343, D344) are needed.
- **Per-output-lane splits:** these are cascaded 1→2 nodes with cusps, so they need F6's cusp pairs.
- **Parallel tunnel bundles:** each tunnel is its own structure. H11 refuses G02 (F8), and tunnel over tunnel breaks P4.
- **Different lengths, exits landing at their joins:** a tunnel's portal sits a fixed end run past its outermost crosser (SPEC-shape §2.1 Ends), and the stretch from the exit to the join is free in Slots. Stages in series per line (A2b: 12, 2, 1 middle tiles) are structures in series, landing steps per D375. So exits can land at their joins.
- **What is not representable:** a tunnel longer than its crossers need (V's terraced portals). That needs terrain-solved seeds, which D365 defers.
- **The 3→2 merger:** a cascade of 2-input merges with overlapping route sets is representable (all 33 of V's joins are binary). Prio must be per merge, not per route (gaps §9).
- **Tunnels up to 15 tiles:** these fit `limit` (max_tunnel_length; the copies are not main-line ones).
- **Size:** about 101 lines, 62 node statements and 50 tunnels against LL²'s 18 lane paths, which already take Shape 8.8 s at keep 16. With hundreds of track-level faces, the 5 s seed (D297) is out of reach without a speed step (INFERRED).
- **What must change:** authored crossings; H11 per run; tunnel × tunnel and a portal over a tunnel in Structure; prio per merge; and a pre-signal flag on a split for choice by free tunnel (gaps §3, a Signals matter).

**Bundle-level SSS:**
- **Same blockers:** it shares every Structure blocker above.
- **Where it helps:** a parallel tunnel group (29 groups: 15 pairs, 3 triples) is the anatomy's own "crossing block". One gadget holds the 2-3 tunnels with their crossers, and its lattice cells disappear from the face count.
- **Where it does not:**
  - V's bundles are short: lanes split per output lane just before the tunnels.
  - Bundles are crossed partially: F3 and F3b pass over A1c only, while A1d and A1e are crossed by F2 and F2b instead (psg197-decisions rows 2-4). The current channel rule ("same crossed channels") makes A1c a channel of its own.
  - The gadget therefore needs per-lane crosser sets and staggered exits. The saving on 197 is smaller than on LL² (INFERRED).
- **What must change:** everything above, plus a "parallel tunnel group" gadget with per-lane crossers.
- **Verdict on D387:** neither approach handles 197 without the Structure work. The cells plan also needs H11 per run.

## Q3. The three biggest risks
1. **H13 is not a certificate, so Slots-on-cells discovers infeasibility (F3).**
   - The prio:B1 coverage with a short slider, and T3 o7t2f0m3270.
   - Rebuilding Slots on cells inherits this: H10 breaks the day H13 is complete.
2. **Bend pairs (F6).**
   - About 116 more dummies on T2.
   - Step sides set by lengths (G1x: EF1 +8 against C1 -8).
   - Curve speed left to an oracle (15-47 breaches).
3. **Clearance and coop tile sharing (F4).**
   - The dummy hole.
   - A lane and its row sit exactly one lane apart and share diagonal tiles (D353), so their rows must be equalities at the tile rule's spacing.
   - The prio join's half-piece pair (D342, D374) is not modelled. G2f's clash at (28,9), where B2's last piece is collinear with prio:B2's final diagonal, is that join drawn as a cusp. Cells need it as a gadget at the node corner, or every prio join breaks a clearance row.

## Suggested tests
1. Outer H12: report cells by non-zero corners and count -4 corners. On T2 top, expect triangles at faces 1, 2, 4 and 13 (F1, F2).
2. H13 red: T2 with a coverage below B1's lower bounds inside it. An H13 with >= 1 passes; the full-row H13 refuses with named rows (F3).
3. A hand cell where a d = h dummy lands at c = -1 on a track. Slots with §8.2's rows puts the bend on that track; the clash oracle must hit (F4).
4. A hand face with two reflex bends whose second dummy's first facing wall is the first dummy (F5).
5. Corpus admissibility, D385(3)'s missing step:
   - Inputs: the PSG 227 import (ORACLE-psg227) and PSG 197's bbh.json.
   - Read headings from the tiles, run RefinePlanarShapeCells, then evaluate every landing and clearance row on the coop's actual lengths.
   - The violations measure exactly what cells exclude.
6. Bundle measure: faces, dummies and rigid cells per hub at track level, against the same with lanes tied and bundle-internal faces classed as corridors, on T2, T3 and LL².
7. PSG 197 slice (G02, G14, G17): Structure and Shape from a hand sidecar. Expect a refusal citing H11 and P4 today.
8. Area: the best T2 on cells against the composer's 862.25.
