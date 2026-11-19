# Brief G2 (landing): headings for every line of a variant, sites included

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch -c wt-g2 feature/junction-tool` (at 8c319ada3c, which holds G1: `PlanarTrackMap::expanded`). Commit only there; the orchestrator merges it. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 120 minutes. One heavy job at a time. Tests via `tools/junction/jt` with the budgets in `docs/junction-tool/TESTS.md`.

## Why
DESIGN-tracks §16.7 ("Headings per stretch") and §16.8 step G2 (read both). The track composer builds every line as one segment per heading, so it needs each line's full heading list, including the pieces inside frays and joins that today's recipes invent, and the piece each crossing sits on. This step computes them by one plain rule; it changes no solver code.

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_paths.{h,cpp}`: `PlanarChannelPath`, `PlanarPaths`, `BuildPlanarPaths` (how a channel's start heading is its in channel's end heading, how crossings are placed on "the first piece that suits"), `CheckPlanarShape`.
- `src/junction/planar/planar_channels.h`: `PlanarChannel`, `PlanarSite`, `PlanarNodeLegs`, `group_crossings`, `channel_of_line`.
- `src/junction/planar/planar_gaps.h`: `PlanarResolvedSite`, `ResolvePlanarSites`.
- `src/junction/planar/planar_regions.h`: `PlanarLineSides::Events()`, `PlanarTrackMap` (tracks, `part_of`, `expanded`).
- `src/junction/planar/planar_model.h`: `Turn`, `Rot`, `IsAxis` (headings 0 N .. 7 NW).
- `src/tests/junction/planar/planar_tracks.cpp` (how variants are enumerated and maps built).

