# Brief: 1a step 9e, Shift-click shows the estimate instead of building

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD` (main checkout), branch `feature/junction-tool`, head `{HEAD}`. You are the only agent here. A copy of the game (`build/openttd-step9d`) may be running. Don't touch it.
- **Decision D51** in `docs/junction-tool/DECISIONS.md`.
  - **The bug:** with Shift held, `Command<>::Post` runs estimate-only. `InternalPostResult` (`command.cpp:230-245`) shows `ShowEstimatedCostOrIncome`, `Post` returns true, and the callback is never called. So the junction sequencer waits forever at "Building... 0 of 12".
  - **The ruling, following house style:** Shift-click on the junction tool shows the estimated cost of the **whole junction** and never starts building.
- Baseline: `{BASELINE}`, all passing.

## 2. Read first, once each

- `src/junction_gui.cpp`: `JunctionToolClick` and `BuildJunctionWindow::StartBuild`.
- `src/command.cpp` lines 225–250, `src/gui.h` line 47 (`ShowEstimatedCostOrIncome(Money cost, int x, int y)`) and `src/gfx_func.h` line 88 (`IsShiftPressed`).
- `src/misc_gui.cpp` lines 485–500 (`ShowEstimatedCostOrIncome`).

## 3. Tests

There's no unit-test seam (it's an input and GUI path), so there are **no new tests.** The suite must stay at `{BASELINE}`.

## 4. Changes (one commit, `src/junction_gui.cpp` only)

In `BuildJunctionWindow::StartBuild(TileIndex tile)`, after `ComputePlacement` has produced the estimate and **before** the affordability check:

```cpp
/* Shift-click estimates the cost instead of building, as with every other build tool. */
if (IsShiftPressed()) {
	if (estimate.bad_tiles.empty()) {
		ShowEstimatedCostOrIncome(estimate.total, TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE);
	} else {
		ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK), TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, CommandCost(estimate.first_error));
	}
	return;
}
```

Use the `ShowErrorMessage` overload that `command.cpp:238` uses, with the same argument order. If no overload accepts these arguments, stop and report. Include `gui.h` and `error.h` (or whichever header declares `ShowErrorMessage` in this tree) if they aren't already included.

Commit message: `Fix: Shift-click on the junction tool estimates the cost instead of hanging`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Do not

- Change the sequencer, the poster, or anything else.
- Touch other files.

## 6. Stop conditions

- `IsShiftPressed`, `ShowEstimatedCostOrIncome` or `ShowErrorMessage` differ from what's described.
- Anything not covered here.

## 7. Gates

1. Build `openttd` and `openttd_test` with no errors and no warnings.
2. The full suite is `{BASELINE}`, all passing.
3. One commit touching only `src/junction_gui.cpp`.

Logs go to `.../s9e_*.log`.

## 8. Report format

- **References read**
- **Commits**
- **Red evidence:** "none: no unit-test seam (named in brief)"
- **Types and members added or removed**
- **Suite results**
- **Manual checks for the engineer:**
  - Shift-click over valid land shows "Estimated cost: £X", where X equals the window's cost line, and builds nothing
  - Shift-click over invalid land shows the reason
  - a normal click still builds
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -1 --format='%h %s'`

When you have reported, stop.
