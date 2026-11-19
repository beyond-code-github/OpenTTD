# Brief PD (analysis, throwaway): what the engineer changed in T3, line by line

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `probe-sl` (build-rel/openttd_test is built from it). Create `probe-pd` from it for the temporary print; commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes. Test runs `timeout -s KILL 60`, one at a time.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Inputs
- The solver's T3 (variant o1t3f1s0, area 1796.25): `/home/pete/OpenTTD/docs/junction-tool/wip/t3-1796-o1t3f1s0.json` (layout JSON, coordinates relative to its north corner; it was built in the game with its north corner at tile (100, 8)).
- The engineer's hand edit of that build (he removed empty space; signals not redone): `/home/pete/OpenTTD/docs/junction-tool/wip/pete-t3-demo.json` (a `map_export` of the box x 95..200, y 0..70: `tiles` with absolute `x`, `y`, `tracks`, `tile_type`, `kind`). The changed tiles lie in x 113..150, y 9..22; everything else is the same.
- Helper scripts (read, may copy): `<S>/py/diff.py` and `<S>/py/glyph.py` (usage: `python3 glyph.py <layout.json> <export.json> x0 x1 y0 y1`; glyphs: `\` X track, `/` Y track, `-` Upper/Lower, `|` Left/Right, `+` several, `=` bridge).

## The engineer's own account
"I also altered the turns after the bridge the they turn immediately upon exit - this is what allowed me to move the crossings". In item 3, check this tile by tile: for each bridge in the window, the first tile after the bridge end and where the line's first turn is, solver vs engineer. In item 4, name the rules that hold a line straight after its bridge end (deck tail length, head lengths, the doubled copy's rejoin shape, curve rows, deck ends on whole pieces, anything else).

## Task
1. **Label the solver's tracks by line.** Add a temporary print (env `PLANAR_LINEDUMP=<path>`) in the test `"planar: dump a mapped component layout"` (or where `PlanarComponentLayout` route programs are available after the solve): for every route program piece, write one line `x y heading kind line` with the piece's tile shifted exactly as the layout JSON is, and `line` the topology line name of that piece (`PlanarRouteProgram` pieces carry a line index into the path; map it to the line name); also each prio row piece with line `prio:<lane>`. Run it: `PLANAR_ONLY=o1t3f1s0 PLANAR_LINEDUMP=<S>/pd-lines.txt JUNCTION_COMPONENTS_TOPOLOGY=T3 JUNCTION_COMPONENTS_OUT=<S>/pd-T3.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s`. Check `<S>/pd-T3.json` equals the input layout JSON's tracks (if not, stop and report).
2. **Label the engineer's tiles.** Tiles he kept carry the solver's labels. For his new tiles (present in his export, absent or different in the solver's layout, offset (100, 8)), follow connectivity from the kept tiles at both ends to assign each new tile run to the line(s) it continues.
3. **Describe the change line by line** in the changed window: which lines he moved (their old and new tile paths, as from/to points and headings), where each crossing in the window sat before and after (which line over which, tile), where each line's turns are before and after, and how many tiles each line gained or lost in length. Render two labelled ASCII maps of the window x 108..152, y 6..26 (one letter per tile: the first letter of the line, digits for lane; or a legend), solver and engineer.
4. **Which composer rules does his version break?** Read `src/junction/planar/planar_components.cpp` (bundles `Bundle` pieces, `bundle offset`, decks and `Cross`/`Deck` head lengths, the n-lane `Join`/`Fray`, the side rules) and `src/junction/planar/planar_paths.cpp` (crossing pieces); for each difference, name the rule (file:line, short quote) that keeps the solver from producing the engineer's geometry, or say none found. Facts only.

## Do not
- Change the composer, slot layer, paths, signal pass or checker. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit the print on probe-pd: `Probe: Junction per-line piece dump (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/ANALYSIS-PD.md` (do not commit): items 1-4 with both maps.
- Reply with items 3 and 4 and the two maps, the commit hash, `git status --porcelain`.

When you have reported, stop.
