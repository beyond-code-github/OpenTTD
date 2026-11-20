# BRIEF X0b: Shape keeps each span's crossers parallel (H11, D376) (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD}. Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel (persistent; do not reconfigure).
- Implements: wip/SPEC-slots.md §0.1 finding (a) and §7 X0b; ruling D376 in docs/junction-tool/DECISIONS.md (read that row: grep `| D376 |`). SPEC-shape's mirrored-crossing rule (§2.1) is loosened for copy decks only.

## Terms (exact, use these)
- A structure `s`'s **separator span**: its listed crossings `s.crossings[k]` with crossers `s.crossers[k]`, deck track `s.separator`.
- Its **copy span** (only when `s.copy != UINT32_MAX`): every crossing `c` in `map.events[s.copy]`, in that order, whose `MirrorOf(c)` is one of `s.crossings`; its crosser is the track of `c` that is not `s.copy`; deck track `s.copy`.
- A **copy-deck crossing**: a crossing in some structure's copy span. Every other crossing that no structure lists is a **plain mirrored crossing** and keeps today's rule (its original's angle, the crosser on one piece across both).
- A span's **width**: n + d, where n is its crossing count and d = 1 when any of its crossings has angle 1 or 3 (diagonal), else 0. With H11 every crosser of a span is parallel, so this is H6's "k at 90 degrees, k + 1 on one diagonal" (antiparallel diagonals share tiles too).

## Read first (once, with offset/limit)
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_shape_layer.h (whole).
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_shape_layer.cpp: the oracle class (lines ~140-600: `EventHeading`, `StructureOf`, `Arrival`, `Angle`, `Least`, `ChainEvents`, `MirrorOf`, `CheckH4`, `CheckH5H6`, `CheckH8`, the check sequence ~585), and `ShapePlanarStructureVariant` (~596-925: crossing rows, mirrored loop, capacity rows, the `PLANAR_H11` block, output assembly).
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp: the tests at lines 132-260 (form of oracle tests), "planar shape: BuildPlanarShape on T2, T3 and LL2" (~476) and the sweep probe (~507).
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/CLAUDE.md, section "Shape layer".
- CODINGSTYLE.md (C++ part).

## Tests, outermost first
1. **Outer (red first):** the existing `[slow]` test "planar shape: BuildPlanarShape on T2, T3 and LL2" runs the oracle on every variant. Do changes 1-3 (oracle only) first, build, run it by exact name and record: it must fail with `H11:` messages (count them per hub; SX0 measured 48/80, 67/158 and 32/64 mixed structures). Then changes 4-6 (program), and it must pass.
2. **New fast test**, after "planar shape layer: H4 a mirrored crossing with another angle than its original": `TEST_CASE("planar shape layer: H11 a span whose crossers are not parallel", "[junction][planar][planarshape]")`: build `BuildPlanarStructure(PlanarTText(2), spec, 16)`; shape each variant with `ShapePlanarStructureVariant` **after** changes 4-6 and CHECK the oracle is empty on each; then, on the first variant that has a structure with at least two separator-span crossers, set that structure's second crosser's heading at its crossing to a value not equal mod 4 to the first's by editing `shaped.tracks[crosser].pieces[event_piece]` by +2, and REQUIRE that the oracle's messages include one starting `H11:` (other messages, e.g. H1 or H4, may appear; the test checks only that H11 is among them). Red evidence for this test: run it before change 3 exists (with changes 1-2 only) and record that no `H11:` message appears.
3. The existing "H4 a mirrored crossing with another angle than its original" and every other `[planarshape]` test stay green unchanged.

## Changes
Oracle (planar_shape_layer.cpp, the oracle class):
1. Add `std::vector<uint32_t> CopyCrossings(const PlanarStructure &s) const` (the copy span's crossings in the copy's travel order, empty without a copy) and `const PlanarStructure *CopyDeckOf(uint32_t c) const` (the structure whose copy span holds c, or nullptr). Replace `Least`'s run model with the width above: `bool Least(uint32_t deck, const std::vector<uint32_t> &crossings, const std::vector<uint32_t> &crossers, int &least) const` (angles by `Angle(c, deck, crosser)`; false when any is outside 1..3); the separator span calls it with `s.separator, s.crossings, s.crossers`; the copy span with `s.copy`, `CopyCrossings(s)` and their crossers.
2. `CheckH4`: for a crossing no structure lists, when `CopyDeckOf(c)` is non-null, check it like a listed crossing against the copy as its separator (angle from the headings in 1..3, stored angle equal to it; messages as the listed branch's); otherwise today's mirrored check. `CheckH5H6`: H6 checks `middles` against the separator span's least and, new, `copy_middles` against the copy span's least (each per allowed kind, -1 for a kind not allowed, all -1 without a copy; each least within `s.limit[kind]`). `CheckH8`: skip only the "sits on another piece of ... than its parent chain's" check when the beside track's crossing `eb[k]` is a copy-deck crossing.
3. New `void CheckH11()`, called after `CheckH8()`: per structure, per span with at least two crossings, the crossers' headings at their crossings (`EventHeading`) reduced mod 4 must all be equal; else one message `H11: structure <i> <separator|copy> span crosses at headings <list>`.
Types (planar_shape_layer.h):
4. `PlanarShapeStructure` gains `std::array<int, 2> copy_middles; ///< The least middle tiles of the copy's deck per kind at its chosen angles (-1 for a kind not allowed, or no copy).`
Program (`ShapePlanarStructureVariant`):
5. Mirrored loop: a copy-deck crossing gets its own `angle[c] = var(1, 3, 0, true)` and the crossing row against the copy, exactly as the listed loop builds it with `p = s.copy` (side from `o.Arrival(c, s.copy, crosser, side)`, tag `cross <c>`; no axis row, the copy's headings already equal the separator's); no `mirror` row. Plain mirrored crossings: unchanged.
6. Capacity: replace the run rows with, per span (separator span; copy span when the structure has a copy) and only when some `limit[kind] >= 0`: one binary d; per crossing two rows `a_k - 2 - d <= 0` and `2 - a_k - d <= 0`; per kind with `limit[kind] >= 0` the row `n + d <= limit[kind]`; tag `cap <i>`. H11 rows, per span with at least two crossings: for k >= 1, `H[crosser_k](at c_k) - H[crosser_0](at c_0) - 4 m = 0` with `m = var(-16, 16, 0, true)`, tag `h11 <i>`. Delete the `PLANAR_H11` block. Output: `shape.middles` from the separator span's `Least`, `shape.copy_middles` from the copy span's (per allowed kind; -1 otherwise).
Docs:
7. src/junction/planar/CLAUDE.md "Shape layer": (a) in the oracle bullet, "H1-H9" becomes "H1-H9 and H11 (each span's crossers parallel: headings equal mod 4; D376)" and add: "a copy-deck crossing (on a structure's copy, mirroring one of its crossings) is checked against the copy like a listed crossing and its crosser may sit on another piece than at the original"; (b) in the program bullet, the mirrored-crossing sentence gains: "except a copy-deck crossing, which has its own angle and crossing row against the copy, so a doubled bridge's two decks may take different angles (D376)"; (c) replace the "Capacity rows" bullet with: "- Capacity and H11 rows, per span (the separator's deck; the copy's deck when there is one): the crossers' headings at their crossings equal mod 4 (H11, D376); one binary d >= |a_k - 2| per crossing; per kind with `limit[kind] >= 0`, n + d <= limit (k crossers take k tiles at 90 degrees, k + 1 on one diagonal). The limit is Structure's (`PlanarStructure::limit`); `middles` and `copy_middles` are the two decks' least widths. The objective is the Line tracks' turning only (no angle preference, SPEC-shape §2.1)."; (d) delete the `PLANAR_H11` line; (e) in the `BuildPlanarShape` bullet, update the three hubs' times to the ones you measure; (f) in planar_shape_layer.h line ~54, "H9 (P1 to P7)" becomes "H9 (P1 to P8)", and the header's H list names H11.

## Run
- Build: `cmake --build /home/pete/OpenTTD/.claude/ng1/build-rel --target openttd_test -j16 > /home/pete/OpenTTD/.claude/ng1/build-rel/x0b-build.log 2>&1`; grep for `error`.
- The outer slow test by exact name: `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 600 ./openttd_test "planar shape: BuildPlanarShape on T2\, T3 and LL2" > x0b-outer.txt 2>&1` (Catch2 splits names on commas, hence the escape); grep `SHAPE`, `H11:`, `FAILED`, `passed`.
- After green: the sweep probe by exact name: `timeout 900 ./openttd_test "planar sweep: Structure and Shape over T1-T3\, LL2 and LL3a" > x0b-sweep.txt 2>&1`; keep the summary SWEEP lines and `shape_cost_sum` lines.
- Fast gate: `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`, and `'[planarstructure]' 10`.

## Do not
- Change Structure, the track map, `ParentChain`, the face or route rows, or the objective.
- Add an angle preference, a flag, or any tolerance; change any existing test's expectation.
- Run the wide gate or any other heavy job.

## Stop conditions
- After changes 4-6 any T2, T3 or LL2 variant is unshaped, or the oracle reports anything but nothing: report the variant, `why` or messages verbatim, and stop (do not loosen a row).
- A copy span's crossing whose `Arrival` against the copy gives no side.
- A sweep `closes` count below `shaped` on any hub.
- Any existing test turns red.

## Gates and commit
- Outer slow test green; `[planarshape]` and `[planarstructure]` fast green inside 10 s each; sweep: c2_bad=0 and closes = shaped on T1, T2, T3, LL2.
- One commit on wt-slots: `Add: Junction planar Shape keeps each span's crossers parallel (H11, D376)` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short, this format)
- References read
- Commit: hash and subject
- Red evidence: the outer test's H11 count per hub before the program change; the new fast test's result before change 3
- Types and members added or removed: names only
- Suite results: per run, cases, passed, time; the outer test's SHAPE lines (hub, variants, ms); the sweep's summary and shape_cost_sum lines
- Stops, anything left undone
- State: `git status --porcelain` and `git log -1 --oneline`
When you have reported, stop.
