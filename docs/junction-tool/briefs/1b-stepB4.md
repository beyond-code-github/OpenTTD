# Brief 1b step B4: bridge signals and the stub-free merge join

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Decisions:** D81 and D82 in `docs/junction-tool/DECISIONS.md`.
- **Code:** the checker in `src/junction_rules.cpp` (step B3 is the latest) and `src/tests/junction_rules.cpp`.

## Read first (once)

- `CheckSignals`, `CheckPrio` (clause 1, the join shape) and their helpers in `src/junction_rules.cpp`
- the G6 and G9 fixtures in `src/tests/junction_rules.cpp`

## Changes

1. **G6 `CheckSignals`, new clause (D82).**
   - For every path and every bridge it crosses:
     - the nearest legal piece before the near head (walking back, stopping at a switch or the path start) carries a signal facing along the path
     - so does the nearest legal piece after the far head (walking forward, stopping at a switch or the path end)
   - If the stretch has no legal piece, there's no violation.
   - Messages: "no signal before a bridge" and "no signal after a bridge", at the bridge head tile.
2. **G9 `CheckPrio` clause 1 (D81).** At the join on the merge tile, the prio row tile must hold exactly h2, with no axis piece. If it holds the axis as well, report "stub on the merge-tile join" at that prio tile. Every other join keeps its prio tile as axis + h2, unchanged.
3. **Fixtures.**
   - Update every G6 and G9 fixture so it satisfies both new rules while keeping its intent.
   - Add failing fixtures, red first against the B3 code:
     - a path over a bridge with no signal before it
     - the same with none after it
     - a merge-tile join whose prio tile has axis + h2

## Do not

- touch the reference `.design`, `.txt` or JSON files; the orchestrator redoes the reference
- touch non-junction source files
- change public signatures
- remove `[!mayfail]`

## Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/stepB4_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is 180 cases; everything passes except the `[!mayfail]` outer test.

Commit message: `Change: Junction rule checker requires signals at bridges and a stub-free merge-tile join`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Signing is off.

## Report

- commit
- red evidence
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
