# CARD E1d: PSG 197's Structure output, written down and handed to Shape (orchestrator, 2026-11-20)

Implements D389 (197 pinned at the post-Structure level). Engineer, 2026-11-20: Shape consumes what we create; the file holds the variant's decisions, the existing builders derive the rest by rule. PLAN-sss-v2 E1 is amended to match (Structure's own search on 197 moves to E7).

## Goal
Shape receives one `PlanarStructureVariant` for 197, identical in type to what `BuildPlanarStructure` hands down. Before Shape relies on it, `CheckPlanarStructurePromise` confirms it (we stand in for Structure, so its promises are ours to keep, D367). A breach is a data error or a model gap: reported and named, never worked around.

## The file: tools/junction/topologies/psg197.structure.txt
The variant's decisions (SPEC-structure §2 items 1-4), one statement per line:
- `tunnel <separator>: <crosser>, ...` one per tunnel (50): the crossers in travel order along the separator, a contiguous run of the separator's `cross` list. Kind tunnel. A crossing of two tunnels is in both tunnels' statements (P3's two-structure case); "different levels" follows from that, and Heights picks which is deeper (D390 (2)).
- `group <separator>, <separator>, ...` the save's parallel tunnel groups (29), information for Shape v2's crossing-stack gadget; the loader keeps it only as a name list.
- `straight <node trunk line>: <leg>` per split and merge that has a straight leg, read from the save (the leg whose first piece keeps the trunk's heading); the other leg's side follows from the authored leg order. No statement: a fork.
- `copy <line> over <separator>`: D3 over C2 and C2b (G18).
Prio per merge (D390 (4)) is topology, so it goes into psg197.txt with its own small parser step, not here.

## The loader
`PlanarStructureVariant LoadPlanarStructureVariant(const PlanarTopology &topology, std::string_view text, const JunctionSpec &spec, std::string *error)`: channels from the topology; sites pinned from `straight`; separators and runs from `tunnel`; then the existing `BuildPlanarTrackMap`, `PlanarBuildStructures`, `PlanarBuildStructureSites`; tag `psg197`. It never calls the search, the ties, the flips or `EventsInChannelOrder` (authored order is pinned).

## Gaps expected (each becomes a test expectation; the outer test lists them until closed)
1. `PlanarBuildStructures` sets kinds to bridge: kinds must come from the file.
2. P4 forbids a crossing inside its crosser's own structure unless one is a bridge: tunnel under tunnel (14 crossings) needs D390 (2) in the oracle.
3. The model's copy doubles the separator; 197 doubles the crosser (D3 over the tunnels): a new kind of copy, a question for Pete once measured.
4. Sites are resolved per channel site (`ResolvePlanarSites`); per-node pins may disagree inside one site.
5. Channels: crosser nodes inside a tunnel group split the group's channel (D390 (3)).
6. Tunnel #49 crosses nothing in the topology (NOTES.md): left out of the file, listed as a known gap.

## Steps
- **E1d-1 (data, Sonnet, docs/tools only):** write psg197.structure.txt from the tunnel table, wip/probes/e1 and the save walks (order.py); checks: 49 tunnel statements whose crossers are contiguous on the separator's `cross` list, every non-TT crossing in exactly one tunnel, every TT crossing in two.
- **E1d-2 (code, Sonnet):** the loader, test-first; outer test "PSG 197's structure variant loads and keeps P1-P8", run red, its breach list recorded as the gap list (characterisation).
- **E1d-3..:** close the gaps one at a time (3 goes to Pete with numbers).
- **E1e:** today's Shape on the variant, measurement only: time, H1-H11, where it fails. Early reading on Gate A's speed risk.

## Load
Every build and test run at -j 4 and JT_JOBS=4 (two crashes at 14 jobs, 2026-11-20).

## Measured (E1d-2, 32f3f1aeb7, 2026-11-20): what 197 needs from the model
Loader test "PSG 197's pinned variant, gaps and promise" (17 gap lines, 10 differs lines, 29 promise lines); the track map is not built yet, so later promises are unmeasured.
- G1 Copy of a crosser: D3 doubled over T30/T32; the model doubles only a structure's separator (D351). Engineer's call.
- G2 Channel sites of 3-6 channels (CD3, EF1, A1, B1, A2m, E2c, EO1, CE123, AD23, NO2, SO3, SO2; D123 1; C2c's nodes pin different legs): Structure groups splits and merges per channel, and 197's cascades make many-way sites. Direction: a site per node (each node a gadget in v2, DESIGN-sss-v2 §3.2); touches T2/T3/LL2 (identity).
- G3 Crossing orientation from ring interleave (planar_regions.cpp:118-122, PlanarLineSides): valid only for minimal crossings; 197's tracks cross and cross back (first: E1b x E2). Direction: with authored crossings the orientation is authored too (from order.py's geometry), D389's pin; derived topologies keep today's rule.
- G4 D24 (split before merge): 28 P7 lines, routes C1, C2, C3, F1, F2, D1, D2, D3 merge and then split into parallel tunnels (C12c -> C12d -> C12/C12b; the choice by free tunnel, gaps §3). Engineer's call: D24 as a hard rule vs a split after a merge into a tunnel choice.
- differs (information for E7): AB2, C2c, C23c, C12c, CE123b, D123, CE123, SO3, NO1, AD12: the save's straight leg is not Structure's rule's (or the rule leaves it open).
- Loader rule settled inline: at a prio join the straight track is the prio row, as PlanarBuildStructureSites picks (last leg on the pinned line).

## G3 design (orchestrator, 2026-11-20; D389 extended, D343: the topology owns the face map)
- With authored crossings the crossing direction is authored too: in `cross L: ...` each partner carries a mark, `M>` (M arrives from L's left, travelling to L's right) or `M<` (from L's right). Marks are mandatory when crossings are authored; the pair's two marks are opposite (if M arrives from L's left, L arrives from M's right). Derived topologies keep today's ring-interleave rule.
- PSG 197's marks come from the geometry (order.py's intersection points): with headings dL, dM at the point (frame x east, y south), M arrives from L's left iff dot(dM, left(dL)) < 0, left(d) = (d.y, -d.x).
- Consumed by PlanarLineSides (planar_regions.cpp:112-125) in place of the interleave when `crossings_authored`. Validated by: an identity test (the authored T with marks from its own derived sides gives the same faces), then P1 on 197 (a mirrored frame or a bad mark fails the face map).
- Steps: E1f-1 marks into psg197.txt (data); E1f-2 parser and PlanarLineSides (code); then rerun the loader characterisation.
