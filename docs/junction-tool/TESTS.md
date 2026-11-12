# Junction test tags and budgets

Run tests through `tools/junction/jt`, never by typing tag filters:
- `tools/junction/jt fast <build-dir> <tag> [budget-s]` runs `<tag>~[slow]~[.]` (no slow, no hidden tests; Catch2 runs a hidden `[.bench]` test whenever another of its tags is named) and fails with "OVER BUDGET" past the budget. Over budget is a stop condition in every brief.
- `tools/junction/jt gate <build-dir>` runs the wide gate `[junction]~[slow]~[mapexport]~[.]`, Release builds only.
Every brief names its tag, build and budget from this table. Adding a test to a fast tag must keep it inside its budget; otherwise tag the test `[slow]`. Measure a tag with `jt` before naming it in a brief (engineer, 2026-11-10: "we've run into this problem a few times now how do we stop it happening").

| Tag | Debug (measured) | Debug budget | Release (measured) |
|---|---|---|---|
| `[junctionmap]` | 2.7 s (TC1 split: the first-height and router-comparison tests moved to `[junctionmapedges]`; was 4.94 s) | 5 s | 0.3 s |
| `[junctionmapedges]` | 2.3 s (TC1: "the first height is a lower bound", "the line-router T matches or beats the old-router T") | 5 s | 0.3 s |
| `[linerouter]` | 10.4 s | 12 s (over the 5 s norm: split or move tests to `[slow]` when next touched) | 1.5 s |
| `[linebuild]` | 0.01 s | 5 s | 0.01 s |
| `[terrainrect]` | 0.02 s | 5 s | 0.01 s |
| `[projection]` | 13 s (information only) | measured in Release, 5 s | 2.7 s (PH1c split: the hub's edge tests moved to `[projectionedges]`; GS1b: the town-hill test lost `[slow]`, 0.02 s) |
| `[relaxed]` | 0.41 s | 5 s | 0.06 s (GS1b: JunctionRelaxedReachable) |
| `[projectionedges]` | not measured | measured in Release, 5 s | 2.6 s (G4's unbuildable-hub test, E1, E2, E3, E3b, PH1; the PH1 W-end limit test is `[slow]`, 13 s) |
| `[game]` | 0.012 s (MakeJunctionSpec, GameJunctionPricing, MakeJunctionTerrain; 10 tests, untagged until 2026-11-12) | 5 s | 0.009 s |
| `[tunnelprice]` | 0.012 s (BL1) | 5 s | 0.009 s (BL1) |
| `[tmapnear]` | 0.8 s | 5 s | 0.12 s (G1a; ridge BENCH 43 ms) |
| `[curvespeed]` | 0.01 s (CS3) | 5 s | 0.01 s |
| `[nodes]` | 2.2 s (G1a, 2026-11-11) | 5 s | 0.19 s (G1a; budget 1 s) |
| `[nodespeed]` | 2.1 s (six-speed sweeps: node solve and prio sizing at 100-196 km/h; run in the gate, not per step) | 10 s | 0.20 s (G1a; budget 5 s) |
| `[nodeoracle]` | gate only, not measured | n/a (in the wide gate, not a per-step tag) | 1.3 s (HN1: 1.34 s; CK1: 250 random terrains, the node search's layouts through CheckJunctionRules; ORACLE line prints terrains, skipped, accepted, search micros) |
| `[positions]` | 0.11 s (N1) | 5 s | 0.02 s (N1: JunctionPositions, 6 tests incl. 300-system brute force) |
| `[facts]` | 0.05 s (N2, HF1b) | 1 s Release, 5 s Debug | 0.05 s Release, 0.16 s Debug (N2: JunctionFacts, 5 tests incl. 200-terrain walk comparison; 128x128 build 755 us) |
| `[topology]` | 0.01 s (G1b(i), G1b(ii-a) to G1b(ii-d), G1c(i-a), G1c(i-b), G1c(ii-a), SB1, G1c(ii-b), G1d, ST1, ST2, CS1, NP1, NP2, P1, P2a, P2b, P3: topology, lane expansion, chord crossings, slot rows, track graph with node levels, track order with band layers, track structure, track programs, seed placement, series bridge cut, seed render: checker ports and movements from a topology, signal tokens, prio element, link directions; 45 tests) | 5 s | 0.012 s (budget 1 s) |
| `[topoproj]` | 2.3 s Debug (P4a, 2026-11-12; P2b, P3, P3b, P4a... 6 tests: graph projection solves on facts; the P4a sweep moved to `[topoprojsweep]`) | 5 s | 2.3 s Debug |
| `[topoprojsweep]` | gate only, not per step (P4a's 20-solve failure sweep on LL_RR) | 10 s | 2.1 s Debug |
| wide gate | (Release only) | | 23.3 s (BL1; budget 120 s) |

Measured 2026-11-10 at the G3a merge.

G4 (2026-11-11): `[projection]` gained three tests and `[tmapnear]` was added; measured on a loaded machine.
