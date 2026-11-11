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
| `[tunnelprice]` | 0.01 s | 5 s | 0.01 s |
| `[tmapnear]` | 0.8 s | 5 s | 0.09 s |
| `[curvespeed]` | 0.01 s (CS3) | 5 s | 0.01 s |
| `[nodes]` | 0.13 s (N0a: curve gap constraints vs the checker) | 5 s | 0.03 s (budget 1 s) |
| `[positions]` | 0.11 s (N1) | 5 s | 0.02 s (N1: JunctionPositions, 6 tests incl. 300-system brute force) |
| `[facts]` | 0.05 s (N2) | 1 s Release, 5 s Debug | 0.05 s Release, 0.16 s Debug (N2: JunctionFacts, 5 tests incl. 200-terrain walk comparison; 128x128 build 755 us) |
| wide gate | (Release only) | | 17 s (budget 120 s) |

Measured 2026-11-10 at the G3a merge.

G4 (2026-11-11): `[projection]` gained three tests and `[tmapnear]` was added; measured on a loaded machine.
