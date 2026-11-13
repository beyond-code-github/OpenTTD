# TU5: a bridge whose span ground is too high is tried as a tunnel (D220; RECON-tunnel-crossings.md step TU5)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-tu5 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes (S-bend model in topology) and .claude/nodes2 (level solve recon): never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- TU1-TU4 merged (c84eea0b8d): a single (non-doubled) separation with `tunnel = true` is programmed (Tunnel/Above tokens), placed (portals as heads), rendered and signalled, and the level solve plans portals (raw slopes rising into the tunnel, one level L) and the ground over it (>= L+1); a game-map test builds it with CmdBuildTunnel. A tunnel is not always feasible (the T's tunnel is refused as a terraform conflict). The level solve fails a bridge whose span ground cannot stay at or below its heads with "the bridge from <a> to <b> cannot keep the ground of tile <t> at most <z> ..." (src/junction_levels.cpp ~:1455).
- TU0's minimal first cut (orchestrator-ruled): every separation starts as a bridge; when the hover plan fails because a SINGLE bridge's span ground is too high, that bridge flips to a tunnel and the plan is re-solved; if the tunnel plan also fails, the hover reports the bridge's original reason. Doubled (main-line) bridges never flip.

## Read first (once each)
CODINGSTYLE.md (C++ part); RECON-tunnel-crossings.md; src/junction_hover.{h,cpp} (PlanJunctionHover, PlanJunctionHoverAt, PlanJunctionHoverNear, the shared placement and the near reject); src/junction_topology.h JunctionGraphSeed and ProgramJunctionTrackGraph; how the level solve's failure reaches the hover (the placement's `why` / error text, JunctionPlacementReason if it names the bridge).

## Changes
1. The level solve's span failure identifies the bridge in a machine-readable way (the structure bridge index, through the placement's reason or a field the failure already carries: say which; no string parsing of the message).
2. In PlanJunctionHoverAt: when the plan fails with that span failure on a single bridge, build a seed copy with that bridge's `tunnel = true` and the programs re-made (ProgramJunctionTrackGraph), and plan once more at the same offset; up to one flip per single bridge per offset (several bridges may flip in turn, each at most once); if the retry fails, return the FIRST failure's reason. The seed copy is local (no cache in this step).
3. The near search's shared placement and reject (P2/P5b) are per seed: a flipped seed computes its own (say how; if the shared placement cannot be made for a flipped seed, the retry skips the shared path).
4. Nothing else.

## Tests (record each red)
1. `[hoverplan]` "a bridge over high ground becomes a tunnel": a hand-made snapshot where a single bridge's span ground is one level above its heads and a tunnel is feasible (use TU4's feasible geometry as the starting point): PlanJunctionHover plans with a `tunnel = true` bridge in the layout, T3 accepts, CheckJunctionRules clean. Red: today it fails with the span reason.
2. Outer: `[realterrainsweep]` HOVERPLAN/HOVERNEAR (T and LL_RR): planned counts may only rise (a drop is a STOP); report every fixture that now plans with a tunnel and its time; `REQUIRE(near_max < 100)` for the T still holds (report if not). `tools/junction/jt identity build-rel`: the LEVELHASH tests call PlanJunctionHover: if a hash moves, list the offsets that changed and confirm each changed only by a flip to a tunnel, then re-pin with that evidence.
3. `[junctiongame]`: a fixture that now plans with a tunnel (if any): its tunnel builds with the game's command after the terraform (as TU4's test does).
4. TESTS.md rows.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `tools/junction/jt fast build '[junctiongame]' 10`; `tools/junction/jt identity build-rel`; `build-rel/openttd_test "[realterrainsweep]"`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Feature: Junction hover tries a tunnel where a bridge's span ground is too high`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus counts before/after, fixtures that gained a tunnel plan, and timing. When you have reported, stop.
