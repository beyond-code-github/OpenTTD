# Junction tool (src/junction/)

Rules of the junction tool as present-tense facts, split per folder: each fact lives in the folder whose code enforces it. Ids in brackets point to docs/junction-tool/DECISIONS.md (D), DESIGN-G1.md (G1 §), or a brief id. Grep DECISIONS.md for the id only for the reason behind a rule.

## Scope and folders
- The tool places generated rail junctions (hubs) from the rail toolbar, prices them exactly, and builds them with ordinary commands (D1, D2, D7, D9).
- It is core C++ in a fork; new code lives under `src/junction/` and edits to upstream files are minimal seams, each in its own `Codechange:` commit (D42, D43).
- The fork never changes the savegame format: no new settings, map bits, pool objects, chunks or version bump; the tool's own state lives in client settings (`openttd.cfg`) only (D69).
- `terrain/`: terrain snapshot, terrain facts, terraform, foundation and slope rules.
- `topology/`: topology, graph, order, structure, programs, curve gap constraints.
- `placement/`: horizontal placement, render, positions solver, level solve.
- `signals/`: signal resolution, drag signals, prio spans.
- `tool/`: hover planning, the window, the ghost drawing.
- `game/`: the game seam, pricing, build sequencer, map export.
- `line/`: line painter and the router shared with the station tool; `junction_router` is live, not superseded (D112, D106, D128).
- `station/`: station generator, rules and window.
- `legacy/`: superseded generator code, not extended; see its CLAUDE.md.
- Root files: `junction_pipeline` (the driver: bridge cut, seed, detours, terraform entry), `junction_track_geometry` (axis and bend helpers shared by placement, render and signals), `junction_layout` (the output type), `junction_route` (route programs, rasteriser, ASCII, `JunctionTrackRoute`/`JunctionTrackPieceTag`), `junction_train_model`, `junction_rules` (the rule checker), `junction_spec` (shared spec, notation, prices, curve spacing, prio sizing).
- Tests are in `src/tests/junction/`, mirroring these folders (see its CLAUDE.md).

## Pipeline
- Topology: ports (sides, tracks, order) and movements; names the junction type (D189).
- Graph: nodes (splits, merges), edges, crossings per edge, built by rule from the topology (D201, D202, G1 §7).
- Order: node and crossing order along each edge by a sweep from the branch side (G1 §8).
- Structure: row stack, side tracks, bridge and tunnel grouping, turns (G1 §9).
- Programs: one token list per graph edge (Fan, Corner, SBend, Over, Under, Tunnel, Above) (G1 §10.7).
- Placement: the least solution of the graph's relations on the terrain facts, as difference constraints in `JunctionPositions` (D216, G1 §14).
- Levels: per-route level solve with ramps, foundations and terraform (D218, G1 §15).
- Render: routes rasterised to a track-only `JunctionLayout` (G1 §11.10).
- Signals: signal tokens resolved on the rendered routes by `SignalJunctionTrackRoutes` (drag spacing through `PlaceDragSignals`), plus the prio element (D211, G1 §12).
- Hover: `PlanJunctionHoverNear` solves at the cursor, then near offsets, and feeds the GUI estimate and build (D197, D96).
- A `JunctionLayout` is output only. Stages read the graph, never a rendered layout (D201, D213); the level solve is the one exception today: it reads the rendered, signalled layout (`junction_pipeline.cpp`).
- The checker `CheckJunctionRules` becomes a test oracle only, never an acceptance step on the hover path; until the RC0 audit shows full rule coverage, the search keeps it in acceptance, and each uncovered rule moves into the template as its own step (D193).

## Rule checker
- The checker (G9c) rejects a split of a main-line lane inside a prio's coverage; a split here means any divergence or join of main-line tracks (D108, D116).
- The checker (G9b) requires prio to hold under OpenTTD's real pre-signal logic: for every main-line (tile, track) within coverage plus the tile before the first upstream signal, a one-tile train there must settle the joiner's Entry red (D107, G9b).
- No adjacent signals on a main-line lane inside a prio's coverage; "signal, one plain tile, signal" is allowed when G9b passes (D107, D116).

