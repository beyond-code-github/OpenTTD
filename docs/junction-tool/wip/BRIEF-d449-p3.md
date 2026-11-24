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

## Run 2: P3b, the LP as a filter (2026-11-25, after P3: the LP's rounded set overlaps CP-SAT's 6/8, 21/25, 34/35 but adds 4/12/19 extras and misses 0-4; T1 draws end to end, T2/T3 locked stage A Infeasible)
- Two variants of the P3 knob (start from wt-c2 HEAD 153b75d601), each one run per hub (T1, T2, T3), threshold 1e-6 only:
  - **P3b-a `PLANAR_D449_LP_MODE=zero`:** inserts whose LP length is below the threshold are held at 0; every other insert is FREE (lower bound 0, as with no lock), not forced on.
  - **P3b-b `PLANAR_D449_LP_MODE=on`:** inserts at or above the threshold are forced on (least length); every other insert is FREE (not held at 0).
  - (P3's mode, both sides fixed, stays the default `PLANAR_D449_LP_MODE=both`.)
- Freeing an insert must use the existing lock machinery (`curve_flex_free` / `flex_free_tracks` / the lock reference) only if it can free single inserts; if it can free only whole tracks, add the smallest per-insert free path in the probe's glue (file-local, test-side) or STOP if it needs planar_draw.cpp: say which.
- Report per hub and mode: stage A status and time (HiGHS 15 s, `JUNCTION_MILP_INCUMBENT=1`), and when it draws: stage B, polish (track, box), judge, domain; inserts used in the final drawing vs CP-SAT's set. Append to p3.md (section "P3b"). Commits: `Add: Junction: D449 P3b, the LP as a filter (PLANAR_D449_LP_MODE)`, `Doc: Junction: D449 P3b numbers`.
