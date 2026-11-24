# BRIEF t2-coverage-pin: a unit test (no MIP solve) that says whether T2's stage A can hold its coverage rows

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes. No MIP solves; LP/elastic solves only (milliseconds).

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build Release `build-rel` (`cmake --build build-rel --target openttd_test -j4`; `JT_JOBS=4`). Never touch ng1 or any openttd process. Never use git stash.
- Regression (orchestrator bisect, 2026-11-25): with the probe defaults (20e7774cf7), T2's stage A (`PLANAR_STAGEA_COORD=128`) is Error at 15 s (no incumbent); with only `PLANAR_NO_COVERAGE=1` it is Optimal. Coverage (D384, `PlanarDrawOptions::coverage`, 871d42d25b) is the cause. Oddity: with coverage off the stage A line reports "flank rows written 1", with it on 156: explain it.
- Engineer: "We should have a unit test that picks this up, we shouldn't need to go to the solve."

## 2. Read first
- `planar_draw.h`: `coverage`, `elastic`, `elastic_families`, `elastic_integer`, `build_only`, `program_out`, `flank_rows`; the probe's stage A option block and the `PLANAR_STAGEA_ELASTIC` knob (planar_domain_encoding.cpp) as the pattern; the coverage row in planar_draw.cpp.

## 3. Test (red or green, both are information)
- `[junction][planardraw][coverage]` "T2's stage A can hold its prio rows at exactly their coverage (LP, D384)": build T2's variant as the probe does, stage A options exactly as the probe's defaults (the same function/struct the probe uses; if the probe builds them inline, extract a file-local function both call), with `elastic = true`, `elastic_families = {"coverage"}` (continuous LP): assert total slack == 0. Also assert the same on T1. Report T2's slack per coverage row and the rows' bounds.
- If T2's slack > 0: the coverage rows cannot hold in stage A's model at all (an upstream gap: the row's place in the track map or Shape); mark the T2 case `[!shouldfail]`, and with elastic on ALL families except coverage hard (`elastic_families` = every family the program writes but coverage), report which families stretch: that names the conflict.
- If T2's slack == 0: the LP holds, so the MIP is hard, not infeasible; say so and report the LP time.
- Explain the flank-row count difference (156 vs 1) from the code.

## 4. Commit
- `Add: Junction: pin, T2's stage A holds its coverage rows (LP)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Gates: `JT_JOBS=4 tools/junction/jt fast build-rel '[coverage]' 5`, `... '[planardraw]' 30`.

## 5. Report
References read; commit; T1/T2 slack; the stretched families if any; the flank-row explanation; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
