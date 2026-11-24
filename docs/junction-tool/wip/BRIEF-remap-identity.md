# BRIEF remap-identity: a contracted crossing sits on the image of its own piece (fixes X2's exactness)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes. No solves.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build Release `build-rel`. `JT_JOBS=4`, cmake `-j4`. Never touch ng1. HEAD holds an orchestrator merge resolution in planar_domain_encoding.cpp (the `PLANAR_FINAL_SAVE` block in the probe): build first; a compile error there is a stop.
- Regression (orchestrator bisect): "planar domain: X2 is exact under the default rule set" (`[planardomainexact]`, :3082) passes at e4d778644b and fails at bde7a48f95 (the remap fix): `X2: too strict 23, too loose 0`. bde7a48f95 selects a crossing's contracted pieces by the original crossing point (fallback: first touching same-heading pair). A point at a piece end lies on two contracted pieces (D414: a crosser may cross at its piece's end), so "the first pair containing the point" can pick the neighbour.
- Rule (orchestrator): **a contracted crossing sits on the image of its original piece.** The contraction maps every original piece to a contracted piece (merged runs map to the merged piece) or erases it (length 0). The crossing's contracted piece on each track is the image of its original event piece. Only when that original piece was erased does the selector fall back: first the original point (bde7a48f95's rule), then the first touching same-heading pair (the older rule).

## 2. Read first
- `ContractedVariant` (planar_domain_encoding.cpp ~:1030 onward) whole: how pieces are erased and merged and how events are remapped today; the pin tests `[contraction]`; the X2 exactness test (:3082) and what "too strict" counts.

## 3. Changes and tests
- Red: the X2 exactness test fails today (record `too strict 23`).
- Implement the rule in `ContractedVariant`'s event remap (an index map original piece -> contracted piece built while contracting; no new type: a local vector).
- Green: X2 exact (too strict 0, too loose 0); the T2 contraction pin "(T2)" still builds stage B's program (its structure check holds) with the same broken tags as before (grid/node-gap causes, `[!shouldfail]`); all `[contraction]` pins unchanged otherwise.

## 4. Do not
- No change to stage A/B, the domain, defaults. Edit/Write only.

## 5. Gates (D453 as amended: the contraction runs `[planardomainexact]`)
- `JT_JOBS=4 tools/junction/jt fast build-rel '[planardomainexact]' 30`, `... '[planardomain]'`, `... '[contraction]' 5`. Every result equal to e4d778644b's except the ones this rule names (report any other change and stop).
- Commit: `Fix: Junction: a contracted crossing sits on the image of its own piece`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 6. Report
References read; commit; red text; X2 after; `[planardomainexact]`, `[planardomain]`, `[contraction]` counts; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
