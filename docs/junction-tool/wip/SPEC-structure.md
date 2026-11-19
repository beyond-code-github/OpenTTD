# SPEC: the Structure layer's hand-off (draft, 2026-11-20, orchestrator; for the engineer)

First of the SSS hand-off specs (D368). Implements src/junction/planar/CLAUDE.md "Architecture" (Structure) and DESIGN-tracks §17.1. Written from the contracts and rulings, not from the code (D368: code is mapped onto it afterwards). Rulings it rests on: D343/D344 (topology), D362, D364, D365, D366, D367, D369, D349 (rows), D351/D322 (copies), D203(2)/D228/D228a (shared structures, flow bound), D347/D347b (prio over a bridge), D24 (split before merge).

## 1. Input
- The topology (authored, D343): ports with their lines in ring order, lines, splits, merges, crossings and their order along each line, routes. Each crossing order is pinned or open (D343/D344); Structure decides only the open ones (triangle flips) and never re-decides a pinned one (D367).
- The spec's rule fields: train length, prio length, lane shares and flows (for doubling and D228a), game limits (longest bridge, max tunnel length).
Nothing else. No headings, positions or lengths exist yet.

## 2. What Structure decides (and nothing else)
Per variant:
1. **Ties:** at every split and merge the rules cannot settle, which leg runs straight (Terms: tie).
2. **Triangle flips:** the order of three mutually crossing lines, where the topology leaves it open. Tunnels do not remove it: three pairwise crossings always form a triangle in plan (unless all three coincide), so the orientation stays a choice. A **stack** (engineer, 2026-11-20: "they can cross within a target area sized according to the bridge/tunnel spans") is not a third option but a combination of the separator and kind bits within either flip: line A's bridge spans B and C (A x B and A x C one run on A), line B's tunnel runs under A and C, line C stays at grade; A x B is a separator x separator crossing in both structures (P3, P4). The crossings stay ordered; with no room needed between them the triangle can be small. Where the crossings sit is Shape's. Three levels only (a fourth needs a second tunnel depth, deferred D365). Corrected 2026-11-20: an earlier draft made the stack a third triangle option.
3. **Structures (D369):** the crossings grouped into structures: a structure is a run of consecutive crossings on one line, its **separator**, with no node and no other crossing between them on that line. Every crossing belongs to exactly one structure.
4. Per structure: the **separator** (which of the crossing lines carries it; per joiner at a prio join, D364) and its **allowed kinds** (bridge, tunnel, or either; D365, D366).
5. **Tracks:** every physical track: the topology's lines; a **copy** beside the separator over a structure where its share needs doubling (D351, D322, D228a; one copy per structure, in series); a **row** beside each lane of a prio join over its coverage (D349).
The variant is the combination of 1-4 (5 follows from them by rule). Variants exist here only (D362).

## 3. Output model (each variant), refined by every later layer (D367)
- **Track map:** every track with its ends (port end, node, or a copy/row split and merge on its parent); per track its events (crossings, nodes) in travel order; the planar map with its faces; sides per stretch.
- **Sites:** each split and merge with its straight leg and the side of the other leg.
- **Structures:** per structure, its separator track and span (first to last crossing event on it), its crossers (the tracks it spans, in order along the separator), its allowed kinds, its copy if any.
- **Structure cost:** a cheap score for ranking (below).
Variants pass down as a list, ranked by Structure cost; later layers add their own cost (D366).

