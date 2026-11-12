# Z2b: the ramp rule from the train (D219), realistic acceleration required, and a hard-facts placement mode (DESIGN-G1 §15.2-§15.4)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-z2b <HEAD>` (given at launch). Never use git stash.
- D218/D219 (read them) and RECON-slope-physics.md (read it): under the realistic model the slope force on a train is m x s x 100 N times the net fraction of its parts on up tiles; line speed holds iff that is within the spare tractive force S(v) = min(te, 746 x P x 18 / (5v)) - [10 m + m x 15 x (512 + v) / 512 + 14 x d x v^2 / 1000]. So within any tl consecutive tiles of a route the level may rise at most k_max = floor(tl x S(v) / (m x s x 100)) (v = spec.reference_speed). Zero loss (D219). The tool requires the realistic model.
- D218: only hard facts fail a solve (Blocked, Water, Spannable where track would sit, outside the snapshot); heights become costs (Z2c). Today the facts overload (T2/T3) also fails on heights.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DECISIONS.md D218, D219; DESIGN-G1.md §15; RECON-slope-physics.md.
- src/junction_generator.h JunctionSpec (tl, reference_speed, curve_limit, train); src/junction_train_model.h JunctionTrainParams; src/junction_generator.cpp DeriveCurveSpacing and the D138 test train (:110-114); src/junction_game.cpp where the spec is filled (:616 curve_limit) and where generation starts from the GUI (find the entry that returns an error to the player).
- src/junction_topology.cpp the facts block (T2 blocked tiles, T3 heights) and the facts overload's signature.

## Tests (record each red)
1. **`[topology]` "topology: ramp levels per train length from the train"**: JunctionRampLevels(spec) for the D138 train at v 196, s 3: equals the formula by hand (state the number; RECON says spare 163.8 kN vs full-train 168.3 kN, so k_max = floor(tl x 163.8 / 168.3)); a weaker train (half the power) gives a smaller k_max; s 0 gives "no limit" (returns tl or more). Red: stub returns 0.
2. **`[topology]` "topology: the hard-facts mode ignores heights"**: a terrain with a ridge row under a straight run (T3's failing case): the facts overload with `heights = false` solves (positions equal the flat solve's), with `heights = true` it fails with a "height" reason as today; a house still fails both. Red: stub.
3. **Game side, `[junction]` existing generator tests or a new one where the spec-from-settings code is tested**: with the original acceleration model the generator refuses with the message "Junctions need realistic train acceleration" (or the existing error mechanism's form). If no test reaches that code without a running game, say so and verify by reading only.

## Changes
1. JunctionSpec gains `uint8_t slope_steepness = 3; ///< _settings_game.vehicle.train_slope_steepness (%).` filled beside curve_limit.
2. `int JunctionRampLevels(const JunctionSpec &spec); ///< D219: the most levels a route may rise within tl consecutive tiles with no speed loss at reference_speed (realistic model, RECON-slope-physics); tl or more when the slope costs nothing.` in junction_generator.{h,cpp} beside DeriveCurveSpacing, integer arithmetic in the game's units.
3. The game's generation entry refuses under the original acceleration model with that message (D219).
4. The facts overload gains `bool heights = true` (last parameter, default keeps today's behaviour): false skips T3's height parts (row-run height steps, vertical-run joins, element fits/joins and node bits) and keeps only class facts (T2: Blocked, Water, Spannable where track sits, outside the snapshot). Element tiles still need their class to be Buildable.
5. `[realterrainsweep]` prints a second line `REALTERRAIN hard solved= failed=` with heights = false (no assertion beyond: every hard-solved placement renders and passes CheckJunctionRules ignoring height rules — CheckJunctionRules has no height rule; heights are checked separately and not here).
6. TESTS.md rows.

## Do not
- No level variables, no terraform use, no Z solve (Z2c). No change to relations or families.

## Stop conditions
- The formula's units don't match the game's (report); the generation entry has no error path to the player; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test` with `"[realterrainsweep]"`, `"[placeoracle]"` (to files); `tools/junction/jt gate build-rel` (report the count); the `openttd` target builds.
- One commit: `Add: Junction ramp rule from the train (D219), realistic acceleration required, hard-facts placement mode`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus k_max for the D138 train at tl 4 and tl 8 (v 196, s 3), and both REALTERRAIN lines. When you have reported, stop.

## Note (orchestrator, from Z2a's report)
With build_on_slopes, JunctionFacts::StraightRun stays true across any ridge (the game ramps the track along inclines and the heights still join); it fails only without slope building. So today's facts let a straight run climb with no ramp rule at all: that is what D219 must close (Z2c's level solve). Nothing in this step depends on it beyond knowing it.
