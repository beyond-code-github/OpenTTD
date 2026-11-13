# G3P: offline prototype of the joint solve (DESIGN-G3 §G3.3, §G3.5 step 1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Find out, offline, whether one joint solve over both axes, with one length variable per bend and the flat build cost as the objective, places the five topologies, and how fast it is. This is research: the fork's behaviour does not change. The only commits are the Python script, the JSON dumps and the result note under `docs/junction-tool/tools/g3proto/`. The C++ dump hook is saved as a patch, never committed.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-g3p <HEAD given at launch>`. Builds `build-rel` and `build` exist.
- Python with HiGHS: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/highs-venv/bin/python` (highspy 1.16, numpy). Install nothing else.
- Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g3p/`.

## Read first (once each, offset/limit)
- `docs/junction-tool/DESIGN-G3-vectors.md` (all);
- `docs/junction-tool/REVIEW-graph-model.md` Q2 to Q4;
- `docs/junction-tool/RECON-s3-probe.md` §0 and §1b;
- `src/junction/placement/CLAUDE.md`;
- `src/junction/placement/junction_placement.cpp`:
  - 300-340 (`why`, `ge`, `eq`);
  - every use of an element's `m` (grep `\.m\b`, about 28 sites);
  - 1470-1520 (the bend size relations);
  - 2060-2150 (the seed rounds and the readback);
- `src/junction/placement/junction_placement.h` (`JunctionPlacementChoices`);
- `src/junction/junction_spec.h` (prices).

## Work
1. **Dump hook (temporary, uncommitted).** Record, for one placement call, every relation `ge(axis, v, u, c)` with its axis, points, constant and `why` reason (rule, edge, node, row), plus every bend element:
   - its edge and token;
   - its kind (Fan, Corner, S-bend), hand and heading;
   - its first and last points;
   - its current `m` and its floor.
   Tag each relation whose constant `c` is computed from a bend's `m` with that bend's id and the formula: `c = a*m + b`, with `a` and `b` in the dump. Do this at each of the ~28 sites by reading the code. A site whose constant depends on `m` non-linearly is listed as such, not approximated.
   Write JSON per topology from a hidden test (`[.g3dump]`) to `docs/junction-tool/tools/g3proto/<name>.json`. For a topology that fails today, dump the relations of the last round before the failure, with the `m` values of that round. Save the hook as `docs/junction-tool/wip/g3p-dump-hook.patch`, then revert it.
   The topologies are T, LL_RR x L_R, L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR, all under pairing any at `MakeTestJunctionSpec()`. For the failing ones, use the default bridge cut.
2. **Model (`docs/junction-tool/tools/g3proto/solve.py`).**
   - Variables: integer positions per point per axis, and an integer length L per bend, bounded below by its floor.
   - Every dumped relation is a row, with `m` replaced by L where it is tagged.
   - Add the link rows for each diagonal: `x_last - x_first = s*L` and `y_last - y_first = s_v*(L - 1)`. Take the signs from the hand and heading, as today's code does (cite the line).
   - If a dumped relation already is that link in one axis, do not add it twice.
   - Objective: the flat build cost from the spec's prices, as the sum over pieces. Track per tile, with a diagonal piece at its price, from the lengths. A bridge is its span tiles times the type price plus heads, at the dump's bridge type. A signal is one per drag spacing along each lane. If a term cannot be written linearly from the dump, leave it out and say which.
   - Ties: add epsilon times the sum of L.
   - Solve as an ILP and as its LP relaxation. Record status, objective, the L values, whether the LP optimum is integral, and the time of each.
   - For infeasible models, compute HiGHS's IIS (irreducible infeasible set) and print it as the `why` reasons in chain form.
3. **Checks.**
   - On T and LL_RR x L_R, the ILP's L must equal today's settled `m` for every bend, or be smaller. List each bend that differs, with old and new values and the objective difference.
   - On the three failing topologies, report feasible or infeasible. When feasible, report the box size, and whether the solution passes back through the fixed path. To check that, apply it with a hidden test using `JunctionPlacementChoices` from the dumped L, if the existing API allows that without a new mechanism; otherwise skip it and say so.
4. **Note.** Write `docs/junction-tool/tools/g3proto/RESULT.md` with a table per topology: variables, rows, ILP status and time, LP integral (yes or no), and objective. Add the bends that differ and the IIS chains. Also list what the model left out, with the reason.

## Do not
- Change any committed C++ behaviour.
- Commit the dump hook.
- Add a solver to the fork, or install packages.
- Invent a relation that is not in the dump, except the link rows and the objective.
- Change rules or floors.

## Stop conditions
- A relation's `m` dependency cannot be read from the code without guessing: list it and stop before modelling.
- The dump of a failing topology cannot be taken without changing placement's behaviour.
- Anything not covered.

## Gates
- `git -C /home/pete/OpenTTD/.claude/nodes diff -- src` is empty at the end.
- One commit on wt-g3p, with the files under `docs/junction-tool/tools/g3proto/` only: `Doc: Junction G3 joint-solve prototype`, ending with the Co-Authored-By line for your model.

## Report (short)
- References read;
- the commit;
- the per-topology table;
- bends that differ on T and LL_RR x L_R;
- the IIS chains;
- non-linear or left-out items;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
