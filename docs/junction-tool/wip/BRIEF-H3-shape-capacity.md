# BRIEF H3: Structure hands down limits; Shape's capacity rows and BuildPlanarShape

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-shape, head HEAD_HASH. Builds: build (Debug), build-rel (Release), current. Logs: /tmp/claude-1000/-home-pete-OpenTTD/315c4ccd-306d-453c-be89-5dda9a8e71c5/scratchpad/h3/.
- Implements: docs/junction-tool/wip/SPEC-shape.md §1 (Structure's added limits), §2.1 Capacity and Cost (no angle preference), §6 (`limit`, `BuildPlanarShape`, `PlanarShapeResult`), §7 steps H3 and H4; D373(2) (ranking), D366. Only one heavy job at a time: never run two builds or test runs at once.

## Read first (once each, with offset/limit)
1. docs/junction-tool/wip/SPEC-shape.md §1, §2.1 (Capacity, Cost), §6, §7.
2. src/junction/planar/planar_structure.h (whole) and in planar_structure.cpp: `PlanarBuildStructures` and the P5 part of `CheckPlanarStructurePromise`.
3. src/junction/planar/planar_shape_layer.h and .cpp (the program's crossing rows and objective; the oracle's H6 and `Least`).
4. src/tests/junction/planar/planar_shape_layer.cpp (the t3 test and the probe).

## Commit A: Structure hands down each structure's limit (SPEC-structure §3 refined by SPEC-shape §1)
- `PlanarStructure` gains `std::array<int, 2> limit;` doc: "The most middle tiles each kind may span (0 bridge, 1 tunnel), -1 for a kind not allowed: bridge min(max_bridge_middle, and with a copy 2 * sd + tl - 2 (D228a)); tunnel min(max_tunnel_length, and with a copy the same D228a bound)." `PlanarBuildStructures` fills it from the spec (it takes the spec as a new last parameter; update its callers). Kinds stay bridge only (D365).
- P5 in `CheckPlanarStructurePromise` additionally checks `limit` equals that formula (message "P5: structure i has limit ..., the spec gives ..."), and checks the crosser count against `limit` instead of recomputing.
- Test (red first): a mutation in the Structure tests: set `limit[0]` to the wrong value on a hand case, expect only P5.
- Commit: `Add: Junction planar Structure hands down each structure's limit (H3)`; update the Structure section of src/junction/planar/CLAUDE.md.

## Commit B: capacity rows, no angle preference, BuildPlanarShape
- **Outer test first (red):** "planar shape: BuildPlanarShape on T2, T3 and LL2" (tag `[planarshape][slow]` if over 1 s Debug): for each hub, `BuildPlanarShape(BuildPlanarStructure(topology, MakeTestJunctionSpec(), 16))`; REQUIRE `variants.size() == 16`, `unshaped.empty()`; every variant's oracle empty (H1-H9, H6 included); ranking: for consecutive variants (structure.cost, cost, tag) non-decreasing lexicographically. Print per hub the time and each variant's tag, structure cost and shape cost. Red: before `BuildPlanarShape` exists, a stub returning an empty result.
- **`PlanarShapeResult`** gains `std::vector<std::pair<std::string, std::string>> unshaped;` (tag, why), doc "Variants handed down that got no shape: each is a bug (SPEC-shape H10)". **`BuildPlanarShape(PlanarStructureResult structure)`** shapes every variant with `ShapePlanarStructureVariant`, keeps the shaped ones ranked by (structure.cost, cost, structure.tag), records the others in `unshaped`; `why` set only when none shaped.
- **Capacity rows** in the program, per structure and per kind with `limit[kind] >= 0`: for its crossers in order k = 0..n-1 with angle a_k (the crossing's a_c; a mirrored crossing's is its original's): binaries b1_k, b2_k, b3_k with b1_k + b2_k + b3_k = 1 and a_k = b1_k + 2 b2_k + 3 b3_k; run starts r_k >= 0 continuous with r_0 >= b1_0 + b3_0 and, for k >= 1, r_k >= b1_k - b1_{k-1} and r_k >= b3_k - b3_{k-1}; row: n + Σ r_k <= limit[kind]. (This is H6's width: k crossers at 90 degrees take k tiles, a run of k at the same diagonal angle k + 1.)
- **Objective:** remove the Σ |a_c - 2| term (SPEC-shape §2.1 Cost: no angle preference); the objective is the Line tracks' turning only.
- **Oracle H6:** compare against `v.structure.structures[i].limit[kind]` instead of the spec (Structure owns the limit now; the spec stays a parameter for H9).
- **The t3 test:** now requires the oracle to return empty (remove the H6 allowance and its comment).
- Update src/junction/planar/CLAUDE.md "Shape layer": the capacity rows, the objective, `BuildPlanarShape` and its ranking.
- Commit: `Add: Junction planar Shape capacity rows and BuildPlanarShape (H3, H4)`.

## Do not
- Do not change the map, the separators, the grouping or any Structure rule other than `limit` and its P5 check.
- Do not relax a row: a variant that will not shape with capacity rows is recorded in `unshaped` and is the answer (report its why and IIS tags); the outer test then fails, and that is a stop to report, not to work around.

## Stop conditions
- Any variant unshaped after commit B (give tag, why, IIS tags).
- An oracle breach on any shaped variant.
- The build fails outside your files.

## Gates (both commits)
- Release build (zero warnings in your files); `tools/junction/jt fast build '[planarshape]' 5` and `tools/junction/jt fast build '[planarstructure]' 5`; `tools/junction/jt gate build-rel` within budget (report the time; it stood at 26.9 s of 30 s). Run the outer test and the probe once each in Release, logs to files; copy the outer test's printed lines and the probe lines into the report.
- Trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` on both commits.

## Report
- References read; commits (hash, subject); red evidence; types, members and functions added or removed (names only); printed lines; gate results and times; stops; state (`git status --porcelain`, `git log -2 --oneline`).

When you have reported, stop.
