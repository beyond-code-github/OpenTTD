# BRIEF recon-rig-v2: from a v2 drawing to the game (read-only)

You execute this brief and nothing else. It is read-only: change no file except the one output file named below; build nothing, run no game, server or client, touch no worktree under `.claude/`. If a question cannot be answered from the code, say so in the output; do not guess. Keep your context small: read with offset/limit, search with scoped grep, never cat whole large files.

Budget: 40 minutes. Output: `/home/pete/OpenTTD/docs/junction-tool/wip/RECON-rig-v2.md`, at most 100 lines, every claim with `file:line`.

## Why
The engineer: "we can't really judge anything by a drawing that we can't place." The v2 drawing pipeline (Slots stage A -> contraction -> stage B -> polish; `src/junction/planar/planar_draw.{h,cpp}`, probe tests in `src/tests/junction/planar/planar_domain_encoding.cpp` tag `[slotsstep2]`) ends in points per track (half tiles) that the domain judge (`CheckPlanarDomain`, `planar_domain.h`; input from `PlanarDomainInputOf`, `planar_domain_map.h`) accepts. We want to build T1's and T2's drawings in a running game (the rig) in a human company. Find the shortest path.

## Read first
- `src/junction/planar/CLAUDE.md`: the "Domain" bullets at the end of the Drawing program section.
- `tools/ottd/README.md` (the rig tooling).

## Questions
1. The domain suite's game cases (`[planardomaingame]`, `src/tests/junction/game/junction_game.cpp`): which function turns a domain input into game commands (BuildRail, bridges, tunnels, signals?), what exactly it takes as input, and which command each kind of piece becomes. Does it set heights, ramps, bridge heads and tunnel portals, or does it only build on a flat test map? Does it build onto a given map/company, or a fresh test map only?
2. The domain rasteriser (proven equal to `RasterizeJunctionRoute`): its function name and what it produces (tiles and track bits per tile?).
3. From a `PlanarDrawResult` of the step 2b probe to a domain input: the call chain and where in the probe test it already happens (line numbers), and whether the probe can already write the drawing to a file (`dump_path`, `PLANAR_DUMP`, `planar_dump.py`): which format, which fields.
4. The rig: how earlier layouts reached the running server (`tools/ottd`, `tools/ottd/layouts`, any JSON format such as `ud-T2-base.json` under docs/junction-tool/wip, and the scripts under `docs/junction-tool/wip/scripts/`): the layout file format, the command that builds it into a company on a running server, and whether it handles bridges, tunnels and heights.
5. The gap: list, in order, what is missing to go from the step 2b probe's final drawing to a built hub in the rig (for example: a writer from points to the rig format; heights for bridge decks and tunnels; signals), and for each, the existing code that comes closest (`file:line`). Say which of these the v1 path (`RenderPlanarRoutePrograms`, `planar_plan`) already solves.

## Output format
One section per question, short bullets, `file:line` on every claim; then a "Shortest path" section: the ordered steps you would take, each naming the existing function it reuses. No recommendations beyond that.

When you have written the file, report its path and five lines of summary, then stop.
