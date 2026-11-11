# BL1: the bridge length limit comes from the game (D208(1))

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-bl1 <HEAD>` (given at launch). You are the only writer there. `tools/junction/jt` builds openttd_test before running.
- D208 (DECISIONS.md, last rows): the bridge length limit is derived from the game. MakeJunctionSpec (src/junction_game.cpp ~:600-640) already asks ChooseJunctionBridgeType per middle length (date, rail speed; D94 falls back to the fastest bridge), but only for n = 1..10: that 10 is an assumption nobody ruled. This step makes the precomputation run to the game's limit and records that limit in the spec, for the new seed placement (DESIGN-G1 §11.8) to read. The older generator paths keep their own 10 (they are vestiges of the flat T; changing them would move the traced T).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (rows for the tags you run); DECISIONS.md D94 and D208 (grep).
- src/junction_generator.h :39-55 (JunctionSpec); src/junction_game.cpp :110-135 (ChooseJunctionBridgeType) and :595-645 (MakeJunctionSpec); src/tests/junction_game.cpp :88-125 (the precompute test).
- src/tunnelbridge_cmd.cpp :236-253 (CheckBridgeAvailability: a type's max_length and the max_bridge_length setting both bound the middle length).

## Design (settled)
1. JunctionSpec gains, after `bridge_span_ok`:
   ```cpp
   uint16_t max_bridge_middle = 10; ///< The longest middle the game allows (D208): the max_bridge_length setting; bridge_span_ok and prices.bridge are defined up to it.
   ```
2. MakeJunctionSpec: `spec.max_bridge_middle = _settings_game.construction.max_bridge_length` (clamped to at least 1); the two precomputed arrays become `std::vector` of size max_bridge_middle + 1, filled for n = 1..max_bridge_middle exactly as today; the closures test `n >= 1 && n <= max` with max captured by value. ChooseJunctionBridgeType already rejects types whose max_length is below n (check this in the code; if it does not, STOP and report).
3. MakeTestJunctionSpec (junction_generator.cpp:~120) is unchanged (its closure says 1..10, and the default member is 10): the traced T and every existing test stay as they are.
4. Nothing else changes: no other `10` in junction_generator.cpp, junction_organic.cpp or the router.

## Tests (record each red)
1. Rename the existing test to "MakeJunctionSpec: bridge closures are precomputed up to the game's max bridge length" and extend it: with max_bridge_length 50 and bridge 0 max_length 50, `spec.max_bridge_middle == 50`, the loop runs n = 1..50 with the same CHECKs, and the outside values become {0, 51, 60}. Red: does not compile (no member), then (member added, precompute still 1..10) fails at n = 11.
2. New test "MakeJunctionSpec: a bridge type's own max length bounds the span": max_bridge_length 50, bridge 0 max_length 12 and every other bridge type unavailable (avail_year in the far future; restore all after): bridge_span_ok(12) true, bridge_span_ok(13) false. Red: say whether it passes already once test 1's code exists (it should, via ChooseJunctionBridgeType); if it does, record that honestly instead of a red.
3. Every suite in the gate stays green; [nodeoracle] stays accepted 90, heights 0, fits 0 (report the ORACLE line).

## Do not
- Do not change MakeTestJunctionSpec, the older generator's nmax loops, junction_organic.cpp, the router or the line painter. Do not change D94's fallback.

## Stop conditions
- ChooseJunctionBridgeType ignores a type's max_length.
- Any existing expectation changes.

## Gates
`tools/junction/jt fast build "[junction]" ` is too broad: run the tag(s) of src/tests/junction_game.cpp as TESTS.md lists them (fast build and build-rel), then `tools/junction/jt gate build-rel`. Update TESTS.md times for those tags. Commit: `Change: Junction bridge limit follows the game's max bridge length (D208)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; members added; suites with times; ORACLE line; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
