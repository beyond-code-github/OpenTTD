# SPEC: the Slots layer (slots and lengths) hand-off (DRAFT, 2026-11-20 night, orchestrator; pending the engineer's ruling on merging Slots and Lengths)

Third SSS hand-off spec, second form: Slots now also decides every length (Lengths, DESIGN-tracks §17.4, folds in; engineer, 2026-11-20: "So we can still call it SSS right, just length is now part of slots?"), IF X2i agrees and the merge is ruled. wip/SPEC-slots.md keeps the checks and the probe record. Evidence: SPEC-slots §7 X2a-X2i and G0; wip/REVIEW-slots-lengths.md (Stiemke condition, options A-E, follow-up). Summary of why: with port pins (D313, D377), exact deck spacing (D376) and cusps fixed, most of a dense hub's lengths are determined by sideways quantities (X2g: T2 60/77 parts cannot be stretched; X2h: still 49 with free-size steps at the needed places); a layer that decides sideways moves without lengths cannot promise that lengths close. The coupled problem is small: 20-40 free degrees per variant once the exact set is in (G0). Written from Shape's output model (SPEC-shape §3, §6) and SPEC-slots §0-§2, not from code (D368).

## 1. Input
- One Shape variant: Structure's variant (track map, sites, structures with `limit`, `side_gap`), per track its heading list and event pieces, per crossing its angle, per structure its axis, `middles`, `copy_middles`, `end_run`; Shape cost.
- **Rule fields handed down by Structure** (D366; as `limit` and `side_gap`): the least part between a crossing and the crosser's next bend; the least node-leg part; the curve-spacing window at the spec's train (`JunctionCurveGaps`); the landing's least length and parity (D375); prio coverage length (D349); the least S-bend (1 lane). Each is a rule the spec already holds; none is a later layer's output. (List to settle in Q2.)
- Variants arrive ranked (Structure cost, Shape cost); Slots solves the top k (D366: 1-3).

## 2. What Slots decides (and nothing else)
1. Every track's sideways slot in every interval of its frame (SPEC-slots §2.1: port, node-group, deck, channel frames), hence every S-bend: where it exists, its side and its whole-lane size.
2. Every piece's length (half-tile units, tile parity), hence every point's position, the box and the footprint.
3. Deck positions: each crosser's place along its separator, the span (exact), within `limit` (SPEC-slots U1).
4. Which fork or merge leg keeps the trunk's slot (D360).
Not decided here: headings (Shape), structures and kinds (Structure), levels and terraform (Heights), signals and joins (Signals).

