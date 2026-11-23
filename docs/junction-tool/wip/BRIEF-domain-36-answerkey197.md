# BRIEF domain-36: V's PSG 197 judged by the behaviour (the answer key, D389/D393)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. Test code only. No solve anywhere (fixed points only). Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The domain suite specifies Slots' behaviour (D436): accept exactly what the game allows. V's real PSG 197 build is legal by construction. Judged through the seam, every refusal is a rule too strict on a real hub (or an extraction artefact), and every refusal's rule name says which. This is a report, not a gate test.
- Read first: DECISIONS D389, D393, D432, D436; in `src/tests/junction/planar/planar_draw.cpp`: `LoadPinned197` (~1646) and "PSG 197 clearance rows evaluated on V's drawing" (~2279-2400: it rebuilds V's geometry per line from `corpus/out/psg197/pins.txt` start points and `lengths.txt` piece lengths along the raw headings, shape.txt + steps.txt), reuse its parsing, do not copy-paste more than needed; `tools/junction/corpus/out/psg197/` (lengths.txt header explains units); in `planar_domain_encoding.cpp`: the seam (`RowsJudge`, `DrawingProgramJudge`, `RuleOf`), `Contracted`, `ContractedVariant`, `X1Fixture(PlanarShapeVariant)`, `PlanarDomainInputOf` use.

## 2. What to build (test file `planar_domain_encoding.cpp`, one hidden test)
`"planar domain: V's PSG 197 judged by the behaviour (answer key)"`, tag `[.][planardomain197]`, env `PLANAR_DOMAIN_REPORT_197` for the report path (default `docs/junction-tool/wip/probes/domain/answerkey-197.txt` under the worktree).
1. Load 197 as `LoadPinned197` does (topology, structure, shape), WITHOUT `RefinePlanarShapeCells` (cells are retired, D431); if Shape's variant cannot be made without it, stop.
2. V's points per track: per line, start pin + lengths along its raw headings (as the existing test). Tracks that are not lines (copies, rows, parts): if V's geometry for them cannot be read from the extractor's files, report them as "unmapped" and judge the rest only where the seam allows (if the judge needs every track, stop and report which tracks are missing and why).
3. The contracted variant of V's drawing: per track, V's runs written on the variant's pieces as `ContractedVariant` does for a polyline (merge equal consecutive headings, drop zero pieces, keep the one zero piece of a zero track, D433). If `ContractedVariant`'s signature cannot take this input, write the smallest adapter in the test file and say so.
4. Judge: the domain on V's points (`CheckPlanarDomain(PlanarDomainInputOf(...))`): expected legal; every domain break is listed (an extraction artefact or a domain gap; first 20 in full). The seam: `DrawingProgramJudge(DEFAULT_RULES)(f, cv, true)` and `(f, cv, false)`.
5. Report: domain legal or its breaks; the judge's verdict; refusals grouped by suite rule name with counts, and per rule the first 10 refusing raw rows (from the drawing program's diagnostic) with the tracks they name; the off-interface counterfactual's verdict; the counts of tracks, pieces, contracted pieces, unmapped tracks. Also: per suite rule, how many of V's places exercise it (e.g. lead rows written, end-run rows, crossing-piece rows, node-gap rows, corridor rows), so we see which cases 197 contains.

## 3. Runs
Release (`build-rel`) once; timeout 300. Commit the report.

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug; must be unaffected).
- One commit: `Test: Junction: V's PSG 197 judged by the domain suite's behaviour (D393, D436)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Report (short)
References read; commit; domain verdict and breaks; judge verdict; refusals by rule name with counts and first examples; exercised-rule counts; unmapped tracks; stops; `git status --porcelain`, `git log -1`. When you have reported, stop. Fix nothing.
