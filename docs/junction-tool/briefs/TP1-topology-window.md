# TP1: the topology choices in their own window; a presets dropdown in the junction window (D222)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-tp1 <HEAD>` (given at launch). Never use git stash. Another agent may work in /home/pete/OpenTTD/.claude/nodes: never touch it. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- D222 (DECISIONS.md, read it whole, with its two refinements): the junction window keeps one compact row for the topology: a presets dropdown plus a "Topology..." button opening a separate Topology window with the individual choices; presets only set those choices ("Custom" when none matches); lanes and gaps by steppers, no typing. D221: lane pairing (any / keep / per direction).
- Today (src/junction_gui.cpp): row 1 = Mainline label + edit box + Branch label + edit box (parsed with ParseLineNotation, LR1); the Rotate/Mirror row also holds WID_BJ_LANES (LP1) cycling the pairing; client settings hold the notations (find their names: the window's edit boxes are initialised from settings) and `junction_lane_pairing`.

## Read first (once each)
CODINGSTYLE.md (both parts); D221, D222; src/junction_gui.cpp (widget tree, the notation boxes and their settings, Regenerate/key, WID_BJ_LANES); src/widgets/junction_widget.h; an existing small OpenTTD window with steppers and a dropdown to copy the pattern (e.g. the town generation window or the game options: say which you followed); src/junction_line.h JunctionLineSpec/ParseLineNotation.

## Changes
1. **Topology window** (new window class in src/junction_gui.cpp, its widgets in src/widgets/junction_widget.h, WindowClass as the game's pattern requires: reuse an existing junction class id only if the game has a slot; otherwise add `WindowClass::JunctionTopology` and say so): rows
   - Main line: "Left lanes" stepper (1..3), "Right lanes" stepper (1..3), "Gap" stepper (1..3).
   - Branch: the same three.
   - Lane pairing: a dropdown (Any / Keep / Per direction), disabled when no turning movement is multi-to-multi (LP1's rule).
   - A one-line summary label showing the notations as text (e.g. "LL_RR × L_R").
   Changes apply at once: they write the existing notation settings (as the notation strings ParseLineNotation reads) and `junction_lane_pairing`, and tell the junction window to regenerate.
2. **Junction window row 1** becomes: a presets dropdown (WWT_DROPDOWN) + a "Topology..." push button that opens (or focuses) the Topology window. Presets (a file-local constant table: name string, main L/gap/R, branch L/gap/R, pairing): "T" (L_R × L_R), "LL_RR + sideline" (LL_RR × L_R), "LL_RR × LL_RR, any lanes", "LL_RR × LL_RR, keep lanes", "LL_RR × LL_RR, per direction". Choosing one writes the same settings as the Topology window and refreshes it if open; the dropdown shows the matching preset or "Custom". The notation edit boxes and their labels are removed from the junction window; WID_BJ_LANES is removed (the Topology window has the pairing).
3. Strings in english.txt for the new widgets (captions, labels, tooltips, preset names, "Custom"); remove the strings of removed widgets if nothing else uses them.
4. The junction window's height must not grow: report the row count before/after.

## Tests
- No unit test can open windows. If the preset table and the "which preset matches" function are free functions, a `[junction]` unit test that every preset round-trips (settings -> match -> same preset) and that a non-preset choice is "Custom"; otherwise none and say so.
- Manual checklist for the engineer (you cannot run the game): open the junction window (no crash), pick each preset, open Topology..., change steppers and pairing, see the summary and the junction window's dropdown switch to Custom, hover still plans for T and LL_RR + sideline, and LL_RR × LL_RR shows the pipeline's refusal reason in the tooltip.

## Gates
- The `openttd` target builds Debug and Release with no new warnings; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Feature: Junction topology choices in their own window, with a presets dropdown`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the widget rows before/after, the window pattern followed, and the manual checklist. When you have reported, stop.
