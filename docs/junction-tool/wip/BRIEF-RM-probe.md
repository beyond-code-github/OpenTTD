# Brief RM (probe, throwaway): prio rows as lines of the planar map, their sides used by clash separation

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-pr` is at {HEAD}. Create `probe-rm` from it; commit only there. Finish with `git switch wt-planar` and rebuild `openttd_test` in build-rel.
- Time box: 2 hours. Every test run uses a hard kill: `timeout -s KILL <s>` (the binary ignores SIGTERM). Single-variant runs: 30 s. End-to-end runs: 60 s.
- Do not touch `/home/pete/OpenTTD/docs/junction-tool/wip/REVIEW-architecture-tracks.md` (another reviewer writes it).

## Why
On T2 variant o1t3f1s0 the composer's prio row strands (`"prio:" + <lane>`, n-lane `Join`, prio case) cost 111 tiles: 932.25 with rows as clashing strands, 834.25 with rows excluded from clashes (`PLANAR_ROWFREE=1`, but then some prios no longer fit end to end). A row is no topology line, so `PlanarLineSides::LineOfStrand` gives it no line and the lazy loop separates its clashes by greedy trial. Mapping a row onto its lane's sides is infeasible. The claim to prove: a row as its own line in the planar map gets correct sides, and with them the rows cost only their real room.

## Read first (once each)
- `src/junction/planar/planar_regions.{h,cpp}` whole (382 + 85 lines): `PlanarMap` (vertices: ring ends, nodes, crossings; rotations per vertex; faces), `PlanarLineSides` constructor, `EventsInChannelOrder`, `Side`, `LineOfStrand`.
- `src/junction/planar/planar_topology.h`: `PlanarLine`, `PlanarNode`, `PlanarCrossing`, `PlanarTopology` (`events`, `ring`, `routes`).
- `src/junction/planar/planar_components.cpp` on probe-pr: the `sides` member (~:807) and where it is built; `build_join` and the n-lane `Join` prio case (lanes `S` far..near, joiners `J`, `sigma` = `rs.side`, +1 = the joiners on the right of travel); `SolveLazy`/`SolveLazy1` and their callers (~:2212-2380, ~:2809, ~:2831), the `sides != nullptr` round.

## Changes
### `planar_regions.{h,cpp}`
1. Add `struct PlanarRowSpec { uint32_t lane; std::vector<uint32_t> joiners; int8_t side; };` (lane: the prio lane's line; joiners: the joiner lines whose join-internal crossing with the lane the row mirrors, in the lane's event order; side: the joiners' side, +1 right) and a second constructor `PlanarLineSides(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarRowSpec> &rows)`. The existing constructor's behaviour is unchanged (implement it as the new one with no rows, or share a private helper; no other new member).
2. In the new constructor, after `EventsInChannelOrder` gives `t` and before the ends and `PlanarMap`, expand `t` for each spec (lane L, joiners Js, side):
   - **Split point:** k = the index in `t.events[L]` of the first crossing whose other line is one of Js. If no such crossing exists, set `why` "prio row lane has no join crossing" and return.
   - **New line L2** (name: L's name + `"~"`, routes: L's routes, from: a new node N, to: L's old `to`). L keeps index and name; L's `to` becomes N. `t.events[L2]` = `t.events[L]` from k on; `t.events[L]` keeps the part before k. Every crossing in the moved part that names L now names L2 (keep `a < b` in `PlanarCrossing`, swapping if needed).
   - **New line R** (name: `"prio:"` + L's name, routes: L's routes, from: N, to: L's old `to` node M).
   - **Node N:** `in = {L}`, `out` = left to right across travel: `side > 0 ? {R, L2} : {L2, R}` (the row is on the side away from the joiners).
   - **Merge node M:** replace L by L2 in `M.in`, and insert R so that `M.in` stays left to right: the row is next to L2 on the side away from the joiner (for `side > 0`: R immediately left of L2; else immediately right).
   - **Row crossings:** for each J in Js, in order, find the crossing c of (L2, J); add a new crossing (R, J) (with `a < b`); append it to `t.events[R]`; insert it into `t.events[J]` immediately after c (the joiner, travelling toward the lanes, meets the lane and then its row).
   - **Paths:** in every route path, insert L2 after L.
   - Count and table size use the expanded line count; `by_name` holds the new names (so `LineOfStrand("prio:B1")` finds R; do not change `LineOfStrand`).
3. Ends: compute as today from the routes; R has no route path, so set `ends[R] = ends[L]` (R's own lane) before the "no route path runs through line" check.
4. If `PlanarMap` validation fails on the expanded topology, `why` carries its message as today (this is a result, not a stop: report it).

### `planar_components.cpp`
5. Add a member `PlanarLineSides row_sides; ///< Sides with each prio row as a line of the planar map (probe).` and `std::vector<PlanarRowSpec> row_specs;`. In the prio case of `build_join` (or `Join`), for each lane k push `{line of S[k], {lines of J[0..k-1]}, sigma}` (the joiners S[k] passes under). After the composer's `Build` completes and before the solve, when `std::getenv("PLANAR_ROWMAP") != nullptr` and `row_specs` is not empty, build `row_sides = PlanarLineSides(topology, channels, row_specs)` with the same topology and channels the existing `sides` uses; if its `why` is not empty, fail the variant with "row map: " + why.
6. Pass `&composer.row_sides` instead of `&composer.sides` to `SolveLazy` at both call sites when `PLANAR_ROWMAP` is set and `row_sides` was built; otherwise unchanged. Nothing else reads `row_sides` (ladders and faces keep `sides`).

## Measure (one at a time, hard kills)
Variant runs: `PLANAR_ONLY=o1t3f1s0 timeout -s KILL 30 build-rel/openttd_test "planar components: T2 from its topology text (mapped) at the probe's area" -s`, area from the `variant o1t3f1s0 area ... size ... in ... ms` line, failure from `no variant solves`. S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
1. Without env: must equal 932.25 75x31 (unchanged).
2. `PLANAR_ROWMAP=1`: area, size, ms or failure (with `PLANAR_IIS=1` on a failure: the IIS rows, sorted unique, and whether any names a `prio:` strand).
3. If 2 solves: `pv run build-rel T2` and `pv run build-rel T3` (tool `/home/pete/OpenTTD/tools/junction/pv`, `--out <S>/rm-<hub>.txt`) with `PLANAR_ROWMAP=1`, each under `timeout -s KILL 600`; per hub built/failed with reasons, best and tag, against `<S>/cov2-T2.txt` (T2 with rows today; T3 has no baseline in this state, report as is).
4. If 2 solves: end to end on T2 and T3 with `PLANAR_ROWMAP=1`: `JUNCTION_COMPONENTS_TOPOLOGY=<hub> JUNCTION_COMPONENTS_OUT=<S>/rm-<hub>.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/rm-e2e-<hub>.txt 2>&1`; report plan success or its message, the `MAPPED` line, `VIOLATION` lines grouped by rule with counts.

## Do not
- Change `LineOfStrand`, the ladders, faces, the slot layer, the signal pass or the checker.
- Run two heavy jobs at once. Touch ports 4077/4079 or start a game client. Use git stash.

## Stop conditions
- A crossing (L2, J) for a J in the spec does not exist after the split.
- `M.in` does not contain L.

## Commit and report
- Commit on probe-rm: `Probe: Junction prio rows as lines of the planar map for clash sides (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-RM.md` (do not commit): changes, measurements 1-4.
- Reply with measurements 1-4, the commit hash, `git status --porcelain`.

When you have reported, stop.
