# GT1: the wide gate back under budget: identity sweeps get their own tag

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-gt1 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes: never touch it. Use absolute paths.
- `tools/junction/jt gate build-rel` runs `[junction]~[slow]~[mapexport]~[.]` with a 120 s budget; it now takes ~108-111 s. Three characterisation sweeps added today dominate: ES1's "every placement edit fails with a reason or is checker-clean" (`[topologysweep]`, ~25 s), LS1's "level solve: identical plans at every near offset" with P2's shared-placement identity folded in (`[realterrainsweep]`, ~17 s), and F1's HOVERNEAR pass inside "hover plan on real terrain" (~14 s with HOVERPLAN). Every step that touches placement or levels already runs the five sweep tags by name for its identity diff.

## Read first
docs/junction-tool/TESTS.md (budgets and the gate row); tools/junction/jt; the three test cases named above (headers only).

## Changes
1. Give ES1's edit sweep and LS1's LEVELHASH test the hidden tag `[.identity]` in addition to their current tags (Catch2: a tag starting with `.` hides the test from the default and from the gate's `~[.]`), so the gate skips them and `build-rel/openttd_test "[identity]"` runs both. Do not hide the HOVERPLAN/HOVERNEAR test (it carries the 100 ms requirement).
2. `tools/junction/jt`: a mode `jt identity <build-dir>` that runs `"[identity]"` in Release with a 60 s budget, in the same style as `gate`.
3. TESTS.md: a row for `[identity]` (what it holds, its time, when to run it: every step that touches src/junction_topology.cpp or src/junction_levels.cpp, alongside the gate), and the gate row's new time.
4. Check the tag still selects the tests by their old tags too (`"[topologysweep]"` and `"[realterrainsweep]"` still run them when named: Catch2 hidden tests run when a tag of theirs is named explicitly; verify and report).

## Gates
- `tools/junction/jt gate build-rel` (count and time: must be under 90 s); `tools/junction/jt identity build-rel` (time; all pass).
- One commit: `Codechange: Junction identity sweeps run by their own tag, out of the wide gate`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus gate and identity times. When you have reported, stop.
