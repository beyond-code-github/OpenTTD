# O4c2: the organic T with rules in the routing, feedback loop deleted (DESIGN-O4 revision 4, D168)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4c2`, branch `wt-o4c2`, head in your launch prompt: the feature branch with O4c1 (router `JunctionMovementRules`) plus the squashed organic WIP from O4b2-O4b2d (`src/junction_organic.{h,cpp}`, its tests, FINDINGS-O4b2b.md): finishing pipeline (crop, row extension, one-tile branch tail, normalise, signal pass), boundary block, D165 main-line turn cost, hug, clean-up pass, forced-crossing cases, and the per-rule feedback loop. You are the only writer there; never touch sibling worktrees, the main worktree or running OpenTTD processes. Never use `git stash`.
- Build: configure once (`cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`), then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`; single cases alone may take minutes, allow them.
- Read first: `CODINGSTYLE.md`; DESIGN-O4.md "Revision 4"; RECONCILE-O4.md; DECISIONS D108, D113, D164-D168; `src/junction_line_router.h` (`JunctionMovementRules`, `resource_cost`); `src/junction_organic.{h,cpp}` and `src/tests/junction_organic.cpp` (whole); `src/junction_signal_pass.cpp` ~260-300 (`GeometryOfPrio`: the prio row lies beside the main line on the side opposite the joiner's approach).

## Why
The per-rule feedback loop couldn't steer (one "not on this tile" penalty per iteration) and made a generation take minutes. Revision 4 puts the rules into the routing: the router refuses routes that break waiting room or D113, and per-round masks from the main lines' current routes keep splits out of prio coverage and put merges where a prio fits. The doubling price schedule is replaced by PathFinder's. The checker then confirms instead of steering.

## Changes
1. **Delete the feedback loop**: the per-rule feedback code, `JunctionFeedbackCounts`, `max_feedback`, reservations and switch penalties driven by violations. `BuildOrganicT` runs: (forced-crossing cases in order, main-line-over first) negotiate -> clean-up pass -> finish -> checker; the first case whose layout is clean (ignoring only "ML bridge is not doubled", D164) is the result; no case clean = unclean with the last case's violations. Keep `JunctionOrganicResult::cases` (per-case lines).
2. **PathFinder prices**: for a piece of movement m that clashes with k pieces of other movements (O4b1's conflict rule, forced-pair prohibitions unchanged), `resource_cost` = (U + h[resource]) x (1 + pf x k) - U, rounded to Money; pf = 0.5 in round 1 and x1.5 each round; after each round, h[resource] += U for every resource in clash. Hug (U/8) and forced-pair prohibitions (1000 x U) are added on top as today.
3. **Order**: each round routes the two main-line through movements first, then the four turning movements, in the expected-list order otherwise.
4. **Movement rules (O4c1)** for the four turning movements only (the main-line through movements get none), rebuilt each round from the other movements' current routes (previous round's for movements not yet routed this round):
   - `shares_from` / `shares_to`: the piece (tile and trackdir) is on the current route of a movement with the same In / Out port.
   - `wait_units` = 2 x `spec.tl` (G7) for all four; `plain_before_head` = 1 (D113).
   - `may_split` (D108) for the two movements splitting off a main line (same In port as a main-line through movement): false on main-line tiles within W = `spec.prio_tiles + spec.sd + 1` tiles upstream (along that main line's current route) of any merge into that main line (the first tile where a turning movement's route shares with it).
   - `may_merge` for the two movements merging into a main line: true only on a main-line piece T such that the main line's current route has W straight axis pieces upstream of T, all inside the box, no split from that main line among them, and the W tiles beside them on the side away from the branch edge (for this T: -y) hold no piece of another movement (tiles outside the box count as free).
   - After each round, for each merge into a main line, the W side tiles beside its window are reserved for the next round: 1000 x U for any piece of any movement except that main line.
5. **Frame**: the crop of the main-line rows never cuts into a merge's prio window: on each main-line row, keep at least 2 tiles beyond the farthest tile of any window (E track upstream is towards x = 0; W track upstream is towards x = W - 1), in addition to today's content margin.
6. **Time**: `JunctionOrganicResult` gains `int64_t micros = 0; ///< Wall time of the generation.` (set by `BuildOrganicT` and `GenerateJunctionOrganic`), and `int rounds = 0; ///< Rounds of the winning case's negotiation.`

## Tests, tag `[junction][organic]`
Keep O4b1's tests and test 7 (forced pairs). Replace the O4b2 tests with:
1. Outer: "GenerateJunctionOrganic: the test spec's T is rule-clean": layout set; `CheckJunctionRules` empty except "ML bridge is not doubled"; INFO p, case, rounds, micros, compactness (occupied, slivers), bridges, ASCII, and the old generator's compactness.
2. The same for `PinnedTestSpec()` and 3. for the narrow-branch spec (TL6, branch_gap 0).
4. "BuildOrganicT: the first negotiation obeys the rules": test spec at p 23: clean, and the winning case's routes satisfy wait_units between each turning movement's split and merge (count from the routes).
5. Test 6 from O4b2c (every bridge crosses something), at the winning p of test 1.
6. `[.bench]` timing: tests 1-3's specs, 1 warm-up + 5 runs: `BENCH organic_gen <spec> clean=<0|1> p=<n> case=<n> rounds=<n> occupied=<n> slivers=<n> median_us=<n>`, and per p for the test spec `BENCH organic_p <p> clean=<0|1> rounds=<n> us=<n>`.
Red evidence: test 4 against the pre-change code (feedback loop) if it can be run; otherwise say so.

## Do not
- Add any feedback rule, penalty driven by checker violations, or tuning of U/8, pf, h or the 1000 x U prohibitions; change the router, checker, signal pass or old generator.
- Add a p search bound, lazy branching, the D80 move or terrain (later steps).

## Stop conditions
- Tests 1-3 unclean at every p: report per p the violations (rule, tile, message), the case and rounds, and the ASCII of the best attempt. Do not add feedback.
- A mask as written can't be computed from the routes (report which).
- Any existing test fails (beyond a lone hardware flake).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[organic]" 8`, then `"[junction]~[slow]"`.
3. Timing alone, three times; BENCH lines.
4. Commit: `Feature: Junction organic T with split and merge rules in the routing`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; names added and removed; per test: p, case, rounds, micros, compactness, bridges, ASCII; BENCH lines; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
