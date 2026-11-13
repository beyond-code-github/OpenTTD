# Review: does the joint loop converge, and what should the rule source be? (holistic design review, 2026-11-14)

Run as `junction-reviewer` with model opus and effort xhigh (engineer approved, 2026-11-14: "We need to figure this out holistically rather than just adding more constraints and rules"). This is read-only: commit nothing and change no code. Write the review to `docs/junction-tool/REVIEW-joint-loop.md` (untracked), at most about 400 lines. Lead with a one-page verdict, and put the detail after it.

## The goal
The OpenTTD junction tool generates rail junction hubs and places them on real terrain in game. The engineer's targets are LL_RR x LL_RR, LLL_RRR x LL_RR and LLL_RRR x LLL_RRR, built in game. Today T (L_R x L_R) and the sideline (LL_RR x L_R) work in game, and L_R x LL_RR seeds since today. None of the targets seeds.

Constraints that are not negotiable:
- The hover (placement at the cursor on real terrain) must take under 100 ms.
- The seed per topology and settings may be slower, because it is cached (D242: at least one seed per preset, keyed on settings, LRU 8, made off the main thread if it stutters).
- Rules are general rule families applied everywhere, never per site. Margins come from the rules, never padding.
- Every relation is a rule floor that cites its rule. Decided rules are in `docs/junction-tool/DECISIONS.md` (D-ids). Grep it rather than guessing.
- Placement is horizontal X/Y difference constraints (`JunctionPositions`), then a level solve, render, signals and the checker `CheckJunctionRules` as an oracle.

## How we got here
- **G3.** The fixed path (`PlaceTrackGraph`, file `src/junction/placement/junction_placement.cpp`) places by "rounds": a Y solve, then an X solve, at fixed bend sizes ("choices"), with sizes read back and re-solved. It fails on the big hubs because the sizes it reads back are wrong.
- **The joint model.** `SolveJunctionJoint` (`src/junction/placement/junction_joint.cpp`) solves X, Y and every bend length L together as one MILP (HiGHS), with the flat build cost as the objective. It reads a *recording* of the fixed path's relations at some choices (`RecordJunctionPlacementSystem`). Each relation is tagged with its dependence on one bend's length (c = a L + b).
- **D240** (engineer): retire the rounds and recording for one rule source. That is DESIGN-G3 §G3.6 v2 (`docs/junction-tool/DESIGN-G3-vectors.md` lines 95-170; review of it: `docs/junction-tool/REVIEW-rule-source.md`).
  - Static rows: the rules that need no solve.
  - Lazy rows, detected at a point: occupancy, the hug, the clearance.
  - A canonical point: least Y, then least X plus the D215 push.
  - A seed loop: MILP → canonical point → detect → add rows → repeat, until nothing is added.
  - Migration R1-R6.
- **Done today, all merged** (STATUS.md line by line has the details):
  - R1: bend sizes as inputs.
  - R2: `detect(x)` and `detect_y` extracted. They still run inside `build()`, because the facts pruning reads the relations already emitted.
  - R3a: a recording can be given a point. `SolveJunctionJointLoop` records, solves, takes the canonical point and repeats.
  - G3.4a-2: a single bridge's sideways column t over an S-bend is a joint variable within the diagonal.
  - G3.4b, D248: either S-bend flanking a crossing, chosen by a binary, with record-only rows for the alternative.
  - R3b: W/E port fans are joint variables tied to the rows by the fan's sign.
  - R3c, D249: the order of a clash is a binary, but only for clashes kept at a joint point.
  - G2c-1: a band edge carries a series of single bridges.
  - R4a, D250: when no cut places, the seed falls back to the loop from all-floor choices.
  - TT3: tests, and the sweep budget.
- **Parked:** R3d, D252, the port offset as a joint variable (`docs/junction-tool/wip/r3d-stopped.patch`). It made LL_RR x LL_RR's round 0 feasible, but T's loop then cycles with period 2 (objective 26600 → 27550 → 53850), and LL_RR x LL_RR's round 0 takes 10.7 s.

## The pattern the engineer wants examined
Each step today was locally correct, and each moved the failure rather than removing it. On LL_RR x LL_RR:
1. Round 0 was infeasible: a bridge was tied to the S-bend before its crossing (RECON-llrr-round0.md).
2. Fixed by D248. It was then infeasible again, once the port fans followed the rows: the port rows were constants, which capped a stack row (RECON-llrr-round0b.md).
3. Fixed by R3d. Round 0 is feasible, and round 1 keeps 529 clashes, after which the recording's own X solve contradicts itself. Meanwhile T stops converging.

