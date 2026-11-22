# BRIEF domain-2a: a drawing's domain input from its variant

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall in total. Every test here runs in well under 1 s; no solves. No rerun loops: if a test fails for a reason you cannot name in two looks, stop and report.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>` (given at launch). Commit there; do not merge.
- Plan: `/home/pete/OpenTTD/docs/junction-tool/wip/CARD-domain-rules.md`, design point 2 (the encoding check needs each drawing judged by the domain). Ruling D428. Step 1 (the predicate) is in: `src/junction/planar/planar_domain.h`.
- This step: a function that turns a shaped variant plus a drawing's points into a `PlanarDomainInput` (nodes and structures read from the variant's track map, never from the drawing program's rows). Step 2b, the enumerator, uses it.

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (repo root), the C++ part.
- `src/junction/planar/planar_domain.h` (whole) and `src/tests/junction/planar/planar_domain.cpp` (its helpers' style).
- `src/junction/planar/planar_structure.h` lines 20-75 (`PlanarStructure`: `separator`, `crossers`, `kinds` (1 bridge, 2 tunnel), `copy`, `limit`; `PlanarStructureVariant`: `map`, `structures`).
- `src/junction/planar/planar_regions.h` lines 95-110 (`PlanarTrackMap`: `tracks`, `expanded` (its `lines` are the tracks in the same order, its `nodes` have `in`/`out` as track indices), `crossings` (track pairs `a`, `b`)).
- `src/junction/planar/planar_topology.h` lines 30-60 (`PlanarLine`, `PlanarNode`).
- `src/junction/planar/planar_shape_layer.h` lines 55-66 (`PlanarShapeVariant::structure`) and 185-195 (`LoadPlanarShapeVariant`).
- `docs/junction-tool/wip/probes/domain/x1-probe.cpp.txt` (main checkout): the orchestrator's probe that loads the X1 texts below through `ParsePlanarTopology`, `LoadPlanarStructureVariant` and `LoadPlanarShapeVariant`; copy its loading lines, not its drawing part.

## 3. The tests first (file `src/tests/junction/planar/planar_domain_map.cpp`, tags `[junction][planar][planardomain]`)

The X1 configuration (use these texts verbatim; the probe shows they load):
- topology: `topology X1` / `port W in E: AC1` / `port S in N: BD1` / `port N out N: C1, B1` / `port E out E: A1, D1` / `node AC1 -> C1, A1` / `node BD1 -> B1, D1` / `cross A1: B1<` / `cross B1: A1>` (one statement per line).
- structure: `tunnel T1 A1: B1` / `straight AC1: A1` / `straight BD1: B1`.
- shape: `shape AC1: 2` / `shape A1: 2` / `shape C1: 1 0` / `shape BD1: 0` / `shape B1: 0` / `shape D1: 1 2` / `at A1: B1@0` / `at B1: A1@0`.
- Do not call `PlacePlanarShapeFlex`. Map tracks to names by `v.structure.map.expanded.lines[t].name` (do not assume an order).

A hand drawing of X1 on the game grid (half tiles; per track its points), legal by the rules of `CheckPlanarDomain`:
- AC1: (0,5) (4,5)
- A1: (4,5) (20,5)
- C1: (4,5) (7,2) (7,0)
- BD1: (11,12) (11,10)
- B1: (11,10) (11,0)
- D1: (11,10) (12,9) (20,9)

### Test 1 (outer): "planar domain: a drawing's input from its variant"
1. `PlanarDomainInputOf(v, points, notes)` on the hand drawing: `REQUIRE` 6 tracks (each equal to its points); 2 nodes: at (4,5) with `in` = {AC1}, `out` = {C1, A1} (as sets), at (11,10) with `in` = {BD1}, `out` = {B1, D1}; 1 structure: separator A1, crossers {B1}, `tunnel` true, `first` 2 and `last` 4 (A1's pieces on tiles (4,2) and (6,2); B1 passes tile (5,2)), `limit` = the structure's `limit[1]`; `notes` empty.
2. `CheckPlanarDomain` on it: legal, no breaks.
3. The hand drawing with B1, BD1 and D1 shifted by -6 in x (B1 at x = 5, nodes still meeting): B1 now meets A1 on A1's piece 0, so the structure is left out with exactly one note containing "no room for a head", and `CheckPlanarDomain` gives a rule set that contains "diamond" (B1 crosses A1 at grade). Assert the note and that "diamond" is in the set (not an exact set: the shifted D1 may break more).

Red: with `PlanarDomainInputOf` returning an empty input, part 1 fails on the track count; record the message.

## 4. The changes

New files `src/junction/planar/planar_domain_map.h` and `.cpp`, added to `src/junction/planar/CMakeLists.txt` (alphabetical, after `planar_domain`); the test file added to `src/tests/junction/planar/CMakeLists.txt`.

```cpp
/** @file planar_domain_map.h A drawing of a shaped variant as the domain's input (D428): nodes and structures read from the track map, never from the drawing program's rows. */

