# SPEC: the Slots layer's hand-off (draft, 2026-11-20 night, orchestrator; for the engineer)

Third SSS hand-off spec (D368). Implements src/junction/planar/CLAUDE.md "Architecture" (Slots) and DESIGN-tracks §17.3. Written from Shape's output model and hand-on (wip/SPEC-shape.md §3 (1)-(6), §6), not from `planar_slots` code; code is mapped onto it afterwards (D368). Rulings it rests on: D313 (ports at the notation spacing, fans as late as possible), D349 (rows), D351 (parallelogram), D360 (Slots owns every sideways move, which leg stays straight), D362, D363 (port-side frames), D365 (no step in a deck, tunnel or ramp), D366, D367, D373(3), D375 (steps on landings). Corpus: RECON-corpus-crossings §3, RECON-corpus-sbend-crossings (of diagonal pass-unders: 42% true S-bends, 45% a corner's diagonal, 13% a doubled pair).

## 0. Two architecture checks, on paper, before any design

Lengths "decides every piece's length, as one solve with only lower bounds left" (CLAUDE). Two questions follow: where does every upper bound go, and can the layer that takes them always meet them?

### 0.1 Where every upper bound lives
Lengths' unknowns are piece lengths. A quantity is an **upper bound** on Lengths when it limits how far a piece may stretch, and **exact** when it fixes a length. Each one in the finished layout, and where it goes:

| # | Bound | What it limits | Owner |
|---|---|---|---|
| U1 | Deck span <= limit (bridge middles; tunnel length; D228a with a copy) | the separator's pieces between its first and last crossing | **Slots**: the crossers' positions along the separator are slots of a deck frame (§2.3); the span is a sum of slot gaps and exact diagonal widths; one row span <= `limit`. The separator's pieces inside the span are exact for Lengths. |
| U2 | Heads, portals, ramps | the separator's end runs | **Shape** (exact end runs, H5); Lengths reads them as constants. |
| U3 | Diagonal crossing pieces under a deck (a crosser's S-bend under the span, a corner's diagonal, a doubled pair) | how far the crosser travels along the separator under the span | **Slots** (Shape hand-on (4), (5)): a known sideways move whose size Slots fixes, counted in U1. |
| U4 | Port pins: a port's lines 1 apart; the two ports of a side gap + 1 apart (`ml_gap` on W/E, `branch_gap` on S) | sideways offsets at the box edge | **Slots** (port frames, D363). Pins are within a side only; nothing pins across sides (the box size is Lengths' objective). **Gap:** the gaps are spec rule fields, Structure's input; Slots may not read the spec (D366). Structure must hand them down (as it does `limit`): a Structure step, see Q3. |
| U5 | S-bend sizes (a slot change d is a diagonal piece of exact size) | the step's diagonal | **Slots**. Where the step sits along the track stays free (Lengths). |
| U6 | A copy exactly 1 from its line; the parallelogram (D351) | sideways offsets; the parallelogram's diagonal | **Slots** (exact offset; the move onto the copy is the crossing's, not a step). |
| U7 | Cusps: legs that leave or reach a node parallel | the legs' sideways opening | **Slots** (Shape hand-on (1)): the opening is a step of exact size. |
| U8 | Any other sideways move inside a frame: a diagonal piece Shape gave one track of a bundle and not its neighbours | the relative offset it makes | **Slots**: inside a frame every relative sideways move has an exact size (§0.3). |
| U9 | Landings between structures in series (D375) | lower bound only (>= the signal tile; odd lengths from signal spacing) | **Lengths** (a lower bound and a parity, no upper bound); the step on the landing is Slots' (U5). |
| U10 | Prio coverage, curve spacing, signal room | lower bounds only | **Lengths**. |
| U11 | Clearances between tracks that are not in one frame | lower bounds on separation (rungs) | **Lengths' spec** (not an upper bound on a length; it is a separation row whose feasibility Lengths' spec must show; out of this spec's scope, flagged). |

