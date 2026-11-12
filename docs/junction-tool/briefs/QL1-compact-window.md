# QL1: a compact junction window that remembers its train and prio (engineer, 2026-11-13)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-ql1 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes (src/junction_levels.cpp): never touch it. Never start a game client or touch the engineer's game or rig servers.
- Engineer: "the junction window is very big right now which makes the tool hard to use in practice. Also it doesn't remember the train or the prio selection between openings." His screenshot: the window spans ~600 px at his scale; rows: name label, Mainline, Branch, train length, Pick train, train info, prio label, prio slider, cost, Projection toggle, Rotate/Mirror, status, Retry/Cancel. Earlier ruling: projection is the way ("Isn't that just the way we do things now?"), so the toggle goes (the old path's code is removed in a later step, not here).
- Layout: src/junction_gui.cpp `_nested_build_junction_widgets` (~:1383-1420), UpdateWidgetSize (~:1180-1195: the train label reserves `std::string(20, 'W')`), the window's state members (~:400-440: picked_train, the prio slider value), statics like `_junction_rotation`/`_junction_mirrored` (~:341) as the pattern for remembered state.

## Read first (once each)
CODINGSTYLE.md (both parts: C++ and GUI/widgets); src/junction_gui.cpp the widget tree, UpdateWidgetSize, OnClick for each widget, the projection toggle's state and readers, how the prio slider value and picked_train are set and read, OnInit/constructor; src/widgets/ for the WID_BJ_* enum.

## Changes
1. **Projection toggle removed:** the WID_BJ_PROJECTION widget, its string and tooltip go; the window always runs the projection path (the state that the toggle flipped is fixed to on; the old path's code stays for now, unreachable from the window: say in the report which functions became unreachable).
2. **Compact layout (one widget tree):**
   - Drop the WID_BJ_NAME row (it repeats the notations).
   - Mainline and Branch on one row: label, edit box, label, edit box (edit boxes fill and resize equally).
   - Train length arrows + label and the Pick train button on one row.
   - Train info and prio label on one row (two labels, each filling half), the slider below.
   - Cost and status stay as two label rows.
   - Rotate/Mirror on one row as today.
   - Retry/Cancel inside an NWID_SELECTION shown only while a build is pinned (the state that enables them today); otherwise the plane is empty (SZSP_HORIZONTAL).
   - The train label's width reserve: a fixed `std::string(12, 'W')`-equivalent replaced by the actual widest of the current string and a 12-character default; remove the 20-W reserve.
3. **Remembered between openings (game session, not saved):** file statics `_junction_picked_train` (std::optional of the picked train's identity: vehicle index plus what the window keeps) and `_junction_prio_tiles` (the slider's chosen value), written whenever the window sets them and read in the constructor. On restore the train is used only if the vehicle still exists, is a front engine train, and belongs to the local company; otherwise cleared. Cleared on a new game/load the same way the existing statics are (find how `_junction_rotation` is reset; if it is never reset, do the same).
4. lang/english.txt: remove the toggle's strings; no new strings unless a label needs a shorter text (say which).

## Tests
- No unit test can draw the window. Add a `[game]` test "the junction window state survives reopening" only if the remembered state is reachable without a window (a free function that validates a remembered train): otherwise none, and say so.
- Manual check (for the engineer; you cannot run the game): list what to look at.

## Gates
- The `openttd` target builds Debug and Release with no new warnings; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Change: Junction window is compact, drops the projection toggle and remembers its train and prio`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the widget tree before/after (row list), the unreachable old-path functions, and the manual checklist. When you have reported, stop.
