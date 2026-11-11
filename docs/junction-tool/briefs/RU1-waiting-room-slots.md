# RU1: waiting room and signal slots as template rules (D193; AUDIT-rule-coverage items 3-4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
Worktree `/home/pete/OpenTTD/.claude/ru1`, branch `wt-ru1`, head given at launch. Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests only through `tools/junction/jt`: `'[nodes]'` (budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/ru1/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DESIGN-N3.md` §2, §5-§7; `docs/junction-tool/AUDIT-rule-coverage.md` rows WaitingRoom, Signals, Deadlock; `src/junction_rules.cpp` 843-937 (CheckWaitingRoom: the two unit rules you mirror); `src/junction_rules_internal.h` / `src/junction_signal_pass.cpp` for IsLegalSignalPiece (grep); `src/junction_nodes.cpp` AddPathDiffs (add_gap, late gaps) and ReadPoints (link_of, governing); `src/tests/junction_nodes.cpp` (helpers; N0c1/N0c2 tests).

## The rules (mirror the checker exactly; the flat layout is legal, so its slots exist)
A **slot** of a path = a piece for which IsLegalSignalPiece(flat layout, piece) is true (read it: axis, not a switch, not on a bridge...). For a stretch of a path from piece a to piece b (a < b), its **last slot** = the greatest i, a < i < b, that is a slot in the flat.
1. **Slot keeping.** For each of these stretches on every path: (switch or bridge head) -> next (switch or bridge head) along the path; if its last slot lies strictly inside a free X or Y run (link_of >= 0, not a diagonal), that run must keep at least one interior piece: d * (end - start) >= 2 along its axis (static for Y runs, static for X runs: both are plain diffs over their own axis). No slot in the flat stretch: nothing (the checker accepts it).
2. **Waiting room (1)**, paths EIn->SOut and WIn->SOut: a = the first split, m = the first merge after it, L = the last slot of (a, m). The checker needs units(a+1 .. L) >= 2 TL. As a unit rule between the points governing a and m: units(m) - units(a) >= 2 TL + units_flat(a) + (U_m - U_Lend), where U_m = flat start unit of m, U_Lend = flat start + units of L. Register it through add_gap(point(a), point(m), g) with g = that right-hand side (a and m are switch pieces, hence points). L's run keeps an interior piece by rule 1, so the constant holds.
3. **Bay (2)**, every merge m on a non-main-line path: s = the last slot before m (the checker's merge signal), u = the last split before s on the path (none: skip); units(u+1 .. s) >= 2 TL: add_gap(point(u), point(m), 2 TL + units_flat(u) + (U_m - U_send)), U_send = flat start + units of s.
Write each rule with a one-line comment naming the checker line it mirrors.

## Tests, red first (`[junction][nodes]`). Red: the rules not added; record failures.
1. "a short waiting room is lengthened" (outer-ish, FlatT at the test spec): positions; AddJunctionNodeConstraints; on EIn->SOut, force the S-port merge west/up toward the split by adding the opposite of its slack: compute the static slack Up/Lo is not needed — instead build the template at a spec with tl = 6 (copy MakeTestJunctionSpec, set tl 6; everything else equal); SolveJunctionNodes on MakeMapTestTerrain; REQUIRE layout; CHECK CheckJunctionRules(layout, spec) has no WaitingRoom violation (INFO ViolationText). Red: the WaitingRoom violation (or another rule) appears. If the red run shows NO WaitingRoom violation at tl 6, try tl 8 and record; if none, STOP and report (the test would not fail for its reason).
2. "slot keeping keeps an interior piece" (inner): FlatT; find a free X run that holds the last slot of some stretch (WARN which); positions; AddJunctionNodeConstraints; CHECK Lo-feasibility: adding d * (x_q - x_p) <= 1 for it (AddDifference the other way) returns false.
3. All existing [nodes] tests pass unchanged (100-196 km/h, searches, OC1's tests).

## Do not
No change to the checker, the signal pass, junction_positions, the generator, the router. No prio row rules. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- A slot in rule 2/3 lies inside a free diagonal, or L and m have a free link between them other than L's own run: report the path and pieces (the constant would not hold).
- An existing test fails: report verbatim.
- OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. One commit on `wt-ru1`: `Feature: Junction node rules for waiting rooms and signal slots (D193)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; counts of rules 1-3 on the flat T; test 1's tl and result; `[nodes]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
