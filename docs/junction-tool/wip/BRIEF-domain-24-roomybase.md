# BRIEF domain-24: a roomy base drawing for P1 and Y1

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. A measurement option, off by default (no gate). Record findings; fix no rule. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- P1 (domain-23) judged 2 polylines and Y1 is about as thin: their base is the program's most compact drawing, so the neighbourhood configurations (k = 3) skip most geometries ("an absorbing piece had no length"). A roomier base gives the absorbers length.
- Read first: `CODINGSTYLE.md`; `src/junction/planar/planar_draw.h` lines 155-190 (option style); `planar_draw.cpp` lines 575-595 (the per-piece length variables: `const double lower = free_piece[i] ? 0 : 1;`); in `src/tests/junction/planar/planar_domain_encoding.cpp`, how P1's and Y1's base drawings are made.

## 2. The option
- `PlanarDrawOptions`: `int least_extra = 0;` doc: `///< A measurement aid (domain suite bases): every piece with a least length gets its length variable's lower bound raised from 1 to 1 + least_extra (an axis piece in whole tiles, a diagonal in half tiles), so a tidy drawing has room. Free pieces (flex, steps, D412) keep 0.`
- planar_draw.cpp: `const double lower = free_piece[i] ? 0 : 1 + options.least_extra;`. Nothing else.

## 3. Use it
- P1's and Y1's base drawings: `least_extra = 2`. Nothing else in those fixtures changes. L1 and the hand-placed fixtures are unchanged.
- The base must still be Optimal and domain-legal; either failing is a stop (report status, or the domain's breaks).

## 4. Tests
- fast: "planar draw: least_extra raises every non-free piece's least length": on X1's variant (the F2 test's read-out style, `program_out`, `build_only`), with `least_extra = 2` every length variable that had lower bound 1 now has 3, and every one that had 0 still has 0. Red first: add the field with no behaviour, run, record the failure.
- The existing P1 and Y1 fast tests must stay green with the new bases.

## 5. Runs
Release (`build-rel`), P1 and Y1, rule set `span+lead+endnode+crosspiece+grid+portzero`, overwrite their `..._portzero.txt` reports (P1's is named `encoding-p1-span+lead+endnode+crosspiece+grid+portzero.txt`; keep each fixture's existing name). Timeout 300 each.

## 6. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planardraw]' 30` (Debug).
- One commit: `Add: Junction: a roomy base drawing for the domain suite's P1 and Y1 (D428)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report (short)
References read; commit; red evidence; members added; per fixture: base size before and after, polylines judged before and after (per configuration), skipped counts, classes; every too strict or too loose group with its key and first example in full; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
