# BRIEF E1d-2: load a pinned Structure output (PSG 197) into a PlanarStructureVariant

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-e1, head HEAD_HASH (check with `git log -1`). Release build `build-rel` there (incremental only).
- **Load: build with `-j 4`, run jt with `JT_JOBS=4`.** The machine crashed twice today at 14 jobs. Never run two builds or test runs at once.
- Card: docs/junction-tool/wip/CARD-E1d-psg197-variant.md (the goal: Shape receives 197's variant; the oracle checks it first; a breach is a data error or a model gap, reported, never worked around). Rulings D389, D390 (DECISIONS.md).
- Budget: 75 minutes in total; any single test run over 60 s is a stop.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part).
- src/junction/planar/planar_structure.h (whole) and planar_structure.cpp lines 60-200 (`StructureLimit`, `PlanarBuildStructures`, `PlanarBuildStructureSites`) and 300-330 (how `BuildPlanarStructure` assembles a variant).
- src/junction/planar/planar_regions.h lines 95-160 (`PlanarTrack`, `PlanarTrackMap`, `PlanarStructureRun`, `BuildPlanarTrackMap`); planar_gaps.h lines 10-40 (`PlanarResolvedSite`, `ResolvePlanarSites`).
- tools/junction/topologies/psg197.structure.txt (the file to load; its grammar is below) and the first 20 lines of tools/junction/topologies/psg197.txt.
- src/tests/junction/planar/planar_structure.cpp lines 1-60 and 140-200 (helpers, `MakeTestJunctionSpec`), and src/tests/junction/planar/planar_topology.cpp's `AuthoredText` helper and its JUNCTION_TOPOLOGY_DIR file read.
- src/junction/planar/CLAUDE.md section "## Structure".

## The file grammar (one statement per line, `#` comments)
- `tunnel T<n> <separator line>: <crosser line>, ...`: one structure of kind tunnel; its crossers in travel order along the separator.
- `group G<nn>: T<n>, ...`: parallel tunnels (information for Shape v2; parsed, ids checked, not stored).
- `copy <line> over T<n>, ...`: a copy of `<line>` over those tunnels.
- `straight <trunk line>: <leg line>` or `straight <trunk line>: none`: at the node whose trunk (a split's in line, a merge's out line) is that line, the straight leg, or none (a fork).

## The function (planar_structure.h / .cpp, after `BuildPlanarStructure`)
```
/**
 * A Structure output written down (D389): load pinned decisions into one variant, through the same builders BuildPlanarStructure uses.
 * <file grammar, as above, one bullet each>
 * @param topology The topology; its crossings must be authored (`crossings_authored`).
 * @param text The decisions.
 * @param spec The spec: the tunnel and flow limits, the side gaps.
 * @param report Receives one line per decision the model cannot take ("gap: ...") and one per straight leg that differs from Structure's own rule ("differs: ...").
 * @param error Receives why the text is malformed; the result is then empty.
 * @return The variant, tagged with the topology's name.
 */
PlanarStructureVariant LoadPlanarStructureVariant(const PlanarTopology &topology, std::string_view text, const JunctionSpec &spec, std::vector<std::string> &report, std::string *error);
```
Body, in this order (malformed input sets `*error` and returns `{}`; each message names the statement):
1. `!topology.crossings_authored`: error "the topology's crossings are not authored".
2. Parse every statement. Errors: unknown statement word; an unknown line name; a tunnel id given twice; a group or copy naming an unknown tunnel id; a tunnel whose separator and a crosser are not a crossing of the topology ("tunnel T<n>: <sep> does not cross <crosser>"); a `straight` whose trunk is the trunk of no topology node, or whose leg is not a leg of that node; a topology node with no `straight` statement ("node with trunk <line> has no straight statement").
3. Runs: one `PlanarStructureRun{separator line, the topology crossing indices of (separator, crosser) in the statement's order, copy}` per `tunnel`, in file order. `copy`: for each tunnel it names, when the copy line is that tunnel's separator set the run's `copy = true`; otherwise push `fmt::format("gap: copy {} over T{}: the model copies only a structure's separator (D351)", line, n)`.
4. `const PlanarChannels channels = BuildPlanarChannels(topology);` `std::string why; const std::vector<PlanarResolvedSite> sites = ResolvePlanarSites(topology, channels, 0, why);` A non-empty `why`: push `"gap: sites: " + why` and return the variant built so far (tag and side gaps set, the rest empty).
5. `v.map = BuildPlanarTrackMap(topology, channels, sites, runs);` a non-empty `v.map.why`: push `"gap: track map: " + v.map.why` and return.
6. `v.structures = PlanarBuildStructures(topology, runs, v.map, spec);` then for each structure `kinds = 2` and `limit = StructureLimit(spec, st)`. If the count differs from the runs: push `fmt::format("gap: {} tunnels gave {} structures", runs.size(), v.structures.size())`.
7. `v.sites = PlanarBuildStructureSites(topology, channels, sites, v.map);` then for every topology node n (`n < topology.nodes.size()`, the same index in the expanded map): the pinned leg's track is the leg l of `v.map.expanded.nodes[n]` (its `out` for a split, `in` for a merge) with `v.map.tracks[l].line` equal to the pinned line; pinned `straight` = that track and `side` = +1 when it is the node's first leg (index 0), else -1; `none`: `UINT32_MAX` and 0. Before overwriting, when Structure's own choice (the built site's straight) differs, push `fmt::format("differs: straight {}: rule {}, pinned {}", trunk name, rule leg's line name or "none", pinned leg's line name or "none")`. A pinned leg with no matching track: push `"gap: straight <trunk>: leg <leg> has no track at the node"` and leave the site as built.
8. `v.tag = topology.name; v.side_gap = PlanarSideGaps(spec); v.cost = runs + runs with copy.` `group` statements are checked in step 2 and otherwise unused.

