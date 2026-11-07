# Brief 1b step R1: turn penalty in the router

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The suite has 229 cases, all passing.
- **Why:** every rail piece costs the same, so many routes tie on cost, and the insertion-order tie-break gives wiggly detours. For example, D1 test 3's detour was `LFFFFLFFFLFRFFFRFFFFRFFRFFFFRFFRFFFFFLFFLFFFFL`.
- **Prior art:** the AI rail pathfinder library charges turn = 50 against tile = 100 (`docs/junction-tool/RECON-joiner.md`).

## 2. Read first

- `src/junction_router.h/.cpp`: the request, the successors, the costs
- `src/junction_generator.cpp`: the `base` request in `BuildJunctionCandidate`
- `src/tests/junction_router.cpp`: the obstacle test

## 3. Changes

1. **The `JunctionRouteRequest` field** `Money turn_cost = 0; ///< Added for each L or R move.`
2. **Router:** add `turn_cost` to the cost of every L or R piece. That includes the pieces inside Exception 2: count each L/R move once.
3. **Generator:** `base.turn_cost = spec.prices.rail_piece / 2`.
4. **Heuristic:** unchanged. It stays admissible because turns only add cost.

## 4. Tests

1. **New router test:** the obstacle test's detour case (wall at x=5, bounds (0,0)–(12,20), bridges priced at 1e9), with `turn_cost = rail_piece / 2`.
   - Count the L and R moves in the result: there must be at most 8. That is two 90° turns at each end of the wall, each made of 2 turns, so 8 turns covers the detour round both sides.
   - Red evidence: the same request with `turn_cost = 0` gives the long wiggly route. Report its turn count; it should exceed 8.
2. **Existing tests:** all stay green. Many generator expectations depend on routes. If the generator's outer test, the determinism test, or the clean-candidate tests fail, report which and stop. Don't retune.
3. **Report the `[.gen]` best layout's ASCII** for the test spec after the change (`build/openttd_test "[.gen]"` with `JUNCTION_GEN_SKIP` unset; print only the final `GenerateJunction` block), so the orchestrator can compare shapes.

## 5. Do not

- change any other cost
- change the checker, the signal pass or the game side

## 6. Stop conditions

- An existing generator test fails.
- Test 1's bound on the turn count is wrong for a legal minimal detour. Report the moves.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/r1_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite, all passing.

Commit message: `Change: Junction router charges a turn penalty`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- suite totals
- the generated ASCII
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
