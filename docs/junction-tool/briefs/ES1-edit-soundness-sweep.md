# ES1: edit-soundness sweep on flat ground (REVIEW-opus-since-terrain-placement top change 3; D193, D217)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-es1 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes2: never touch it. Never start a game client. Use absolute paths.
- D193: correct by construction; the rule checker (CheckJunctionRules) is the test oracle. The review found the relations are complete only for unedited graphs; L1 (just merged, e4e5b75913) made occupancy legality graph-based, and its test showed an m edit (LL_RR `choices.m[4][0]` 6 -> 5) that the old whitelist accepted. This step measures how sound the placement is under edits: every edit either fails with a reason or yields a checker-clean layout.

## Read first (once each, offset/limit)
CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (sweep rows); src/junction_topology.h JunctionPlacementChoices and the fixed-choices PlaceJunctionTrackGraph overload with x_floors/y_floors (~:240-290); src/tests/junction_topology.cpp: the sweep "the curve family alone keeps every seed clean" (~:2600) as the pattern for seed -> render -> signals -> CheckJunctionRules, and L1's test "topology: occupancy legality reads the graph".

## Test (the step's only change besides TESTS.md)
`[topologysweep]` "topology: every placement edit fails with a reason or is checker-clean", in src/tests/junction_topology.cpp:
- Topologies T and LL_RR; specs tl 3, 4, 6 at v 100 and 160 (6 specs each, as the existing sweeps make specs).
- Per topology and spec: the seed (MakeJunctionGraphSeed or the sweep's way); 200 edits from `std::mt19937 rng(1)` (fixed, reseeded per spec): each edit is one or two (50/50) of these atoms, drawn uniformly among those that exist for the seed:
  - m: an S-bend token (m > 0) gets m + d, d in {-2, -1, 1, 2}, kept >= 1;
  - corner: a corner token (corner > 0) gets corner + d, d in {-1, 1}, kept >= 1;
  - port: port_offset + d, d in {-2, -1, 1, 2};
  - row gap: a y_floor (1 + i, seed row of item i + d), i a random stack item, d in {1, 2, 3};
  - push: an x_floor (p, seed column of p + d), p a random placement point, d in 1..8.
- Each edit: the fixed-choices placement. Failure (no nodes) with a non-empty error counts `failed`; failure with an empty error is a counterexample. Success: render, signal pass, CheckJunctionRules: no violation counts `clean`; any violation (or an empty render, or a signal error) is a counterexample.
- Print `EDITSOUND topology=<T|LL_RR> tl=.. v=.. tried=200 failed=F clean=C dirty=D` per spec and, for each counterexample, one line: the atoms, the first rule id, tile and message.
- Assert dirty == 0 per spec.

## Red
Run the test at the parent of L1's merge (f403a2d77e, a scratch checkout of only the test file in your worktree, Release): record its EDITSOUND lines. Expected: at least one dirty (the whitelisted m edit class). Then at the launch HEAD.

## Stop conditions
- dirty > 0 at the launch HEAD: do NOT commit. Report every EDITSOUND line and the counterexamples grouped by (atom kind, rule), at most 3 examples per group with the full atoms; save the test as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/es1-test.patch and leave the worktree clean. These are findings for the next steps, not for you to fix.
- The sweep takes more than 30 s in Release: report the time and stop.
- Anything not covered.

## Gates (only when dirty == 0 everywhere)
- `build-rel/openttd_test "[topologysweep]"` (report the time); `tools/junction/jt gate build-rel` (count unchanged: the test is not in the gate). TESTS.md row for the test.
- One commit: `Add: Junction sweep: every placement edit fails with a reason or is checker-clean`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus all EDITSOUND lines (red and at HEAD) and the counterexample groups. When you have reported, stop.
