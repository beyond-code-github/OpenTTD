# BRIEF drawn-hub: the contract to the back end, written from a v2 drawing, rendered for the rig (D452; handover step 2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes in all. The probe run in part 6 caps every solve at 15 s and runs once.

## 1. State
- **Run 2 (2026-11-25).** Run 1 stopped at zero-length port tracks (BR1's AC1, BD1: the split sits on the port, D412). Its work is in the WIP commit `583d6eff49` (helper commit d0fe669219 done; planar_hub.{h,cpp} real; `RenderPlanarDrawnHub` a stub; tests A and B written). Start from it (head below includes it); do not rewrite history: the `Add:` commit goes on top of the WIP. Rulings for run 2 (orchestrator, under D452):
  1. **Zero-length tracks are tracks of the hub.** Hub nodes list every track at them, zero-length ones included (`in`/`out` from the track map's expanded nodes, not from `NodesOf`). `PlanarDomainInputOfHub` drops zero-length tracks from its nodes exactly as `NodesOf` does, so the domain sees what it sees today. The §4.1 promise stands as written.
  2. **Render at a node with no piece before it on the path** (a split on the in port): its split switch is on piece 0 and it gets no arrival; a merge with no piece after it on the path likewise has its switch on the last piece and no next-piece logic. A zero-length track adds no pieces. Report what the signal pass does with such a split.
  3. **The oracle skips domain breaks whose rule is `grid` or `turn`**: the rasterise promise already reports that track (one line per break).
  4. **`PlanarDrawnHub::Structure` gets `int limit`** (the most middles, -1 none, as `PlanarDomainStructure::limit`), filled from the domain input, and `PlanarDomainInputOfHub` passes it on, so the span rule is judged through the hub.
- Load limit (CPU instability): every jt run `JT_JOBS=4`, every direct cmake build `-j4`. Another agent works in ng2 in parallel: never touch ng2.
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Builds: Debug `build`, Release `build-rel`.
- Design: DECISIONS D452 and `docs/junction-tool/wip/SPEC-slots-lengths.md` §4.1 (the drawn hub). Read §4.1 whole: it is the contract this step builds. Recon: `docs/junction-tool/wip/RECON-rig-v2.md`. Flat ground; no levels, no tunnels, rows not rendered (§4.1).

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md`: "Back end (`planar_plan`)", "Track map", the last two Drawing program bullets (Domain, `PlanarDomainInputOf`).
- `src/junction/planar/planar_plan.{h,cpp}`: `PlanarRender` (h:46-55) and `RenderPlanarRoutePrograms` (cpp:192-297) whole; `PlanPlanarJunction`'s signal call (cpp:320-330).
- `src/junction/planar/planar_components.h` lines 20-45 (`PlanarProgramPiece`).
- `src/junction/planar/planar_domain.h` (whole); `src/junction/planar/planar_domain_map.{h,cpp}` (`PlanarDomainInputOf`, `AddStructure`, `NodesOf`).
- `src/junction/planar/planar_regions.h` lines 85-130 (`PlanarTrack`, `PlanarTrackMap`); `src/junction/planar/planar_topology.h` lines 35-80 (ports, nodes, routes).
- `src/junction/signals/junction_signals.h` lines 25-42.
- `src/tests/junction/planar/planar_domain_encoding.cpp` lines 3652-3700 (BR1) and 7040-7075 (`[slotsstep2]` polish and domain verdict).

## 3. Design (decided)
New files `src/junction/planar/planar_hub.{h,cpp}` (register in the folder's CMakeLists.txt as its neighbours are):
```cpp
/** The drawn hub (D452, SPEC-slots-lengths §4.1): the one hand-off from Slots to Heights, Signals and the judges. Half tiles, x east, y south, every tile >= 0. */
struct PlanarDrawnHub {
	struct Track { std::string name; PlanarTrack::Kind kind; std::vector<std::array<int, 2>> points; };
	struct Node { std::array<int, 2> at; std::vector<uint32_t> in; std::vector<uint32_t> out; bool split; };
	struct Port { PlanarSide side; bool in; std::vector<uint32_t> tracks; };
	struct Structure { uint32_t separator; uint32_t first; uint32_t last; int ends; bool tunnel; std::vector<uint32_t> crossers; };
	struct Route { std::string name; bool prio; std::vector<std::vector<uint32_t>> paths; };
	std::vector<Track> tracks;
	std::vector<Node> nodes;
	std::vector<Port> ports;
	std::vector<Structure> structures;
	std::vector<Route> routes;
};
/** A v2 drawing (points per track of the variant's track map) as a drawn hub; empty hub and why on failure. */
PlanarDrawnHub PlanarDrawnHubOf(const PlanarShapeVariant &v, const std::vector<std::vector<std::array<int, 2>>> &points, std::vector<std::string> &notes, std::string &why);
/** The contract's oracle (§4.1 promises); one line per broken promise, empty when it holds. */
std::vector<std::string> CheckPlanarDrawnHub(const PlanarDrawnHub &hub);
/** The hub's domain input (a projection: tracks' points, nodes, structures). */
PlanarDomainInput PlanarDomainInputOfHub(const PlanarDrawnHub &hub);
```
- `PlanarDrawnHubOf`: `in = PlanarDomainInputOf(v, points, notes)` gives points (beside openings included), nodes and structures; premise, checked first: `in.tracks.size()` equals the track map's track count and `in.tracks[i]` is track i = expanded line i (else why, STOP and report). Names and kinds from `v.structure.map.tracks`; `split` from the expanded topology's node; ports from `map.expanded.ports` (lines are tracks); routes from `map.expanded.routes` (name, prio, paths as given). Then shift every point by one even vector (whole tiles) so the least raster tile over all tracks is (0, 0).
- `CheckPlanarDrawnHub`: the §4.1 promises: every track rasterises (no `rule`); every path's consecutive tracks t, u share a node with t in `in` and u in `out`; every port track's port-side end lies on the box edge of its side (box = the tiles' bounding box); every structure's first..last within its separator's raster with axis end pieces; `CheckPlanarDomain(PlanarDomainInputOfHub(hub))` accepts (its rule as the line).
- `planar_plan.{h,cpp}`: `PlanarRender RenderPlanarDrawnHub(const PlanarDrawnHub &hub);`. First extract from `RenderPlanarRoutePrograms` a file-local helper that takes a `std::vector<PlanarProgramPiece>` and a name and returns the `JunctionRaster` (move string F/L/R, `B<middle>` from a Head to the next Head, `RasterizeJunctionRoute`, the tile-by-tile check against the planned pieces) or a why; `RenderPlanarRoutePrograms` calls it unchanged in behaviour (its own commit, `Codechange:`). `RenderPlanarDrawnHub` then, per route and path: the pieces of the path's tracks' domain rasters in order as `PlanarProgramPiece{tile, heading, kind, k}` (kind Head at a structure's `first`/`last` on its separator, Middle between, else Ground; k = the track's place in the path), the helper, `JunctionTrackRoute{running index, 0, pieces, tags}` with tag edge = track index; the signal route as `RenderPlanarRoutePrograms` builds it, with the hub's nodes in place of topology nodes (arrival at each node between consecutive tracks; split switch on the first piece after it, merge switch and merge node on the last piece before it); node tiles likewise; port tiles from the first (in) / last (out) piece on a port track; layout via `AddRasterToLayout`, size from the tiles. A tunnel structure: why "tunnel <separator name> not rendered (no levels)".

## 4. Tests, outermost first
**Test A** (outer; `src/tests/junction/planar/planar_domain_encoding.cpp`, tags `[junction][planar][planardomain]`): "the drawn hub of BR1 holds its contract and renders for the back end". BR1's base drawing and variant -> `PlanarDrawnHubOf` (why empty) -> `CheckPlanarDrawnHub` empty -> `RenderPlanarDrawnHub` (why empty) -> the layout's track bits per tile equal the union of the domain raster pieces over every track minus each structure's `first..last` on its separator; one bridge per structure at the head tiles; `SignalJunctionTrackRoutes` on it (the test spec) returns no why. Red first: stubs returning an empty hub with why "stub"; record the failure text.
**Test B** (inner, same tags): `CheckPlanarDrawnHub` on BR1's hub edited three ways: a path with two tracks swapped (names the path), a point moved off the grid (names the track), a port track moved one tile inward (names the port); each gives exactly one line naming it.
**Test C**: `RenderPlanarRoutePrograms` unchanged: `[planar]` in Release (part 9).
Budget: `tools/junction/jt fast build '[planardomain]'`.

## 5. Probe hook and docs
- `[slotsstep2]` probe (~:7068, after the domain verdict): when env `PLANAR_RIG_OUT=<path>` is set: `PlanarDrawnHubOf` on the final drawing, `CheckPlanarDrawnHub`, `RenderPlanarDrawnHub`, `SignalJunctionTrackRoutes` (the probe's spec), write `JunctionLayoutToJson(layout)` to `<path>` (unsignalled when the signal pass fails, said in the line); print `rig: <path> size WxH tracks N bridges M signals S contract <ok|lines>` and any why.
- `src/junction/planar/CLAUDE.md`: a short "Drawn hub (`planar_hub`)" section with the decided facts (D452). `docs/junction-tool/TESTS.md` only if a tag's measured time changes.

## 6. Probe run (after the gates; Release)
In `/home/pete/OpenTTD/.claude/ng1/build-rel`: `PLANAR_SLOTS_HUB=T1 PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 JUNCTION_MILP_INCUMBENT=1 PLANAR_RIG_OUT=<scratchpad>/T1-v2.json ./openttd_test '[slotsstep2]' > <scratchpad>/t1.log 2>&1`. Copy the JSON to `/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/rig-2026-11-25/T1-v2.json` and commit it. Report the `rig:` line, the judge and domain lines, and the signal pass's prio outcome.

## 7. Do not
- No levels, terraform, tunnels, row rendering. No change to `SignalJunctionTrackRoutes`, `RasterizeJunctionRoute`, `CheckPlanarDomain`, `RasterPlanarDomainTrack`, `PlanarDomainInputOf`, any solver or default; no v1 adapter to the hub. No knob besides `PLANAR_RIG_OUT`.
- Never start a game client or server, never touch the rig (ports 4077/4079) or other worktrees. No `git stash`. Source edits only with the Edit tool (no Python or shell rewrites, not even for temporary tests).

## 8. Stop conditions
- The track-index premise in part 3 fails; the helper extraction changes any `[planar]` result; `RasterizeJunctionRoute` refuses a BR1 path (report its why); a path whose consecutive tracks share no node.

## 9. Gates and commits
- D453: `tools/junction/jt fast build '[planardomain]'`, and `tools/junction/jt fast build-rel '[planar]' 30` (Release: `[planar]` is over its Debug budget before this step). No wide gate: the orchestrator runs it at the merge.
- Commits: `Codechange: Junction: the route-program raster step as a helper`; `Add: Junction: PlanarDrawnHub, the contract from Slots to the back end (D452)`; `Doc: Junction: T1 v2 rig layout`. End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 10. Report
References read; commits (hash, subject); red evidence; types and members added; suite results (counts, times, failures you did not cause); the `rig:` line, judge and domain lines, prio outcome; stops and anything undone; `git status --porcelain` and `git log -1`. When you have reported, stop.
