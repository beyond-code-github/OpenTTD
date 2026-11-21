# Brief X8: the C++ loaders read the extractor's PSG 197 output

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}` (the corpus branch is merged in: `tools/junction/corpus/out/psg197/` holds the extractor's `topology.txt`, `structure.txt`, `shape.txt`, `steps.txt`, `lengths.txt`). Work and commit only there.
- Goal: the extractor's 197 files load through the same C++ loaders as the hand-authored `tools/junction/topologies/psg197*.txt`, so later steps can use the extractor's names and coordinates. This step only measures and reports; it fixes no loader and no extractor.

## 2. Read first (once each)
- `tools/junction/corpus/README.md` (what each file holds).
- `src/tests/junction/planar/planar_draw.cpp`: `ReadTopologyFileForDraw`, `Shaped197`, the test "PSG 197 (Shape's own shape)".
- `src/tests/junction/planar/planar_shape_layer.cpp`: "V's shape of PSG 197 through Shape's checker (E1h)" (how a shape file is loaded and checked).
- The declarations of `ParsePlanarTopology`, `LoadPlanarStructureVariant`, `LoadPlanarShapeVariant`, `CheckPlanarShapePromise`, `PlacePlanarShapeFlex`, `RefinePlanarShapeCells`.

## 3. The test (hidden; no red/green: a measurement)
`planar draw: the extractor's PSG 197 loads (X8)`, tags `[.][planarpsg197]`, in `src/tests/junction/planar/planar_draw.cpp`. Files are read from `std::string(JUNCTION_TOPOLOGY_DIR) + "/../corpus/out/psg197/" + name` (a small static helper like `ReadTopologyFileForDraw`). Each stage WARNs its outcome and stops the test (`return`) on failure, so later stages report only when earlier ones pass:
1. `ParsePlanarTopology(topology.txt)`: the error, or the counts of ports, lines, nodes and crossings.
2. `LoadPlanarStructureVariant(topology, structure.txt, MakeTestJunctionSpec(), report, &error)`: the error, or the number of report lines by prefix (`gap:`, `differs:`, other) and the first 20 report lines.
3. `LoadPlanarShapeVariant(topology, variant, shape.txt, shaped, why)`: why, or success; then `CheckPlanarShapePromise` on it (the count, and the first 20 messages).
4. `PlacePlanarShapeFlex`, then `RefinePlanarShapeCells` on the loaded shape: the flex count, the dummies, the cells, why.
5. `CheckPlanarDrawable(shaped, 60.0, {.elastic = true})` (an LP): status, total slack, families (count and sum) and the first 30 stretched tags.
Also WARN the same five lines for the hand-authored files (`Shaped197`-style loading of `psg197.txt`, `psg197.structure.txt`, `psg197.shape.txt` with the same flex and cells steps), so the report shows both side by side.

## 4. Do not
- Change any loader, the extractor, the files under `out/`, or any other test.

## 5. Stop conditions
- Anything not covered. A load failure is NOT a stop: it is the result. Report it.

## 6. Gates
- Release `build-rel` -j 4, target `openttd_test` only (a GUI client runs from `build-rel/openttd`: do not rebuild or kill it). Run `build-rel/openttd_test "planar draw: the extractor's PSG 197 loads (X8)" > /tmp/claude-1000/x8.log 2>&1` (budget 5 minutes). Debug build and `tools/junction/jt fast build '[planardraw]'` to confirm nothing else changed.
- Commit the test: `Add: Junction test: the extractor's PSG 197 through the C++ loaders (X8)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report (short)
- References read; commit
- Per stage, extractor vs hand-authored: outcome and the key numbers; every failure message verbatim
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
