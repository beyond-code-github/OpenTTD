# GS1: move the shared types out of legacy/junction_generator.h into src/junction/junction_spec.{h,cpp} (identity step)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -b wt-gs1 <head>` with the head given in your launch prompt. Builds: `build` (Debug) and `build-rel` (Release). Ignore docs/junction-tool/wip/. Never use git stash.
- The tool now lives in src/junction/ (see docs/junction-tool/PATHS.md and src/junction/CLAUDE.md). legacy/junction_generator.h mixes types that every folder uses with the superseded generator's API. 23 files outside legacy/ include it. This step separates the two, so live code stops depending on legacy/. Behaviour is identical.

## Read first
src/junction/CLAUDE.md, src/junction/legacy/CLAUDE.md, src/tests/junction/CLAUDE.md, CODINGSTYLE.md (the include and doc-comment sections), and src/junction/legacy/junction_generator.h (all 234 lines).

## The changes
1. New `src/junction/junction_spec.h`, with the standard OpenTTD file header, an `@file` line and include guard JUNCTION_SPEC_H. Move these declarations into it VERBATIM from junction_generator.h, with their doc comments, in this order:
   - `JunctionNotationError`
   - `JunctionNotation`
   - `ParseJunctionNotation`
   - `JunctionUnitPrices`
   - `JunctionSpec`
   - `CurveSpeedLimitForSpacing`
   - `DeriveCurveSpacing`
   - `JunctionRampLevels`
   - `MakeTestJunctionSpec`
   - `JunctionPrioSizing`
   - `SizeJunctionPrio`

   Give it exactly the includes those declarations need. If a declaration needs a type that is declared only in junction_generator.h, stop.
2. New `src/junction/junction_spec.cpp`. Move the definitions of those six functions VERBATIM from legacy/junction_generator.cpp (lines 46, 75, 90, 108, 124 and 1971 at the time of writing), with their comments, plus any file-static helpers and constants that ONLY they use. If a helper or constant is also used by code that stays in junction_generator.cpp, stop and report it. Do not duplicate it.
3. `junction_generator.h` includes "../junction_spec.h" in place of the moved declarations. Everything else in it stays.
4. Includes, file by file, for every file that includes junction_generator.h, outside legacy/ and in the tests:
   - If the file uses no symbol that remains in junction_generator.h, replace that include with junction_spec.h, using the correct relative path.
   - Otherwise keep the include as it is.
   - Decide by grepping the file for the remaining symbols: `JunctionGenerateError`, `JunctionGenerateResult`, `GenerateJunction`, `JunctionFrame`, `MakeJunctionFrame`, `JunctionRouteCache`, `JunctionCandidateParams`, `BuildJunctionCandidate`, `ScoreJunctionLayout`, `JunctionTerrainPlanes`, `JunctionLibrary*`, `FitJunctionLibrary`, `JunctionTMap*`, `ConstructJunctionTMap`, `JunctionEWindowPhase`, `JunctionNodeTopologyT`, `JunctionBlockedPortApproach`, `RuleViolation` forward use.
   - The compiler confirms the result.
   - List the files that keep junction_generator.h in your report. These are live code still using legacy.
5. CMake: add junction_spec.{h,cpp} to src/junction/CMakeLists.txt's add_files, in alphabetical order.
6. CLAUDE.md:
   - src/junction/CLAUDE.md: add `junction_spec` (shared spec, notation, prices, curve spacing, prio sizing) to the root-files line.
   - src/junction/legacy/CLAUDE.md: replace the "generator.h types used by many headers" fact with the list of files that still include junction_generator.h after this step.
   - docs/junction-tool/PATHS.md: add one line saying that the symbols moved to junction_spec.{h,cpp}.

## Do not
Rename, reformat or edit any moved declaration or definition. Change no behaviour. Do not touch any other legacy code, tests' expectations, tags or hashes. Do not split junction_topology.cpp.

## Stop conditions
- A moved symbol depends on something that stays in legacy.
- Any identity hash or test count changes.
- Any premise above is false.

## Gates
- Before the change, run `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root to get a baseline.
- After the change:
  - Debug `build` and `build-rel` both build `openttd` and `openttd_test`, with no new warnings. The baseline is 16 known warnings in legacy/junction_nodes.cpp and topology.
  - `jt gate` and `jt identity` show the same counts as the baseline.
  - `tools/junction/jt fast build '[topology]'` passes.
- One commit: `Codechange: Junction shared spec types move out of the legacy generator`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read.
- Commit hash and subject.
- Baseline and after counts.
- Symbols moved.
- Includes switched, as a count, plus the list of files that keep junction_generator.h.
- Any helper you had to move with the functions.
- Stops.
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
