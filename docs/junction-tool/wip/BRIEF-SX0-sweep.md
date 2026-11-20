# BRIEF SX0: Structure + Shape sweep, measuring what Slots needs from Shape (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-shape, head {HEAD}. Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel (persistent; do not reconfigure).
- Implements: wip/SPEC-slots.md §0 (architecture checks), the measurement it asks of Shape's output (conditions C1 and C2 below). This is a probe: one hidden test case that prints numbers. No product code changes.

## Read first (once, with offset/limit)
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/CLAUDE.md, sections "Terms", "Structure", "Shape layer".
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_shape_layer.h and planar_structure.h (the types only).
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp lines 255-520: `ClosureLp` and the two `[slow]` T2/T3/LL2 tests; your test follows their form.
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_channels.h (`PlanarChannels::channel_of_line`).
- CODINGSTYLE.md (C++ part).

## The one change
Add one TEST_CASE at the end of src/tests/junction/planar/planar_shape_layer.cpp:
`TEST_CASE("planar sweep: Structure and Shape over T1-T3, LL2 and LL3a", "[junction][planar][planarshape][.probe]")`
- Hubs: T1, T2, T3 (`PlanarTText(n)`), LL2 (`PlanarLL2Text()`), LL3a: the text of /home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/topologies/LL3a.txt pasted into the test as a raw string literal (comments and blank lines may be dropped). A hub whose parse fails prints `SWEEP <hub> PARSE <error>` and is skipped.
- Per hub: `BuildPlanarStructure(topology, spec, 16)` (spec = `MakeTestJunctionSpec()`), then `BuildPlanarShape(...)` on its result. Time each call with steady_clock.
- Per hub, measure and print ONE line:
  `SWEEP <hub> variants=<n> struct_ms=<t> struct_breaches=<sum of CheckPlanarStructurePromise messages over variants> shaped=<n> unshaped=<n> shape_ms=<t> shape_breaches=<sum of CheckPlanarShapePromise messages over shaped variants> closes=<n of shaped whose ClosureLp closes> c1_bad=<n> c1_pairs=<n> c2_bad=<n> c2_structures=<n>`
- For each unshaped variant also print `SWEEP <hub> UNSHAPED <tag> <why>`; for the first 3 breach messages of each oracle per hub print `SWEEP <hub> BREACH <tag> <message>`.
- **C1 (channel lines cross in parallel)**, per shaped variant: for every map crossing c (index into `v.structure.map.crossings`, tracks a and b), for each of its two tracks t: the heading of t at c is `v.tracks[t].pieces[v.tracks[t].event_piece[k]]` reduced mod 8 into 0..7, where k is the index of c in `map.events[t]`. The topology line of track t is t when t < topology.lines.size(), else `map.part_of[t]`; skip t when that is UINT32_MAX (a copy or row). Key: (channel of t's line, channel of the other track's line, or skip when the other is a copy/row). Collect the set of headings of t per key. c1_pairs = number of keys over all shaped variants; c1_bad = keys whose set has more than one heading. For the first 5 bad keys per hub print `SWEEP <hub> C1 <tag> ch<C> x ch<D> headings=<list>`.
- **C2 (a structure's crossers are parallel)**, per shaped variant, per `PlanarStructure s` in `v.structure.structures`: for each k, crossing `s.crossings[k]` and crosser track `s.crossers[k]`, the crosser's heading at that crossing (as in C1) reduced mod 4 (a line, not a direction). c2_structures = structures with at least two crossers; c2_bad = those whose set has more than one value. For the first 5 bad per hub print `SWEEP <hub> C2 <tag> structure <i> sep <track> headings=<list mod 8, in crosser order>`.
- The test makes no REQUIRE on the measurements (it is a probe); REQUIRE only that parsing of T1-T3 and LL2 succeeds.

## Run
- Build: `cmake --build /home/pete/OpenTTD/.claude/ng1/build-rel --target openttd_test -j16 > /home/pete/OpenTTD/.claude/ng1/build-rel/sx0-build.log 2>&1`; grep it for `error`.
- Run by exact test name (a hidden case; jt has no mode for it): `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar sweep: Structure and Shape over T1-T3, LL2 and LL3a" > sx0-run.txt 2>&1`; then `grep SWEEP sx0-run.txt`.
- Then the fast gate for the file's tag: `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10` (your hidden case does not run in it; it proves the file still builds and the fast cases pass).

## Do not
- Change any file but src/tests/junction/planar/planar_shape_layer.cpp.
- Change `ClosureLp`, the oracles, or any product code; fix any breach or unshaped variant you find; add REQUIREs on measurements.
- Run the wide gate or any other heavy job (one heavy job at a time on this machine).

## Stop conditions
- The run exceeds 900 s (timeout kills it): report which hub lines printed and stop.
- LL3a crashes or `BuildPlanarStructure` throws: report the output tail and stop (keep the commit of the test).
- Anything in the measurement definitions above that does not map onto the types: stop and ask; do not reinterpret.

## Gates and commit
- Build clean; `[planarshape]` fast green inside 10 s.
- One commit on wt-shape: `Add: Junction planar probe sweeping Structure and Shape over T1-T3, LL2 and LL3a` with the Co-Authored-By line `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short, this format)
- References read
- Commit: hash and subject
- The SWEEP lines verbatim (all of them, from the grep)
- `[planarshape]` result: cases, passed, time
- Stops, anything left undone
- State: `git status --porcelain` and `git log -1 --oneline`
When you have reported, stop.
