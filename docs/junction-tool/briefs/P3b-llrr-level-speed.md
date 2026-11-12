# P3b: P3's level-solve speed-ups and the steep-slope abort guard (RECON-llrr-time.md)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-p3b <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes and .claude/nodes2: never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- P3 (docs/junction-tool/RECON-llrr-time.md, read it whole; patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p3.patch on af3a03a004; profile harness p3-profile-harness.patch, not to be committed): LL_RR hover time is the level solve's coordinate descent, 80% in exact_cost's O(n^2) raised-vs-lowered corner pair loop, and exact_cost recomputed per group. Three identity-preserving speed-ups (pair loop over two lists with a max/min prefilter; cache the held levels' cost until a group moves; lazy formatting of blocked-tile/height-step notes) took HOVERPLAN_LLRR max 208 -> 28 ms and HOVERNEAR_LLRR mean 470 -> 135 ms, with the LL_RR hash over 7344 offsets 5813095187505070117 unchanged and the T LEVELHASH unchanged. It also found an ABORT reachable from the hover: on arctic-hilly-4 at offset (137,109) exact_cost builds a foundation tile with an impossible steep slope (22) and GetRailFoundation/GetBridgeHeadGround reach GetHighestSlopeCorner's NOT_REACHED; the patch counts it as a violation.

## Changes
1. Apply p3.patch's src changes (resolve against HEAD: RW6b, QL2, CR1 and others merged since af3a03a004; keep the profile harness OUT).
2. The steep-slope guard: before any call that needs a valid slope (GetRailFoundation, GetBridgeHeadGround) on a corner set the clamp produced, check the slope is valid (the game's IsValidSlope or the corner-difference rule: say which) and treat an invalid one as a violation, as the patch does. In the report, explain WHY the clamp can produce an invalid tile there (corners of one tile differing by 2 while each corner pair across tiles is within 1? a foundation tile's corners left free?), from the code and that offset; if the explanation shows the clamp itself is wrong, stop and report rather than guard.
3. Tests: `[hoverplan]` "the level solve never asks the game about an impossible slope": arctic-hilly-4 at that offset (or the hand-made corner set it reduces to): PlanJunctionHover returns (plan or reason), no abort. Red: it aborts at HEAD (record the signal). `[realterrainsweep]`: an LL_RR identity hash test like LEVELHASH (`[.identity]` tag as GT1 did) pinning 5813095187505070117 (re-measure at HEAD with only the guard applied: offsets that aborted before change by design; pin what HEAD+guard gives, then show the speed-ups keep it).
4. TESTS.md rows.

## Stop conditions
- Any hash differs between HEAD+guard and HEAD+guard+speed-ups; T LEVELHASH moves; the clamp explanation shows a clamp bug; anything not covered.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `tools/junction/jt identity build-rel` (with the new LL_RR hash; report the time against its 60 s budget: if over, say by how much and stop before raising anything); `build-rel/openttd_test "[realterrainsweep]"` (all lines; report HOVERPLAN/HOVERNEAR for T and LL_RR); `tools/junction/jt gate build-rel` (count, time; note any T near-max failure under load and rerun once standalone).
- One commit: `Change: Junction level solve faster on big hubs; an impossible slope is a violation, not an abort`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the clamp explanation and the numbers. When you have reported, stop.
