# G3.3b-5: zero-size bends in the counters, choices from the joint result, and the joint loop measured (DESIGN-G3 §G3.3a item 3, D236)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Occupancy (and the bend hug tiles) are still held at constants taken at the recorded sizes. Before writing linear clash rows, measure the plainest loop:
1. record the system at choices C;
2. solve the joint model;
3. turn its lengths into choices C';
4. repeat until C' == C, then check the fixed path places at C.

This is the lazy-constraint idea in its simplest form: each round re-reads occupancy at the new sizes. If it reaches a fixed point on the hubs, that is the go-live structure. If it cycles, the numbers say where, and the linear clash rows come next.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-g33b5 <HEAD given at launch>`. Build `openttd_test` in both builds before the first run.
- Another agent works in `.claude/nodes` (a hidden probe test in the same test file): never touch it.
- Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g33b5/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.3 and §G3.3a;
- `src/junction/placement/junction_joint.h/.cpp` (all);
- the `JunctionSystem*` types and `JunctionPlacementChoices` in `junction_placement.h`;
- `src/tests/junction/placement/junction_joint.cpp` (all; its helpers `MakeJointInputs`, `DefaultCut`).

## Part A: zero-size bends in the counters (model change, test first)
- A walked bend whose L may be 0 (an S fan with its binary z, or a rows-sized S-bend with floor 0) is today extrapolated to L = 0 by its affine raster. A 0-size bend is no bend: it adds no units and no curves.
- For each such bend, use its z (the S fan's existing one; for a rows-sized S-bend add one, with the same `L >= z`, `L <= M z` rows as the S fan). Write each affine quantity `q1 + (L - 1) d` as `(q1 - d) z + d L`, which is exact for L >= 1 and 0 at L = 0.
- Each curve-gap row that involves a curve of such a bend gets `- M (1 - z)` on its right side, once per such bend in it. M is the same big-M the S fan rows use.
- **The approximation, stated.** When a bend vanishes, the pairs between its neighbours keep the gaps computed on the full curve list. A non-consecutive pair's `(j - i)` then counts the vanished curves, so that gap is stricter than the game needs. Count, per solve, the rows where a vanished bend lies strictly between i and j, and report the count. Do not fix it in this step.
- **Test A1, "joint: a zero-size bend adds no units"** (outer, red first). Take T's recorded system. For one S fan or rows-sized S-bend, force L = 0 by a bound (find one; if T has none walked, use LL_RR x L_R; if neither has one, STOP).
  - Check the joint solve is Optimal.
  - Check that the counters of the route through it give the same `U` at the next bend's entry as a route with that bend removed. Compute that as `E_prev_exit + 2 h · Δ` directly in the test.
  - Red: run it before Part A; record the failure.
- Tests 1 and 2 of b-3 and the b-2 prototype test must stay exactly as they are.

## Part B: choices and positions from the joint result
- Add to `JunctionJointResult`:
  - `std::vector<int> x, y;` with the comment "Per system point, its column and row at the optimum.";
  - `JunctionPlacementChoices choices;` with the comment "The lengths as the fixed path's choices."
- Fill `choices` the way the system's bends name their source:
  - a vertical S-bend or S fan L goes to `m[edge][tok]`;
  - a joiner Corner L goes to `corner[edge][tok]`;
  - `port_offset` is the recorded one.
- If a bend's source does not map to one of these, STOP and name it.
- Size the vectors as the fixed path expects: per edge, per token of the programs. Take the shapes from a recorded seed's `choices`.
- **Test B1, "joint: choices round-trip".** On T and LL_RR x L_R, recording at the seed's choices and solving gives `choices` equal to the seed's, except T's Corner e5 = 5.

## Part C: the loop, measured (hidden test `[.g3loop]`, not in any gate)
- For T, LL_RR x L_R, L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR, under pairing any, at `MakeTestJunctionSpec()`, on the seed's cut if the seed places, else on `DefaultCut`.
  - **C_0:** the seed's choices if it places. Otherwise, the choices of a rounds-mode recording on the default cut. If that recording yields no choices, use all-floor choices (every free S-bend 1, S fan 0, joiner Corner k) and say which you used.
  - **Each round k:**
    - record with fixed choices C_k (the fixed path; if it fails, record what it reaches, as b-2's test 3 does);
    - joint solve;
    - C_{k+1} = result.choices;
    - stop when C_{k+1} == C_k, or when the joint status is not Optimal, or after 12 rounds.
  - **At a fixed point:** place through the fixed path at C. Report whether it places, and its box.
- WARN one line per round: the hub, k, status, objective, the sum of L, the number of bends whose L changed, and ms. Then one summary line per hub: the outcome (fixed point that places / fixed point that fails, with its error / infeasible, with the conflict chain / cycle, with the bends whose L repeats a period / 12 rounds without settling) and the total ms.

## Do not
- Write linear occupancy or clash rows.
- Change the rounds, the fixed path, the seed or the pipeline.
- Make the loop live.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Part A changes any L or objective in the b-2 or b-3 tests.
- Test B1 differs.
- Identity or any existing test changes.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`;
- `[.g3loop]` run once in Release, its output in the report.

One commit on wt-g33b5: `Add: Junction joint model handles zero-size bends and gives choices`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence for A1;
- B1 result;
- the `[.g3loop]` output verbatim, at most 60 lines;
- the approximation count from Part A per hub;
- members added;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
