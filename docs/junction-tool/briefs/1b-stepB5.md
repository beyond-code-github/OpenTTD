# Brief 1b step B5: doubled bridges and whole-train bays in the checker

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Decisions:** D80 (doubled ML bridges), D83 (whole-train bays), D85 (doubled arrangement) in `docs/junction-tool/DECISIONS.md`. Read those three rows.
- **Code:** the checker in `src/junction_rules.h/.cpp`, its tests in `src/tests/junction_rules.cpp`, and the hidden design harness `src/tests/junction_design.cpp`.
  - Run the harness with `JUNCTION_DESIGN=<file> build/openttd_test "[.design]"`.
  - `JUNCTION_PIECES=1` lists each route's pieces as `index@x,y` (with a `b` suffix for pieces on a bridge).
- **Acceptance layout:** `docs/junction-tool/layouts/reference-LR-TL4-doubled.design`. Today it gives 12 violations. Every one of them comes from the rules this brief changes.

## 2. Read first (once, with offset/limit)

- `src/junction_rules.cpp`:
  - `CheckConnectivity` (around line 340–380)
  - `SwitchEventsOf`
  - `JoinerSignals` (around line 587)
  - `CheckBridges`
  - `CheckWaitingRoom` (around line 714)
  - the top of `CheckPrio` (lines 873–960: how the ML merge and its joiner are found)
- the helpers at the top of `src/tests/junction_rules.cpp` (`Build`, `Port`, `Has`, `AddSignal`) and one existing G7 and one G9 test
- the acceptance layout file

## 3. Definitions (use these exactly)

- **ML path:** a path whose movement is EIn→EOut or WIn→WOut.
- **Units:** `JunctionPiece::units`. The length of a path is the sum of its pieces' units.
- **Lane merge:** a merge event on an ML path where the joiner (the other path entering the switch tile on another trackdir, found exactly as `CheckPrio` finds it) has the same `from` and `to` as the ML path.
- **Queue merge:** a merge event on a path that is not an ML path.
- **Merge signal** of a queue merge at piece index `m`: the nearest signalled piece of that path before `m`, with index `s`.
- **Distance back from `s` to piece `i < s`:** the sum of units of pieces `i+1 .. s`.
- **Upstream split** of a queue merge: the nearest split event on the same path with index less than `s`, with index `u`. If there is none, `u` is the path's first piece.
- **Queue length:** L = distance back from `s` to `u`. Bay size B = `2 * spec.tl` units.

## 4. Changes (all in `src/junction_rules.cpp`, plus tests)

### 1. G1, doubled movements
In `CheckConnectivity`, a movement with more than one path is no longer automatically "ambiguous". It is allowed when both of these hold:
- **(a)** every pair of its paths has a non-empty common prefix and common suffix (compared by tile and trackdir), and the two remaining middles share no piece;
- **(b)** all its paths have the same length in units.

Violations:
- (a) fails: keep "ambiguous movement …".
- (b) fails: "doubled lanes differ in length", at the movement's from-port tile.

### 2. G5, ML bridges must be doubled
In `CheckBridges`, for every bridge that has a piece on an ML path, some other path of the same movement must use none of that bridge's pieces. Otherwise add `JunctionRule::Bridges` "ML bridge is not doubled" at the bridge's start tile, once per bridge.

### 3. Lane merges carry no prio
- `CheckPrio` skips lane merges entirely: no joiner Entry check, no "missing prio", no prio clauses.
- `JoinerSignals` also skips them, so their signals are checked as ordinary signals by G6's type clause.
- No other change to `CheckPrio`. Its coverage walk already runs once per ML path, so in a doubled section it runs along both lanes. Do not deduplicate violations.

