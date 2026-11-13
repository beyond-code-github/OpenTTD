# TU1: a crossing can be a tunnel in the structure and programs (D220; RECON-tunnel-crossings.md step TU1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-tu1 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes (OrderJunctionTrackGraph) and .claude/nodes2 (junction_levels.cpp): never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- TU0 (docs/junction-tool/RECON-tunnel-crossings.md, read it whole). Orchestrator's model for this step (differs from TU0's "carrier flips" wording; simpler): a `JunctionTrackBridge` describes one grade separation of edge `edge` against `crossed`; `tunnel = false`: `edge` goes OVER them on a bridge (today); `tunnel = true`: the same `edge` goes UNDER them in a tunnel. Flipping a crossing between bridge and tunnel never changes which edge is the separated one. Doubled (main-line) separations stay bridges in this series (TU6 later); a tunnel is only ever set on a single (non-doubled) separation.
- Programs today: the bridge's edge gets Over tokens (one per bridge, HeadsPair etc.), crossed edges get Under tokens (DESIGN-G1 §10).

## Read first (once each)
CODINGSTYLE.md (C++ part); RECON-tunnel-crossings.md; DESIGN-G1 §9-§10; src/junction_topology.h JunctionTrackBridge, the program token kinds (Over/Under), JunctionTrackStructure; src/junction_topology.cpp: where bridges are made (structure), where Over/Under tokens are emitted (programs), and every reader of Over/Under tokens (grep: placement, render, signals, events).

## Changes
1. `JunctionTrackBridge` gains `bool tunnel = false; ///< The edge passes under the crossed edges in a tunnel instead of over them on a bridge (D220); only on a single (not doubled) separation.` (equality includes it).
2. Program token kinds gain `Tunnel` (the separated edge's token where it has Over today) and `Above` (a crossed edge's token where it has Under today), each with a doc comment; emitted only when the separation's `tunnel` is true. Every reader of Over/Under that would meet a Tunnel/Above token: placement, render, signals, route events: for now FAIL with a reason "tunnel crossings are not placed yet" (placement) rather than mis-handling them; say each site.
3. Nothing sets `tunnel = true` yet (TU5 will). No change to any existing behaviour.

## Tests (record each red)
1. Outer identity: `tools/junction/jt identity build-rel` and the five sweep tags' build-rel outputs equal to launch HEAD's (times stripped); LL_RR circuit byte-identical.
2. `[topology]` "topology: a tunnel separation gives tunnel tokens": take the T seed's structure, set `tunnel = true` on its single (non-doubled) bridge, re-run programs: the separated edge has a Tunnel token where it had Over, each crossed edge an Above token where it had Under; PlaceJunctionTrackGraph then fails with "tunnel crossings are not placed yet". Red: the member does not exist (compile) is not a red; record a characterisation.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time). TESTS.md row.
- One commit: `Add: Junction crossings can be tunnels in the structure and programs (not yet placed)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the reader sites that now refuse tunnels. When you have reported, stop.
