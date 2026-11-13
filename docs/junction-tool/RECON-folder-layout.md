# RECON: folder layout for the junction tool (RO1)

Read-only recon on feature/junction-tool (724daf0a63..). 61 files `src/junction_*`, 2 in `src/widgets/`, 29 in `src/tests/`.

## 1. How OpenTTD organises src/
- Feature folders are flat inside: `src/linkgraph/` (demands, flowmapper, linkgraph*, mcf, refresh), `src/pathfinder/` (+ nested `yapf/`), `src/network/` (+ `core/`), `src/script/` (+ `api/`), `src/saveload/`, `src/blitter/`, `src/video/` (+ `cocoa/`). Nesting is at most one extra level, only for a sub-feature (yapf, core, api).
- Files keep their feature prefix inside the folder (`linkgraph/linkgraph.cpp`, `linkgraph_gui.cpp`, `network/network_admin.cpp`). That also matters here: `cmake/SourceList.cmake` aborts with FATAL_ERROR if two files in one target share a basename, so the `junction_` prefix must stay.
- CMake: each folder has a `CMakeLists.txt` of one `add_files(...)` list (sorted; `CONDITION` optional, used in video/ and os/). A parent adds children with `add_subdirectory(x)` first (`pathfinder/CMakeLists.txt` begins `add_subdirectory(yapf)`). `src/CMakeLists.txt` lines 1-30 hold the `add_subdirectory` list, alphabetical (`game`, `lang` ...). `add_files` prefixes `CMAKE_CURRENT_SOURCE_DIR`, so a subfolder list needs no path.
- Includes: root headers from a subfolder use relative `../`: `linkgraph/mcf.cpp` has `#include "../stdafx.h"`, `yapf_rail.cpp` has `"../../stdafx.h"`. Siblings in the same folder are bare (`"mcf.h"`). Outside includers use the src-relative path: `landscape.cpp` has `#include "pathfinder/yapf/yapf_river_builder.h"`, `console_cmds.cpp` has `"network/core/network_game_info.h"`. Only `src/3rdparty/squirrel/include` is a global include dir, so there is no src-rooted include.
- Widgets: all `*_widget.h` live in `src/widgets/` (own CMakeLists), not in feature folders (even `script/` has none).
- Tests: `src/tests/` is flat (no subfolders; `add_test_files` list in `src/tests/CMakeLists.txt`, files `#include "../x.h"`). Only 3rdparty has a nested tests dir (`src/3rdparty/icu/tests`). `add_test_files` is a plain `_add_files_tgt(openttd_test ...)`, so it works from any CMakeLists.
- README/CLAUDE.md in feature folders: none. Only `src/3rdparty/*` has READMEs. Nested CLAUDE.md is new here.
- Lang strings: all `src/lang/english.txt` (17 `junction` lines, 107 STR_JUNCTION uses in the tool). They stay.

## 2. Inventory (lines = .h + .cpp; tests in section 4)
Includes listed are junction files only, other than the file's own header. "Out" = non-junction includers.

