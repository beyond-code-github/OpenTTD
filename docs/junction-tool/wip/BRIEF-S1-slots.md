# Brief S1: the slot layer as production code (`BuildPlanarSlots`), no composer change

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar` at {HEAD}. Commit on `wt-planar`.
- Design: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §15 (read §15.1 and §15.3 step 1). Ruling: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` row D350. Docs live only under `/home/pete/OpenTTD/docs/junction-tool/`.
- Reference implementation: the spike test on branch `spike-slots` at f4d1ef0e49, file `src/tests/junction/planar/planar_slots_spike.cpp`. Read it with `git show f4d1ef0e49:src/tests/junction/planar/planar_slots_spike.cpp`. Do NOT check out that branch. The production model is exactly the spike's model as of that commit. Port it; do not redesign it.

## Read first (offset and limit)
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/CLAUDE.md`.
- `src/junction/planar/planar_channels.h`, `planar_gaps.h` (`PlanarResolvedSite`, `ResolvePlanarSites`) and `planar_paths.h`.
- `src/junction/planar/planar_gaps.{h,cpp}`, as the style model for a small pure planar module with its test.
- `src/junction/planar/CMakeLists.txt`, `src/tests/junction/planar/CMakeLists.txt` and `src/tests/junction/planar/planar_gaps.cpp` (test style).

## The tests (write first, see them red, record the failure text)
New file `src/tests/junction/planar/planar_slots.cpp`, tag `[junction][planar]`. Each test builds its variant the way the spike does: parse, `FlipPlanarTriangles` with the variant's flips, `ResolvePlanarSites` with its ties, and `BuildPlanarPaths(...).over` with its over bits.

1. **"planar slots: T2 o1t3f1s0 gap changes".** For each channel, the gap change between each pair of neighbouring lines must equal the expected value below. A line's net step is the sum of its steps between consecutive intervals, plus the step of the site link into it. The gap change of a pair (left, right) is the net step of right minus the net step of left. The objective is 50.

   Expected, by channel and lines:

   | Pair | Expected |
   |---|---|
   | EF1/EF2 | +1 |
   | CB1/CB2 | -1 |
   | ED1/ED2 | -1 |
   | AB1/AB2 | +1 |
   | CD1/CD2 | 0 |
   | AF1/AF2 | 0 |
   | A1/A2 | 0 |
   | B1/B2 | 0 |
   | C1/C2 | 0 |
   | D1/D2 | 0 |
   | E1/E2 | 0 |
   | F1/F2 | 0 |

   Identify channels by their line names, not by index.
2. **"planar slots: LL² o17t0f1s0 gap changes".** Same check, objective 53:

   | Pair | Expected |
   |---|---|
   | EF1/EF2 | +1 |
   | WO1/WO2 | 0 |
   | EO1/EO2 | 0 |
   | AB1/AB2 | +1 |
   | CD1/CD2 | +1 |
   | SO1/SO2 | 0 |
   | E1/E2 | -1 |
   | F1/F2 | 0 |
   | A1/A2 | 0 |
   | B1/B2 | -1 |
   | C1/C2 | 0 |
   | D1/D2 | 0 |
   | EDj1/EDj2 | 0 |
   | CBj1/CBj2 | 0 |
   | AFj1/AFj2 | 0 |

   This test also covers one-track intervals: LL² has single-line channels.
3. **"planar slots: tracks".** On T2 o1t3f1s0:
   - each lane of the two prio join straight channels (B and E) has a row track in `post`, on the side away from the joiners;
   - each over crossing interval of B has a copy track beside each line;
   - every channel's least slot is 0;
   - in every interval, the tracks' slots strictly increase left to right.
4. **"planar slots: fast".** T2 o1t3f1s0 and LL² o17t0f1s0 each build in under 20 ms (Release; use `std::chrono::steady_clock`).

Red: before the code exists, the test file must fail to link or compile against the missing header. Then add a stub `BuildPlanarSlots` that returns an empty result with `why` set, so the tests fail on assertions. Record the assertion failure text.

## The code
New files `src/junction/planar/planar_slots.h` and `src/junction/planar/planar_slots.cpp`, registered in `src/junction/planar/CMakeLists.txt`. Test registration goes in `src/tests/junction/planar/CMakeLists.txt`.

```cpp
/** What a track in a channel interval is. */
enum class PlanarSlotKind : uint8_t {
	Line, ///< A topology line.
	Copy, ///< A doubled line's copy over a crossing.
	Row, ///< A prio row beside its lane (D349).
};

/** One track of an interval. */
struct PlanarSlotTrack {
	PlanarSlotKind kind;
	uint32_t line; ///< Into PlanarTopology::lines: the line, or the line it copies or protects.
	int slot; ///< Sideways position in lane spacings, left to right across travel; per channel, the least slot is 0.
};

/** One interval of a channel: pre, x<k>, mid<k> or post. */
struct PlanarSlotInterval {
	std::string name;
	std::vector<PlanarSlotTrack> tracks; ///< Left to right.
};

/** A straight line's step across a site. */
struct PlanarSlotLink {
	uint32_t from; ///< The line before the site.
	uint32_t to; ///< The line after it.
	int step; ///< Slot change, after minus before.
};

/** The slot layer of one variant (DESIGN-tracks §15). */
struct PlanarSlots {
	std::vector<std::vector<PlanarSlotInterval>> channels; ///< Per channel, its intervals in travel order.
	std::vector<PlanarSlotLink> links;
	double objective = 0;
	std::string why; ///< Empty, or why no slots were found.
};

PlanarSlots BuildPlanarSlots(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, const std::vector<uint32_t> &over);
```

- **Slots:** a channel's slot values within its intervals are comparable. Normalise so the least slot over all of a channel's intervals is 0. That makes the free offset `o(channel)` disappear from the output.
- **Solve:** port the spike model exactly, including the D312 far rule as the spike computes it, the prio-fray gap of 2, rows in `post`, copies in over `x<k>`, port pins, charged site links, the steps-plus-widths objective, and no width row for a one-track interval. Solve it with `JunctionMilp` (`src/junction/placement/junction_milp.h`), integer variables as in the spike.
- **Failure:** on a non-Optimal status, set `why` to `"no slots: <status>"` and return. Do not port the spike's deletion filter, its forcedness pass or its file output.
- The spike file stays on its branch. Do not add it to wt-planar.
- Update `src/junction/planar/CLAUDE.md` in the same commit with present-tense facts about the slot layer: what it computes, its inputs and outputs, and that the composer does not read it yet. No decision references. Follow the file's style.

## Do not
- Change the composer, `planar_components.cpp`, or any existing behaviour.
- Add env knobs, dumps or a forcedness pass.
- Change weights or rules to make an expectation pass. If an expectation fails with the exact spike model, stop and report the actual against the expected values.

## Stop conditions
- Any expected value fails with the spike model ported exactly.
- A rule needs data that the four inputs do not give.

## Gates
- Debug: `tools/junction/jt fast build '[planar]'` (budget per docs/junction-tool/TESTS.md).
- Release: `tools/junction/jt gate build-rel`.
- Commit: `Add: Junction planar slot layer (BuildPlanarSlots, not yet read by the composer)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- references read;
- the commit hash and subject;
- the red evidence: the failure text per test;
- types and members added;
- gate results with counts and times, naming any failure you did not cause;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
