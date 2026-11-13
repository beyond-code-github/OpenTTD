# TT3: tests follow the hubs the joint loop now seeds (D250, D251)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Since the seed falls back to the joint loop (R4a) and the loop's model gained port fans, the flanking S-bend and clash order, some hubs that were refused now seed. `jt gate build-rel` at the launch head fails 3 tests:
1. `src/tests/junction/topology/junction_topology.cpp` "topology: an L_R main with a multi-lane branch is refused, not crashed": at least LL_RR now seeds. The test takes 26.6 s Release in the gate.
2. Same file, "bridge cut: the reported failure is the default cut's": the error now ends with "; the joint loop: <outcome>", whose text contains "bridge heads hug crossers".
3. `src/tests/junction/placement/junction_joint.cpp` "joint: a port fan's size follows the rows": `MakeLoopStart` now finds a seed for L_R x LL_RR, so its `REQUIRE(s.description == "all-floor choices")` fails.

First, check that every newly seeded hub is legal. Then update the tests.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-tt3 <HEAD given at launch>`. `build-rel` is built at the launch head; rebuild `build` (delete stale `.pch` files if the build complains). Leave untracked files alone.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/tt3/`.

## Read first
- the three tests above, and one existing test that checks a seed with `CheckJunctionRules` (grep "CheckJunctionRules" in junction_topology.cpp);
- `tools/junction/jt` and `docs/junction-tool/TESTS.md` (the `[sweep]` row).

## Part A: the new seeds are legal (do this first)
For L_R x LL_RR, L_R x LL_R, L_R x L_RR, LL_RR x LL_RR (pairing any) and LL_RR x LL_RR (pairing keep), run `MakeJunctionGraphSeed`. For each hub that seeds:
- render, signal and run `CheckJunctionRules` the way the existing clean-seed test does;
- report the hub, the box size, and the checker's violations.

If any seeded hub has a violation, STOP and report it, with the violations. That would be a real finding. Write nothing else.

## Part B: the tests (only if Part A is clean)
1. Rename "topology: an L_R main with a multi-lane branch is refused, not crashed" to "topology: an L_R main with a multi-lane branch seeds clean or names a reason". It CHECKs, per branch, that the seed has routes and the checker is clean, or that the error is non-empty. Tag it `[junction][topologysweep][.sweep]`, because it runs the joint loop.
2. "bridge cut: the reported failure is the default cut's": apply the existing checks to the part of the error before "; the joint loop:". Then CHECK that the error contains "; the joint loop:".
3. "joint: a port fan's size follows the rows": build its round-0 choices with `JunctionFloorChoices` directly, not `MakeLoopStart`, so it tests the all-floor start whether or not the hub seeds. Drop the `REQUIRE` on the description.
4. **D251:** in `tools/junction/jt`, the sweep's default budget becomes 240 (`budget=${3:-240}`). Update TESTS.md's `[sweep]` row: budget 240 s (D251), with your measured time, dated 2026-11-14 TT3.
5. TESTS.md's `[topologysweep]` and wide-gate rows note the move.

## Do not
- Change any `src/junction/` code.
- Loosen a check other than as listed.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- Part A finds a violation.
- A gate fails for a reason not listed above.
- Anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`: all pass. Report the time;
- `tools/junction/jt sweep build-rel`: all pass, under 240 s;
- `tools/junction/jt identity build-rel`.

Give every `jt` call a Bash timeout of at least 400000 ms.

One commit on wt-tt3: `Codechange: Junction tests follow the hubs the joint loop seeds`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- Part A's table: hub, seeded or the error, box, violations;
- the commit;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
