# BRIEF slots-flank: stage A keeps the turn rules with flank rows (D448 part 1)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. Every solve is capped at 15 s; each probe runs once; no rerun loops.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Ruling: D448 part (1). Read it in `docs/junction-tool/DECISIONS.md`; it is the last row. Read D447 and D430 (the lead rows, whose `C * T - I >= 0` pattern this follows) too.
- Evidence: `docs/junction-tool/wip/probes/slots/step2c.md`. Stage A lets flex inserts and flanks be zero. Every turn break on T1 and T2 is a used insert that, through zero-length pieces, becomes adjacent to a piece more than one eighth from it, or reaches a node next to a leg or trunk more than one eighth from it.

## 2. Read first (once, with offset/limit)
- `CODINGSTYLE.md` (repo root).
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions` around `node_lead` (line 174) and `coord_max`.
- `src/junction/planar/planar_draw.cpp`:
  - the helpers `var`, `row` and `eq` (about lines 459-520);
  - the piece length variables (about 595-640: `free_piece`, `flex`, `capped`, and `length[t][i]` as {variable, scale});
  - `zero_track` (534) and `is_beside` (781);
  - the node ends (815-830);
  - where the lead rows (`PlanarNodeLeadRows`, tag `lead ...`, about 2840-2900) are produced and where they are written into the program.
- `src/tests/junction/planar/planar_domain_encoding.cpp`: the `[slotsstep2]` test (line 6664 onwards) and the `[slotsstep2cdiag]` test after it.

## 3. The rule (production, behind a new option)
`PlanarDrawOptions::flank_turn`, a `bool` defaulting to `false`, declared next to `node_lead`. Its doc comment should say: D448, which rows it writes, and the tag.

When it is on, for each track t that is not a zero track, and for each flex middle m in `v.tracks[t].flex` that is not capped (its upper bound is not 0), do the following for each side (before: flank m - 1; after: flank m + 1):
1. Let h = the insert's heading, `pieces[m]` mod 8. Let `dist(a, b)` = min(d, 8 - d) with d = (a - b) mod 8.
2. **Conflict inside the track.** Walk outward from the flank: before means indices m - 2, m - 3, ... down to 0; after means m + 2, m + 3, ... up to the last piece. Each piece visited counts. Stop after the first piece whose length variable has a lower bound >= 1 (a piece with a least length); that piece counts too. The side has a conflict if any visited piece q has `dist(h, pieces[q] mod 8) > 1`.
3. **Conflict at the node.** If the walk reached the track's end (index 0 before, or the last piece after) without stopping, and that end is at a node, not a port, look at the other lines at that node. Use the same node bookkeeping as the node rows near line 815, and skip any line that `is_beside` or `zero_track` marks. The other lines are the trunk if t is a leg, and the legs if t is the trunk. Take each other line's end piece at the node (its first piece if it starts there, its last if it arrives there). The side has a conflict if any of them has `dist(h, heading) > 1`, OR if t is a leg and a sibling leg's end piece has heading equal to h (F8).
4. **The row.** If the side has a conflict, write `K * len(flank) - len(insert) >= 0` with K = `options.coord_max`, over the two length variables as the program uses them (respect each variable's scale exactly as the lead rows do). Tag: `flank <track name> <m> before|after`. Write it at most once per (m, side). If the flank variable is capped at 0, write no row: stop and report instead, because that is a case the rule does not cover.

Count the rows written in a new `PlanarDrawResult::flank_rows` (`size_t`, default 0, with a one-line doc comment).

## 4. Tests, outermost first (red before green)
Add them to `src/tests/junction/planar/planar_domain_encoding.cpp`, after `[slotsstep2cdiag]`, with tags `[junction][slotsflank]`, not hidden.
1. **"planar slots: T1 stage A with flank rows contracts within the turn rules (D448)":**
   - Build T1 with offers 1 and D443 off, and stage A as `[slotsstep2]` does: `SlotsOptions`, `node_gaps` off, tidy on, no keep-apart rows, and `fa` with its headings, starts and base filled as in `[slotsstep2]`. Set `flank_turn = true`.
   - REQUIRE Optimal and `flank_rows > 0`.
   - Build the geometry, then `ContractedVariant`, exactly as `[slotsstep2]` does.
   - CHECK that every track's pieces are at most one eighth apart, the per-track turn check `[slotsstep2]` writes. CHECK `NodeTurnsHold(X1Fixture(plain), Contracted{plain, cv->points}, &why)` and print `why` on failure.
   - Red first: before the rows are written (the option exists but writes nothing), this must fail on the turn checks. Record the failure text.
2. **"planar slots: T1 stage A without flank rows breaks the turn rules (D448 other side)":** the same build with `flank_turn = false`. CHECK that the per-track turn check fails OR `NodeTurnsHold` fails. This pins the need. It is green from the start, so say so in the report.
3. Then run the T2 measurement. In the `[slotsstep2]` and `[slotsstep2cdiag]` tests, env `PLANAR_FLANK=1` sets `flank_turn = true` on stage A; unset leaves them unchanged. Run, Release:
   - `PLANAR_FLANK=1 PLANAR_STEP2C_OUT=docs/junction-tool/wip/probes/slots/flank.md build-rel/openttd_test '[slotsstep2cdiag]'`
   - `PLANAR_FLANK=1 PLANAR_STEP2B_OUT=docs/junction-tool/wip/probes/slots/flank.md build-rel/openttd_test '[slotsstep2]'`

   Run them one after the other. They report, for T1 and T2: the remaining breaks classified, stage B's listing status, and, if it is feasible, the lazy loop, polish, judge and domain. Stage B here still uses step 2b's zero objective, not the movement objective: that is the next step.

## 5. Do not
- Do not add used binaries, minimal cuts or any other rule. Do not change the lead rows, `ContractedVariant`, `NodeTurnsHold` or `LazyLoop`.
- Do not change any default: `flank_turn` stays false, and no rule set or `SlotsOptions` turns it on.
- Do not add the movement objective (next step). Do not run T3, 197 or offers 3.

## 6. Stop conditions
- A flank that needs a row is capped at 0, or a flex middle has no flank on one side (m is 0 or the last piece).
- T1's red test passes before the rows exist.
- `jt gate` shows a failure other than the known intermittent SIGSEGV/SIGILL under parallel load. Rerun the failing test alone; if it passes alone, report it and go on.
- Anything not covered here.

## 7. Gates
- Debug `build`: `tools/junction/jt fast build '[slotsflank]'`. Record its time and add a row to `docs/junction-tool/TESTS.md` for `[slotsflank]` with the measured Debug and Release times and a 5 s Debug budget. If it is over 5 s Debug, tag both tests `[slow]` as well and say so.
- Release `build-rel`: `tools/junction/jt gate build-rel`. Report the passed/total count and the time.
- After the probes, add a "Reading" of at most 8 lines to flank.md, facts only: per hub, the flank row count, the remaining breaks by class, the listing status, and, if it got that far, the loop, polish, judge and domain verdicts.
- Commit as two commits: the option, rows and tests as `Add: Junction: flank rows keep stage A within the turn rules (D448)`; then flank.md and TESTS.md as `Doc: Junction: flank rows measured on T1 and T2`. Each ends with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report (short)
References read; commits; red evidence (test 1's failure text); the types and members added; jt times and gate counts; per hub, the flank-row count, remaining breaks, listing status and later verdicts; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
