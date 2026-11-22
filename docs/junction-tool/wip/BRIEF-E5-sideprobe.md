# BRIEF: E5 side probe on the T3/LL2 crowding (read-outs only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 60 minutes in total. Each LP has a 15 s cap. Run the hidden hub test at most twice (once after the build, once more only if the first run crashed or printed nothing from the probe). No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build dir `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Plan: docs/junction-tool/STATUS.md, NOW item "0. LATEST" (steps (a), (b) and Gemini's cause 2). Design: src/junction/planar/CLAUDE.md, section "Drawing program", the pipeline bullet (D422, D425) and the pairwise bullet.

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md`, C++ part.
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions`, `PlanarDrawResult`, `PlanarDrawPipelineOptions` (lines ~1-215).
- `src/junction/planar/planar_draw.cpp`: the pairwise `sides_from` block (lines ~1160-1310: `Axis`, `axes`, the candidate loop over `axis_index` and `side`, `sep_extremal`, the `free_sides` branch); the elastic family test (line ~265); `DrawPlanarPipeline` from the commit stage to the end (lines ~1560-1696: `commit_rounds`, `last`, `last_input`, the `free_stretched_sides` block).
- `src/tests/junction/planar/planar_draw.cpp`: the hidden test "planar draw: the pipeline on T2, T3 and LL2" (lines ~342-440).

