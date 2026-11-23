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
- **Keep-apart rows (D381), no clash rows.** Slots writes separation rows read from the map and Shape's headings before the solve (parallel neighbours one lane apart through bends; per-face rows keeping opposite walls apart). G1c showed closure and headings alone do not give a planar drawing. Superseded wording follows: Two parallel neighbours are kept apart by slot gaps (exact, at least one lane); non-parallel walls can only meet across a common face (planarity); overlap-freedom is a by-construction promise. A clash found by the oracle is a missing promise (e.g. a face with facing reflex corners, fixed by a dummy edge read from the headings), never a row (engineer, 2026-11-20).
- **Cost:** steps (1 per lane), then area (D329's linearisation), then a stated deterministic order. Slots' cost ranks the k solved variants; the cheapest is the seed.

## 4. Output model (each solved variant), refining Shape's (D367)
- Shape's variant unchanged.
- Per frame: intervals and slots. Per track: steps (stretch, side, size, anchor) and every piece's length; hence every point.
- Per structure: crosser positions, span middles per allowed kind (exact).
- The box (width, height) and the footprint tiles.
- Slots cost.
- **Hand-on to Heights** (named now): the fixed footprint and every piece's tiles; ramps only on axis pieces (Heights' own rule).

### 4.1 The drawn hub: the contract to Heights, Signals and every judge (D452, 2026-11-25)
The engineer: "separate the contract from the version". The back end (Heights, Signals, the rig export) and the judges (the domain suite, the rig) read ONE typed hand-off, `PlanarDrawnHub`, never a solver's internals (v1's `PlanarRouteProgram` with copy masks, v2's `PlanarDrawResult`). Any solver version writes it; v1 keeps its own path until it is retired or adapted.

