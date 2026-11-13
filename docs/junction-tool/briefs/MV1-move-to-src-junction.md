# MV1: move the junction tool into src/junction/ feature folders (identity step)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Create branch `wt-mv1` there at the head given in your launch prompt: `git -C /home/pete/OpenTTD/.claude/nodes checkout -b wt-mv1 <head>`. Its builds are `build` (Debug) and `build-rel` (Release). Ignore the untracked docs/junction-tool/wip/. Never use git stash.
- Plan: docs/junction-tool/RECON-folder-layout.md (read it once, at the start). The engineer approved the layout below, which REPLACES the recon's tree: 8 folders, no model/, no check/, no signals/, no hover/, no gui/.
- This step only moves files. It renames nothing and changes no code beyond include lines, CMake lists and the one helper rename named below. Behaviour must be identical, so the identity tests must pass unchanged.

## Read first
CODINGSTYLE.md (the include section only), cmake/SourceList.cmake, src/pathfinder/CMakeLists.txt and src/pathfinder/yapf/CMakeLists.txt (the nested-folder pattern), src/pathfinder/yapf/yapf_rail.cpp (the first 20 lines, for the `../../` includes), and RECON-folder-layout.md §4 (mechanics and risks).

## The layout (git mv; basenames unchanged unless stated)
- `src/junction/` (root): junction_layout.{h,cpp}, junction_route.{h,cpp}, junction_train_model.{h,cpp}, junction_rules.{h,cpp}, junction_rules_internal.h
- `src/junction/terrain/`: junction_terrain.h, junction_terrain_json.{h,cpp}, junction_facts.{h,cpp}, junction_terraform.{h,cpp}
- `src/junction/topology/`: junction_topology.{h,cpp}, junction_chain.{h,cpp}, junction_signal_pass.{h,cpp}
- `src/junction/placement/`: junction_positions.{h,cpp}, junction_levels.{h,cpp}
- `src/junction/tool/`: junction_hover.{h,cpp}, junction_gui.{h,cpp}, junction_ghost_draw.{h,cpp}
- `src/junction/game/`: junction_game.{h,cpp}, junction_build.{h,cpp}, junction_map_export.{h,cpp}
- `src/junction/line/`: junction_line.{h,cpp}, junction_line_router.{h,cpp}, junction_router.{h,cpp}
- `src/junction/station/`: junction_station_generator.{h,cpp}, junction_station_rules.{h,cpp}, junction_station_gui.cpp
- `src/junction/legacy/`: junction_generator.{h,cpp}, junction_skeleton.{h,cpp}, junction_nodes.{h,cpp}, junction_throughput.{h,cpp}
- DELETE (git rm): src/junction_organic.{h,cpp} and src/tests/junction_organic.cpp. First confirm that nothing else includes junction_organic.h or calls its functions. If something does, stop.
- The compact helper moves into the tests and is renamed, because the test target already has a junction_compact.cpp: `git mv src/junction_compact.h src/tests/junction/junction_compactness.h` and `git mv src/junction_compact.cpp src/tests/junction/junction_compactness.cpp`. Update its include guard to JUNCTION_COMPACTNESS_H and its @file line. It leaves the game build and joins openttd_test.
- Stay where they are: src/widgets/junction_widget.h, src/widgets/junction_station_widget.h, src/lang/english.txt.
- Tests: `git mv src/tests/junction_X.cpp src/tests/junction/<folder>/junction_X.cpp`, with each test going to the folder of what it tests:
  - root (`src/tests/junction/`): layout, route, rules, train_model, compact.
  - terrain: facts, terraform.
  - topology: topology, chain, signal_pass.
  - placement: positions.
  - tool: gui, real_terrain.
  - game: build, game, map, map_export.
  - line: line, line_build, line_router, router.
  - station: station_generator, station_rules.
  - legacy: generator, nodes, skeleton, throughput, design.
  - Any junction test this list does not name: stop.

