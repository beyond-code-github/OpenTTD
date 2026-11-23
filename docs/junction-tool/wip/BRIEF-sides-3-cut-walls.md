# BRIEF sides-3: keep-apart separates the face's wall segments, cut at crossings

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. A production change in the keep-apart rows; the wide gate once. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- sides-2 (`docs/junction-tool/wip/probes/slots/sides-forced.md`, the hand check of X1 `sep 4 3 6`): the face walk cuts a piece at a crossing, so a face's wall is only part of a piece (e.g. B1 piece 0 from the crossing (11,7) to (11,10)); the keep-apart rows of a pair use the WHOLE pieces' endpoints (B1's other end (11,0) lies in another face). `walk_strict` certifies the in-face part, so its forced side fails on the whole piece. Fixtures with no crossings (M1, K1) show 0 failures.
- The change: a pair's separation rows use the endpoints of the two WALLS as the face has them: a wall cut at a crossing ends at the crossing point (the program's crossing point variables, `cross_point`), not at the piece's far end. Each part of a cut piece is kept apart in its own face.
- Read first: `PlanarShapeFaceWalls` (how walls carry their cut ends), the pair loop's candidate rows (which point variables each candidate row uses), `cross_point`, the read-out `PlanarKeepApartPair` and `PlanarKeepApartHolds` (it evaluates candidates on points: it must use the same wall endpoints, i.e. the crossing point where a wall is cut; read how it gets points), sides-forced.md.

## 2. Changes
1. In the pair loop, each wall's two endpoints are the piece's points, except an end where the face cuts it at a crossing: there, the crossing point (both coordinates). Candidate rows use these endpoints. The read-out records them so `PlanarKeepApartHolds` evaluates the same rows (extend `PlanarKeepApartPair` with what it needs: name it in the report).
2. Nothing else changes (rules, skips, adjacency, along mode).

## 3. Tests and measurement
- A pinned behaviour test (through the judge): X1's polyline from the hand check is accepted by the domain, and every keep-apart pair that holds on the in-face parts holds (the judge's verdict on it is unchanged or becomes accept; report which). If any exactness test turns too loose, that is a finding: report the groups in full and stop before committing.
- All eleven exactness tests 0/0.
- Rerun the sides measurement (`[.][sidesmeasure]`, with off-interface drawings) and the 197 read: the new table (forced share / failures for walk, walk_strict, facing). Expected: walk_strict failures drop on the crossing fixtures; any that remain: the first five in full.

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`, `[planardraw]` 30 (Debug), `tools/junction/jt gate build-rel` once (intermittent parallel-load crash: report, pass alone).
- One commit: `Fix: Junction: keep-apart separates a face's wall segments, cut at crossings (D431)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; planar CLAUDE.md keep-apart bullet updated (D225); the table in sides-forced.md.

## 5. Report (short)
References read; commit; the fields added; exactness; the new table; remaining walk_strict failures; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