## Definitions (the rule; implement exactly this)
For a variant (flipped `channels`, `paths = BuildPlanarPaths(topology, channels, over_bits)`, `sites = ResolvePlanarSites(...)`, `events = PlanarLineSides(topology, channels).Events()`):
1. **A line's base list:** the pieces of its channel's path, `paths.paths[channels.channel_of_line[l]].headings`, with each channel crossing of the line on that channel crossing's `crossing_piece`.
0. **Which leg is straight, and its side** (amended after the first attempt): for a node of a site that is not composed per node (a site is per node when it is a lane split, a lane merge, or has other than two channels: `PlanarSitePerNode` in planar_components.cpp), the straight leg is the node's line in the site's resolved straight channel (`PlanarResolvedSite::straight`) and the other leg's side is the site's resolved `side`; only for a per-node site use `channels.node_legs[n]`.
2. **Split legs** (the line starts at a split node n; h0 = the in line's last heading after its own rule): the straight leg (`node_legs[n].straight`) keeps the base list. The other leg (side s = `node_legs[n].side`) starts with one piece `Turn(h0, s)`; if its base list's second piece equals `Turn(h0, s)` and no event of the line sits on the base list's first piece, that first piece is dropped, else the base list follows whole. At a fork (no straight leg) each leg does this with its own side: the left leg (first in `node.out`) s = -1, the right s = +1.
3. **Merge legs** (the line ends at a merge node m; h1 = the out line's first heading): the straight leg keeps its base list. The other leg (side s) ends with one piece `Turn(h1, -s)`; if its base list's second-last piece equals `Turn(h1, -s)` and no event of the line sits on the base list's last piece, that last piece is dropped, else the base list is followed by it.
4. **Site crossings** (a topology crossing in `group_crossings`, both lines starting, or both ending, at the group): over is the line of the site's straight channel (`PlanarResolvedSite::straight`; for a site composed per node, the node's straight leg); with neither line straight, or no straight channel: `why = "a site crossing with no straight leg: <line> x <line>"`. At a split group the crossing sits on the first piece of each line that suits, counting from the line's start; at a merge group on the last piece that suits, counting back from the line's end. Suits: the over line's piece is axis; the under line's piece is not parallel to the over line's piece. No piece suits: `why = "no piece suits site crossing <line> x <line>"`.
5. **Order:** each line's event pieces (its `events` in travel order, channel and site crossings) must be non-decreasing; otherwise `why = "events out of order on <line>"`.
6. **Copies and rows** of the track map (`PlanarTrackMap` tracks of kind `Copy` and `Row`): the parent line (`tracks[i].line`) and the first and last piece of the parent's list they run beside: the pieces of the first and last parent event the copy mirrors (`expanded.events` of the copy track mapped to the parent's events by crossing partner), and for a row the piece of its first event (or, with none, the parent's last piece) to the parent's last piece.

## Start from the first attempt
A first attempt (rules 2 and 3 without the amendments) is saved as a patch: apply it on wt-g2 with `git apply /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/g2-attempt1.patch`, read it, then change it to rules 0, 2 and 3 as amended above (it used `node_legs` for every node and dropped pieces that carried events, which left legs with no axis piece for their crossings). Its readings stay: a leg starting with base minus its first piece already starts with `Turn(h0, s)` (no repeated heading); at a merge with no straight leg the left leg (first in `node.in`) has s = -1, the right s = +1.

## Output (new, in `planar_paths.h/.cpp`)
```cpp
/** The headings of every line of a variant, sites included (DESIGN-tracks §16.7), and the parent range of each copy and row track. */
struct PlanarLineHeadings {
	std::vector<PlanarChannelPath> lines; ///< Per topology line: its headings and, per entry of its events (PlanarLineSides::Events order), the piece the crossing sits on (crossing_piece).
	std::vector<std::array<uint32_t, 3>> beside; ///< Per track of the map: {parent line, first piece, last piece} for a copy or row; {UINT32_MAX, 0, 0} otherwise.
	std::string why; ///< Empty, or the first rule that failed.
};
PlanarLineHeadings BuildPlanarLineHeadings(const PlanarTopology &topology, const PlanarChannels &channels, const PlanarPaths &paths, const std::vector<PlanarResolvedSite> &sites, const std::vector<std::vector<uint32_t>> &events, const PlanarTrackMap &map);
```
(If `planar_paths` cannot include `planar_regions.h` or `planar_gaps.h` without a cycle, put both in `planar_regions.{h,cpp}` instead and say so.)

## Tests (new file section in `src/tests/junction/planar/planar_tracks.cpp`; red before the code, record the failure)
1. Outer: `"planar line headings: every variant of T2, T3 and LL² by the plain rule"` (`[junction][planar]`, or `[slow]` if LL² puts the tag over budget): per variant whose `BuildPlanarPaths` succeeds and whose track map (form A) builds: `BuildPlanarLineHeadings`; count variants with empty `why` and the failures grouped by text (INFO/print); then for every variant with empty `why` check:
   - consecutive headings of a line differ by exactly one eighth (`std::abs(Rot(a, b)) == 1`);
   - a line from an in port starts on the port's heading; a line to an out port ends on the port's heading;
   - at every split node the straight leg's first heading equals the in line's last; the other leg's first heading is `Turn(h0, s)`; at every merge node mirrored;
   - event pieces are non-decreasing and inside the list;
   - at every crossing the over line's piece is axis and the under line's piece is not parallel; at channel crossings the over line is in `paths.over`'s channel; at site crossings it is the straight channel's line;
   - copies and rows: `beside` has a parent and first <= last < the parent's heading count.
   Require: T2 and T3 have at least one variant with empty `why`. Print the counts for all three hubs.
2. `"planar line headings: T2 o1t3f1s0 straight fray leg and diverging leg"`: on that variant, the EF fray (the split of `EF1`/`EF2` into E and F lines): each E line (straight) starts with the EF line's last heading; each F line starts with `Turn(h0, s)` for its node's side.

## Do not
- Change `BuildPlanarPaths`, the shape program, the track map, the composer or the solve. Make any rule beyond the six above. Touch ports 4077/4079 or start a game client. Use git stash.

## Stop conditions
- More than half of T2's or T3's variants fail one rule: stop after the measurement, report the failure counts by text and one failing case each (line names, headings).

## Gates
- Tests red then green; `tools/junction/jt fast build-rel '[planar]'` (count and time).
- `src/junction/planar/CLAUDE.md`, "Track map" section: one fact stating `BuildPlanarLineHeadings` and the rule in one sentence per numbered item 1-4.
- One commit: `Add: Junction headings of every line of a variant, sites included`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash; red evidence; types and functions added; the variant counts per hub (ok / failures by text); gate results; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