### 4. G7, whole-train bays (D83)
Add to `CheckWaitingRoom`, for every queue merge on every path (keep the existing clauses as they are):
- **No merge signal:** "no signal before the queue merge".
- **L < B:** "less than one whole-train bay before the upstream split" at the merge tile.
- **Otherwise, for k = 1 .. floor(L / B):** let `i` be the piece whose distance back from `s` is exactly k·B.
  - **No such piece** (k·B falls inside a 2-unit piece): "bay boundary {k} falls inside a piece".
  - **`i` is on a bridge, is a bridge head, or is a switch piece of this path:** "bay boundary {k} on a bridge or switch".
  - **`i` is not signalled:** "no signal at bay boundary {k}".
- Report each at piece `i`'s tile, or at the merge tile when there is no `i`.

## 5. Tests (write each failing test first and record its red message)

Build fixtures with `Build`/`RasterizeJunctionRoute` route programs. Assert with `Has(...)` and its negation, following the existing tests.

1. **Doubled pass:** two W→W paths, where lane B leaves by `RFL`, carries a bridge, and returns by `LFR`. Neither "ambiguous movement" nor "doubled lanes differ in length" nor "ML bridge is not doubled" appears.
2. **Unequal lanes:** lane B with one extra S-bend pair, so it is longer. Expect "doubled lanes differ in length".
3. **Two stretches:** paths that diverge and reconverge twice. Expect "ambiguous movement".
4. **Undoubled ML bridge:** a single W→W path with a bridge. Expect "ML bridge is not doubled".
5. **Lane merge:** in fixture 1, the lane merge gives no G9 violation and needs no Entry.
6. **Bays:** a queue (an S-bound connection merging at an outbound merge, or a joiner at an ML merge) with L ≥ 2B and signals at B and 2B passes. Then:
   - remove the 2B signal: expect "no signal at bay boundary 2";
   - shorten the queue to L < B: expect "less than one whole-train bay";
   - shift the geometry so B lands inside a 2-unit piece: expect "falls inside a piece";
   - put a bridge where 2B lands: expect "on a bridge or switch".
7. **Prio coverage along both lanes:** a joiner merging right after the lane merge, so lane B's walk reaches lane B's bridge block. Expect "ML block within the train length needs exactly one join".
8. **Existing tests:** update any whose layouts now break the new rules, keeping each test's intent. G5 doubling and G7 bays are the likely ones. Report every test you changed and why.

## 6. Acceptance layout

1. Run the harness on `reference-LR-TL4-doubled.design`.
2. Reach 0 violations by moving or adding signals only (`rsig`/`signal` lines). Do not change routes or prio tracks.
   - Bays need signals at exact distances on the four queues: N→E, N→W, E→S and W→S.
   - Keep the spacing rule (G6) satisfied. Do not put a filler signal on the piece right before a required one if you can avoid it.
   - If it can't reach 0 by signal changes alone, stop and report the remaining violations with your bay arithmetic.
3. Then:
   - write `docs/junction-tool/layouts/reference-LR-TL4-doubled.txt` (harness output from the `shift` line up to the line before "violations");
   - write `tools/ottd/layouts/reference-LR-TL4-doubled.json` with `JUNCTION_JSON=<path>`, and `reference-LR-TL4-doubled-lefthand.json` with `JUNCTION_MIRROR=1` as well.
4. The old single-bridge `reference-LR-TL4.design` will now fail G5 (and maybe G7). Leave it and its exports unchanged.

## 7. Do not

- touch non-junction source files
- change public signatures in `junction_rules.h`
- add rule groups
- remove `[!mayfail]` from the outer test
- refactor code the brief does not name

## 8. Stop conditions

- A definition in §3 is ambiguous for a case you meet. For example, a queue merge on a path that also has a lane merge, or a path that starts inside a doubled section.
- The acceptance layout can't reach 0 by signal changes.
- Anything not covered.

## 9. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/stepB5_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is 182 cases; everything passes except the `[!mayfail]` outer test.

Commit message: `Change: Junction rule checker understands doubled bridges and whole-train bays`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 10. Report

- references read
- commit
- red evidence per new test
- functions changed
- tests changed and why
- suite totals
- the acceptance harness result, plus its ASCII and the signal lines you changed
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
