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
- **Mirrored crossings:** a copy's or row's k-th crossing mirrors its parent's k-th crossing (Structure's contract: beside tracks' crossings follow by rule from their parent's structure, so they are in no structure's list). It takes the parent's role: beside a separator it is a parallel deck of that structure (as a copy is); beside a crosser it is spanned by the same separator. Its angle equals the original's, and the crosser sits on one piece across both crossings.
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
- H9 Refinement: Structure's output is unchanged, and P1-P7 still hold on it.
- H10 Every variant handed down is shaped. A variant with no shape is a bug: either in Shape, or a condition missing from Structure's promise, which is then added there (never a skip, D361).

## 5. Settled (D373)
- Q1 **Closure:** Tamassia's theorem (valid angles imply a drawing) covers orthogonal drawings only; for 8 directions with positive lengths it is not guaranteed. Shape owns winding (H7); whether a winding-valid shape always closes once Slots opens the cusps is probed first (§7 H2) with a lengths-only LP oracle (lower bounds 1, no clash rows). A missing condition goes into Shape's model, not a later layer.
- Q2 **Ranking:** Structure cost, then Shape cost (lexicographic).
- Q3 **Rows:** a row takes its lane's headings over its whole range. D342's other freedoms are not Shape's: a gap between row and lane is for Slots' spec; a row through a tunnel or over a bridge is Structure's. Toggle candidate: row headings of its own between joins.

## 6. Output types (target; code is mapped onto these, D368)
New header src/junction/planar/planar_shape_layer.h (name to settle against today's planar_shape.h when mapping):
- `PlanarStructure` gains `std::array<int, 2> limit;` (max middle tiles, index 0 bridge, 1 tunnel; -1 when that kind is not allowed). A Structure step, briefed against SPEC-structure §3.
- `PlanarShapeTrack { std::vector<int> pieces; std::vector<uint32_t> event_piece; }`: the track's heading list (one lifted heading per piece, eighths clockwise from north, consecutive pieces one eighth apart) and, per entry of the map's `events[track]`, the piece that crossing sits on (CLAUDE Terms: Shape = heading list + crossing pieces). The heading at a point is the heading of the piece it sits on; stretch k runs from piece event_piece[k-1] (0 for k = 0) to event_piece[k] (the last piece for the last stretch).
- `PlanarShapeStructure { uint8_t axis; std::array<int, 2> middles; std::array<int, 2> end_run; }`: least middles and end run tiles per kind (-1 not allowed).
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
