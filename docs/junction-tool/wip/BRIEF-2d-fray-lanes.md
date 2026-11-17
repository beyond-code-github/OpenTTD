# Brief 2d: a fray doubles and fans per lane (prio lanes only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, head `{HEAD}`, build dir `build-rel` (Release).
- Why: `/home/pete/OpenTTD/docs/junction-tool/wip/RECON-ll2-area.md` cause A. The mapped n-lane `Fray` (planar_components.cpp ~891) treats a fray as all-prio or all-plain by its straight channel (`rs.prio`): a prio fray doubles every near lane over the farther legs, spaces lanes at least 4 apart and has a required fan. On LL² every entry fray's straight channel has one prio lane (E1, F2, A1, B2, C1, D2), so every lane doubles and fans (EF fans 24/22, CD 30/28) where the hand composition has none.
- The rule (the checker's: a main line, with doubled bridges, is a lane whose path holds prio): in a fray, a near lane S[j] doubles over the farther legs exactly when its own line carries a prio route; otherwise it crosses them single. The fan exists exactly when some lane doubles. On the T every lane of B and E is prio, so the T is unchanged.

## 2. Read first (once, offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md` lines 55-80.
- `planar_components.cpp` lines 891-927 (`Fray`), 1172-1190 (`build_fray`), 1500-1517 (the in-port fan clash rule: it applies its fan part only to lanes whose second piece turns).
- `src/junction/planar/planar_topology.h` (`PlanarLine::routes`, `PlanarRoute::prio`, `PlanarLineByName`).

## 3. Baseline (launch head), output to files
- Per variant, the INFO line (`area ... size ... in ... ms`) or the failure message, for: T2 and T3 every variant (`o{0,1}t{0..3}f{0,1}s{0,1}`, 32 each, `PLANAR_ONLY=<tag>`) with the tests `"planar components: T2 from its topology text (mapped) at the probe's area"` and `"planar components: T3 from its topology text (mapped) at the hand composition's area"`. Save as `t-variants-before.txt` in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/2d/`.
- LL² (`"planar components: LL² from its topology text (mapped) at the probe's area"`), `timeout 60` each: o49t0f0s0, o49t1f0s0, o49t0f1s0, o1t0f0s0, o0t0f0s0. Expected at the launch head (from the recon): 2064.25, 1970.25, 1654.25, 2022.25, unsolved (budget).

## 4. Changes (only in `MappedComposer`)
1. `Fray(int h, int sigma, const std::vector<std::string> &ins, const std::vector<std::string> &S, const std::vector<std::string> &V, const std::vector<bool> &dbl)` replaces the `bool prio` parameter; `dbl[k]`: lane k's straight leg S[k] doubles over the farther legs (`dbl[0]` is unused: the far lane crosses nothing).
   - Fan per in lane: required `AddSeg(x, Lt, 2, std::nullopt, 1.0)` when any `dbl[k]` (k ≥ 1) is true; else no fan piece.
   - Lane spacing at the split points: `Ge((Dot(r, sp[k]) - Dot(r, sp[k - 1])) * sigma, dbl[k] ? 4 : 2)`.
   - S[j] (j ≥ 1): `dbl[j]` → today's prio branch (Double, run, PairDecks); else today's plain branch (run, deck run, crosses, Deck).
2. `build_fray`: `dbl[k] = rs.prio && the line named S[k] carries a prio route` (any `t.routes[r].prio` for `r` in `t.lines[line].routes`, line found with `PlanarLineByName`). Pass `dbl`.
3. `src/junction/planar/CLAUDE.md`: replace the fray facts ("a prio fray's fan …") with the per-lane rule as present-tense facts. Update the T2/T3 numbers only if they changed.

## 5. Measure after
The same runs as §3; save `t-variants-after.txt`. Report: whether every T2/T3 variant line is identical (diff), the T2/T3 best, and an LL² before/after table (area, size, ms).

## 6. Do not
- Do not touch `Join`, `Bundle`, `build_core`, the core's doubling, `Fork`, `merge{n}j`, the hand composer (`class Composer`), the clash rules.
- No env knobs.

## 7. Stop conditions
- Any T2 or T3 variant line differs between before and after: stop before committing, report the diff.
- Any LL² variant that built before now fails construction (a `M.Fail` reason): stop, report.

## 8. Gates
- `tools/junction/jt fast build-rel '[planargaps]'`, `'[planarmodel]'`, `'[planar]'`; `'[planarcomponents]'` runs about 6 s against 5 s at the launch head in hand-composer tests: report its time, do not chase it.
- One commit: `Change: Junction mapped fray doubles and fans only its prio lanes`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 9. Report (short)
References read; commit; T diff result; LL² table; gates with count and time; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
