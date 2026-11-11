# N0a: curve speed as pairwise gap constraints, checked against the checker (D183, DESIGN-nodes 4a/4b)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
The node model (docs/junction-tool/DESIGN-nodes.md §4a, §4b) turns the game's curve speed rule into pairwise lower bounds between curve positions along a path, so the position solve can handle it without branching. This step adds that translation and proves it against the existing rule-3 implementation (CurveSpeedLimitOfTrain, a line-for-line copy of Train::GetCurveSpeedLimit). Nothing in generation changes.

## State
Worktree `/home/pete/OpenTTD/.claude/n0a`, branch `wt-n0a`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags: `'[nodes]'` (new; budget 1 s Release, 5 s Debug) and `'[curvespeed]'` in both builds; `jt gate build-rel` once before the commit. Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n0a/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; DESIGN-nodes.md §4a and §4b; RECON-edge-rules.md §1 and §2; in `src/junction_rules.cpp`: `CurveSpeedLimitOfTrain`, `PaddedPathDirections`, `JunctionMinCurveSpeed` (grep); `src/junction_rules.h` around `JunctionMinCurveSpeed`; `DeriveCurveSpacing` and `CurveSpeedLimitForSpacing` in `src/junction_generator.cpp` (~73-99) and their declarations in `src/junction_generator.h`; `src/tests/CMakeLists.txt` and the `junction_skeleton` entries in `src/CMakeLists.txt` (~292) for how files are registered.

## Definitions (exact)
n = 2 * tl (parts in a train window, one part per unit). Two curves share a window iff their positions differ by at most n - 2. CL = DeriveCurveSpacing(reference_speed, curve_speed, tl). For curves 0..k-1 along a path with hands h[] and positions pos[] (units, increasing), the constraints are, for every pair i < j:
- j == i + 1 and h[i] != h[j]: pos[j] - pos[i] >= 2.
- j == i + 1 and h[i] == h[j]: pos[j] - pos[i] >= min(n - 1, max(2, CL)).
- j >= i + 2: pos[j] - pos[i] >= min(n - 1, (j - i) * CL).
The oracle: a path is fine iff JunctionMinCurveSpeed's window minimum over its directions is >= reference_speed.

## Tests, red first (new file `src/tests/junction_chain.cpp`, tags `[junction][nodes]`)
1. "curve gaps of the test spec": tl 4, curve_speed 0, reference_speed 160 (CL = 5): hands {L, R} -> exactly {(0,1,2)}; {L, L} -> {(0,1,5)}; {L, R, L} -> {(0,1,2), (1,2,2), (0,2,7)}; {L} and {} -> empty. Red: the function returns an empty vector (declare and define it returning `{}` first; record the assertion failure).
2. "curve gaps agree with the checker": deterministic `std::minstd_rand` seeded 1; 5000 chains; tl drawn from {2, 3, 4, 6, 8}, curve_speed 0, reference_speed 160; 1..6 curves, each hand random, each gap to the previous curve random in 1..16; the first curve at position n (so the padding is straight). Build the directions: start DIR_NE... use `Direction::NE`-style names as in the codebase; a Left curve turns 45 degrees left (ChangeDir with the 45-left DirDiff), Right the other way; one Direction per unit; append n straight units after the last curve. Model = `JunctionCurveGapsHold(JunctionCurveGaps(...), pos)`; oracle = `JunctionMinCurveSpeedOfDirections(dirs, tl, 0) >= 160`. Assert: never model true and oracle false (soundness). Count model false and oracle true per tl ("conservative"); assert that count is 0 for tl 4; for every other tl, `WARN` the count (Catch2 WARN, so it prints). Red: with the empty-vector stub, model is always true, so soundness fails; record it.
3. `[curvespeed]` stays green (JunctionMinCurveSpeed is refactored onto the new function, same results).

## Changes
1. `src/junction_rules.h` / `.cpp`: `int JunctionMinCurveSpeedOfDirections(const std::vector<Direction> &dirs, uint8_t tl, uint8_t curve_speed);` with a doc comment: the least CurveSpeedLimitOfTrain over every window of 2*tl consecutive directions; UINT16_MAX when there is none. `JunctionMinCurveSpeed` becomes: pad as today with PaddedPathDirections, then return `JunctionMinCurveSpeedOfDirections(dirs, tl, curve_speed)`. No other change to rules.
2. New `src/junction_chain.h` / `src/junction_chain.cpp` (OpenTTD file header and include style as `junction_skeleton.*`), registered in `src/CMakeLists.txt` next to junction_skeleton:
   - `enum class JunctionCurveHand : uint8_t { Left, Right };` (doc: a 45-degree direction change along a path).
   - `struct JunctionCurveGap { uint16_t i; uint16_t j; int16_t min_gap; };` (doc: pos[j] - pos[i] >= min_gap, DESIGN-nodes 4a).
   - `std::vector<JunctionCurveGap> JunctionCurveGaps(const std::vector<JunctionCurveHand> &hands, uint8_t tl, uint8_t curve_speed, uint16_t reference_speed);` exactly the Definitions above, pairs in order (i ascending, then j ascending); doc comment cites Train::GetCurveSpeedLimit and DESIGN-nodes 4a, and says the set is sufficient, and exact when a window holds at most two curves.
   - `bool JunctionCurveGapsHold(const std::vector<JunctionCurveGap> &gaps, const std::vector<int> &pos);`
3. `src/tests/junction_chain.cpp` registered in `src/tests/CMakeLists.txt`.
4. `docs/junction-tool/TESTS.md`: a `[nodes]` row with the times you measure.

## Do not
No change to generation, the router, the skeleton, DeriveCurveSpacing, CurveSpeedLimitOfTrain's logic, or any other test. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Test 2 finds a soundness failure after the change (report the smallest failing chain: tl, hands, positions, oracle speed); a conservative count > 0 at tl 4; `[curvespeed]` changes; OVER BUDGET; anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `[curvespeed]` both builds, `jt gate build-rel`. One commit on `wt-n0a`: `Feature: Junction curve speed as pairwise gap constraints (D183)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence (both tests); types and functions added; the conservative counts per tl; jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