## Includes (scripted; one rule)
- Every `#include "..."` of a moved or root file is rewritten to the path relative to the including file's directory, the way src/pathfinder/yapf/ includes `../../stdafx.h`.
  - Junction to junction example: in src/junction/topology/junction_topology.cpp, `#include "junction_levels.h"` becomes `#include "../placement/junction_levels.h"`, and `#include "stdafx.h"` becomes `#include "../../stdafx.h"`.
  - Example from outside: in src/rail_gui.cpp, `#include "junction_gui.h"` becomes `#include "junction/tool/junction_gui.h"`.
  - Example from a test: in src/tests/junction/topology/junction_topology.cpp, `#include "../junction_topology.h"` becomes `#include "../../../junction/topology/junction_topology.h"`, and `#include "../stdafx.h"` becomes `#include "../../../stdafx.h"`.
- Keep the include ORDER and grouping exactly as it is. Change only the path strings. Write the rewrite as a script in your scratchpad, run it once, then grep for any remaining `"junction_` include that does not resolve.
- Doc comments that name a path, such as `@file`, keep the basename. Change nothing else in comments.

## CMake
- src/CMakeLists.txt: remove the junction_* lines from add_files, and add `add_subdirectory(junction)` in alphabetical position among the existing add_subdirectory lines.
- src/junction/CMakeLists.txt: `add_subdirectory(...)` for each of the 8 folders, then `add_files(...)` for the root's files. Each folder's CMakeLists.txt has one `add_files(...)` listing its files alphabetically, following src/pathfinder/yapf/CMakeLists.txt.
- src/tests/CMakeLists.txt: remove the junction_* lines and add `add_subdirectory(junction)`. src/tests/junction/CMakeLists.txt holds `add_subdirectory` for each test folder plus `add_test_files(...)` for its own files, including junction_compactness.{h,cpp}. Each test folder has one `add_test_files(...)`.
- Any conditional around the junction files in src/CMakeLists.txt is kept as it was. If one exists that you cannot carry over 1:1, stop.

## Docs (same commit)
- New docs/junction-tool/PATHS.md: a table mapping every old path to its new path (every moved, renamed and deleted file), with one line above it: "Docs written before MV1 cite the old flat paths; this table maps them."
- Update src paths in docs/junction-tool/STATUS.md (resume line only), TESTS.md and tools/ottd/README.md. Do not edit other docs, briefs, recon files or CLAUDE-draft.md.
- Patches under scratchpad/ or docs/junction-tool/wip/: do not touch them.

## Do not
- Rename any function, type or test, or change any Catch2 tag, test name, expected value or LEVELHASH.
- Split or edit any file's code, including junction_generator.h and junction_topology.cpp.
- Create CLAUDE.md files. That is the next step.
- Touch other worktrees, game clients, Pete's config or the rig servers.

## Stop conditions
- An include cycle or symbol that only compiles with a code change.
- A duplicate-basename error you cannot fix with the layout above.
- Any identity hash or planned count that differs.
- A test that fails after the move but passed before. Run the gate once BEFORE the move to get a baseline.

## Gates
1. Before the move: `tools/junction/jt gate .claude/nodes/build-rel` and `tools/junction/jt identity .claude/nodes/build-rel`. Record the pass and fail counts.
2. After the move: reconfigure, then build both `build` (Debug, the targets openttd and openttd_test) and `build-rel`, with no new warnings. Run `jt gate` and `jt identity` on build-rel with the same counts as the baseline. Run `jt fast .claude/nodes/build topology`.
3. `git grep -n '"junction_' -- src | grep -v '^src/junction/\|^src/tests/junction/'` lists only includes of the form `junction/...` or the widgets headers.
4. One commit: `Codechange: Junction tool files move into src/junction/ feature folders`, ending with the line `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read.
- Commit hash and subject.
- Baseline and after counts for gate, identity and topology.
- Files moved, deleted and renamed, as counts.
- Include lines edited, as a count from the diff.
- CMake files created or edited.
- Stops.
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
