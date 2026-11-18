# Brief: spike the slot layer on T2 (compare only, solver unchanged)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, currently on branch `wt-planar` at {HEAD}. Create the branch `spike-slots` from it (`git switch -c spike-slots`) and commit there. Do not commit to `wt-planar`. When you finish, run `git switch wt-planar` so ng1 is back where it started.
- Build: `cmake --build build-rel --target openttd_test -j8` (Release), output to a file.
- Time box: 2 hours. When it runs out, stop and report what you have.

## Why (read once)
- `docs/junction-tool/wip/REVIEW-sideways-modelling.md` (Q1, Q2, Q4).
- `docs/junction-tool/DESIGN-tracks.md` §3a.
- `docs/junction-tool/DECISIONS.md` rows D312, D333, D349 (grep `^| D312`, and so on).

The question: can a slot layer, computed from the topology alone, predict the sideways steps that today's solver picks? You build a standalone model and compare it with a solved layout. The solver is not changed.

## Read first (offset and limit)
- `src/junction/planar/planar_channels.h` (whole file: `PlanarChannel`, `PlanarSite`, `PlanarNodeLegs`, `PlanarChannels`).
- `src/junction/planar/planar_gaps.h` (`PlanarResolvedSite`, `ResolvePlanarSites`).
- `src/junction/planar/planar_paths.h` (`PlanarPaths::over`, `FlipPlanarTriangles`, `BuildPlanarPaths`).
- `src/junction/planar/planar_topology.h` (lines, ports, nodes; `ParsePlanarTopology`).
- `src/tests/junction/planar/planar_components.cpp` lines 92-130 (how the T2 test gets `PlanarTText(2)`).
- `src/junction/planar/planar_components.cpp`, but only to look up these facts, not to copy them:
  - `Double` ~900 (the lane and copy parallelogram);
  - `Join` ~971-1030 (the prio rows on side `-sigma`);
  - the D312 copy-side comment ~1351;
  - how the variant tag `o{over}t{ties}f{flips}s{steps}` is turned into the flips (`FlipPlanarTriangles`), the ties (`ResolvePlanarSites`) and the over choice (`BuildPlanarPaths(...).over`).
- `CODINGSTYLE.md` (C++ part).

## The model (exactly this)
Units: one slot is one lane spacing.

1. **Intervals.** A topology line belongs to exactly one channel (`channel_of_line`). Cut each channel, in travel order, at its channel crossings (`PlanarChannel::crossings`) into intervals:
   - `pre` before the first crossing;
   - `x<k>` for crossing k itself;
   - `mid<k>` between crossings k and k+1;
   - `post` after the last crossing.

   A channel with no crossings has one interval, `post`.
2. **Tracks in an interval**, as an ordered list from left to right across travel:
   - **The channel's lines**, in `PlanarChannel::lines` order.
   - **Copies.** In a crossing interval `x<k>` where this channel is the over channel (`PlanarPaths::over`), each line whose `line_share` is a full share (`>= 1.0`, as the composer uses it to double) gets a copy track. The copy sits directly beside its line, on the side D312 gives: the far lane copies outward, and every other lane copies away from the far lane. Take "far" exactly as the composer's comment ~1351 defines it.
   - **Prio rows (D349).** For each Join site (`PlanarSiteKind::Join`) whose resolved site has `prio` set, each lane of the straight channel gets a row track in that channel's last interval, `post`. The row sits directly beside its lane, on side `-side` of the resolved site (the side away from the joiners).
3. **Variables.** One integer variable `s(track, interval)` per track per interval it is in. One free integer offset `o(channel)` per channel.
4. **Constraints.**
   - **Order:** in each interval, consecutive tracks satisfy `s(right) - s(left) >= 1`.
   - **Continuity:** a line present in consecutive intervals of its channel keeps its variables linked only through the objective (below); there is no equality.
   - **Ports:** a channel whose lines start at an in port has its `pre` interval (or its first interval) pinned to `s(line) = o(channel) + rank`. Here rank is 0, 1, ... in `lines` order. Do the same at the last interval for an out port.
   - **Copy:** in `x<k>`, a copy's `s` differs by exactly 1 from its line's `s`, on its side.
   - **Straight continuation across a site.** When a resolved site has a `straight` channel, the lines the node legs mark as straight (`PlanarNodeLegs::straight`) link the last interval of the channel before the site to the first interval of the straight channel after it, with `s(after) - s(before) = 0`. Channels joined only through a turn (the `other` channel) are not linked; their `o` stays free.
