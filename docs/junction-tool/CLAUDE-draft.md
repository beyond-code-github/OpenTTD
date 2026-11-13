# Junction tool (src/junction_*, src/tests/junction_*)

Rules of the junction tool as present-tense facts. Ids in brackets point to docs/junction-tool/DECISIONS.md (D), DESIGN-G1.md (G1 §), or a brief id. Read DECISIONS.md only for the reason behind a rule; grep it for the id first.

## Scope and files
- The tool places generated rail junctions (hubs) from the rail toolbar, prices them exactly, and builds them with ordinary commands (D1, D2, D7, D9).
- It is core C++ in a fork; new code lives in `junction_*` files and edits to upstream files are minimal seams, each in its own `Codechange:` commit (D42, D43).
- The fork never changes the savegame format: no new settings, map bits, pool objects, chunks or version bump (D69).
- The tool's own state lives in client settings (`openttd.cfg`) only (D69).
- Current pipeline files: `junction_topology.{h,cpp}` (topology, graph, order, structure, programs, placement, render, signal tokens), `junction_positions` (difference constraints), `junction_facts` (terrain facts), `junction_levels` (level solve), `junction_terraform`, `junction_hover` (`PlanJunctionHover`, `PlanJunctionHoverNear`; `PlanJunctionHoverAt` is file-static), `junction_chain` (curve gap constraints), `junction_route` (route programs, rasteriser, ASCII), `junction_layout` (the output type), `junction_game` (game seam, spec and pricing), `junction_build`, `junction_gui` (window; hover always plans on this pipeline), `junction_ghost_draw`, `junction_rules` (checker), `junction_signal_pass` (only `PlaceDragSignals`, called by `junction_topology.cpp`, is current), `junction_train_model`, `junction_terrain.h`.
- Superseded by the graph pipeline, all compiled: `junction_generator` (template generator; called by `junction_gui.cpp`'s unreachable non-projection branch and by `console_cmds.cpp`; 15 headers use it for shared types such as `JunctionSpec`, so it is not deletable yet), `junction_nodes` (called by `junction_game.cpp`, `SearchJunctionNodes` in `GenerateJunctionNear`; also a test oracle), `junction_skeleton` and `junction_throughput` (called only by the generator), `PlaceJunctionSignals` in `junction_signal_pass`, `junction_organic` (no caller outside its own test) and `junction_compact` (tests only). The window's hover never reaches them. New work does not extend them (D201, D216, G1 §14.1, RECON-folder-layout §5).
- Line painter files: `junction_line`, `junction_line_router`. `junction_router` is live, not superseded: the line painter and the station tool both use it. Station files: `junction_station_*`. Map export: `junction_map_export`, `junction_terrain_json` (console only) (D112, D106, D128).
- Tests are `src/tests/junction_*.cpp` (Catch2, tags in the Testing section).

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
- A `JunctionLayout` is output only; no stage reads a rendered layout as its input (D201, D213).
- The checker `CheckJunctionRules` is a test oracle, never an acceptance step on the hover path (D193).

## Notation and topology
- A line is written with `L` (one lane of one direction), `R` (the other direction) and `_` (one empty tile): LL_RR has two lanes each way with a one-tile gap (D38).
- Tracks of one direction are adjacent (gap 0); the underscores set the only gap between direction groups (D142).
- The notation is a default spacing; spacing may vary along a line (D38).
- Main line and branch each take their own notation (D60).
- A topology holds ports per side as lane groups (side, direction, lane count, line, gap to the next group) and movements (in group to out group) (G1 §1).
- Lanes are expanded by rule, not listed; unequal lane counts are allowed (D126, D135).
- Branch ports are on the S side only; N-side and both-side branches are refused as not ruled (G1 §8).
- Lane pairing for multi-to-multi movements is a topology option: any (default), keep, or per direction; it applies only where both groups of a turning movement have more than one lane (D221).
- Topology is chosen in a separate Topology window opened from the junction window; the main window holds a presets dropdown (which only sets the Topology window's choices, "Custom" when none match) and a Topology... button; lanes and gaps use steppers (D222).
- Driving side follows the game's road-side setting; Mirror flips a placement (D66).
- Terms: game compass (N up), right-hand running; say "split" for any point where tracks diverge or join, "S-bend" for a sideways step, "shift" only for the coop shifter (D108).

## Graph, order and structure
- Splits come before merges along every track; outgoing lines split before incoming lines merge (D24, D189).
- A line splits by direction first, then by lane (line choice); a lane group's lane legs merge before the port on the exit side, and a joining branch splits by direction then by lane next to the main line it feeds (D187, D204, D205).
- Two legs of one branch bound for one main lane merge with each other first and join as one joiner, with one prio element per lane (D224).
- Every lane of a direction splits toward the branch; no shifters in LL_RR (D187, D191).
- A switch has two legs; more are refused as not ruled (G1 §7).
- Crossings are counted per graph edge; two paths cross when their end ports interleave around the box; every chord pair is crossed exactly once (G1 §6, G1 §7).
- Over and under: a crossed row edge (a lane along its own row) carries one doubled bridge over all its crossings (D156, D190, G1 §9).
- A crossing of two non-row edges needs exactly one joiner (a path from a branch slot): the joiner goes over on a single bridge; zero or two joiners are refused as not ruled (G1 §9).
- Branch-on-branch crossings sit in the outermost band beside the branch ports (G1 §8).
- One bridge or tunnel may straddle several crossings of one path within its span limits; the default is one bridge per crossed row edge, cut into series bridges at `JUNCTION_BRIDGE_MAX_CROSSERS` (2) crossers per bridge (D203, D209, G1 §13).
- Rows: lanes spread inside the box by S-bends to open rows for prio rows, doubling copies and joiner approaches, and return to the notation spacing at the ports (D203, D38).
- A prio sits opposite the joiner; the approach is on the joiner's side; a crossed lane's doubling copy shares the prio's row or sits away from the branch (G1 §9).
- A row change sits next to the feature it serves, and the free run lies on its other side (D212, D215).
- Exit lane legs of a direction merge in the outer band beside the branch port (D205, D210).

## Bridges and tunnels
- Every main-line bridge is doubled, whatever it crosses (D80, D174).
- A bridge carrying branch traffic is single (D174).
- A single branch bridge that carries a waiting bay counts as part of the bay (D174).
- A doubled bridge is a symmetric or parallelogram arrangement with equal pathfinder cost on both lanes; the shape is free (D100, D105).
- In the parallelogram, copy 1's heads sit one tile upstream of copy 0's, both spans equal, one signal tile between each split and its head (D118, D145).
- A bridge's crossers are packed exactly; heads hug the outermost crossers (G1 §11.8).
- A crossing set longer than the cap splits into consecutive bridges in series with one plain signalled tile between them; copies stay parallel with a signal each (D209).
- The series cap is `JUNCTION_BRIDGE_MAX_CROSSERS` = 2 crossers per bridge (D209).
- The longest bridge middle is the game's `max_bridge_length` setting, and a span is allowed when `ChooseJunctionBridgeType` finds a bridge for it (D208, `MakeJunctionSpec`).
- A placement whose doubled bridge needs a middle above that is refused naming the bridge (D208).
- Bridge type: the cheapest bridge at least as fast as the design speed; if no bridge reaches it on an unlimited rail type, the fastest bridge is used and trains slow on it (D35, D94, D141).
- A tunnel is a wormhole like a bridge: one block, no signals inside, surface track may cross the tiles between portals (D154).
- A tunnel is a `JunctionBridge` with `tunnel = true`; only build, pricing, JSON, ASCII (`O` portals) and the rig script branch on it (D154).
- In the graph pipeline a `JunctionTrackBridge` has `tunnel`, valid on a single (not doubled) separation only; tokens Tunnel and Above mirror Over and Under; a tunnel longer than `max_tunnel_length` is refused naming it (D220, TU1-TU3).
- A crossing may be a tunnel or a bridge; tunnels have no span-height limit and suit wide topologies (D220).
- Tunnel portals are on Buildable, non-steep, free inclines; entry rises along travel with the track at its low edge; the exit is the first tile with the same low edge inclined the other way (D155).
- Tunnels cross other tunnels only at different depths; a tunnel carries a floor level `z` (D155).
- Tunnels may pass under towns and industries; bridges never span blocked tiles (D175).
- Bridges may span existing plain rail and plain road of any owner (D104).
- Tunnel placement, render, signals and level solve exist for a single separation; the hover never chooses a tunnel: only a test sets a crossing to tunnel (TU1-TU4).

## Placement (relations, facts, choices, edits)
- Placement is relations plus terrain facts solved together; no flat positions enter the terrain solve (D216, G1 §14.1).
- Every relation is a rule floor "at least N", scaling with train length and speed; each entry cites its rule (G1 §14.3, G1 §16.1).
- Relations are rule families applied everywhere, never per site: every curve-gap pair on every route, and unrelated tracks never share or touch a tile (D217).
- Site floors and per-shape constants exist only as test oracles (D217).
- Every edge is one chain of elements; one floor table keyed by (previous kind, next kind) holds every run constant (G1 §16.1, RW1-RW6).
- Curve gaps: consecutive same-hand bends at least `min(2TL - 1, max(2, CL))` in units along the path, opposite hands at least 2, non-consecutive at least `min(2TL - 1, (j - i) CL)` (`JunctionCurveGaps`, G1 §11.8).
- Curve length CL is derived from the train, not set: the smallest bend spacing (1..12) whose curve speed limit is at least the design speed, capped at 2 TL (`DeriveCurveSpacing`, D67).
- A diagonal half piece counts as half a tile for CL (D36).
- A split or merge is the tile of its first diverging or last joining half piece (G1 §11.8).
- Terrain facts are bounds on the X phase: a straight run on a row is buildable on every tile between its points, heights included (G1 §14.4).
- A mid-row obstacle is never absorbed by positions; it needs a graph edit (G1 §14.4).
- Rows (Y) come from the structure and change only by an edit named by an X failure (G1 §14.4).
- Only hard facts fail a solve: houses or objects kept, water, the map edge, unpurchasable tiles; heights cost instead of failing (D218).
- Blocked tile classes: houses, other companies' property, unclearable objects; own track is blocked for track and Spannable for bridges; water is allowed only under spans; trees and rough land cost their clearing price (D93, D104).
- Objective within a graph: the least solution, then each row change pushed toward its feature (D215, G1 §14.5).
- Cursor spot first: the hover plan solves at the cursor; a nearby offset is used only when the cursor spot has no layout, shown as moved (D197).
- Placement relations are checked by tests on both topologies (the T and LL_RR x L_R) from the start so nothing T-only can pass (D202).
- Stub length at a branch port is two tiles: the signal tile then the boundary tile; the stub is one rule constant (D115a, D206).
- The junction footprint ends one tile beyond the outermost signal on each leg; every track of a leg ends flush at the footprint edge (D58).
- Planar signal windows and node footprints: a node has a footprint from the rules (switch tile, required straight runs, prio coverage) and a window along its line (D182).

## Heights (levels, foundations, terraform, cone, ramp rule, bridge heads, tunnels)
- Height is solved per tile on the rendered routes after placement; a ramp is a tile, not a graph token, and a height failure is not a graph edit (D218, G1 §15).
- A tile can be a ramp only when it holds a lone axis track that every route crosses the same way and is not a node, port, head or route end; a ramp climbs or drops at most one level (`SolveTileLevels`).
- A track piece running along a tile's incline is a ramp and is never levelled; a levelled foundation applies only across an incline and to diagonal pieces (D152b).
- Curves and diagonals cannot sit on a ramp tile (G1 §15.4).
- Ramp rule: along every route the level gain over any train length is at most `k_max = floor(tl x S(v) / (m x s x 100))` in the travel direction; descents are free (D219, G1 §15.4).
- The junction tool requires the realistic acceleration model and refuses to generate with the original model, saying why (D219).
- Terraform is allowed by default and costed with the game's price per moved corner, shared corners counted once (D218, D95).
- The level solve costs terraform exactly by clamping the ground to the cones of fixed track corners and descends to the cheapest levels (HC1, P4b, P6b).
- The hover plan carries a level for every track tile, prio tiles included (H3).
- The levelled foundation is offered only where `GetRailFoundation` says Leveled (HM1).
- A bridge head may sit on the slope rising to its span; the ground under a span stays at or below the heads (IG3).
- The ramp rule counts the climb onto and across bridges: a flat head climbs onto the deck, an inclined head sits at it (IG4).
- Tunnel level plan: portals on the game's raw slope rising into the tunnel, the outer edge at level L, ground over the middle at least L+1, the game's exit search ends at the planned portal (TU4).
- The level solve never asks the game about an impossible slope; it returns a plan or a reason (P3b).
- Each port has its own height, derived from its tile and the piece's direction as the game builds track (D180).
- Terraform steps are the game's: simulated corner by corner they reach exactly the plan's corners (H2, HC1).

## Signals and prio
- Default signal type is one-way block, a junction-only client setting separate from the signal toolbar's type (D20, D68).
- Signal spacing SD is the game's signal spacing when dragging (`drag_signals_density`), applied as `CmdSignalTrackHelper` does: logical length 192 per axis piece and 128 per diagonal half piece, density SD x 192, minimise-gaps mode (D65, D97).
- Fill walks upstream from the downstream fixed signal, so the short block falls at the upstream end and bays line up on the grid (D97).
- Signals are graph tokens placed by rule; a run floor that exists to hold a signal becomes a Signal token with its own rule (D211).
- A legal signal piece is not on a bridge, a head or a split tile and has no overlapping track on its tile (G1 §12.2).
- BeforeSplit and BeforeMerge: the last legal piece before the switch; AfterSplit and AfterMerge: the first legal piece after it; BeforeBridge and AfterBridge likewise; Stub: the piece before the boundary tile (G1 §12.2).
- Every bridge and tunnel is its own block: a signal on the nearest legal piece before the near head and after the far head, on every path (D82, D154).
- Between a split and the next bridge head there is a signal-legal piece with a signal; likewise a merge after a far head (D113).
- A signal straight after every line-choice split stands; no minimum length from a split to the next bridge head (D192).
- One signal may serve several rules of its stretch; no back-to-back signals where one suffices (D117).
- Waiting bays: every queue ending at a merge signal has boundary signals at least 2 TL units apart going back from the merge signal, each boundary on a legal piece that is not a split of the path; bays may contain a bridge; at least one bay before the upstream split (D83, D88).
- A train stopped in a bay does not hold a split, so a held train never blocks the main line (D71, D83).
- Bays add signals and never replace SD spacing; following trains keep close headway (D83).
- Prio: a merge onto the main line carries a prio, a parallel unused track joined at intervals upstream so a main-line train holds the merging signal red (D25, D27).
- Prio signals are always pre-signals (entry, combo, exit) regardless of the junction signal type; one prio segment per main-line block (D74).
- Prio joins are trailing pairs of 45-degree pieces; an unused-track join is a prio connection, not a merge (D75).
- The prio's first join may sit on the merge tile, where it holds only its half piece with no axis stub (D79, D81, D116).
- The checker (G9c) rejects a split of a main-line lane inside a prio's coverage; a split here means any divergence or join of main-line tracks (D108, D116).
- The checker (G9b) requires prio to hold under OpenTTD's real pre-signal logic: for every main-line (tile, track) within coverage plus the tile before the first upstream signal, a one-tile train there must settle the joiner's Entry red (D107, G9b).
- No adjacent signals on a main-line lane inside a prio's coverage; "signal, one plain tile, signal" is allowed when G9b passes (D107, D116).
- A prio's length comes from the picked train: the gap between the main-line train's front and the joiner's tail never drops below one signal block while the joiner accelerates to design speed (D102, D137).
- Prio coverage is in whole tiles, sized by exact timing; coverage above 32 tiles is refused ("Prio coverage would exceed 32 tiles"); the prio element's joins sit at even offsets, so an odd length is built one tile longer (D34, D137, G1 §12.4).
- The window has a prio length slider from one TL up to the sized length; below the maximum the spec carries `prio_strict = false` (D139).
- Every main-line lane that has a joiner (a merge whose other edge is not a row edge) has a prio element; the throughput estimate does not decide it on the graph pipeline (D159, G1 §9).
- Main-line delay from the junction is strictly zero by default (D119).

## Hover and GUI
- Generation on hover is under 100 ms for both topologies (T and LL_RR x L_R); `[realterrainsweep]` requires it of the plan and of the near search over the 48 fixtures (D151, D62, P6b).
- The window caches up to 32 hover results per (anchor, rotation, mirror, notations, TL, signal type, SD, train, prio tiles, lane pairing); the cache is cleared when the window's settings change and after a build, and has no terrain hash (D93, `JunctionGenerateKey`).
- The near search checks `cancel` before each offset and returns "cancelled" when set; the GUI passes the job's cancel flag; a superseded hover is cancelled when the cursor moves (CN1).
- The near search rejects an offset before placing it when its X solve must refuse it; a tunnel separation counts as a bridge there; the reject never rejects a placeable offset (P5b).
- The near search moves the box by at most 8 columns and 4 rows and only when the cursor spot fails (F1, D197).
- Junction generation requires a picked train; the tool does not generate without one; the programmatic interface takes the train's loaded parameters (D137).
- The player picks a train by clicking one of their trains, as the depot's clone tool does; the tool reads power, tractive effort, loaded weight, air drag, max speed and length (TL) from the game's caches (D101).
- The picked train, the prio slider, notations, TL, signal type and lane pairing persist across sessions in the client config (QL2, D69).
- Loaded mass uses the live `freight_trains` multiplier; design speed is `min(train, rail type)` (D101, D141).
- Persistent values are client settings (`openttd.cfg`, `gui.junction_*`), never the savegame (D64, D69).
- Follow house UI conventions for tooltips, windows and keys (D48).
- The window shows a persistent cost line (`Cost: {YELLOW}{CURRENCY_LONG}`), red when unaffordable or invalid (D14, D49).
- Shift-click shows the estimated cost of the whole junction at the clicked tile and never starts building (D51).
- Invalid-placement text is generic ("tiles shown in red can't be used"); only "too close to edge of map" stays specific (D55).
- Rotation (`.`) and mirroring (`,`) are by hotkey (D15).
- A solved plan is built by posting ordinary commands in sequence; a failed command stops the sequence and reports what was and was not built (D7).
- The estimate is the exact sum of test-only runs of every command, re-checked just before building; building is refused when unaffordable (D9).
- Where a bridge span tile is also a rail tile, its clearing cost is counted once (D50).
- Estimate and build cover terraform steps first; after a build an information message states `Spent X, quoted Y` (CN1, QL2, IG2).
- Only tiles the layout uses are outlined, while hovering and while building: white for track tiles, blue for tiles that are only terraformed; no full grid is drawn (QL1, CN1).
- Cost and placement errors appear in the cursor tooltip, not in the window (QL1).
- Building is not network-aware beyond the game's own commands; no new network command exists (D136(c), D69).

## Game rules the model mirrors
- Rail foundations and slopes: `GetRailFoundation` (a piece along an incline is a ramp, D152b); `build_on_slopes` setting (D152b).
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
- YAPF: curve penalty 100 per 45-degree curve, red-signal look-ahead penalty, first red +1000, combo and exit +10000, pass-through station tile +1000, uphill +200, doubleslip +100; ties between lanes are broken by these, so doubled lanes are built with equal cost (D100, D132).
- A dead-end tile with several pieces is a choice for YAPF; a plain dead end drops the branch (D132).
- Terraform costs: `CmdTerraformLand` and `CmdLevelLand` prices plus clearing (D95).
- Slope force: realistic acceleration adds +1 per vehicle part on an up tile and -1 on a down tile; one incline tile is one level (D219, G1 §15.4).
- The solver reads terrain only through a small interface with a game implementation and a hand-built test implementation, as `FontCache` and `MockFontCache` (D6).
- Bridge type for a limited rail type follows the rail's own limit; for an unlimited type it follows D141 (D45, D94).

## Testing
- Test-first, outside-in: write the outer test, record it red, then build the inside (D4).
- Run tests through `tools/junction/jt`, never hand-typed tag filters: `jt fast <build-dir> <tag> [budget-s]`, `jt gate <build-dir>` (Release only, wide gate `[junction]~[slow]~[mapexport]~[.]`), `jt identity <build-dir>` (Release only) (D140).
- `jt fast` runs `<tag>~[slow]~[.]` and fails with "OVER BUDGET" past the budget; over budget is a stop condition (docs/junction-tool/TESTS.md).
- Budgets per tag are in docs/junction-tool/TESTS.md; adding a test to a fast tag keeps it inside the budget or tags it `[slow]`.
- Fast tags (Debug budget 5 s unless the table says otherwise): `[junctionmap]`, `[junctionmapedges]`, `[linerouter]` (12 s), `[linebuild]`, `[terrainrect]`, `[projection]` and `[projectionedges]` (measured in Release), `[relaxed]`, `[topology]`, `[topoproj]`, `[nodes]`, `[positions]`, `[facts]` (1 s Release), `[terraform]`, `[hoverplan]`, `[game]`, `[junctiongame]`, `[junctionprice]`, `[tunnelprice]`, `[tmapnear]`, `[curvespeed]`, `[realterrain]`.
- Gate-only sweeps: `[topologysweep]`, `[topoprojsweep]`, `[topologystretch]`, `[realterrainsweep]`, `[nodeoracle]`, `[placeoracle]`, `[nodespeed]`, `[junctiongamesweep]`; `[identity]` is the hidden identity tag run by `jt identity`.
- Run `jt identity` in every step that touches `junction_topology.cpp` or `junction_levels.cpp`, alongside the gate.
- Every step runs: its fast tag in Debug, the gate in Release, and `jt identity` where the line above applies (D140).
- Identity tests pin placements, renders, signals and plan hashes over the sweeps (the [topologysweep] cells, `LEVELHASH` over the 48 fixtures); a pinned number moves only by a stated rule with the changed cases listed.
- A refactor step is an identity on every seed: `SamePlacement` over the [topologysweep] cells, the circuit file byte-identical, all sweep numbers equal; a step that moves a seed stops and reports (G1 §16.2, G1 §16.4).
- A planned count in a sweep (`HOVERPLAN`, `HOVERNEAR`, `LEVELHASH`) may only rise; a drop is a stop (P6b).
- The checker is an oracle for generated layouts: a violation found in a test is a missing rule in the model (D193).
- Real terrain comes from the 48 fixtures in `tools/junction/terrain` (generated 256x256 maps); the game seam is tested on a game map with a company (`[junctiongame]`) (RT1, IG2).
- Write red tests red first and record the failure text before the fix.
- Slow sweeps are tagged `[slow]` and stay out of per-step runs; no baseline rebuilds for timing (D140).
- Reuse the persistent worktree builds: Debug `build`, Release `build-rel`; ccache on both (D140).
- Wait for no test that has no budget: a test with no stated budget gets one before it joins a fast tag (TESTS.md).

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
