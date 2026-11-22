# BRIEF domain-17b: the grid run's domain verdicts (harness bug)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. Test code only. One Release run per configuration with `span+lead+endnode+crosspiece+grid`. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- domain-17 (ae7255b562) reports 885 too-strict polylines on X1 under `+grid`, but at least some are off the grid. Example from `encoding-x1-span_lead_endnode_crosspiece_grid.txt`, group `[integer]`: B1 `0*2,1*2,0*2,7*3,0*8` from (11,10) runs north on x = 10. `RasterPlanarDomainTrack` on those points says `grid` ("the piece to (10, -1) does not join two edges of one tile"), so the domain rejects it, yet the report files it as domain-legal (too strict). A domain verdict is being taken from something other than the polyline's own points, or reused across polylines.

## 2. Changes (harness only)
1. Find where the grid run gets the domain verdict for each polyline (blind stream, by-construction stream, the per-polyline cache) and make it `CheckPlanarDomain(PlanarDomainInputOf(fixture variant, the polyline's own points, notes))` for every polyline, every stream. Report in one line what was wrong.
2. A guard in the report path: before a polyline is classed too strict, `REQUIRE` every track's `RasterPlanarDomainTrack` on its points has no rule (a too-strict polyline is on the grid by definition).
3. Fix the by-construction walker's gap (domain-17 finding 1): also refuse a turn over one eighth and two consecutive pieces on one tile (the raster's `turn` rule), so it produces only geometries the raster accepts. Report the refused count per configuration (it must be 0 after the fix).

## 3. Test (fast)
- "planar domain: a too-strict polyline is on the grid": the example above is not too strict under `+grid` (it is domain-rejected). Red: before the fix it is classed too strict.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`.
- Release once each: X1, X2, M1, K1 with `span+lead+endnode+crosspiece+grid` (overwrite), timeout 300.
- One commit: `Fix: Junction: the domain suite's grid run takes each polyline's own domain verdict (D428)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; what was wrong; red evidence; per configuration the classes with group keys and counts and first examples; walker refusals; state. When you have reported, stop.
