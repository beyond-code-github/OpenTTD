# BRIEF domain-14: the suite judges contracted rows (D432)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Test code only. Run Release measurements once each: X1 and X2, rule set `span+lead+endnode+crosspiece` only. No wide gate (no production change); `[planardomain]` and `[planar]` fast tags only. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D432 (DECISIONS.md): Slots writes its rows over contracted lines: no piece of length 0; an unused S-bend does not exist; a straight run is one piece. The harness today judges the drawing program's uncontracted rows and searches every piece-length representation of a polyline (arc consistency, caps, undecided, "other shape" from zero-length crossing pieces). Under D432 each polyline has exactly one writing: judge that, once.

## 2. The change (harness file `src/tests/junction/planar/planar_domain_encoding.cpp` only)
1. **Contracted variant of a polyline** (static helper `ContractedVariant(fixture, polyline points) -> optional<PlanarShapeVariant>` plus its points): per track, its runs (heading mod 8, length) from the polyline. Find one representation of the runs on the variant's piece list (the existing `Representations` with cap 1); none: the polyline is **other shape**. From that representation, the contracted track keeps exactly the pieces with length > 0, in order, with their lifted headings (from the variant's piece list) merged when consecutive equal headings (sum the lengths), `flex` and `steps` empty, and its points = the polyline's corners. Each event piece is remapped to the contracted piece holding the crossing: the piece whose closed segment holds the intersection of the two tracks' polylines and whose heading (mod 8) equals Shape's event piece heading (mod 8); none such: **other shape**. Copy everything else from the fixture's variant. Run `RefinePlanarShapeCells` only if the read-out refuses without it (say which).
2. **Judging:** per polyline, build the read-out (`keep_apart_out`, `program_out`, `build_only`, the rule set's options) on its contracted variant and judge the polyline's points once: every keep-apart pair holds, every lead row holds, `CheckPlanarProgramOnPoints` breaks nothing. Classes: other shape, both accept, both reject, too strict, too loose (no undecided).
3. **Remove** the representation search, arc consistency, caps and the `undecided` class from the report path (keep `Representations` for step 1). Report wall time per configuration.
4. **Fast tests:** replace the X2 base test's search with the contracted judgement (it must stay well under 0.1 s Debug); update the tests that used the search or arc consistency to the contracted judgement, or delete those whose only subject was the search (list which).

## 3. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planar]' 15` (report times; `[planardomain]` should drop well below 1 s).
- Release once each: X1 and X2 with `span+lead+endnode+crosspiece` (overwrite those two report files), timeout 300.
- One commit: `Change: Junction: the domain suite judges contracted rows, one writing per polyline (D432)`, Co-Authored-By Claude Sonnet 5.5.

## 4. Stop conditions
- The read-out refuses a contracted variant in a way the brief does not cover; the base polylines of X1 or X2 are not both accept; anything not covered.

## 5. Report (short)
Commit; per configuration the classes (other shape, both accept, both reject, too strict, too loose) with group keys, counts and first examples; wall times; the fast-tag times; the tests removed or changed; state. When you have reported, stop.