| file | lines | owns | stage | junction includes | Out |
|---|---|---|---|---|---|
| layout | 161+592 | JunctionLayout (track-only render result), tile ops | model | terraform | - |
| route | 57+295 | JunctionTrackRoute, per-route data | model | layout | - |
| train_model | 56+149 | design train, speed/accel model | model | - | - |
| compact | 25+94 | layout compaction | model | layout | - |
| terrain.h | 90 | JunctionTerrain tile data | terrain | - | - |
| terrain_json | 25+90 | terrain fixture JSON load/dump | terrain | terrain | console_cmds |
| facts | 66+195 | terrain facts for placement | terrain | terrain | - |
| terraform | 38+162 | terraform plan | terrain | terrain | - |
| topology | 440+4522 | topology, graph, structure, programs, placement, render, signals, detours | topology..render | facts layout route terraform; cpp: chain generator levels line positions route rules_internal signal_pass | - |
| chain | 82+162 | edge chains | topology | generator layout rules | - |
| positions | 74+218 | difference constraints | placement | - | - |
| levels | 40+1586 | level solve, ramps, foundations | placement | facts topology; cpp: positions | - |
| signal_pass | 35+660 | signal resolve on rendered routes, prio | signals | generator layout route; cpp: rules_internal | - |
| rules (+_internal.h 92) | 88+1848 | checker `CheckJunctionRules` | check | generator layout route topology | - |
| hover | 59+109 | PlanJunctionHover/At/Near | hover | layout terraform topology; cpp: facts generator | - |
| ghost_draw | 40+138 | ghost track drawing | gui | - | - |
| gui | 73+1826 | build window | gui | layout; cpp: ghost_draw game generator hover line terraform topology rules widgets/junction_widget | rail_gui, viewport |
| game | 134+727 | game seam: estimate, near search | game | layout build generator terrain train_model; cpp: rules nodes | console_cmds |
| build | 61+86 | build commands from layout | game | layout | - |
| map_export | 32+257 | layout JSON for the rig | game | - | console_cmds |
| line | 95+396 | line painter model | line | generator route; cpp: signal_pass | - |
| line_router | 98+1199 | line painter router | line | layout line router terrain | - |
| router | 73+604 | tile-level A* | line (shared) | generator layout route terrain | - |
| station_generator | 28+309 | station layout | station | generator layout station_rules; cpp: route router rules_internal | console_cmds |
| station_rules | 45+281 | station rules | station | layout router; cpp: rules_internal | - |
| station_gui | 565 | station window | station | gui game station_generator widgets/junction_station_widget | rail_gui (via junction_gui.h) |
| generator | 234+2093 | template generator PLUS shared types (JunctionSpec, notation, prices, curve spacing) | legacy + shared | layout terrain train_model; cpp: nodes positions rules rules_internal line_router route router signal_pass skeleton throughput | - (but 15 junction headers include it) |
| nodes | 283+2583 | node template and solve (test oracle, still called by game.cpp) | legacy | facts generator layout positions rules rules_internal | - |
| skeleton | 68+186 | skeleton solver | legacy | - | - |
| organic | 40+266 | organic router | legacy | generator layout line rules terrain; cpp: line_router rules_internal route | - |
| throughput | 90+575 | throughput model | legacy | generator layout route rules train_model | - |
| widgets/junction_widget.h, junction_station_widget.h | 60, 31 | widget ids | gui, station | - | - |

Non-junction files that name junction things: `rail_gui.cpp` (include `junction_gui.h`, `ShowBuildJunctionWindow`, `JunctionToolClick`, `IsJunctionTrainPickActive`), `viewport.cpp` (`junction_gui.h`), `console_cmds.cpp` (game, map_export, station_generator, terrain_json), `window_type.h` (`BuildJunction`, `BuildJunctionStation`, `JunctionTopology`), `widgets/rail_widget.h` (WID_RAT_BUILD_JUNCTION*), `settings_type.h` (11 `junction_*` client settings), `table/settings/gui_settings.ini`, `src/CMakeLists.txt`, `widgets/CMakeLists.txt`.

