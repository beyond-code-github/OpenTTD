# BRIEF contraction-diag: why stage A's contracted drawings break stage B (diagnosis on the pinned fixtures)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes. No solves: everything here runs on the committed fixtures (fixed points). This step diagnoses; it does NOT fix.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build: Release `build-rel` (built). Another agent works in ng1 in parallel: never touch ng1. Load limit: `JT_JOBS=4`, cmake `-j4`.
- The pin (5a7762be53): test "planar slots: stage A's contracted drawing satisfies stage B's rows (T1, T2)" (`[contraction]`, `[!shouldfail]`), fixtures `tools/junction/topologies/t1-stagea.drawing.txt`, `t2-stagea.drawing.txt`, options from `StageBOptions`. Red:
  - **T1:** stage B's program builds; `CheckPlanarProgramOnPoints` breaks `end 1`, `integer half`, `bound half`.
  - **T2:** stage B's program is not built: "unsupported: structure 4's crossings sit on different pieces of its separator".

## 2. Read first (once each, with offset/limit)
- `src/tests/junction/planar/planar_domain_encoding.cpp`: the pin test, `StageBOptions`, `ContractedVariant` (:1030) and what it calls.
- `src/junction/planar/planar_draw.cpp`: where the `end` rows are written (grep `"end {} `), the group named `half` (grep `"half"`), and the check that returns "crossings sit on different pieces of its separator".
- `src/junction/planar/planar_draw.h` 80-125 (`PlanarProgram`, `PlanarProgramCheck`).

## 3. Questions (answer each with file:line and numbers from the fixtures)
**T1**
1. `end 1`: the row as written (terms named by track/piece/point where possible, bounds), its value on the contracted points, and the same quantity on stage A's uncontracted points (the fixture before contraction). Did stage A have this row? (stage A options vs `StageBOptions`: `end_node`, `node_gaps`, anything else that differs.) If stage A had it and held it, what did the contraction change?
2. `integer half` and `bound half`: which variables are in group `half`, what they mean, which one is determined non-integer or out of bounds, its value, and the points it is computed from. Is a contracted point off the half-tile grid stage A kept (grid rows), or is it a stage B-only variable?

**T2**
3. Structure 4: its separator track, its crossings (with which tracks), and for each crossing the separator piece index and heading in stage A's uncontracted variant and in the contracted variant. Which contraction step put them on different pieces (a zero-length piece between them removed, a flex group contracted across the span, a step kept, ...)? Quote the contraction code that does it.

**Both**
4. For each cause: one sentence stating it as a rule that the contraction or stage B breaks, and which of the two is wrong per the contract of `ContractedVariant` (its doc comment) and stage B's options. Do not propose code.

## 4. Changes
1. Remove the `[planardraw]` tag from the pin test (keep `[junction][contraction][!shouldfail]`): `[planardraw]` was over its 5 s budget with it (5.138 s).
2. Diagnosis output: `/home/pete/OpenTTD/.claude/ng2/docs/junction-tool/wip/probes/slots/contraction-diag.md`, at most 80 lines, the answers to part 3. Any temporary test code you add to print values is removed before the commit (or kept as a hidden `[.]` test named `contraction diag` if it is the only way to reproduce the numbers: say which).

## 5. Do not
- No fix to the contraction, any row, option or default; no new knob. No solves.
- Never touch ng1, the rig or game clients. No `git stash`. Source edits only with the Edit or Write tool.

## 6. Gates and commit
- `JT_JOBS=4 tools/junction/jt fast build-rel '[contraction]' 5`; `JT_JOBS=4 tools/junction/jt fast build-rel '[planardraw]'`.
- Commit: `Doc: Junction: contraction diagnosis on the pinned stage A drawings` (plus the tag change), ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 7. Report
References read; commit; the part 3 answers in at most 15 lines (the file has the detail); suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
