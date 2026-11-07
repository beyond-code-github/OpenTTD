# Brief 1b step B2: streamlined prio joins in the checker

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Decision:** D75 in `docs/junction-tool/DECISIONS.md`. Prio connections are now trailing 45° "joins" instead of 90° X+Y rungs.
- **Code:** step B's checker in `src/junction_rules.h/.cpp` and `src/tests/junction_rules.cpp`, plus the hidden design harness `src/tests/junction_design.cpp`. Run it with `JUNCTION_DESIGN=<file> build/openttd_test "[.design]"`; the `.design` format is read by that file.

## 2. Read first (once, with offset/limit)

- `src/junction_rules.cpp`:
  - the switch-event helper (`SwitchEventsOf`)
  - `ComputeBlocks`
  - `CheckPrio` and its helpers
  - `CheckSignals` (only where it uses switch events)
  - `CheckDeadlock`
- the G9 fixtures in `src/tests/junction_rules.cpp`
- `docs/junction-tool/layouts/reference-LR-TL4.design` (it uses rungs, and you will convert it)

## 3. The join (replaces the rung)

**Definition.** A join links an ML tile T on path p to the adjacent prio-row tile T' (across the prio-side edge):
- T holds exactly p's axis track plus half piece h1.
- T' holds exactly the axis track plus half piece h2.

h1 touches the prio-side edge and p's **exit** edge, so ML trains entering T can't reach it. h2 touches the ML-side edge and the edge facing p's **entry** direction.

| ML traffic | prio side | h1 (on T) | h2 (on T') |
|---|---|---|---|
| +x (`X_SW`; entry NE, exit SW) | −y (NW) | Left | Right |
| +x | +y (SE) | Lower | Upper |
| −x (`X_NE`; entry SW, exit NE) | −y | Upper | Lower |
| −x | +y | Right | Left |

Edges per track:
- Upper: NE + NW
- Lower: SE + SW
- Left: SW + NW
- Right: NE + SE

## 4. Changes (all in `src/junction_rules.cpp`, plus tests)

1. **Switch events:** a merge counts only if the other trackdir `t'` is on a track used by some path. Unused tracks (prio joins) never make merge events. Splits are unchanged: unused half pieces can't be reached.
2. **`CheckPrio` (G9):**
   - **Joins replace rungs everywhere:**
     - in clause 1 (shape): a prio row tile holds only the axis, or the axis plus h2 at a join
     - in clause 3 (downstream join)
     - in clause 4 (one join per ML block in range, and one per prio segment)
     - in clause 5 (prio signals between consecutive joins)
   - **X+Y rungs** are now a shape violation: "prio rung must be a streamlined join".
   - **Clause 2 (C touches nothing used) has one exception:** at a join, h1 meets p's track at T's exit edge and inside T. That contact is the join itself, so it's allowed.
   - **Components:** keep the rule that a tile's unused bits form one node. h1 is the unused bit on T.
3. **Fixtures.**
   - Convert every G9 fixture to joins, with the same pass/fail intent, and keep every clause covered.
   - Add failing fixtures:
     - a join whose h1 faces forward, so ML trains could take it (expect G9 shape, plus the trace effects you observe; assert G9)
     - an X+Y rung (expect "must be a streamlined join")
   - Add one pass fixture for each of the four table rows.
4. **Reference layout.** Convert `docs/junction-tool/layouts/reference-LR-TL4.design`:
   - Replace each `track x y X Y` prio-row rung and each ML-tile `track x y Y` with the join pieces from the table, at the same x.
     - The E prio is on −y of eastbound (+x) traffic: prio row gets Right, ML tile gets Left.
     - The W prio is on −y of westbound (−x) traffic: prio row gets Lower, ML tile gets Upper.
   - Prio-row tiles keep X.
   - Run the harness and get 0 violations. Change nothing else in the file.
   - If it can't reach 0 violations, stop and report the violations.
   - Then regenerate `docs/junction-tool/layouts/reference-LR-TL4.txt`: harness output lines 8 to the line before "violations".
   - Regenerate `tools/ottd/layouts/reference-LR-TL4.json`, and with `JUNCTION_MIRROR=1` regenerate `reference-LR-TL4-lefthand.json`, using `JUNCTION_JSON=<path>`.

## 5. Do not

- touch non-junction source files
- change public signatures in `junction_rules.h`
- remove `[!mayfail]` from the outer test

## 6. Stop conditions

- The table in §3 contradicts what the tracer or game tables say (e.g. h1 turns out to be reachable from p).
- The reference can't reach 0 violations with joins.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/stepB2_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is 177 cases; everything passes except the `[!mayfail]` outer test.

Commit message: `Change: Junction rule checker uses streamlined prio joins instead of rungs`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- references read
- commit
- red evidence for the new failing fixtures
- what changed
- suite totals
- the reference harness result, plus its new ASCII
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