## 3. Proposed layout
Tree (live dependencies run downward: terrain, model, topology, placement/signals/rules, line, hover, game, gui/station):
```
src/junction/CMakeLists.txt        add_subdirectory x 11
  CLAUDE.md                        scope, pipeline overview, working rules, game rules (draft: Scope, Pipeline, Game rules, Working rules)
  terrain/   terrain.h terrain_json facts terraform                  (terrain facts; CLAUDE: Placement facts part)
  model/     layout route train_model compact                        (output types; CLAUDE: Render, layout-is-output rule)
  topology/  topology chain                                          (CLAUDE: Notation and topology, Graph/order/structure, Bridges and tunnels)
  placement/ positions levels                                        (CLAUDE: Placement, Heights)
  signals/   signal_pass                                             (CLAUDE: Signals and prio)
  check/     rules rules_internal                                    (checker; CLAUDE: oracle-not-acceptance rule, Game rules mirrored)
  line/      line line_router router                                 (line painter + tile router)
  hover/     hover                                                   (CLAUDE: Hover part)
  game/      game build map_export                                   (game glue: estimate, build, export)
  gui/       gui ghost_draw                                          (CLAUDE: GUI part; widgets stay in src/widgets)
  station/   station_generator station_rules station_gui
  legacy/    generator skeleton nodes organic throughput             (CLAUDE: "do not extend", who still calls what)
src/tests/junction/<same folder names>/junction_*.cpp, plus src/tests/junction/CLAUDE.md (Testing section, TESTS.md rules)
```
Folders hold the folder's tests too, mirrored under `src/tests/junction/` (OpenTTD has one test dir; mirroring keeps a CLAUDE.md per area without leaving src/tests). Cross-cutting tests (`design`, `map`, `real_terrain`, `game`, `generator`, `topology` pipeline tests) sit at `src/tests/junction/` root or in their main folder. Mapping by name: layout/route/train_model/compact tests to model; facts/terraform to terrain; topology/chain to topology; positions to placement; rules to check; line/line_router/line_build to line; build/map_export/game to game; gui to gui; station_* to station; generator/skeleton/nodes/organic/throughput/router tests to legacy (router test to line); design, map, real_terrain at root.

Cohesion facts: all CMake and include direction is already acyclic except these, which the move preserves and the splits below remove:
1. `topology.cpp` includes `levels.h` and `positions.h` (placement) while `levels.h` includes `topology.h`: topology <-> placement at cpp level, because `PlaceJunctionTrackGraph` lives in topology.cpp.
2. `generator.h` is included by 15 live headers for shared types (JunctionSpec, notation parse, unit prices, CurveSpeedLimitForSpacing, DeriveCurveSpacing): so live folders include `../legacy/junction_generator.h`. First split: extract those into `model/junction_spec.h`.
3. `rules.h`/`chain.h` include `generator.h` the same way.

Files to split later (the move does not split):
- `junction_topology.cpp` 4522 lines. By function: ports/chords 37-330 (`JunctionPortSlots`, `JunctionChordCrossings`, `FindRowItems`) = topology; `BuildJunctionTrackGraph` 330-817 = graph; `StructureJunctionTrackGraph` 817-1150 = structure; `JunctionBendMoves`/`JunctionRunFloor` 1151-1220 + `PlaceTrackGraph` 1220-3452 (2200 lines, one function: its own split by relation group) + `PlaceJunctionTrackGraph`/`JunctionPlacementNearReject`/`MakeJunctionPlacementShared` 3452-3593 = placement; `RenderJunctionTrackPlacement` 3593-3785 + `AddJunctionTracks` = render; `JunctionRouteEvents`, `SignalJunctionTrackRoutes` 3812-4188 = signals; `MakeJunctionHubTopology` 4188, `AppendBridge*`/`CutJunctionTrackBridges` 4222-4348 (bridges), `MakeJunctionGraphSeed` 4348, `ApplyJunctionDetours` 4381 = detours. Targets: topology/{ports,graph,structure,bridges}, placement/place, model or render/render, signals/tokens.
- `junction_nodes.cpp` 2583, `junction_generator.cpp` 2093 (both legacy, leave), `junction_rules.cpp` 1848, `junction_gui.cpp` 1826 (window vs hover-estimate vs train pick), `junction_levels.cpp` 1586, `junction_line_router.cpp` 1199. Tests: `topology` 4461, `line_router` 2259, `rules` 1563, `nodes` 1515.
- `junction_generator.h` (shared types vs legacy API) as above.

