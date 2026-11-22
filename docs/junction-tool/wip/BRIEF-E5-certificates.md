# BRIEF: E5 crowding certificates on T3 and LL2 (exact single-face solves; prio removed), read-outs only

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 60 minutes in total. Each solve has a 15 s cap. Run the hidden test once (a second time only if the first crashed or printed nothing). No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Why: every earlier read-out on the T3/LL2 crowding had sides read from drawings inside it, so none proves anything. This step gets certificates. (C) is an exact solve: every keep-apart pair in a few faces gets binaries, nothing is guessed, and other faces' keep-apart is left out (a relaxation). Infeasible proves no drawing exists with these headings; Optimal shows the faces fit. (A) re-runs the pipeline with the prio statement removed, with a heading diff as the control. Prompt and reply: `/home/pete/OpenTTD/docs/junction-tool/wip/gemini/2026-11-22-zoom-out.md`.

## 2. Read first (once each)
- `CODINGSTYLE.md`, C++ part.
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions` (`pairwise`, `pairwise_all_faces`, `only_faces`, `continuous`, `tidy`, `corridor`), `PlanarDrawResult` (`status`, `conflict`, `binaries`, `ms`, `points`).
- `src/junction/planar/planar_draw.cpp`: `COORD_MAX` (line ~145) and every use of it inside `CheckPlanarDrawable` (`grep -n COORD_MAX`), including `M = 4 * COORD_MAX` in the pairwise block.
- `src/tests/junction/planar/planar_draw.cpp`: `ShapeFirst`, the hidden test "planar draw: the pipeline on T2, T3 and LL2" (its decode of sep tags with `PlanarShapeFaceWalls`), `PlanarTText`, `PlanarLL2Text`.

## 3. Changes

### 3.1 A coordinate bound option
- `PlanarDrawOptions`: `double coord_max = 4096;` doc `///< The largest coordinate, length and parameter (half tiles); the pairwise big-M is 4 x this. A tighter bound tightens the binary rows (a measurement aid for exact solves).`
- In `CheckPlanarDrawable`, every use of `COORD_MAX` reads `options.coord_max` instead (keep the constant as the default's source if you like: `double coord_max = 4096` may be written as `= COORD_MAX` only if the constant stays declared before the struct; otherwise keep the literal and the constant equal). With the default, behaviour is identical.

### 3.2 New hidden test "planar draw: crowding certificates on T3 and LL2", tag `[.][planarcert]`
**(C) exact face solves.** For each case below, `ShapeFirst(text, 1)` (the same variant the hub test uses), then `CheckPlanarDrawable(c.v, 15.0, o)` with:
`o = {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .only_faces = F, .corridor = true, .tidy = false, .continuous = {"point", "axis", "diagonal", "param", "cross"}, .coord_max = B}`. Everything else is default: no `sides_from`, so every pair in F gets binaries; inserts free (no `curve_flex_from`); bays and curves on; node gaps off.
- Cases (hub, F, B): T3 {75} 256; T3 {29} 256; T3 {66} 256; T3 {75, 29, 66} 256; T3 {75} 4096 (control: the loose bound); LL2 {33} 256; LL2 {34} 256; LL2 {37} 256; LL2 {33, 34, 37} 256.
- WARN per case: `cert <hub> faces <F> B <B>: <status> ms <ms> binaries <binaries>`. On Infeasible, WARN every conflict tag, one per line (`cert <hub> faces <F> conflict <tag>`), with sep tags decoded as the hub test does. On Optimal, WARN the box (width, height) if the result has it, and for each wall pair of F that the hub test's last round listed as stretched on that hub (T3: sep 75 2 15, 75 5 8, 29 1 10, 29 4 9; LL2: sep 37 6 12, 33 0 9, 33 1 9, 33 2 9, 33 3 10, 33 3 8, 34 1 9, 34 3 7, 37 6 11), WARN the two walls' end points from `r.points` (decode format).
- Before the cases, WARN once per hub that the face numbers refer to this variant: print the walls of each face in F (track#piece h<heading>) so the reader can check they are the faces named above. If a face in F does not exist, WARN it and skip the cases that use it (no stop).

**(A) prio removed.** For T3 (`PlanarTText(3)`) and LL2 (`PlanarLL2Text()`): build the text with every line starting with `prio` removed; `ShapeFirst(stripped, 1)`; `DrawPlanarPipeline(c.v, o)` with defaults and `o.final_seconds = 15`.
- WARN `noprio <hub>: last round slack <s>, by family <family: count, sum ...>, final <status>, stopped '<stage>'`, and each stretched row decoded as the hub test does.
- Control: also build `ShapeFirst(text, 1)` with the prio statement kept. For every track name present in both variants' maps whose name does not start with `prio:` and contains no `"` (no rows, no copies), compare the heading lists (`v.tracks[t].pieces` mod 8 as a list). WARN `noprio <hub> headings: <n> identical, <m> differ`, then one line per differing track: `<name>: with [..] without [..]`.

## 4. Do not
- No change to any default or any existing behaviour (the `coord_max` refactor must be identical at 4096). No pipeline change. Do not interpret beyond tabulating.

## 5. Stop conditions
- `ShapeFirst` fails on a stripped text (the parser or Structure refuses a topology without prio): report the message and run only (C).
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- `build-rel/openttd_test "planar draw: crowding certificates on T3 and LL2" > build-rel/cert.log 2>&1`; copy to `docs/junction-tool/wip/probes/e5land/cert-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: exact face certificates and a no-prio run for the T3/LL2 crowding (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed; members added.
- Suite results (count and time per gate).
- (C): a table, one row per case: hub, faces, B, status, ms, binaries. Then every conflict line of each Infeasible case verbatim (join Catch's 80-column wraps first), and the face wall listings.
- (A): the noprio lines per hub, the stretched rows decoded, the heading control lines verbatim.
- Stops and anything left undone.
- State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
