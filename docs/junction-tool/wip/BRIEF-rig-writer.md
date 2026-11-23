# BRIEF rig-writer: a v2 drawing as v1 route programs, rendered and signalled for the rig (handover step 2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes in all. The probe runs in part 6 cap every solve at 15 s and run once each.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Builds: Debug `build`, Release `build-rel`.
- Why (engineer): "we can't really judge anything by a drawing that we can't place". The rig has built v1 hubs many times from `PlanPlanarJunction`'s path: route programs -> `RenderPlanarRoutePrograms` -> `SignalJunctionTrackRoutes` -> `JunctionLayoutToJson`. The v2 drawing (points per track) has never entered that path. This step converts it into route programs so the existing render and signal pass do the rest. Recon: `docs/junction-tool/wip/RECON-rig-v2.md`. Flat ground: no levels, no tunnels.

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md`: "Back end (`planar_plan`)", the `PlanarRouteProgram` bullet of the component solver, "Track map", and the last two Drawing program bullets (Domain, `PlanarDomainInputOf`).
- `src/junction/planar/planar_components.h` lines 20-45 (`PlanarProgramPiece`, `PlanarRouteProgram`, `PlanarComponentLayout`).
- `src/junction/planar/planar_plan.cpp` lines 192-345 (`RenderPlanarRoutePrograms`, `PlanPlanarJunction`'s signal call).
- `src/junction/planar/planar_domain.h` (whole) and `src/junction/planar/planar_domain_map.{h,cpp}` (`PlanarDomainInputOf`, `AddStructure`).
- `src/tests/junction/planar/planar_domain_encoding.cpp` lines 3652-3700 (BR1) and 7040-7075 (the `[slotsstep2]` polish and domain verdict).

## 3. The conversion (decided)
`PlanarComponentLayout PlanarDrawingPrograms(const PlanarShapeVariant &v, const std::vector<std::vector<std::array<int, 2>>> &points, std::string &why)`:
- `in = PlanarDomainInputOf(v, points, notes)`; the topology is the track map's expanded topology `T = v.structure.map.expanded` (its lines are the tracks; copies are tracks with their own route paths, so every program has `copy` 0 and no `copy_switches`). Premise to check first: `in.tracks[i]` is expanded line i for every i. If not, STOP.
- For each route r of T and each path p of `T.routes[r].paths` (in order): one `PlanarRouteProgram{r, p, 0, pieces, {}}`. Its pieces: for k over the path's lines, `RasterPlanarDomainTrack(in.tracks[path[k]]).pieces` in order (an empty raster, a zero-length track, adds nothing), each as `{piece.tile, piece.heading, kind, k}`.
- kind: on a track that is the separator of a domain structure, the pieces `first` and `last` are `Head` and the pieces strictly between are `Middle`; every other piece is `Ground`. A tunnel structure: `why` "tunnel <name> not built (no levels)", return.
- A raster with a non-empty `rule`: `why` names the track and the rule, return.
- Shift every piece's tile by one whole-tile offset so the least x and least y over all programs are 0.
- `width`/`height` the bounding box; `variant` "v2"; other fields default.
Then the caller renders exactly as `PlanPlanarJunction` does on flat ground: `RenderPlanarRoutePrograms(T, layout)`, then `SignalJunctionTrackRoutes(layout, routes, network, spec, &why, &prio_beside)`.
Prio rows (`prio:<line>` tracks) carry no route, so they get no program: the signal pass places the prio element itself, as in v1. Report what it did on T1.

## 4. Tests, outermost first
**Test A** (`src/tests/junction/planar/planar_domain_encoding.cpp`, tags `[junction][planar][planardomain]`): "planar domain: a v2 drawing renders through the v1 route programs (BR1)". BR1's base drawing and variant -> `PlanarDrawingPrograms` (why empty) -> `RenderPlanarRoutePrograms(T, ...)` (why empty) -> its layout's track bits per tile equal the union of `RasterPlanarDomainTrack` pieces over every track, minus the bridge pieces `first..last` of each structure's separator; the layout has one bridge per structure, at the head tiles. This is the cross-check of the two rasterisers on one drawing. Red first: a stub returning an empty layout with why "stub"; record the failure text.
**Test B** (same file and tags): a drawing with one point moved off the grid gives `why` naming that track.
Budget: `tools/junction/jt fast build '[planardomain]'`.

## 5. Changes
1. `planar_domain_map.h/.cpp`: `PlanarDrawingPrograms` as in part 3, doc comment citing this brief.
2. `[slotsstep2]` probe (planar_domain_encoding.cpp, after the domain verdict ~:7068): when env `PLANAR_RIG_OUT=<path>` is set and the domain accepts: convert, render, signal (spec as the probe has it), and write `JunctionLayoutToJson(layout)` to `<path>`; print `rig: <path> size WxH tracks N bridges M signals S` and, when the render or the signal pass fails, its why. If the signal pass fails, still write the unsignalled render layout to `<path>` and say so in the line.
3. `src/junction/planar/CLAUDE.md`: one bullet under the domain bullets for `PlanarDrawingPrograms` (decided facts only).

## 6. Probe runs (after the tests are green; Release; scratch output under your session scratchpad)
In `/home/pete/OpenTTD/.claude/ng1/build-rel`:
`PLANAR_SLOTS_HUB=T1 PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 JUNCTION_MILP_INCUMBENT=1 PLANAR_RIG_OUT=<scratchpad>/T1-v2.json ./openttd_test '[slotsstep2]' > <scratchpad>/t1.log 2>&1`.
Copy the JSON to `/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/rig-2026-11-25/T1-v2.json` (create the folder) and commit it. Report the `rig:` line, the judge and domain lines, and what the signal pass did with the prio (placed / why).

## 7. Do not
- No levels, terraform, tunnels. No change to `RenderPlanarRoutePrograms`, `SignalJunctionTrackRoutes`, `CheckPlanarDomain`, `RasterPlanarDomainTrack`, any solver or default. No knob besides `PLANAR_RIG_OUT`. No tile-level writer as a fallback: a render failure is reported.
- Never start a game client or server, never touch the rig (ports 4077/4079) or other worktrees. No `git stash`. Source edits only with the Edit tool (no Python or shell rewrites, not even for temporary tests).

## 8. Stop conditions
- `in.tracks` not indexed like the expanded lines; a path whose consecutive tracks do not meet (last point != next first point after the domain input's merges); `RenderPlanarRoutePrograms` refusing BR1 (report its why: it names whether the move rules or the tile check failed); a bridge head piece not axis.

## 9. Gates and commit
- `tools/junction/jt fast build '[planardomain]'`, `tools/junction/jt fast build '[planar]'`, `tools/junction/jt gate build-rel` (a SIGSEGV/SIGILL in an unrelated test that passes alone is CPU instability: rerun it alone and say so).
- Commits: `Add: Junction: PlanarDrawingPrograms, a v2 drawing as v1 route programs for the rig`, then `Doc: Junction: T1 v2 rig layout`. End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 10. Report
References read; commits (hash, subject); red evidence; types and members added; suite results (counts, times, failures you did not cause); the `rig:` line, judge and domain lines, and the prio's fate; stops and anything undone; `git status --porcelain` and `git log -1`. When you have reported, stop.
