# G3.3b-3: curve gaps as unit counters in the joint model (DESIGN-G3 §G3.3a item 1, D236)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
In the joint model only (`SolveJunctionJoint`), the curve-gap relations that are held at constants (tagged `nonlinear`, with reason "curve gap") are replaced by exact linear rows: one unit counter per curve of every route. The fixed path and today's rounds do not change.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-g33b3 <HEAD given at launch>`. Builds `build` and `build-rel` are configured with HiGHS.
- Another agent works in `.claude/nodes2`, on the hug (b-4), in the same files. Keep your changes in their own functions and blocks, so the two merge with little conflict. Never touch `.claude/nodes2`.
- Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g33b3/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.3a;
- `src/junction/placement/junction_joint.h/.cpp` (all);
- the `JunctionSystem*` types in `src/junction/placement/junction_placement.h`;
- `src/junction/placement/junction_placement.cpp`:
  - the curve-gap family (grep "The curve-gap family");
  - `sym_routes` (grep "auto sym_routes", about 150 lines), which is the unit accounting you generalise;
  - the recording sites (grep `SystemTag`);
- `src/tests/junction/junction_track_geometry.cpp`, test "bend rasters are affine in their length" (the quantities and how it reads them);
- `src/tests/junction/placement/junction_joint.cpp`.

## The rows (the math, which is not open)
Per route (lane path and copy, the same set `sym_routes` walks, N side skipped as there), with bends `B_0 … B_n` in travel order. Every bend that `sym_routes` meets counts: Corners, S-bends, Fans and row S-bends, except a 0-size one. Each bend has:
- its anchor point `f` (the element's `first`, as `sym_routes` uses it);
- its in heading;
- its hand and corner flag;
- its length L, which is the system bend's variable if the bend is one, else the constant of today's size;
- its halves: S-bend, Fan and row S-bend `2L`; joiner Corner `2L + 1`; other Corners as `el.offs.size()` today, constant.

From the raster (P1: affine), each bend has these quantities, each `q(L) = q1 + (L - 1)(q2 - q1)`, read from the rasters at L = 1 and 2 exactly as the P1 test reads them:
- `before = (before_dx, before_dy)`;
- `close = (close_dx, close_dy)`;
- `close_units`;
- each curve's units `cu_k`.
A bend whose halves are constant uses its one raster.

Then, with integer variables per route, `E_i` (the units at bend i's entry tile, `f_i + before_i`):
- `E_0 = 2 h_start · ((f_0 + before_0) - start)`, where `start` is the route's start point (as `sym_routes`: the port point) and `h_start` its start heading;
- `E_{i+1} = E_i + close_units_i + 2 h_i · ((f_{i+1} + before_{i+1}) - (f_i + close_i))`, where `h_i` is bend i's out heading (fixed; P1 table);
- the units at curve k of bend i: `U = E_i + cu_k`. That is an expression, not a variable;
- each pair `(i, j, min_gap)` of `JunctionCurveGaps(hands, tl, curve_speed, reference_speed)` over the route's curves in order becomes `U_j - U_i >= min_gap`.

`h · d` is `VX[h] d.x + VY[h] d.y`. Every term is linear in the point variables and the L variables.

## Recording
Add to `JunctionPlacementSystem` what the rows need, and nothing more: a per-route list of bends as above (anchor point id, system bend index or -1 with the constant halves, in heading, out heading, hand, corner, and the coefficient pairs), the route's start point and start heading, and the route's curve hands. Fill it from the same walk as `sym_routes`, reading the elements the walk reads. If a bend has neither a system bend nor a constant size, STOP and name it.

## Model change
In `SolveJunctionJoint`: drop every relation whose reason is "curve gap" and that is tagged `nonlinear`; add the rows above. Leave every other relation as it is. If a relation's reason is "curve gap" but it is not tagged `nonlinear`, keep it and report how many there are.

## Tests (`src/tests/junction/placement/junction_joint.cpp`), outer first
1. **"joint: curve-gap counters agree with today's units at the seed".** For T and LL_RR x L_R, at the recorded system's solved positions and the seed's m, evaluate every route's `U` per curve and compare with today's numbers.
   - Where today's X form exists (rows known), the difference `U_j - U_i` for consecutive curves equals the X form's `(xoff_j + 2 s x_anchor_j) - (xoff_i + 2 s x_anchor_i)`.
   - Where only the Y form exists, it equals `(yoff_j + 2 sv y_anchor_j) - (yoff_i + 2 sv y_anchor_i)`.
   - Read today's forms through a test-visible copy, or by recording `SymCurve` values into the system as test-only data. Choose the copy only if recording would need a new member beyond one vector of (route, curve, xoff, yoff, seg); if neither fits, STOP.
   - Red: run the test before the rows exist; record the failure.
2. **Extend "joint: reproduces the prototype on T and LL_RR x L_R"** to "...and with counters": with the counters in, list every L that differs from the b-2 result (T e5 = 5, the rest today's).
   - Any L that becomes smaller: list it as a re-pin candidate, with the objective delta.
   - Any L that becomes larger, or a change in status: STOP and report. That would mean the counters are stricter than today's rules, which test 1 should have caught.
   - CHECK the list is as observed, and pin it.
3. **"joint: failing topologies with counters" (report only, WARN).** L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR at the default cut, recorded as b-2's test 3 does: status, objective, the box if feasible (the least east and south point values), and the conflict chain if infeasible.

## Do not
- Change the rounds, the fixed path, any relation outside the joint model, or the hug and occupancy relations (b-4 and b-5 own those).
- Add parity variables or floors for non-affine bends: P1 holds; if a raster reads non-affine anyway, STOP.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Test 1 disagrees anywhere.
- Test 2 shows a larger L or a changed status.
- Identity or any existing test changes.
- Anything not covered.

## Gates
- 0 new warnings;
- the joint tests by name (Debug);
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`;
- the solve time per topology, Release.

No CLAUDE.md change: the joint model is not live yet. One commit on wt-g33b3: `Add: Junction joint model keeps curve gaps by unit counters`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- test 1 (equal yes or no; the number of curves compared);
- test 2's re-pin list;
- test 3's per-topology lines;
- held "curve gap" relations removed, and any kept;
- members added;
- gate and identity counts and times, and the solve times;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
