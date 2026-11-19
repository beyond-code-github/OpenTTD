# BRIEF S1: Structure's promise oracle, then the gap on today's variants

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-structure, head HEAD_HASH. Builds: build (Debug) and build-rel (Release); both are stale (the worktree was on another branch), so the first build is incremental but long: run it once, in the background of nothing else. Logs: /tmp/claude-1000/-home-pete-OpenTTD/83ff9380-9534-4138-8dc1-79a833146e49/scratchpad/s1/.
- Implements: docs/junction-tool/wip/SPEC-structure.md §4 (promise P1-P7), §6 (output types), §7 step S1; DESIGN-tracks §17.1; rulings D364, D369, D371, D371a. The design is the source of truth, not today's code (D368): today's code is only measured against the promise here.

## Read first (once each, with offset/limit)
1. docs/junction-tool/wip/SPEC-structure.md (whole; short).
2. src/junction/planar/CLAUDE.md lines 1-70 ("Architecture", "Terms", "Topology").
3. CODINGSTYLE.md (skim the naming and doc-comment rules).
4. src/junction/planar/planar_regions.h lines 80-115 (`PlanarTrack`, `PlanarTrackMap`, `BuildPlanarTrackMap`); src/junction/planar/planar_topology.h lines 25-90 (`PlanarLine`, `PlanarRoute`, `PlanarCrossing`, `PlanarTopology`).
5. src/tests/junction/planar/planar_tracks.cpp lines 1-80 (`TracksVariant`, `BuildTracksVariant`: how today's variants are built: flips, ties, over bits) and its CMakeLists.txt registration.
6. src/junction/junction_spec.h lines 30-60 (`tl`, `max_bridge_middle`, `max_tunnel_length`).

## Files
- New: src/junction/planar/planar_structure.h and planar_structure.cpp (add to the planar CMakeLists the way planar_regions.cpp is added); src/tests/junction/planar/planar_structure.cpp (register like planar_tracks.cpp), tag `[planarstructure]`.
- The header declares exactly the types of SPEC §6: `PlanarStructureSite`, `PlanarStructure`, `PlanarStructureVariant`, `PlanarStructureResult`, and the function `CheckPlanarStructurePromise`. Do NOT declare or implement `BuildPlanarStructure` in this step. Doc-comment every type and member from the SPEC's wording.

## The oracle: `CheckPlanarStructurePromise(topology, spec, v)` returns one message per breach, each starting with its promise id ("P3: ...")
- **P1:** `v.map.why` is empty (the track map's own planarity and face validation); every crossing of `topology` maps to exactly one crossing of `v.map` between tracks that are (parts of) its two lines; no track lists the same crossing twice in `v.map.events`.
- **P2:** `v.map.conflicts` is empty.
- **P3:** every crossing of `v.map` is in exactly one `PlanarStructure`; within a structure, every crossing involves `separator`, and the crossings are consecutive entries of `v.map.events[separator]` in that order; `crossers` are the other tracks of those crossings in the same order.
- **P4:** for a crossing c of structure S (separator X, crosser Y): if Y is the separator of a structure T and c lies within T's span on Y (its index in `v.map.events[Y]` is between T's first and last crossing's indices, inclusive), then one of S, T must allow bridge and the other tunnel; with bridge-only kinds that is a breach.
- **P5:** every structure has at least one kind; for kind bridge, `crossers.size() <= spec.max_bridge_middle`; for kind tunnel, `<= spec.max_tunnel_length` (one middle tile per crosser at the most compact angle). The D228a flow bound is NOT checked in this step: say so in the report.
- **P6:** a structure's `copy` is set exactly when its separator is a full-share line (the track map's own notion of share: find how `BuildPlanarTrackMap` decides a copy and use the same test), and then it is a `PlanarTrack::Kind::Copy` track beside the separator; every Copy track of the map is some structure's copy; every Row track's line is a lane of a prio join.
- **P7:** on every route path of `topology`, no split node follows a merge node, ignoring the copy and row splits and merges the map adds (they are not in `topology`).

## Tests, in this order (red before green for each)
1. **Oracle on a valid hand case:** pick the smallest topology text you find in src/tests/junction/planar/ that has one crossing (or write one with two lines crossing once, from the notation in planar CLAUDE "Topology"); build its track map with `BuildPlanarTrackMap` (one flip/tie/over choice); fill a `PlanarStructureVariant` by hand: one structure, separator the over track, crosser the other, kinds bridge, copy as the map has it, sites from the map's nodes. Assert the oracle returns empty. Red first: write the test before implementing the oracle body (a stub returning {"not implemented"}), record the failure.
2. **One mutation test per promise P2-P6,** each starting from test 1's variant and changing one field so only that promise breaks; assert exactly one message, starting with that id. Examples: P3, drop the crossing from the structure; P5, set kinds to 0; P6, set copy to UINT32_MAX where the map has a copy (or set one where it has none); P4, add a second structure on the crosser spanning the same crossing with bridge kind; P2, push one entry into `map.conflicts`. P1 and P7: one test each if a one-field mutation breaks only them (P1: a duplicate entry in `map.events`; P7: a hand topology text whose route path merges then splits, if the parser accepts it; if not, say so).
3. **Characterisation (tag `[planarstructure][slow]` if over 5 s Release):** for T2, T3 and LL2 (`PlanarTText(2)`, `PlanarTText(3)`, `PlanarLL2Text()`), enumerate today's variants as `BuildTracksVariant` does (copy that helper into the new test file; do not include the other test file), for both `joiner_over` values; wrap each into a `PlanarStructureVariant` with TODAY's implicit grouping: one structure per map crossing (separator = the track on the over channel per `over`; for crossings the map adds for copies and rows, report them as such and leave them out of structures), kinds bridge, copy the map's copy beside that separator if any, sites from the map's split and merge nodes. Run the oracle. The test PRINTS a tally per hub and form: variants, and per promise the number of variants breaching it, plus the first message of each kind. It asserts nothing about the counts (characterisation); it only REQUIREs that every variant was built.

## Do not
- Do not change any existing source file except the two CMakeLists registrations.
- Do not implement grouping, separators per joiner, forced splits, cost or ranking (that is S2).
- Do not make the oracle lenient to pass today's variants.

## Stop conditions
- A promise cannot be checked from the fields the SPEC types give (say which, and what is missing).
- A mutation breaks more than one promise and no single-field mutation isolates it.
- The build fails outside your files.

## Gates
- Release build of openttd_test (log to file; zero warnings in your files).
- `tools/junction/jt fast build-rel '[planarstructure]' 5` (tags need brackets); then `tools/junction/jt gate build-rel` green within its 30 s budget.
- Commit: `Add: Junction planar Structure promise oracle (S1)` with trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read
- Commit hash and subject
- Red evidence per test (the failure line before the code)
- Types and functions added (names only)
- The characterisation tally (copy the printed lines)
- Gate results and times
- Stops, and what was left unchecked (D228a at least)
- State: `git status --porcelain`, `git log -1 --oneline`

When you have reported, stop.
