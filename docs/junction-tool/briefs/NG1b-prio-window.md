# NG1b: the negotiated merge keeps its prio window (D181a)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
NG1 (`briefs/NG1-negotiate-a-for-n-to-a.md`, its change is UNCOMMITTED in this worktree: keep it and build on it) made the house case build, but the layout fails rule 9 "missing prio": the probe let N->A merge right after A's detour (goal window 52..54), so A has no straight row-0 run for the prio's coverage (spec.prio_tiles = 14 tiles upstream of the merge) and the signal pass places none. The house sits between the A->S split (x 36) and any merge in that window, so the merge must move east until its whole prio window is straight row 0. The test "an unbuildable hub tile gives no layout" (junction_map.cpp ~1000) is the same case one tile east (D180: the hub tile is a tile of A like any other): it must build clean too.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-ng1`, head cfa2989ce3 plus NG1's uncommitted change to `src/junction_generator.cpp` and `src/tests/junction_map.cpp`. `build` (Debug) and `build-rel` (Release) exist. Tests only through `tools/junction/jt` with bracketed tags (`docs/junction-tool/TESTS.md`): `'[projectionedges]'`, `'[projection]'`, `'[tmapnear]'` in Release; `'[junctionmap]'`, `'[junctionmapedges]'` in Debug; `jt gate build-rel`. Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/ng1b/`.

## Read first (once each, offset/limit)
D148, D179a, D180, D181, D181a in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md`; `git diff` in the worktree (NG1's block after `say("N->A failed")`); `JUNCTION_E_PRIO_AFTER_SPLIT` and its comment (~line 244) and `n_to_a_min_x` (~1408) in `src/junction_generator.cpp`; `approach_usable` / `ports_blocked` (~1130) and the ports-widened redo (~1700); the two tests named below.

## Tests, red first
1. "an obstacle on the main line beside the hub: A is routed again clear of N->A (D181)" (already written by NG1): fix its violation reporting (an `INFO` inside a `for` body is dropped at the end of each iteration: build one `std::string` of all violation messages first, then `INFO` it once). Red today: `CHECK(result.clean)` fails with "rule 9 ... missing prio".
2. Replace "an unbuildable hub tile gives no layout" with "an unbuildable hub tile: A is routed round it (D180, D181)", same tags, same setup (keep `LimitMapTestRows`). Asserts: `REQUIRE(result.layout.has_value())`, `result.clean` (violations INFO'd as in 1), and the log contains `"N->A negotiates: A routed again clear of N->A"`. Update its comment. Red today: not clean (missing prio).
3. Unchanged: every other test, the flat identity, the golden traces.

## Changes (`src/junction_generator.cpp`, NG1's negotiation block only, plus the reservation)
1. Probe goals: x in `[n_to_a_min_x, frame.width - 3]` (no longer capped at x_e) whose prio window, row-0 tiles x in `[goal - 1 - spec.prio_tiles, goal]`, is all usable: for each tile, `terrain == nullptr || approach_usable(tx)` (the existing lambda; it reads row 0). Every other row-0 tile in `[0, frame.width - 1]` is in `probe_blocked`, as NG1 does now.
2. After the probe routes: its goal x is the x of its last piece on row 0 (state this in a comment; if the probe raster has no row-0 piece, stop and report). `a_reserved` = the probe's tiles with y > 0 (as now) PLUS `tile(tx, y)` for tx in `[goal - 1 - spec.prio_tiles, goal]`, y in `[1, g]`.
3. If goal > x_e: `x_e = goal; say(fmt::format("ports widened to {}..{}", x_w, x_e)); if (ports_blocked()) return result;` before the redo (same order as the D179a path).
4. Comment: quote D181a.

## Do not
No change to the signal pass, the checker, rule 9, the router, A->S, B's lanes, growth_cannot_help, the old router; no second negotiation round; no change to FinishJunctionCandidate's terrain argument. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Either test still unclean (report the violations and the log lines of the redone height); A fails with the reservation; the probe finds no goal; any other test, flat identity or golden trace changes; OVER BUDGET; anything not covered.

## Gates
0 warnings; all jt runs above pass; report `[projectionedges]` Release time and each of the two tests' times. One commit on `wt-ng1` with NG1 and NG1b together: `Change: Junction A is routed again clear of N->A and its prio window when N->A fails (D181)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (both tests); locals added; both tests' logs (the redone height) ; jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
