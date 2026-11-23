# RECON rig v2: from a v2 drawing to the game (2026-11-25, Sonnet Explore, BRIEF-recon-rig-v2.md; file written by the orchestrator from its report)

## Q1 Domain suite game cases (`[planardomaingame]`, src/tests/junction/game/junction_game.cpp)
- No single domain-input -> commands function. Helpers: `BuildPieces(points)` (:1329) rasterises with `RasterPlanarDomainTrack` and issues `BuildRail` per piece; `BuildDomainBridge(raster)` (:1398) takes a `JunctionRaster` (v1), bridge between the first/last `on_bridge` pieces; tunnels by direct `BuildTunnel` in pins 7/8 (:1599, :1648).
- No signals in the domain pins. Heights by hand (`PrepareDomainMap` :1288 flat grass h4; `SetTileHeight` in tunnel pins). Fresh test map only.
- v1 whole-plan build: `[planarbuild]` (:1240): `PlanPlanarJunction` -> `MakeJunctionBuildSteps` -> BuildRail/Bridge/Signal, flat map.

## Q2 Domain rasteriser
- `RasterPlanarDomainTrack(points)` (planar_domain.cpp:85) -> `PlanarDomainRaster{pieces{tile, Track, from, to, heading}, rule, why}`. Equality to `RasterizeJunctionRoute` is a CLAUDE.md claim (not re-verified).

## Q3 Probe -> domain input
- `[slotsstep2]` (planar_domain_encoding.cpp:6664): :7059 `jc.points` (polish, else stage B last), :7068 `CheckPlanarDomain(PlanarDomainInputOf(cv->v, jc.points, notes))`.
- The probe writes no drawing file (md + stdout only; `PLANAR_DUMP_TRACK` prints one track). `dump_path` JSON exists (planar_draw.cpp:2370) but stage B/polish options do not set it.
- Structures in the domain input carry `first/last` piece indices on the separator raster -> head tiles.

## Q4 The rig
- `tools/ottd/ottd` + GS `tools/ottd/gs/main.nut`. Layout JSON = `JunctionLayoutToJson` (junction_layout.cpp:175): tracks with bits, bridges {start,end,tunnel,z}, signals.
- `ottd build-layout FILE X Y` (ottd:206; GS `build_layout` main.nut:183): rail, then bridges (`FastestRailBridge`), tunnels (`GSTunnel.BuildTunnel`, ignores `z`), signals. Company from `ottd company`. No terraform except `raise_rect`. `--find-clear` finds flat space.
- Earlier planar layouts: docs/junction-tool/wip/rig-2026-11-17/*.json (same format). `ud-T2-base.json` does not exist.

## Q5 Gap, in order
1. Points -> pieces with track bits: `RasterPlanarDomainTrack` per track (closest consumer `BuildPieces`).
2. Structures -> `JunctionBridge` entries from the domain structures' head tiles.
3. Tunnel levels/ramps: missing for v2 (v1 `SolveJunctionLevels` takes `JunctionTrackRoute`s); GS cannot terraform per corner.
4. Writer: pieces into a `JunctionLayout` (offset to >= 0), `JunctionLayoutToJson`.
5. Signals: v1 pass needs tagged routes; nothing from a v2 drawing.
6. Build: `ottd build-layout`.

## Shortest path
1. Final points + `cv->v` from `[slotsstep2]`; domain accept.
2. Rasterise each track; structure pieces not laid as rail; structures -> bridges (head tiles).
3. Normalise offsets, `JunctionLayoutToJson`, `ottd build-layout FILE X Y`.
4. Later: tunnel levels (new code from `SolveJunctionLevels`), signals (v1 pass with tagged routes).

## Open
- GS ignores tunnel `z`; domain frame origin/negative offsets unchecked; domain accept does not prove a build with heights.
