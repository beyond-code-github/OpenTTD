# TU3: tunnel separations render and signal (D220; RECON-tunnel-crossings.md step TU3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-tu3 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes (order sweep) and .claude/nodes2 (placement facts recon): never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- TU2 (1221dce6e3): a `Tunnel` token builds the single separation's Heads element (portals at its ends, middle within spec.max_tunnel_length); `Above` reads as Under; RenderJunctionTrackPlacement still refuses "tunnel crossings are not placed yet". TU0 (docs/junction-tool/RECON-tunnel-crossings.md): render emits `T<n>` instead of `B<n>` in the route notation; RasterizeJunctionRoute already builds tunnel JunctionBridges (`tunnel = true`) from 'T' (D154/D155); signals unchanged (D154, INFERRED); D110 (a bridge right after a bend drops the closing turn) must NOT apply to tunnels (a portal needs an axis run before it).

## Read first (once each)
CODINGSTYLE.md (C++ part); the RECON; src/junction_topology.cpp RenderJunctionTrackPlacement (the Over/Under handling, D110, the refusal TU1 added), SignalJunctionTrackRoutes (how bridges enter signal placement), JunctionRouteEvents; src/junction_layout.h JunctionBridge (`tunnel`); RasterizeJunctionRoute ('T'); src/junction_rules.cpp: how the checker treats tunnels (CheckJunctionRules: bridges, crossings, tunnel lengths).

## Changes
1. Render: a Tunnel token emits the tunnel piece the rasteriser builds as `JunctionBridge{..., tunnel = true}`; D110's closing-turn drop is skipped for tunnels; Above renders as Under does. The render refusal goes.
2. Signals: whatever bridges need for signal placement applies to tunnels unchanged; if SignalJunctionTrackRoutes or JunctionRouteEvents distinguishes bridges in a way a tunnel breaks, report it (do not special-case without saying).
3. Checker: if CheckJunctionRules rejects a correct tunnel layout (e.g. a rule that assumes a crossing is a bridge), report the rule and stop: the checker is the oracle (D193); changing it is not this step's call.

## Tests (record each red)
1. Outer identity: `tools/junction/jt identity build-rel`; the sweep tags' outputs equal to launch HEAD's (times stripped); LL_RR circuit byte-identical.
2. `[topology]` "topology: a tunnel separation renders and signals": TU2's setup (T seed, its single bridge as tunnel): render succeeds, the layout has exactly one JunctionBridge with `tunnel = true` whose ends are the placement's portals and no bridge for that separation, signals succeed, CheckJunctionRules is clean (report every violation if not, and stop). Red: today's render refusal.
3. TESTS.md row.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Add: Junction tunnel separations render and signal`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the ASCII render of the T with the tunnel (as the existing render tests print), and any checker finding. When you have reported, stop.
