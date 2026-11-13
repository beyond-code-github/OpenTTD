# Junction tool (src/junction/)

Rules of the junction tool as present-tense facts, split per folder: each fact lives in the folder whose code enforces it.

## Scope and folders
- The tool places generated rail junctions (hubs) from the rail toolbar, prices them exactly, and builds them with ordinary commands.
- It is core C++ in a fork; new code lives under `src/junction/` and edits to upstream files are minimal seams, each in its own `Codechange:` commit.
- The fork never changes the savegame format: no new settings, map bits, pool objects, chunks or version bump; the tool's own state lives in client settings (`openttd.cfg`) only.
- The fork links HiGHS (MIT), found by CMake's `find_package(highs)` and listed in `vcpkg.json`; only `JunctionMilp` includes it.
- `terrain/`: terrain snapshot, terrain facts, terraform, foundation and slope rules.
- `topology/`: topology, graph, order, structure, programs, curve gap constraints.
- `placement/`: horizontal placement, render, positions solver, level solve.
- `signals/`: signal resolution, drag signals, prio spans.
- `tool/`: hover planning, the window, the ghost drawing.
- `game/`: the game seam, pricing, build sequencer, map export.
- `line/`: line painter and the router shared with the station tool; `junction_router` is live, not superseded.
- `station/`: station generator, rules and window.
- `legacy/`: superseded generator code, not extended; see its CLAUDE.md.
- Root files: `junction_pipeline` (the driver: bridge cut, seed, detours, terraform entry), `junction_track_geometry` (axis and bend helpers shared by placement, render and signals), `junction_layout` (the output type), `junction_route` (route programs, rasteriser, ASCII, `JunctionTrackRoute`/`JunctionTrackPieceTag`), `junction_train_model`, `junction_rules` (the rule checker), `junction_spec` (shared spec, notation, prices, curve spacing, prio sizing).
- Tests are in `src/tests/junction/`, mirroring these folders (see its CLAUDE.md).

## Terms
- Split: a graph node where tracks diverge. Merge: a graph node where tracks join. Both are topological.
- Merger: a coop pattern (ABR06 style) built from merges, prios and signals; a prio is one merger style; the style is a user option per hub type with presets.
- S-bend: a sideways step; shift: only the coop shifter; never "switch".

## Pipeline
- Topology: ports (sides, tracks, order) and movements; names the junction type.
- Graph: nodes (splits, merges), edges, crossings per edge, built by rule from the topology.
- Order: node and crossing order along each edge by a sweep from the branch side.
- Structure: row stack, side tracks, bridge and tunnel grouping, turns.
- Programs: one token list per graph edge (Fan, Corner, SBend, Over, Under, Tunnel, Above).
- Placement: the least solution of the graph's relations on the terrain facts, as difference constraints in `JunctionPositions`.
- Levels: per-route level solve with ramps, foundations and terraform.
- Render: routes rasterised to a track-only `JunctionLayout`.
- Signals: signal tokens resolved on the rendered routes by `SignalJunctionTrackRoutes` (drag spacing through `PlaceDragSignals`), plus the prio element.
- Prio length: the slider in the window decides the coverage built; its top and start M is `JunctionPrioTiles`: a joiner starting from rest at the merge and a main-line train of the same picked train following at the reference speed, stepped by the game's acceleration model; M is the main train's largest lead until the joiner reaches that speed, in tiles rounded up, plus TL and the signal spacing, on flat track; at most `JUNCTION_MAX_PRIO_TILES`.
- Hover: `PlanJunctionHoverNear` solves at the cursor, then near offsets, and feeds the GUI estimate and build.
- A `JunctionLayout` is output only. Stages read the graph, never a rendered layout; the level solve is the one exception today: it reads the rendered, signalled layout (`junction_pipeline.cpp`).
- The bridge-cut search, when no cut places, runs the joint loop on the default cut (a series of at most the cap from the start) from all-floor lengths, and takes its placement if the loop settles and the placement at its lengths succeeds; otherwise it reports the default cut's failure with the loop's outcome.
- The checker `CheckJunctionRules` is a test oracle; the search still keeps it in acceptance, because the template does not yet cover every rule.

