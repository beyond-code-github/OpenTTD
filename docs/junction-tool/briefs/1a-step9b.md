# Brief: 1a step 9b, cost line in the Junction window

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD` (main checkout), branch `feature/junction-tool`, head `{HEAD}`. You are the only agent here. A copy of the game (`build/openttd-step9`) may be running. Don't touch it.
- Decisions D48 and D49 in `docs/junction-tool/DECISIONS.md`:
  - add a persistent cost line to the Junction window, following the Fund New Industry window's cost line
  - leave the cursor tooltip unchanged
- Baseline: `{BASELINE}`, all passing.

## 2. Read first, once each

- `CODINGSTYLE.md`, in full. Quote a rule not quoted before; these have been used: the `this->` rule, "Use /* */ for single line comments.", and "Do not put external declarations in implementation (i.e. cpp) files."
- `src/junction_gui.cpp`: `BuildJunctionWindow` (its members, `GetWidgetString`, `ComputePlacement`, `OnRealtimeTick`, the nested widgets). Also `src/widgets/junction_widget.h`.
- `src/industry_gui.cpp` around line 558, and `src/lang/english.txt` line 3190 (`STR_FUND_INDUSTRY_INDUSTRY_BUILD_COST`), for the precedent.

## 3. Tests

The window has no unit-test seam, so there are **no new tests**. The suite must stay at `{BASELINE}`, all passing.

## 4. Changes (one commit)

1. **`widgets/junction_widget.h`:** add `WID_BJ_COST, ///< Cost of building the junction at the cursor.` after `WID_BJ_NAME`.
2. **`english.txt`,** in the junction block, aligned:
   - `STR_JUNCTION_COST :{BLACK}Cost: {YELLOW}{CURRENCY_LONG}`
   - `STR_JUNCTION_COST_UNAFFORDABLE :{BLACK}Cost: {RED}{CURRENCY_LONG}`
   - `STR_JUNCTION_COST_INVALID :{BLACK}Cost: {RED}{STRING}`
3. **Nested widgets:** a `WWT_LABEL WID_BJ_COST` with `SetFill(1, 0)`, right below `WID_BJ_NAME`.
4. **`BuildJunctionWindow`** gets these members, documented:
   - `Money placement_cost = 0;`
   - `bool placement_affordable = false;`
   - `StringID placement_error = INVALID_STRING_ID; ///< First error if the placement is invalid, else INVALID_STRING_ID.`
   - `bool has_placement = false; ///< The cursor is over the map and the fields above are set.`
5. **In the hover code,** right where the tooltip text is chosen:
   - set `has_placement = true` and `placement_cost = estimate.total`
   - set `placement_affordable = estimate.affordable`
   - set `placement_error = estimate.bad_tiles.empty() ? INVALID_STRING_ID : estimate.first_error`
   - then call `this->SetWidgetDirty(WID_BJ_COST)`

   Where the hover code clears the ghost because the cursor is off the map, set `has_placement = false` and mark the widget dirty.
6. **`GetWidgetString(WID_BJ_COST)`:**
   - if `!has_placement`: `GetString(STR_EMPTY)`
   - otherwise, if `placement_error != INVALID_STRING_ID`: `GetString(STR_JUNCTION_COST_INVALID, placement_error)`
   - otherwise, if `!placement_affordable`: `STR_JUNCTION_COST_UNAFFORDABLE` with `placement_cost`
   - otherwise: `STR_JUNCTION_COST` with `placement_cost`

   Follow the existing `GetWidgetString` pattern in this window.
7. **While building or failed,** the hover code is skipped, so the line keeps showing the cost of the build in progress. That's intended, so don't change it.

Commit message: `Add: Show the junction cost in the Junction window`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Do not

- Change the cursor tooltip or any other behaviour.
- Touch upstream files other than `english.txt`.

## 6. Stop conditions

- The label needs a fixed width that would differ from house style. Report it rather than guessing.
- Anything not covered here.

## 7. Gates

Logs go to `.../s9b_*.log`.

1. Build `openttd` and `openttd_test` with no errors, no warnings, and no strgen warnings.
2. The full suite is `{BASELINE}`, all passing.
3. One commit, touching only `src/junction_gui.cpp`, `src/widgets/junction_widget.h` and `src/lang/english.txt`.

## 8. Report format

- **References read,** with the new rule quoted
- **Commits**
- **Red evidence:** "none: no unit-test seam (named in brief)"
- **Types and members added or removed**
- **Suite results**
- **Manual checks for the engineer:**
  - the cost line shows and updates with the cursor
  - it's yellow when affordable, red when unaffordable
  - it shows the reason when invalid
  - it's empty off the map
  - it matches the money actually spent
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
