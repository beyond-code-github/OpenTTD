# BRIEF: E5 side probe, whole faces free, then the IIS of what pins them (read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 45 minutes in total. Each solve has a 15 s cap. Run the hidden hub test once (a second time only if the first crashed or printed nothing from the probe). No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Previous steps: 7372d911eb, 1f7d6f187d, a9ed91bf0b (the `side_probe` block in `DrawPlanarPipeline`: per-pair forced sides, free inserts, and the "joint" solves with binaries on just the stretched pairs; the `free_sides` branch writes `sep_extremal` rows). Result: the stretched pairs do not fit together even with inserts free (T3 12, LL2 7), and slack moves onto neighbouring pairs whose sides stayed fixed. This step frees every pair of the faces involved, then asks which hard rows pin those faces.

## 2. Read first (once each)
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions` (`free_sides`, `only_faces`), `PlanarDrawResult` (`conflict`, `points`, `lengths`).
- `src/junction/planar/planar_draw.cpp`: the pairwise `sides_from` block's `free_sides` test (search `options.free_sides`); the `side_probe` block (`git show a9ed91bf0b -- src/junction/planar/planar_draw.cpp`); how the pipeline gets `PlanarShapeFaceWalls(v)` and track names (`v.structure.map.tracks[t].name`) if it already does anywhere in the file; else the hidden test's decode block in `src/tests/junction/planar/planar_draw.cpp` (~line 395) shows the pattern.

## 3. Changes

### 3.1 `PlanarDrawOptions` (planar_draw.h), one new member
- `std::vector<uint32_t> free_faces;` doc: `///< A probe: with sides_from, every sep pair of these faces gets the binary side choice, as free_sides does for single pairs.`

### 3.2 The `free_sides` test (planar_draw.cpp)
- A pair takes the binary branch also when `f.face` is in `options.free_faces`. Nothing else in the branch changes.

### 3.3 The `side_probe` block, after the joint solves
Let `faces` be the distinct face numbers of the stretched tags (`sep <face> <i> <j>`).
1. **"faces free"**: one solve as the "joint free" solve (commit-stage options, `curve_flex_from = nullptr`, `node_gap_from = &last_input_lengths`, `sides_from = &last_input`, `elastic_integer = true`, `continuous = {"point", "axis", "diagonal", "param", "cross"}`), with `free_sides = tags` and `free_faces = faces`, 15 s. Log `side probe faces free: <status> slack <total> sep <sep sum> curve <curve sum> binaries <n> sides_free <n> faces <list> ms <ms>` and one `side probe faces free stretched <tag> <amount>` line per stretched row.
2. **"faces hard"**, only when step 1 is Optimal: one LP with the commit-stage options, `curve_flex_from = nullptr`, `node_gap_from = &<step 1 result>.lengths`, `sides_from = &<step 1 result>.points` (every pair re-picks its side from the step 1 drawing), `elastic_families = {"curve"}` (so every `sep` row is hard), 15 s. Log `side probe faces hard: <status> slack <total> ms <ms>`. When it is not Optimal, log every conflict tag (no cap), one per line: `side probe faces hard conflict <tag>`, and for a tag of the form `sep <face> <i> <j>` append ` = <track>#<piece> h<heading> | <track>#<piece> h<heading>` decoded from `PlanarShapeFaceWalls(v)` (`box` for a wall with no track), as the hidden test's decode block does.
- Append to `out.log` directly; do not touch `out.rounds` or `log_round`.

## 4. Do not
- Do not change any default or any path other than the `free_sides` test. Do not run PSG 197. Do not interpret beyond tabulating.

## 5. Stop conditions
- Step 1 reaches its time limit without an incumbent: log it, skip step 2, report.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- The hidden test once with `PLANAR_PROBE_SIDES=1`: `build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/sideprobe-faces.log 2>&1`. Copy the log to `docs/junction-tool/wip/probes/e5land/sideprobe-faces-<short head hash before your commit>.log` in the same commit.
- One commit: `Add: Junction: side probe with whole faces free and the IIS of the hard faces (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed; members added.
- Suite results (count and time per gate).
- For T3 and LL2: the "faces free" line and its stretched rows; the "faces hard" line; every "faces hard conflict" line verbatim (join Catch's 80-column wraps first). If there are more than 80 conflict lines for a hub, give the family counts (first word) and every `sep`, `node`, `leg`, `bay`, `corridor` and `pin` line verbatim, and leave the rest in the log.
- Stops and anything left undone.
- State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
