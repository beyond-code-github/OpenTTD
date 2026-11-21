# SPEC: the Shape layer's hand-off (draft, 2026-11-20, orchestrator; for the engineer)

Second SSS hand-off spec (D368). Implements src/junction/planar/CLAUDE.md "Architecture" (Shape) and DESIGN-tracks §17.2. Written from the contracts, the track map and wip/SPEC-structure.md, not from the code; code is mapped onto it afterwards (D368). Rulings it rests on: D340 (shape program: faces turn once, least turning), D342 (crossers may bend), D349 (a row follows its lane as an offset copy through bends), D351 (parallelogram), D360 (no fork, merge or spread diagonals; Slots owns sideways moves), D362, D364-D372, D228a, D346. Game facts: `GetTunnelBridgeLength` counts middle tiles for both kinds; `CheckBridgeAvailability` caps a bridge at min(type max_length, max_bridge_length); `CmdBuildTunnel` caps middles at max_tunnel_length.

## 0. The first questions, answered from the contracts
- **Rows (and copies): yes, Shape outputs them, but decides nothing for them.** Slots needs every physical track headed (D366: it reads only Shape's model), and a structure's capacity counts them (they are crossers, S2c1 ruling). Their headings are derived: a row or copy has its parent's headings over its range (D349 for rows; a copy sits on its separator's axis piece, D351). A mirrored crossing sits on the same piece of the crosser as the original. So Shape solves on the **base tracks** only (topology lines and their parts) and copies headings to beside tracks: the model stays the size of the topology.
- **Faces and sides: yes, Shape reads both; Structure already promises them (P1, P2).** Sides give: the arrival side s of each crossing (the crossing rule below), each non-straight leg's turn side at a node, and the order of parallel legs (which Slots keeps). Faces give the face rule: every inner face winds exactly once (D340), the one heading-only condition that stops Lengths from meeting a shape that cannot close. Shape does not decide sides or faces; it never changes them (D367).

## 1. Input
- One Structure variant (SPEC-structure §3, §6): the track map (tracks, ends, events in travel order, faces, sides per stretch pair, the expanded topology with port headings), sites (straight leg and side), structures (separator, crossings, crossers, allowed kinds, copy), Structure cost.
- **Added to Structure's output (D366, orchestrator default):** each structure's limit per allowed kind in middle tiles: bridge min(longest bridge, and for a main-line separator 2d + tl - 2 from D228a, head to head B = middles + 2); tunnel min(max_tunnel_length, the same D228a bound for a main line, portal to portal). Structure computes these for P5 already; Shape must not re-read the spec's rule fields (they are Structure's input).
Nothing else. No lengths or positions exist yet.

