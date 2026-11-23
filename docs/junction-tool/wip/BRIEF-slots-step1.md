# BRIEF slots-step1 (measurement): stage A's lazy loop with free binaries, a tight bound and a feasibility objective (D446)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Every solve is capped at 15 s, except the polish, which is capped at 3 s. The loop runs at most 12 rounds per hub. Run each hub once, with no reruns. Change no production code (nothing under `src/junction/`).

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Ruling: DECISIONS D446 (read it in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md`; it is the last row), and D444/D445. Evidence: `docs/junction-tool/wip/probes/slots/step0.md`. On T2's stage A round 1, `coord_max` 64 plus `tidy = false` gave Optimal in 582 ms with 204 free binaries. Fixing the sides from the breaking drawing was Infeasible.
- The question: with D446, does stage A's lazy loop CONVERGE on T2 (offers 1, D443 off), in how many rounds and how long? And does a 3 s length polish keep the drawing legal and shorten it? T1 is the regression.

## 2. Read first (once, with offset/limit)
`src/tests/junction/planar/planar_domain_encoding.cpp`: `LazyRun`, `LazyBestCandidate` and `LazyLoop` (about lines 5815-5990); the lazy test (about 5992-6100), for the variant build and `sa`; the `[slotsstep0]` test (at the end of the file), for `tight` and the failing-pairs count.

## 3. The changes (test file only)
1. `LazyLoop` gets two parameters at the end of its signature, both defaulted so the existing calls are unchanged: `bool fix_sides = true, bool tight_bound = false`. Update its doc comment to say what each does.
   - With `fix_sides == false`, a round's failing pairs are appended to `free_list` (binaries) instead of `fixed`. No pair is ever fixed, so the Infeasible branch never frees any.
   - With `tight_bound == true`, every round after the first Optimal round sets `o.coord_max = std::max(64, (2 * biggest + 15) / 16 * 16)`, where `biggest` is the largest |coordinate| over `run.last.points`. If such a round is Infeasible or Error, it is solved ONCE more with `o.coord_max` doubled: write both rows, the second with stage text `<stage>+` (e.g. `A+`), and count both in `run.rounds` and `run.ms`. If the retry is not Optimal either, the loop stops as it does today, with `stop` set to the retry's status. Round 0 keeps the base's `coord_max`.
2. A hidden test `TEST_CASE("planar slots: stage A lazy loop under D446 on T1 and T2 (step 1)", "[.][slotsstep1]")` after the `[slotsstep0]` test. For each hub of T1 and T2 (env `PLANAR_SLOTS_HUB` limits it to one, as in the lazy test):
   - Build the variant with offers 1 and D443 off, exactly as the `[slotsstep0]` test does for T2. Copy it; do not factor it out.
   - `sa` as in the lazy test (`SlotsOptions(fa, DEFAULT_RULES)`, `node_gaps = false`), plus `sa.tidy = false`.
   - `const LazyRun a = LazyLoop(v, sa, {}, tag, "A", md, false, true);` writes its round table as the lazy test does: the header `| stage | round | status | ms | pairs fixed | pairs free | binaries | failing pairs | freed |`, with the tag `STEP1 <hub>`. Then one summary line: rounds, converged, stop, pairs with rows of all pairs, total ms.
   - If `a.converged`, run the polish: `sa` with `tidy = true`, `keep_apart_out = true`, `only_pairs_on = true`, `only_pairs = a.list`, `coord_max` = tight from `a.last.points` (same formula), and `start_points = &a.last.points`, with a 3 s cap. Write status, ms, `start_values`, the sum of piece lengths before (`a.last.lengths`) and after, and the number of pairs without rows that fail on the polish's points (as LazyLoop counts them).
   - Output to env `PLANAR_STEP1_OUT` (default `slots-step1.md`), appending, and echo to stdout. The test asserts nothing beyond the variant builds (REQUIRE as in the lazy test).

## 4. Do not
- Do not change the lazy test, `[slotsstep0]`, `SlotsOptions` or production code. Do not add stage B, the contraction, the judge or the domain check (later steps).
- Do not run T3, 197 or offers 3. Do not raise caps or rerun.
- Run one heavy job at a time (CPU instability).

## 5. Stop conditions
- The existing lazy test no longer compiles or behaves differently with the defaults (it must not: the defaults keep today's path).
- Round 0 for T2 is not Optimal with 40-60 failing pairs (step 0 measured 51).
- Anything not covered here.

## 6. Gates and run
- Build `build-rel` (Release, target `openttd_test`) with output to a file; grep it for errors and warnings.
- Run the probe alone, T1 then T2 in one run: `PLANAR_STEP1_OUT=docs/junction-tool/wip/probes/slots/step1.md build-rel/openttd_test '[slotsstep1]' > <scratch file> 2>&1`.
- Build `build` (Debug) and run `tools/junction/jt fast build '[planar]'`. A 6.0 s overrun is pre-existing (step 0 measured 6.027 s); report the time, and stop only if it is more than 6.5 s.
- Below the tables in step1.md, add a "Reading" of at most 6 lines, facts only, with no recommendations.
- Commit: `Doc: Junction: stage A lazy loop under D446 on T1 and T2 (slots step 1)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report (short)
References read; commit; for each hub the round table, the summary line and the polish line; the jt planar time; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