Contents (all in one frame, half tiles, x east, y south; whole-tile shifted so every tile is >= 0):
- **tracks:** per track `name`, `kind` (Line, Copy, Row as the track map's `PlanarTrack::Kind`), `points` in travel order (a legal track: `RasterPlanarDomainTrack` gives no rule). A zero-length track has one point.
- **nodes:** per node `at` (its point), `in`, `out` (track indices), `split`.
- **ports:** per port `side`, `in`, `tracks` (left to right across travel).
- **structures:** per bridge or tunnel `separator` (track), `first` and `last` (the outermost pieces of the separator's raster: heads or portals), `ends` (pieces at each end belonging to the structure), `tunnel`, `crossers`, `limit` (the most middles, -1 none).
- Zero-length tracks (a split on its in port, D412) are tracks of the hub: nodes list them; the domain projection drops them from its nodes as `NodesOf` does.
- **routes:** per route `name`, `prio`, `paths` (each a list of track indices from its entry port track to its exit port track, consecutive tracks meeting at a node; copies are tracks, so a doubled lane's two ways are two paths). Rows carry no route.

What the contract promises (checked by its oracle, `CheckPlanarDrawnHub`): every track rasterises; every path's consecutive tracks meet at a node that lists them; every port track starts or ends on the box edge of its side; every structure's `first..last` lies on its separator's raster with axis end pieces; the domain judge (`CheckPlanarDomain` on the hub's tracks, nodes and structures) accepts.

Readers: the domain input is a projection of the hub (tracks, nodes, structures). The render (`RenderPlanarDrawnHub`) gives Signals and Heights what v1's render gives them (`PlanarRender`: track routes, layout, signal network, node and port tiles), one `JunctionTrackRoute` per path, built by the one game rasteriser (`RasterizeJunctionRoute` on the path's move string, cross-checked tile by tile against the domain raster). Rows are not rendered in the first version: the signal pass places the prio element by its own search as in v1 (a known gap, to close when Signals reads rows from the hub).

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

**§8-§11 are v1 evidence (D388).** They were designed for track-level Slots; SSS v2 rewrites Slots' rows by family in E5 (PLAN-sss-v2.md, DESIGN-sss-v2 §3.3). Read them for the rule fields (§8.1) and the offset-curve and flex mechanisms, not as the plan.

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

## 9. Keep-apart rows (D381): chords from reflex bends (orchestrator design, 2026-11-21)

**Idea (the compaction step of topology-shape-metrics, made octilinear).** A face whose walk turns only one way (convex) and closes with positive lengths is a simple polygon: its walls cannot cross or touch except where they meet. So Slots refines every face into convex cells before the solve, by **chords**: one per reflex vertex, a straight dummy edge from the vertex into the face, along the extension of the piece that reaches the vertex, ending on a wall piece read from the face's turn sequence (its **front**). Each chord is a segment with a free length >= its gap; every chord is a row set written from the map and Shape's headings, no solution read (D381). The choice of front restricts lengths (some compact drawings are excluded), never feasibility: TSM's refinement trades the same way.

**9.1 The face walk at piece level.** `WalkFace` gives each face's stretches with the face on the left (H7). Expand each stretch into its Shape pieces (reversed with heading + 4 for a backward wall), add the box edge between port ends as a wall piece of the box side's heading (its length an expression in the port points and W, H), and give each vertex its turn t in eighths (the lifted difference of the headings either side; a node vertex between two legs of one node: the legs' difference, 0 at a cusp; a crossing vertex: the crossing's corner). With the face on the left the walk's turns sum to -8 (H7), so a **convex** vertex has t <= 0 and a **reflex** vertex t > 0.

**9.2 Where reflex vertices are.** Crossing corners and node corners are never reflex (every corner around a crossing is under 180 degrees; a node's corners are 0, 45, 135 or 180). Reflex vertices are the bends of tracks turning away from the face, and the two bends of a step on the face's far side (conditional: they exist only when the step does). A cusp's opening step gives one reflex bend on the leg (always present).

**9.3 The front of a reflex vertex v.** Headings count clockwise, so with the face on the left a convex corner turns left (t < 0) and a reflex corner turns right (t > 0); the extension of the piece entering v (heading h) points into the face. Walking on from v the wall heads right of the ray (c = cumulative lifted heading minus h starts at t(v) > 0, so the wall moves to the ray's right) and must come back across the ray's line moving leftward. **The front is the first piece e after v whose cumulative lifted c lies in [-3, -1]** (c = -2 is the orthogonal case: a wall square across the ray, facing v). Cumulative, not mod 8, so a wall that winds is counted right. Step-diagonal pieces count for c but are never fronts. A face walk that returns to v without such a piece: flag it (should not happen in a closed face, box walls included).
- Open item for the probe (riskiest part): the orthogonal front lemma (Tamassia 1987) does not carry over proven; G2 measures whether this rule's chords give zero clashes. If a chord's front is wrong the solve shows it as infeasibility or a clash at that vertex, named.

**9.4 Chord rows.** Chord from v with heading h, landing point R on front piece e: `R = v + L * STEP[h]`, `R = start(e) + u * STEP[h_e]`, `0 <= u <= len(e)` (len(e) the piece's length expression), `L >= gap(h, h_e)` with gap = `RungSpacing(h_e, h)` from planar_model (1 or 2 units: the tile clash rule's spacing). A conditional chord (from a step bend) has every row relaxed by M (1 - z) of its step's binary. A chord makes the refined cells convex at v; vertices it creates on e are convex on both sides (a chord meets e at a non-parallel heading).

**9.5 Bundles.** Two lanes of one channel bound a thin face; where they bend together the outer lane's bend is reflex for that face, its chord lands on the inner lane with L >= gap: the outer lane turns at least a lane later. This is the frame row of §2.1 written by the same rule; no separate neighbour family.

**9.6 Exempt:** decks (a crosser under a deck middle is not a wall of the deck's faces at the crossing: corners there are crossing vertices), and the two legs of one node within their first piece (they meet at the node by definition).

**9.7 Probe G2 (riskiest first, time-boxed):** chords on every face of T2's top 3 variants with G1's model as fixed by G1g; count clashes by group, chords whose front search failed, solve time. Pass: zero clashes; then T3, LL2.

## 10. Flex points (D383): how the drawing stretches

**Why.** In orthogonal TSM any shape can stretch along a cut: every piece crossing the cut is perpendicular and lengthens. In eight directions a diagonal crossing an x-cut also moves in y, so stretching needs an axis piece inserted in the diagonal. With steps as the only insert, coverage alone forced ~100 S-bend lanes on T2 (G1s/G1t) and the MILP found no point in 15 s.

**10.1 The promise (Shape).** Shape marks a flex point on every diagonal piece outside spans and outside crossing lattices (a crosser's stretch between two crossings). Today the probe marks every such piece; Shape's own rule for where (a cut per long lower bound crossed only by stretchable pieces) is a later refinement measured against this.

**10.2 The model (Slots).** A flex point on a piece of heading h (odd) is: part a, an axis insert of heading h+1 (2 per lane, 0..32 lanes), part b (diagonal; odd by the grid rows), an axis insert of heading h-1, part c. No binaries. Each insert is an axis piece for the grid (its cross coordinate odd along it, unconditional rows, D382's elimination covers them). Priced as length (2 per lane). The piece's least total applies to a + b + c.

**10.3 Not modelled (oracles).** No curve rows on the inserts' bends (unconditional rows would force every insert to the curve gap); the curve oracle counts breaches. Keep-apart rows (§9) must treat a used insert's bends as conditional reflex vertices, like a step's; until G2 measures it, chords from flex bends are not written.

**10.4 Measured (G1v-G1x, ng1 e9c24e3e49).** Full model, 15 s: T2 3/3 2.8 s 54 step lanes; T3 o7t0f1m3270 8.8 s 62, o7t0f0m3270 4.2 s 106, o7t2f0m3270 infeasible (lattice inside spans: a Structure promise gap). The remaining steps are sideways offsets of straight port and node legs (fan-outs, long diagonal approaches), not stretch.

## 11. Beside tracks as offset curves (orchestrator design, 2026-11-21 night; from G2e/G2f, ng1 708d5e3629)

**Why.** One beside equality per piece (§8.2) lets a copy or prio row and its parent each take their own steps and flex inserts, so they drift into each other (the B bundle's same-channel clashes, G2d). Rigid pairs (no sideways moves on shared pieces, equality at both ends) remove them (T2 without coverage: same-channel 25 -> 3, 25x29 tiles) but cannot stretch, so coverage makes T2 infeasible; rigid pairs with their own flex drift again (78 clashes).

**Model.** On the shared run (after the beside track's opening cusp, before its closing cusp), the beside track is the parent chain's offset curve at one lane on the map's side: it has the parent's element sequence (parts, steps, flex inserts, same headings), its own length variables, and every vertex row `beside vertex = parent vertex + m(a, b, s)`, where m is the miter: the intersection of the two offset lines of the headings a (before) and b (after) at one lane on side s. m depends on headings and side only (Shape's and the insert's fixed headings), so every row is written before the solve: valid by construction. The beside track takes no own step places or flex inserts there; it follows the parent's.
- m is symmetric in (a, b), so a zero-length parent insert gives a zero-length beside insert; part lengths differ from the parent's by constants (lower bounds where negative).
- Check before use: every m on the grid and of the right parity for the beside track's pieces; name any heading pair where it is not (do not fake).
- Coverage then stretches the pair together through the parent's flex.