## Tests (src/tests/junction/planar/planar_structure.cpp), outermost first; record each red before the code
1. **"planar structure: PSG 197's pinned variant, gaps and promise"** tags `[junction][planar][planarstructure]`. Read both files from `JUNCTION_TOPOLOGY_DIR` (`psg197.txt`, `psg197.structure.txt`), parse, load with `MakeTestJunctionSpec()`. REQUIRE the parse and load errors are empty. Then print with `WARN` (not fail): every report line, then every line of `CheckPlanarStructurePromise(topology, spec, v)`, then counts: tracks, structures, sites, report gap lines, differs lines, promise breaches. This is the characterisation; it must pass while gaps remain.
2. **"planar structure: PSG 197's pinned variant keeps the promise"** tags `[.][planarpsg197]` (hidden, not in the gate): REQUIRE no "gap:" line and `CheckPlanarStructurePromise` empty. Expected red until the gaps close; record its first failure.
3. **"planar structure: a pinned variant equal to Structure's own gives its structures and sites"** `[junction][planar][planarstructure]`: authored T at one lane (`AuthoredText(PlanarTText(1))`: copy that helper into this file as a static function, unchanged); `BuildPlanarStructure(topology, MakeTestJunctionSpec(), 1)`'s top variant v0; write its decisions as text (a `tunnel T<i>` per structure: separator and crossers by the topology line of each track, `map.part_of` or `tracks[].line`; a `straight` per topology node from v0.sites, `none` for UINT32_MAX); load it. REQUIRE an empty report and error; the same number of structures, each with the same separator line, crosser lines and copy as v0's (kinds differ by design: tunnel); the same straight track and side at every site.
4. **"planar structure: pinned decisions are refused with a reason"** `[junction][planar][planarstructure]`, on test 3's text, one SECTION each: topology not authored (`PlanarTText(1)` derived): "not authored"; an unknown statement: "unknown"; a tunnel naming a non-crossing pair: "does not cross"; one `straight` line removed: "has no straight statement".
Expected red for tests 1, 3, 4 before the function exists: a link or compile failure is not acceptable red; first add the declaration and a stub that sets `*error = "not implemented"` and returns `{}`, then record the red messages.

## Do not
- Do not change `BuildPlanarStructure`, the builders, `CheckPlanarStructurePromise`, the data files or any other layer.
- Do not add a member to `PlanarStructureVariant` or any other type.
- Do not close any gap the report names; this step only reports them.

## Stop conditions
- Test 1's load returns an error on the real files (report it verbatim).
- An existing test changes result.
- A red that fails for another reason than the one named.

## Gates and commit
- `cmake --build /home/pete/OpenTTD/.claude/ng1/build-rel -j 4 --target openttd_test > <scratch>/build.log 2>&1`; grep `error|warning`.
- `JT_JOBS=4 /home/pete/OpenTTD/.claude/ng1/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarstructure]' 30`, then `JT_JOBS=4 .../jt gate /home/pete/OpenTTD/.claude/ng1/build-rel` (at 4 jobs the gate reads about 31 s against its 30 s budget: report the time and pass count; over budget alone is not a stop for this gate run).
- Run test 2 once by name: `/home/pete/OpenTTD/.claude/ng1/build-rel/openttd_test "planar structure: PSG 197's pinned variant keeps the promise"` and keep its output.
- src/junction/planar/CLAUDE.md "## Structure": add one bullet: `- A Structure output can be written down (D389): `LoadPlanarStructureVariant` loads pinned tunnels, copies and straight legs through the same builders into one variant, reporting each decision the model cannot take ("gap:") and each straight leg that differs from the rule ("differs:"); PSG 197's is tools/junction/topologies/psg197.structure.txt.`
- One commit: `Add: Junction planar structure: load a pinned Structure output (D389)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push or merge.

## Report (short)
- References read
- Commit: hash and subject
- Red evidence: each test and its red message
- Types and members added: names only
- Test 1's printed output, in full (report lines, promise breaches, counts); test 2's first failure line
- Suite results: `[planarstructure]` count and time; gate count and time
- Stops and anything left undone
- State: `git status --porcelain`, `git log -1 --oneline`

When you have reported, stop.
