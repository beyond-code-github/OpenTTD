# Brief 1c step PL3: prio coverage exempts only unjoinable blocks (D99a)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decision:** D99a in `docs/junction-tool/DECISIONS.md` (read it, and D99).
  - 8682323379 exempted every block the ML crosses in exactly one tile.
  - At SD 2 that is every block on a signalled straight, so prios lost all their joins but the merge tile's. The engineer saw it in game.
  - Narrowed rule: a covered block is exempt only if it has no tile where a join can go.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/pl3/`
- **Don't run any `tools/ottd` command.**

## 2. Read first

- `src/junction_signal_pass.cpp`, `PlacePrio`: the `required` loop (it uses `JunctionBlockTilesOnPath`) and the join-search loop.
- `src/junction_rules.cpp`, G9 part 4: the coverage loop (it uses `JunctionBlockTilesOnPath`).
- `src/junction_rules_internal.h/.cpp`: `JunctionBlockTilesOnPath`.

## 3. Changes

1. **`PlacePrio`:**
   - Remove the `JunctionBlockTilesOnPath(...) == 1` skip from the `required` loop, so every covered block is required again.
   - In the join search, when no join tile is found for a block, decide as follows:
     - if the block contains **any** tile of `ml` (non-bridge) whose `TracksAt` is exactly `TrackBits{axis}`, or the reserved pattern the search already recognises (`TrackBits{axis, h1}` with the neighbour `TrackBits{axis, h2}`), keep today's `return false`;
     - otherwise the block is unjoinable (all its ML tiles are switches or crossings): skip it with `continue`.
2. **Checker G9 part 4:** replace the `JunctionBlockTilesOnPath(...) == 1` exemption with: no violation for `joins_in(*block) == 0` when **no** `ml` tile in the block has `TracksAt == TrackBits{axis}`, where `axis` is the merge piece's track. More than one join is still a violation.
3. **`JunctionBlockTilesOnPath`:** remove it from the header and the .cpp if nothing else uses it. Grep first.

## 4. Tests (`src/tests/`); red first

1. **New "prios keep their joins at SD 2":**
   - For the test spec's generated layout (SD 2, `prio_tl` 1), find each ML merge that has a prio. Use the check context, as other G9 tests do, or locate the merge tiles of the joiner paths.
   - For each such prio, the farthest prio join is at least TL − 2 tiles upstream of the merge tile, along the ML row. A join is an ML-row tile with the axis plus a half piece toward the prio row.
   - Red: today only the merge tile has a join, so the distance is 0.
2. **Update "CheckPrio: clause 4, coverage":** 8682323379 flipped one of its CHECKs (a join-less one-tile block). Restore it to expect the violation when that block has a plain axis tile, which it does in that test. If the fixture's block is actually a switch tile, keep `CHECK_FALSE` and say so.
3. **"a longer prio covers two train lengths"** (`prio_tl` 2) must still generate and be clean. If it now gives NoLayout, report the least-bad violations and the ASCII, then stop.
4. Release suite, all passing. `[.golden]` will change; don't compare it.

## 5. Regenerate the circuits

`JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=tools/ottd/layouts/<file> build-rel/openttd_test "[.circuit]"`:
- n=1: `circuit-LR-TL4.json`
- n=2: `-prio2.json`
- n=3: `-prio3.json`

Commit them. Report each circuit's junction prio rows as ASCII (the rows just above the E track and just below... wherever the prio rows are), enough to see the joins.

## 6. Do not

- change any other rule, the fill or the generator
- run the rig

## 7. Stop conditions

- `prio_tl` 1 or 2 gives no clean layout.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > pl3/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > pl3/dbg.log 2>&1`: no errors or warnings in the files you touched.

Commit message: `Fix: Junction prio coverage exempts only blocks with no joinable tile`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- commit
- red evidence
- suite totals
- the `[.gen]` candidate counts at `prio_tl` 1/2/3 (`JUNCTION_GEN_PRIO`)
- the prio-row ASCII per circuit
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
