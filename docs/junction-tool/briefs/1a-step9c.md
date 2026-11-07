# Brief: 1a step 9c, size the cost line for large amounts

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD` (main checkout), branch `feature/junction-tool`, head `{HEAD}`. You are the only agent here. A copy of the game (`build/openttd-step9b`) may be running. Don't touch it.
- **Engineer's report:** "The window is too small for the full price to be visible". The `WID_BJ_COST` label is sized from its empty initial text.
- **House pattern (D48):** reserve width for the largest value in `UpdateWidgetSize`. See `company_gui.cpp:416-421` (`GetParamMaxValue`) and `group_gui.cpp:478-483` (`GetParamMaxDigits`).
- Baseline: `{BASELINE}`, all passing.

## 2. Read first, once each

- `CODINGSTYLE.md` (you may skip quoting a rule this time).
- `src/junction_gui.cpp`: `BuildJunctionWindow`, including whether it already overrides `UpdateWidgetSize`.
- `src/company_gui.cpp` lines 405–425 and `src/group_gui.cpp` lines 470–485.
- `src/lang/english.txt`: the `STR_JUNCTION_*` block.

## 3. Tests

There's no unit-test seam, so there are **no new tests**. The suite must stay at `{BASELINE}`.

## 4. Changes (one commit)

1. **In `BuildJunctionWindow`,** add an `UpdateWidgetSize` override, or extend the existing one, matching the base signature used by `BuildRailDepotWindow`. For `WID_BJ_COST`:
   ```cpp
   size.width = std::max(size.width, std::max(
   	GetStringBoundingBox(GetString(STR_JUNCTION_COST, GetParamMaxDigits(15))).width,
   	GetStringBoundingBox(GetString(STR_JUNCTION_COST_UNAFFORDABLE, GetParamMaxDigits(15))).width) + padding.width);
   ```
   Add the comment `/* Reserve room for late-game amounts of up to 15 digits. */`. If `UpdateWidgetSize` already exists, add only this case and leave other widgets untouched.
2. **In `english.txt`,** realign the colons of the `STR_JUNCTION_STATUS_*` lines (and any other `STR_JUNCTION_*` line that's off) to the same column as the rest of the junction block. Whitespace only.

Commit message: `Fix: Make room for large amounts in the Junction window cost line`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Do not

- Change any behaviour other than the label's minimum width.
- Change any string text, only alignment.

## 6. Stop conditions

- `GetParamMaxDigits`, `GetStringBoundingBox` or the `UpdateWidgetSize` signature differ from the precedents.
- Anything not covered here.

## 7. Gates

1. Build `openttd` and `openttd_test` with no errors, no warnings, and no strgen warnings.
2. The full suite is `{BASELINE}`, all passing.
3. One commit touching only `src/junction_gui.cpp` and `src/lang/english.txt`.

Logs go to `.../s9c_*.log`.

## 8. Report format

- **References read**
- **Commits**
- **Red evidence:** "none: no unit-test seam (named in brief)"
- **Types and members added or removed**
- **Suite results**
- **Manual checks for the engineer:** the full cost is visible in a late-game save, and the window isn't absurdly wide
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
