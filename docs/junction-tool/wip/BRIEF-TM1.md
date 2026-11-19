# Brief TM1: the track map, step 1 (pure, test-first)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Run `git switch -c wt-tracks wt-planar` (wt-planar is at {HEAD}); commit only on `wt-tracks`.
- Design: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §16 (read it whole, it is short). Background: `wip/REVIEW-architecture-tracks.md` Q1 and Q3; `wip/PROBE-RM.md`.
- Prior art: branch `probe-rm` (commit 81f0e6d49d) expands a topology with prio rows inside `PlanarLineSides` (`git diff wt-planar probe-rm -- src/junction/planar/planar_regions.cpp`). Reuse its split/merge/mirror steps; do not reuse its line-level exemption hack (this brief replaces it with per-stretch sides).
- Time box: 3 hours. Every test run uses a hard kill: `timeout -s KILL 60`.

## Read first (once each)
- `CODINGSTYLE.md`.
- `src/junction/planar/planar_regions.{h,cpp}` (whole): `PlanarMap`, `EventsInChannelOrder`, `PlanarLineSides`.
- `src/junction/planar/planar_topology.h`, `planar_channels.h` (`group_crossings`, `line_share`, `channel_of_line`, sites), `planar_gaps.h` (`PlanarResolvedSite`).
- `src/junction/planar/planar_slots.cpp` `BuildIntervals` (the copy rule: which lines get copies in which `x<k>`, and their side) and the copy run over `mid<k>` between consecutive doubled crossings.
- `src/tests/junction/planar/planar_slots.cpp` (`BuildVariant`: how a variant is built for a test) and `src/tests/junction/CLAUDE.md`.

## The type and function (in `planar_regions.{h,cpp}`, next to `PlanarLineSides`)
```cpp
/** One track of the track map: a topology line, a copy or a prio row. */
struct PlanarTrack {
	enum class Kind : uint8_t { Line, Copy, Row };
	Kind kind;
	uint32_t line; ///< The topology line it is, or runs beside.
	std::string name; ///< The line's name; a copy "<line>\"" plus its first crossing's index; a row "prio:<line>".
};

/** The hub's tracks as one planar map (DESIGN-tracks §16): the skeleton expanded with copies and rows, with sides per stretch. */
struct PlanarTrackMap {
	std::vector<PlanarTrack> tracks; ///< Lines first (same index as the topology's lines), then the parts lines are split into, then copies and rows.
	std::vector<uint32_t> part_of; ///< Per track, the topology line whose part it is (a line part), or UINT32_MAX for copies and rows.
	std::vector<std::vector<uint32_t>> events; ///< Per track, its crossings (indices into crossings) in travel order.
	std::vector<PlanarCrossing> crossings; ///< The expanded map's crossings (track indices).
	std::vector<PlanarFace> faces; ///< As PlanarLineSides::Faces, over tracks.
	std::string why; ///< Empty, or the expansion or validation failure.
	/** Side of track p's stretch ps relative to track q's stretch qs: +1 right of q's travel, -1 left, 0 when they share no face. */
	int8_t Side(uint32_t p, uint32_t ps, uint32_t q, uint32_t qs) const;
	std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, int8_t> side; ///< (p, ps, q, qs) -> side, filled from the faces.
	std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> conflicts; ///< Stretch pairs two faces give opposite sides.
};

/** Build the track map of a variant. @param joiner_over Inside prio joins: false, the crossed lanes double over the joiners; true, a full-share joiner doubles over the lanes. */
PlanarTrackMap BuildPlanarTrackMap(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<uint32_t> &over, bool joiner_over);
```
If the existing `PlanarMap` cannot be fed a topology with these tracks without a change to its validation, stop and report (do not rewrite `PlanarMap`).

