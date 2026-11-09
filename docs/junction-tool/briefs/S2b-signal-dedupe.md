# S2b: one signal may do several jobs (D117 a, b)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2b`, branch `wt-s2b`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1` (ccache is shared, so it is fast after the first build). Debug only if the gate asks.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8` (8 jobs: other agents share the machine). Iterate with your own tag; full `"~[.]"` only at the gate.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (read the rows named below; it is newer in the main worktree than in yours).
- Plan: `docs/junction-tool/PLAN-S2.md` §4 S2b. Decisions D117 (the ruling; parts a and b only; c is S2c), D82, D113, D24, D97.

## Why (settled)
The checker (`CheckSignals` part 3, `src/junction_rules.cpp` ~709-770) and the signal pass (`PlaceRequiredSignals`, `src/junction_signal_pass.cpp` 69-101) both demand that the *nearest* signal-legal piece of each stretch carries a signal. Two rules whose stretches overlap from opposite ends (after a bridge's far head, before the next merge) then require two adjacent signals. D117: (a) a rule is satisfied by any signal in its stretch; (b) the pass removes a signal adjacent to another on the same path when every rule still holds without it.

A stretch = the pieces scanned from `from` in `step` direction up to, not including, the first `on_bridge` piece or switch piece (the scan `nearest` already does).

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- `src/junction_rules.h` (all), `src/junction_rules.cpp` 681-790 (`CheckSignals`) and 1640-1680 (`CheckJunctionRules`, `JunctionRulesClean`).
- `src/junction_signal_pass.cpp` 1-110 and 585-597.
- `src/tests/junction_generator.cpp` 1-70 and 225-300 (spec helpers, how a generated layout's paths are traced in tests); `src/tests/junction_rules.cpp` tests touching "no signal before"/"after a bridge" (grep).

## Tests (red first)
1. Outer, `[junction][generator]` "GenerateJunction: no adjacent signals on the main lines (D117)": generate `MakeTestJunctionSpec()`; trace paths (`MakeJunctionCheckContext` with `FindJunctionPorts` and `JunctionTMovements()`, as the signal pass does); on every mainline path (E in→E out, W in→W out) no two consecutive pieces are both signalled (`IsSignalledPiece`). Record the offending pairs seen red. If the test spec shows none, use the spec of the existing test that sets the D138 train (`14194, 1090000, 561, 25, 196`) with TL 5 and `prio_tiles = 15` instead, and say so; if that shows none either, stop.
2. Inner, `[junction][rules]` "CheckSignals: any signal in the stretch satisfies the rule (D117 a)": take a generated layout that is clean; find a mainline bridge whose far head is followed by two or more signal-legal pieces before the next switch; move the signal after the far head one legal piece further downstream (still before the switch); CHECK no `"no signal after a bridge"` violation. Red today: the nearest piece lacks the signal.
3. Same tag, "CheckSignals: an empty stretch is still a violation": remove that signal entirely; CHECK `"no signal after a bridge"` is reported (passes before and after; guards the change).

## Changes
1. `junction_rules.{h,cpp}`: add
   ```cpp
   /** Whether the stretch scanned from 'from' in 'step' direction (up to a bridge piece or a switch) has a signal (D117 a). */
   bool JunctionStretchHasSignal(const JunctionLayout &layout, const JunctionPath &path, const std::set<size_t> &switches, size_t from, int step);
   ```
   In `CheckSignals` part 3, every `require(nearest(...))` and both bridge clauses report a violation only when `nearest(...)` is non-null (the stretch has a legal piece) and `!JunctionStretchHasSignal(...)` for the same scan. The D113 `blocked_by_switch` clauses stay as they are. Update the part-3 comment citing D117.
2. `PlaceRequiredSignals`: before `place(NearestSignalPiece(...))` for each requirement, skip when `JunctionStretchHasSignal(...)` for the same scan. Order of requirements unchanged.
3. New phase at the end of `PlaceJunctionSignals` (after `PlacePrios`): `static void RemoveRedundantSignals(JunctionLayout &layout, const JunctionSpec &spec)` (D117 b). Trace paths; walk paths in `PathOrder`, pieces from upstream to downstream; for each pair of consecutive signalled pieces (i, i+1) on a path, try removing the signal on piece i, then (if that was kept) the one on piece i+1. A removal is kept when every violation of `CheckJunctionRules` after the removal was also present before it (compare rule, tile and message), and no new one appears. Re-trace after each kept removal.
4. Tests in other files that pin signal counts or exact signal positions and change because of 1-3: update the expected values only and list each change (test, old, new). Do not delete or weaken assertions.

## Do not
- Change D117 c (the gap-0 entry), the lane programs, the router, the fill spacing rule, prio placement, or any rule other than part 3 of `CheckSignals`.
- Change the signal pass phases' order.

## Stop conditions
- A test fails for a reason other than a pinned signal count or position.
- Phase 3 makes the full suite more than 20% slower in wall time (report both times).
- Tests 1-2 can't be made red on either spec.

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction]~[slow]" 8`, then `"~[.]"`; report wall times of both and, for comparison, the `"~[.]"` wall time on your worktree's head before the change (run it once at the start).
3. Commit: `Change: Junction signals: one signal may do several jobs`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test (the failure message seen before the code existed)
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
