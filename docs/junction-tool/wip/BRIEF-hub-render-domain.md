# BRIEF hub-render-domain: the drawn hub renders from the domain raster (bridge reached by a curve)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes. One probe run at the end.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Build Release `build-rel` (the rig server and the engineer's client run `build-rel/openttd`: never start, stop or signal them; rebuilding is fine). `JT_JOBS=4`, cmake `-j4`. Never touch ng2.
- Red pin (d20a0307e6, `[!shouldfail]`): "the drawn hub renders every domain-accepted drawing (T1 with curves)": F1 reaches a bridge head by a curve (half pieces heading 3, then the near head on heading 4); v1's move string can only write a bridge after a half piece as the end of an S-bend (D110, `RasterizeJunctionRoute`), so the render refuses a drawing the domain and the game accept.
- Ruling (orchestrator, amends SPEC-slots-lengths §4.1 under D452): **the hub render builds its pieces from the domain raster, not from v1 move strings.** The domain rasteriser is the game truth (D428, proven equal to `RasterizeJunctionRoute` where both can express a drawing). v1's `RenderPlanarRoutePrograms` and the shared helper stay as they are; the hub render no longer calls the helper.

## 2. Read first
- `planar_plan.cpp`: `RenderPlanarDrawnHub`, `StartTrackdirOf`, `RasterOfProgram`; `junction_route.h/.cpp`: `JunctionPiece` (tile, trackdir, units, on_bridge), `JunctionRaster` (pieces, bridges), the `'B'` case (which pieces it marks `on_bridge`: heads and middles or middles only; match it exactly), `AddRasterToLayout`; `planar_domain.h`.

## 3. Changes
- In `RenderPlanarDrawnHub`, per path: a `JunctionRaster` built directly from the path's domain pieces: `tile`; `trackdir` = of the piece's two trackdirs the one whose exit side is the edge holding `piece.to` (`StartTrackdirOf`'s rule, applied to every piece); `units` as `AppendPiece` sets them (2 for on_bridge or a diagonal trackdir, else 1); `on_bridge` exactly as v1's `'B'` case marks a bridge's pieces (Head/Middle kinds from the hub's structures); one `JunctionBridge{start, end}` per structure on the path (head tiles; tunnels stay refused). Tags, signal routes, node and port tiles unchanged.
- The cross-check stays as a test, not in the render: BR1's test A additionally renders BR1 through `RasterOfProgram` (v1 strings) and asserts identical `JunctionRaster` pieces and bridges to the domain-built one (where v1 can express a drawing, the two agree).
- `docs/junction-tool/wip/SPEC-slots-lengths.md` §4.1 "Readers": replace "built by the one game rasteriser (`RasterizeJunctionRoute` on the path's move string, cross-checked tile by tile against the domain raster)" with "built from the domain raster (the game truth, D428); BR1's test cross-checks it against `RasterizeJunctionRoute` where v1's move strings can express the drawing". Same edit in `src/junction/planar/CLAUDE.md`'s drawn hub section.

## 4. Tests
- The red pin goes green: drop `[!shouldfail]`.
- BR1's test A with the cross-check (green).
- `[planar]` Release: `RenderPlanarRoutePrograms` unchanged (the X2 exactness failure there is a known regression from the contraction remap, fixed in ng2 in parallel: ignore it, name it).

## 5. Probe and rig file
- One T1 run: `PLANAR_SLOTS_HUB=T1 PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 PLANAR_GRID_COND=1 PLANAR_STAGEA_NODEGAPS=1 PLANAR_STAGEB_CURVES=1 JUNCTION_MILP_INCUMBENT=1 PLANAR_RIG_OUT=/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/rig-2026-11-25/T1-v2-curves.json`. Report the stage B, polish, judge, domain and `rig:` lines; commit the JSON if written.

## 6. Gates and commits
- `JT_JOBS=4 tools/junction/jt fast build-rel '[planardomain]'`, `... '[planar]' 30`, `... '[contraction]' 5`.
- Commits: `Fix: Junction: the drawn hub renders from the domain raster`, `Doc: Junction: T1 v2 rig layout with curve spacing`. End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 7. Report
References read; commits; red/green; the on_bridge convention you matched (file:line); probe lines; suites; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