## 4. Move mechanics
- `git mv`: 61 src files + the 29 tests (names unchanged). Widgets headers stay in `src/widgets/` (OpenTTD convention, `WID_*` enums are part of the window system; zero edits to `widgets/CMakeLists.txt`).
- CMake: delete `src/CMakeLists.txt` lines 253-313 (61 lines); add `add_subdirectory(junction)` after `game` (line 14); new `src/junction/CMakeLists.txt` (11 `add_subdirectory`) and 12 folder lists. Tests: remove the 29 names (lines 7-35) from `src/tests/CMakeLists.txt`, add `add_subdirectory(junction)` there and one `add_test_files` per test folder (CMake allows `add_subdirectory` from tests). `JUNCTION_TERRAIN_DIR` stays in `src/tests/CMakeLists.txt`.
- Include edits (scripted, resolve each include to the new path): src junction files have 150 junction-to-junction includes (about two thirds change to `../<folder>/junction_x.h`, same-folder ones stay), 261 non-junction quoted includes that gain `../../` (src depth two: `"stdafx.h"` -> `"../../stdafx.h"`; 30 each are `stdafx.h` and `safeguards.h`), and 2 widget includes (`../../widgets/junction_*.h`). Tests move to depth three (`src/tests/junction/<f>/`): all 297 `../` includes (127 junction, now `../../../junction/<folder>/junction_x.h`, and 170 other, gaining `../../`) change. External includers: `rail_gui.cpp`, `viewport.cpp` (`junction/gui/junction_gui.h`, 2 edits), `console_cmds.cpp` (4 edits). Total about 740 include edits, all mechanical.
- Alternative that avoids the 261+170 depth edits: `target_include_directories(openttd_lib PRIVATE src)` and src-rooted includes; rejected because OpenTTD never does it.
- Risks: (1) rename detection: in-flight branches/worktrees editing `src/junction_*` (wt-ig6 and others under .claude) merge by rename detection only above ~50% similarity, so land the move when no step is open or rebase them first. (2) Three unapplied patches (`docs/junction-tool/yr1-wip.patch`, `d3c-branch-swap.patch`, `wip/z2d-joint-levels.patch`) name old paths: apply before the move or edit paths. (3) Docs: 341 docs files, about 1458 references to `src/junction_*`/`src/tests/junction_*` (briefs, RECONs, STATUS, DECISIONS). Leave history, ship an old-to-new path table, update STATUS.md, TESTS.md, CLAUDE-draft and `tools/ottd/README.md`. (4) `tools/junction/jt`, `bench_anchors.py`, `terrain_fixtures.py` contain no src paths (only console command names), so they are safe; `jt` filters by Catch2 tag, not file. (5) Catch2 tags/test names unchanged, so TESTS.md budgets hold. (6) Generated tables: none specific to junction (strings come from `lang/`; `table/strings.h` is the only generated include, 4 files). (7) The duplicate-basename guard: keep the `junction_` prefix. (8) No `src/junction` name clash exists. Verify with a full Release build plus `tools/junction/jt gate`.

## 5. Legacy
| file | live callers | tests | verdict |
|---|---|---|---|
| organic | none in src (not even gui/game) | junction_organic.cpp only | deletable with its test, no src change |
| skeleton | `generator.cpp` only | junction_skeleton.cpp | goes with generator |
| throughput | `generator.cpp` only | junction_throughput.cpp | goes with generator |
| generator | `junction_gui.cpp` (`GenerateJunction`, window key), `console_cmds.cpp` (`GenerateJunction`, `GenerateJunctionNear`), `junction_game.cpp`; also 15 headers for shared types | generator, compact, rules, design, map, game, nodes, organic, chain, throughput, topology tests | NOT deletable yet: GUI and console still call it; shared types must be extracted first |
| nodes | `junction_game.cpp` (`JunctionNodeTopologyT`, `SearchJunctionNodes` in `GenerateJunctionNear`), `chain.cpp`/`topology.cpp` via chain.h | nodes, design, chain, game tests | live through game.cpp; legacy only by name |
| router | `line_router.h`, `station_rules.h`, `station_generator.cpp`, `generator.cpp` | router, line_router tests | NOT legacy: shared by line painter and station; placed in line/ (the draft calls it superseded; reconcile) |
Order to delete later: organic (free), then skeleton + throughput with generator after GUI/console move to the topology path and the spec types are extracted; nodes after `junction_game.cpp` stops calling it. Until then `legacy/` holds generator, skeleton, nodes, organic, throughput.
