# Brief 1b step B3: the first prio join on the merge tile

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Decision:** D79 in `docs/junction-tool/DECISIONS.md`. It revises D59.
- **Code:** step B2's checker (`src/junction_rules.cpp`, `CheckPrio`) and its fixtures (`src/tests/junction_rules.cpp`).

## Read first (once)

- `CheckPrio` and its helpers in `src/junction_rules.cpp`
- the G9 fixtures in `src/tests/junction_rules.cpp`
- `docs/junction-tool/layouts/reference-LR-TL4.design`

## Changes

1. **`CheckPrio` clause 1 (shape).** A join's ML tile T may also be the merge switch tile M. There, T holds p's axis, the joiner's half piece and h1, and nothing else. Elsewhere, joins keep the B2 rule: axis + h1 only.
2. **`CheckPrio` clause 3.**
   - The downstream join must be **on M's switch tile**. A join after M, or none at M, is a violation: "the first prio join must be on the merge tile".
   - p's first signal after M is no longer compared against a join; G6 still requires it.
3. **Clause 2 exception.** At a join on M, h1 meets p's track at T's exit edge and inside T. The joiner's piece is also inside T. All of those contacts are the join or the merge itself, so they're allowed.
4. **Fixtures.**
   - Update the G9 fixtures so their first join is on the merge tile, keeping each one's pass/fail intent.
   - Add failing fixtures:
     - first join one tile after the merge, expecting "the first prio join must be on the merge tile"
     - a merge tile join holding an extra unrelated track, expecting a shape violation

   Red first for the new failing fixtures: run them against the B2 code and record the failure.
5. **Reference layout `docs/junction-tool/layouts/reference-LR-TL4.design`.**
   - **E prio:**
     - Drop prio-row tile (23,-1).
     - Make (22,-1) `X Right`, and add `track 22 0 Left` (the merge tile Q).
     - Remove the ML join pieces at (23,0).
     - Move the E-row after-merge signal from route 0 piece 17 (24,0) to piece 16 (23,0).
   - **W prio:**
     - Drop prio-row tile (9,1).
     - Make (10,1) `X Lower`, and add `track 10 2 Upper` (the merge tile A).
     - Remove the ML join pieces at (9,2).
     - Move the W-row after-merge signal from route 2 piece 18 (8,2) to piece 17 (9,2).
   - **Trim the west edge by one column** if the checker allows it: start routes 0 and 1 at x = 8 (one F fewer), end routes 2 and 5 at x = 8 (one F fewer), and adjust their `rsig` piece indices.
   - Run the harness until it reports 0 violations, changing nothing else.
   - If it won't reach 0, stop and report the violations.
   - Then regenerate:
     - `reference-LR-TL4.txt` (harness lines 8 to the line before "violations")
     - `tools/ottd/layouts/reference-LR-TL4.json` (`JUNCTION_JSON`)
     - `reference-LR-TL4-lefthand.json` (`JUNCTION_MIRROR=1`)

## Do not

- touch non-junction source files
- change public signatures
- remove `[!mayfail]`

## Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/stepB3_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is 179 cases; everything passes except the `[!mayfail]` outer test.

Commit message: `Change: Junction rule checker puts the first prio join on the merge tile`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Signing is off.

## Report

- commit
- red evidence
- suite totals
- the reference result and its new ASCII
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
