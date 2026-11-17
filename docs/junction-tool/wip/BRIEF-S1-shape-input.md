# S1: the shape as an input (DESIGN-tracks §12 S1, D340)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Code read with offset/limit. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/S1/`.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`. Release `build-rel`.
- Read first: `/home/pete/OpenTTD/CODINGSTYLE.md`; `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` row D340; `DESIGN-tracks.md` §12; `wip/RECON-shape-seam.md`; `src/junction/planar/CLAUDE.md`; `planar_paths.{h,cpp}` (whole `BuildPlanarPaths`); `src/tests/junction/planar/planar_paths.cpp`; in `planar_components.cpp`: `MappedComposer` ctor and `Build` (~1150), `SolveMappedVariant`, `SolvePlanarMapped` (variants, flips, `PLANAR_ONLY` filter); `planar_components.h`.

## Tests, outermost first, each seen red by assertion before its code
1. (planar_components tests, [planarcomponents]) `planar components: a variant solved with its own shape given back is identical`: T2 topology; for tag `o0t2f1s1`: `SolvePlanarMappedVariant(t, spec, "o0t2f1s1", nullptr, a, &why)` and with `shape` = the default shape of that variant (from `PlanarDefaultShape`, below), `b`; REQUIRE both ok; CHECK `a.area == b.area`, `a.track == b.track`, `a.width == b.width`, `a.height == b.height`, routes equal piece for piece. Red: with the entry point declared and the shape argument ignored... it would pass; so instead first make the entry point honour `shape` by replacing the paths, with `CheckPlanarShape` not yet written, and assert also the next test.
2. (planar_paths tests, [planar]) `planar paths: every variant's own shape passes the shape check`: for T2, T3, LL² and every (over choice of `PlanarOpenCrossings`, flips of `PlanarTriangles`): `p = BuildPlanarPaths(t, FlipPlanarTriangles(ch, f), o)`; if `p.why` empty: `CHECK(CheckPlanarShape(t, chf, p.over, p.paths).empty())`. Red: stub returning "unchecked".
3. (planar_paths tests) `planar paths: a shape breaking a rule is refused with its reason`: T2 default paths of o0 f0; mutate (a) an over crossing's piece to a diagonal heading -> reason names "over" ; (b) two consecutive headings 90° apart -> reason names "45"; (c) a crossing_piece index out of range -> reason names the channel. Each `CHECK` on substring. Red: stub.
4. `BuildPlanarPaths(t, ch, o, &p.paths)` returns paths equal to `p.paths` (in test 2's loop).

## Changes
1. `planar_paths.h/.cpp`: `std::string CheckPlanarShape(const PlanarTopology &t, const PlanarChannels &channels, const std::vector<uint32_t> &over, const std::vector<PlanarChannelPath> &paths);` returns empty or the first broken rule, checking exactly the rules `BuildPlanarPaths` builds to (read them; list them in the doc comment): one path per channel; headings non-empty, consecutive headings 45° apart; a channel's first heading = its up port's heading, or its in channel's last heading when it starts at a node (as `head` does); its last heading = its down port's heading when it ends at a port; `crossing_piece` one per channel crossing entry, in range and in travel order; at each crossing the over piece axis and the under piece not parallel to it (the `suits` rule). If `BuildPlanarPaths` enforces a rule you cannot write as a check on these fields, stop and report it.
2. `BuildPlanarPaths(..., const std::vector<PlanarChannelPath> *shape = nullptr)`: computes `over` as today; when `shape` is given, `paths = *shape` and `why = CheckPlanarShape(...)`, skipping the decide code; otherwise today's code unchanged. No other edit inside the decide code.
3. `planar_components.cpp`: `MappedComposer` takes `const std::vector<PlanarChannelPath> *shape` (default nullptr) and passes it to `BuildPlanarPaths` at its read point. `SolveMappedVariant` takes and passes it. `std::vector<PlanarChannelPath> PlanarDefaultShape(const PlanarTopology &, const JunctionSpec &, const std::string &tag, std::string *why)` (planar_components.h): the paths `BuildPlanarPaths` decides for that variant's over choice and flipped channels (the same channels `SolvePlanarMapped` would use). `bool SolvePlanarMappedVariant(const PlanarTopology &, const JunctionSpec &, const std::string &tag, const std::vector<PlanarChannelPath> *shape, PlanarComponentLayout &, std::string *why)` (planar_components.h): solves exactly that one variant (same params as `SolvePlanarMapped`), with the given shape or the default when null, and fills the layout as the winner would be filled. Share code with `SolvePlanarMapped` (factor the variant parsing/params out rather than duplicate); `SolvePlanarMapped`'s behaviour unchanged.
4. `src/junction/planar/CLAUDE.md`: the facts: the shape (per channel piece headings and crossing pieces) can be given to a variant's solve; `CheckPlanarShape` holds the rules a shape must obey; the default shape is `BuildPlanarPaths`' own. No D-refs.

## Do not
- Do not change any heading decision, rule, or jog construction. Do not add a shape program. Do not change `SolvePlanarMapped`'s results.

## Gates
- Build; `jt` gate suite (`/home/pete/OpenTTD/docs/junction-tool/TESTS.md`); T2 and T3 mapped [slow] tests.
- pv before/after on T2, T3 and LL² (201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt`), `-j 6`: must be identical (areas and reasons); stop before committing if not.
- Commit: `Change: Junction channel shape as an input to a variant's solve` + blank line + body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence per test; types/functions added; suites; pv identical yes/no; stops; `git status --porcelain`, `git log -1`. Under 20 lines. When you have reported, stop.