## 3. Rules (the model)
- **Closure:** every face closes in x and y with the pieces' headings (Shape's), lengths and steps.
- **Exact (SPEC-slots §2.2, carried over):** port pins (a port's lanes one lane apart, ports `side_gap` apart); copies exactly one lane from their line; rows one lane from their lane (D377 Q4); cusps (legs on the trunk's point); deck crossers per span at Shape's width with no step inside a deck, tunnel or ramp; separator straight across its span; end runs (`end_run`).
- **Lower bounds (from the rule fields):** pieces, node legs, curve spacing between bends (the S-bends' own bends included), landings with parity, coverage.
- **Steps:** whole lanes; anchored per D378 (in-bound fans next to the fray, out-bound close-ups next to the join); steps on diagonal stretches allowed (D377 Q5).
- **No clash rows.** Two parallel neighbours are kept apart by slot gaps (exact, at least one lane); non-parallel walls can only meet across a common face (planarity); overlap-freedom is a by-construction promise. A clash found by the oracle is a missing promise (e.g. a face with facing reflex corners, fixed by a dummy edge read from the headings), never a row (engineer, 2026-11-20).
- **Cost:** steps (1 per lane), then area (D329's linearisation), then a stated deterministic order. Slots' cost ranks the k solved variants; the cheapest is the seed.

## 4. Output model (each solved variant), refining Shape's (D367)
- Shape's variant unchanged.
- Per frame: intervals and slots. Per track: steps (stretch, side, size, anchor) and every piece's length; hence every point.
- Per structure: crosser positions, span middles per allowed kind (exact).
- The box (width, height) and the footprint tiles.
- Slots cost.
- **Hand-on to Heights** (named now): the fixed footprint and every piece's tiles; ramps only on axis pieces (Heights' own rule).

## 5. Promise (tests at Slots' output, D367)
- S-P1 Refinement: Shape's output unchanged; H1-H9, H11 still hold.
- S-P2 Closure: every face closes; every point's position follows from its track's pieces.
- S-P3 Exact rules hold (pins, copies, rows, cusps, deck spans within `limit`, no step in a no-step zone, end runs).
- S-P4 Lower bounds hold (curve spacing, legs, landings and parity, coverage).
- S-P5 Clash-free: the clash oracle (`JunctionTracksMayShareTile`) finds nothing.
- S-P6 Every variant handed to Slots is solved; an unsolved one is a missing promise upstream (D361).
- S-P7 Time: the top k within the seed budget (D297: 5 s for the whole seed).

## 6. Open questions (for the engineer)
- Q1 The merge itself (SPEC-slots §7 decision card): Slots and Lengths become one Slots layer; D362's list and planar CLAUDE "Architecture" change; Slots' frames and rows carry over as Slots' sideways part.
- Q2 SETTLED (orchestrator, 2026-11-21, from the rules as the composer and layout program already apply them; reversible, toggle candidates noted): see §8.1.
- Q3 k (variants solved per seed): 1, 2 or 3, from G1's time.

## 7. Build order (each a brief, test-first at Slots' output)
- G0 done (82b161c4ed): free degrees per variant T2 20, T3 29-38, LL2 23-26 with the exact set.
- G1 **Riskiest first:** the joint solve (integer steps, lengths, exact rows, lower bounds from Q2's list at their spec values, no clash rows) on the top 3 variants of T2, T3, LL2; then the clash oracle. Count solved, clashes (each by face and corner pair), time per variant.
- G2 Reduced formulation if G1's time needs it: solve in the closure's free directions (G0) instead of all lengths.
- G3 Area (one linearisation or a fixed-box sweep) against MappedComposer's areas (T2 906.25, T3 1941.25, LL2 1347.25).
- G4 Oracle `CheckPlanarSlotsPromise` (S-P1..S-P5) with hand variants; outer test on T2, T3, LL2.

## 8. G1 design (orchestrator, 2026-11-21)

Units: half tiles, as `PlanarModel` (`STEP`: an axis unit moves 1 half tile, a diagonal unit 1 half tile on each axis). A lane is 2 units sideways on an axis, one `Right(h)` unit (1, 1) on a diagonal; in both cases `Dot(offset, N(h)) = 2` per lane with `N(h) = STEP[(h + 2) % 8]`.

### 8.1 Q2: the lower bounds and their rule fields
Structure will hand these down as it hands down `limit` and `side_gap` (D366); G1 builds them in the probe from the test spec (a stand-in, named as such; moving them into Structure is a later step).
| Field | Value (units) | Applies to | Source |
|---|---|---|---|
| piece | axis 2 and even, diagonal 1 | every piece's total | `Model::AddPiece`, game geometry |
| parity | every bend and end point on a tile-edge midpoint (x + y odd); an axis piece's points on its axis' parity | every point but crossings | `Model::AddSeg` |
| `clear` | (1 at 90 degrees, 2 diagonal crossers) + 2 `end_run[k]`, max over allowed kinds | separator: the part from its first (last) crossing back (on) to the nearest bend or step; same on a copy's deck | tile geometry; Shape's `end_run` |
| `cross` | 1 | a crosser's parts on each side of a crossing | a crossing never on a bend |
| deck gap | >= 2 between consecutive crossings along a span; span middles `(last - first) / 2 + 1 (+1 diagonal) <= min limit[k]` over allowed kinds | each span (separator deck, copy deck) | SPEC-slots §2.2, D376 |
| landing | >= `clear` + `clear` + 2 between two spans on one separator piece | structures in series | D375 (odd preference left to G3's cost) |
| `node_leg` | 2 | a node's leg: its travel from the node to its next event or end; EXCEPT between two consecutive splits (merges) on one trunk: 0 (a trident on one tile: axis trunk, legs straight plus both diagonals, D110 a) or at least the signal tile (D110 b), one binary per pair (not yet in the probe; G1 had a plain >= 2, which forbids D110 a) | layout program rule, D110 |
| curve | consecutive same hand >= g, opposite >= 2; k apart >= min(2 tl - 1, k cl), k = 2, 3; g = min(2 tl - 1, max(2, cl)), cl = `DeriveCurveSpacing` | along every route path, through nodes and copies (D313) | `Model::PieceCurves`, `MakeParams` |
| `bay` | 2 tl + 4 | a non-prio route path: from its last topology split or port to each topology merge | D83, composer |
| `coverage` | 2 prio_tiles + 4 | each prio row track's length | D349, TM5p |
A node is a topology node when no track at it is a copy or row (`part_of` = UINT32_MAX).

### 8.2 Steps
- A **step place** is a (track, stretch) with one piece of it named: the last piece when the stretch ends at a split node, the first when it starts at a merge node, else the first (D378's anchoring; position along the piece stays free). None on a separator's or copy's stretch inside its span.
- A step of n = p - m lanes (p, m integers in 0..8, binaries zp, zm with p <= 8 zp, m <= 8 zm, p >= zp, m >= zm, zp + zm <= 1) on heading h is an exact piece of heading h + 1 (p) or h - 1 (m), length 2p or 2m, splitting its piece into a before part and an after part (each >= 0; the piece's total minimum applies to before + after). It moves sideways and along: travel included (review F3).
- A **cusp** (a non-straight leg at the trunk's heading) is a step anchored at the node: before part 0, size >= 1 lane on the map's side.
- **Copies and rows** keep exactly one lane from their parent chain (`ParentChain`, H8): per piece j of the beside track, at one point on it outside its opening and closing cusp steps (an event if it has one, else a bend end), `Dot(point - parent point on chain piece j, N(h_j)) = 2 s`, s the beside side (+1 right).
- **Curve rows:** Shape's bends per route path as `PieceCurves` (unconditional, k counted over Shape's bends); a step's two bends against the nearest Shape bend on each side, k = 1, relaxed by M (1 - z). Window rows through steps are not written: the curve oracle measures what this misses.
- No neighbour (frame) rows in G1 besides pins, decks, cusps, copies and rows: the clash oracle splits hits into same-channel neighbours (the frame rows planned in SPEC-slots §2.1) and across faces (unknown: a missing promise).
- **Objective:** 1000 per step lane (p + m), plus every part's length, plus width + height. Area is G3.

### 8.3 Oracles (S-P5 and the curve rule, at G1's output)
- **Clash:** the solved points to a `PlanarGeometry` over the expanded topology (one deck per span, `back`/`front` = `clear`), `RasterPlanarLines`, and the layout program's own tile scan, exported unchanged as `PlanarGeometryClashes`. Each hit: tracks, stretches, pieces, headings, tile, the faces both stretches bound (or none), same channel or not.
- **Curve:** per route path, the solved curves (hands, positions) through `JunctionCurveGaps` and `JunctionCurveGapsHold`; count breaches.
- Time per variant, 15 s limit; a timeout counts as unsolved.