/**
 * The domain input of a drawing.
 * @param v The shaped variant the drawing belongs to.
 * @param points Per track of v's map, its points (as PlanarDrawResult::points).
 * @param notes Receives one line per structure left out, and why.
 * @return The input: every track's points as given; the nodes; the structures.
 */
PlanarDomainInput PlanarDomainInputOf(const PlanarShapeVariant &v, const std::vector<std::vector<std::array<int, 2>>> &points, std::vector<std::string> &notes);
```

Rules, in this order:
1. **Tracks:** `input.tracks = points`.
2. **Nodes:** for each node of `v.structure.map.expanded.nodes`, its point is the last point of its first `in` track that has a point. Nodes with equal points merge into one (union of `in`, union of `out`). From each merged node drop every track whose points are all equal (a zero-length track, e.g. a trident's middle line), from both lists. Emit the merged nodes in order of their first expanded node.
3. **Structures:** for each `PlanarStructure s` of `v.structure.structures`, one `PlanarDomainStructure` with `separator = s.separator`, `crossers = s.crossers`, `tunnel = (s.kinds & 1) == 0`, `limit = s.limit[tunnel ? 1 : 0]`. When `s.copy != UINT32_MAX`, a second one with `separator = s.copy` and `crossers` = every track c with a crossing {s.copy, c} in `v.structure.map.crossings` (either order), same kind and limit.
4. **Span from the geometry:** raster the separator and every crosser with `RasterPlanarDomainTrack`. The middles are the separator's pieces whose tile holds a piece of any crosser; `first` = the least such index minus 1, `last` = the greatest plus 1. Leave the structure out, with a note `structure <k> (<separator name>): <why>`, when no crosser piece meets a separator tile ("no crosser meets its separator"), when the least index is 0 or the greatest is the last piece ("no room for a head"), or when a raster has an error ("raster: <rule>"). Do not check straightness or limits here: the domain does.

## 5. Do not
- Read no row, option or result field of the drawing program; include nothing from `planar_draw`, `planar_slots`, `planar_layout`.
- Do not change `planar_domain.h/.cpp`.
- No enumerator, no hub runs, no env hooks.

## 6. Stop conditions
- The X1 texts fail to load, or the map's tracks are not the 6 lines named above (e.g. a copy track appears): stop and report.
- The hand drawing is reported illegal: stop and report the breaks; do not change the points or a rule.
- Anything not covered.

## 7. Gates
- `tools/junction/jt fast build '[planardomain]' 5` (Debug).
- `tools/junction/jt gate build-rel` (Release).
- Add to `src/junction/planar/CLAUDE.md`, directly after the **Domain** bullet: `- \`PlanarDomainInputOf\` (planar_domain_map.h) gives a drawing's domain input from its variant's track map: nodes at one point merged without their zero-length tracks, one structure per Structure structure and per copy deck, its heads one piece outside the separator pieces its crossers meet.`
- One commit: `Add: Junction: a drawing's domain input from its variant (D428)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report (short)
References read; commits; red evidence; types and members added; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
