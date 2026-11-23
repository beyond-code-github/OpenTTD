# BRIEF slots-step0 (measurement): why T2's stage A round 1 is slow

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Every solve capped at 15 s. Eight measured solves plus one round-0 solve; each runs once, with no reruns and no loops. This is a measurement: change no production code (nothing under `src/junction/`).

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Plan: `docs/junction-tool/wip/SCOPE-slots-v2-stages.md`, "Steps for the morning", step 0, and "Gemini's review, digested" (Adopted, first two bullets). Ruling: DECISIONS D444, D445.
- The known fact (lazy-3, `wip/probes/slots/lazy.md`): T2 with offers 1 and D443 off, stage A. Round 0 has no keep-apart rows and is Optimal in about 2.4 s; 51 pairs fail on its points. Round 1 gives those 51 pairs rows (204 binaries) and finds nothing in 15 s (status Error). The pairwise big-M is `4 * coord_max` (planar_draw.cpp, near line 1543), and stage A runs with the default `coord_max = 4096`.
- The hypotheses, in order: (1) the big-M is too large; (2) grid parity; (3) the length objective. Every knob you need already exists in `PlanarDrawOptions`: `coord_max`, `tidy`, `start_lengths`, `fixed_pairs`, `grid`, `only_pairs_on` and `only_pairs`.

## 2. Read first (once, with offset/limit)
- `src/tests/junction/planar/planar_domain_encoding.cpp`: `LazyRun`, `LazyBestCandidate` and `LazyLoop` (about lines 5815-5990); the lazy test (about 5992-6100), for how it builds T2's variant and stage A's options `sa`; the lazydiag test (about 6196 to the end), as the pattern for a hidden probe test.
- `src/junction/planar/planar_draw.h`: the option fields listed above, and `PlanarDrawResult` (`status`, `ms`, `binaries`, `points`, `lengths`, `keep_apart`, `start_values`).

## 3. The change: one hidden test
Add `TEST_CASE("planar slots: T2 stage A round 1 cheap checks (step 0)", "[.][slotsstep0]")` after the lazydiag test, inside the same namespace scope as the lazy test (so it can call `LazyBestCandidate`).
1. Build T2's variant with offers 1 and D443 off exactly as the lazy test does: parse, build the structure, shape variant 0, strip the flex, then `PlacePlanarShapeFlex(v, why, true, false, 1)`. Copy those lines; do not factor them out.
2. Stage A's options exactly as the lazy test: `const X1Fixture fa(v); PlanarDrawOptions sa = SlotsOptions(fa, DEFAULT_RULES); sa.node_gaps = false;`.
3. Round 0: `sa` with `keep_apart_out = true`, `only_pairs_on = true` and `only_pairs` empty, 15 s cap. REQUIRE Optimal. Call it D0. The failing pairs are those in D0's `keep_apart` with `skipped` empty, `candidates` not empty, and `!PlanarKeepApartHolds(p, D0.points)` (as LazyLoop does). Record their count (expect 51; if it is not between 40 and 60, stop and report). `biggest` = the largest |coordinate| over D0's points. `tight = std::max(64, (2 * biggest + 15) / 16 * 16)`, the same formula as stage B (planar_draw.cpp:2736).
4. Round 1 runs. Each starts from `sa` with `keep_apart_out = true`, `only_pairs_on = true` and `only_pairs` = the failing tags, and changes only what the row lists:

| run | change from that base |
|---|---|
| R0 | none (the lazy-3 baseline; expect Error at the cap) |
| R1 | `coord_max = tight` |
| R2 | `tidy = false` |
| R3 | R1 + R2 |
| R4 | R3 + `start_lengths = &D0.lengths` |
| R5 | R1, with `only_pairs` empty and `fixed_pairs` = each failing tag with `LazyBestCandidate(p, D0.points)` (no binaries) |
| R6 | R5 + `grid = false` |
| R7 | R3 + `grid = false` |

   For each run, write one table row: run, status, ms, binaries, `start_values` (R4 only), and, if Optimal, the number of pairs without rows that fail on its points (computed the same way as in step 3). Also write `tight`, `biggest` and D0's ms and failing-pair count once above the table.
5. Write the output to the file named by env `PLANAR_STEP0_OUT` (default `slots-step0.md`), appending, and echo each row to stdout as it lands. The test asserts only round 0's REQUIRE; it never fails on a run's status.

## 4. Do not
- Do not touch production code, `SlotsOptions` or the lazy tests. Do not add per-pair big-M (that comes after this measurement, if R1/R3 show it matters).
- Do not raise the 15 s cap, rerun a run, or add runs or configurations (T1, T3, offers 3).
- Do not run another heavy job in parallel with the probe (CPU instability; one solve at a time).

## 5. Stop conditions
- Round 0 is not Optimal, or its failing count is outside 40-60.
- A run returns `unsupported` or a status other than Optimal, Infeasible, Error or a time limit: record it and continue to the next run. Stop only if R0 itself is unsupported.
- Anything not covered here.

## 6. Gates and run
- Build `build-rel` (Release), target `openttd_test`, output to a file; grep it for errors.
- Run the probe alone: `PLANAR_STEP0_OUT=docs/junction-tool/wip/probes/slots/step0.md build-rel/openttd_test '[slotsstep0]' > <scratch file> 2>&1`. This is a hidden-tag probe run, which is allowed here; jt does not run hidden tags.
- Then `tools/junction/jt fast build '[planar]'` in Debug (build `build` first): the new test is hidden, so the planar tag must be unchanged. Report its time; a pre-existing over-budget is known (STATUS), so report it, not stop on it.
- Below the table in step0.md, add a "Reading" of at most 6 lines, facts only: which change moved round 1 from Error to a result, and how fast. Do not recommend.
- Commit the test and step0.md: `Doc: Junction: T2 stage A round 1 cheap checks (slots step 0)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report (short)
References read; commit; tight and biggest; the table (8 rows); jt planar time; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
