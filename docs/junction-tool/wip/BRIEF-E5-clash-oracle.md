# BRIEF: E5 an independent clash check on drawings, and each clash traced to its skip rule

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 75 minutes in total. Solves keep their existing caps. Run each measurement once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `build-rel`.
- Context (card docs/junction-tool/wip/CARD-E5-bundle-ends.md, last bullet): the pipeline's "clean" final drawings of T1 and T2 contain overlapping tracks (T1 A1#0/B1#1 12 half tiles on one diagonal; T2 seven overlaps, e.g. D1#9/D2#9 14 long on one row), and F2 touches D2 off the listed crossing. The drawing program writes no keep-apart row for those wall pairs: its skip rules (adjacent walls, span piece, same piece, zero contraction) exempt them. This step adds a check that does not trust the drawing program's rows, and names the skip rule behind each clash.

## 2. Read first (once each)
- `tools/junction/planar_dump.py` (whole file) and the dump writer in `src/junction/planar/planar_draw.cpp` (`dump_path`, `DumpPair`).
- The pairwise block's skip rules in planar_draw.cpp (where a pair is skipped before rows are written: adjacent, span, same piece, zero contraction, and any other `continue` before the candidates).
- `PlanarTrackMap::crossings` and the expanded topology's nodes (`map.expanded.lines[t].from/.to`) in src/junction/planar/planar_regions.h.

## 3. Changes
### 3.1 Dump additions (production, only when `dump_path` is set)
- `skipped`: every wall pair the pairwise block considers and skips, `{tag, reason}` with reason one of `adjacent`, `span`, `same piece`, `zero contraction`, or the name of any other skip branch you find (one word each, list them in the report).
- `crossings`: per map crossing `{a, b}` (the two track names).
- `ends`: per track `{name, from, to}` as node or port ids (strings like `node 12`, `port 3`), so the checker knows which tracks may meet at a point.
### 3.2 `planar_dump.py clash FILE`
For every pair of different tracks with points:
- **overlap:** two segments collinear and sharing a stretch of positive length (always a clash);
- **meet:** two segments intersecting or touching at a point, except (a) a point that is an end point of both tracks where they share a node or port id, and (b) intersections of a pair listed in `crossings`, allowing as many such points as the pair has crossings (report extra ones).
For each clash print the two `track#piece` with points, the kind, and, by finding both segments as walls of a common face in `faces` (match track and piece), the pair tag and whether that pair is in `pairs` (written, with chosen side and margin) or in `skipped` (with its reason), or neither (not in a common face).
Summary line: `clash <file>: overlaps <n>, meets <m>`, and counts by skip reason.
### 3.3 Dumps to check
- With `PLANAR_DUMP=build-rel/dumps`: the hub pipeline test (T1, T2, T3, LL2 as today) and the any-hub test with `PLANAR_ANYHUB_CONTRACT=1 PLANAR_ANYHUB_GRID=1`; in the any-hub K3 block set `po.dump_dir = <dir>/anyhub-k3/<hub>` when `PLANAR_DUMP` is set (create the directory), so 197's contracted grid final is dumped.
- Run `clash` on every `final-0.json` and every last `round-*` / `prune-*` / `commit-*` file present, per hub.

### 3.4 Docs
- `src/junction/planar/CLAUDE.md`: extend the dump bullet with `skipped`, `crossings`, `ends` and the `clash` command (one sentence).

## 4. Do not
- No change to rows, skip rules, defaults or solves. No fix. No third-party Python packages.

## 5. Stop conditions
- A skip branch cannot be given a reason without restructuring the block: name it `other` and say where it is.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s; known flakes as before, rerun alone and report).
- The runs in 3.3, logs to build-rel; `clash` outputs to `build-rel/clash-<hub>-<file>.txt`.
- Copy the clash outputs (not the dumps) to `docs/junction-tool/wip/probes/e5land/clash-<short head hash before your commit>/`, same commit.
- One commit: `Add: Junction: an independent clash check on drawings (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits; files changed; members added; the skip reasons found.
- Suite results.
- Per hub and file: the summary line; for T1, T2 and 197's final every clash line verbatim (cap 30 per file).
- Stops and anything left undone. State.

When you have reported, stop.