## Game rules the model mirrors
- Bridge ends and span heights: `CheckBridgeSlope`, `CmdBuildBridge`, `BRIDGE_TOO_LOW_FOR_TERRAIN`; bridge type tables `table/bridge_land.h` (D35, IG3, IG4).
- A bridge span clears tiles with no build-above handler once (`CmdBuildBridge`, `tunnelbridge_cmd.cpp`) (D50).
- Tunnels: `CmdBuildTunnel` (portal and exit search), `max_tunnel_length` (D155, TU4).
- `BRIDGE_Z_START` lives in `bridge.h` as a `static constexpr` (D43).
- Vanilla cannot signal bridges: `CmdBuildSingleSignal` needs a plain rail tile (D19).
- Signal dragging: `CmdSignalTrackHelper` (`rail_cmd.cpp`) with weights 192 and 128 (D97).
- Pre-signal logic: OpenTTD's segment exploration and pre-signal propagation; a signal tile belongs to both segments it bounds; reference `docs/junction-tool/tools/sigsim.py` (D107).
- Curve speed: `Train::GetCurveSpeedLimit`; the penalty applies only when one train covers two or more same-direction 45-degree bends at once; the limit is `232 - (13 - s)^2` for average spacing s in half tiles (straight 2, diagonal 1, clamped 1..12), plus `speed / 2 x curve_speed` of the rail type; bends one wagon apart cap at 88; one left and one right is exempt only when they are the only bends under the train (D67, D18).
- A 90-degree bend caps speed at 61 (D18).
- Train steps: a straight tile is 16 steps and a diagonal piece 8 (`GetNewVehiclePos`); the curve check counts wagon lengths of 8 steps (D36).
- YAPF: curve penalty 100 per 45-degree curve (`rail_curve45_penalty`), red-signal look-ahead penalty, first red +1000, exit +10000, pass-through station tile +1000, uphill +200, doubleslip +100; ties between lanes are broken by these, so doubled lanes are built with equal cost (D100, D132).
- A dead-end tile with several pieces is a choice for YAPF; a plain dead end drops the branch (D132).
- Slope force: realistic acceleration adds +1 per vehicle part on an up tile and -1 on a down tile; one incline tile is one level (D219, G1 §15.4).
- The solver reads terrain only through a small interface with a game implementation and a hand-built test implementation, as `FontCache` and `MockFontCache` (D6).
- Bridge type for a limited rail type follows the rail's own limit; for an unlimited type it follows D141 (D45, D94).

## Working rules for agents
- Execute the brief and nothing else; add no type, member, file or mechanism the brief does not name; stop at a clean state and report a question instead of choosing between approaches (briefs/ header).
- A workaround that contradicts a ruling is a breach; a shortcut that changes what a layout looks like goes to the engineer first (D164).
- Grep docs/junction-tool/DECISIONS.md for an id or topic before asking the engineer anything; a recorded decision is not re-asked (DECISIONS header).
- Work in the worktree the brief names; create parallel worktrees by hand from the feature branch, because `isolation: "worktree"` branches from master (D140).
- Never use `git stash`; the stash stack is shared across worktrees; use a WIP commit.
- Never touch another agent's worktree under `.claude/`.
- Never start a game client, touch the engineer's game, or touch the rig servers on ports 4077 and 4079.
- Rig runs use their own ports and a scratch run dir; a GUI client for the engineer uses his normal `~/.config/openttd` and data dirs, only one GUI client at a time.
- Use absolute paths; send command output to files and report only what failed.
- Commit messages follow OpenTTD's style (`Add:`, `Feature:`, `Fix:`, `Codechange:`, `Doc:`) and end with the Co-Authored-By line given in the session.
- A seam edit to an upstream file is its own `Codechange:` commit (D42).
- Margins come from the rules or the checker; no hedged padding (D119, D120).
- Follow `CODINGSTYLE.md` (C++ part) for all code.
- The report is the standard report named in the brief: what changed, test results per gate (count and time), red evidence, open questions; stop when reported.
- Hand-made layouts are checker fixtures only; layout design belongs to the generator (D86).
- A layout step reads the graph, never a rendered layout, and names its source artefact (D201, D213).
- Every commit that changes behaviour or a rule updates the CLAUDE.md of the folder whose code enforces it, in the same commit, adding promoted facts and removing superseded ones; open questions never go in a CLAUDE.md (D225).
