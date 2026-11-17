# Brief 2a: gap steps from the channel graph (test-first)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, head `{HEAD}`. Build dir `build-rel` (Release, already configured).
- Plan: `docs/junction-tool/DESIGN-pieces.md` §3 source 3 and §7 step 2 (main checkout `/home/pete/OpenTTD/docs/junction-tool/`), decision D335 in `DECISIONS.md` there.
- This step adds a pure function and its tests. The composer (`planar_components.cpp`) is NOT changed.

## 2. Read first (once, with offset/limit)
- `/home/pete/OpenTTD/.claude/ng1/CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_channels.h` (whole, 75 lines), `src/junction/planar/planar_topology.h` lines 19-95 (PlanarSide, PlanarLine, PlanarPort, `ring`).
- `src/junction/planar/planar_components.cpp` lines 733-736 (`PlanarSitePerNode`), 826-835 (`PortHeading`, `TurnOf`), 1107-1131 (how the composer resolves each site's straight channel, other channel, side and prio, tie bits included). Your `ResolvePlanarSites` reproduces lines 1107-1131 exactly.
- `src/tests/junction/planar/planar_paths.cpp` lines 1-40 and 64-75 (includes, `ChannelIndex` helper, how a T topology is parsed).
- `src/junction/planar/CMakeLists.txt`, `src/tests/junction/planar/CMakeLists.txt`, `src/junction/planar/CLAUDE.md`.

## 3. What the function computes (the rule)
At each side of the box the port lines sit at fixed spacing (a pinned gap between neighbours). Walk `topology.ring` (port lines clockwise from the NW corner). For each consecutive pair `ring[i], ring[i+1]` (no wrap-around) whose ports are on the same `PlanarSide` and whose channels differ (`channels.channel_of_line`), look at it from both channels: X = one, Y = the other.
- **Where Y lies for X**, from X's port line: X's line is an in line (`line.from.port`) → the ring successor lies to X's left (-1), the predecessor to its right (+1). X's line is an out line (`line.to.port`) → successor right (+1), predecessor left (-1).
- **X is an in channel** (its ring line is an in line): take the resolved site with `kind == Fray` and `channel == X` (if none, or per-node, nothing). If its `side` equals where Y lies, X's fray sends a leg into the gap:
  - fray prio (the resolved `prio`) → step `{X, fan = true, Y}`;
  - else, Y's ring line is an out line → step `{Y, fan = false, X}`;
  - else → return empty steps and `why = "a plain fray's leg runs toward a neighbouring in port"`.
- **X is an out channel**: take the resolved site with `kind == Join` and `channel == X`. If its `side` equals where Y lies, a joiner lands in the gap → step `{X, fan = false, Y}`.
- Drop duplicates (same `channel` and `fan`; keep the first). Sort by `channel`, then `fan` (false first).

## 4. Types and functions (new files `src/junction/planar/planar_gaps.h` and `.cpp`, file header and `@file` comment as in planar_paths.h)
```cpp
/** A site with its straight channel decided: the composer's view of a two-channel split or join. */
struct PlanarResolvedSite {
	uint32_t straight = UINT32_MAX; ///< UINT32_MAX for a site composed per node.
	uint32_t other = UINT32_MAX;
	int8_t side = 0; ///< Where the other channel leaves or arrives: +1 right of travel, -1 left (from its turn).
	bool prio = false; ///< The straight channel carries a prio route.
};

/** A sideways step a pinned gap at the box edge needs (DESIGN-pieces §3 source 3). */
struct PlanarGapStep {
	uint32_t channel; ///< The channel that steps.
	bool fan; ///< True: on the in channel's fray fan; false: on the out channel's close-up after its join.
	uint32_t neighbour; ///< The channel across the pinned gap.
};

std::vector<PlanarResolvedSite> ResolvePlanarSites(const PlanarTopology &topology, const PlanarChannels &channels, uint64_t ties, std::string &why);
std::vector<PlanarGapStep> BuildPlanarGapSteps(const PlanarTopology &topology, const PlanarChannels &channels, const std::vector<PlanarResolvedSite> &sites, std::string &why);
```
- `ResolvePlanarSites`: one entry per `channels.sites`, same order; a copy of composer lines 1107-1131 as a free function (per-node sites keep the defaults; tie bits consumed in site order exactly as there; `PortHeading`/`TurnOf` become `static` helpers in the .cpp). Where the composer calls `M.Fail("a site's channels have no single port heading")`, set `why` to that text and return an empty vector.
- `BuildPlanarGapSteps`: the rule of §3. `why` stays empty on success.
- Add both files to `src/junction/planar/CMakeLists.txt` beside planar_paths.

## 5. Tests (new file `src/tests/junction/planar/planar_gaps.cpp`, registered in its CMakeLists; tag `[junction][planargaps]`)
Helper in the file: channel of a line name (copy `ChannelIndex` from planar_paths.cpp's test). For each test: parse `PlanarTText(n)`, `BuildPlanarChannels`, `FlipPlanarTriangles(channels, 1)`, `ResolvePlanarSites(..., ties, why)` (REQUIRE why empty), `BuildPlanarGapSteps` (REQUIRE why empty). Compare against exactly three steps, written in the test by line names and sorted the same way:
- `{channel of EF1, fan = true, neighbour = channel of CB1}` (W: EF's prio fray sends F toward CB, its fan takes the gap)
- `{channel of AF1, fan = false, neighbour = channel of CD1}` (S: AF's close-up)
- `{channel of ED1, fan = false, neighbour = channel of AB1}` (E: ED's close-up)
1. **"planar gaps: T3 o0t1f1 steps"**: n = 3, ties = 1.
2. **"planar gaps: T2 o0t2f1 steps"**: n = 2, ties = 2.
3. **"planar gaps: resolved sites of the T"**: n = 3, ties = 1: for each site that is not per node, `straight` and `other` are the two `others`, `side` is ±1; the EF fray's straight channel is E's (`channel of E1`) and its `prio` is true; the ED join's other is D's channel with side +1.

**Red first:** write the header, a `.cpp` whose two functions return `{}` (empty why), and the tests; build; run; record the failure messages (tests 1-3 must fail on their assertions, not on compile errors). Then implement.

## 6. Do not
- Do not touch `planar_components.cpp`, `planar_paths.*`, `planar_channels.*` or `planar_model.*`. The composer keeps its own copy of the resolution for now.
- Do not add LL² expectations, per-node sites, or any step kind beyond §3.
- No env knobs, no debug printing.

## 7. Stop conditions
- Tests 1 or 2 give a step set other than the three named after a faithful implementation of §3: stop, report the computed set (channel by its first line's name, fan, neighbour) for both, and the resolved side of every site. Do not adjust the rule.
- Anything in the composer's lines 1107-1131 that does not carry over to a free function as described.

## 8. Gates
- `tools/junction/jt fast build-rel '[planargaps]'` (budget 5 s) and `tools/junction/jt fast build-rel '[planarmodel]'`, `tools/junction/jt fast build-rel '[planar]'`.
- Update `src/junction/planar/CLAUDE.md` with the rule as present-tense facts (no decision ids), in the same commit.
- One commit: `Add: Junction planar gap steps from the channel graph`, body a short line on what it computes, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 9. Report (short)
References read; commit hash and subject; red evidence (each test and its failure message before the code); types and functions added; suite results with count and time per gate; stops or anything undone; `git status --porcelain` and `git log -1 --oneline`. When you have reported, stop.
