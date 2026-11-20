# BRIEF E1d-2b: the loader's sites from the pins (follows E1d-2's stop)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-e1, head HEAD_HASH, with E1d-2's work UNCOMMITTED in the tree (planar_structure.h, planar_structure.cpp, tests/junction/planar/planar_structure.cpp). You continue from that tree; do not revert it.
- E1d-2's brief: docs/junction-tool/wip/BRIEF-E1d2-structure-loader.md. Read it once: everything in it stands (tests, Do not, gates, CLAUDE.md bullet, commit, report) except the two body steps replaced below. E1d-2 stopped on test 3 (side at the T's merge E1/D1: the expanded node's legs hold a prio row track on line E1, so the leg index is not the authored order) and test 1 stopped at step 4 (`ResolvePlanarSites` refuses 197: "a site's channels have no single port heading").
- **Load: build `-j 4`, `JT_JOBS=4`; never two builds or test runs at once.**
- Budget: 60 minutes.

## Read first
- The brief above; the current `LoadPlanarStructureVariant` in src/junction/planar/planar_structure.cpp; planar_channels.h `PlanarSite` (lines 40-60) and `PlanarChannels::channel_of_line`; planar_gaps.h `PlanarResolvedSite`.

## Replaced step 4: resolved sites from the pins (no call to ResolvePlanarSites)
For each channel site s (`channels.sites[s]`), build `PlanarResolvedSite r`:
- For each of the site's nodes, its pinned leg line (from its `straight` statement; `none` gives no channel), mapped by `channels.channel_of_line`.
- All of the site's nodes `none`: `r.straight = UINT32_MAX`, `r.other = UINT32_MAX`, `r.side = 0`, `r.prio = false`.
- Otherwise the first node (in `site.nodes` order) with a pinned leg decides: `r.straight` = that leg's channel; when it is not one of `site.others`, push `fmt::format("gap: site of trunk {}: pinned leg {} is not one of the site's channels", trunk, leg)` and treat the site as all `none`. `r.other` = the other entry of `site.others` (a site whose `others` is not exactly 2: push `"gap: site of trunk <trunk>: <k> channels"` and treat it as all `none`). `r.side` = +1 when the pinned leg is the first leg of that TOPOLOGY node (`topology.nodes[n].out[0]` for a split, `in[0]` for a merge), else -1. `r.prio = channels.channels[r.straight].prio`.
- When another node of the same site pins a leg in a different channel (or `none` against a channel), push `fmt::format("gap: site of trunk {}: its nodes pin different straight legs ({} and {})", ...)`; the first node still decides.
- differs: when `channels.sites[s].straight != UINT32_MAX` (Structure's own rule settled it) and differs from `r.straight`, push `fmt::format("differs: straight {}: rule {}, pinned {}", trunk of the site's first node, the rule channel's leg line at that node or "none", pinned leg line or "none")`. A rule tie (`UINT32_MAX`) gives no line. Remove the old differs comparison of step 7.

## Replaced step 7: per-node sites
`v.sites = PlanarBuildStructureSites(topology, channels, sites, v.map);` then for every topology node n: the pinned leg's track is the leg l of `v.map.expanded.nodes[n]` with `v.map.tracks[l].kind == PlanarTrack::Kind::Line` and `v.map.part_of[l]` equal to the pinned line; `straight` = l; `side` = +1 when the pinned leg is the TOPOLOGY node's first leg, else -1; `none`: `UINT32_MAX`, 0. No such leg: push `"gap: straight <trunk>: leg <leg> has no track at the node"` and leave the site as built.

## Tests
Unchanged from E1d-2 (tests 1-4). Test 3 must now pass with the rule above; if it still fails, stop and report the node, both sides and both straights. Record test 1's full output again and test 2's first failure.

## Stop conditions, gates, commit, report
As E1d-2's brief (commit subject `Add: Junction planar structure: load a pinned Structure output (D389)`, with the CLAUDE.md bullet), plus: report test 1's output in full. When you have reported, stop.
