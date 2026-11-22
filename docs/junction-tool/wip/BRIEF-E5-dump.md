# BRIEF: E5 drawing dumps and an offline reader (debugging backbone)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 75 minutes in total. Solves keep the caps their tests already use. Run each measurement once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Why: every E5 question so far needed a new hidden test that WARNs decoded text (sep tags decoded by hand in about 8 places of src/tests/junction/planar/planar_draw.cpp). This step makes any drawing run write one JSON file holding everything needed to answer such questions offline with a script, plus the script.

## 2. Read first (once each)
- `CODINGSTYLE.md`, C++ part.
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions` (`export_path`, `sides_from`, `margin_tags`), `PlanarDrawResult` (`status`, `slack`, `stretched`, `points`), `PlanarDrawPipelineOptions`, `PlanarDrawPipelineResult` (`last_round`, `last_input`).
- `src/junction/planar/planar_draw.cpp`: the `exporting` block (~line 233 and ~1563, how the JSON is written today, `json_number`); the pairwise block from `if (options.clearance && options.pairwise) {` to the end of the `sides_from` candidate loop (~1300-1372: `axes`, `ends`, `pi`/`pj`, the margin formula); `DrawPlanarPipeline` (the seven `CheckPlanarDrawable` calls).
- `PlanarShapeFaceWalls` / `PlanarFaceWalls` / `PlanarCellWall`.

## 3. Changes
### 3.1 `PlanarDrawOptions::dump_path` (production, a measurement aid)
- `std::string dump_path; ///< A measurement aid: when not empty, the drawing is written there as JSON after the solve (tracks, faces, rows that stretched, every keep-apart pair with its candidates and their margins on the drawing).`
- During the pairwise block, for every pair that gets rows (every branch: binaries, `sides_from`, forced, free), record when `dump_path` is set: the pair tag, the two walls (track, piece), and each candidate written or considered: index c, axis `s`, spacing, `from` (i or j), and which candidate the rows use (`chosen`, -1 when binaries decide). Store these in a local vector; do not change any row.
- After the solve, when `dump_path` is set and the result has points, write one JSON object:
  - `status`, `slack`;
  - `tracks`: per track `{name, headings: [...], points: [[x,y],...]}` (headings = the shaped track's pieces mod 8; points = `result.points[t]`, empty when none);
  - `faces`: per face of `PlanarShapeFaceWalls` `{face, walls: [{track, name, piece, heading}]}` (dummy/box walls with `track` = -1 and `name` "dummy"/"box" as the code marks them);
  - `stretched`: `[{tag, slack}]` from `result.stretched`;
  - `pairs`: per recorded pair `{tag, i: {track, piece}, j: {track, piece}, chosen, candidates: [{c, s: [x,y], spacing, from, margin}]}`, where `margin` is the same formula as the `sides_from` loop (least over the two endpoints of each wall of `(q - p) . s - spacing`, p on the `from` wall) evaluated on `result.points`; for binary runs `chosen` is the candidate whose binary is 1 if readable, else -1.
- With `dump_path` empty nothing changes (no extra work, same rows).

### 3.2 Pipeline
- `PlanarDrawPipelineOptions::dump_dir` (`std::string`, doc `///< A measurement aid: when not empty, each stage's drawing is dumped there as <stage>-<n>.json (seed, round, commit, prune, final).`). Pass `dump_path` = `dump_dir + "/<stage>-<n>.json"` into each of the seven calls; `<stage>` one of seed, round, commit, prune, final; `<n>` the round counter the code already keeps (0 when none).

### 3.3 Test hooks (env, hidden behaviour only)
- Hub pipeline test ("planar draw: the pipeline on T2, T3 and LL2", and the T1/197 pipeline tests if they share the options-building helper; otherwise only this one): `PLANAR_DUMP=<dir>` sets `o.dump_dir = <dir>/<hub>`; create the directory.
- "PSG 197 clearance rows evaluated on V's drawing": with `PLANAR_DUMP=<dir>`, the "sepV allfaces W1" run and the `PLANAR_PROBE_VRES` hybrid run (step 1) write `<dir>/197v/sepv-allfaces.json` and `<dir>/197v/vres.json`; also write `<dir>/197v/vref.json` = `{tracks: [{name, points}]}` from `vref` (empty lists kept).

### 3.4 Offline reader `tools/junction/planar_dump.py` (Python 3, standard library only)
Subcommands, each reading one or two dump files:
- `rows FILE [--family F] [--top N]`: stretched rows, largest first, with sep tags decoded to `track#piece hH` for both walls.
- `pair FILE TAG`: both walls with their endpoints, every candidate with margin (chosen marked), and the face walk both ways (walls in between as `track#piece hH len L`, L = max(|dx|,|dy|) of the wall's points).
- `face FILE ID`: the face's walls in order with endpoints and lengths, and its stretched rows.
- `diff A B`: pairs present in both whose `chosen` differs, with each side's margins.
- `svg FILE (--face ID ... | --tracks NAME ...) -o OUT.svg [--ref FILE2]`: draw the listed faces' walls (or the listed tracks), labelled `track#piece`, stretched sep pairs as red dashed lines between the walls' midpoints, y down; with `--ref`, the same tracks from FILE2 in grey underneath. Plain SVG, viewBox from the points, stroke widths readable at 800 px.
- `--help` on each.

### 3.5 Docs
- `src/junction/planar/CLAUDE.md`: one bullet for `dump_path` / `dump_dir` and the reader (a measurement aid, off by default).
- `docs/junction-tool/TESTS.md` or the nearest tools README: one line naming `tools/junction/planar_dump.py` and `PLANAR_DUMP` (pick the file that already lists `PLANAR_PROBE_*` env aids; if none, CLAUDE.md only).

## 4. Do not
- No change with `dump_path` empty. No change to rows, defaults or solves. Do not refactor the existing WARN decoders. No third-party Python packages.

## 5. Stop conditions
- A pair's candidates cannot be recorded in one branch without restructuring the block: record the branches you can, say which you could not.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- `PLANAR_DUMP=build-rel/dumps build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/dump-hubs.log 2>&1` (exact name from the file).
- `PLANAR_DUMP=build-rel/dumps PLANAR_PROBE_VRES=1 build-rel/openttd_test "PSG 197 clearance rows evaluated on V's drawing" > build-rel/dump-197v.log 2>&1`.
- Reader checks, outputs to files: `python3 tools/junction/planar_dump.py rows build-rel/dumps/197v/vres.json --top 10`; `pair` on its largest sep row; `face` on that row's face; `svg` of that face with `--ref build-rel/dumps/197v/sepv-allfaces.json`; `diff` of T3's last round against its final. The `rows` output must show the same top rows and slacks as the log's `vres row` lines.
- Copy the dumps for T3's last round and final and the three 197v files to `docs/junction-tool/wip/probes/e5land/dumps-<short head hash before your commit>/`, same commit (skip any file over 5 MB and say so).
- One commit: `Add: Junction: drawing dumps and an offline reader (E5 debugging)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed; members added.
- Suite results (count and time per gate).
- Dump sizes; the reader check outputs (rows top 10, the pair, the face, the diff count), each trimmed to 30 lines; the SVG path.
- Whether `rows` matched the log. Stops and anything left undone. State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
