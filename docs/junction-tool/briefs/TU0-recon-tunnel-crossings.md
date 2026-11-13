# TU0 (recon): tunnels as crossings in the graph pipeline (D220)

Read-mostly recon: you may add temporary code in your worktree to measure; commit nothing; save any experiment as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/tu0.patch and leave the worktree clean. Keep your context small. Never use git stash. Never start a game client or touch the engineer's game or rig servers. Mark anything not read or measured as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-tu0 <HEAD>` (given at launch). Other agents work in /home/pete/OpenTTD/.claude/nodes and .claude/nodes2: never touch them. Use absolute paths.
- D220 (DECISIONS.md: read it, and D154, D155, D190, D203, D208, D209, D223): a crossing may be a tunnel as well as a bridge; tunnels have no span-height issue (IG3: the ground under a bridge must stay at or below its heads), which matters on rising ground and for wide hubs (LLL_RRR). The graph pipeline (src/junction_topology.cpp: structure bridges, CutJunctionTrackBridges, programs Over/Under tokens, placement bridge relations, render, signals; src/junction_levels.cpp bridge heads/span) knows only bridges. Layouts already carry tunnels (`JunctionBridge::tunnel`, D154) and the old router built them (D155: portals on inclined slopes, CmdBuildTunnel's rules; JunctionTunnelPrice).

## Questions
1. The game's tunnel rules from src/tunnelbridge_cmd.cpp CmdBuildTunnel: portal slopes, both portals at the same height, what the ground between must be (above the tunnel? any?), max length, what a tunnel may pass under (rail, roads, houses?), price; with file:line.
2. Where in the graph pipeline a crossing's kind would be chosen and what changes per stage if a crossing is a tunnel instead of a bridge: which edge goes under (the D223/D190 rule: the turning/branch leg under; for a tunnel the UNDER edge is the one in the tunnel), program tokens (Under -> a tunnel token?), placement relations (portal tiles at the ends: inclined slopes in the travel direction; length; no head-clearance or span relation), the level solve (portals: inclined tiles at the tunnel level; the ground between: at least one level above the tunnel? from Q1), the ramp rule (a portal is a climb/descent, IG4's profile), render (JunctionBridge with tunnel = true), signals (as bridges?).
3. A minimal first cut: e.g. a per-crossing choice "bridge or tunnel" fixed by a simple rule (tunnel when the bridge's span ground would be above its heads? or always for the branch under the main?) vs a cost choice in the level solve. Which is the plainest model that serves D220, and what it needs from the engineer (a decision card if so).
4. Size: the steps (one per stage) with their identity/red tests, in order.

## Output
docs/junction-tool/RECON-tunnel-crossings.md (untracked, ~70 lines) and the patch path if any.

## Report
10 lines: the game's rules, the per-stage changes, the minimal first cut, any decision for the engineer, and the step list. `git -C /home/pete/OpenTTD/.claude/ng1 status --porcelain` (clean). When you have reported, stop.
