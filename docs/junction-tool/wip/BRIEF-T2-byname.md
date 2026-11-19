# BRIEF T2N: remove T2's by-name branch from the product path

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-t2name, head HEAD_HASH. Builds: build-rel (Release), build (Debug). Write logs under /tmp/claude-1000/-home-pete-OpenTTD/83ff9380-9534-4138-8dc1-79a833146e49/scratchpad/t2n/.
- Implements: docs/junction-tool/DESIGN-tracks.md §17.6 "Independent, now: remove the by-name branch for T2 from the product path" (engineer ruling: "T2 now, LL² with SSS"); review wip/REVIEW-architecture-holistic.md §5 table row "hand Composer, Block227, the by-name branch: drop from the product path now (D326, D330)" and §7 suggested test 5.
- What it means: `SolvePlanarComponents` (src/junction/planar/planar_components.cpp, about line 3034) sends a topology named "T2" to the hand `Composer`. After this step it sends T2 to `SolvePlanarMapped`, like every topology other than LL2. LL2 keeps the hand composer (it moves later, with SSS).

## Read first (once each, with offset/limit)
1. CODINGSTYLE.md (the repo root).
2. src/junction/planar/planar_components.cpp: lines 100-180 (`LL2Routes`, `LL2Outline`, `T2Routes`, `T2Outline`, the `Composer` class header), every line containing `t2` (grep -n '\bt2\b' gives 28 lines), lines 2530-2550 (`SolveVariant`), 3034-3090 (`SolvePlanarComponents`).
3. src/tests/junction/planar/planar_components.cpp lines 30-105 (`CheckSolves` and the area tests).
4. src/junction/planar/CLAUDE.md lines 99-130 ("Back end" and "Component solver").
5. docs/junction-tool/TESTS.md: the `jt` usage at the top and the `[planarcomponents]` rows.

## Tests (outermost first)
1. **New, in src/tests/junction/planar/planar_components.cpp,** tags `[junction][planarcomponents][slow]`: `TEST_CASE("planar components: T2 by name reaches the generic solver", ...)`. Parse `PlanarTText(2)`, call `SolvePlanarComponents(topology, MakeTestJunctionSpec(), layout, &why)`, `REQUIRE` ok, then `CHECK(layout.variant.rfind("o", 0) == 0)` (a mapped variant tag, `o..t..f..s..`; the hand composer's tags look like `+1F1w-1`) and `CHECK(layout.rungs > 0)` (only the mapped solver sets rungs). Expected red before the change: both CHECKs fail on the assertion (variant like `+1F1w-1`, rungs 0). Record the exact failure lines. If it fails any other way (compile error, REQUIRE), stop.
2. **Existing outer test, must stay green after the change:** src/tests/junction/planar/planar_plan.cpp `"planar: T2 builds from its component layout and passes the checker"` (it goes through `PlanarSeedFor` -> `SolvePlanarComponents`, so after the change it runs the mapped T2 end to end through the checker). Run it before the change too and record pass/fail and time. If it fails AFTER the change, stop and report the violation list (it means the mapped T2 is not checker clean end to end; that is the engineer's question, not yours to fix).
3. Existing `"planar components: T2 from its topology text (mapped) at the probe's area"` (area <= 963) must stay green.

## Changes
1. `SolvePlanarComponents`: replace
   ```cpp
   const bool t2 = topology.name == "T2";
   if (topology.name == "T1") return fail(...);
   if (!t2 && topology.name != "LL2") return SolvePlanarMapped(topology, spec, layout, why);
   ```
   with the T1 line unchanged and `if (topology.name != "LL2") return SolvePlanarMapped(topology, spec, layout, why);`. Then make the rest of the function LL2-only: the route check loops over `LL2Routes()`, `variants = {{1, 0}, {1, 1}, {-1, 0}, {-1, 1}};` with no `if`, and the thread calls `SolveVariant(topology, params, variants[i])`.
2. `SolveVariant`: drop the `bool t2` parameter and its `@param` line; construct `Composer composer(params, variant);`.
3. `Composer`: drop the `t2` constructor parameter, its `@param`, and the `bool t2;` member. At every use of `t2` (the 28 lines from Read first item 2), keep only the LL² branch: `t2 ? A : B` becomes `B`; `if (t2) { X } else { Y }` becomes `Y`; `if (t2) { X }` with no else is deleted; a `t2` argument passed to a helper (for example `this->Fray(..., t2, ...)`) becomes `false`, and if that helper's parameter is then always false, leave the helper unchanged (do not fold it; report it under "left undone"). Initialisers: `wdiag(variant.other)`, `join_s(1)`.
4. Delete `T2Routes()` and `T2Outline()` and their comments.
5. Comments: every comment that says the hand composer serves "T2 or LL²" (for example the `Composer` class comment, line ~162 "Builds the model of one variant of T2 or LL²") is changed to say LL² only. Do not delete any other comment.
6. Tests: delete `TEST_CASE("planar components: T2 solves at the probe's area", ...)` (lines ~82-85); it now duplicates the mapped test at line ~92.
7. src/junction/planar/CLAUDE.md: line ~101 "(T2, LL², `seed.components`, `bound` 0)" becomes "(LL² by its hand composition, every other topology but the one-lane T by `SolvePlanarMapped`; `seed.components`, `bound` 0)"; line ~115 "has a hand composition per topology name, `T2` (`PlanarTText(2)`) and `LL2` (`PlanarLL2Text()`)" becomes "has a hand composition for `LL2` (`PlanarLL2Text()`) only; T2 goes to `SolvePlanarMapped` (D326)"; line ~123 "Four variants per hand composition (LL²: ...; T2: S fray side x S join side)" drops the T2 part; line ~128 drops "and T2 at 935 tiles (`+1F1w-1`, 89x33)". No other CLAUDE.md edits.

## Do not
- Do not touch `MappedComposer`, `SolvePlanarMapped`, LL² behaviour, the topology texts, or any area limit.
- Do not fold or simplify helpers beyond removing `t2`; do not reformat untouched code.
- Do not move tests between tags or change budgets.
- Do not fix anything the mapped T2 does wrong end to end.

## Stop conditions
- The new test is not red for the stated reason.
- The T2 end-to-end checker test fails after the change.
- Any LL² test changes result (LL² must be bit-identical: `tools/junction/pv run build-rel LL2` then `pv check build-rel LL2` exit 0; and the LL² hand-composer test "planar components: LL² solves at the probe's area" passes with the same area as before; record both areas).
- Anything not covered.

## Gates
- Release build (`cmake --build build-rel -j 14 --target openttd_test > log 2>&1`), zero new warnings in the touched files.
- `tools/junction/jt fast build-rel planarcomponents 5` (budget per TESTS.md), plus run singly the [slow] tests named above and "planar: LL² builds from its component layout and passes the checker" and "planar components: LL² solves at the probe's area", by exact name via `build-rel/openttd_test "<name>"`, before and after; record times.
- `tools/junction/jt gate build-rel` green (budget 30 s; over budget is a stop).
- Commit: `Change: Junction planar T2 goes to the mapped solver; the hand composer is LL² only` with the trailer line `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read
- Commits: hash and subject
- Red evidence: the new test's failure lines before the change
- Types and members added or removed: names only
- Suite results: each named test before/after (pass, area, time), pv LL2 check, gate result and time; any failure you did not cause
- Stops, and anything left undone (e.g. helpers whose parameter is now always false)
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
