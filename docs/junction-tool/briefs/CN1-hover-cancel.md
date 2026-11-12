# CN1: a superseded hover search stops (LL_RR near searches run for seconds on hills; P3/LR1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-cn1 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes2 and .claude/ng1: never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- The window detaches a running job when the cursor moves to another spot (src/junction_gui.cpp StartJob ~:866, DetachJob ~:667 sets `job->cancel`), but the projection worker (StartProjectionJob ~:922) calls `PlanJunctionHoverNear(...)` which never reads `cancel`: a superseded LL_RR near search keeps running for seconds (LR1: up to 9.6 s), so detached threads pile up and load the CPU while the engineer moves the mouse.

## Changes
1. `PlanJunctionHoverNear` (src/junction_hover.{h,cpp}) gains a last parameter `const std::atomic<bool> *cancel = nullptr` (doc comment): checked before each offset; when set, it returns at once with `why = "cancelled"` and no layout.
2. StartProjectionJob passes `&job->cancel`; when the plan comes back cancelled the worker sets done without making terraform steps (the result is ignored anyway).
3. Nothing else.

## Tests
- `[hoverplan]` "a cancelled near search stops at once": PlanJunctionHoverNear on a fixture that fails at its offset (so the search runs), with the flag already set: returns no layout, why "cancelled", and takes under 1 ms (Release; in Debug just the result). Red: the parameter does not exist (compile) is not a red; instead record that with the flag ignored (temporarily) the call takes the full search time.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `tools/junction/jt gate build-rel` (count, time); the `openttd` target builds Debug and Release.
- One commit: `Change: Junction hover search stops when the cursor moves on`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report. When you have reported, stop.

## Amendment 1 (engineer: "The tooltip showing the estimate vs actual was replaced instantly by feedback about the ground under the cursor")
4. The "spent vs quoted" result moves out of the hover tooltip: when a build finishes (where QL2 sets `show_spent = true`, src/junction_gui.cpp ~:1360), show it with the game's information message (`ShowErrorMessage(GetEncodedString(STR_JUNCTION_STATUS_SPENT, spent, quote), {}, WarningLevel::Info, x, y)` at the junction's screen position, as command.cpp ~:214 shows an info message at a tile; read error_gui.cpp ~:280-300 for the signature), which stays on screen for its usual time whatever the cursor does. Remove `show_spent` and its uses (the tooltip goes back to cost/status only). Keep the Debug line. Add to the manual checklist: after a build, an information message with spent and quoted appears and stays while the cursor moves.
