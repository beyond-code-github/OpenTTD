# Junction tool tests: src/tests/junction/ (Catch2)

- Tests mirror the `src/junction/` folders; tags are listed below.

## Testing
- Placement relations are checked by tests on both topologies (the T and LL_RR x L_R) from the start so nothing T-only can pass (D202).
- Test-first, outside-in: write the outer test, record it red, then build the inside (D4).
- Run tests through `tools/junction/jt`, never hand-typed tag filters; the build dir is given relative to the worktree and the tag in brackets: `tools/junction/jt fast build '[topology]' [budget-s]`, `tools/junction/jt gate build-rel` (Release only, wide gate `[junction]~[slow]~[mapexport]~[.]`), `tools/junction/jt identity build-rel` (Release only) (D140).
- `jt fast` runs `<tag>~[slow]~[.]` and fails with "OVER BUDGET" past the budget; over budget is a stop condition (docs/junction-tool/TESTS.md).
- Budgets per tag are in docs/junction-tool/TESTS.md; adding a test to a fast tag keeps it inside the budget or tags it `[slow]`.
- Fast tags (Debug budget 5 s unless the table says otherwise): `[junctionmap]`, `[junctionmapedges]`, `[linerouter]` (12 s), `[linebuild]`, `[terrainrect]`, `[projection]` and `[projectionedges]` (measured in Release), `[relaxed]`, `[topology]`, `[topoproj]`, `[nodes]`, `[positions]`, `[facts]` (1 s Release), `[terraform]`, `[hoverplan]`, `[game]`, `[junctiongame]`, `[junctionprice]`, `[tunnelprice]`, `[tmapnear]`, `[curvespeed]`, `[realterrain]`.
- Gate-only sweeps: `[topologysweep]`, `[topoprojsweep]`, `[topologystretch]`, `[realterrainsweep]`, `[nodeoracle]`, `[placeoracle]`, `[nodespeed]`, `[junctiongamesweep]`; `[identity]` is the hidden identity tag run by `jt identity`.
- Run `jt identity` in every step that touches `src/junction/topology/junction_topology.cpp`, `src/junction/placement/junction_placement.cpp`, `src/junction/signals/junction_signals.cpp`, `src/junction/junction_pipeline.cpp` or `src/junction/placement/junction_levels.cpp`, alongside the gate.
- Every step runs: its fast tag in Debug, the gate in Release, and `jt identity` where the line above applies (D140).
- Identity tests pin placements, renders, signals and plan hashes over the sweeps (the [topologysweep] cells, `LEVELHASH` over the 48 fixtures); a pinned number moves only by a stated rule with the changed cases listed.
- A refactor step is an identity on every seed: `SamePlacement` over the [topologysweep] cells, the circuit file byte-identical, all sweep numbers equal; a step that moves a seed stops and reports (G1 §16.2, G1 §16.4).
- A planned count in a sweep (`HOVERPLAN`, `HOVERNEAR`, `LEVELHASH`) may only rise; a drop is a stop, except when a correctness rule refuses plans that were wrong, with every changed offset listed and explained (P6b, HH2).
- Real terrain comes from the 48 fixtures in `tools/junction/terrain` (generated 256x256 maps); the game seam is tested on a game map with a company (`[junctiongame]`) (RT1, IG2).
- Write red tests red first and record the failure text before the fix.
- Slow sweeps are tagged `[slow]` and stay out of per-step runs; no baseline rebuilds for timing (D140).
- Reuse the persistent worktree builds: Debug `build`, Release `build-rel`; ccache on both (D140).
