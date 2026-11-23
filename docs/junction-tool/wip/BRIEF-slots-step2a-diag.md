# BRIEF slots-step2a (diagnosis): where T2's contracted writing breaks the node-turn rules (F6/F8)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. Each solve is capped at 15 s and runs once; there are three solves in all. This is a diagnosis: fix nothing, and do not touch production code (nothing under `src/junction/`).

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Ruling D447 (the last row of `docs/junction-tool/DECISIONS.md`): stage A has no keep-apart rows and picks S-bend offers by the length objective. Then a contraction that keeps the turn rules (F6/F8). Then stage B. The contraction rule cannot be designed until we know concretely how T2's contracted writing breaks F6/F8. lazy-3 (`wip/probes/slots/lazy.md`, "lazy-3 T2") found that the node turns FAIL and that stage B's listing solve (no keep-apart rows) is Infeasible, but `NodeTurnsHold` names no node.
- F6: at a node, every leg's heading differs from the trunk's by at most one eighth. F8: no two legs of a node share a heading. A zero-length track is transparent. See the doc comment of `NodeTurnsHold`.

## 2. Read first (once, with offset/limit)
`src/tests/junction/planar/planar_domain_encoding.cpp`:
- `NodeTurnsHold` (about lines 910-1001).
- The lazy test (about 5992 to 6200), especially the stage B contraction (about 6050-6175): how `vc`, `points`, `lengths` and `mapped` are built, and the steps-cleared `plain` variant.
- The `[slotsstep1]` test, for the T2 variant build.

## 3. The changes (test file only)
1. `NodeTurnsHold` gets a last parameter `std::vector<std::string> *why = nullptr`. When `why` is null, behaviour is unchanged. When it is not null, each place that returns false instead pushes one line and goes on to the next node; the function returns false if any line was pushed. Each line gives:
   - the node index;
   - the rule (`F8` for two legs on one heading, `F6` for a leg more than one eighth from the trunk, or `beside` for a heading of -1);
   - the trunk (track name and the heading used, or that it took a zero-trunk recursion);
   - the leg or legs (track name, arriving or starting, and the heading).

   Track names come from `f.v.structure.map.tracks[t].name`. Update the doc comment.
2. A hidden test `TEST_CASE("planar slots: T2 contracted node turns (step 2a)", "[.][slotsstep2diag]")` after `[slotsstep1]`:
   - Build T2's variant with offers 1 and D443 off, copied from `[slotsstep1]`.
   - Stage A: `sa = SlotsOptions(fa, DEFAULT_RULES)`, `node_gaps = false`, tidy at its default (on), `keep_apart_out = true`, `only_pairs_on = true` and `only_pairs` empty. One solve; REQUIRE Optimal.
   - Contract it exactly as the lazy test's stage B does (copy those lines), giving `vc`, `points`, `lengths` and `plain` (steps cleared).
   - Call `NodeTurnsHold(X1Fixture(plain), a_cv, &why)`, with `a_cv` = {plain, points} as in the lazy test.
   - For each failing line, write the line, and then, for each track it names, a block covering the 4 pieces nearest that node (its first 4 if starting, its last 4 if arriving). The block has three parts:
     - in the UNCONTRACTED variant `v`: piece index, heading, stage A's length (`a.lengths`), and whether it is a flex middle (insert) or a flex flank (the piece just before or after a middle);
     - in the contracted `plain`: piece index, heading and length;
     - the track's point at the node in stage A's drawing, and the node point.
   - Then the stage B listing solve exactly as the lazy test does it (`SlotsOptions(fb, DEFAULT_RULES)` with the three references and the tight `coord_max`, `keep_apart_out`, `only_pairs_on`, no pairs). Report its status and ms. If Infeasible, report its `conflict` tags (at most 40).
   - Output to env `PLANAR_STEP2A_OUT` (default `slots-step2a.md`) and echo it to stdout.

## 4. Do not
- Do not change the contraction, the turn rules, production code or other tests' behaviour. Do not try fixes.
- Do not run T1, T3, 197 or offers 3.

## 5. Stop conditions
- Stage A is not Optimal.
- `NodeTurnsHold` passes, so no node fails. Report it: that is a finding, since lazy-3 said FAIL.
- Anything not covered here.

## 6. Gates
- Build `build-rel` (Release) with output to a file; grep it for errors. Run it with `PLANAR_STEP2A_OUT=docs/junction-tool/wip/probes/slots/step2a.md build-rel/openttd_test '[slotsstep2diag]'`.
- Build `build` (Debug) and run `tools/junction/jt fast build '[planar]'`. Its pre-existing time is 6.03 s; stop if it is above 6.5 s.
- Run `tools/junction/jt fast build '[planardomainexact]'` too, because `NodeTurnsHold` is used by the exactness judge. All tests must pass as before; report its time.
- Below the output in step2a.md, add a "Reading" of at most 8 lines, facts only. For each failing node: which piece makes the leg's heading at the node differ from Shape's (for example a zero-length flank so that the insert's heading reaches the node, or a removed insert), and whether that piece is a flex flank, a middle or a fixed piece.
- Commit: `Doc: Junction: where T2's contracted writing breaks the node turns (slots step 2a)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report (short)
References read; commit; each failing line with its blocks (in brief); the listing solve's status and conflict; the jt times; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
