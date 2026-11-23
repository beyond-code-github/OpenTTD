# BRIEF domain-29: name the box and grid-half variables; the box is interface

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 50 minutes wall. Variable group NAMES only in production (no row, bound or integrality changes), so no wide gate. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- After domain-28 the remaining too-strict polylines (X1 S2, X2 C1/C2, L1b; see the `..._portzero` reports) all break port rows plus `box` rows and/or `integer other` / `bound other`. The box (the hub's footprint) is interface, like the ports. `other` is the default variable group and covers the box width/height (planar_draw.cpp ~518-519) AND the grid halves (`half[v] = add_variable(lower, upper, 0, true)`, ~1950), so the tags cannot say which.
- Read first: `CODINGSTYLE.md`; planar_draw.cpp ~451 (`add_variable`, group default `"other"`), ~518-519, ~1897-1975; grep planar_draw.cpp for every use of variable group names (`continuous`, `elastic_families`, export) and confirm none matches `other`, `box` or `half` by name; if one does, stop and report. In the test file: `OnlyProgramRowsReject` and the off-interface class (domain-26).

## 2. Changes
1. planar_draw.cpp: `width` and `height` get group `"box"`; each grid half gets group `"half"`. Nothing else.
2. Test file, the off-interface rule: a failing item is INTERFACE when its tag starts with `port ` or is `box` or starts with `box `, or is `bound box` / `integer box`. A polyline the domain accepts whose every failing item is interface is off interface (was: only `port `). Update the class's doc comment.

## 3. Tests
fast `[junction][planar][planardomain]`: "planar domain: the box width and height are group box, grid halves group half": a read-out program of X1 with grid on: the two box variables (find them by the `box` rows' terms or by the program's group list) have group `box`; at least one variable has group `half` and none of the halves is `other`. Red first (record).

## 4. Runs
Release (`build-rel`), X1, X2, M1, K1, L1, Y1, P1, rule set `span+lead+endnode+crosspiece+grid+portzero`, overwrite each `..._portzero` report (P1's tracked name has `+`; keep it). Timeout 300 each. Report every remaining too-strict group with its failing tags (now `half` vs `box` is visible) and first example in full.

## 5. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planardraw]' 30` (Debug).
- One commit: `Change: Junction: box and grid-half variable groups; the box is interface in the domain suite (D428)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; red evidence; per fixture: classes before (domain-28) and after; remaining too strict / too loose groups in full; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
