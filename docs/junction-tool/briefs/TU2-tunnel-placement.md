# TU2: tunnel separations are placed (portals as the separation's heads) (D220; RECON-tunnel-crossings.md step TU2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-tu2 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes (graph merge trees) and .claude/nodes2 (junction_levels.cpp): never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- TU1 (968b7b3fc2): `JunctionTrackBridge::tunnel`; programs emit `Tunnel` on the separated edge (where it would have Over) and `Above` on the single crossed edge (where it would have Under; the S-bend before it is kept); PlaceTrackGraph refuses at the top ("tunnel crossings are not placed yet"), the span loop and RenderJunctionTrackPlacement refuse too. TU0 (docs/junction-tool/RECON-tunnel-crossings.md, read it whole): portals reuse the Heads element; the length cap is the game's `max_tunnel_length` (default 64) instead of max_bridge_middle; no span-height relation exists in placement; a portal needs an axis run before it (no D110 "bridge right after a bend drops the closing turn" for tunnels); portals' height needs are the level solve's (TU4), not placement's.

## Read first (once each)
CODINGSTYLE.md (C++ part); the RECON; TU1's commit (`git -C /home/pete/OpenTTD/.claude/ng1 show 968b7b3fc2 --stat` and its placement parts); src/junction_topology.cpp: how Over tokens become Heads elements and their relations (head spacing, middle length cap, clearance families, D110), how Under tokens become crossed-edge elements; where max_bridge_middle comes from in the spec (JunctionSpec) and whether the spec carries max_tunnel_length (it does for the old router: D155; find it).

## Changes (src/junction_topology.cpp placement only)
1. Remove the top-of-placement refusal and the span-loop refusal; a `Tunnel` token builds the same element a bridge's Over builds (the two ends = the portals; the run between = the tunnel's middle), with: the length cap from the spec's tunnel length (not max_bridge_middle); no D110 closing-turn drop (an axis run of at least one tile before each portal, as a floor-table entry or the existing relation that D110 relaxes: say which); the relations that exist only because a bridge deck is above ground (bridge middle clearance family, "bridge heads hug crossers" if it is about the deck: read and say which apply) do not apply to a tunnel; everything else as for a single bridge.
2. An `Above` token builds what an Under builds (the crossed edge crosses over the tunnel's middle at grade).
3. Render keeps refusing (TU3).

## Tests (record each red)
1. Outer identity: `tools/junction/jt identity build-rel`; the sweep tags' outputs equal to launch HEAD's (times stripped); LL_RR circuit byte-identical (no seed has a tunnel).
2. `[topology]` "topology: a tunnel separation is placed": TU1's test setup (T seed, its single bridge set to tunnel): the placement succeeds; the separated edge has two portal points on its row with an axis run before each; the tunnel length (portal to portal) is within the spec's tunnel cap; the crossed edge crosses strictly between the portals; Render then fails with "tunnel crossings are not placed yet" (unchanged). Red: today's placement refusal.
3. If a relation had to be dropped or changed for tunnels, a second test shows its absence matters (a case a bridge refuses and a tunnel places, or say none exists).

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time). TESTS.md row.
- One commit: `Add: Junction tunnel separations are placed, portals as their heads`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus which bridge relations apply to tunnels and which do not (with why), and the cap's source. When you have reported, stop.
