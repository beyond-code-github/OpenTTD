# BRIEF domain-34: the suite judges a behaviour through one seam

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. Test code only (no production change). Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The domain suite specifies a BEHAVIOUR Slots needs whatever its implementation (engineer, 2026-11-23): given a contracted drawing (variant + points), accept exactly when the game allows it, and name the rule when rejecting. Today the suite reaches into the drawing program (`CheckPlanarDrawable` read-outs, its tags and options) in many places. One seam makes every behaviour test independent of the implementation; the drawing program becomes its first implementation.
- Read first: `CODINGSTYLE.md`; in `src/tests/junction/planar/planar_domain_encoding.cpp`: `readouts`, `accepts`, `NodeTurnsHold`, the turn check in `ContractedVariant`, `VerdictsOf`, `BrokenOnOwnBranches`, `RunEncoding`'s classing and counterfactual, and every TEST_CASE (list them first: `grep -n TEST_CASE`).

## 2. The seam (test file, one anonymous-namespace section near the top, after the fixtures' types)
```cpp
/** A judged drawing: the behaviour the suite specifies for Slots (accept exactly what the game allows; name the rule that refuses). */
struct RowsVerdict {
	bool accept = false;
	std::set<std::string> rules; ///< When refused: the suite's rule names that refuse (see RULE_NAMES), each once.
};

/** What the suite judges: one implementation of Slots' rows on fixed points. `interface_rows` false: without the port and box rows (the off-interface counterfactual). */
using RowsJudge = std::function<RowsVerdict(const X1Fixture &f, const Contracted &cv, bool interface_rows)>;

/** The drawing program (CheckPlanarDrawable read-outs, keep-apart pairs, lead rows) as a RowsJudge, under a rule-set string as RunEncoding parses it. */
RowsJudge DrawingProgramJudge(const std::string &rules);
```
- `RULE_NAMES`: a `const std::map<std::string, std::string>` from the drawing program's tag FAMILY (tag up to its first space; for `bound <g>` / `integer <g>` the whole two words) to the suite's rule name. The suite names: `turn` (F3 within a track), `node turn` (F6), `keep apart`, `lead`, `end run` (`end ...`), `crossing piece` (`cross ...`), `grid` (`grid ...`, `integer half`, `bound half`), `port` (`port ...`, `portzero`), `box` (`box`, `bound box`, `integer box`), `corridor`, `node gap` (`leg`, `leg zero`), `length` (`piece`, `flex`, `bound axis`, `bound diagonal`), and `other` for anything unmapped (an unmapped family is reported in the verdict as `other <family>` so it is visible).
- `DrawingProgramJudge` moves today's `readouts` + `accepts` + `NodeTurnsHold` + the per-track turn check behind the seam, unchanged in behaviour. The turn checks yield `turn` / `node turn`.
- `RunEncoding` takes a `const RowsJudge &judge` (callers pass `DrawingProgramJudge(rules)`; the rule-set string parsing moves into `DrawingProgramJudge`), classes polylines from `RowsVerdict::accept` and the counterfactual from `judge(f, cv, false)`. Reports print the suite rule names of refusals (keep the drawing program's raw tags in the report too, one line, for diagnosis).

## 3. Tests: sort and port
- **Behaviour tests** (assert a verdict on a geometry: every exactness test, every two-sided pinned case, F1-F6, D414, D430, D434, BR1 end run, Y1 copy, two port lines on one point, off-interface L1, the grid-blame test): rewrite each to judge through a `RowsJudge` (`DrawingProgramJudge(DEFAULT_RULES)` unless the test needs a rule off, then the rule-set string without it) and assert `accept` and/or that `rules` contains the suite rule name. No drawing-program tag, option or variable group appears in a behaviour test. Tag them `[junction][planar][planardomain]` (exactness keeps `[planardomainexact]`).
- **Drawing-program tests** (anything that inspects the program itself: `least_extra`, variable groups, `port_zero` flag mechanics, program-vs-keep-apart agreement, checker by track sets, representations): keep them unchanged, tag `[junction][planar][planardraw]` instead of `[planardomain]`, and put them after a comment `/* The drawing program's own tests (an implementation of RowsJudge), not the behaviour. */`.
- If a test does both, split it into one of each. List every test and its bucket in the report.

## 4. F7 as a behaviour test (known failure)
"planar domain: a connected trident is accepted (D433, F7)": K1 with BC1 = 0 and B1, C1 starting at the split node (domain-33's geometry: B1 and C1 start at (4,9), B1 = {4:16}, C1 = {0:7, 9:2}); assert the domain accepts and the judge accepts. Tag it `[junction][planar][planardomain][!shouldfail]` with a comment naming F7 (STATUS) so the gate stays green and the test flips to "unexpected pass" when fixed. Counterpart (BC1 = 1 half tile, rejected with `node gap`) is a normal test.

## 5. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug; if the behaviour tests exceed 5 s in Debug because exactness moved, report the time, do not retag), `tools/junction/jt fast build '[planardraw]' 30`, `tools/junction/jt gate build-rel` once.
- Every exactness count and every pinned verdict identical before and after (this is a refactor): show the eight exactness totals before and after.
- One commit: `Change: Junction: the domain suite judges a behaviour through one seam, RowsJudge (D428)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; the test list with buckets; the RULE_NAMES map as written; exactness totals before/after; F7's shouldfail status; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
