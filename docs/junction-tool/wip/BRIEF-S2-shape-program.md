# S2: the shape program (DESIGN-tracks §12 S2, D340)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Code read with offset/limit. Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/S2/`.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}` (S1 landed: `BuildPlanarPaths(..., shape)`, `CheckPlanarShape`, `PlanarDefaultShape`, `SolvePlanarMappedVariant`).
- Read first: `/home/pete/OpenTTD/CODINGSTYLE.md`; DECISIONS row D340; `DESIGN-tracks.md` §12; `wip/SPIKE-shapeflow3.md` (the spike MIP; this step recasts it per channel); `planar_paths.{h,cpp}` (`BuildPlanarPaths`, `CheckPlanarShape` and its rule list, `suits`); `src/junction/placement/junction_milp.h` (the HiGHS wrapper; use it, as `planar_model` does); `src/junction/planar/CMakeLists.txt` and `src/tests/junction/planar/CMakeLists.txt`.

## The program (settled; implement exactly)
Per channel c with crossings x_1..x_m in travel order (the entries of `PlanarChannel::crossings`), start heading s_c and end heading e_c exactly as `CheckPlanarShape` requires (up port's heading, or the in channel's last heading for a channel starting at a node; the down port's heading when it ends at a port, else free):
- Integer variables H_{c,i} in [-16, 23] (unwrapped heading at crossing x_i; the real heading is H mod 8), plus unwrapped start S_c = s_c (fixed) and end E_c (= e_c + 8 k_c with k_c integer when fixed, free integer when free).
- Turn between consecutive points (S, H_1, ..., H_m, E): t = next - prev, cost |t| via t = p - n, p, n >= 0. Minimise the sum over all channels. A channel starting at a node takes its in channel's E as its S (so S is a variable linked by equality to E of the in channel, not fixed) where `CheckPlanarShape` says so.
- At each channel crossing (both channels c, d with their entries i, j; `over` from the variant): the over channel's heading is axis: H_over - 2 z = 0 for an integer z (mod 8 even: write H = 2 z + 8 w, z, w integer, or equivalent); the under is not parallel to the over: (H_under - H_over) mod 4 != 0, written with the tied-corner form of spike 3: H_under - H_over = a + 4 q with integer q and a in [1, 3] integer.
- Read the solution and build each channel's `PlanarChannelPath`: from S, step by one 45° per piece towards H_1 (|t| pieces, each one heading), a piece with heading H_1 mod 8 that is the crossing piece of x_1, ..., then towards E. A channel with no crossings and |t| = 0 has one piece. Consecutive crossings with the same heading share one piece only if `CheckPlanarShape` allows two crossings on one piece (read it; if it does not, give each its own piece by inserting nothing and stop to report if that cannot satisfy the check).
- If any rule `CheckPlanarShape` enforces is not covered by the above, add it as a row in the same terms only if it is a condition on the headings at crossings or ends; otherwise stop and report the rule.

## Files
- New `src/junction/planar/planar_shape.{h,cpp}` (add to CMake, file header in the folder's style): `std::vector<PlanarChannelPath> SolvePlanarShape(const PlanarTopology &t, const PlanarChannels &channels, const std::vector<uint32_t> &over, int *turns, std::string *why);` (turns = the objective; empty result and `why` on failure) and `int PlanarShapeTurns(const std::vector<PlanarChannelPath> &paths);` (sum over channels of |turn| between consecutive pieces; also counts from S and to E as above, i.e. the same objective measured on any shape).
- New test file `src/tests/junction/planar/planar_shape.cpp` (tag [planarshape]), registered in CMake.

## Tests (each red by assertion first: `SolvePlanarShape` stub returning the default paths unchanged and turns = their `PlanarShapeTurns`, then the program)
1. `planar shape: every variant's program shape passes the shape check`: T2, T3, LL², every over choice and flip: when the default paths build, `SolvePlanarShape` succeeds and `CheckPlanarShape(...)` is empty. (The stub passes this: it is the guard.)
2. `planar shape: the program never turns more than the default`: same loop, `turns <= PlanarShapeTurns(default)`, and `turns == PlanarShapeTurns(result)`. (Stub passes: guard.)
3. `planar shape: the program turns less than the default on T2`: T2 over choice and flip of variant o0t2f1s1 (`PlanarDefaultShape` gives the default): `turns < PlanarShapeTurns(default)`. Red with the stub. Record the two numbers.
4. Time: the whole T2 loop under 1 s (CHECK on measured time).

## Gates
- Build; `jt` gate suite. No solver behaviour change (nothing calls `SolvePlanarShape` yet), so no pv run needed; state that.
- Report per hub: variants, sum of default turns vs program turns, max ms per call.
- `src/junction/planar/CLAUDE.md`: the fact: `SolvePlanarShape` finds the least-turning channel shape obeying the crossing rules, per variant, not yet used by the solve. No D-refs.
- Commit: `Add: Junction shape program (least-turning channel shapes under the crossing rules)` + blank line + body + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit; red evidence; functions added; suites; per-hub numbers; stops; `git status --porcelain`, `git log -1`. Under 20 lines. When you have reported, stop.