## Expansion (one op, applied to a copy of `EventsInChannelOrder`'s topology)
**Parallel(L, first, last, side):** a track beside line L over L's events `first..last` (inclusive, indices into L's events; `last` may be "to L's end node"), on `side` (+1 right of L's travel).
- Split L at a new split node N just before event `first`, and, unless the range ends at L's end node, at a new merge node N2 just after event `last`. The parts are new tracks (`part_of` = L); the part keeping L's start keeps L's index.
- The new track T runs from N to N2 (or to L's end node M). N's out legs are ordered left to right across travel (T on `side`); N2's (or M's) in legs likewise, T next to L's part on `side`.
- For each crossing c of L in the range, with crosser X: add crossing (T, X); in T's events in the same order; in X's events immediately before or after c: before when X arrives from T's side of L (`CrossingFrom`-style: the side X comes from), after otherwise.
- Paths: L's parts replace L in every route path, in order. Ring ends: a part, and T, take L's ring ends for the crossing validation.

**What is expanded:**
1. **Copies:** for each channel c and each maximal run of consecutive crossings of c where `over == c`, each full-share line of c (the slot layer's rule) gets Parallel over that run's crossings, on the slot layer's copy side.
2. **Prio join internals:** for each site s of kind Join with `prio`, lanes S far..near and joiners J as `build_join` orders them, sigma = side: if `joiner_over` is false, each S[j] (j >= 1) gets Parallel over its crossings with J[0..j-1] on side sigma (its copy toward the joiners); if true, each full-share J[k] gets Parallel over its crossings with S[k+1..] on the side away from the lanes. (On T2, T3 and LL² no joiner is full share, so `joiner_over` adds no copies there.)
3. **Rows:** each lane S[k] of a prio join gets Parallel from just before its first crossing with any J (or, with none, its last event + 1) to its end node (the merge), on side -sigma.
Order: rows first, then join copies, then channel copies (a later Parallel splits whichever part holds its range). If a range falls across parts in a way not covered here, stop and report.

## Sides per stretch
Build the faces with `PlanarMap` over the expanded tracks. For every face and every two stretches (p, ps), (q, qs) on it, the face's side of q's stretch gives `side[(p, ps, q, qs)]`. When two faces give one stretch pair opposite sides, record it in `conflicts` unless p and q (or the topology lines they are parts of or run beside) cross. Do not set `why` for conflicts; the test checks them.

## Tests (write first, record each red failure text; `[junction][planar]` in a new `src/tests/junction/planar/planar_tracks.cpp`, registered in the tests' CMakeLists)
1. `"planar track map: every variant of T2, T3 and LL² validates in both forms"`: for every T2 and T3 variant (o 0..1, t 0..3, f 0..1; s does not matter), and LL² o0..63 with t 0..7 and f 0..1 limited to the variants whose `BuildPlanarPaths` succeeds, for `joiner_over` false and true: `why` empty, `conflicts` empty. Red: the function does not exist (compile error is acceptable as the first red only for this test; record it, then a stub returning `why = "stub"` and record that red).
2. `"planar track map: tracks"`: T2 o1t3f1s0 (`BuildVariant(PlanarTText(2), 1, 3, 1)` as in planar_slots tests): it has exactly one Row track per lane of each prio join (count: 4 on T2) and one Copy track per (full-share line, over run) (count from the slot layer's copy tracks in `x<k>` intervals over the same variant, merged over consecutive runs); every Row's crossings are with joiner lines only.
3. `"planar track map: joiner over at T2's E join gives J0, row, S1, J1"`: T2 o1t3f1s0 with `joiner_over` true. Let S1 be the E join's near lane, J0 and J1 its joiners, R its row: R's part after its crossing with J0 bounds a common face with J0's part after that crossing on one side and with S1's part after its crossing with J0 on the other, and S1's last stretch bounds a common face with J1. Assert through `Side` (R lies on the joiners' side of J0 after the crossing... state each expected sign in a comment derived from sigma). If expressing this needs anything beyond `Side` and `faces`, stop and report.
4. `"planar track map: fast"`: T2 and LL² o17t0f1: under 20 ms each in Release (`#ifdef NDEBUG`).

## Gates
- Debug `build`: `tools/junction/jt fast build '[planar]'` inside budget.
- Release `build-rel`: `tools/junction/jt gate build-rel`; failures by name.
- `git switch` stays on `wt-tracks` at the end; rebuild `openttd_test` in build-rel on it.

## Do not
- Change the composer, slot layer, `PlanarLineSides`' behaviour, the signal pass or the checker.
- Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Stop conditions
- `PlanarMap` validation needs changing.
- A Parallel range spans parts in a way not covered.
- Test 1 finds conflicts or validation failures: do not change the rules to make them pass; report the first three (variant, tracks, stretches, faces) and stop after committing what passes.

## Commit and report
- Commit on `wt-tracks`: `Add: Junction track map: the skeleton expanded with copies and rows, sides per stretch`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Update `src/junction/planar/CLAUDE.md` with a short "Track map" section (decided facts only).
- Report: references read; commit; red evidence per test; types and members added; suite results; stops and anything undone; `git status --porcelain` and `git log -1`.

When you have reported, stop.
