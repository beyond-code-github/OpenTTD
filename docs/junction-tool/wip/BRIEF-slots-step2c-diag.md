# BRIEF slots-step2c (diagnosis): classify every turn break in stage A's drawing on T1 and T2

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 40 minutes wall. Two stage A solves, one per hub, nothing else. A diagnosis: fix nothing, and touch no production code.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Evidence (`wip/probes/slots/step2b.md`): stage A (D447: no keep-apart rows, tidy on, offers 1, D443 off) gives drawings whose contraction breaks the turn rules. Per track (F3: consecutive non-zero pieces must be at most one eighth apart), T1 fails on tracks 4, 5 and 11, and T2 on 13, 18, 19 and 22. At nodes, T1 breaks F6 at node 1 and F8 at nodes 3, 4 and 5; T2 breaks F6 at nodes 0, 1, 13 and 14 and F8 at nodes 7, 9 and 11. Stage A lets flex inserts AND flanks be zero, so a zero piece can sit between two pieces more than one eighth apart.
- Before stage A gets turn rows, every break must be classed by its configuration in the UNCONTRACTED variant `v`.

## 2. Read first (once, with offset/limit)
In `src/tests/junction/planar/planar_domain_encoding.cpp`, read the `[slotsstep2]` test (from line 6664 to the end): how it builds `v`, `fa`, `g` and `cv`. Also read `NodeTurnsHold` with its `why`. In `src/junction/planar/planar_draw.h`, read the `PlanarShapeTrack` fields `pieces`, `flex` (flex middles) and `event_piece`.

## 3. The change: one hidden test
`TEST_CASE("planar slots: stage A turn breaks classified (step 2c)", "[.][slotsstep2cdiag]")` after `[slotsstep2]`. Build T1 and T2 exactly as `[slotsstep2]` does, up to and including the stage A solve. Copy those lines. Then, per track of `v`:

1. **Per-track breaks.** Walk the pieces of `v` with stage A's lengths. Wherever two consecutive pieces of NON-ZERO length, i and j > i with every piece between them of length 0, have headings more than one eighth apart (`min(d, 8 - d) > 1` on headings mod 8), write one line with:
   - the track name;
   - i and j, with their headings and lengths;
   - each zero piece between them: its index, its heading, and its role, which is `middle` if it is in `flex`, `flank` if it is a piece just before or after a middle, else `fixed`;
   - whether a crossing sits on any of those pieces (`event_piece`);
   - the class: `A` if the zero run holds exactly one flank and that flank lies between two middles (two inserts with a zero flank between them); `B` if the zero run includes a fixed (Shape) piece; `C` for anything else (describe it in a few words).
2. **Node breaks.** Call `NodeTurnsHold` with `why`, but on a version of the fixture whose zero-length END pieces are read through. To do that, build `Contracted` from `v` with each track's leading and trailing zero-length pieces removed (pieces and points both). Keep at least one piece per track. A zero-length track keeps its first piece. Do not change `NodeTurnsHold`. For every `why` line, add per named leg:
   - its first or last NON-ZERO piece of `v` (index, heading, role);
   - the zero pieces between that piece and the node (index, heading, role);
   - Shape's heading of the leg's end piece (`v`'s first or last piece);
   - the class: `N1` if the leg's non-zero end piece is a flex middle, meaning an insert reaches the node because its node-side flank is 0; `N2` if it is a fixed piece; `N3` for anything else (describe).
3. A count per hub per class. Output goes to env `PLANAR_STEP2C_OUT` (default `slots-step2c.md`), and is also echoed.

## 4. Do not
- Do not change any existing function or test, and do not touch production code.
- Do not try fixes or extra solves. Do not run other hubs or offers.

## 5. Stop conditions
- Stage A is not Optimal on either hub.
- You find no per-track break on a hub that step 2b says fails. Report it; that means the walk differs from `ContractedVariant`'s.
- Anything not covered here.

## 6. Gates
- Build `build-rel` (Release) with output to a file; grep it for errors. Run `PLANAR_STEP2C_OUT=docs/junction-tool/wip/probes/slots/step2c.md build-rel/openttd_test '[slotsstep2cdiag]'` once.
- Build `build` (Debug) and run `tools/junction/jt fast build '[planar]'` (pre-existing 6.03 s; stop above 6.5 s).
- Add a "Reading" of at most 6 lines to step2c.md: the class counts per hub, facts only.
- Commit: `Doc: Junction: stage A's turn breaks classified (slots step 2c)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report (short)
References read; commit; every break line (compact) with its class; the counts; the jt time; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
