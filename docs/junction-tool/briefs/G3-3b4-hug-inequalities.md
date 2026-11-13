# G3.3b-4: the bridge hug as inequalities and cost in the joint model (DESIGN-G3 §G3.3a item 2, D236)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
In the joint model only (`SolveJunctionJoint`):
- drop the equalities that pin a doubled bridge's heads to the outermost crosser tile by the flat solve's order;
- make the "every crosser tile lies between the heads" inequalities exact where the crosser's tile on the row is a straight vertical piece;
- let the cost objective (the span tiles) pull the heads tight.
The fixed path and today's rounds do not change.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-g33b4 <HEAD given at launch>`. Builds `build` and `build-rel` are configured with HiGHS. Build `openttd_test` there before the first test run; the build dirs may be behind.
- Another agent works in `.claude/nodes`, on curve gaps (b-3), in the same files. Keep your changes in their own blocks, so the two merge with little conflict. Never touch `.claude/nodes`.
- Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g33b4/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.3a;
- `src/junction/placement/junction_joint.h/.cpp` (all);
- the `JunctionSystem*` types in `src/junction/placement/junction_placement.h`;
- `src/junction/placement/junction_placement.cpp`:
  - `edge_tiles_of` (grep "auto edge_tiles_of");
  - the hug inequalities (grep "Every crosser's tiles on the copy row");
  - `hug_heads` (grep "auto hug_heads");
  - the recording sites (grep `SystemTag`);
- `src/tests/junction/placement/junction_joint.cpp`.

## The change (the math, which is not open)
Each hug inequality today is `x_z - x_head >= 1 - dx`, or the mirrored form, for a crosser tile `(z.pt, z.dx)` on a bridge row. Classify the tile by the `edge_tiles_of` branch that made it:
- **Straight tiles** (a Tile, a Heads tile, a HeadsPair tile, or a vertical run's tile, the last branch of the loop): `dx` is 0, and the tile's column is its point's column whatever the rows are. The inequality is linear and exact. Record it untagged, `nonlinear = false`, with reason "bridge heads clear crossers".
- **Bend tiles** (a Fan or another bend's `offs`): `dx` depends on the bend's raster and on which of its rows the bridge row is. Keep it tagged `nonlinear`, held at its constant as today, and count it.

The `hug_heads` equalities (reason "bridge heads hug crossers", the ones that pin a head to the outermost tile) are tagged `nonlinear` with the flat-solve order. Recording keeps them. `SolveJunctionJoint` drops them: the span cost already in the objective (`bridge (price(1) - price(0))` per span tile) pulls each head to the nearest legal column. Whether a dropped hug that leaves a head looser than today costs more is the measure.

## Tests (`src/tests/junction/placement/junction_joint.cpp`), outer first
1. **"joint: bridge heads hug without the flat order".** For T and LL_RR x L_R, solve the joint model with the change. Then, with the resulting lengths fixed, place through the fixed path. Check that every doubled bridge's span (heads' column distance, both copies) equals its span in the seed placement. If the joint solve's span differs, the test fails with the bridge and both spans.
   - Red: write the test first, then remove the equalities. If the test was green before the change, that's expected (the equalities pinned the same answer). Record "green before, green after" and say so. If it goes red after removing them, STOP and report: it means cost alone does not pull the heads tight, and that is Pete's decision.
2. **"joint: straight crosser tiles are exact"** (characterisation). For T, LL_RR x L_R and the three failing topologies at their default cut: count the hug inequalities that are straight versus bend. INFO the counts per topology. CHECK on T and LL_RR x L_R that the straight ones, evaluated at the recorded solved positions, hold.
3. **"joint: failing topologies with the hug relaxed" (report only, WARN).** L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR, recorded as b-2's test 3 does: status, objective, the box if feasible, and the conflict chain if infeasible.

## Do not
- Change the rounds, the fixed path, or relations outside the joint model's reading of the record.
- Touch the curve-gap relations (b-3) or occupancy (b-5).
- Linearise bend tiles. That is a later step once their count is known.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Test 1 red after the change (see above).
- Identity or any existing test changes.
- A hug inequality site whose tile kind cannot be read from `edge_tiles_of`'s branch without changing what it returns: name it.
- Anything not covered.

## Gates
- 0 new warnings;
- the joint tests by name (Debug);
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`.

No CLAUDE.md change: the joint model is not live yet. One commit on wt-g33b4: `Add: Junction joint model lets the span cost hug bridge heads`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- test 1 (red or green, before and after);
- test 2's counts;
- test 3's lines;
- members added;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
