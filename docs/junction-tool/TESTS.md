# Junction test tags and budgets

Run tests through `tools/junction/jt`, never by typing tag filters:
- `tools/junction/jt fast <build-dir> <tag> [budget-s]` runs `<tag>~[slow]~[.]` (no slow, no hidden tests; Catch2 runs a hidden `[.bench]` test whenever another of its tags is named) and fails with "OVER BUDGET" past the budget. Over budget is a stop condition in every brief.
- `tools/junction/jt gate <build-dir>` runs the wide gate `[junction]~[slow]~[mapexport]~[.]`, Release builds only.
Every brief names its tag, build and budget from this table. Adding a test to a fast tag must keep it inside its budget; otherwise tag the test `[slow]`. Measure a tag with `jt` before naming it in a brief (engineer, 2026-11-10: "we've run into this problem a few times now how do we stop it happening").

| Tag | Debug (measured) | Debug budget | Release (measured) |
|---|---|---|---|
| `[junctionmap]` | 4.9 s (PH1c: 4.95 s, not split) | 5 s | 0.6 s |
| `[linerouter]` | 10.4 s | 12 s (over the 5 s norm: split or move tests to `[slow]` when next touched) | 1.5 s |
| `[linebuild]` | 0.01 s | 5 s | 0.01 s |
| `[terrainrect]` | 0.02 s | 5 s | 0.01 s |
| `[projection]` | 13 s (information only) | measured in Release, 5 s | 2.7 s (PH1c split: the hub's edge tests moved to `[projectionedges]`) |
| `[projectionedges]` | not measured | measured in Release, 5 s | 2.6 s (G4's unbuildable-hub test, E1, E2, E3, E3b, PH1; the PH1 W-end limit test is `[slow]`, 13 s) |
| `[tunnelprice]` | 0.01 s | 5 s | 0.01 s |
| `[tmapnear]` | 0.8 s | 5 s | 0.09 s |
| wide gate | (Release only) | | 17 s (budget 120 s) |

Measured 2026-11-10 at the G3a merge.

G4 (2026-11-11): `[projection]` gained three tests and `[tmapnear]` was added; measured on a loaded machine.
