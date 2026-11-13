# IC1 (read-only recon): icons for the new rail-toolbar tools (junction, junction station, line painter) and window buttons (engineer, 2026-11-13)

Read-only: change no source file; write only the output doc. Keep your context small. Never start a game client or touch the engineer's game or rig servers.

## State
- Main repo /home/pete/OpenTTD at feature/junction-tool: read there; do not build. Other agents work in .claude/nodes and .claude/nodes2: never touch them.
- The engineer: "we need to find better icons for our new tools still". The rail toolbar (src/rail_gui.cpp, src/widgets/rail_widget.h, `_nested_build_rail_widgets`) has buttons WID_RAT_BUILD_JUNCTION, WID_RAT_BUILD_JUNCTION_STATION and the line painter (find its widget); their sprites and cursors are set there (e.g. `GetRailTypeInfo(_cur_railtype)->cursor.autorail`, `SPR_CURSOR_RAIL_STATION`). The junction window's Rotate/Mirror/Pick train buttons are text (QL1 found no existing rotate/mirror sprite; SPR_CLONE_TRAIN means clone).

## Questions
1. What sprites each new tool button and cursor uses today (file:line), and what the neighbouring vanilla buttons use.
2. Candidate existing base-set sprites (src/table/sprites.h and the GUI sprite ranges) that could stand for: a junction (split/merge of tracks), a junction station, a line painter (multi-track drag), rotate, mirror/flip, pick a vehicle. For each: the sprite name/id, where vanilla uses it, and whether its meaning would clash.
3. How OpenTTD adds new GUI sprites in practice: extra sprites via the baseset's extra GRF (openttdgui.grf / the `extra` sprite list, src/table/files.h or similar, SPR_OPENTTD_BASE ranges), which files change, how many sprites at which zoom levels and bit depths (8bpp + 32bpp?), and whether a GRF/PNG source must be built with nml/grfcodec in this repo's build (find the build step for openttdgui.grf: media/baseset/openttd/*.nfo/*.png?). Effort estimate in steps.
4. A recommendation: which tools can use existing sprites now, which need custom ones, and the smallest path to custom ones.

## Output
docs/junction-tool/RECON-tool-icons.md (untracked, ~60 lines).

## Report
10 lines: current sprites, usable existing candidates, how custom sprites get in, recommendation. When you have reported, stop.
