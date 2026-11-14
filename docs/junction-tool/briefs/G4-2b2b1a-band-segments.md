# G4 2b2b1a: A4, band edges' cuts, segments and bend roles (D265, D273; RECON-O3 Q3, findings 4-6)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
O3 (DESIGN-G4 §3, 2b2b2) separates two band edges that are neighbours in a band, segment by segment. This step adds the static data it reads: for every band edge, the cuts along its program and the segments between them, and the role of every bend. It is data only, built in `ArrangeJunctionTrackGraph` from the graph, the order and the programs. Placement does not read it yet; nothing else moves.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`: run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-2b2b1a <HEAD given at launch>`. Leave its untracked files alone.
- `build-rel` (Release) exists there. Delete stale `.pch` files if the build complains.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2b2b1a/`.
- Another agent works in `.claude/o4c2` on placement; you touch only the files named here. Build only `openttd_test`. Never start a game client.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `docs/junction-tool/DECISIONS.md`, rows D255, D256, D258, D260, D265, D273.
- `docs/junction-tool/DESIGN-G4-static.md` lines 54-70 (§1, A4) and 357-379 (§11.4).
- `docs/junction-tool/RECON-O3-bands.md` Q3 (the T edge 4 example, the counts).
- `src/junction/topology/junction_topology.h` lines 86-160 (graph, row items, band events, order) and 200-302 (steps, roles, ends, node shapes, programs, the arrangement).
- `src/junction/topology/junction_topology.cpp` from line 1341 (`ArrangeJunctionTrackGraph`) to its end.
- `src/tests/junction/topology/junction_topology.cpp` lines 5397-5420 (`ArrangementTopology`, `CheckArrangementSeed`) and 5689-5720 (the large-hub counts test).

## Definitions
- **Band edge.** A graph edge that appears in some line of `order.band_lines`.
- **Its direction.** North when it starts at an S port (an `In` edge from a port slot) or when its end node's level is less than its start node's; south when it ends at an S port or the end node's level is greater. If both ends are nodes of equal level, it stays in one band. If it is none of these, stop and report.
- **Cuts**, in travel order along the edge's copy 0 program (`programs.edges[e]`; band edges are never doubled; stop if one has a non-empty `copies[e]`):
  - `Start` and `End`: the edge's two ends. `event` is the index in `order.bands[b]` of the node's band event when the end is a band node (level 2b), else `NO_EVENT`. `token` is the end's token from `programs.ends[e]` (may be `NO_TOKEN`).
  - `Crossing`: one per crossing of this edge with another band edge (`graph.crossings`), at the token of this edge that names the other edge (Over, Under, Tunnel or Above with `index` equal to the other edge). `event` is that crossing's index in `order.bands[b]`. With D258 one run passes under several crossings; each still gets its own cut.
  - `Row`: one per Under token whose `index` is not a band edge (a row edge's doubled bridge), at that token. `event` is `NO_EVENT`; `row` is the order row of the edge overhead (the row whose in, leg or out edge it is: the row of `order.rows` holding that edge's node, read as the arrangement's `unders` already do; if you cannot read it there, stop and report).
  - `Free`: one per free bend (below), at its token.
- **Segments.** The pieces of the program between two consecutive cuts, as `[first_cut, last_cut]` indices into the edge's cut list. A segment's `band` is the band it lies in: the band of a band-node end or crossing cut that bounds it; otherwise from the direction: after the edge's last Row cut of row r it is in band r when running north, band r + 1 when running south; before its first Row cut, the band of its Start; between two Row cuts of the same row it lies on the row's item rows and its band is `NO_BAND`. If two rules give different bands for one segment, stop and report.
- **Bend roles.** Every bend token (Fan, Corner, SBend) of a band edge gets exactly one role:
  - `Start` / `End`: it serves the edge's end: it is `programs.ends[e].first.token` or `.second.token`, or its role is `Diverge` (Start) or `Converge` (End), or it is a Fan at an S port end (Start for an In edge, End for an Out edge).
  - `Crossing`: its step role is `Cross`. Its cut is the crossing cut of the same crossing: the nearest crossing cut after it for a bend before that cut's token, before it for one after, within the same D258 group.
  - `Free`: none of the above. A Free bend is its own cut (kind `Free`).
  - For Start, End and Crossing, `approaches` is true when the bend lies before its cut's token in travel order (an End bend, a crossing's first Corner, the S-bend form's S-bend); false when after (a Start bend, a crossing's second Corner). The S-bend form's S-bend straddles its crossing: give it `approaches = true` and set `straddles = true`.
  - `partners`: the edges it moves toward or away from at its cut: for Start/End at a node, the node's other edges (`programs.nodes[n]` trunk, straight, diverging, without this edge); for Start/End at a port, none; for Crossing, the other edge of the crossing; for Free, none.
  - A bend that matches two rules, or a step role `LaneSplit`, `LaneMerge` or `Shift` on a band edge that is not Free by these rules, is a stop: report the hub, edge, token and role.

## Types (in `junction_topology.h`, beside `JunctionArrangementUnder`)
```cpp
/** A place where a band edge's segments end, in the edge's travel order (DESIGN-G4 A4). */
struct JunctionBandCut {
	static constexpr uint32_t NO_EVENT = UINT32_MAX;
	enum class Kind : uint8_t {
		Start, ///< The edge's start (a node or a port).
		End, ///< The edge's end.
		Crossing, ///< A crossing with another band edge.
		Row, ///< An Under token below a row edge's doubled bridge.
		Free, ///< A bend that serves no event.
	};
	Kind kind;
	uint32_t token; ///< Index into the edge's program; JunctionTrackEdgeEnd::NO_TOKEN for an end no token ends on.
	uint32_t event; ///< Index into order.bands[band] for a band node end or a crossing, else NO_EVENT.
	uint32_t row; ///< Row cuts: the order row overhead; 0 otherwise.
};

