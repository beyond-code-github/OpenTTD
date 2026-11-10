# O4c1: a rule state machine in the line router (DESIGN-O4 revision 4, D168)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4c1`, branch `wt-o4c1`, head in your launch prompt (the feature branch: line router with O2, O4h, O4p, O4n and the Head level from O4b1). You are the only writer there; never touch sibling worktrees, the main worktree or running OpenTTD processes. Never use `git stash`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`; the benchmark alone: `build-rel/openttd_test "FindJunctionLineRoute: route cost benchmark"`.
- Read first: `CODINGSTYLE.md`; DESIGN-O4.md "Revision 4"; DECISIONS D113, D161, D162, D168; `src/junction_line_router.{h,cpp}` (whole); in `src/junction_rules.cpp` `CheckWaitingRoom` (~803-871, how units are counted from a split) and the D113 clause (grep "split in a bridge's block").

## Why
The organic junction routes each movement with this router. Rules about where a movement may split off its siblings and merge into others were enforced afterwards by a feedback loop that failed (RECONCILE-O4.md). Revision 4 makes them router constraints: the router tracks, per movement, whether it is still on track shared with a sibling (same In port), on its own track, or merged into a sibling (same Out port), and how far it has come since the split, so a route that breaks a rule is never generated.

## Changes
1. `junction_line_router.h`:
   ```cpp
   /** Where a movement may split from and merge into its siblings' current routes (DESIGN-O4 revision 4, D168). Only for single-track requests. */
   struct JunctionMovementRules {
   	std::function<bool(const JunctionPiece &piece)> shares_from; ///< The piece (same tile and trackdir) lies on the current route of a movement with the same In port.
   	std::function<bool(const JunctionPiece &piece)> shares_to; ///< The piece lies on the current route of a movement with the same Out port.
   	std::function<bool(const JunctionPiece &piece)> may_split; ///< The first private piece after shared track may be this one (coverage mask, D108); unset means always.
   	std::function<bool(const JunctionPiece &piece)> may_merge; ///< The first piece on a sibling's route after private track may be this one (prio mask); unset means always.
   	uint wait_units = 0; ///< Private units (JunctionPiece::units) needed between the split and the merge (G7).
   	uint plain_before_head = 0; ///< Signal-legal private pieces needed between the split and a bridge or tunnel head, and between a far head and the merge (D113).
   };
   ```
   Request member: `std::optional<JunctionMovementRules> rules; ///< When set, the route obeys them (DESIGN-O4 revision 4).` Unset changes nothing.
2. `junction_line_router.cpp`: `LineNode` gains, used only when `rules` is set and all part of `Key`:
   - `uint8_t phase = 0; ///< 0 on shared track from the In port, 1 private, 2 merged.`
   - `uint16_t private_units = 0; ///< Units since the split, capped at wait_units.`
   - `uint8_t plain = 0; ///< Signal-legal private pieces since the split or the last far head, capped at plain_before_head.`
   - `bool after_head = false; ///< A bridge or tunnel ended since the split.`
   Transitions, for each piece track 0 adds that is not a bridge or tunnel middle (heads count), in order:
   - Start piece(s): phase 0 if `shares_from(start)` else 1 (private_units 0, plain 0).
   - Phase 0: `shares_from(p)` keeps phase 0. Otherwise the piece is the split: refused unless `may_split` is unset or true for p; phase 1, private_units = p.units (capped), plain = 1 if p is signal-legal else 0.
   - Phase 1: `shares_from(p)` is refused (no rejoining). `shares_to(p)` is the merge: refused unless private_units >= wait_units, and (after_head ? plain >= plain_before_head : true), and `may_merge` unset or true for p; phase 2. Otherwise private_units += p.units (capped), plain += signal-legal (capped).
   - Phase 2: a piece with `!shares_to(p)` is refused (no parting after a merge).
   - A bridge or tunnel move in phase 1 is refused unless plain >= plain_before_head; after it, plain = 0 and after_head = true. Bridge and tunnel moves in phases 0 and 2 are unaffected.
   - Signal-legal piece: not a bridge head, tunnel portal or middle, and (phase 1) private. Say in the report if `IsLegalSignalPiece` in the rules code says more that the router can know.
   - Goal: as today; if `shares_to` is set and the goal piece shares, the goal is reached only in phase 2 (or entering phase 2 on the goal piece under the merge rule).
   - With `rules` set and `line.tracks != 1`, or with a non-empty `prefix`: return the error "movement rules need one track and no prefix".
   - The heuristic is unchanged (constraints only remove routes).

## Tests (red first; record each red), tag `[junction][linerouter]`, single track, flat ground, synthetic sibling routes as lambdas
1. "FindJunctionLineRoute: a movement splits only where allowed": `shares_from` true for the straight row the route starts on (tiles x from start down to start - 12), goal off the row; `may_split` false for the first 6 tiles: the route's first private piece is at least 6 tiles from the start. Red: it splits earlier.
2. "FindJunctionLineRoute: waiting room before the merge": `shares_from` on the start row as in 1, `shares_to` true on a target row the goal lies on, `wait_units` larger than the shortest private stretch: the private units between split and merge are >= wait_units (count them in the test from the route). Red: shorter.
3. "FindJunctionLineRoute: no rejoining and no parting": a case where the cheapest path would leave the shared row and come back to it: the route never returns to a `shares_from` piece after its split, and never leaves `shares_to` after its merge. Red: it does.
4. "FindJunctionLineRoute: a signal-legal piece before a bridge after a split": an obstacle right after the split that forces a bridge, `plain_before_head` = 1: at least one private non-head piece lies between the split and the bridge head. Red: the head follows the split directly.
5. "FindJunctionLineRoute: rules need one track": two-track request with rules: the error text.
6. "FindJunctionLineRoute: no rules is unchanged": the benchmark's single-track cases with `rules` unset give the same cost and expansions as before the change (record before).
7. `[slow]` "FindJunctionLineRoute: rule routes are as cheap as an uninformed search": tests 1, 2 and 4's requests: informed cost <= uninformed cost (D161).

## Do not
- Change the organic search, the generator, the checker or costs; change behaviour when `rules` is unset.

## Stop conditions
- A rule as written can't be applied at the point a piece is added (report which and why).
- Any existing test fails (beyond a lone hardware flake).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[linerouter]~[slow]" 8`, `"[linerouter][slow]"` (6 jobs), `"[junction]~[slow]"`.
3. Benchmark alone once after the change (costs and expansions as before).
4. Commit: `Feature: Junction line router obeys split and merge rules for a movement`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence per test; names added; signal-legal finding; benchmark lines; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
