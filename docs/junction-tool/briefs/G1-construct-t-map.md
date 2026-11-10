# G1: construct the T's map, build once, check once (DESIGN-O4 "G1")

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## The principle (engineer: "maps don't have candidates")
One map is constructed from rules; one layout is built per height; the checker runs once on it. There is no sweep, no best-of, no retry with other parameters. If the result is not rule-clean, that is a missing constraint: stop and report it. Do not add a loop over alternatives to make a test pass.

## State
- Worktree `/home/pete/OpenTTD/.claude/g1`, branch `wt-g1`, head given at launch (from `feature/junction-tool`). Create `build` in the worktree configured like `/home/pete/OpenTTD/build` (read its CMakeCache for generator and build type); build `openttd_test` only.
- Design: main checkout `docs/junction-tool/DESIGN-O4.md`, section "G1: constructing the T's map" (read it; it is the authority).

## Test speed (engineer: "a test suite that runs in seconds not minutes")
New tests are tagged `[junctionmap]`; the tag runs in under 5 s (measure with `--durations yes`). Develop against that tag only. Run the wider gates once at the end.

## Read first (once each)
- `CODINGSTYLE.md`.
- `src/junction_generator.cpp` lines 120-330 and 324-760 (`BuildJunctionCandidateWithGap`: you reuse its pieces), `src/junction_generator.h`, `src/junction_skeleton.h`.
- `src/tests/junction_generator.cpp` lines 1-66 (`PinnedTestSpec`).

## What to build (`src/junction_generator.cpp`, declared in `src/junction_generator.h`)
```cpp
/** The constructed map of a T (DESIGN-O4 "G1"): the decisions taken and what they fixed. */
struct JunctionTMapResult {
	std::optional<JunctionLayout> layout; ///< Finished (cropped, signalled) when built; set even when not rule-clean.
	bool clean = false; ///< JunctionRulesClean on the layout, run once.
	std::vector<RuleViolation> violations; ///< CheckJunctionRules when not clean.
	int height = 0; ///< The height that routed.
	int heights_tried = 0;
	int layouts_built = 0; ///< Must equal the number of heights at which every edge routed (at most 1 with the current rule).
	uint8_t stagger = 0; ///< D118, chosen by the smaller bridge span.
	uint8_t e_phase = 0; ///< D148a.
	int64_t micros = 0;
	std::string log; ///< One line per height tried: which edge failed, or "built".
};
/** Construct the T's map from the rules and build it once (DESIGN-O4 "G1"). */
JunctionTMapResult ConstructJunctionTMap(const JunctionSpec &spec, const JunctionTerrain *terrain = nullptr);
/** D148a: the E-side window phase. 1 when prio_tiles is odd and at least 5, else 0. */
uint8_t JunctionEWindowPhase(int prio_tiles);
```

Construction, per height H from the generator's first height (the `first` in `GenerateJunctionUnchecked`) upward, stopping at the first H where every edge routes (cap: first + 8):
1. Frame, rows, stubs, `base` request and `base_layout` exactly as `BuildJunctionCandidateWithGap`, with `lane_style = 1` semantics.
2. Route A->S (the old E->S request: split window row 0, x 2..xI-2 plain, goal `goal_s`, the old column fence) and then N->A (the old N->E request: start `start_n`, merge window row 0 from `max(xO, x_s + prio_tiles + JUNCTION_E_PRIO_AFTER_SPLIT + e_phase)` to width-3) with B's row crossable at cost 0 (B goes over). These routes are final: they are not rerouted later.
3. The crossing set: the x of every piece of these two routes on the W row and on the b row (as `note_crossings`). Build `JunctionSkeletonInputs` exactly as the old parallelogram branch does, and solve for stagger 0 and stagger 1. Take the stagger whose solution is valid (no conflict, inside the frame, bridge spans ok) with the smaller `x_he[0] - x_hw[0]`; ties take 1 (as the old tie-break). If neither is valid: log it, try the next height.
4. Lanes from `BuildParallelogramLanes`; add them; route B->S (the old W->S request with `sl_x = pos.x_sl + 1`) and N->B (the old N->W request: goals b row, x `pos.x_sb + 1 .. pos.x_nw`), with the old fences and `under_spans`. Do not reroute A->S or N->A: the doubled bridge spans their crossings by construction.
5. Finish with the old `finish` logic (move it to a static function `FinishJunctionCandidate`, behaviour unchanged, used by both the old code and yours), then `JunctionRulesClean` once; on unclean, `CheckJunctionRules` once for `violations`.
Share code with the old function by extracting static helpers where the brief names them (`FinishJunctionCandidate`) and otherwise by calling the existing helpers; do not change `BuildJunctionCandidateWithGap`'s behaviour or `GenerateJunction`.

## Tests (`src/tests/junction_map.cpp`, new, registered in `src/tests/CMakeLists.txt`; tag `[junctionmap]`)
Red first: declare everything with stub bodies (empty result, phase 0) and see each test fail on an assertion.
1. "the constructed T is rule-clean": for `MakeTestJunctionSpec()` and the pinned spec (copy `PinnedTestSpec` as a static function): `clean` is true and `layouts_built == 1`. On failure print the violations and `log` (rule, tile, message).
2. "the T map is built once per height": `layouts_built` <= `heights_tried` and equals 1; a counter proves `JunctionRulesClean` ran once (expose nothing new: count via `layouts_built`, which you increment exactly where the checker runs).
3. "the E-side phase follows D148a": `JunctionEWindowPhase` for 1, 3, 4, 5, 7, 9, 14, 15 gives 0, 0, 0, 1, 1, 1, 0, 1.
4. "the doubled bridge spans the branch crossings": in the test-spec layout, every piece of A->S and N->A on the W and b rows lies strictly between the lanes' bridge heads.
5. BENCH, printed to the test output: `BENCH tmap spec=<name> height=<h> micros=<us> occupied=<n> slivers=<n> clean=<0|1>`. Also, in a separate `[.bench]` test (not in the 5 s budget), the old `GenerateJunction` for the same specs with its micros and occupied/slivers, for comparison.

## Do not
- Do not sweep any parameter, build more than one layout per height, or pick a best among layouts.
- Do not change `GenerateJunction`, `BuildJunctionCandidateWithGap` behaviour, the router, the skeleton solver, the signal pass or the checker.
- Do not touch `junction_organic.*`, savegame, network or command code.
- No `std::to_string`/`std::printf` in tests (poisoned); `fmt::format` or `std::cout`. No `pkill`, broad `kill` or bare `git stash`.

## Stop conditions
- Test 1 is not clean for a spec: stop, commit nothing, report the violations (rule, tile, message), the log, the stagger and phase taken, and an ASCII render (`RenderJunctionAscii`) of the layout. That is a missing constraint for the orchestrator; do not tune.
- `[junctionmap]` takes over 5 s.
- Anything the brief does not cover.

## Gates
- 0 warnings; `[junctionmap]` green under 5 s; once at the end `"[junction]~[slow]"` green (report time).
- One commit on `wt-g1`: `Feature: Junction T constructed from its map and checked once`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence per test; types and members added; suite results with times; BENCH lines; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
