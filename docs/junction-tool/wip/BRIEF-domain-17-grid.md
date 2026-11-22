# BRIEF domain-17: the grid rows judged; on-grid enumeration by construction

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Test code only (no gate). One Release run per configuration, rule set `span+lead+endnode+crosspiece+grid`. Record findings; fix no rule. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Engineer: "the only truly legal geometries are on-grid ones." Slots must produce whole tiles by construction (D431; D382's grid rows, `PlanarDrawOptions::grid`), but the suite filters off-grid geometries out before judging, so the grid rows are never tested; and the enumerator picks lengths blindly, so most of each configuration is wasted off grid (M1: 78k of 105k).

## 2. Changes (harness `src/tests/junction/planar/planar_domain_encoding.cpp`)
1. **Rule-set suffix `+grid`:** sets `grid = true` on the read-outs (program and keep-apart). `CheckPlanarProgramOnPoints` then sees the grid rows (forced variables written 2 h + p, `grid link` rows; an off-grid point leaves an h non-integer: `integer <group>` in `broken`).
2. **Off-grid geometries are judged, not filtered:** with `+grid`, a geometry off the grid (the domain's raster gives "grid") is a polyline like any other: domain rejects it; rows accept it (contracted judging) = **too loose**. Group too-loose off-grid cases by the first off-grid track and its corner index. Without `+grid` keep today's "off grid" class.
   - Contracted writing of an off-grid polyline: as now (runs from the points); if `ContractedVariant` cannot write it, it is other shape.
3. **On-grid enumeration by construction:** a second enumeration mode, used for every configuration's on-grid part: instead of independent length lists, each configuration axis gives its candidate lengths, and the generator walks each varied track from its start, offering at each piece only the lengths that keep every corner on an edge midpoint and every axis piece on its centre line (east-west on odd y, north-south on odd x; a diagonal half tile flips the parity). Report per configuration how many geometries each mode produced.
   - The off-grid judging (2) uses the existing blind enumeration's off-grid geometries.

## 3. Test (fast)
- "planar domain: the grid rows reject an off-grid drawing": X1's base shifted by one half tile in x on every track (all corners off the grid): with `grid` on, `CheckPlanarProgramOnPoints` breaks an `integer` or `grid` row; without `grid`, it breaks none. Red: record without the `+grid` handling.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` (under 1 s Debug).
- Release once per configuration (X1, X2, M1, K1) with `span+lead+endnode+crosspiece+grid` (reports `encoding-<cfg>-span_lead_endnode_crosspiece_grid.txt`), timeout 300 each.
- One commit: `Change: Junction: the domain suite judges the grid rows and enumerates on the grid by construction (D428, D431)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; red evidence; per configuration: on-grid geometries generated, classes (other shape, both accept, both reject, too strict, too loose) with group keys, counts and first examples (off-grid too-loose groups by track and corner); wall times; state. When you have reported, stop.
