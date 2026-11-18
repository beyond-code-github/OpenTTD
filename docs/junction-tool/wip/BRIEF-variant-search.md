# Step: variant local search past the enumeration cap (DESIGN-tracks §14)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`. One heavy job at a time (no parallel test runs besides jt's own). Time box 120 minutes; at the box, stop at a clean state (WIP commit allowed on wt-planar, say so) and report.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `0958786f36`, Release `build-rel`.
- Read first: `/home/pete/OpenTTD/CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md` ("Component solver"); `src/junction/CLAUDE.md` "Working rules for agents"; `/home/pete/OpenTTD/docs/junction-tool/TESTS.md` lines 1-10; `docs/junction-tool/wip/localsearch/ls.py` (the Python driver this step ports: one-bit flips, best improving neighbour, cache by tag) and the START/BEST lines of `wip/localsearch/ls2.log` (from tag o0t0f0s0 it reaches o17t0f1s0 at 1347.25 after 3 moves, 48 variants solved).
- Code: `src/junction/planar/planar_components.cpp` `SolvePlanarMapped` (~2841-2875: refuses more than 6 variant bits, else enumerates every variant, one thread each, then `PickBest` ~2695), `VariantOutcome` (~2201), `MappedTag`, `FlipPlanarTriangles`, `SolveMappedVariant`; `planar_components.h` `PlanarComponentLayout`.

## Behaviour
- Up to 6 variant bits: unchanged (every variant, one thread each).
- More than 6 bits (and neither `PLANAR_ONLY` nor `PLANAR_DRY` set): local search instead of the refusal. Start at the all-zero variant {0, 0, 0, 0}. Each step solves every one-bit neighbour of the current variant (flip one bit of over, ties, flips or steps, within each field's bit count) that is not yet solved, at most `SEARCH_THREADS` (= 4, a `static constexpr` in the .cpp, comment: the machine limit, one heavy job at a time) solving at once, each with its own variant deadline as today. Move to the neighbour with the least area when it beats the current variant's area by more than 0.01, or when the current variant failed and the neighbour solved (ties between neighbours: the lower tag in enumeration order f, o, ties, steps as the existing loops nest). Stop when no neighbour improves. Every outcome is cached by tag and solved once. The result is `PickBest` over every solved outcome.
- `PlanarComponentLayout` gains `size_t variants_solved = 0; ///< Variants solved by the search or the enumeration.`, set in both paths (enumeration: the number of variants run).
- No other behaviour change (shapes, deadlines, the variant solve itself untouched).

## Tests (outermost first; each red for the reason named, recorded before the code)
1. Existing `planar components: LL² from its topology text (mapped) at the probe's area` (`src/tests/junction/planar/planar_components.cpp` ~97): change its limit 807 -> 1347.25 and add `CHECK(layout.variants_solved > 0)` and `CHECK(layout.variants_solved < 200)` in that test only (through `CheckSolves` if it needs a parameter, else in the test). Red: "... too many variants" (the refusal). If it is red for any other reason, stop and report.
2. Add to the T2 mapped test (or a new `[junction][planarcomponents]` case on T2's text if the T2 test is `[slow]`): `variants_solved == 32` (enumeration). Red: 0.
3. Update `src/junction/planar/CLAUDE.md`: the "(at most 6 bits; ...)" sentence and the "Four variants per composition ..." / variant lines it contradicts: above 6 bits a local search from the all-zero variant (one-bit flips, best improving neighbour, 4 solves at once, cached) replaces the refusal; `variants_solved` counts them.

## Gates
- `/home/pete/OpenTTD/tools/junction/jt gate build-rel` (budget 30 s; report count and time; it ran 29-31 s before this step, a known overrun not yours).
- The LL² test above and the T2 and T3 mapped tests (`[slow]`), singly, with their times. LL² wall time is expected around a minute (48 variants, 4 at a time): report it.
- Commit on wt-planar: "Feature: Junction planar variant local search past the enumeration cap", ending with "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>".

## Do not
- Do not change the ≤6-bit path, the variant solve, the shape choice, deadlines or any composer code. No random starts, no restarts, no two-bit moves. Do not lift the cap for `PLANAR_ONLY`/`PLANAR_DRY` beyond what exists.

## Report
References read; commit hash and subject; red evidence per test; members added; gate results (count, time) and the three [slow] tests (pass, time, LL² area/variant/variants_solved); stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
