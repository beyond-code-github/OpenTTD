# O4b2b: main-line turn cost, the non-converging specs, and the last feedback rules (continues O4b2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4b2`, branch `wt-o4b2`, head `e790a645af` ("WIP: Junction organic T feedback loop (O4b2, not for merge)"). You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash`. build-rel is configured and built.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Timing runs alone.
- Read first: `docs/junction-tool/briefs/O4b2-rule-clean-organic.md` (whole, with Amendments 1 and 2: the model you continue), DECISIONS D115/D115a, D119, D164, D165 (this step's ruling), the WIP commit's diff against `da0d2b409c` (`git diff da0d2b409c..HEAD --stat`, then the organic files).

## Where O4b2 stopped (measured)
- Test spec: clean at p 16 and 22-28; best p 24: 230 occupied / 39 slivers (the old generator: 190 / 53). The W main line leaves its row for 27 tiles (up a row, bridges, back down at x = 1, curves into the port), which costs tiles and leaves "missing prio" with no prio geometry at a curved merge.
- Pinned spec: every p fails in iteration 2, "2 conflicts left after 16 rounds".
- TL6 narrow branch (`branch_gap = 0`, `tl = 6`): every p fails in iteration 0 with 2 conflicts left (not caused by the boundary block).
- No feedback yet for "signals too far apart".
The finishing choices are accepted: crop mirror, row extension, and one plain Y tile past each branch port (D115's one tile).

## Changes, in this order
1. **Main-line turn cost (D165).** `JunctionNegotiationPrices` gains `Money ml_turn_cost = 0; ///< Turn cost of the two main-line through movements (D165); 0 means the branch movements' turn cost.` Router requests of EIn->EOut and WIn->WOut use it as `turn_cost` when non-zero. `BuildOrganicT` and `GenerateJunctionOrganic` take it through a new trailing parameter `Money ml_turn_cost = 0` and pass it on. No other change to how turns are priced.
2. **Measure D165** before anything else changes: test spec, p 14..28, at `ml_turn_cost` = rail_piece x tl / 2, rail_piece x tl, rail_piece x 2 x tl (and 0 as the baseline). Per value and p: clean or not (and why), iterations, occupied, slivers, cost, and whether either main line leaves its row (count main-line pieces off rows 0 and ml_gap + 1, bridges included as on-row when their heads are on the row). Write the table to `docs/junction-tool/FINDINGS-O4b2b.md`. The default used by tests from here on is rail_piece x tl (the middle value); the engineer picks the final one from the table.
3. **Why the pinned and TL6 cases don't converge.** At ml_turn_cost = rail_piece x tl: if they now converge, say so and skip to 4. If not: for the TL6 case at p 20, report the two conflicts left (tiles, movements, pieces, levels), the conflict count per round, the routes' moves, and the ASCII of the last round's routes; then try, one at a time and without keeping them, `max_rounds` = 48, and the box height H = 20, and report which (if either) converges. Do not change the cost schedule, the switch rule or the router. Record the diagnosis in FINDINGS-O4b2b.md. If neither converges, stop after recording (this is a design question for the orchestrator).
4. **Feedback for "signals too far apart"** (Signals rule): a switch penalty of U x 2^iteration on the violation tile and on the `spec.sd` tiles before and after it along each movement through that tile (so splits and merges leave the stretch where the signal pass can't place signals). Count it in a new `JunctionFeedbackCounts::signal_gap_penalties`.
5. **Feedback for "missing prio" with no prio geometry** (the merge is not on a straight main-line piece): a switch penalty of U x 2^iteration on the merge tile, counted in `coverage_penalties`, instead of the empty reservation.

## Tests
O4b2's tests 1-5 as written there (with Amendment 2's filter), using the default ml_turn_cost. Test 4's `prio_rows >= 1` expectation is replaced by: clean, and INFO the feedback counts. Red evidence is not required for unchanged tests; for changes 4 and 5, record in the report the first violation each one fixed (iteration, tile).

## Do not
- Change the line router, the checker, the signal pass or the old generator.
- Tune the cost schedule, feedback amounts or penalty growth beyond this brief.
- Add the D80 move (O4b3).

## Stop conditions
- Change 3 finds no converging variant (after recording).
- Any test of 1-3 stays unclean after the changes: report per p the violations left, feedback per iteration, ASCII of the best attempt, and stop.
- Any existing test fails (beyond a lone hardware flake).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[organic]" 8`, then `"[junction]~[slow]"`.
3. Timing test alone, three times; report the BENCH lines.
4. Commit on top of the WIP commit: `Feature: Junction organic T made rule-clean by feedback into negotiated routing`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Then squash the WIP commit and yours into one (`git reset --soft da0d2b409c` then one `git commit` with that message) so the branch has one clean commit.

## Report
References read; commit; the D165 table (from FINDINGS-O4b2b.md); the convergence diagnosis; per test: p, compactness, cost, feedback counts, iterations, ASCII; the old generator's compactness beside each; BENCH lines; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
