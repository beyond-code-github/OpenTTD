# O4b2c: hugging, clean-up of needless bridges, faster convergence, last feedback rule (continues O4b2/O4b2b)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4b2`, branch `wt-o4b2`, head `dbe692962d` (two WIP commits on `da0d2b409c`: O4b2 and O4b2b). You are the only writer there; never touch sibling worktrees or the main worktree. Never use `git stash`. build-rel is configured and built.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`. Timing runs alone.
- Read first: briefs `O4b2-rule-clean-organic.md` (with Amendments 1-2) and `O4b2b-ml-turns-convergence.md` (the model you continue), `docs/junction-tool/FINDINGS-O4b2b.md` in your worktree, DECISIONS D165 and D166 (this step's rulings), `src/junction_organic.{h,cpp}` and its tests.

## Why (the engineer, in game, 2026-11-10)
"Apart from a couple of needless bridges, this is pretty good! They are not as compact as I'd build them." On the test-spec T at p 23: the split from a main line towards the branch is a long diagonal he would "hug in a bit", and the join from the branch into the other main line he would build "slightly closer to the diagonal" of the other split. Diagnosis (D166): routes that turn early or late cost the same money, so ties are broken arbitrarily and leave long, loose diagonals; and negotiation's history costs outlive the conflicts that caused them, so some bridges survive their crossing. O4b2b also measured slow convergence (about 33 rounds) because a conflict starts at U, far below a bridge's price, and one feedback rule missing (rule 8).

## Changes, in this order (measure the test spec at p 23 and 25 before change 1, then after each change: iterations, rounds, clean, occupied, slivers, bridges count)
1. **Conflicts start at a bridge's price (D166 c).** The present factor pf starts at the cheapest bridge for one track (the minimum over n = 0..max_bridge_middle of `spec.prices.bridge(n)` for spans `spec.bridge_span_ok` allows) instead of U, and doubles each round as before. `max_rounds` default in `BuildOrganicT`'s negotiation becomes 48 (O4b2b measured convergence by round 33 with the old start). Report the rounds actually needed.
2. **Hug preference (D166 a).** `resource_cost` adds U / 8 for a Surface piece of movement m when none of the 8 tiles around its tile, nor its own tile, holds a piece of another movement (any level). Shared tiles count as holding another movement's piece. Applies to every movement, every round, and in the clean-up pass.
3. **Clean-up pass (D166 b).** After a negotiation converges and before finishing: up to 3 passes over the movements in order; each movement is rerouted with history costs dropped (h = 0) and every conflict priced at 1000 x U, keeping the other prices (hug, switch penalties, reservations, ml_turn_cost); the new route replaces the old one when its router cost is lower than the old route's cost recomputed under the same prices, and it has no conflict. Stop when a pass changes nothing. Report bridges before and after on the test spec.
4. **Feedback for rule 8** ("a signal block holds both a split and a merge"): a switch penalty of U x 2^iteration on the violation tile and on the `spec.sd` tiles before and after it along each movement through it (as the signal-gap feedback), counted in a new `JunctionFeedbackCounts::block_mix_penalties`.

## Tests
O4b2's tests 1-5 as amended (Amendment 2's filter; test 4 = clean with feedback counts INFO). Test 1 adds INFO of the bridge count and compares compactness with O4b2b's best (211 + 46 = 257 at p 23) and the old generator (190 + 53 = 243). Add test 6 "BuildOrganicT: the clean-up pass removes a bridge nothing crosses": on the test spec at p 23, after the search, every `JunctionBridge` of the layout has at least one piece of another movement under its span or a main-line track it crosses (count tiles under each span holding track at grade); red evidence: record whether it fails before change 3 (if it already passes, say so).

## Do not
- Change the line router, the checker, the signal pass or the old generator.
- Tune the hug amount, the pf start or penalty growth beyond what is written; add other feedback rules.
- Add the D80 move.

## Stop conditions
- Any of tests 1-3 unclean at every p after the changes: report per p the violations left, feedback per iteration, rounds, and the ASCII of the best attempt, and stop.
- Any existing test fails (beyond a lone hardware flake).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[organic]" 8`, then `"[junction]~[slow]"`.
3. Timing test alone, three times; BENCH lines.
4. Squash everything since `da0d2b409c` into one commit: `git reset --soft da0d2b409c`, then `git commit` with `Feature: Junction organic T made rule-clean by feedback into negotiated routing`, body summarising O4b2-O4b2c in three lines and ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. FINDINGS-O4b2b.md is part of it.

## Report
References read; commit; the measurement table (before change 1 and after each change); per test: p, compactness, cost, bridges, feedback counts, iterations, rounds, ASCII; BENCH lines; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
