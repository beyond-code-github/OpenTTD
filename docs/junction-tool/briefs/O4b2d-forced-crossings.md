# O4b2d: forced crossings decided up front, clean-up fixed (continues O4b2c)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4b2`, branch `wt-o4b2`, head `dbe692962d`, with O4b2c's changes UNCOMMITTED (changes 1-4 of `briefs/O4b2c-hug-cleanup-convergence.md`). Continue from that state. You are the only writer there; never touch sibling worktrees or the main worktree, the running OpenTTD server or client. Never use `git stash`. build-rel is configured and built.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8`; a single case alone: `build-rel/openttd_test "<name>" > x.log 2>&1` (allow it several minutes; don't cut runs short at 110 s).
- Read first: briefs O4b2 (with Amendments 1-2), O4b2b, O4b2c (all in the main worktree's `docs/junction-tool/briefs/`), DECISIONS D164-D167 (main worktree), `FINDINGS-O4b2b.md` in your worktree, `src/junction_organic.{h,cpp}` and its tests.

## Why
O4b2c measured: starting conflicts at a bridge's price stops convergence (reverted, D167); the stuck "2 conflicts" are always a forced crossing (E->S against N->E at grade); the hug works; the clean-up pass produced invalid layouts at p >= 17 ("missing movement EIn to EOut", "stray track", "unexpected exit") for a reason not yet found.

## Changes, in this order
1. **Revert O4b2c change 1**: pf starts at U again. `max_rounds` stays 48.
2. **Forced pairs.** A static function computes, from the six ports' positions in clockwise order around the box boundary, every pair of movements (A, B) that share no port and whose endpoints interleave (exactly one of B's ports lies strictly between A's two ports in that cyclic order). Test 7 below pins the T's three pairs.
3. **Crossing cases.** `JunctionNegotiationPrices` gains `std::vector<std::pair<int, int>> over; ///< Forced pairs (over, under) by movement index (D167).` For each (A over B): a Surface piece of B on a tile where A has a Surface piece whose bit overlaps or crosses B's costs 1000 x U (and vice versa, from round 1, both directions), and a BridgeSpan piece of B over a tile where A has a Surface piece costs 1000 x U. A's BridgeSpan over B is priced normally. (TunnelBore: the mirror rule, B's bore under A is the allowed way and A's bore under B costs 1000 x U; on flat ground there are no tunnels.)
4. **Enumerate cases** in `BuildOrganicT`: every assignment of the forced pairs (2^k), ordered so that assignments putting main-line movements over come first (D156), then by index. Each case runs the full feedback loop; keep the best clean result by the existing score; stop enumerating once a case is clean and every remaining case starts with a box-span lower bound no better (skip the bound if not obvious: run all cases). Report per case: clean, iterations, rounds, occupied, slivers, bridges.
5. **Clean-up bug.** Find why the clean-up pass yields layouts with a missing EIn->EOut, stray track and unexpected exits (O4b2c, p >= 17). Fix it within the clean-up pass's own code. If the fix needs a design choice, stop and report the cause.

## Tests
O4b2's tests 1-5 as amended (Amendment 2's filter; test 4 = clean with feedback counts INFO), test 1 also INFO the winning case and bridge count, compared with O4b2b's best (257) and the old generator (243). Test 6 from O4b2c (every bridge crosses something). Test 7 "forced pairs of a T": the three pairs E->S x W main line, N->E x W main line, E->S x N->E for MakeTestJunctionSpec's ports at p 20. Red evidence for 6 and 7.

## Do not
- Change the line router, the checker, the signal pass or the old generator.
- Tune hug, history, pf or penalty amounts; add feedback rules; add the D80 move.

## Stop conditions
- Tests 1-3 unclean at every p in every case: report per p and case the violations left, rounds, ASCII of the best attempt.
- The clean-up fix needs a design choice.
- Any existing test fails (beyond a lone hardware flake).

## Gates
1. Build: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[organic]" 8`, then `"[junction]~[slow]"`.
3. Timing test alone, three times; BENCH lines (they will be slow with 8 cases; report as measured).
4. Squash everything since `da0d2b409c` into one commit (`git reset --soft da0d2b409c`, one `git commit`): `Feature: Junction organic T made rule-clean by feedback into negotiated routing`, body summarising O4b2-O4b2d in four lines, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; the clean-up bug's cause and fix; per test: p, winning case, compactness, cost, bridges, feedback counts, iterations, rounds, ASCII; the per-case table for the test spec at the winning p; BENCH lines; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