**Finding (a): non-parallel crossers under one deck put an upper bound in Lengths.** Two crossers of one structure that cross the separator at different headings are not parallel, so their distance along the separator is not a sideways offset in any frame: it changes with the lengths of their pieces on the side where they converge. Their gap is then a Lengths quantity, and U1 becomes an upper bound on Lengths (a D362 breach). Shape's capacity rows today allow it ("a change of angle between consecutive crossers starts a new run"). Fix (Q1, D376): Shape promises **H11: the crossers under one span are parallel at their crossings** (headings equal mod 4; one angle per span, so H6's width is k at 90 degrees or k + 1 on a diagonal). A doubled structure's two decks are two spans: they may take different angles, the crossers bending between them (an exact move, the decks being one lane apart). The corpus's three diagonal forms (S-bend, corner's diagonal, doubled pair) all cross in parallel bundles. The sweep (SX0) counts how many handed-down structures break H11 today.

**Finding (b): lines of one channel must stay parallel.** A frame's offsets are slots only while its tracks are parallel. Shape solves each base track separately; with no angle preference (D373, §2.1 cost) two lines of one channel can cross a structure at different angles at equal turning. Covered by H11 at structures; elsewhere a difference between bundle neighbours is a relative move inside a frame (U8), exact by Slots. The sweep (SX0, C1) counts channel pairs whose lines cross in different headings.

### 0.2 Slots stays always feasible once it carries them
Slots' rows (§2.2) are all **difference rows**: x_u - x_v >= g (order and gaps), x_u - x_v = c (copies, pins, node legs, no-step intervals), x_last - x_first <= L (spans). A system of difference rows is a network matrix: its LP is integral, and it is feasible exactly when its constraint graph has no negative cycle. Steps are free (a slot may change between consecutive intervals of a track, at a cost) except inside no-step zones. So a negative cycle can only run through the rigid rows, and there are three kinds:
- **F1 deck:** inside one deck frame no step is allowed, so the crossers' least packing must fit the span. Holds when Slots' least gaps inside a deck are exactly Shape's width model (k at 90 degrees, k + 1 for a diagonal run, copies and rows counted as crossers as P5 does) and H6 holds. **Promise needed from Shape: H6 with that width model, and H11** (one run per structure; with mixed runs the gap between runs is not a slot, finding (a)).
- **F2 pins:** a pinned section holds only port lines (only ports touch the box), in the ring order Structure's map gives (P1); the pinned gaps (1 within a port, gap + 1 between ports) are each at least the least gap 1. **Promise needed from Structure: the side gaps handed down (Q3), each at least 0.**
- **F3 rigid chains:** two rigid rows that meet with no steppable interval between them could disagree (a pin directly against a deck, a node inside a no-step zone, two decks whose no-step strips touch). Shape's H5 keeps nodes and other crossings off heads, spans and ramps; a port's end segment has at least 4 units before its first event (planar CLAUDE, layout program); two strips that touch belong to parallel separators over the same crossers, which give equal rows, never conflicting ones. No Shape promise is needed for port ends: the stretch from a port end to the first deck has a free length, so the room for a step there is a Lengths lower bound (corrected 2026-11-20: an earlier draft asked Shape to extend H5).
With F1-F3 every handed-down variant has a slot answer: the least packing everywhere, steps wherever pins or nodes need them. Slots never fails on a variant (L9); a variant it cannot slot is a missing promise above it (D361).

### 0.3 What makes stretching succeed (Slots' outer promise)
Slots' exact quantities are walls of fixed size inside faces. Lengths closes every face with lower bounds only when, between any two **exact sections** of a face (a pin section, a deck strip, a node point), each wall's sideways change is either all Slots-fixed or free in both directions. A face strip whose two walls run between two exact sections with sideways changes of one sign only (they only diverge, or only converge) would make Lengths solve an exact length. Slots' frames are therefore **the face strips between exact sections** (§2.1), and every sideways move inside a frame is Slots-fixed (U8). Whether this holds on every handed-down variant is the riskiest question in this layer: §7 X2 measures it first with the closure LP of SPEC-shape §7 H2, Slots' exact values plugged in.

## 1. Input
- One Shape variant (SPEC-shape §3, §6): Structure's variant (track map, sites, structures with `limit`), per track its heading list and event pieces, per crossing its angle, per structure its axis, least middles and end runs, Shape cost.
- **Added to Structure's output (Q3, orchestrator default):** per side of the box, its ports in ring order with the gap between consecutive ports (`ml_gap` on W and E, `branch_gap` on S, plus 1), as `limit` was added for Shape.
Nothing else. No lengths or positions exist yet; Slots never reads the spec, the topology text or a rendered layout.

## 2. What Slots decides (and nothing else)
Per variant, one slot answer:
1. **Slots:** every track's integer sideways position in every interval of every frame it belongs to, in lane spacings.
2. **Steps:** hence every S-bend: where a track's slot changes between consecutive intervals, its size (the exact diagonal) and side. Where along the stretch the step sits is Lengths'.
3. **Straight legs at forks and merges with no straight leg** (D360): which leg keeps the in line's slot.
4. **Cusp openings** (hand-on (1)): the step that opens parallel legs after a split or closes them before a merge.
5. **Deck positions:** each crosser's position along its structure's separator (hand-on (3)), hence the span in middle tiles, exact.
6. **Sizes of relative moves inside frames** (U8, hand-on (4)): each diagonal piece inside a frame strip that its neighbours lack gets an exact length.
7. **Port close-ups and fans** (D313, D363): the steps between a port's pinned section and the bundle inside.
8. **Landing steps** (D375): a separator and its copy may step between structures in series.

### 2.1 Frames
- An **exact section** is a cross-section where a set of parallel tracks has fixed relative positions: a port side's pinned section (U4); a structure's **deck strip** (its crossers under the span, plus the separator and its copy as the strip's walls); a node point (its legs on one point, U7).
- A **frame** is a face strip between two exact sections whose walls are parallel bundle tracks: a channel (its lines, copies and rows), a corridor of channels running side by side (several channels in one strip, any direction), a node group (a node's legs from the node to their next exact section), a port frame (from the pinned section to the next). Frames are read from Shape's model (headings, events, faces); Slots does not choose them.
- Inside a frame tracks are parallel except on pieces whose sideways size Slots fixes (U5-U8). Outside frames (open faces) sideways changes are free and Lengths' own.
- Every track is cut at its events into stretches (Structure's terms); a frame's **intervals** are the stretches between its exact sections and events; slots are per (track, interval).

### 2.2 Rules (the model)
- **Order:** in each interval the tracks lie in the order the map's sides give (Structure P2); neighbours at least 1 apart.
- **Copies and rows:** a copy exactly 1 from its line over its structure, on the side the map gives; the line ends on its copy's slot after a doubled crossing (D351; not a step). A row exactly 1 from its lane over its range (D373(3) leaves the gap to Slots; plainest first, Q4).
- **Decks:** each span (a separator's deck, and its copy's deck as a second span) is its own strip, its crossers parallel (D376); between a doubled pair's two strips a crosser's bend is an exact move. In a deck strip no track steps; the separator and its copy hold their slots across the span and ramps; each crosser's position along the separator is its slot in the strip, at least Shape's width apart (1 at 90 degrees; on a diagonal run each extra crosser 1, the run's first 2); `span = last - first + width` within `limit[kind]` for every allowed kind. A crosser may step under the span (hand-on (5)) only by the diagonal its Shape heading already gives; its width counts.
- **Nodes:** at a split every leg starts on the trunk's slot; at a merge every leg ends on the out line's slot; a straight leg keeps it; parallel legs (cusps) separate by a step on the side the map gives, after the node (before it at a merge). At a fork or a merge with no straight leg one binary picks which leg keeps the slot (D360; the only yes/no choice in the model).
- **Pins:** at each port side's section, a port's lines consecutive slots in port order; consecutive ports of a side their handed-down gap + 1 apart.
- **Port fans and close-ups** (D313, D378): a port frame's steps are anchored to their node end (in-bound fans just before the fray, out-bound close-ups just after the join); Lengths places them as near that node as curve spacing allows.
- **Landings** (D375): between structures in series the separator and its copy may step; a step there is ordinary.
- **Cost:** the sum of step sizes over every track (weight 1 per lane spacing), then, as a tie-break only, the sum of interval widths; in-bound fans late and out-bound close-ups early follow from D313 as a tie-break: the spread stretch costs more than the tight one (Q5 for the weight). Slots' cost is added below Structure's and Shape's for ranking (D366).

## 3. Output model (each variant), refining Shape's (D367)
- Shape's variant unchanged (Structure's variant, headings, event pieces, angles, structures' axes and end runs).
- Per frame: its kind, its exact sections, its intervals, and per interval its tracks left to right with slots (the frame's zero at its least slot).
- Per track and stretch: its steps in travel order (from slot, to slot, side), each with its exact diagonal size; the piece list refined by those steps (the steps inserted as exact pieces between the Shape pieces they sit among).
- Per structure: each crosser's position along the separator, the span in middle tiles (exact), and the separator's pieces inside the span (exact).
- Per piece of every track: **exact** (with its length) or **free** (Lengths' variable).
- Slots cost.
- **Hand-on to Lengths (named now so its spec starts from it):** (1) every exact piece is a constant, every free piece a variable with lower bounds that are Lengths' own (curve spacing, coverage, signal room, landings); (2) every face closes with lower bounds only (L8), so Lengths needs no upper bound and no binary; (3) clearances between tracks in different frames are Lengths' (U11), never Slots'; (4) where a step sits along its stretch is free; (5) landing lengths and their parity (D375) are Lengths'.

## 4. Promise (each a test at Slots' output, D367)
- L1 Refinement: Shape's output is unchanged, and H1-H9 (H11 once adopted) still hold on it.
- L2 Order and gaps: every interval's tracks are in the map's side order, neighbours at least 1 apart; copies and rows exactly 1 from their parent.
- L3 Nodes: legs on the trunk's slot at the node; a fork or merge with no straight leg has exactly one leg on it; cusps open only by a step.
- L4 No step inside a no-step zone (deck strip, heads, ramps, tunnel); the separator and its copy hold their slots across the span.
- L5 Spans: per structure and allowed kind, the span (positions plus widths) fits `limit[kind]`.
- L6 Pins: every port side's section holds its handed-down gaps exactly.
- L7 Frames close: every frame strip's sideways change between its exact sections equals the sum of its tracks' steps and fixed moves ("positions agree around every loop a pin closes").
- L8 Stretching succeeds: the lengths-only LP (Shape's headings, Slots' exact pieces as constants, every free piece at least 1, no clash rows) closes.
- L9 Every variant handed down is slotted. A variant with no slot answer is a bug: in Slots, or a condition missing from Shape's or Structure's promise (§0.2), which is then added there (D361).

## 5. Questions (settled: Q1 D376, Q2-Q6 D377; Q2 decided by X2's evidence)
- **Q1 SETTLED (D376): H11, one angle per structure (finding (a)):** Shape adds the promise that a structure's crossers are parallel at their crossings; its capacity rows drop the mixed-angle runs. Option B: allow an angle change only between crossers that meet at their next event on the converging side (a triangle whose sizes Slots can fix). Option C: keep mixed angles, and Lengths carries the span as an upper bound (a D362 breach). Recommendation: A (plainest; the corpus's diagonal forms are parallel bundles), toggle candidate B. SX0 measures how many handed-down structures it touches.
- **Q2 Frames as face strips between exact sections (§0.3, §2.1):** today's `planar_slots` frames are channels with intervals cut at crossings; the spec widens them to corridors and node groups (DESIGN-tracks §17.3 already lists both) and defines them from faces. Recommendation: adopt; X2 measures it.
- **Q3 Pins handed down by Structure:** Structure's output gains the side gaps (ml_gap, branch_gap) per port side, as `limit` was added for Shape. Recommendation: adopt (a D366 hand-down, no decision moves).
- **Q4 Row gap:** a row exactly 1 from its lane over its whole range (like a copy), or free between joins (D342 allows a gap). Recommendation: exactly 1 now, toggle candidate the free gap.
- **Q5 Steps on diagonal stretches:** today's composer refuses a slot step on a diagonal piece. Coop builds them (diagonal, axis, diagonal). Recommendation: allow; a step's size is exact either way.
- **Q6 Cost:** steps weighted 1 per lane spacing, widths as a tie-break, ranking lexicographic (Structure, Shape, Slots) as D373(2).

- **Q7 OPEN (for review, 2026-11-20 night): how Slots sizes its steps.** A step's needed size depends on the room the free lengths around it leave (a face's 2D closure), and faces share walls, so a face-by-face rule does not guarantee the whole closes; Lengths' real lower bounds (curve spacing, signal room, coverage) are larger than the probes' 1. Options: (A) lengths-free rules (port, node-group and deck frames with per-strip rows), the closure LP as an oracle only; (B) Slots picks integer steps in a program that also requires some lengths to exist, with Lengths' lower bounds handed down as rule fields; Slots decides steps only, Lengths re-solves lengths with its own objective; costs 0.3-2.8 s per variant as probed (X2f); (C) steps become Lengths' integer variables (Slots no longer owns sideways moves; Lengths no longer binary-free). Review: wip/REVIEW-slots-lengths.md.

## 6. Output types (target; code is mapped onto these, D368)
New header src/junction/planar/planar_slots_layer.h (name to settle against today's planar_slots.h when mapping):
- `PlanarSlotFrame { uint8_t kind; std::vector<std::vector<std::pair<uint32_t, int>>> intervals; }`: per interval its tracks left to right with slots (kind: channel, corridor, node group, port, deck).
- `PlanarSlotStep { uint32_t piece; int from; int to; }`: a step on a track, inserted before Shape's piece `piece`, from slot to slot (its size |to - from| is the exact diagonal).
- `PlanarSlotTrack { std::vector<PlanarSlotStep> steps; std::vector<int> exact; }`: per piece of the refined list, its exact length or -1 (free).
- `PlanarSlotStructure { std::vector<int> positions; int middles; }`: per crosser its position along the separator; the span in middle tiles.
- `PlanarSlotsVariant { PlanarShapeVariant shape; std::vector<PlanarSlotFrame> frames; std::vector<PlanarSlotTrack> tracks; std::vector<PlanarSlotStructure> structures; int cost; }`.
- `PlanarSlotsResult { std::vector<PlanarSlotsVariant> variants; std::string why; }`, ranked by (Structure cost, Shape cost, Slots cost, tag).
- `PlanarSlotsResult BuildPlanarSlots(PlanarShapeResult shape)` (renames against today's `BuildPlanarSlots` when mapping).
- `std::vector<std::string> CheckPlanarSlotsPromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarSlotsVariant &v)`: L1-L8 as a test oracle, one message per breach. L9 is a property of `BuildPlanarSlots`.

## 7. Build order (each step one brief, test-first at Slots' output)
- **SX0 done (wt-shape 15f22f629e, probe `[.probe]`):** keep 16 on T1, T2, T3, LL2: both oracles clean, every variant shapes and closes (Shape 0.14 / 0.53 / 1.1 / 2.6 s). **H11 is broken widely today:** structures with mixed crossing angles T1 16/16, T2 48/80, T3 67/158, LL2 32/64 (e.g. T2 `o3t2f1m150` structure 2: headings 0 4 4 7 7 3, an axis run then a diagonal run). C1 as defined over-counts (it pools a crosser's headings over all its crossings with one channel, where a turn between them is legal); C2 is the measure that matters. **LL3a: Structure hands down 0 variants** (cause not yet read). Corpus hubs as topologies are a later widening (none are in the planar notation yet).
- **SX1 done (wt-shape 50a5da66e8, `PLANAR_H11`, off by default):** with a structure's crossers parallel, every T1, T2, T3, LL2 variant still shapes, keeps H1-H9 and closes; mixed-angle structures 0. Turning summed over 16 variants: T1 64 -> 80, T2 160 -> 256, T3 364 -> 405, LL2 64 -> 112 (about +1 to +6 eighths per variant). Shape time T2 0.52 -> 0.77 s, T3 1.1 -> 1.8 s, LL2 2.5 -> 1.75 s. Flag-off numbers equal SX0's. **LL3a:** all 38 Structure combinations skipped, "crossing F1 x E2 in node group 0 has no join or fray site": the three-lane lane-paired fray (EF1..3 -> E1, F1, E2, F2, E3, F3) has crossings inside its node group that Structure's site model does not classify. A Structure gap, outside Slots; for its own step.
- X0a Structure hands down the side gaps (Q3, D377): `PlanarStructureVariant::side_gap`, oracle P8.
- X0b Shape: H11 per span (D376) with its oracle row; a mirrored crossing under a copy's deck takes its own angle and the crosser may bend between the two decks; capacity per span; `PLANAR_H11` retires.
- **X0a done** (wt-slots 7c077f7661): `side_gap`, P8. **X0b done** (d95eb6bd18): H11 per span, copy decks own angle, `copy_middles`; Shape slower (LL2 8.8 s, T3 2.3 s at keep 16; a speed step is owed).
- **X2a-X2d (probes, wt-slots c63cec9015..82021b2ed3):** the closure LP with Slots' least exact answer and NO steps (cusps one lane, deck crossers one tile apart, port lanes pinned) closes 0/64; by family, pins alone break every T2 and T3 variant: a port's lanes pinned one lane apart cannot let their fray's legs cross without a fan (D313), and the LP had no steps. With one free sideways step per stretch (X2d) every combination closes 64/64: a Slots answer exists for every variant. **X2e (c0f7a0e3ae):** whole-lane steps (axis 2, diagonal 1 half-tile unit per lane), none on a separator or copy inside its span, with every exact row: closes 64/64 (joint MILP up to 11 s on LL2: a probe, not Slots' method). So Slots always has an answer (L9 is reachable); Slots' design job is to pick those steps from Shape's model without lengths: X2f tests frames as face strips (Q2) by fixing the steps Slots' rule picks and re-running the closure.
- **X2f (5334fc337e):** the fewest whole-lane steps that close: T1 2 and T2 3 steps per variant, all at ports (in-port fans port->split, out-port close-ups merge->port: D313); T3 16-27 and LL2 16-24, mostly in node groups (split->crossing, crossing->merge, merge->merge) plus port fans; channel stretches between two crossings rarely step. Corpus (RECON-corpus-steps): 52% of coop steps at ports, 19% near switches (3-6 tiles off), 19% mid-run, 7% under spans, 2% on landings.
- X1 **Oracle:** `CheckPlanarSlotsPromise` driven by hand variants (two lines crossing under one bridge; one fray; one port side), one failing test per promise.
- X2 **Riskiest first, time-boxed:** frames from face strips (§2.1) and the least slot answer (least packing, steps only where pins and nodes need them) on every handed-down variant; count L8 closes, and per failure the face and its exact sections.
- X3 The slot program (difference rows, the fork binary, cost), `BuildPlanarSlots`.
- X4 **Outer test:** `BuildPlanarSlots` on T2, T3, LL2 (keep 16): every variant slotted, L1-L9 kept, ranking, time per variant (the seed budget is 5 s, D297; Shape alone already takes 2.8 s on LL2 at keep 16, so Slots must be milliseconds per variant).
