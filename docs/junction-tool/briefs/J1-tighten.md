# Brief J1: make the requested W-side tightening actually happen

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Read first:**
  - the diagnosis behind this brief, in the scratchpad `why/` files: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/why/` (`winner.txt`, `item1_closer1.txt`, `item1_closer2.txt`, `item2_q0.txt`, `item2_zero_relaxedtrace.txt`);
  - D107, D108, D110, D113 and D116 in `docs/junction-tool/DECISIONS.md`.
- **The engineer said it plainly:** "none of them are fixed". The final layout must change visibly, and you must show it.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/j1/`
- **Don't run any `tools/ottd` command.**

## 2. Changes

1. **Separate slacks (`junction_generator.h/.cpp`).**
   - `JunctionCandidateParams::joiner_slack` is split into `e_slack` (the N→E window, about l.300) and `w_slack` (the N→W window, `nw_max_for`, about l.325).
   - `GenerateJunction` and `BuildJunctionLibrary` sweep both from 0 to 3. Within the per-height batch the order is crossing cost, then `e_slack`, then `w_slack`, then margin, then style.
   - If the S1 time in `build-prof` exceeds 1.5 s, report it; don't optimise.
2. **Lane B entry (about l.362–375).** The `q` loop starts at 0. Remove the `want_axis` outer loop and the `plain_axis` check. Instead, accept q values in increasing order, keeping the first whose near head lands at `(h_e, b_row)`. (q = 0 gives one real plain tile after the S-bend, which carries the signal before the bridge.) Lane A follows as the mirror, or as style 1 does today.
3. **G9c (`junction_rules.cpp` about l.1227–1247).** Drop the "+ one more signalled segment" extension. G9c checks only the coverage itself (the walk with `covered < 2·tl·prio_tl`). Per D116, G9b (the real-semantics prio reach) is the judge of anything beyond it. Update the comment to cite D116.
4. **D113, a split in a bridge's block (`junction_rules.cpp`, the D82 clause in `CheckSignals`).**
   - Where `nearest(i − 1, −1)` stops at a split before finding a signal-legal piece (about l.707), it currently returns nullptr and the check passes vacuously. Instead, add a violation: `"split in a bridge's block: no signal between the split and the bridge"`, at the bridge head.
   - Do the same for the far side: `nearest(last + 1, 1)` reaching a merge.
   - This applies to every path (ML and branches).
5. **G1 path tracer (about l.220).** Allow entering a bridge head from a corner piece too, not only from an axis piece. OpenTTD allows a curve straight onto a ramp. The rule from change 4 now keeps out the throughput fault the diagnosis found.

## 3. Tests; red first where the item is new

1. **D113:** a hand-built path where a split tile is directly followed by a bridge head is flagged. With one signal-legal plain tile carrying a signal between them, it isn't. Red: today there is no violation.
2. **G1:** a hand-built corner piece directly before a head is traced into the bridge, with no "dead end" violation. Red: today it gives a dead end.
3. **G9c:** the diagnosis's `item1_closer2` variant (two tiles closer) no longer has "split inside prio coverage" at the lane merge. Reproduce it via `BuildJunctionCandidate` params if you can, else from a hand-built fixture equivalent; WARN if not reproducible.
4. **Outer test, the shape the engineer asked for.** For `GenerateJunction(MakeTestJunctionSpec())`:
   - `CheckJunctionRules` is empty;
   - on the W side, the plain tiles between lane B's entry S-bend and its near head are ≤ 1;
   - the N→W merge to lane merge distance is less than the head's value of 8. Measure both from the layout pieces, writing the measuring code in the test.
   Red: today both assertions fail (2 tiles and 8).
5. **Report the before and after ASCII** of `GenerateJunction(MakeTestJunctionSpec())` with x indices, plus size and cost. Name the before/after values of both W-side measures.
6. **`[.gen]` counts** at prio 1.
7. Release suite, all passing. The width test (`size.x <= 32`) must hold. `[.golden]` will change.

## 4. Regenerate the circuit
`JUNCTION_GEN_PRIO=1 JUNCTION_CIRCUIT=tools/ottd/layouts/circuit-LR-TL4.json build-rel/openttd_test "[.circuit]"`. Commit it.

## 5. Do not

- implement D110 parts 3 and 4 (SL-first split, shift-back): the next step
- change the score (D114), the stubs (D115) or the crop: a later step

## 6. Stop conditions

- The outer test's shape assertions can't both pass with a clean layout. Report the best layout and its violations.
- Anything not covered.

## 7. Gates

1. `cmake --build build-rel --target openttd openttd_test > j1/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > j1/dbg.log 2>&1`: no errors or warnings in the files you touched. Build only.

Commit message: `Fix: Junction W side builds the tight entry and prio spacing it was meant to`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Use "split", never "switch", in user-visible text.

## 8. Report

- commit
- red evidence
- the before and after ASCII, with the two W-side measures and cost
- `[.gen]` counts
- the S1 time
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
