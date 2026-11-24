# BRIEF hub-render-bridge: every domain-accepted drawing renders (bridge head after a diagonal)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes. One probe run (T1 with curves) to save the fixture; after that no solves.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Build Release `build-rel` (the rig server and the engineer's client run `build-rel/openttd`: never start, stop or signal them; rebuilding is fine). Load limit `JT_JOBS=4`, cmake `-j4`. Never touch ng2.
- Evidence (91fc4ac0d6's run 2): T1 with `PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 PLANAR_GRID_COND=1 PLANAR_STAGEA_NODEGAPS=1 PLANAR_STAGEB_CURVES=1 JUNCTION_MILP_INCUMBENT=1`: polish Optimal, judge accept, domain accept 0 breaks, but `RenderPlanarDrawnHub` fails: "route F1 does not rasterise: bridge at position 15 must start from an axis piece". The contract (SPEC-slots-lengths §4.1, D452): the render gives the back end every hub the domain accepts.
- Code: `RasterOfProgram` in `src/junction/planar/planar_plan.cpp` builds the move string (a Head piece emits `B<middle>`) and calls `RasterizeJunctionRoute` (`src/junction/junction_route.cpp` ~:100-135: a bridge from a non-axis current trackdir uses the last L/R turn to find its head trackdir, D110).

## 2. Read first
- `planar_plan.cpp` `RasterOfProgram`, `StartTrackdirOf`, `RenderPlanarDrawnHub`; `junction_route.cpp` the `'B'`/`'T'` case; `planar_hub.h`; the `PLANAR_RIG_OUT` hook in the probe.

## 3. Steps
1. **Fixture:** add env `PLANAR_FINAL_SAVE=<file>` to the probe, writing the final (polished) drawing's points per track (format: `track <name> x0 y0 x1 y1 ...`, header comment line), run T1 once with the knobs above, commit the file as `tools/junction/topologies/t1-final-curves.drawing.txt`.
2. **Pin (red first):** test `[junction][planardomain]` "the drawn hub renders every domain-accepted drawing (T1 with curves)": T1's variant (as the probe builds it, contracted `plain`) + the fixture points -> `PlanarDrawnHubOf` -> `CheckPlanarDrawnHub` empty -> `RenderPlanarDrawnHub` why empty -> layout tiles equal the domain raster minus bridges (as BR1's test A). Record the red text.
3. **Diagnosis (in the report, at most 10 lines):** F1's pieces around position 15 (headings, kinds, the moves emitted), what `RasterizeJunctionRoute` expects there, and why they differ (for example: the planned head is the axis piece after a diagonal and the helper emits `B` from the diagonal, where v1's programs always have the closing turn as its own piece; or the last turn found is in the wrong direction).
4. **Fix only if it is local to the hub's move building** (`RenderPlanarDrawnHub` or the hub's call into the helper: e.g. emit the closing turn onto the head's heading as its own move before `B`, when the planned pieces have it), with `RenderPlanarRoutePrograms`' behaviour unchanged (`[planar]` Release 213 of 213) and `RasterizeJunctionRoute` unchanged. If the fix needs `RasterizeJunctionRoute` or v1's helper behaviour changed, STOP after committing the red pin (`[!shouldfail]`) and the diagnosis.
5. After a fix: run the probe once more with `PLANAR_RIG_OUT=/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/rig-2026-11-25/T1-v2-curves.json` (same knobs) and commit the JSON.

## 4. Do not
- No change to `RasterizeJunctionRoute`, the domain, any solver or default. Source edits with Edit/Write only.

## 5. Gates and commits (D453)
- `JT_JOBS=4 tools/junction/jt fast build-rel '[planardomain]'`, `JT_JOBS=4 tools/junction/jt fast build-rel '[planar]' 30`.
- Commits: `Add: Junction: pin, the drawn hub renders T1 with curve spacing` (fixture, knob, test), `Fix: Junction: ...` (if fixed, a subject naming the cause), `Doc: Junction: T1 v2 rig layout with curve spacing` (JSON). End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 6. Report
References read; commits; red text; diagnosis; fixed or stopped; the `rig:` line; suite results; `git status --porcelain`, `git log -1`. When you have reported, stop.