## Rule checker
- The checker rejects a split of a main-line lane inside a prio's coverage; a split here means any divergence or join of main-line tracks.
- The checker requires prio to hold under OpenTTD's real pre-signal logic: for every main-line (tile, track) within coverage plus the tile before the first upstream signal, a one-tile train there must settle the joiner's Entry red.
- No adjacent signals on a main-line lane inside a prio's coverage; "signal, one plain tile, signal" is allowed when the pre-signal requirement above holds.

## Game rules the model mirrors
- Bridge ends and span heights: `CheckBridgeSlope`, `CmdBuildBridge`, `BRIDGE_TOO_LOW_FOR_TERRAIN`; bridge type tables `table/bridge_land.h`.
- A bridge span clears tiles with no build-above handler once (`CmdBuildBridge`, `tunnelbridge_cmd.cpp`).
- Tunnels: `CmdBuildTunnel` (portal and exit search), `max_tunnel_length`.
- `BRIDGE_Z_START` lives in `bridge.h` as a `static constexpr`.
- Vanilla cannot signal bridges: `CmdBuildSingleSignal` needs a plain rail tile.
- Signal dragging: `CmdSignalTrackHelper` (`rail_cmd.cpp`) with weights 192 and 128.
- Pre-signal logic: OpenTTD's segment exploration and pre-signal propagation; a signal tile belongs to both segments it bounds; reference `docs/junction-tool/tools/sigsim.py`.
- Curve speed: `Train::GetCurveSpeedLimit`; the penalty applies only when one train covers two or more same-direction 45-degree bends at once; the limit is `232 - (13 - s)^2` for average spacing s in half tiles (straight 2, diagonal 1, clamped 1..12), plus `speed / 2 x curve_speed` of the rail type; bends one wagon apart cap at 88; one left and one right is exempt only when they are the only bends under the train.
- A 90-degree bend caps speed at 61.
- Train steps: a straight tile is 16 steps and a diagonal piece 8 (`GetNewVehiclePos`); the curve check counts wagon lengths of 8 steps.
- YAPF: curve penalty 100 per 45-degree curve (`rail_curve45_penalty`), red-signal look-ahead penalty, first red +1000, exit +10000, pass-through station tile +1000, uphill +200, doubleslip +100; ties between lanes are broken by these, so doubled lanes are built with equal cost.
- A dead-end tile with several pieces is a choice for YAPF; a plain dead end drops the branch.
- Slope force: realistic acceleration adds +1 per vehicle part on an up tile and -1 on a down tile; one incline tile is one level.
- The solver reads terrain only through a small interface with a game implementation and a hand-built test implementation, as `FontCache` and `MockFontCache`.
- Bridge type for a limited rail type follows the rail's own limit; for an unlimited type it is the cheapest bridge at least as fast as the design speed, else the fastest bridge.

## Working rules for agents
- Execute the brief and nothing else; add no type, member, file or mechanism the brief does not name; stop at a clean state and report a question instead of choosing between approaches.
- A workaround that contradicts a ruling is a breach; a shortcut that changes what a layout looks like goes to the engineer first.
- Work in the worktree the brief names; create parallel worktrees by hand from the feature branch, because `isolation: "worktree"` branches from master.
- Never use `git stash`; the stash stack is shared across worktrees; use a WIP commit.
- Never touch another agent's worktree under `.claude/`.
- Never start a game client, touch the engineer's game, or touch the rig servers on ports 4077 and 4079.
- Rig runs use their own ports and a scratch run dir; a GUI client for the engineer uses his normal `~/.config/openttd` and data dirs, only one GUI client at a time.
- Use absolute paths; send command output to files and report only what failed.
- Commit messages follow OpenTTD's style (`Add:`, `Feature:`, `Fix:`, `Codechange:`, `Doc:`) and end with the Co-Authored-By line given in the session.
- A seam edit to an upstream file is its own `Codechange:` commit.
- Margins come from the rules or the checker; no hedged padding.
- Follow `CODINGSTYLE.md` (C++ part) for all code.
- The report is the standard report named in the brief: what changed, test results per gate (count and time), red evidence, open questions; stop when reported.
- Hand-made layouts are checker fixtures only; layout design belongs to the generator.
- A layout step reads the graph, never a rendered layout, and names its source artefact.
- Every commit that changes behaviour or a rule updates the CLAUDE.md of the folder whose code enforces it, in the same commit, adding promoted facts and removing superseded ones; open questions never go in a CLAUDE.md.
