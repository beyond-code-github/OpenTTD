# Brief PRIO-d108: no ML switch inside a prio's coverage (D108)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decision:** D108 in `docs/junction-tool/DECISIONS.md`; read it, and D107.
- **Today:** the E prio's coverage (upstream of the N→E merge on the E row) reaches back over the E→S split. The W side already keeps its N→W merge clear of the lane merge (D107 3b).
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/d108/`
- **Don't run any `tools/ottd` command.**

## 2. Read first

- `src/junction_rules.cpp`: `CheckPrio` (G9), `CheckPrioReach` (G9b), and the `SwitchEventsOf` / `UsedTracks` helpers.
- `src/junction_generator.cpp`: the `route_movements` lambda (about l.244–290). Note the E→S route (`e_to_s`), which starts on the E row (row 0) at x ≤ xI−2 and heads +x (`X_SW`), and the N→E goals `PlainRowPieces(l, 0, Trackdir::X_SW, xO, 61)`. Also read `nw_max_for` (the D107 3b margin).

## 3. Changes

1. **Checker G9c, in `CheckPrio` or as a new clause.**
   - For each ML merge with a prio, walk upstream along each ML lane path of the movement through the merge piece.
   - The walk covers the same coverage as the G9 coverage loop (`covered < 2·tl·prio_tl` units), plus the pieces of one more signal segment beyond it: continue until you have passed one more signalled piece, and include the pieces up to and including the piece just past it.
   - If any `SwitchEventsOf` event of that lane (other than the merge itself) lies inside the walked range, add a violation (`JunctionRule::Prio`) `"switch inside prio coverage"` at the switch tile.
2. **Generator.** In `route_movements`, after E→S is routed, let `x_s` be the x of `e_to_s`'s first piece (the E→S split on the E row). The N→E goals become `PlainRowPieces(l, 0, Trackdir::X_SW, std::max(xO, x_s + cover + spec.sd + 2), 61)`, where `cover = spec.tl * spec.prio_tl`. This applies in every pass and lane style, since the lambda is shared.
3. Nothing else changes.

## 4. Tests; red first

1. **New G9c checker test:** the test spec's layout at head has a "switch inside prio coverage" violation at the E→S split once G9c exists, before the generator change.
   - Build the layout with `BuildJunctionCandidate` at the winning params, or use `GenerateJunction`'s result, before the generator fix. Assert the violation; that's the red.
   - After the generator change, `GenerateJunction(MakeTestJunctionSpec())` is fully clean.
2. **`[.gen]` counts at `JUNCTION_GEN_PRIO` = 1, 2 and 3.** If any is 0, stop and report the least-bad candidate's violations and ASCII.
3. Release suite, all passing. `[.golden]` will change; don't compare it.
4. Report the test spec's new size, against the 23×14 before.

## 5. Regenerate the circuits
`JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=tools/ottd/layouts/<file> build-rel/openttd_test "[.circuit]"`:
- n=1: `circuit-LR-TL4.json`
- n=2: `-prio2.json`
- n=3: `-prio3.json`

Commit them.

## 6. Do not

- change the prio shape, phase 3c or D99/D99a code
- change W-side margins

## 7. Stop conditions

- No clean candidates at some prio value.
- Anything not covered.

## 8. Gates

1. `cmake --build build-rel --target openttd openttd_test > d108/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > d108/dbg.log 2>&1`: no errors or warnings in the files you touched. Build only; don't run the Debug suite.

Commit message: `Change: Junction prios never cover an ML switch`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report

- commit
- red evidence
- `[.gen]` counts
- size before and after
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
