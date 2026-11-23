# BRIEF slots-step2b (measurement): stage B from the suite's own contraction (ContractedVariant) of stage A's drawing, on T1 and T2

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Each solve is capped at 15 s, except the polish (3 s). The lazy loop runs at most 12 rounds per hub. Run each hub once; no reruns. Do not touch production code (nothing under `src/junction/`).

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Rulings: D446 and D447, the last two rows of `docs/junction-tool/DECISIONS.md`. Stage A has no keep-apart rows; stage B carries keep-apart lazily, with a tight bound and free binaries.
- Evidence (`wip/probes/slots/step2a.md`): the lazy test's own contraction drops only unused S-bend inserts. It keeps every other zero-length piece. On T2, D1's last piece is a zero-length flank on h2, so `NodeTurnsHold` reads D1 as arriving at node 10 on h2 (F8 with E1), although D1 really arrives on h1. Stage B with no keep-apart rows is then Infeasible in 24 ms.
- Hypothesis: the zero-length pieces kept by that contraction get a least length of 1 in stage B, which forces extra length everywhere. The suite's certified contraction `ContractedVariant` (D432: every zero piece dropped, equal headings merged, flank-first within one eighth per turn) is the one the judge reads. Stage B should be built from it.

## 2. Read first (once, with offset/limit)
`src/tests/junction/planar/planar_domain_encoding.cpp`:
- `Geometry` (line 102), `X1Fixture` (105 onwards: `heads`, `idx`, `n`, `StartsOf`), `Contracted`, `NodeTurnsHold` (with its new `why`) and `ContractedVariant` (about 1008-1100).
- The lazy test's stage B (about 6140-6230): `plain`, the references, the judge and domain verdict lines.
- `LazyLoop` (with `fix_sides`, `tight_bound`, `near_margin`), and the `[slotsstep1]` and `[slotsstep2diag]` tests.

## 3. The change: one hidden test
`TEST_CASE("planar slots: stage B from ContractedVariant of stage A on T1 and T2 (step 2b)", "[.][slotsstep2]")` after `[slotsstep2diag]`. For each hub of T1 and T2 (env `PLANAR_SLOTS_HUB` limits it to one):
1. Build the variant with offers 1 and D443 off, copied from `[slotsstep1]`. Set `const X1Fixture fa(v);`.
2. Stage A: `SlotsOptions(fa, DEFAULT_RULES)`, `node_gaps = false`, tidy on (the default), no keep-apart rows (`keep_apart_out = true`, `only_pairs_on = true`, `only_pairs` empty). One solve; REQUIRE Optimal.
3. Geometry: `Geometry g;` for each track t, `g[name][k] = (int)std::lround(a.lengths[t][k])` for every piece k, where name is the name `fa.idx` maps to t. First REQUIRE that every `a.lengths[t][k]` is within 1e-6 of an integer.
4. `const std::optional<Contracted> cv = ContractedVariant(fa, g);`. If it has no value, write that and skip to the next hub. That is a finding (other shape), not a stop.
5. Check that the contraction does not move the drawing: for every track, every point of `cv->points[t]`, translated by the single offset that maps `cv->points[0][0]` onto `a.points[0][0]`, must appear in `a.points[t]`. If not, stop and report the first mismatch (track, index, both points). This is a stop because it means the geometry units are wrong.
6. Report:
   - the per-track turn check, as the lazy test writes it;
   - `NodeTurnsHold(X1Fixture(plain), Contracted{plain, cv->points}, &why)`, with `plain` = `cv->v` with steps cleared (as the lazy test does it), and every `why` line;
   - the global counters `g_turn_moved` and `g_turn_rejected`, before and after the call.
7. Stage B on `plain`:
   - `fb = X1Fixture(plain)`, `sb = SlotsOptions(fb, DEFAULT_RULES)`;
   - references from `cv->points`: `port_zero_from = node_gap_from = PortZeroFrom(cv->points)`, `along_from = AlongFrom(Contracted{plain, cv->points})`;
   - `coord_max` tight from `cv->points`, with the same formula as step 1;
   - `sb.tidy = false`.

   Then:
   - Run the listing solve (no rows), as the lazy test does. If Infeasible, call `LazyConflict(plain, listing, tag, "B (no keep-apart rows)", md)` and go to the next hub.
   - Otherwise run `LazyLoop(plain, sb, {}, tag, "B", md, false, true)`.
   - If it converged, run the polish exactly as `[slotsstep1]` does (tidy on, 3 s, `start_points`, `only_pairs` = the loop's list).
   - On the polish's drawing (or the loop's last drawing if the polish is not Optimal), give the judge and domain verdicts exactly as the lazy test writes them (`DrawingProgramJudge(DEFAULT_RULES)` with and without interface rows; `CheckPlanarDomain(PlanarDomainInputOf(...))` with its break rules).
8. Output to env `PLANAR_STEP2B_OUT` (default `slots-step2b.md`), appending, and echo it to stdout. Tag each hub `STEP2B <hub>`.

## 4. Do not
- Do not change `ContractedVariant`, `NodeTurnsHold`, `LazyLoop`, other tests or production code.
- Do not add a node-turn keeping rule, re-add S-bend offers, or try any other fix. This step only measures.
- Do not run T3, 197 or offers 3, and do not raise caps.

## 5. Stop conditions
- Step 3's REQUIRE (non-integer lengths) fails, or step 5's point check fails.
- Stage A is not Optimal.
- Anything not covered here.

## 6. Gates
- Build `build-rel` (Release) with output to a file; grep it for errors. Run `PLANAR_STEP2B_OUT=docs/junction-tool/wip/probes/slots/step2b.md build-rel/openttd_test '[slotsstep2]'` once.
- Build `build` (Debug) and run `tools/junction/jt fast build '[planar]'`. Its pre-existing time is 6.03 s; stop if it is above 6.5 s.
- Below the output in step2b.md, add a "Reading" of at most 8 lines, facts only. Per hub, give: node turns, listing status, loop converged/rounds/pairs/ms, polish, and the judge and domain verdicts.
- Commit: `Doc: Junction: stage B from ContractedVariant on T1 and T2 (slots step 2b)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report (short)
References read; commit; per hub: node-turn lines, listing status (and its conflict families if Infeasible), the round table, polish, judge and domain; the jt time; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