## 3. Background (why)
On T3 and LL2 the last LP round leaves `sep` rows stretched. Each stretched pair kept the side (axis and direction) the pipeline picked from a reference drawing. We want to know, for each stretched pair, whether ANY of its candidate sides can hold while every other keep-apart row may stretch. A pair is a `sep <face> <i> <j>` tag. Its candidates are the (axis, side) combinations the `sides_from` loop already enumerates: candidate index `c = axis_index * 2 + side`, `side 0` meaning "from wall i to wall j" (the loop's `from = ends[side]`).

## 4. Tests
This step adds no behaviour to the default path: every new option is off by default. There is no red test. The hidden test is the measurement. The fast and gate suites must stay green.

## 5. Changes

### 5.1 `PlanarDrawOptions` (planar_draw.h), two new members
- `std::vector<std::pair<std::string, int>> forced_sides;` doc: `///< A probe: with sides_from, these sep pair tags ("sep <face> <i> <j>") take the given candidate (axis index * 2 + side) instead of the best one, written as one hard row tagged "sephard <face> <i> <j>" (sep_extremal endpoints).`
- `std::vector<std::string> margin_tags;` doc: `///< A read-out: with sides_from, for these sep pair tags every candidate's margin on the reference is listed in PlanarDrawResult::sep_margins.`

### 5.2 `PlanarDrawResult` (planar_draw.h), one new member
- `std::vector<std::pair<std::string, std::string>> sep_margins;` doc: `///< A read-out (margin_tags): per pair tag, each candidate as "c<k> s(x,y) spacing <n> from i|j margin <m>", joined by "; ".`

### 5.3 The `sides_from` block (planar_draw.cpp)
Inside the existing candidate loop (`for (const Axis &axis : axes) for (int side ...)`), where `margin` is computed:
- When the pair's tag is in `options.margin_tags`, append that candidate's text (format above, `from` = `i` when `from == &pi`) to a local string; after the loop push `(tag, text)` onto `result.sep_margins`.
- When the pair's tag is in `options.forced_sides` with candidate `c`, remember that candidate's axis pointer, `from` and `to` (as `forced_from`/`forced_to` already do for the walk rule, but with new local names, e.g. `probe_axis`, `probe_from`, `probe_to`).
After the loop and after the existing `have_forced` block, and before the `free_sides` test: if the pair is in `forced_sides` and `c < 2 * axes.size()`, write exactly the rows the plain path writes with `sep_extremal` (the `along` rule picking one endpoint of each), using the probe candidate instead of `best_*`, with tag `fmt::format("sephard {} {} {}", f.face, i, j)`; push to `result.sep_sides` as the plain path does; then `continue`. If `c >= 2 * axes.size()`, write nothing for this pair (no row at all), push `(tag, "candidate out of range")` to `result.sep_sides`, and `continue`.
- Worked shape (the rows, where `pa` is the probe axis, `pf`/`pt` the probe ends):
  ```cpp
  const std::array<uint32_t, 2> &p = (*pf)[along(pf, pa) > 0 ? 1 : 0];
  const std::array<uint32_t, 2> &q = (*pt)[along(pt, pa) > 0 ? 0 : 1];
  Terms t;
  for (int d = 0; d < 2; d++) {
  	t.emplace_back(q[d], pa->s[d]);
  	t.emplace_back(p[d], -pa->s[d]);
  }
  row(std::move(t), pa->spacing, JunctionMilp::INF, fmt::format("sephard {} {} {}", f.face, i, j));
  ```
  The existing `along` lambda reads `best_axis`; give it the axis as a parameter (`along(ends_of, axis)`) and update its existing call sites to pass `best_axis`. That is the only change to existing code paths.
- "sephard" is not in any `elastic_families` list, so the row is hard (the family test at line ~265 matches the first word exactly; confirm this when you read it, and stop if it does not).

### 5.4 `PlanarDrawPipelineOptions`, one new member
- `bool side_probe = false;` doc: `///< A probe: after the last LP round, per stretched sep pair and per candidate side, one LP like the last round with that pair's side hard (forced_sides) and every other row as in the last round; logged as "side probe".`

### 5.5 `DrawPlanarPipeline`
- Keep the lengths of the last Optimal round's input next to `last_input`: a local `std::vector<std::vector<double>> last_input_lengths` (use the exact element type of `PlanarDrawResult::lengths`) set from the same `prev` at every place `last_input` is set from `prev.points`.
- After the `free_stretched_sides` block, when `options.side_probe`:
  1. Collect the stretched pairs: every tag in `last.stretched` starting with `"sep "`, each once.
  2. One margins run: `CheckPlanarDrawable` with the options of the commit-stage round (copy the option list of the `commit_rounds` call at line ~1569 exactly), but `node_gap_from = &out.rounds[0].lengths`, `sides_from = &out.rounds[0].points` (the seed, which is discovery round 1's reference) and `margin_tags` = the stretched tags. Log each `sep_margins` entry as `side probe seed margins <tag>: <text>`.
  3. For each stretched tag, for `c` = 0, 1, 2, 3: `CheckPlanarDrawable(v, 15.0, ...)` with the same commit-stage option list, but `node_gap_from = &last_input_lengths`, `sides_from = &last_input`, `forced_sides = {{tag, c}}`, `margin_tags = {tag}`. Log one line: `side probe <tag> c<c>: <status> slack <total slack, 3 decimals> sep <sep-family slack sum> curve <curve-family slack sum> ms <ms>`; on the first `c` of each tag also log the `sep_margins` text as `side probe last margins <tag>: <text>`.
  - Do not push these results into `out.rounds` and do not call `log_round` for them (the final must stay the last entry pattern the test expects); append the lines to `out.log` directly.
  - Use the same seconds argument (15.0) as the free-sides round.

### 5.6 The hidden test "planar draw: the pipeline on T2, T3 and LL2"
- `o.side_probe = std::getenv("PLANAR_PROBE_SIDES") != nullptr;` next to the other env reads, and add `PLANAR_PROBE_SIDES (per-pair side probe)` to the comment listing the measurement aids.
- Step (a) read-out, only when `o.side_probe`, after the decode block: for each stretched `sep` tag of `r.last_round.stretched`, decode its two walls as the decode block does; for each wall that is a track (not box), print once per track per hub:
  `<hub> track <name>: pieces <headings>; flex <flex middles>; steps <steps>; stretched piece <k>; beside of <list of beside tracks whose ParentChain contains this track, or "none">; is beside <yes|no>`.
  Compute "beside of" with the `ParentChain` loop the test already has (every `b` with `map.part_of[b] == UINT32_MAX`), collecting chains once per hub into a map from track to beside names.
- Nothing else in the test changes.

## 6. Do not
- Do not change any default, any existing option's meaning, or the rows any existing path writes (apart from the `along` parameter refactor, which must write identical rows).
- Do not add the binary round of step (c); that is a later step.
- Do not run PSG 197 or any test outside the gates and the one hidden test.
- Do not interpret results beyond tabulating them; no fixes.

## 7. Stop conditions
- The candidate loop does not enumerate exactly `2 * axes.size()` candidates in the order `axis_index * 2 + side`, or `axes.size()` differs from 2 on some pair (report the count you see; keep going only if every stretched pair has 2 axes).
- The elastic family test is not an exact first-word match.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 8. Gates
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flake: "planar components: T2 mapped lands in its box at the anchor"; rerun alone and report.
- The hidden test once, by name, with `PLANAR_PROBE_SIDES=1`, output to `build-rel/sideprobe.log` (e.g. `build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/sideprobe.log 2>&1`). Copy the log to `/home/pete/OpenTTD/docs/junction-tool/wip/probes/e5land/sideprobe-<head hash>.log` in the same commit.
- One commit: `Add: Junction: per-pair side probe for the keep-apart crowding (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 9. Report (short, fixed format)
- References read
- Commits: hash and subject
- Types and members added or removed: names only
- Suite results (count and time per gate)
- From the log, for T3 and LL2 only: a table, one row per stretched tag: the two walls (decode text), the side the last round used, c0..c3 status and sep slack, and the seed margins and last margins of each candidate. Then the (a) track lines verbatim. Then the last-round slack and wall time per hub.
- Stops and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
