# CS3: curves across an edge boundary are judged by the checker's own rule (rule 3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
CS1 (approach) and CS2 (continuation) carry the router's own curve state (`AdvanceState`/`TurnAllowed`: after a closed S-bend pair, `s_bend_window` clear units before any turn) across edge boundaries. That rule is the router's conservative approximation; the source of truth is the checker's rule 3 (`CheckCurveSpeed`, `src/junction_rules.cpp`: every `2*tl`-unit window of a path gets `CurveSpeedLimitOfTrain`, which exempts exactly one left and one right curve; violation only below `reference_speed`). NW1's investigation (chained lookups over the real path N -> lane B -> W out on the flat test spec) showed the router refusing what rule 3 accepts: N->B's S-jog then 6 units then the shift-back's S-bend is unlimited under rule 3 (1L+1R windows) but refused by the pair rule; with chaining this broke the flat T. Inside one edge the router's rule stays (it never cost anything there); across a boundary, rule 3 decides.

## State
- Worktree `/home/pete/OpenTTD/.claude/cs3`, branch `wt-cs3`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only.
- Tests only through `tools/junction/jt` with bracketed tags (TESTS.md): `'[approach]'`, `'[continuation]'`, `'[curvespeed]'` (new tag) in Debug; `'[linerouter]'`, `'[projection]'`, `'[projectionedges]'`, `'[tmapnear]'` in Release; `'[junctionmap]'`, `'[junctionmapedges]'` in Debug; final gate. OVER BUDGET is a stop.

## Read first (once each)
`CODINGSTYLE.md`; `CurveSpeedLimitOfTrain` and `CheckCurveSpeed` in `src/junction_rules.cpp` (about 544-620) and `junction_rules.h`; in `src/junction_line_router.cpp` the CS1 approach seeding of the root state and the CS2 continuation replay in `IsGoal` (about 395-404), and how a node's path pieces can be recovered at goal time; `JunctionLineRouteRequest` (`approach`, `continuation`) in `src/junction_line_router.h`; `route_line`'s `approach`/`continuation` lambdas in `src/junction_generator.cpp`; the `[approach]` and `[continuation]` tests.

## Tests, red first
1. In `src/tests/junction_rules.cpp` (or the file that tests rule 3), tag `[junction][curvespeed]`: "JunctionMinCurveSpeed matches rule 3 on a path": for two piece sequences (one with a 1L+1R window only: unlimited; one with L, R, L inside one window: limited), the new function's value equals what `CheckCurveSpeed` reports for a layout path made of the same pieces (or, if building such a layout is heavy, equals hand-computed values from `CurveSpeedLimitOfTrain`'s formula: say which). Red: the function does not exist (declare it first returning 0).
2. In `src/tests/junction_line_router.cpp`, `[continuation]`: "a continuation the checker accepts is not refused": a request whose route must end with an S-jog and whose continuation, 6 units after the goal, starts an S-bend of the other hand (the NW1 geometry: jog R-L... then L-R so the window holds one L and one R): routes (rule 3 accepts it). Red: today refused by the pair rule.
3. The existing `[approach]` and `[continuation]` tests keep passing (their geometries are refused by rule 3 too; if one is not, report which and stop).
4. Flat identity, golden trace, every building test unchanged.

## Changes
1. `src/junction_rules.{h,cpp}`: `int JunctionMinCurveSpeed(const std::vector<JunctionPiece> &pieces, uint8_t tl, uint8_t curve_speed); ///< Rule 3 over a piece sequence: the least CurveSpeedLimitOfTrain over every 2*tl-unit window, the ends padded with straight units as CheckCurveSpeed pads a path.` Refactor `CheckCurveSpeed` to build its windows the same way through a shared static helper so both use one computation (no behaviour change; its tests must stay green).
2. `src/junction_line_router.h`: `JunctionLineRouteRequest` gains `std::function<bool(const std::vector<JunctionPiece> &pieces)> boundary_ok; ///< CS3: judges a route joined with its approach and continuation (rule 3); set with approach/continuation. Unset: no boundary check.`
3. `src/junction_line_router.cpp`: remove CS1's seeding of the root curve state from `approach` and CS2's `AdvanceState` replay of `continuation` in `IsGoal`. Instead, in `IsGoal`, when `boundary_ok` is set and the start's approach or the goal's continuation is non-empty: build `approach(start) + the route's track-0 pieces + continuation(goal)` and accept the goal only if `boundary_ok` returns true. Keep `approach` and `continuation` members (they now feed this check). If the node's start (for its approach) is not known at goal time, stop and report what it needs.
4. `src/junction_generator.cpp`, `route_line`: set `boundary_ok` to a lambda returning `JunctionMinCurveSpeed(pieces, spec.tl, spec.curve_speed) >= spec.reference_speed` when `spec.curve_limit` (else always true). Comment citing this brief. No chaining change here (chaining is NW1's).

## Do not
No change to the router's in-edge curve rule, the checker's verdicts, the old router, decisions, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Any existing test, the flat identity or the golden trace changes outcome or result.
- OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds; the jt runs above pass. One commit on `wt-cs3`: `Change: Junction curves across an edge boundary are judged by rule 3`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; functions and members added or removed; jt lines; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
