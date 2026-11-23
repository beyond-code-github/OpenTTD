# BRIEF rig-writer: a v2 drawing as a JunctionLayout for the rig (handover step 2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes in all. The probe run in part 6 caps every solve at 15 s and runs once.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Builds: Debug `build`, Release `build-rel`.
- Why: the engineer: "we can't really judge anything by a drawing that we can't place". Recon: `docs/junction-tool/wip/RECON-rig-v2.md` (read it whole; it is short). Flat ground, rail and bridges only in this step: no levels, no tunnels, no signals.

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md`, the last two bullets of the Drawing program section ("Domain", `PlanarDomainInputOf`).
- `src/junction/planar/planar_domain.h` (whole, it is short) and `src/junction/planar/planar_domain_map.{h,cpp}` (`PlanarDomainInputOf`, `AddStructure`).
- `src/junction/junction_layout.h` lines 40-70 and 130-145; `src/junction/junction_route.cpp` lines 155-200 (`AddRasterToLayout`, `NormaliseJunctionLayout`).
- `src/tests/junction/game/junction_game.cpp` lines 1240-1340 (`[planarbuild]` build loop, `PrepareDomainMap`, `BuildPieces`) and 1398-1410 (`BuildDomainBridge`).
- `src/tests/junction/planar/planar_domain_encoding.cpp` lines 3652-3700 (BR1) and 7055-7075 (the `[slotsstep2]` domain verdict).

## 3. The mapping (decided)
For a `PlanarDomainInput in` that `CheckPlanarDomain` accepts:
- Rasterise every track with `RasterPlanarDomainTrack`. Every piece becomes a track bit (`piece.track`) on its tile (`piece.tile`), merged per tile as `AddRasterToLayout` does, EXCEPT the pieces `first..last` (inclusive) of each structure's separator raster: those are the bridge (heads and middles).
- Each structure that is a bridge becomes `JunctionBridge{start = sep.pieces[first].tile, end = sep.pieces[last].tile}`. A structure that is a tunnel is not built: its separator pieces `first..last` stay out of the rail too, and the function records `"tunnel <separator> not built (no levels)"` in `notes`.
- Then `NormaliseJunctionLayout` (offsets >= 0, size the bounding box).
- A raster with a non-empty `rule`, or a bridge whose head tiles do not share a row or column, fails with `why` (return an empty layout), no partial output.

## 4. Tests, outermost first
**Test A** (outer, `src/tests/junction/game/junction_game.cpp`, tags `[junction][junctiongame][planardomaingame]`): "a v2 drawing's layout builds in the game (BR1)". BR1's base domain input (as the BR1 test at planar_domain_encoding.cpp:3681 builds it: if that input is not reachable from junction_game.cpp without moving fixture code, STOP and report) -> `PlanarDomainLayout` -> on `PrepareDomainMap()`'s map, `MakeJunctionBuildSteps(layout, origin)` and the same command loop as `[planarbuild]` (rail and bridges; no signals): every command succeeds, and the built tiles' track bits equal the layout's. Red first with a stub returning an empty layout and why "stub": record the failure text.
**Test B** (inner, `src/tests/junction/planar/planar_domain_encoding.cpp`, tags `[junction][planar][planardomain]`): on BR1's base input the layout has exactly one bridge, its heads on one row or column, no track bit on any tile from the head to the head inclusive along the separator, every tile offset >= 0; and a hand input with a bad point (`rule` "grid") gives an empty layout and a `why` naming the track.
Budgets: `tools/junction/jt fast build '[planardomain]'`, `tools/junction/jt fast build '[junctiongame]' 10`.

## 5. Changes
1. `planar_domain_map.h/.cpp`: `JunctionLayout PlanarDomainLayout(const PlanarDomainInput &in, std::vector<std::string> &notes, std::string &why);` with a doc comment citing this brief's mapping.
2. `[slotsstep2]` probe (planar_domain_encoding.cpp, at the domain verdict ~:7068): when env `PLANAR_RIG_OUT=<path>` is set and the domain accepts, write `JunctionLayoutToJson(PlanarDomainLayout(...))` to that path and print `rig layout: <path> size WxH tracks N bridges M notes K` (or the why).
3. `src/junction/planar/CLAUDE.md`: one bullet under the domain bullets for `PlanarDomainLayout` (decided facts only). TESTS.md: measured times if a tag row changes.

## 6. Probe run (after the tests are green; Release)
In `/home/pete/OpenTTD/.claude/ng1/build-rel`: `PLANAR_SLOTS_HUB=T1 PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 JUNCTION_MILP_INCUMBENT=1 PLANAR_RIG_OUT=<your scratchpad>/T1-rig.json ./openttd_test '[slotsstep2]' > <scratchpad>/t1.log 2>&1`. Then copy the JSON to `/home/pete/OpenTTD/docs/junction-tool/wip/rig-2026-11-25/T1-v2.json` (create the folder) and commit it with the docs. Report the rig layout line, the judge and domain lines. If T1 is not domain-accepted, report; do not retry.

## 7. Do not
- No levels, terraform, tunnels or signals. No change to `CheckPlanarDomain`, `RasterPlanarDomainTrack` or any solver. No new knob besides `PLANAR_RIG_OUT`.
- Never start a game client or server, never touch the rig (ports 4077/4079), never touch other worktrees. No `git stash`; source edits with the Edit tool.

## 8. Stop conditions
- BR1's input unreachable from the game test without moving code; a bridge head piece that is not axis; any command failure in test A you cannot trace to the mapping in part 3 (report the command, tile and error; do not change the mapping).

## 9. Gates and commit
- The two fast tags above and `tools/junction/jt gate build-rel` (a SIGSEGV/SIGILL in an unrelated test that passes alone is CPU instability: rerun it alone, say so).
- Commit: `Add: Junction: PlanarDomainLayout, a v2 drawing as a rig layout (flat, rail and bridges)`, then `Doc: Junction: T1 v2 rig layout` for the JSON. End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 10. Report
References read; commits (hash, subject); red evidence; types and members added; suite results (counts, times, failures you did not cause); the probe's rig layout, judge and domain lines; stops and anything undone; `git status --porcelain` and `git log -1`. When you have reported, stop.