## 4. Promise (each a test at Structure's output, D367)
- P1 The map is a valid planar map of every track (V − E + F = 2, simple faces), every authored crossing appears once, and events are totally ordered per track.
- P2 Sides per stretch have no conflict.
- P3 Every crossing is in exactly one structure, except a crossing of two separators (a stack, or a bridge over another line's tunnel), which is in both their structures; a structure's crossings are consecutive events on its separator with no node between. (Corrected 2026-11-20 after S1: "exactly one" made P4 unreachable.)
- P4 No two separators cross unless one can be a bridge and the other a tunnel (bridge over bridge is impossible; tunnel under tunnel deferred, D365): their allowed kind sets are narrowed so that holds.
- P5 Each structure's crossers fit its kind's limit at the most compact crossing angle (longest bridge / max tunnel length; D228a for a main line), for every kind in its allowed set.
- P6 Copies and rows follow their rulings: a copy exactly where its separator's share needs doubling, beside it over the structure's span; a row beside each prio lane over its coverage, away from the joiners.
- P7 Split before merge on every route path (D24).

## 5. Settled (D371)
Q1 cheap counts: structures plus copies (D371a); Q2 maximal runs plus forced splits; Q3 a copy per structure whatever the kind; Q4 band counts read from the track map (Structure's output).

## 6. Output types (target; code is mapped onto these, D368)
New header src/junction/planar/planar_structure.h:
- `PlanarStructureSite { uint32_t node; bool split; uint32_t straight; int8_t side; }`: a split or merge of the track map, its straight leg (a track) and the side the other leg leaves or arrives on.
- `PlanarStructure { uint32_t separator; std::vector<uint32_t> crossings; std::vector<uint32_t> crossers; uint8_t kinds; uint32_t copy; }`: crossings are indices into the map's crossings in travel order along the separator; crossers the tracks it spans in the same order; kinds a bit set (1 bridge, 2 tunnel); copy a track or UINT32_MAX.
- `PlanarStructureVariant { std::string tag; PlanarTrackMap map; std::vector<PlanarStructureSite> sites; std::vector<PlanarStructure> structures; int cost; }`.
- `PlanarStructureResult { std::vector<PlanarStructureVariant> variants; std::string why; }`, variants ranked by cost, ties by tag.
- `PlanarStructureResult BuildPlanarStructure(const PlanarTopology &topology, const JunctionSpec &spec)`.
- `std::vector<std::string> CheckPlanarStructurePromise(const PlanarTopology &topology, const JunctionSpec &spec, const PlanarStructureVariant &v)`: the promise P1-P7 as a test oracle (D367): one message per breach, empty when kept.
Until tunnels are built (D365): kinds is always bridge, no stacks.

## 7. Build order (each step one brief, test-first at Structure's output)
- S1 **The promise oracle, then the gap:** `CheckPlanarStructurePromise` driven by hand topologies, one failing test per promise (each input breaks only that promise). Then, characterisation only: wrap today's variants (channels, resolved sites, track map) into `PlanarStructureVariant` and run the oracle over every T2, T3 and LL2 variant; report breaches by promise. No change to existing code.
- S1 DONE (wt-structure f3e9febf65, 636ba3bd55): oracle and 10 tests green, gate 659/659. Tally over today's variants (both forms): P3 breached by every variant of T2, T3, LL2: crossings inside node groups (fray and join sites) get no separator in Structure today (only channel crossings have `over`; the composer decides the rest later, a D366 breach). P6 breached in the joiner_over=false form on T2 and T3: a prio join doubles its lanes over the joiners with no share test (D331/D332 say only full-share lines double). P1, P2, P5, P7 clean; P4 unreached (one structure per crossing).
- S2 plan, from the tally: (a) the oracle's P3 takes the two-structure exception above, and P6 checks one copy per structure in series; (b) a separator for every site crossing: joiner x lane by D364's per-joiner bit, other site crossings by D227's defaults (C1 far-side turning leg over, C2 through over, C3 farther joiner over); (c) copies only where the separator's share is full (D331/D332), in both forms; (d) grouping into maximal runs with forced splits, cost (D371a), ranking, `BuildPlanarStructure`. D228a in P5 needs the main line's signal spacing: check JunctionSpec for it before briefing.
- S2b (D372 separator per mover): every crossing in a node group is now mover x keeper with a separator; P1-P7 clean on T2, T3, LL2 in every form.
- S2c design notes (orchestrator, 2026-11-20, to settle before its brief): (1) **Maximality:** today's map makes copies per channel run and per site, so a separator whose channel run continues straight into its site crossings (no node between) gets two copies where D371(2) wants one structure. Needs an oracle check (adjacent structures on one separator with no event between must be one unless P5 forces the split) and copies made from Structure's grouping, not the reverse; a merged run whose channel and site copy sides disagree is a stop to bring to the engineer. (2) **Variant count:** per-mover bits multiply variants (T3: 12 movers, about 65k variants; LL2 about 57k). Cost (D371a: structures + copies) is cheap from per-crossing separators and line event order, so rank all variants on it and build full maps only for the top k handed down (D370's seed list). Ties are wide: tie-break by tag for now; diversity of the top k is a later question. (3) `over` for channel crossings must come from the open-crossing rule plus bits WITHOUT `BuildPlanarPaths` (that is Shape's; it also fails 1024 LL2 variants on piece fit, a Shape failure Structure must not inherit).
- S2c1 (running): separators per crossing, maximal runs with forced splits, copies made from runs; oracle maximality. Rulings on its stop (2026-11-20): runs are cut at a prio lane's row start (the row's split node is a node; a bridge cannot hold a switch); copy and row tracks are physical crossers: every map crossing belongs to the structure that spans it (a structure's copy crossings are that structure's: the doubled pair is one structure), and P3/P5 count them. Red counts before the refactor ("P3 not maximal", S2b maps): T2 36/60, T3 136/216 messages ({}/all movers), LL2 0. Open if it bites: forced splits still count line crossings only.
- S2c2 design (orchestrator, 2026-11-20): `BuildPlanarStructure(topology, spec, keep)`.
  - Bits: every channel crossing (D347b: main line over is the search's start, not a fixed rule), ties, open triangles (flips), movers (D372). Tag `o..t..f..m..`.
  - Cheap cost per variant without a track map: `PlanarCrossingSeparators` + `PlanarGroupStructures` give runs; cost = runs + runs with copy (D371a). `PlanarLineSides` depends only on the flipped channels: build it once per flip and reuse.
  - Search: exhaustive when the bits total at most 16; otherwise best-improvement one-bit flips from the default (all channel bits at main line over, ties 0, flips 0, no mover separators: D227 C2), keeping every variant evaluated. A variant whose sites do not resolve is skipped and counted.
  - Output: the `keep` best by (cost, tag), each with its track map, sites and structures; a map with a why is skipped and counted.
  - S3 outer test: T2, T3, LL2 with keep 16: every variant handed down keeps P1-P7 (maximality included); costs non-decreasing; per hub the bits, variants evaluated, skipped, and time (target well under the 5 s seed, D297; Structure should be milliseconds to low hundreds).
- S2 **Close the gaps S1 names,** one promise at a time, in the new module: structures (grouping, separator per joiner D364, forced splits), copies per structure, sites, cost, ranking. Today's code is reused only where S1 shows it already keeps the promise.
- S3 **Outer test:** `BuildPlanarStructure` on T2, T3, LL2: every variant keeps the promise; ranking order; time (Structure in milliseconds, review §5).
