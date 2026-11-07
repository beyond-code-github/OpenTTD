# Brief 1b step B5b: bay boundaries per D88, then the doubled acceptance layout

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Previous step:** B5 (`98a66a83f5`) added whole-train bays to `CheckWaitingRoom` with exact k·B boundaries.
- **Ruling:** D88 in `docs/junction-tool/DECISIONS.md` replaces the exact boundaries. Read the D83 and D88 rows.
- **Brief:** B5's brief, `docs/junction-tool/briefs/1b-stepB5.md` §3, still defines merge signal, upstream split, distance back and B.

## 2. Read first

- the bay block in `CheckWaitingRoom` (`src/junction_rules.cpp`)
- the B5 bay tests in `src/tests/junction_rules.cpp`
- `docs/junction-tool/layouts/reference-LR-TL4-doubled.design`

## 3. Change the bay clause (D88)

Replace the k·B loop with a walk back from the merge signal at index `s`:

- Set `prev = s` and `k = 1`.
- Walk back from `prev`. Let `cand` be the first piece `i < prev` with distance back from `prev` to `i` ≥ B that passes all three checks:
  - `IsLegalSignalPiece(...)` or `IsSignalledPiece(...)`
  - not `on_bridge`, and not a bridge head
  - not a switch piece of this path (a split or merge event index)
- If there is no such piece, or `cand <= u` (at or beyond the upstream split), stop.
- If `cand` is not signalled, add "no signal at bay boundary {k}" at its tile.
- Set `prev = cand`, `k++`, and repeat.

Keep "no signal before the queue merge" and "less than one whole-train bay before the upstream split" (L < B), both unchanged.

Remove these two messages:
- "bay boundary {k} falls inside a piece"
- "bay boundary {k} on a bridge or switch"

## 4. Tests

Update the B5 bay tests:

| Test | Before | After |
|---|---|---|
| the "falls inside a piece" variant | failing | passes; assert no G7 bay message |
| the "bridge where 2B lands" variant | failing | passes when the first legal piece past the bridge is signalled |
| new: bridge variant without that signal | | expect "no signal at bay boundary 2" at that piece |

Keep the other bay cases. Record red evidence for every test you change or add, against the B5 code.

## 5. Acceptance layout

1. Run the harness on `reference-LR-TL4-doubled.design`.
2. Add or move signal lines only to reach 0 violations. B5 found that `rsig 6 9 block`, `rsig 3 19 block` and `rsig 4 16 block` were needed. Re-check them under the new rule.
3. If 0 is not reachable by signals, stop and report the violations with arithmetic.
4. Then:
   - write `docs/junction-tool/layouts/reference-LR-TL4-doubled.txt` (harness output from the `shift` line up to the line before "violations");
   - write `tools/ottd/layouts/reference-LR-TL4-doubled.json` with `JUNCTION_JSON=<path>`, and `reference-LR-TL4-doubled-lefthand.json` with `JUNCTION_MIRROR=1` as well.

## 6. Do not

- touch files other than `src/junction_rules.cpp`, `src/tests/junction_rules.cpp`, the acceptance `.design` file and the three exports
- change routes or prio tracks in the design file

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/stepB5b_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is 198 cases (B5's 189 plus the 9 router cases); everything passes except the `[!mayfail]` outer test.

Commit message: `Change: Junction bay boundaries are the first legal piece a train length back`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- red evidence
- tests changed
- suite totals
- the acceptance result, plus its ASCII and the signal lines added
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