## 2. What Shape decides (and nothing else)
Per variant, one shape (variants live only in Structure, D362; D340's "several shapes per variant" is superseded):
1. **Headings:** each base track's absolute heading at every point (its ends and each event), as a lifted integer (eighths, not reduced mod 8). A stretch turns **monotonically** from its start heading to its end heading, one eighth per piece. A turn and turn back between two events is a sideways move, so it is never Shape's: it is an S-bend, which belongs to Slots (D360, CLAUDE "makes no sideways moves").
2. **Crossing angles:** each crossing's angle a in 1..3 eighths, which follows from the two headings at the event.
3. **Each structure's axis:** the one axis heading its separator holds from first to last crossing.
Derived, not decided: beside tracks' headings; per structure the least middle tiles at the chosen angles, and the straight runs its ends need (§3).

### 2.1 Rules (the model)
- **Ports:** a port track's heading at its port end is the port's heading.
- **Nodes:** at a split with in-heading h, the straight leg starts at h; every other leg starts at h or h + its side (one eighth toward its side, never away), its side read against the straight leg (a row's split and merge, and the three-leg merge where a prio lane, its row and the joiner end, give several other legs). A fork's legs each start at h or h + their own side; which one keeps the in line's slot is Slots' (D360). Merges mirror this at their end. Legs that start parallel are ordered by their sides; Slots separates them.
- **Crossings:** at a crossing of separator p and crosser c arriving from side s: H_c - H_p = s*a + 8q, a in 1..3 (D340's rule, with separator for "over"). The separator's heading is an axis heading. Inside a structure, the separator's stretches between its crossings have turn 0. Two separators crossing (a stack, or a bridge over a tunnel line, P3/P4) are both axis there, so a = 2.
- **Mirrored crossings:** a copy's or row's k-th crossing mirrors its parent's k-th crossing (Structure's contract: beside tracks' crossings follow by rule from their parent's structure, so they are in no structure's list). It takes the parent's role: beside a separator it is a parallel deck of that structure (as a copy is); beside a crosser it is spanned by the same separator. Its angle equals the original's, and the crosser sits on one piece across both crossings; except under a copy's deck (D376, 2026-11-20): a crossing on a structure's copy is a second span, with its own angle and crossing rule against the copy, and the crosser may bend between the two decks.
- **Faces:** each inner face's walk winds exactly once (its stretch turns plus its corner turns sum to 8 eighths; a corner between parallel legs at a node is a cusp, 4 eighths, oriented by the sides); the outer face winds once the other way.
- **No U-turn:** along every route path the lifted heading stays within 3 eighths of the entry port's heading.
- **Capacity (per structure, for every kind in its allowed set):** the middles needed at the chosen angles fit the kind's limit. Width along the separator (from track pieces): k crossers in a row at 90 degrees take k tiles; k consecutive crossers at the same 45 or 135 degree angle take k + 1 tiles (a diagonal crosses an axis row in two half pieces, and two neighbouring diagonals share a tile); a change of angle between consecutive crossers starts a new run. Structure's P5 guarantees every structure fits at 90 degrees, so a capacity row can always be met.
- **Ends (tunnels on paper, D365):** beyond its outermost crossings the separator holds its axis for the heads: a bridge's head tile each end; a tunnel's portal and ramp tile each end (it dives one level on flat ground; crossers stay at grade and may bend over it, D342). An "either" structure reserves the tunnel's ends (D366). No node or other crossing lies on a head, portal or ramp. A crossing of a bridge separator with a tunnel separator lies on middle tiles of both.
- **Cost:** least total turning (sum of |turn| over stretches, D340); no angle preference among equal turning (corpus, RECON-corpus-crossings §3: under bridges 61% of crossed tracks pass diagonally as S-bends and 37% perpendicular; over tunnels 67% right angle), then a stated deterministic order. (Corrected 2026-11-20 at the engineer's prompt; the first draft preferred 90 degrees. H2 still carries the |a - 2| term; H3 removes it.) Shape's cost is added below Structure's for ranking (D366).

## 3. Output model (each variant), refining Structure's (D367)
- Structure's variant unchanged (map, sites, structures, kinds, cost).
- Per track (beside tracks included): heading at each point; hence its piece list (stretch by stretch) and the piece each event sits on.
- Per crossing: its angle.
- Per structure: its axis; least middle tiles per allowed kind at the chosen angles; end runs per allowed kind (bridge: head; tunnel: portal + ramp); its no-event zone as an event range on the separator.
- Shape cost (turning).
- **Hand-on to Slots (named now so its spec starts from it):** (1) cusps: legs that start or end parallel at a node, which only a sideways move separates; (2) each structure's no-step zone (span, heads, and for a tunnel the ramps); (3) under each structure, the crossers' spread across its separator is an upper bound Slots must keep (crossers' positions along a separator are their slots in their own bundle), so their summed spacing stays within the limit; (4) diagonal crossing pieces move a crosser sideways by their length: a known move Slots accounts for, not a step it adds. (5) Crossers' S-bends under a deck are the coop norm (corpus: 61% of tracks under bridges, 76% of crossing bridges), so "no step inside a deck" binds the separator only; a crosser may step under the span, and its widened footprint counts against the limit in (3). (6) Between structures in series on one separator (a split long bridge or tunnel), the separator and its copy may step on the landing (D375); the landing is at least the 1 signal tile, odd lengths preferred (a Lengths matter).

## 4. Promise (each a test at Shape's output, D367)
- H1 Headings are continuous: a stretch turns monotonically one eighth per piece; each event's heading is the same on both stretches it joins.
- H2 Port headings as authored.
- H3 Node rule: straight leg keeps h; the other leg (each fork leg) h or one eighth to its side; merges mirrored.
- H4 Crossing rule: separator axis at each crossing; crosser not parallel; arrival side as the map's sides; separator x separator at 90 degrees.
- H5 Structures: each separator straight on one axis piece from its first to its last crossing, with its end runs per allowed kind free of other events.
- H6 Capacity: per structure and per allowed kind, the least middles at the chosen angles fit the limit Structure handed down.
- H7 Faces wind once; no U-turn bound on every route path.
- H8 Beside tracks: a copy's or row's headings equal its parent's over its range; mirrored crossings on the crosser's same piece.
- H9 Refinement: Structure's output is unchanged, and P1-P8 still hold on it.
- H11 (D376, per run since D396): a span's runs are its maximal groups of consecutive crossers (in deck order) with headings equal mod 4; a run of k crossers is k tiles wide at 90 degrees and k + 1 on a diagonal; a span's least middles are n plus one per diagonal run (H6); the gap between runs is a Slots length.
- H10 Every variant handed down is shaped. A variant with no shape is a bug: either in Shape, or a condition missing from Structure's promise, which is then added there (never a skip, D361).

## 5. Settled (D373)
- Q1 **Closure:** Tamassia's theorem (valid angles imply a drawing) covers orthogonal drawings only; for 8 directions with positive lengths it is not guaranteed. Shape owns winding (H7); whether a winding-valid shape always closes once Slots opens the cusps is probed first (§7 H2) with a lengths-only LP oracle (lower bounds 1, no clash rows). A missing condition goes into Shape's model, not a later layer.
- Q2 **Ranking:** Structure cost, then Shape cost (lexicographic).
- Q3 **Rows:** a row takes its lane's headings over its whole range. D342's other freedoms are not Shape's: a gap between row and lane is for Slots' spec; a row through a tunnel or over a bridge is Structure's. Toggle candidate: row headings of its own between joins.

## 6. Output types (target; code is mapped onto these, D368)
New header src/junction/planar/planar_shape_layer.h (name to settle against today's planar_shape.h when mapping):
- `PlanarStructure` gains `std::array<int, 2> limit;` (max middle tiles, index 0 bridge, 1 tunnel; -1 when that kind is not allowed). A Structure step, briefed against SPEC-structure §3.
- `PlanarShapeTrack { std::vector<int> pieces; std::vector<uint32_t> event_piece; }`: the track's heading list (one lifted heading per piece, eighths clockwise from north, consecutive pieces one eighth apart) and, per entry of the map's `events[track]`, the piece that crossing sits on (CLAUDE Terms: Shape = heading list + crossing pieces). The heading at a point is the heading of the piece it sits on; stretch k runs from piece event_piece[k-1] (0 for k = 0) to event_piece[k] (the last piece for the last stretch).
- `PlanarShapeStructure { uint8_t axis; std::array<int, 2> middles; std::array<int, 2> end_run; std::array<int, 2> copy_middles; }`: least middles and end run tiles per kind (-1 not allowed); `copy_middles` the copy deck's (D376).
- `PlanarShapeVariant { PlanarStructureVariant structure; std::vector<PlanarShapeTrack> tracks; std::vector<uint8_t> angles; std::vector<PlanarShapeStructure> structures; int cost; }` (angles per map crossing).
- `PlanarShapeResult { std::vector<PlanarShapeVariant> variants; std::string why; }`, ranked per D373(2).
- `PlanarShapeResult BuildPlanarShape(PlanarStructureResult structure)`.
- `std::vector<std::string> CheckPlanarShapePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarShapeVariant &v)`: H1-H9 as a test oracle, one message per breach (topology and spec only for H9's P1-P7 and H6's limits; the oracle is a test tool, not a layer). H10 is a property of `BuildPlanarShape`, tested in H4.
Until tunnels are built (D365): kinds always bridge; the tunnel rows and end runs have hand-structure tests written when D365's build starts.

## 7. Build order (each step one brief, test-first at Shape's output)
- H1 **Oracle:** `CheckPlanarShapePromise` driven by two hand variants (two lines crossing; one fray), one failing test per promise (each mutation breaks only that promise). No characterisation of today's code: `BuildPlanarLineHeadings` needs channel paths (`BuildPlanarPaths`), which Structure's variants do not carry (D366), so it cannot run on them; H2's ILP is judged by the oracle instead, and today's `SolvePlanarShape` is mapped onto H2 as its starting code.
- H2 **Riskiest first (Q1), time-boxed:** the heading ILP of §2.1 without capacity on every handed-down variant: count shaped, unshaped (H10), and closure under the lengths-only LP with cusps opened by one slot; time per variant.
- H3 Capacity rows and end runs, Structure's `limit`, cost and ranking; `BuildPlanarShape`.
- H4 **Outer test:** `BuildPlanarShape` on T2, T3, LL2 (keep 16): every variant shaped, H1-H10 kept, ranking order, time (milliseconds per variant; the seed budget is 5 s, D297).
- **Built (2026-11-20):** H1 oracle (c1f326dfd8, 38c46d9d21); H2 heading program and closure probe (7b0e4255d5); H2b parent chain, probe opens cusps (bdff7e9913, b69d8e57cf); H3 Structure `limit` (8e194c4c5b), capacity rows and `BuildPlanarShape` with the outer test (cb19e03bd5). Findings on the way: the parser needs nodes for any crossing (no port-to-port line); a row's parent is its lane's part chain; the first closure LP failed only because it put parallel legs on one point (Slots' cusp). Result: every T2, T3, LL2 variant (keep 16) shapes, keeps H1-H9 and closes.

## 8. Faces Slots can draw: the guarantee behind D361 (DRAFT, orchestrator, 2026-11-21 night; for the engineer)

**Scope under SSS v2 (D388):** this section now applies to free faces only (faces between bundles: spines' outer lanes, gadget hulls, the box). Faces inside bundles and gadgets are rigid (lanes as offset curves, catalogue gadgets) and get no cells; a dummy lands only on a physical wall, never on another dummy (DESIGN-sss-v2 §3.2, H16). §8.7's runtime certificate (D386 (1)) is superseded: the feasibility program is a test oracle (D388 b).

**Why this section.** Slots (D379) decides every length on Shape's headings, and D361 asks that its output be valid by construction. That needs two statements, proved before Slots' rows are built (D368):
- **G-planar:** any lengths that satisfy Slots' rows give a planar drawing of Structure's map with tile clearances (no clash).
- **G-feasible:** Slots' rows always have a solution on a variant Shape hands down (Slots never discovers infeasibility, D361).

H7 (each face winds once) is TSM's validity condition. In orthogonal drawing it is enough: every valid orthogonal representation refines into rectangles and compacts (Tamassia). In eight directions it is not: G2 showed a ray's landing wall depends on lengths, and faces with fixed headings can fold or collapse. Slots G1-G2 tried to close the gap from below with row families read off clash counts; that is the generate-and-check D361 rules out. This section closes it from above.

### 8.1 Proposed promise: convex cells (H12)
Shape refines every inner face into **convex cells** by dummy edges and hands them down:
- A **dummy edge** runs from a reflex vertex into the face along a heading Shape chooses (one of the headings that splits the reflex angle into two convex ones) and ends on a wall **Shape names**. Choosing the landing wall is a combinatorial decision on headings only, the analogue of TSM's rectangularization, so it is Shape's (it owns headings and decides without lengths). Step and flex bends are Slots' and are not reflex in Shape's model (they live inside a piece); see 8.4.
- **H12:** every cell's walk turns one way only (each corner turns <= 0 with the cell on the left) and winds once.

### 8.2 Slots' rows per cell (complete, no choice left)
- **Closure** of each cell (its walls' lengths times their headings sum to zero), the dummy edge's landing point on its named wall (0 <= u <= the wall's length).
- **Clearance:** for each track wall e of the cell and each track wall f whose heading faces e (3 to 5 eighths from e's, walked), the distance between their lines along e's normal is the sum of the lengths of the walls between them times their normal components: linear, with no choice, because in a convex cell the order of walls fixes which faces which. Row: that distance >= the clearance for the pair's headings (`RungSpacing`). Exempt: e and f adjacent through a corner where they legitimately meet (a node, a crossing).

### 8.3 Proof sketch of G-planar
1. A closed walk whose heading turns monotonically through exactly one turn, with lengths >= 0, is a convex polygon (possibly with zero-length walls): it cannot self-intersect.
2. The cells of a face tile it: every dummy edge is a segment inside the face from a vertex to a point on a named wall (its landing rows), so the cells' union is the face and their interiors are disjoint.
3. Faces tile the box (the map is planar and every face winds once, H7, with the outer face the other way).
4. Clearance rows keep facing walls of each cell apart; walls of different cells are separated by the cells between them. Hence no two track pieces share a tile except where the map says they meet.
- **Open in the proof:** (a) whether "facing walls" (3-5 eighths) covers every pair the tile clash rule cares about, including pairs meeting at 45 degrees across a short wall; (b) degenerate cells (a wall of length 0 making two cells touch).
- **Checks for the paper proof (from the engineer's Gemini review, 2026-11-21):** (c) no dummy edge may land on a zero-length insert (flex or step piece) alone: its landing wall must be a physical wall, or collapsing the insert to 0 pinches the walls either side; (d) per interior corner angle of a convex cell (45, 90, 135 degrees), show that facing-wall separation plus the leg minimums implies the tile clash spacing (1-2 half tiles) on all four normals (x, y, x+y, x-y), not only the facing wall's own.

### 8.4 What this does to steps, flex and beside tracks
- A Slots step or flex insert adds bends inside one of Shape's pieces. With Shape's cells fixed, such bends make the cell non-convex. Two readings to decide: (i) steps and flex inserts stay inside frames whose cells are synchronized corridors (beside offset curves, SPEC-slots-lengths §11), where the corridor is a pair of parallel walls whose convexity holds piecewise; (ii) Shape places the bends (a flex point or a step is a pair of Shape headings with length >= 0, D383's "Shape marks flex points" read literally), so every bend is in Shape's model and convexity covers it.
- Beside tracks: SPEC-slots-lengths §11 (offset curves) is a clearance construction for the thin cell between a track and its row or copy; it fits 8.2 as the corridor case.

### 8.5 G-feasible (the harder half, open)
Every Slots row above is homogeneous (closure, landing, clearance as ">= c" with c > 0 scales away), except the **exact quantities**: port pins and side gaps, deck gaps and spans, beside offsets (one lane), exact prio length (D384), node and join shapes. Scaling all lengths of a drawing up keeps every homogeneous row and every lower bound, so G-feasible reduces to SPEC-slots §0.3's question: **the exact quantities sit in rigid zones, and every cut between two rigid zones crosses pieces that can stretch** (axis pieces along the cut's normal, flex points on diagonals). That is a statement about Shape's cells and Structure's zones; to be written as a promise once 8.1-8.4 are settled.

### 8.6 Decisions for the engineer (card)
- Shape gains a decision (dummy edges and their landing walls) and a promise (H12, convex cells).
- 8.4: steps and flex inserts as Slots-only bends inside corridors (i), or as Shape-placed bend pairs (ii).
- Order: prove 8.3's open points and 8.5 on paper (T2 by hand, then the corpus) before any Slots code.

### 8.7 H12b: how Shape chooses dummies, and why realizability is the test (DRAFT, orchestrator, 2026-11-21 night)
**Correction to 8.1's framing.** A dummy edge need not "land on its wall for all lengths". As in TSM, it is a refinement that the next layer enforces (Slots writes its landing rows, which restrict lengths). What must hold is that the refined representation can still be drawn, with Slots' exact quantities: that is G-feasible (8.5), and it is a property of Shape's choice.

**Facts (geometry, not probe).**
- A convex cell with fixed headings and k walls is drawable on its own and has k - 2 degrees of freedom (scale included).
- Triangles (k = 3) are rigid: their side ratios are fixed by the headings. In orthogonal TSM every cell is a rectangle (2 degrees), and a rectangular subdivision is always drawable. In eight directions a cycle of glued triangles (or other low-freedom cells) can force contradictory scales, so convex cells alone do NOT imply G-feasible.
- Whether a refinement is drawable is decided exactly by a linear system: per cell closure (sum of lengths times headings = 0), every wall length >= 1 (homogeneous rows scale, so >= 1 loses nothing), each dummy's landing point within its wall, plus the exact quantities Structure and Shape already know (port pins and side gaps, deck gaps and spans, beside offsets of one lane, D384's exact prio length, node and join shapes).

**Proposal (H12b, H13).**
- **H12b rule:** at each reflex bend (in heading h, out h + 1, face on the left), candidates are d in {h, h-1, h-2, h-3} (the four headings that split 225 degrees into two convex angles), each with the walls its ray can face in the cell (c = wall heading - d in [-3, -1], walked); a dummy that lands on a box edge or on a wall of a longer free run is preferred, triangles avoided where a candidate gives a cell of 4+ walls. Shape picks the first candidate in that order.
- **H13 (new promise, its test): the refined representation is realizable** by the linear system above. Shape checks it with one LP per variant (no lengths are handed down; the LP is Shape's certificate of its own decision, tested at its output per D367). If it fails, Shape tries the next candidate at the vertices the infeasible rows name (a finite search inside Shape's own decision); if none certifies, the variant is refused with the named rows, which is a missing Structure promise (D361, never a skip).
- RULED D386 (yes to both card items). Rule order made exact for H12b: d in the order h, h-3, h-1, h-2 (lifted); landing = the first wall after v in the cell walk with c in [-3, -1]; take the first candidate whose two cells both have 4+ walls, else the first valid one. On the fray this picks d = east onto the E box edge (two 4-wall cells), the hand dummy of H12a.
- First parity check (engineer's Gemini review): on T2, whether any rigid cell (triangle) touches an exact one-lane offset; if yes, H13 carries the forced parities.
- Open: (a) integrality and grid parity (D382) of a realization: the LP is over the reals; scaling makes it rational and integral, but parity needs its own argument (likely: scale by 2 then shift odd as D382's forced parities allow); (b) clearance (8.3 c, d) is a separate per-cell condition.

**Card for the engineer.**
1. May Shape run an LP to certify its own refinement (H13)? It hands no lengths down; Slots still decides every length. Reading of D366/D367: a layer testing its own promise. Alternative: no LP, only combinatorial rules (avoid rigid cells by rule), with no proof of G-feasible.
2. The fallback search (try other dummy candidates where the LP names rows) is a search inside Shape's own decision; acceptable under D361, or must the first rule be proved sufficient?

### 8.8 A flex step needs no dummy: the proof (D407, orchestrator, 2026-11-22)
Amends 8.1-8.4 for flex pairs; settles 8.4 as reading (ii) for flex: the bends are Shape's (D397), and they are no corners of a cell.

**Setting.** A free one-piece stretch at heading h carries its flex pairs as pieces h, h-1, h, h+1, h (`PlanarShapeTrack::flex` lists the middles, the *inserts*). Slots sizes each insert >= 0. Every insert is a wall of the cell it bounds.

**Definitions (per cell, walked with the cell on the left).**
- *Reduced walk:* the cell's walls with every insert removed and its turn added to the wall before it. A wall before an insert and the wall after it have equal headings, so they meet with turn 0.
- *Side:* a maximal run of walls joined at turn-0 corners of the reduced walk, with the inserts between them. So a side is a, insert, b, insert, c (one stretch's pairs), plus today's straight-corner joining (two walls of equal walked heading meeting at a corner).
- *Lines of a side* at heading h: the lines of its non-insert walls (a, b, c). An insert has no line of its own for clearance.
- *Inner line* of side S: the line of S nearest the cell's interior. The *strip* of S: the band between its innermost and outermost lines.
- A dummy is a side of one wall and one line, and it is no track.

**The rule.**
- **H12 (amended):** every cell's *reduced* walk turns one way and winds once. Shape refines on the reduced walk, so a flex bend is no corner and gets no dummy; a dummy never starts at a flex bend and never lands on an insert (8.3 (c)).
- **Rows:** every vertex k of a cell (the corner after wall k) keeps `max(RungSpacing(h_e, h_k), RungSpacing(h_e, h_k+1))` from the line of every non-insert track wall e of every other side, except the sides of walls k and k+1; and keeps 0 from the line of every dummy of the cell except a dummy whose end it is. A vertex between two walls that are no track has no rows (as today). Insert vertices are vertices like any other.

**Lemma 1 (strip).** Every point of S lies in its strip, so on the outer side of its inner line (closed).
*Proof.* a's and b's lines are parallel at heading h; an insert of length s >= 0 runs from a's end to b's start, so every point of it is a convex combination of points on a's line and b's line. Every non-insert wall of S lies on one of S's lines. All lie between the innermost and outermost lines.

**Lemma 2 (tile separation across a grid line).** Let L be a line of heading h on the tile grid (an axis line on tile middles, a diagonal line through tile-edge midpoints), A a set of pieces with every point on one closed side of L, and J a piece of heading g whose two end points lie at least `RungSpacing(h, g)` from L on the other side (in `Dot(Right(h), .)` units). Then J shares no tile with a piece of A, except opposite corners, which the tile rule allows.
*Proof (half tiles; tiles are [2i, 2i+2] x [2j, 2j+2]; axis pieces run on middles between edges, diagonal pieces between edge midpoints of one tile, a corner each).*
- h axis, say north, L at x = X (X odd), A at x <= X. Every piece of A lies in a tile column whose middle is <= X. J: a north-south piece needs 2 (x >= X + 2, odd, so column X + 2 or more); an east-west or diagonal piece needs 1 (x >= X + 1, so its tile is column X + 2 or more: an east-west piece runs between even x, a diagonal piece spans x in [X + 1, X + 2]). Disjoint columns.
- h diagonal, say north-east, normal (1, 1), L at x + y = C (C odd), A at x + y <= C, J at x + y >= C + 2 (`RungSpacing` is 2 for a diagonal target). A tile with middle (m, n) holds points up to x + y = m + n + 2. Its axis pieces and its north-east, south-west and south-east corners reach x + y = m + n + 1; if one of them is A's, m + n + 1 <= C, so the tile holds no point at x + y >= C + 2 and no piece of J. Its north-west corner reaches only m + n - 1; the only piece of that tile lying wholly at x + y >= m + n + 1 is the south-east corner, the opposite corner. Disjoint, or opposite corners.
- The other headings are these two turned. The spacing used is the same quantity today's rows use (`RungSpacing(h, own)` on L's normal), whatever the heading of A's pieces: **a diagonal insert beside an axis side, or an axis insert beside a diagonal side, needs no extra row** (the units check of CARD-E5-flexstep). Lemma 2 assumes the grid (D334); the drawing program does not yet hold grid parity (D382), as for every row today.

**Theorem (G-planar with flex, per cell).** In a drawing that keeps the rows, the closure of each cell and every length >= 0, no two non-adjacent sides of a cell share a tile (opposite corners aside), and no vertex of a cell lies beyond one of its dummies.
*Proof.*
1. Take sides S and T, not adjacent. Every vertex of T keeps the spacing from every line of S, hence from S's inner line, on the interior side. Each piece of T has both end points there with its own heading among the two at each end, so each piece meets Lemma 2's condition for J. Every point of S lies on the outer side of S's inner line (Lemma 1). Lemma 2 with L = S's inner line, A = S: no shared tile. Inserts are covered without a row of their own, because they lie in the strip.
2. Adjacent sides S and R share one corner vertex c. Every other vertex of S keeps rows against every line of R and every other vertex of R against every line of S; only c is exempt. So step 1 holds for S minus its last wall against R minus its first, and the two walls at c meet at c as today (D400's node proof, crossing corners at the crossing point).
3. **End caps.** A step's inner vertices (a/insert and insert/b) are vertices of their side S, not of the corner, so they keep rows against every line of both adjacent sides. A step whose b would reach or cross the next side's line (q at or beyond R's line, e.g. b of length 0 before a turning corner) breaks q's row, and a vertex of the adjacent side inside S's pocket (between a's and b's lines) breaks its row against S's inner line. Both are refused by rows, with no extra family.
4. Dummies: a vertex keeps 0 from each dummy line of its cell, so each cell stays on its side of its dummies and the cells tile the face (8.3 step 2) even when an inward step points at a dummy. Rows give no spacing across a dummy (8.3 step 4 is unproved across a dummy, see Open).
5. Convexity: the reduced walk turns one way and closes (H12 amended), so the sides' headings are a convex sequence; with every vertex inside every other side's lines, the inner lines' half-planes contain every other side, and no side folds back over another.

**Cost, stated.**
- Conservative: no track may enter the pocket an outward step leaves (between a's and b's lines on the outer side is S's own; the inner pocket is closed to other sides by their rows against the outer line too). Toggle candidate if a hub needs it.
- b (and a) before a turning corner must be long enough for q's row: at least one tile on an axis side before a 135 degree corner (q's distance to R's line is b's length times a positive dot product). A track bending from the insert straight into the next side's heading would turn 90 degrees or more, which no track does, so this refuses nothing buildable.

**Open (not D407's).**
- Spacing across a dummy: two cells' walls on either side of a dummy have no row between them (8.3 step 4 assumed "separated by the cells between them"; no margin holds it). Pre-existing; relevant to the ~48 non-flex dummies left on 197. Candidate: by Lemma 2 a dummy line on the tile grid separates with spacing on one side only, so rows `>= RungSpacing` on one side and `>= 0` on the other suffice; to be written as its own ruling if 197 shows it.
- 8.3 (a), (d) are answered by Lemma 2 for walls facing a side's inner line; pairs at 45 degrees across a short wall remain the adjacent-side case of step 2.

### 8.9 The pocket rule: an adjacent side may use an outward step's room (DRAFT probe, orchestrator, 2026-11-22; Gemini Q1, digest wip/gemini/2026-11-22-corridor-pocket-reply-digest.md)
**Why.** On PSG 197, 9 faces fail under 8.8 (faces 7, 8, 11-17): thin strips between two lanes of one port, closed by a lane crossing or merging between them, where V makes room by stepping the lanes apart. 8.8 closes an outward step's pocket to every other side.

**Rule (per side S with at least one insert, walked heading h, u = STEP[h]).** Let L be S's last insert and F its first, in walk order; q = the vertex after L, p = the vertex before F; R = the side after S, P = the side before S. Only when the cell has at least 3 sides and R (resp. P) consists of track walls:
- **Along rows:** every vertex with a wall in R, except the corner S/R, keeps `Dot(v - q, u) >= m`. Every vertex with a wall in P, except the corner P/S, keeps `Dot(v - p, -u) >= m`.
- **Dropped rows:** a vertex with both walls in R drops its rows against S's walls before L (it keeps the lines of the walls after L). A vertex with both walls in P drops its rows against S's walls after F.
- Everything else is 8.8. A vertex at R's far corner keeps all its 8.8 rows and gains the along row.
- **m:** 0 for an axis h, 2 for a diagonal h.

**Proof (R; P mirrors it).**
1. Every piece of S has `Dot(STEP, u) > 0` (h itself, and h +- 1), so every point of S up to q lies at `Dot(x - q, u) <= 0`.
2. Every point of R lies at `Dot(x - q, u) >= m`. Its vertices do by the along rows, and the corner S/R is the end of S, past q.
3. The separating line M through q is perpendicular to h. q is a track vertex (a tile-edge midpoint).
   - For an axis h, M is a tile-edge line. A piece lies wholly inside one tile, and no piece lies along a tile edge (axis pieces run on middles, diagonal ones touch an edge at one end only). So pieces on the two closed sides of an edge line share no tile: m = 0.
   - For a diagonal h, M is a diagonal grid line. Lemma 2 needs `RungSpacing` = 2 on one side: m = 2.
   - So R shares no tile with S's walls up to L.
4. S's walls after L are kept from R by their own lines (R's vertices keep those rows; Lemma 1 for that part of S, Lemma 2). The corner S/R is the adjacent-walls case of 8.8 step 2.
5. Every other pair of sides is as in 8.8. R's far-corner vertex satisfies both its 8.8 rows and the along row, so R's last piece, whose ends both satisfy the along row, is covered by step 3.

**Cost (measured by the reference computation, `/tmp/claude-1000/pocket_ref.py` in the 2026-11-22 session).** Not weaker than 8.8, only different. The along row forces R to stay beyond S's last step. A side that turns back 135 degrees under S (R's heading h - 3) must then not reach back past the step, which 8.8 allowed when R stayed inside a's line. FS-b's hand cell drawing (top at heading 5 reaching y 16 below q at y 14) is refused by it. Which form a corner needs depends on lengths (a disjunction). Hence a flag, default off, and the decision from 197's measurement. If neither form alone draws 197, the next options are a rule picking the form per corner before the solve (e.g. by whether R's side is a port lane's sliver), or one binary per (S, R) corner.

### 8.10 A step of length 0 imposes no spacing (orchestrator, 2026-11-22; found by the elastic diagnosis, CARD-E5-flexstep Progress)
**Finding.** In 8.8's rows a vertex's spacing is `max(RungSpacing(h_e, h_k), RungSpacing(h_e, h_k+1))` over its two walls. At a corner of a flex insert, that counts the insert's heading even when the insert has length 0 and no physical existence. On PSG 197 face 7, a diagonal lane (B2c) crossing between two parallel axis lanes one lane apart carries an axis insert parallel to them. Its corners were held 2 from each lane, a strip of 4, where V's layout has 2. Without the insert's heading, faces 7, 8, 11 and 13 draw (elastic slack 0).

**Rule.** At a vertex between an insert I and a non-insert wall W, against a target line of heading h_e:
- **Base row:** the vertex keeps `base = RungSpacing(h_e, h_W)`.
- **Room row:** when `hi = RungSpacing(h_e, h_I) > base`, also `M * (d - base) - len(I) >= 0`, where d is the row's distance expression (the same `Dot(V - A, normal)`) and M = `COORD_MAX`.
- Inserts are never adjacent (an h piece lies between any two), so a vertex has at most one insert wall.

**Proof (Lemma 2 per piece).**
- Lemma 2 needs each piece J's two end points at `RungSpacing(h_e, g_J)` from the line.
- The base row gives this for W.
- For I: if `len(I) = 0` there is no piece, and nothing is needed.
- If `len(I) > 0`, the room row gives `d > base`. Every point is an integer variable and every normal has integer components, so d is an integer and `d >= base + 1`. RungSpacing takes only the values 1 and 2, so `hi > base` means base = 1 and hi = 2, hence `d >= 2 = hi`.
- Both end points of I are such vertices, so I meets Lemma 2.
- No binary. Sound for the integer program.
- In the elastic mode (an LP, D409) the room row is weaker (a fractional d - base lets a long insert through). That is acceptable for diagnosis only; the oracle's verdict comes from the integer program.
