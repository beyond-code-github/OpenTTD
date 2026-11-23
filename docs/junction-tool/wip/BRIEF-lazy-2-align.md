# BRIEF lazy-2: Slots solves with exactly the judge's rules; T1's drawing made legal or explained

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall; each solve capped at 15 s; no rerun loops. Test code only unless stated.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- lazy-1 (`docs/junction-tool/wip/probes/slots/lazy.md`): T1's converged stage-B drawing (and the all-rows control) is refused by the judge ([grid] [keep apart] [length] [node], plus [crossing piece] [end run] with offers 1) and the domain ([grid] [node] [turn]). The solve's options differ from the judge's (fixture options, `port_zero_from`, `node_gap_from`, `along_from` references), and the turn rules (F3 within a track, F6/F8 through nodes) are harness checks only. A Slots result means nothing until the solve uses the certified rule set.
- Read first: `DrawingProgramJudge` and `X1Fixture`'s options in `planar_domain_encoding.cpp` (exactly which options and references the judge's read-outs use); the `[.][lazymeasure]` test (its stage A/B options, its contraction); `ContractedVariant` (its turn handling, F3) and `NodeTurnsHold` (F6/F8).

## 2. Steps
1. One options builder in the test file: `PlanarDrawOptions SlotsOptions(const X1Fixture &f, const std::string &rules)` returning exactly the options `DrawingProgramJudge` uses for its program read-out (minus the read-out flags `program_out`, `build_only`, `keep_apart_out`); `DrawingProgramJudge` calls it (no behaviour change: the eight-plus exactness tests must stay 0/0). The references (`port_zero_from`, `node_gap_from`, `along_from`) are per drawing: for a solve, state what the lazy test passes (the stage-A drawing's own values, as the judge would read them from it) and use that.
2. The lazy test's stage B uses `SlotsOptions` (plus `only_pairs`), with the references read from stage A's drawing.
3. Stage B's contracted variant must satisfy the turn rules by construction: before solving, check it with the same functions the judge uses (`ContractedVariant`'s one-eighth check, `NodeTurnsHold`); if it fails, report which track/node and why (stage A chose S-bends that make an illegal writing) — do not repair.
4. Re-run T1 (offers 1 and 3): rounds, pairs with rows, ms; the judge's and the domain's verdicts on the result. If still refused: for each refusing rule, the first refusing row/break in full and its reading (option still mismatched, a reference read wrongly, or a real gap).

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`; `[planardomainexact]` via the gate: `tools/junction/jt gate build-rel` once.
- One commit: `Test: Junction: Slots solves with the judge's rule set (lazy-2)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; results appended to lazy.md.

## 4. Report (short)
References read; commit; the options the judge uses (list); T1 results and verdicts; every remaining refusal explained; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