Other observations:
- **RECON-fidelity.md:** frozen port fans, and round-0 Y-only recordings that gave a zero-x point.
- **RECON-R3-loop.md:** which inner solves fail on which hub.
- **The hug of crossers.** "Which crosser tiles lie on a head row" depends on the rows, which the MILP moves. It is recorded at the recording's rows. Removing it makes round 0 feasible, but it is not individually necessary to the conflict.
- **Choice-dependent rule sets.** The recorded rule set changes with the choices: fan empty or sized branches, which rows exist, which rows a head lies on. The MILP sees only the branch present at the recording. Relaxing the absent branch by a binary was tried twice for port fans, and both times was unsound: T's objective fell by 3400, because the fan "goes free" when its sized rows are not recorded.
- **The conflict chains.** `JunctionMilp` reports the IIS of the LP relaxation, so with big-M binaries the chains can point at the wrong rows. One came back empty.
- **Times.**
  - The hidden sweep (`jt sweep`) went from 63 s to 262 s (412 s with R3d).
  - The wide gate is 45 s.
  - `[milp]` Debug is at its 10 s budget.
  - LL_RR x LL_RR's round 0 MILP alone takes 10.7 s.
- **The cut search.** `CutJunctionTrackBridges` does not finish in 100 s on LLL_RRR x LL_RR once the crosser cap is gone (RECON-G2c.md).

## Questions (answer each, with evidence: file:line, or a run in a free worktree)
1. **Convergence.** Can a record → MILP → canonical point → re-record loop converge in general, when some rule families' existence or constants depend on the rows and columns the MILP moves? Which families are those? List every family in `build()` and classify it:
   - static in L;
   - affine in L, tagged;
   - depends on the solution's rows or columns (stale when they move);
   - branch-dependent (empty or sized);
   - order-dependent (occupancy).
   
   `docs/junction-tool/RECON-R2-families.md` is a start, but it predates today's work.
2. **The alternative.** Is a single, complete model feasible and tractable? That is, every family that depends on the solution modelled inside the MILP:
   - disjunctions with binaries for orders, branches and "which row a head lies on";
   - indicator constraints, or the big-M forms HiGHS supports.
   
   Estimate the binary count on LL_RR x LL_RR and LLL_RRR x LLL_RRR, and the likely solve time. Today's MILP has about 1500 relations, about 50 lengths, and up to 529 clash binaries.
3. **Other architectures.** Compare with options such as:
   - lazy constraint generation inside one branch-and-bound (callbacks), versus re-solving from scratch;
   - decomposition: Y (rows and stack) first as its own MILP over lengths, then X at fixed rows. The Y families look mostly row-local;
   - fixing the structure-level choices (which S-bend, which branch, the offset) by a cheap search outside, with the rest a pure LP/difference system;
   - a constructive generator for the regular hubs, given that the coop corpus shows regular patterns.
   
   For each: completeness (can it miss a feasible layout?), determinism, seed time, and what it does to the hover (which must stay a fixed-length difference system under 100 ms).
4. **Is the model right at the root?** Are some failures caused by programs or structure that are wrong for big hubs, not by the solver? For example:
   - the hug rule ("heads hug the outermost crossers") as an equality;
   - "a single bridge passes over the S-bend it crosses" (DESIGN-G1 §10.7, written for the T);
   - the lane split runs;
   - the prio element widths.
   
   Look at the conflict cycles in the RECON files and judge whether each binding rule is a true game or coop rule, or an artefact of how the T was generalised.
5. **The diagnosis tooling.** What would give exact conflicts? For example, a deletion filter over the MILP's relation rows, or HiGHS's own IIS on the MIP.
6. **Recommendation.** One path. Order the first three steps with their tests (outside-in), say what to delete, and say what in today's merged work is reused or retired. Name which decisions (D-ids) it would revise.

## Read
- `docs/junction-tool/STATUS.md`: the top ~12 lines;
- `docs/junction-tool/DESIGN-G3-vectors.md`: all, §G3.6 in particular;
- `docs/junction-tool/REVIEW-rule-source.md`;
- the RECON files named above, plus `RECON-F8-run.md`;
- `docs/junction-tool/DECISIONS.md`: grep D2xx for the last 60 entries, plus D113, D145, D174, D209, D215, D217, D224, D228, D229;
- the CLAUDE.md files under `src/junction/` (topology, placement);
- the code:
  - `src/junction/placement/junction_placement.cpp` (`PlaceTrackGraph`: `build()`, `detect`, `add_occupancy`, the fixed path, `record_system`);
  - `src/junction/placement/junction_joint.cpp` (all);
  - `src/junction/junction_pipeline.cpp` (`CutJunctionTrackBridges`, `MakeJunctionGraphSeed`);
  - `src/junction/topology/junction_topology.cpp` (the programs builder ~1100-1250);
- tests: `src/tests/junction/placement/junction_joint.cpp`.

You may build and run in the free worktree `/home/pete/OpenTTD/.claude/ng1`:
- `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-review <HEAD given at launch>`;
- builds `build` and `build-rel`, configured with HiGHS; delete stale `.pch` files if the build complains;
- throwaway instrumentation only, reverted at the end.

Never touch the other worktrees. Never use any form of git stash, pkill or a broad kill. Never start a game client. Use `timeout` on every run. Leave no background processes.

## Output
`docs/junction-tool/REVIEW-joint-loop.md` in the main checkout:
- the verdict (one page);
- the answers to questions 1-6;
- the evidence.

Report back a 15-line summary and the path. When you have reported, stop.
