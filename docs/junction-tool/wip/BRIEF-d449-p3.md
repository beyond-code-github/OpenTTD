# BRIEF d449-p3: LP rounding chooses the S-bends, HiGHS sizes them (D449 proof, step 3; a probe)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes. Per hub (T1, T2, T3): one LP solve, at most two locked HiGHS stage A runs (15 s each, thresholds below) and the pipeline after them. No CP-SAT runs (P2's optima stand). No retry loops.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build Release `build-rel` (`cmake --build build-rel --target openttd_test -j4`; `JT_JOBS=4`). Never touch ng1 or any openttd process. Never use git stash.
- Evidence: `docs/junction-tool/wip/probes/d449/p1.md`, `p2.md` (P2, P2b), and the control in `docs/junction-tool/wip/HANDOVER-2026-11-26.md` item 0: locking CP-SAT's own set through `PLANAR_STAGEA_LOCK_FILE` (piece lengths, CP-SAT `--lengths=` format) sizes T1's stage A in 105 ms. The node rules and v1's slot model do not choose a feasible set.

## 2. The probe
Behind env `PLANAR_D449_LP=<threshold>` in `[slotsstep2]` (stage A, the probe's defaults):
1. Build stage A's program as usual but solve it as the LP relaxation: `continuous` = every group, `grid = false` (grid rows need integers), every other option as the defaults; no objective change (tidy as usual). Report status and time.
2. Write a lock file in exactly `PLANAR_STAGEA_LOCK_FILE`'s format: per track and piece, the LP length; then run the probe's normal locked path with it (`curve_flex_from` + `curve_flex_above` = the threshold from the env): inserts with LP length >= threshold stay positive (least length), the rest are held at 0. Reuse the lock-file code path; no new lock mechanism.
3. Thresholds: run twice per hub, `PLANAR_D449_LP=0.5` and `PLANAR_D449_LP=0.000001`.
4. Report per hub and threshold: LP status/time; inserts with LP length above the threshold (count) and their overlap with CP-SAT's min-length set (P2's sets in p2.md or its scratchpad output: same / LP-only / CP-SAT-only); locked stage A status and time; then stage B, polish (track length, box), judge, domain; if stage A is Infeasible, the conflict families (probe listing) and how many locked inserts are in it.
5. Append to `docs/junction-tool/wip/probes/d449/p3.md` (new file, at most 60 lines), with a comparison row against P2's CP-SAT optimum where stage A succeeds (track ratio, box ratio).

## 3. Do not
- No change to the drawing program's rules or defaults, the lock mechanism or Shape. Only the knob and its LP + lock-file glue (file-local in planar_domain_encoding.cpp). If `continuous` cannot express "every variable continuous" without new code in planar_draw.cpp, STOP and report what is missing.

## 4. Commits
- `Add: Junction: D449 P3 probe, LP rounding locks the S-bends (PLANAR_D449_LP)`, `Doc: Junction: D449 P3 numbers`. End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Gate: `JT_JOBS=4 tools/junction/jt fast build-rel '[contraction]' 5`.

## 5. Report
References read; commits; the per-hub, per-threshold table; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
