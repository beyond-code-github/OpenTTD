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
