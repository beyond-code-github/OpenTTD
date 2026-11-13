# P4b: the level solve's descent uses the incremental exact cost (RECON-incremental-cost.md)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-p4b <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes (graph merge trees) and .claude/ng1 (tunnel tokens): never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- P4 (docs/junction-tool/RECON-incremental-cost.md, read it whole; patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p4.patch with a `P4_CHECK` constexpr assert against the full exact_cost; do NOT apply p4-profile.patch): incremental exact_cost (only the reach of changed seeds recomputed; L/U are max-propagation fixed points) is exact on 131,669 + 29,655 calls and the [identity] tag; descent 7.3x, level solve 3.2x; HOVERNEAR_LLRR mean 131 -> ~94 ms, max 1463 -> ~600 ms; both identity hashes unchanged.

## Changes
1. Apply p4.patch's src changes (resolve against HEAD). The full-recompute check stays available as a test, not as production code: keep the full exact_cost function (it is the reference), and make the check a `[hoverplan]` test "the incremental exact cost equals the full one" that runs the descent on 3 LL_RR fixtures and 3 T fixtures with the check enabled through a test-only switch (a function parameter defaulting to off, or the existing seam pattern in junction_levels; no global, no env var; say which).
2. No other change.

## Tests
- `tools/junction/jt identity build-rel`: both hashes unchanged (T 16256945156918441187 planned 2919; LL_RR 11818222337964832513 planned 1180); report its time.
- The new `[hoverplan]` test (red: with the incremental update deliberately skipping one corner, record that it fails; then restore).
- `build-rel/openttd_test "[realterrainsweep]"`: report HOVERPLAN/HOVERNEAR for T and LL_RR.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `tools/junction/jt gate build-rel` (count, time). TESTS.md rows.
- One commit: `Change: Junction level solve updates its terraform cost incrementally`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report with the numbers. When you have reported, stop.
