# G4 2b1a: band lines, neighbour pairs over every event order, neighbouring event pairs (DESIGN-G4 §1 A1-A3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The static model (DESIGN-G4) separates neighbouring band edges by profile rows. It needs, per band, every pair of edges that can be west/east neighbours in **any** order of the band's events that D255(2) allows, not only the sweep's own order, and every pair of incomparable events whose touched edges can be neighbours (one event-order binary each later). This step adds those as topology data. Nothing in placement reads them yet: no layout, objective, hash or digest moves.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-2b1a <HEAD given at launch>`. Leave its untracked files alone. `build` (Debug) and `build-rel` (Release) exist; delete stale `.pch` files if the build complains.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2b1a/`.
- Another agent is changing `junction_topology.cpp`'s band programs (~1195-1275) in a different worktree. Do not touch the programs, `Step` or edge ends.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part);
- `docs/junction-tool/DESIGN-G4-static.md` lines 56-68 (A1-A6; you do A1-A3) and lines 30-46 (P4);
- `src/junction/topology/junction_topology.h` lines 139-163 (`JunctionTrackBandEvent`, `JunctionTrackOrder`) and 270-294 (`JunctionTrackArrangement`);
- `src/junction/topology/junction_topology.cpp`: the band sweep ~575-730 (where `items` changes per event) and `ArrangeJunctionTrackGraph` ~1332-1370 (band_pairs);
- the reference probe (throwaway, not a template): `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/arr2/arr2_probe.patch` lines 9-50 (the four sites where the sweep's line changes) and `.../scratchpad/g4/g4_probes.patch` lines 639-737 (P4: replacements by prefix/suffix diff, transitive closure of band_pairs, replay, adjacency);
- `src/tests/junction/topology/junction_topology.cpp` ~5394-5540 (`ArrangementTopology`, `CheckArrangementSeed`, the ARR1 tests);
- `src/junction/topology/CLAUDE.md`.

## Changes
1. **The order records its band lines** (`JunctionTrackOrder`, new member, after `bands`):
   `std::vector<std::vector<std::vector<uint32_t>>> band_lines; ///< Per band b, the sweep's line of edges west to east before its first event and after each event: order.bands[b].size() + 1 lines.`
   Record `items` at exactly the four sites of the ARR2 probe (band start, the crossing swap, the node gather, the node expansion), into `order.band_lines[level / 2]`. Size `band_lines` with `bands`. Every band, including an empty one, gets its start line.
2. **The arrangement gains three members** (`JunctionTrackArrangement`, after `band_pairs`), with these types declared inside it next to `BandPair`:
   ```cpp
   struct BandReplacement { std::vector<uint32_t> removed; std::vector<uint32_t> inserted; }; ///< The contiguous block of edges an event takes out of the line and the block it puts in, west to east.
   struct BandNeighbour { uint32_t band; uint32_t west; uint32_t east; bool extension_only; }; ///< Two edges adjacent in the line, west then east, in some order of the band's events; extension_only when the sweep's own order never makes them adjacent.
   struct BandEventPair { uint32_t band; uint32_t first; uint32_t second; }; ///< Indexes into order.bands[band], first < second: neither is ordered before the other, and an edge one touches is adjacent to an edge the other touches in some line.
   std::vector<std::vector<BandReplacement>> band_replacements; ///< Per band, per event (order.bands[band] index).
   std::vector<BandNeighbour> band_neighbours; ///< Per band, sorted by (west, east), each directed pair once.
   std::vector<BandEventPair> band_event_pairs; ///< Per band, sorted by (first, second).
   ```
   `ArrangeJunctionTrackGraph` computes them from `order.band_lines` and its own `band_pairs`:
   - **Replacements:** between consecutive lines k and k+1 of a band, strip the common prefix and the common suffix (the probe's diff, prefix first); what remains of line k is `removed`, of line k+1 `inserted`.
   - **Precedence:** `before[i][j]` from the band's `band_pairs` (south i before north j), transitively closed.
   - **Every order, by downsets:** start from the empty set of done events with the band's start line. A ready event is one not done whose every predecessor is done. Applying it finds its `removed` block in the line (`std::search`) and replaces it by `inserted`. Memoise visited done-sets (a `std::set<std::vector<bool>>` is fine), so each downset is expanded once. The line at a downset does not depend on the order that reached it; if a second path reaches a visited downset with a different line, or a `removed` block is not found or is empty, STOP and report the band and event.
   - **Neighbours:** every adjacent pair of every reached line, directed west then east. `extension_only` when the pair is adjacent in none of the sweep's own `band_lines`.
   - **Event pairs:** i < j with neither `before[i][j]` nor `before[j][i]`, such that in some reached line an edge in `removed ∪ inserted` of i is adjacent (either side) to an edge in `removed ∪ inserted` of j (the probe's definition).
3. **CLAUDE.md** (topology; standalone facts, no D-ids): the sweep records each band's line of edges before and after every event; the arrangement lists each event's replacement block, every pair of edges that are neighbours in some order of the band's events the layers allow (marked when only another order than the sweep's makes them neighbours), and every unordered pair of events whose edges can be neighbours.

## Tests (red first: add the members and an empty computation first, then see each test fail on an assertion; record each red message)
In `src/tests/junction/topology/junction_topology.cpp`, `[junction][topology]`, using `ArrangementTopology` and `MakeJunctionGraphSeed` as the ARR1 tests do:
1. **"arrangement: band replacements replay the sweep's lines"**: on T, LL_RR x L_R and L_R x LL_RR, each band has `bands[b].size() + 1` lines, and applying the band's replacements in sweep order to its start line yields each next line.
2. **"arrangement: LL_RR x L_R has the extension-only neighbours of band 4"**: band 4 has extension-only neighbours whose unordered edge pairs are exactly {2,23}, {4,8}, {4,23}, {17,23} (P4).
3. **"arrangement: downset enumeration equals every linear extension"**: on the same three hubs, for each band, enumerate every linear extension of `before` by recursion, replay each, and collect directed adjacent pairs and event pairs by the same definitions; they equal `band_neighbours` and `band_event_pairs` for that band.
4. **"arrangement: band counts on the large hubs"** in `[junction][topology][.arr3]` (hidden; run once, never in a gate): for LL_RR x LL_RR, LLL_RRR x LL_RR and LLL_RRR x LLL_RRR (AnyToAny, graph and order only; no seed), WARN per hub the events, unordered neighbour pairs, extension-only unordered pairs, event pairs and the arrangement's wall time. P4's sampled lower bounds: LL² 90 / 14 / 21, LLL_RRR x LL_RR 163 / 26 / 83, LLL³ 295 / 49 / 139; the exhaustive counts may only be equal or higher. CHECK each is at least its bound.

## Do not
- Touch placement, the joint, the pipeline, the programs, `Step`, edge ends, or any existing arrangement member.
- Add begin/end events to the neighbour pairs (the replacements give each edge's inserting and removing event), a 2-SAT test, or anything of A4-A6.
- Run `jt sweep` or `jt identity` (D262: this step adds data only).
- Touch any worktree but `nodes`; `.claude/nodes2` is reserved for the engineer's game.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- A replay inconsistency (above).
- Test 4's arrangement time over 2 s on any hub: report the counts and times, do not optimise.
- A count in test 4 below its bound.
- Any existing test fails.

## Gates (D262)
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`: all pass, report the time;
- test 4 once: `build-rel/openttd_test '[.arr3]' > <scratchpad>/2b1a/arr3.txt 2>&1` with a 300 s timeout.

Give every `jt` call a Bash timeout of at least 400000 ms.

One commit on wt-2b1a: `Feature: Junction arrangement lists band neighbours over every event order`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read; the commit; red evidence per test; types and members added;
- test 4's numbers per hub (events, neighbour pairs, extension-only, event pairs, time);
- `[topology]` and gate times; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
