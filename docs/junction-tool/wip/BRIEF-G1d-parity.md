# BRIEF G1d: which tracks' grid rows make LL2's base infeasible (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD} (G1c, 00b5371521). Release `build-rel`.
- G1c: LL2 at off=511 is infeasible with the parity and evenness rows and feasible without them (off 511|1024), so some tracks' grid rows conflict with the closure, pins and steps (the review's F6, wip/REVIEW-slots-lengths.md). G1d names them.

## Changes (src/tests/junction/planar/planar_slots_probe.cpp only)
1. `SolveG1` gains an optional `const std::set<uint32_t> *grid_tracks` (default nullptr = every track): when given, item 2's parity rows (start x + y odd, cross coordinate odd) and the evenness rows are written only for tracks in the set.
2. A new test `"planar probe: G1d the grid rows that block LL2"` (`[junction][planar][planarshape][.probe]`), LL2 first variant only, off=511 throughout:
   - Deletion filter: start with S = every track. For each track t in index order: solve with S minus {t}; if still infeasible, remove t from S for good. At the end S is a minimal set (every track in it is needed for infeasibility). Print `G1D LL2 <tag> minimal=<n> tracks=<names>`.
   - For each track in S, print `G1D TRACK <name> part_of=<topology line or copy/row> pieces=<headings mod 8> events=<count> from=<port side|node index> to=<port side|node index>`.
   - Then, with grid rows on S only, drop the pin rows one at a time (each same-side ring pair, as `ClosureLp` bit 4 lists them) and print `G1D PIN <tag of the pin row> solved=<0|1>` for each pin whose removal makes it feasible.
3. Time: each solve is about 10 ms; the filter is at most a few hundred solves.
- Commit: `Add: Junction planar probe: G1d, the grid rows that block LL2`.

## Run
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: G1d the grid rows that block LL2" > g1d-run.txt 2>&1; grep '^G1D' g1d-run.txt > g1d.txt` (Release, alone).
- Release gate after the commit.

## Do not
- Change any existing row; fix the infeasibility; touch any other file; run other hubs or variants.

## Stop conditions
- The full set S = every track is feasible at off=511 (contradicts G1c): report and stop.
- The gate fails in a test you did not touch: rerun alone once; passes alone: report a flake and continue; else stop.

## Report (short)
- Commit; gate (count, time); `g1d.txt` verbatim; `git status --porcelain`, `git log -1`.
When you have reported, stop.
