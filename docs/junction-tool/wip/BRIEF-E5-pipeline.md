# BRIEF E5-pipeline: the drawing pipeline for optional flex, built in (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head: given in the launch message. Release build `build-rel`, `-j 4` only.
- Source: DECISIONS D422 (adopted; read the row in full) and D418. The pipeline exists today only as probe code in the hidden test "planar draw: PSG 197 option (1), directions from earlier drawings" in `src/tests/junction/planar/planar_draw.cpp`. This step ports it into the library with D422's defaults, and leaves the probe as it is.

## 2. Read first (once each, offset/limit)
- The probe test above, in full: the seed, the rounds loop (`PLANAR_PROBE_EXTREMAL`, `PLANAR_PROBE_ZERO_TWICE`, `PLANAR_PROBE_FLEX_PRICE`, `PLANAR_PROBE_IRL1`, `PLANAR_PROBE_BOX`), and the `PLANAR_PROBE_RATCHET` block (commit rounds, prune by `curve flex <track> <piece>` tags, final). This is the logic to port.
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions`, `PlanarDrawResult`, `PlanarDrawPins`.
- `src/tests/junction/planar/planar_draw.cpp`: `ShapeFirst`, `DrawnCurveGaps`, the committed-flex test.
- `CODINGSTYLE.md` (Doxygen and naming).

## 3. Tests (outermost first; red before green)
1. **"planar draw: the pipeline draws the one-lane T with optional flex (D422)"**, `[junction][planar][planardraw]`: `ShapeFirst(PlanarTText(1), 1)`, then `DrawPlanarPipeline(c.v, {})`. REQUIRE `final.status == "Optimal"`. CHECK every gap from `DrawnCurveGaps(c, r.final, false, &r.committed)` is >= its least, and every flex middle below `commit_above` in `r.committed` has length 0 in `r.final.lengths`. Red: the function declared and returning a default result first.
2. **"planar draw: the pipeline on PSG 197 (D422)"**, hidden `[.][planarkeepapart]`: the flex-C variant exactly as the probe loads it (`LoadPinned197(flex_c, flex_c_pins, true)`), no pins, `DrawPlanarPipeline(flex_c, {})`. WARN every `log` line, `committed`, `pruned`, and the final with the probe's `describe`. CHECK `final.status == "Optimal"`. Run it once and report its WARN lines and time. Expected, from the probe run that motivated D422: about 10 committed, Optimal in about 17 s. Different numbers are not a stop (the probe's seed and order may differ slightly); report them.

## 4. Changes (planar_draw.h / planar_draw.cpp)
a. Types, after `CheckPlanarDrawable`'s declaration, with Doxygen on each member:
```cpp
/** The drawing pipeline's settings (D422 defaults). */
struct PlanarDrawPipelineOptions {
	const PlanarDrawPins *pins = nullptr; ///< Answer-key pins, or none.
	double round_seconds = 30.0; ///< Time limit of each LP round.
	double final_seconds = 60.0; ///< Time limit of the integer final.
	double first_flex_price = 0.05; ///< Insert price per half tile in the first discovery round.
	double irl1 = 0.1; ///< From the second discovery round, an insert's price is irl1 / (its previous length + 0.5).
	double box_price = 0.01; ///< Price per half tile of width and of height, in every round.
	double commit_above = 1.0; ///< An insert at or above this length (half tiles) in the last discovery drawing is committed.
	int discovery_rounds = 8; ///< At most this many discovery rounds.
	int commit_rounds = 4; ///< At most this many rounds after the commit, and again after the prune.
};

/** What the drawing pipeline did, and its drawing. */
struct PlanarDrawPipelineResult {
	PlanarDrawResult final; ///< The integer final (or the last result before the pipeline stopped).
	std::vector<std::vector<double>> committed; ///< The commitment reference handed to the final (as curve_flex_from).
	size_t committed_count = 0; ///< Inserts committed before the prune.
	size_t pruned = 0; ///< Inserts the prune held at 0.
	std::vector<std::string> log; ///< One line per round: stage, round, status, total slack, curve slack, box, length.
	std::string stopped; ///< Empty, or the stage that ended without an Optimal round.
};

PlanarDrawPipelineResult DrawPlanarPipeline(const PlanarShapeVariant &v, const PlanarDrawPipelineOptions &options);
```
b. `DrawPlanarPipeline` in planar_draw.cpp, the probe's order with these settings:
   1. Seed: `CheckPlanarDrawable(v, round_seconds, {.elastic = true, .elastic_tidy = 0.001, .pins = options.pins})`; not feasible: `stopped = "seed"`, return.
   2. Discovery rounds k = 1..discovery_rounds: `{.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = true, .flex_weights = (k == 1 ? nullptr : &weights), .box_price, .flex_price = (k == 1 ? first_flex_price : 0), .elastic = true, .elastic_tidy = 0.001, .elastic_families = {"sep"}, .pins, .sides_from = &previous.points}` (keep the header's field order). After each Optimal round, weights[t][i] = irl1 / (lengths[t][i] + 0.5). Stop after a round at slack 0 whose previous round was also at slack 0 (the probe's zero-twice), or at a round that is not Optimal (`stopped = "discovery"`, return), or after the last round.
   3. Commit: `committed` = the last discovery round's lengths; `committed_count` = flex middles at or above `commit_above`. Rounds up to commit_rounds with `.curve_flex_from = &committed, .curve_flex_above = commit_above, .box_price, .elastic_families = {"sep", "curve"}`, sides from the previous round; stop at slack 0 or when slack falls by at most 0.5.
   4. Prune once: each stretched tag `curve flex <track> <piece>` sets `committed[track][piece] = 0` (`pruned` counts them); if any, the commit rounds again (step 3's loop) from the last round.
   5. Final: `CheckPlanarDrawable(v, final_seconds, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = true, .curve_flex_from = &committed, .curve_flex_above = commit_above, .pins, .sides_from = <the input drawing of the last Optimal round>})`. The input drawing is the sides the last round proved (CARD-E5-flexstep's correction: never re-read sides from a round's own rounded output).
   Every round appends a `log` line.
c. `src/junction/planar/CLAUDE.md`, drawing program section: one bullet with D422's pipeline (stages, defaults, that the final takes the proved sides). Remove the bullet's claim, if any, that whole-tile integrality is deferred to Slots (D419 amended by D422).

## 5. Do not
- Do not change `CheckPlanarDrawable`'s defaults or any row family.
- Do not remove or edit the probe test or its switches.
- Do not add the additions pass, warm starts, or any tuning; D422's defaults only.

## 6. Stop conditions
- Test 1 not Optimal, or a gap below its least.
- `[planardraw]` over its 5 s budget with test 1 in it: report the time (do not tag `[slow]` yourself).
- Anything outside `planar_draw.h`, `planar_draw.cpp`, `src/tests/junction/planar/planar_draw.cpp` and `src/junction/planar/CLAUDE.md` seems needed.

## 7. Gates
- `tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flake: "planar components: T2 mapped lands in its box at the anchor"; rerun alone and report.
- Test 2 once by name (hidden), output to a file; report its WARN lines.
- One commit: `Add: Junction: the drawing pipeline for optional flex (D422)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 8. Report (short, fixed format)
- References read
- Commits: hash and subject
- Red evidence: each new test and its failure before the code
- Types and members added or removed: names only
- Suite results, and test 2's WARN lines and wall time
- Stops and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
