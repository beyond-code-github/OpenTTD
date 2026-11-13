# F5 probe: does a horizontal band run fit LL_RR x LL_RR? (recon for REVIEW-joint-loop F5)

Read-only recon. Commit nothing; throwaway code only, reverted at the end. Keep your context small: send command output to files and report only what you found. If the probe cannot be built as described, stop and report why; do not invent another form.

## Goal
REVIEW-joint-loop F5 (docs/junction-tool/REVIEW-joint-loop.md: Verdict fact 3, F5, Q4 "A single bridge passes over the S-bend", E3): a band edge that is crossed by a single bridge moves sideways only by a 45-degree S-bend (`src/junction/topology/junction_topology.cpp` ~1209-1211: `SBend` then `Under`/`Above`). A free S-bend's rows are its columns minus one, so on LL_RR x LL_RR edge e1 needs at least 26 rows in band 1, and round 0 of the joint model is infeasible.

The engineer must choose between a horizontal band run (the edge turns square, runs east-west, and turns back, so the crosser meets it square under a short bridge) and taller bands. This probe measures the first option.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-f5 <HEAD given at launch>`. Rebuild `openttd_test` in `build-rel` (delete stale `.pch` files if the build complains). Leave untracked files alone.
- The reviewer's probe sources and logs: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/review/` (`probe_test.cpp`, `probe.log`, `g3loop.log`). `docs/junction-tool/RECON-llrr-round0b.md` has the infeasible cycle.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/f5/`.

## Read first (offset/limit)
- `src/junction/topology/junction_topology.h` ~200-240: `Step`; `Corner` is "a same-hand 90-degree pair";
- `junction_topology.cpp` ~1150-1250 (the band programs);
- `src/junction/placement/junction_placement.cpp`: how a band chain places a Corner (grep `StepKind::Corner`), and how an `Over` single bridge relates to the crossed edge's S-bend (grep "over S-bend" / `JunctionSBendColumnForm`);
- `src/tests/junction/placement/junction_joint.cpp`: how a test runs `SolveJunctionJoint` on LL_RR x LL_RR at `JunctionFloorChoices` (round 0).

## The probe (throwaway)
1. In the band programs, where a crossed band edge emits `SBend(hand)` then `Under`/`Above` (bridge_over(x, e)), emit instead `Corner(hand)`, `Under`/`Above`, `Corner(-hand)`: the edge turns 90 degrees toward the hand, runs east-west under the crosser's bridge, and turns back to its heading. Keep the S-bend's stack item as the second Corner's index if it had one (it had NO_ROW).
2. Run LL_RR x LL_RR (pairing any) round 0 through `SolveJunctionJoint` at `JunctionFloorChoices`, recorded with no point, as the existing round-0 tests do.
3. If placement refuses the programs (a Corner in a band chain not supported, or the crossing over a horizontal run not ruled), report the exact error and where it comes from, then stop. Do not change placement to make it accept them.
4. If it runs, report: status, objective, e1's band-1 rows used, and the conflict chain if infeasible. Then also run T, LL_RR x L_R and L_R x LL_RR the same way and report status and objective against today's (T 27300, LL_RR x L_R 86350 at the seed; for round 0 report both before and after the probe).
5. Count how many crossings changed form per hub.

## Do not
- Commit, or change anything outside the throwaway probe.
- Run the gate or sweep; run only targeted tests with `timeout 600`.
- Touch any worktree other than nodes2. No `git stash`, no `pkill` or broad `kill`, no game client, no background processes left.

## Report (short, about 25 lines)
- the probe diff summary (files, lines);
- per hub: round-0 status and objective, before and after;
- LL_RR x LL_RR: e1's band rows, conflict chain if any, or the refusal and its source;
- crossings changed per hub;
- your one-paragraph reading of what this says about F5 (data, not a recommendation);
- `git -C /home/pete/OpenTTD/.claude/nodes2 status --porcelain` after reverting (only the pre-existing untracked files), and the diff saved as `f5/probe.patch`.

When you have reported, stop.
