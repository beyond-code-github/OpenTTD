# RO1 (read-only recon): a feature folder layout for the junction tool, following how OpenTTD organises src/

Read-only: write only the output doc. Keep your context small (grep/glob, offset/limit). Never touch any worktree under /home/pete/OpenTTD/.claude, any game client or the rig servers.

## State
- Repo /home/pete/OpenTTD, branch feature/junction-tool. The junction tool's ~90 files sit flat in src/ (src/junction_*.{h,cpp}, plus src/widgets/junction_widget.h and any other junction-related files such as line painter or junction station: find them all) and src/tests/junction_*.cpp. The engineer: "lets look at how this repo organises by feature and do the move - we should add some structure that aids with progressive disclosure and potentially more nested CLAUDE.md files. Partition by feature as makes sense."
- A draft of the tool's rules as present-tense facts is at docs/junction-tool/CLAUDE-draft.md (sections: pipeline, topology, graph/order/structure, bridges/tunnels, placement, heights, signals/prio, hover/GUI, game rules, testing, working rules); it will be split across the new folders' CLAUDE.md files.

## Questions
1. How OpenTTD organises src/ by feature today: subfolders (src/pathfinder/, src/linkgraph/, src/script/, src/network/, src/blitter/, src/video/, src/saveload/ ...): their CMakeLists.txt pattern (add_files, conditions), how files outside include them (`#include "pathfinder/yapf/..."`), how tests are laid out (src/tests/ flat or nested?), and whether any feature folder has a README or similar. Cite examples.
2. Inventory of the junction tool's files: each file, its lines, what it owns (one line), which pipeline stage or feature it belongs to, which other junction files it includes, and which NON-junction files include it (rail_gui.cpp, window_type.h, settings, viewport...).
3. A proposed layout: `src/junction/` with feature subfolders (e.g. topology/graph, placement, levels/terrain, render+signals, hover+GUI, game glue/build, station, line painter, legacy pipelines still compiled), and the tests' layout; per folder: files, a one-line purpose, and what its CLAUDE.md would hold (which sections of the draft). Keep folders cohesive (few cross-folder includes); flag files that should later be split (e.g. a 4000+ line junction_topology.cpp: name the natural split points by function, but the move itself does not split files).
4. The move's mechanics: git mv list, CMakeLists edits (new src/junction/CMakeLists.txt with add_subdirectory, removals from src/CMakeLists.txt; tests), include-path edits (count per file), the widgets header (OpenTTD keeps widgets in src/widgets/: keep it there or move?), lang strings (stay in src/lang/english.txt), and the risk points (generated tables, the tests' include of "../junction_*.h", tools/junction scripts referencing paths, docs referencing src paths).
5. Legacy: which junction files belong to superseded pipelines (old generator/router/nodes/organic/skeleton) and whether they are still used by the current pipeline or tests (so they can go to src/junction/legacy/ or be deleted later: list who calls them).

## Output
/home/pete/OpenTTD/docs/junction-tool/RECON-folder-layout.md (untracked, ~100 lines).

## Report
10 lines: OpenTTD's pattern, the proposed tree (top two levels), the CLAUDE.md placement, the move's size (files, include edits, CMake edits), the risks. When you have reported, stop.
