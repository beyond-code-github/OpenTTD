# Brief 1c step PL2: prio coverage skips single-tile blocks (D99)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decision:** D99 in `docs/junction-tool/DECISIONS.md`. Read it.
- **Previous step** (57fd3b5e84) added `JunctionSpec::prio_tl`. At `prio_tl` 2 and 3 every candidate fails with "9 missing prio". The longer coverage meets a block of one ML tile (a split's switch, signalled on both sides) where no join fits.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/pl2/`
- **Don't run any `tools/ottd` command:** a rig run is live.

## 2. Read first

- `src/junction_signal_pass.cpp`: `PlacePrio` (about l.281–395). In particular the `required` blocks loop and the join search, which returns false when a block has no join tile.
- `src/junction_rules.cpp`: G9 part 4 "Coverage" (about l.1189–1203).
- `src/junction_rules_internal.h`: `ComputeBlocks` and `BlockOfPiece`.

## 3. Changes

1. **A helper in `junction_rules_internal.h/.cpp`:**

   ```cpp
   /** The number of distinct tiles of the path's non-bridge pieces that lie in the given block (D99). */
   uint JunctionBlockTilesOnPath(const JunctionLayout &layout, const std::map<std::pair<int, int>, size_t> &blocks, const JunctionPath &path, size_t block);
   ```

   Use whatever map type `ComputeBlocks` returns. Match it exactly.
2. **`PlacePrio`:** in the loop that builds `required`, skip a block when `JunctionBlockTilesOnPath(layout, blocks, lane, *block) == 1`. Use the lane path that loop iterates.
3. **Checker G9 part 4:** in the coverage loop, when `JunctionBlockTilesOnPath(layout, blocks, ml, *block) == 1`, don't add a violation for `joins_in(*block) == 0`. Exactly 1 join is still allowed; more than 1 is still a violation.
4. Nothing else changes.

## 4. Tests

1. **Checker, red first:** add a test in `src/tests/junction_rules.cpp` (or wherever G9 tests live; find them with grep "missing prio" or "ML block within the train length").
   - Take the generated test-spec layout with `prio_tl = 2`. It currently gives the violation "ML block within the train length needs exactly one join", or "missing prio".
   - Before the change, record which violations it has.
   - After both changes, `GenerateJunction` at `prio_tl = 2` must give a layout and `CheckJunctionRules` must be empty.
   - Update the existing test "GenerateJunction: a longer prio covers two train lengths" to REQUIRE the layout now, rather than accepting NoLayout.
2. **Golden at `prio_tl` 1 is expected to stay identical** (`[.golden]`, before and after, `cmp`).
   - If it differs, a single-tile block was inside 1-TL coverage somewhere, and the layout may now have fewer joins.
   - That would be acceptable under D99, but report the diff (`diff` of the two files, first 20 lines) and continue. Don't stop.
3. Release suite, all passing.

## 5. Experiment

1. Run `[.gen]` with `JUNCTION_GEN_PRIO` = 2 and 3. Report the candidate count, the failure histogram, and the final ASCII and size.
2. For each prio value that generates: `JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=tools/ottd/layouts/circuit-LR-TL4-prio{n}.json build-rel/openttd_test "[.circuit]"`. Commit those files.

## 6. Do not

- change the fill, bays or any other rule
- run the rig

## 7. Stop conditions

- Prio 2 still gives NoLayout after the change. Report the least-bad candidate's violations and ASCII, then stop with the code committed (it is still correct per D99).
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > pl2/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > pl2/dbg.log 2>&1`: no errors or warnings in the files you touched.

Commit message: `Change: Junction prio coverage skips single-tile blocks`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- commit
- red evidence
- suite totals
- cmp result (or the diff)
- the experiment numbers and ASCII
- the circuit files written
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
