# D78: place-time generation on real terrain

**Rests on:** D77, D78, D87, D90, D92. **Facts:** `RECON-terrain.md`.

## Design (orchestrator's calls, reversible: D93)

1. **One plane.** A junction is built on one level H. A tile is usable at H if it is flat at H, or if a leveled foundation lifts it to H: non-steep, `GetTileMaxZ == H`, with `build_on_slopes` on, costing `BuildFoundation`.
   - Bridge heads sit at H. Span tiles need `GetTileMaxZ ≤ H` and `H < GetTileZ + max_bridge_height`.
   - That is the coop norm (flat junctions). It keeps every 1b rule valid unchanged.
   - Sloped track (inclined foundations) is a later option.
2. **Pure terrain snapshot, `JunctionTerrain`.** A grid in the canonical frame. Per tile it holds:
   - z_min and z_max;
   - a steep flag;
   - a class: buildable, water, house, other company, unclearable, own rail;
   - the clear cost.

   It is filled on the main thread from the map for a given (anchor, rotation, mirror) and read only by pure code. The router, the generator and their tests never touch the map, and tests fill it by hand.
3. **Router support.** `JunctionRouteRequest` gets `const JunctionTerrain *terrain` and `int plane`:
   - a piece's tile must be usable at the plane (class buildable, z rule as in 1), and costs clear + foundation;
   - bridge heads are usable at the plane;
   - span tiles pass the span rule and may be water or rail, but not houses (they would have to be cleared; first cut: blocked).
4. **Obstacles, first cut:**

   | Obstacle | Treatment |
   |---|---|
   | Houses, other companies, unclearable objects | blocked (the build path's Auto flag refuses houses anyway) |
   | Water | blocked except under spans |
   | Own rail | blocked (joining is the endpoint solver's business) |
   | Trees and rough land | cost |
5. **The generator** takes an optional terrain:
   - the plane H is swept over the distinct z_max values near the anchor (the most common first);
   - terrain cost is added to the score.
6. **Anchor:** the hover tile is the hub point, where the N branch column xI meets the E track row 0 in the canonical frame. It stays stable while candidates and sizes change.
7. **Speed:**
   - Building the snapshot is cheap: a fixed 64×40 window, a few map reads per tile.
   - Generation runs on a worker thread from the snapshot (pure data), started when the hover rests on a tile for 300 ms. It is cached per (anchor, orientation, params, terrain hash).
   - While it runs, the ghost shows the last result's outline in grey. When it finishes on the current tile, the ghost updates.
   - The real build still re-verifies everything through `EstimateJunction`'s command test runs.
8. **`EstimateJunction`:** relax the flat rule to the plane rule in 1, with foundations priced by `TestRail` as today.

## Steps

| Step | What | Acceptance |
|---|---|---|
| T1 | `JunctionTerrain` type plus router support (3, 4) | router unit tests on synthetic grids: a hill forces a bridge or a detour; houses block; water only under spans; a leveled foundation costs 250 |
| T2 | generator with terrain (5) | on a synthetic map with a ridge and a pond, `GenerateJunction` gives a checker-clean layout that avoids them; flat terrain gives the same result as today |
| T3 | game snapshot builder plus the `EstimateJunction` plane rule (2, 8) | a unit test on a small test map (as `junction_game` tests do); the estimate accepts leveled-foundation tiles |
| T4 | GUI worker and hover rest (6, 7) | manual: hover over hills and towns shows adapted layouts; no UI freeze |
| T5 | rig check | a hilly seed: generated layouts build with 0 failures |

## Progress

- **T1** a06206c944 (worktree): `JunctionTerrain`, plus router support. 216 cases.
- **T2** 0e9d0179eb (worktree):
  - `GenerateJunction(spec, terrain)` sweeps up to 3 planes, scores terrain cost, and checks usability after the crop.
  - Flat terrain gives the same layout as no terrain.
  - A house block is routed around.
  - A pond across the ML rows gives NoLayout (known limit: the ML would need its own water bridge; later).
- **T3a** ca1cdc373f: orientation helpers, generator hub offset, prio terrain check.
- **T3b** 9b746201c2: `MakeJunctionTerrain` from the map; the plane rule in `EstimateJunction`.
- **T4** 0768c9721e: hover-rest worker thread, LRU cache, hub-anchored placement, thread-safe spec.
- **T5** a6f95e7385 + d95f3b00f7 (D94) + 97fae742d2:
  - `junction_export` debug console command, plus rig `start --hilly`, `junction-here` and `junction-scan`.
  - The first scan found no layout anywhere: in 1950 no bridge reached 160 km/h, fixed by D94.
  - Hilly seed 3, 12 random spots: 3 generated, **all 3 built with 0 failures**; 9 found no layout.
- **T6** 543775bde5: nearest fit. `GenerateJunctionNear` tries (0,0), then 8 offsets at distance 2, then 8 at distance 4, from one margin-4 snapshot. The GUI snaps the ghost and says how far it moved; moving the cursor cancels the search.
  - Hilly seed 1, 12 spots: without it, 3 generated; **with it, 5 generated (offsets 2,0 and −2,2), all 5 built with 0 failures.**
  - Cost: a spot with no fit costs about 1.5 s per try in Release, so up to about 25 s for all 17 tries. The search is cancellable, but a cheap early reject of hopeless planes would help (follow-on).
  - `junction-scan` now maxes the loan first (after three builds the first run ran out of cash).
- **Next levers for the no-layout rate** (on hills the one-plane rule is strict):
  - (a) done in T6;
  - (b) sloped track on the legs (inclined foundations);
  - (c) terraforming the plane (`CmdLevelLand` cost);
  - (d) a water bridge for the ML.
