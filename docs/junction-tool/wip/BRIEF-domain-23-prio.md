# BRIEF domain-23: a prio row in the domain suite (batch 2b, part 1)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Test code only (no gate). One Release run per rule set below. Record findings; fix no rule. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The domain suite (`src/tests/junction/planar/planar_domain_encoding.cpp`) is exact on X1, X2, M1, K1, L1 and Y1 under `span+lead+endnode+crosspiece+grid+portzero` (D434). No fixture has a prio row yet; prio rows are the beside tracks of T1/T2 (D421: one-piece offset curve one lane from its parent, exact one-lane opening and closing steps; D384: exactly its coverage long; D353: may share the opposite diagonal half of its main line's tiles).
- Read first: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D353, D384, D421, D432, D434; `wip/BRIEF-domain-19-batch2a.md` §2 (how Y1/L1 were built: `ShapePlanarStructureVariant`, base drawing by the program) and `wip/BRIEF-domain-21-neighbourhood.md` §2 (`NeighbourhoodConfigs`); in the test file, the Y1 and L1 fixtures and their `RunEncoding` tests (copy their pattern exactly).

## 2. Fixture P1 (text verbatim; a refusal by the parser, the structure loader or Shape is a stop: report its message)
- topology: `topology P1` / `port W in E: A1` / `port S in N: C1` / `port E out E: AC1` / `node A1, C1 -> AC1` / `prio A1`
- structure: empty (no structures). If the loader needs a statement, stop and report.
- shape: `ShapePlanarStructureVariant`, as Y1/L1.
- Base geometry: as BRIEF-domain-19 §2 (the program's drawing with the rule set's options plus `grid`, `tidy`, `corridor = true`, integer; Optimal; the domain accepts its points). Either failing is a stop.
- Report the expanded tracks (name, from, to, beside or not) and Shape's headings, one line each.

## 3. Configurations
Only the generic neighbourhood configurations (`NeighbourhoodConfigs`, k = 3). No hand-named ones.

## 4. Tests
- fast: `"planar domain: P1 loads, has a prio row and its base is legal"`: the expansion holds at least one beside track; the base drawing is Optimal and domain-legal.
- slow: `[.][planardomainenc]` via `RunEncoding`, env `PLANAR_DOMAIN_REPORT_P1`.

## 5. Runs and findings
- Release (`build-rel`), rule sets `span+lead+endnode+crosspiece+grid+portzero` and the same with corridor rows in the read-out as Y1 has them (if Y1 already reads corridor rows by default, one run). Report `encoding-p1-<rules>.txt` under the worktree's `docs/junction-tool/wip/probes/domain/`, committed. Timeout 300.
- Expect too-strict polylines whose failing rows are the prio's own exact rules (corridor offset, opening/closing step, coverage length): the domain has no prio semantics, so a row that is longer or off its lane can be tile-legal. Report too strict grouped by the failing row's tag family (`corridor`, coverage/`prio`, keep-apart, other), with counts and the first example of each. Any too-strict polyline whose failing rows are NOT the prio's own rules, and every too-loose polyline, is a real finding: give its group key and first example in full.
- Say whether the program writes D384's exact-length row for the prio (its tag) and whether it holds on the base.

## 6. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug).
- One commit: `Add: Junction: a prio row fixture in the domain suite (D428)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report (short)
References read; commit; expanded tracks and headings; base status and size; per rule set: classes with counts, too strict by tag family, every real finding in full; wall times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
