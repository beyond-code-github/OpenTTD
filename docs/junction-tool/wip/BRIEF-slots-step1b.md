# BRIEF slots-step1b (measurement): does T2's stage A loop converge with a tidy objective or rows for near pairs?

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. Every solve is capped at 15 s; the loop runs at most 12 rounds per mode. Run each mode once, T2 only, with no reruns. Change no production code (nothing under `src/junction/`).

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Evidence: `docs/junction-tool/wip/probes/slots/step1.md`. Under D446 (`tidy = false`, tight bound, free binaries) T1 converges in 3 rounds, but T2 does not. From round 2 on, about 18 new pairs fail each round, binaries grow from 316 to 820, and round 7 hits the cap.
- The hypothesis: with no objective, each round's drawing moves freely and breaks new pairs. Two levers: (a) the length objective (`tidy = true`), which keeps drawings compact and close to one another; (b) adding rows for NEAR pairs too, meaning pairs without rows whose best candidate's margin on the round's points is below a threshold, not only the failing ones.

## 2. Read first (once, with offset/limit)
`src/tests/junction/planar/planar_domain_encoding.cpp`: `LazyBestCandidate` (about line 5895), `LazyLoop` (about 5916 onwards), and the `[slotsstep1]` test (about 6414 to the end of the file).

## 3. The changes (test file only)
1. A new function right after `LazyBestCandidate`: `int LazyBestMargin(const PlanarKeepApartPair &p, const std::vector<std::vector<std::array<int, 2>>> &points)`. It returns the best candidate's margin, using the same computation as `LazyBestCandidate` (the largest over candidates of the least over its rows of `s . (b - a) - spacing`). Make `LazyBestCandidate` call it or share the loop only if you can do so without changing its result; otherwise duplicate the loop. A one-line doc comment.
2. `LazyLoop` gets one more defaulted parameter at the end: `int near_margin = 0`. A pair without rows on an Optimal round is added (it counts as "failing" in the table) when `!PlanarKeepApartHolds(p, r.points)`, OR when `near_margin > 0 && LazyBestMargin(p, r.points) < near_margin`. With the default 0 the behaviour is unchanged. Update the doc comment.
3. In the `[slotsstep1]` test, env `PLANAR_STEP1_MODE` picks the mode. If it is unset, nothing changes. `tidy` sets `sa.tidy = true` for the loop. `near` passes `near_margin = 4` (half tiles, i.e. two tiles). `tidynear` does both. The tag becomes `STEP1 <hub> <mode>`. The polish runs unchanged after a converged loop.

## 4. Runs
With `PLANAR_SLOTS_HUB=T2` and `PLANAR_STEP1_OUT=docs/junction-tool/wip/probes/slots/step1.md` (appending), run `build-rel/openttd_test '[slotsstep1]'` once per mode: `tidy`, `near`, `tidynear`. Run them one after another, never in parallel.

## 5. Do not
- Do not change production code, the lazy test or `[slotsstep0]`. Do not try other margins, other hubs or offers. Do not raise caps or rerun.
- Do not add a movement objective or any other lever.

## 6. Stop conditions
- With `PLANAR_STEP1_MODE` unset, the test's path would differ from today's.
- A mode's round 0 is not Optimal. Record it and go on to the next mode; stop only if all three fail.
- Anything not covered here.

## 7. Gates
- Build `build-rel` (Release) with output to a file; grep it for errors and new warnings.
- Build `build` (Debug) and run `tools/junction/jt fast build '[planar]'`. Its pre-existing time is 6.03 s; stop only if it is above 6.5 s.
- Below the new tables in step1.md, add a "Reading (step 1b)" of at most 6 lines, facts only. Per mode, give: converged or not, rounds, the last pairs-with-rows count, total ms, and the polish line if one ran.
- Commit: `Doc: Junction: stage A loop on T2 with tidy and near pairs (slots step 1b)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report (short)
References read; commit; for each mode its round table, summary line and polish line; the jt planar time; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