5. **Objective:** minimise `sum |s(t, i+1) - s(t, i)|` over every line and every consecutive pair of its channel's intervals, plus `sum width(i)` with `width(i) = s(rightmost) - s(leftmost)` in interval i. Write each `|.|` with two non-negative variables. Weight both terms 1.
6. **Forcedness.** For every (line, interval pair) whose solution step is non-zero, solve once more with that step pinned to 0. Report "forced" if the result is infeasible, else "optional", together with the objective change.

Solve with `JunctionMilp` (`src/junction/placement/junction_milp.h`), the same as the solver does.

## The code
- One new test file, `src/tests/junction/planar/planar_slots_spike.cpp`, registered in `src/tests/junction/planar/CMakeLists.txt`. It holds one test case: `TEST_CASE("planar slots spike: T2 slot model against the solved layout", "[.spike]")`. The `.` hides it from the gate.
- All model code lives in that test file, in an anonymous namespace. Add nothing under `src/junction/`.
- The test reads the variant tag from `PLANAR_ONLY` (default `o1t3f1s0`) using `std::getenv`. Do not use `atoi`, `stoi` or `strtol` (they are safeguarded); parse the digits by hand.
- **Output:** write to the file named by `SLOTS_OUT` (default `slots.txt` in the current directory), one line per (line, interval):
  `slot <line name> <channel index> <interval> <s> <step-from-previous-interval> <forced|optional|-> <delta-objective|->`.
  Then write one line per copy and row track: `track <copy|row> <of line> <interval> <s>`.
- The test makes no assertions about the slots. It only requires that the model solved (`REQUIRE` on the status).

## The comparison (a Python script in the scratchpad, not committed)
- Get the observed layout:
  ```
  PLANAR_MAPPED_DUMP=<dir> PLANAR_ONLY=<tag> build-rel/openttd_test "planar components: T2 from its topology text (mapped) at the probe's area"
  ```
  The `seq <strand>: <heading>:<length> ...` lines give each strand's pieces. Headings are 0..7; consecutive pieces of the same heading join.
- An observed sideways step is a piece of heading h±1 between two pieces of heading h. Its size in slots is its length / 2. Its sign is + for h+1 and − for h−1. Report every other turn as a turn, not a step.
- Compare per line (strand name = line name; skip copy strands, which carry `'` or `"` marks, and `prio:` strands):
  - the predicted `sum |step|` and the sequence of signed steps;
  - the observed sum and sequence.
- Report the table. Then give the totals:
  - lines where predicted equals observed;
  - lines where the model predicts steps that the solver did not take;
  - lines where the solver took steps that the model did not predict.
- Run it for `o1t3f1s0`, then for `o1t2f1s0` and `o0t2f1s1`.

## Do not
- Change anything under `src/junction/`.
- Tune the weights, add terms, or "fix" the model to match the solver. Report mismatches as they are.
- Add the test to any fast tag.
- Start a game client, or touch ports 4077 or 4079.

## Stop conditions
- A piece of data the model needs (copy side, row side, the straight legs, port pins) cannot be read from the named structures without inventing a rule. Report which one.
- The model is infeasible for `o1t3f1s0`. Report the IIS rows (`JunctionMilp::InfeasibleRows`) mapped back to constraint names.

## Gates
- The build succeeds. `build-rel/openttd_test "[.spike]"` runs.
- `tools/junction/jt` is not needed: the solver is unchanged.
- Commit on `spike-slots`: `Add: Junction slot layer spike on T2 (test only)`, ending with the line `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Write `docs/junction-tool/wip/SPIKE-slots.md` (do not commit it) with:
- the three comparison tables and totals;
- the forced/optional list;
- every rule you had to read from code, with file:line;
- open questions.

Reply with: the references read, the commit hash, the totals per variant, stops, and `git status --porcelain` plus `git log -1`. When you have reported, stop.
