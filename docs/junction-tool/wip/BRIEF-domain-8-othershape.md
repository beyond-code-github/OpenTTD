# BRIEF domain-8: set apart polylines that realise another shape

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. Test code only. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The X1 suite's 42 too-strict polylines (`[bound | end]`) are polylines where a crossing lies on another piece than the one Shape assigned (e.g. B1 crossing A1 on its diagonal step, while Shape put the crossing on B1's piece 4). The rows rightly enforce Shape's choice; such a polyline belongs to another shape, not to this variant. The harness must set them apart before judging.

## 2. The change (harness file only)
- **Conformance of a representation:** for each crossing c of the map (`v.structure.map.crossings[c]`, tracks a and b, each with its event piece from `v.tracks[t].event_piece` for c's position in `map.events[t]`): the two assigned pieces (segments from point k to point k + 1 of each track) meet in one point that lies on both closed segments. A zero-length assigned piece meets only if its single point lies on the other segment.
- **Other shape:** a domain-legal or domain-illegal polyline is class **other shape** when no joint representation is conformant: run the arc-consistency pass with only the conformance constraints (binary, per crossing over its two tracks); an emptied list means other shape. Otherwise judge it as now, but with the conformance constraints added to the AC-3 pass and the search (a representation that is not conformant never counts as accepted).
- Report "other shape" per configuration and in the totals (polylines and geometries); list the first 3 with their polylines.

## 3. Test (fast, harness file): "planar domain: other shape is set apart"
The 42-group's first example (B1 pieces 2 = 4, 3 = 2, 4 = 2, 5 = 2, 6 = 4 over the base) is other shape; the base polyline is conformant. Red: before the change the first is not other shape.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel`.
- Release `plain+lead` and `span+lead` (overwrite), timeout 300.
- One commit: `Change: Junction: the encoding report sets apart polylines of another shape (D428)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; red evidence; per set the totals (other shape, both accept, both reject, too strict, too loose, undecided) and group keys with counts and first examples; suites; state. When you have reported, stop.
