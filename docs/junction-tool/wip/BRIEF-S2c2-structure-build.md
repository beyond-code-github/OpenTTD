# BRIEF S2c2: BuildPlanarStructure (the variant list) with its outer test (S3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-structure, head HEAD_HASH (S2c1 landed: `PlanarCrossingSeparators`, `PlanarGroupStructures`, `PlanarStructureRun`, `BuildPlanarTrackMap(topology, channels, sites, runs)`). Release build build-rel. Logs: /tmp/claude-1000/-home-pete-OpenTTD/83ff9380-9534-4138-8dc1-79a833146e49/scratchpad/s2c2/. Tests print with WARN.
- Implements docs/junction-tool/wip/SPEC-structure.md §3 (output: variants ranked by cost), §6 (`BuildPlanarStructure`), §7 "S2c2 design" and S3. Rulings: D362 (variants live in Structure only), D347b (every channel crossing is a choice; main line over is the start), D364/D372 (a separator bit per mover), D371a (cost = structures + copies), D366 (no Shape code in Structure).

## Read first (once each, with offset/limit)
1. docs/junction-tool/wip/SPEC-structure.md §3, §6, §7.
2. src/junction/planar/planar_structure.{h,cpp}; src/tests/junction/planar/planar_structure.cpp (`WrapToday`: how a variant's structures and sites are built from runs and the map; `BuildMapOf`).
3. src/junction/planar/planar_regions.h (S2c1's functions).
4. src/junction/planar/planar_paths.cpp lines 40-60 (`PlanarOpenCrossings`) and 200-220 (how `over` comes from bits): READ ONLY; Structure must not call `BuildPlanarPaths`.
5. src/junction/planar/planar_components.cpp: grep -n "CountMappedBits" and read that function (how ties and triangles are counted) and `FlipPlanarTriangles`/`PlanarTriangles` declarations; src/junction/planar/planar_gaps.h (`ResolvePlanarSites`).

## Changes (planar_structure.{h,cpp} unless named)
1. `std::vector<uint32_t> PlanarChannelOver(const PlanarChannels &channels, uint64_t bits)` (planar_structure): every channel crossing is a bit, in crossing order. Bit clear: the prio channel is over if exactly one of the two is prio, otherwise channel `a`; bit set: the other one. (Bit clear = today's default and D347b's start.)
2. Move the structure and site building of the test's `WrapToday` into the library as file-local functions of planar_structure.cpp (same logic, doc-commented), so the test and `BuildPlanarStructure` share them; the test's `WrapToday` becomes a thin call.
3. `PlanarStructureResult BuildPlanarStructure(const PlanarTopology &topology, const JunctionSpec &spec, size_t keep)` (declared in planar_structure.h, doc-commented from SPEC §6 and this list):
   - Channels `BuildPlanarChannels(topology)`; per flip mask over `PlanarTriangles(channels)`: `FlipPlanarTriangles(channels, f)` and `PlanarLineSides(topology, flipped)`, each built once per flip and cached.
   - Per (flip, ties) with ties counted as `CountMappedBits` counts them: `ResolvePlanarSites`; a failure skips that (flip, ties) and is counted. Movers: `PlanarMovers(topology, flipped, sites)`; mover mask m selects separator movers (bit i = movers[i]).
   - Per variant (over bits o, ties t, flips f, movers m): `PlanarChannelOver`, `PlanarCrossingSeparators`, `PlanarGroupStructures`; cost = runs + runs with copy. A `why` from these skips the variant and is counted by its text. Tag `o{o}t{t}f{f}m{m}`.
   - Search: if (over bits + tie bits + flip bits + max movers over the tie combos) <= 16, every variant; otherwise best-improvement over one-bit neighbours (one over bit, one tie bit, one flip bit, one mover toggled from the current sites' movers) starting from o0 t0 f0 m0, until no neighbour lowers the cost; every variant evaluated is kept in the pool.
   - Output: from the pool, the `keep` lowest by (cost, tag); for each, build its track map from its runs; a map with a non-empty `why` is skipped and counted, and the next is taken; then its structures and sites (change 2). `why` on the result is empty unless no variant survives.
   - Add to `PlanarStructureResult`: `size_t evaluated = 0; std::map<std::string, size_t> skipped; bool exhaustive = false;` (doc-commented). No other new members.

## Tests (outside-in; red before green, record each failure line)
1. **Outer (S3), first, with a stub `BuildPlanarStructure` returning an empty result:** for T2, T3, LL2 with keep 16: `REQUIRE(!result.variants.empty())`; every variant keeps the promise (`CheckPlanarStructurePromise` empty, maximality included); costs non-decreasing in order; WARN per hub: bits per kind, exhaustive or not, evaluated, skipped by reason, kept, best and worst cost, best tag, and wall time (Release). Red: the REQUIRE with the stub.
2. `PlanarChannelOver`: with bits 0 on T2, the result equals what `BuildPlanarPaths(..., 0)` gives for `over` (call it in the TEST only, as an oracle of today's default); with one bit set, exactly that crossing's over changes.
3. Cost: on T2 o0t0f0m0 the cost equals runs + copies counted from the built map's structures.

## Do not
- Do not call `BuildPlanarPaths`, `CheckPlanarShape` or any composer from library code.
- Do not change S2c1's functions, the oracle, the composers, or existing tests' expectations.
- No ranking terms beyond D371a.

## Stop conditions
- Ties cannot be counted without `CountMappedBits`' composer-specific parts (then report what it needs).
- The movers differ between tie combos in a way the mask cannot address (report an example).
- Any variant handed down breaks the promise.
- Wall time over 5 s for any hub (D297): stop and report the counts.

## Gates
- Release build, zero warnings in touched files; `tools/junction/jt fast build-rel '[planarstructure]' 5` (tag the outer test `[slow]` if it alone exceeds 5 s, and say so), `tools/junction/jt fast build-rel '[planar]' 5`, `tools/junction/jt gate build-rel` within 30 s.
- Commit: `Add: Junction planar BuildPlanarStructure, the ranked variant list (S2c2)` with trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; functions, types and members added or moved (names); the outer test's WARN lines per hub; gates; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
