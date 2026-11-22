# BRIEF: E5 facing pairs take their side from the face walk (D426 probe)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 75 minutes in total. Existing caps on every solve. Run each measurement once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Ruling: D426 in `docs/junction-tool/DECISIONS.md` (main checkout `/home/pete/OpenTTD`): inside bundle faces, at least one lane with the side fixed by the planar map and Shape's headings, never read from a drawing; D418 sides only for pairs this rule does not decide. Card: `/home/pete/OpenTTD/docs/junction-tool/wip/CARD-E5-bundle-ends.md`.
- Design section: src/junction/planar/CLAUDE.md, Drawing program, the pairwise bullet (`sides_from`, candidates) and `sides_by_walk` / `walk_strict` in planar_draw.h.

## 2. Read first (once each)
- `CODINGSTYLE.md`, C++ part.
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions` (`sides_from`, `sides_by_walk`, `walk_strict`, `sep_extremal`), `PlanarDrawResult` (`walk_fixed`, `sep_sides`), `PlanarDrawPipelineOptions`.
- `src/junction/planar/planar_draw.cpp` from `if (options.clearance && options.pairwise) {` (~line 1151) to the end of the `sides_from` branch: the `Axis` struct and `axes` (axes[0] is wall i's inward normal `STEP[(h_i + 6) % 8]`; axes[1] is wall j's inward normal when the walls differ mod 4, else the shared direction), the `monotone(from, to, normal)` lambda, the `forced_axis` / `forced_side` logic, the candidate loop (`axis_index`, `side`; side 0 = from wall i to wall j), the `have_forced` block, `sep_sides`.
- The hidden test "planar draw: the pipeline on T2, T3 and LL2" and "planar draw: the pipeline on PSG 197 (D422)" in `src/tests/junction/planar/planar_draw.cpp` (their env reads and the per-round side print).

## 3. The rule (settled; implement exactly)
For a pair (i, j) in the `sides_from` branch, with the existing `monotone` lambda and `n_i = STEP[(w[i].heading + 6) % 8]`, `n_j = STEP[(w[j].heading + 6) % 8]`:
- **facing** when `monotone(i, j, n_i) && monotone(j, i, n_j)`.
- **facing, anti-parallel** when facing and `(w[j].heading - w[i].heading) mod 8 == 4`: the pair takes the single candidate (axis 0, side 0), j at least the spacing beyond i along n_i. No margin comparison.
- **facing, other angle** when facing and the walls differ mod 4: the best candidate is chosen by the existing margin rule (largest least margin over the reference, first on a tie), but only between (axis 0, side 0) and (axis 1, side 1).
- **facing with equal walked headings** (`w[j].heading % 8 == w[i].heading % 8`) is degenerate: treat as not facing.
- Every other pair: unchanged (D418, all candidates by margin).
- The written rows are the ones the chosen candidate writes today (with `sep_extremal`, the extremal endpoints); no new row family, the tag stays `sep <face> <i> <j>`.
- The rule applies only when the new option is on and only in the `sides_from` branch. It takes precedence over `sides_by_walk` if both are on (do not combine them; with `facing_sides` on, skip the `sides_by_walk` forcing for that pair).
- The `free_sides` / `free_faces` / `forced_sides` probes keep precedence over this rule (they are later in the branch or earlier; keep their current precedence and apply this rule only where the pair would otherwise take the plain best-margin path).

## 4. Changes
- `PlanarDrawOptions`: `bool facing_sides = false;` doc `///< D426 probe: with pairwise and sides_from, a pair whose face walk is monotone both ways (i to j towards i's inward normal, j to i towards j's) takes its side from the walk: anti-parallel walls the common normal, other angles the better of the two inward normals on the reference; other pairs keep sides_from's choice.`
- `PlanarDrawResult`: `uint32_t facing_fixed = 0;` (anti-parallel pairs fixed), `uint32_t facing_choice = 0;` (other-angle facing pairs). For a facing pair, append ` facing` or ` facing2` to that pair's `sep_sides` text.
- `PlanarDrawPipelineOptions`: `bool facing_sides = false;` passed as `.facing_sides` into every `CheckPlanarDrawable` call of `DrawPlanarPipeline`, exactly as `.sibling_lanes` is.
- Both hidden pipeline tests: `o.facing_sides = std::getenv("PLANAR_PROBE_FACING") != nullptr;` beside the other env reads (add it to the comment listing the aids); when set, WARN `<hub> facing: fixed <facing_fixed> choice <facing_choice>` from the last round. The existing per-round side print already shows the ` facing` marks for T3 and LL2.
- `src/junction/planar/CLAUDE.md`, Drawing program section: one bullet for `facing_sides` (present tense, off by default, a probe, D426), same commit.

## 5. Tests (fast)
1. New test, tag `[planardraw]`: "planar draw: facing pairs take their side from the face walk on T2". `ShapeFirst(PlanarTText(2), 1)`; `DrawPlanarPipeline(c.v, o)` with `o.facing_sides = true`, `o.final_seconds = 5`. Assert `r.last_round.facing_fixed >= 1`, and that every `sep_sides` entry of `r.last_round` ending in " facing" names spacing and `from i` (the side 0 candidate: its text is produced by the existing format `"s (x,y) spacing n from i"`). Red: with the members added and the rule not yet applied, `facing_fixed >= 1` fails; record the message. If T2 has no anti-parallel facing pair, use T3 and report it; if neither has one, stop.
2. The gates below stay green with the option off.

## 6. Do not
- No change with the option off. No length bound (no `walk_strict`-style guaranteed projection). No change to Shape, flex, the commit or `sibling_lanes`.

## 7. Stop conditions
- `axes[1]` is not wall j's inward normal for walls that differ mod 4, or side 1 is not "from j to i": report what you see.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 8. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- M1: `PLANAR_PROBE_FACING=1 build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/facing-hubs.log 2>&1`
- M2: `PLANAR_PROBE_FACING=1 PLANAR_PROBE_SIBLINGS=1 build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/facing-sib-hubs.log 2>&1`
- M3: `PLANAR_PROBE_FACING=1 build-rel/openttd_test "planar draw: the pipeline on PSG 197 (D422)" > build-rel/facing-197.log 2>&1` (its final CHECK fails at baseline, a 60 s Error; known).
- Copy the three logs to `docs/junction-tool/wip/probes/e5land/facing-{hubs,sib-hubs,197}-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: facing pairs take their side from the face walk, a probe (D426)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 9. Report (short, fixed format)
- References read
- Commits: hash and subject; files changed
- Red evidence
- Types and members added or removed: names only
- Suite results (count and time per gate)
- Per measurement and hub (T1, T2, T3, LL2; 197 in M3): facing fixed / choice counts; the last round's slack by family; final status; stopped stage; for T3 and LL2 every stretched sep row with its decode line and, from the per-round side print, its last-round side text (with any facing mark). Join Catch's 80-column wraps first.
- Stops and anything left undone
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