/** One segment of a band edge between two consecutive cuts (DESIGN-G4 A4). */
struct JunctionBandSegment {
	static constexpr uint32_t NO_BAND = UINT32_MAX;
	uint32_t first_cut; ///< Index into the edge's cuts.
	uint32_t last_cut; ///< first_cut + 1.
	uint32_t band; ///< The band it lies in, or NO_BAND between two Row cuts of one row.
};

/** The role of one bend token of a band edge toward the cut it serves (DESIGN-G4 A4). */
struct JunctionBandBend {
	uint32_t token; ///< Index into the edge's program.
	uint32_t cut; ///< Index into the edge's cuts of the cut it serves (its own cut when Free).
	bool approaches; ///< It lies before that cut's token in travel order.
	bool straddles; ///< The S-bend form's S-bend of a crossing: on both sides of it.
	std::vector<uint32_t> partners; ///< The edges it moves toward or away from at its cut.
};

/** A band edge's cuts, segments and bends (DESIGN-G4 A4). */
struct JunctionBandEdge {
	uint32_t edge;
	bool north; ///< Its travel direction.
	std::vector<JunctionBandCut> cuts;
	std::vector<JunctionBandSegment> segments;
	std::vector<JunctionBandBend> bends; ///< In token order.
};
```
`JunctionTrackArrangement` gains `std::vector<JunctionBandEdge> band_edges; ///< Per band edge, by edge index (DESIGN-G4 A4).` Add `operator==` defaults only if the arrangement already compares (match its siblings).

Declare the types and the empty member first, build, and only then write the tests, so their red is an assertion, not a compile error.

## Tests (in `src/tests/junction/topology/junction_topology.cpp`, after "arrangement: band counts on the large hubs"; red first)
1. **"arrangement: band edges' cuts at the seeds"** `[junction][topology]`. For T, LL_RR x L_R and L_R x LL_RR: the number of band edges is 6, 14 and 16; the number of Row cuts is 4, 20 and 8; the number of Crossing cuts is twice the band crossings (2, 2 and 6); every edge has exactly one Start first and one End last; cuts are in strictly increasing token order where both have tokens; every bend token of every band edge has exactly one `JunctionBandBend`; segments are consecutive pairs of cuts covering the cut list. Expected red: 0 band edges.
2. **"arrangement: T edge 4's cuts and bends"** `[junction][topology]`. T's edge 4: cuts Start, Row, Row, Crossing, End in that order (RECON-O3 Q3: start, row crossings at y3 and y4, the band crossing, end); its Corner after the start has role Start, `approaches == false`, partners the start node's other edges; its crossing S-bend has role Crossing, `straddles == true`, partners the other crossing edge. Before writing the expected values, print the edge's program and ends to a file and check they agree with this; if they do not, stop and report the program.
3. **"arrangement: band edges on the large hubs"** `[junction][topology][.arr3]` (or extend the existing `.arr3` test's struct): for LL_RR x LL_RR 36 band edges, 40 Row cuts and 18 Crossing cuts (9 crossings); print each hub's count of Free cuts and segments to the test output (`WARN`) and put the three hubs' numbers in your report. LLL³'s counts: print only.

## CLAUDE.md
`src/junction/topology/CLAUDE.md`, after the bullet beginning "The sweep records each band's line": add "The arrangement cuts each band edge at its ends, its crossings with band edges, its passes under a row's doubled bridge and its free bends (bends serving no end or crossing); the segments between cuts lie in one band or on a row's item rows; each bend names the cut it serves, whether it approaches it, and the edges it moves toward."

## Do not
- Do not change the order, the programs, `band_neighbours` or `band_event_pairs`; do not add begin/end events to `BandNeighbour` (D273).
- Do not touch `src/junction/placement/` or its tests.
- Do not add counts or values you did not measure as pins; the pins above are from RECON-O3 Q3. If one differs, stop and report the measured value and the edges behind it.

## Stop conditions
- Any pinned count differs.
- A bend fits no role or two, or a band edge has a copy 1 program.
- A segment gets two different bands, or a Row cut's row cannot be read.
- Any existing test changes.

## Gates
- While working: `tools/junction/jt fast topology`, output to a file.
- Before the commit: `tools/junction/jt gate`; run the `.arr3` test once by the runner's way for hidden tags (see `docs/junction-tool/TESTS.md`). Report count and time of each.
- One commit: `Add: Junction arrangement cuts band edges into segments`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. The CLAUDE.md edit goes in the same commit.

## Report
References read; commit (hash, subject); red evidence (each test and the failure seen); types and members added (names only); suite results (count, time, failures you did not cause); the Free cut and segment counts per hub; stops and anything left undone; state: `git status --porcelain` and `git log -1` of the worktree. When you have reported, stop.
