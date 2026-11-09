# O4b2: a rule-clean organic T: feedback, prios, signals, judge, score (DESIGN-O4 revision 3, search steps 2-4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4b2`, branch `wt-o4b2`, head in your launch prompt. You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1`.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Timing runs alone.
- Design: `docs/junction-tool/DESIGN-O4.md` revision 3 ("The search" steps 2-4), decisions D119, D137, D153, D159, D160-D163. O4b1 (merged) built `NegotiateJunctionMovements` in `src/junction_organic.{h,cpp}`.

## Why
O4b1's negotiated T converges (146 occupied, 25 slivers) and the existing signal pass places signals on it, but the full checker reports (measured): "missing prio" (the W merge: its prio row's side tiles are taken by a movement), "split inside prio coverage" plus PrioReach (the E->S split upstream of the N->E merge), waiting room short ("7 units, needs 8") and "less than one whole-train bay before the upstream split", "ML bridge is not doubled", and two Footprint complaints (a piece at y = -1, a signal on the boundary). This step closes the loop: feedback from the rules into the negotiation's prices, until the layout is rule-clean, then the judge and the score. Nothing may be hand-placed: the feedback only changes prices and minimum lengths; the router places all track.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`; DESIGN-O4 revision 3; D119, D137, D159-D163.
- `src/junction_organic.{h,cpp}` and `src/tests/junction_organic.cpp` (whole).
- `src/junction_generator.cpp` ~440-520 (crop, `NormaliseJunctionLayout`, `PlaceJunctionSignals`: the old pipeline's common end), ~826-830 `PrioTimingsOf`, ~920-950 (`RemoveRedundantJunctionSignals`, the prio timing gate with `spec.prio_strict`).
- `src/junction_signal_pass.cpp` ~260-447 (`GeometryOfPrio`, `PlacePrio`: the prio row is on the side opposite the joiner's approach, beside the main-line track, from the merge upstream over the coverage; its row tiles must be empty).
- `src/junction_rules.cpp`: the messages and tiles of the Prio, PrioReach, WaitingRoom, Bridges and Footprint violations (grep the message strings above).
- `src/tests/junction_generator.cpp`: `PinnedTestSpec` (~33) and "a long train with a narrow branch still fits" (~844).

## The model (settled; implement exactly)
1. **The finishing pipeline**: after a negotiation converges, build the layout as O4b1 does, then apply the old generator's common end in the same order (normalise as `NormaliseJunctionLayout` does, `PlaceJunctionSignals`), then `CheckJunctionRules`. If the old pipeline does something between those calls that the organic layout needs (for example cropping or extending the port rows so no signal falls on the boundary), mirror it and name it in the report; if mirroring needs a choice, stop and report.
2. **Main-line bridges (D163)**: `resource_cost` adds 1000 x U for BridgeSpan and Head pieces of the two main-line through movements (EIn->EOut, WIn->WOut).
3. **Feedback loop**: up to `max_feedback` (default 8) iterations. Each iteration: negotiate (O4b1's rounds) with the current extra prices and minimum lengths; finish (1); if no violations, stop (clean). Otherwise apply, for every violation, the rule's feedback below, and iterate. History costs carry over between iterations (do not reset them).
   - **Prio row needed** ("missing prio" at a main-line merge tile M): reserve the prio row: every tile on the side opposite the joiner's approach (as `GeometryOfPrio` decides it from the joiner's piece before the merge), beside the main-line track, from M upstream for `spec.prio_tiles + spec.sd + 1` tiles. Reserved tiles cost 1000 x U for any piece of any movement except the main-line through movement of that track (whose own tiles they are not; it never uses them anyway). Reservations persist across iterations.
   - **Split inside a coverage** (Prio "split inside prio coverage", PrioReach "ML tile in prio coverage does not hold the joiner"): for every main-line merge M on that track, add a switch penalty of U x 2^iteration on every main-line tile from M upstream for `spec.prio_tiles + spec.sd + 1` tiles (D108: coverage plus one segment). A switch penalty on tile T is paid by a piece on T that forms a split or merge with another movement's piece there (the O4b1 switch rule): `resource_cost` adds it.
   - **Waiting room or bay short** (WaitingRoom violations): every movement whose route has a merge on the violation tile (it shares the tile with a movement of the same `to` port with a different bit) gets `min_units` = its current track-0 units + 2 x `spec.tl` (one train length in the half-tile units `CheckWaitingRoom` counts). Persist across iterations.
   - **Any other violation** (Connectivity, Geometry, CurveSpeed, Order, Signals other than the boundary case handled by (1), Deadlock, Footprint after (1), Bridges other than D80): no feedback exists; stop the loop and report it as unclean.
4. **Judge**: a clean layout then passes the prio timing gate exactly as the old generator applies it (`PrioTimingsOf` equivalent, `spec.prio_strict`) and gets `RemoveRedundantJunctionSignals` as the old generator does. (Throughput beyond the prio gate and D159's branch prios are not in this step.)
5. **Search over p and score**: `GenerateJunctionOrganic(spec)` tries box W = 44, H = 16 and p in {14, 16, ..., 28}; keeps clean, gated layouts; scores by compactness (`MeasureJunctionCompactness`: occupied + slivers, D153), ties by cost (the layout's estimate as the old generator scores it, or rail pieces + bridge prices if no estimate function is reachable: name which), and returns the best with its p, score and per-rule feedback counts.

## API (in `src/junction_organic.{h,cpp}`)
```cpp
/** Counts of the feedback the search applied, per rule (DESIGN-O4 revision 3, step 2). */
struct JunctionFeedbackCounts {
	int prio_rows = 0; ///< Prio rows reserved.
	int coverage_penalties = 0; ///< Coverage windows penalised for switches.
	int length_raises = 0; ///< Minimum lengths raised.
	int iterations = 0; ///< Feedback iterations run.
};

/** One organic T for a box and branch position (steps 1-4); the layout is set when it is rule-clean and passes the prio timing gate. */
struct JunctionOrganicResult {
	std::optional<JunctionLayout> layout;
	std::vector<RuleViolation> violations; ///< Left after the last iteration when unclean.
	JunctionFeedbackCounts feedback;
	int p = 0;
	JunctionCompactness compactness{};
	Money cost = 0;
	std::string error;
};

JunctionOrganicResult BuildOrganicT(const JunctionSpec &spec, int width, int height, int p, int max_feedback = 8);
/** The best rule-clean organic T over the branch positions tried (step 4). */
JunctionOrganicResult GenerateJunctionOrganic(const JunctionSpec &spec);
```
`NegotiateJunctionMovements` gains the extra inputs it needs (reserved tiles, switch penalties, per-movement `min_units`) as one `const` struct parameter with defaults, so O4b1's tests keep their call unchanged; name it `JunctionNegotiationPrices`.

## Tests (red first: with stubs returning an empty result, record each failure), tag `[junction][organic]`
1. Outer: "GenerateJunctionOrganic: the test spec's T is rule-clean": `MakeTestJunctionSpec()`: layout set; `CheckJunctionRules` on it is empty (nothing ignored); INFO p, compactness, cost, feedback counts, ASCII; and the old generator's numbers for comparison (`GenerateJunction(spec)`: its occupied and slivers via `MeasureJunctionCompactness`).
2. "GenerateJunctionOrganic: the pinned spec's T is rule-clean": the same with `PinnedTestSpec()` (copy the helper or expose it as the test file allows without changing other tests).
3. "GenerateJunctionOrganic: a long train with a narrow branch": the spec of the old test "a long train with a narrow branch still fits": layout set and clean.
4. "BuildOrganicT: feedback fixes the measured violations": `MakeTestJunctionSpec()`, W 44, H 16, p 20: clean, and `feedback.prio_rows >= 1` (the W merge's prio row had to be reserved, as measured).
5. `[.bench]` "GenerateJunctionOrganic: timing": tests 1-3's specs timed (1 warm-up, 5 runs), printing `BENCH organic_gen <spec> clean=<0|1> p=<n> occupied=<n> slivers=<n> iterations=<n> median_us=<n>`, and one line per p for the test spec: `BENCH organic_p <p> clean=<0|1> iterations=<n> occupied=<n> slivers=<n> us=<n>`.

## Do not
- Hand-place track, prios, splits, merges or bridges; change the checker, the signal pass, the old generator or the line router.
- Add the D80 doubled-bridge move, terrain cases, D159 branch prios, throughput beyond the prio gate, or tune the feedback amounts beyond what is written.

## Stop conditions
- Test 1 is unclean after `max_feedback` iterations for every p: report, per p, the violations left (rule, tile, message), the feedback applied per iteration, and the ASCII of the best attempt. This is the step's main risk; report it fully rather than adding feedback rules.
- Mirroring the old pipeline's finishing needs a choice.
- Any existing test fails (beyond a lone hardware flake: rerun alone and report).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[organic]" 8`, then `"[junction]~[slow]"`.
3. Timing test alone, three times; report the BENCH lines.
4. Commit: `Feature: Junction organic T made rule-clean by feedback into negotiated routing`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. One commit.

## Report
References read; commit; red evidence per test; names added; what the old pipeline's finishing needed (1); per test: p, compactness, cost, feedback counts, iterations, ASCII; the old generator's compactness beside it; BENCH lines; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.

## Amendment 1 (orchestrator, after the first run)
The first run's work is uncommitted in the worktree (API, D163 price, reservations, switch penalties, finishing pipeline with the crop mirror, prio gate, score, tests): continue from it. Its open choices are accepted (history pointer in `JunctionNegotiationPrices`, windows walked along the main-line path, 0-based iteration so the first penalty is U). Rulings on its three questions:
1. **The box boundary holds only ports.** The checker forbids signals and bridge heads on the boundary, and today's routes ran along the bottom row and bridged over the branch port tiles. Every router request of the negotiation gets `blocked` = every tile of the bottom row (y = H - 1) and of the left and right columns (x = 0, x = W - 1) except the six port tiles. The top row is not blocked (the E main line runs on it). Blocked tiles carry no track and no bridge span (the router's existing meaning).
2. **Feedback for "split in a bridge's block" (Signals, D113)**: a switch penalty of U x 2^iteration on the violation tile. Count it in a new `JunctionFeedbackCounts::bridge_block_penalties`.
3. **Waiting room and bays: move the split, not the length.** Replace the `min_units` raise. For each WaitingRoom violation at tile M: every movement with a merge at M; for each, its split from its `from`-port sibling (the last tile both routes share before they part); add a switch penalty of U x 2^iteration on that split tile and on the next 2 x `spec.tl` tiles of the sibling's route downstream of it, so the split can only move upstream (the movement's private part, its waiting room, grows without the route growing). Count each in `coverage_penalties`... no: in `length_raises` renamed `split_moves` (rename the member). `min_units` stays in `JunctionNegotiationPrices` but this step sets none.
4. The Footprint violations other than the boundary ones, if any appear after 1, are a stop.
Report additionally: whether the W main line still detours (in the first run it left its row and ran three rows lower so that branch movements could bridge over it), with its moves, because the engineer will want to judge that.
