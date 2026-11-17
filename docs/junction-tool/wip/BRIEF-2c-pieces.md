# Brief 2c: fans and bundle leads as required pieces (no '+'/'-' switches)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, head `{HEAD}`, build dir `build-rel` (Release).
- Plan: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-pieces.md` §2 (every piece required) and §6 (removed: optional segments and their switches). The previous step (2b) removed the jog switches (`flex{c}j`, `join{s}j`).
- What is left in `MappedComposer` (`src/junction/planar/planar_components.cpp`, class from line 751) as optional pieces whose switch the width profile already fixes:
  - n-lane `Fray` (line ~891), group `fray{s}+` (prio, switch fixed on) or `fray{s}-` (plain, fixed off): the fan piece `AddSeg(x, Lt, 2, …, true)` (~899) and the plain branch's `AddSeg(S[j], Lt, 2, …, true)` (~921).
  - `Bundle` (~1026), group `bundle{c}+` (fixed on): the first piece's lead when `opt0` (~1037), capped at `OPTMAX * max(1, n - 1)`.
- A switch fixed on means: the piece exists, length at least its `lo`, at most `OPTMAX` (or the given cap). Fixed off means length 0. This step emits the fixed-on pieces as ordinary required pieces with no cap and emits no piece for the fixed-off ones.

## 2. Read first (once, offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md` lines 55-80.
- `src/junction/planar/planar_model.h` lines 335-380 (`AddSeg`, how a fixed switch bounds a piece).
- `planar_components.cpp` lines 891-930 (`Fray`), 1022-1060 (`Bundle`), 1180-1232 (the fray call and `build_bundle`).

## 3. Baseline first (launch head), output to files
- Full T3: `build-rel/openttd_test "planar components: T3 from its topology text (mapped) at the hand composition's area" -s`; record the INFO line (variant, area, size, ms) and wall time.
- Full T2: `"planar components: T2 from its topology text (mapped) at the probe's area"`, same.
- One-lane T: `"planar components: the one-lane T from its topology text (mapped)"`, same.
- `PLANAR_DRY=1` with the test `"planar components: LL² from its topology text (mapped) at the probe's area"`: save the `DRY` lines from stderr, sorted.

## 4. Changes (only in `MappedComposer`)
1. `Fray`: add a parameter is not needed; use the existing `prio`:
   - fan, per in lane: `prio` → `M.AddSeg(x, Lt, 2, std::nullopt, 1.0)` (required); not `prio` → no piece (the `STUB` run is followed directly by the `FANW` run).
   - plain branch (`!prio`) S[j]: remove the `Lt` piece; `S[j]` gets `AddSeg(S[j], h, 2)` then `d0 = AddSeg(S[j], h, 2)` as now, with nothing between.
2. The fray call (~1191-1193): remove the two `M.opt_group` lines and their comment.
3. `Bundle`: the lead is `M.AddSeg(l, h, 0, std::nullopt, IsAxis(h) ? 1.0 : std::sqrt(2.0))` for every piece (no `opt`, no cap). Drop the `opt0` parameter and the local `o`; update the doc comment ("the first piece optional when asked" goes).
4. `build_bundle`: remove the `opt0` computation (`fray`, `opt0` locals) and the two `M.opt_group` lines with their comment; call `Bundle(c, heads, doubled)`. If `fray`/`site_of` become unused there, remove only the now-unused local.
5. `src/junction/planar/CLAUDE.md`: replace the sentence "The width profile fixes the switches it decides: a prio fray's fan and a diverging bundle's first piece on ('+'), a plain fray's step off ('-'); …" with the facts as they now are (a prio fray's fan and every bundle piece are required pieces; a plain fray has no fan; the only optional pieces left in the mapped composer are a one-lane join's exit jog and a fork's legs). Keep `OPTMAX` (still used by those and by the hand composer). Update the T3/T2 timing facts with the numbers you measure in §5.

## 5. Measure after
The same four runs as §3. Report a before/after table.

## 6. Do not
- Do not touch the hand composer (`class Composer`, lines 155-733), `Fork` (~815), `Join`, `Flex`, `merge{n}j`, `OPTMAX`, `planar_model.*`, curve rows, the up-front side rule.
- No env knobs.

## 7. Stop conditions
- T3 area above 1941.25 or T2 above 917.25 after the change, or the one-lane T newly failing: stop before committing, report the numbers.
- The LL² DRY lines differ between before and after: stop before committing, report the diff.
- Any wall time more than doubles: report it (do not stop for that alone).

## 8. Gates
- `tools/junction/jt fast build-rel '[planargaps]'`, `'[planarmodel]'`, `'[planar]'`, `'[planarcomponents]'` (the last runs about 6 s against 5 s at the launch head in hand-composer tests; report its time, do not chase it).
- One commit: `Change: Junction mapped composer builds fans and bundle leads as required pieces`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 9. Report (short)
References read; commit; the before/after table (variant, area, size, ms, wall); DRY comparison; gate results with count and time; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
