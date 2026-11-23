# BRIEF domain-30: off interface by counterfactual

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 50 minutes wall. Test code only (no gate). Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The off-interface class (domain-26, widened in domain-29) is decided by failing TAGS (`port `, `box`, `bound box`, `integer box`). On L1, 25 polylines still come out too strict with port rows plus `integer half` / `bound half`: values derived from the broken interface rows, so the tags cannot tell a consequence from a cause. All 25 were accepted while the suite skipped port rows.
- The rule: a polyline is OFF INTERFACE when the domain accepts it, the full rows reject it, and the same read-out built with the interface rows removed (`skip_rows = {"port ", "box"}`; check that `skip_rows` matches `box` exactly and `box ...` by prefix as its doc says, else stop) accepts it. Otherwise it keeps its class (too strict if the reduced rows also reject).
- Read first: in `src/tests/junction/planar/planar_domain_encoding.cpp`: `readouts`, `accepts`, `OnlyProgramRowsReject` and where the class is decided (`judge`), the D434 helper `PortZeroFrom`; `PlanarDrawOptions::skip_rows` doc in planar_draw.h.

## 2. Changes (test file only)
1. Replace the tag test (`OnlyProgramRowsReject`'s interface rule) by the counterfactual: when the domain accepts and `accepts` is false, build the reduced read-out for that polyline (same options as `readouts`, plus `skip_rows = {"port ", "box"}`) and run `accepts` on it; true means off interface. Remove the interface-tag rule and its helper if nothing else uses them; update the class's doc comment to state the counterfactual.
2. The report's off-interface example lists the failing tags of the FULL rows, as now.

## 3. Tests
fast `[junction][planar][planardomain]`: "planar domain: an L1 polyline off its N edge is off interface, not too strict": the first L1 example from the report (varied `A2.7=0 A2.8=0 A2.9=2 A2.10=1 A2.11=0`, polyline `A1:1*5,0*2,;A2:1*1,0*2,7*1,;AB1:;AB2:2*6,;B1:2*12,;B2:2*6,;`): the full rows reject, the reduced rows accept. If the reduced rows reject it, STOP and report their failing tags (that would be a real too-strict finding).

## 4. Runs
Release (`build-rel`), X1, X2, M1, K1, L1, Y1, P1, rule set `span+lead+endnode+crosspiece+grid+portzero`, overwrite each `..._portzero` report (P1 keeps its `+` name). Timeout 300 each.

## 5. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug).
- One commit: `Change: Junction: off interface by counterfactual in the domain suite (D428)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; red evidence (the new test before the change: it is classed too strict); per fixture: classes before (domain-29) and after; every remaining too strict / too loose group in full; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
