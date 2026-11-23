# BRIEF recon-t2-sbends: classify the S-bends of legal T1 and T2 drawings by role

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. One hidden test, run once. Change no production code.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Ruling D449 (the last row of `docs/junction-tool/DECISIONS.md`): a bend pre-program will decide S-bends before Slots. Its parts are to be designed from evidence: node shapes, lane slots in multi-lane channels, port-gap steps, and whatever remains.
- `wip/probes/shape/recon-197-steps-2.md` classified V's 197 steps by role. This brief does the same for two legal drawings of our own hubs (offers 1, D443 off, flank rows on, grid on):
  - T1: stage A solved by HiGHS in the test itself (it is legal end to end: `wip/probes/slots/flank.md`);
  - T2: the piece lengths of a CP-SAT solution with the length objective, in a file (one value per line, per track then per piece, the order `PLANAR_STAGEA_LOCK_FILE` reads), path given by env `PLANAR_T2_LENGTHS`.

## 2. Read first (once, with offset/limit)
- `src/tests/junction/planar/planar_draw.cpp`: `[recon197steps2]` (about line 4467 onwards), for the role, node kind, other end and sibling side logic.
- `src/tests/junction/planar/planar_domain_encoding.cpp`: `[slotsstep2]` (about 6664 onwards), for the T1/T2 variant build (offers 1, D443 off), stage A options with `PLANAR_FLANK`, and the `PLANAR_STAGEA_LOCK_FILE` reader.

## 3. The change: one hidden test
`TEST_CASE("planar slots: S-bends of legal T1 and T2 drawings by role (recon)", "[.][recont2sbends]")` in `planar_domain_encoding.cpp` after `[slotsstep2]`. Per hub:
1. Build the variant as `[slotsstep2]` does. Get the lengths:
   - T1: solve stage A (`SlotsOptions`, `node_gaps` off, no keep-apart rows, `flank_turn` on, grid on, tidy on); REQUIRE Optimal, then take `lengths`.
   - T2: read the file (REQUIRE the count matches).
2. A USED S-bend = a flex middle m with length > 0. For each used S-bend write one row:
   - track (and its kind: Line, Copy or Row) and stretch index k (the stretch holding piece m);
   - the stretch's two ends (node with index and split/merge, crossing with its other track, or port);
   - the track's role at each node end, as recon 2 defines it (straight, diverging, trunk, fork leg; none if it is not a node end);
   - direction d = sign(pieces[m] - pieces[m-1]) (+1 right), and, at a node end, `toward` or `away` relative to the sibling as recon 1 computes it;
   - its length and its distance from the node end in pieces (m minus the stretch's first piece, or last piece minus m);
   - whether the stretch's Shape headings include a diagonal.
3. Then the same features for every node-adjacent stretch WITHOUT a used S-bend (as recon 2 lists stretches without steps), so the tables count both.
4. Cross-tables: used / not used by (split or merge) x role x other end; by node kind; and, separately, the used S-bends that are NOT at a node end (port, crossing-crossing), listed.

Output to env `PLANAR_RECON_OUT`, run with `PLANAR_T2_LENGTHS=<path given by the orchestrator> PLANAR_RECON_OUT=docs/junction-tool/wip/probes/slots/recon-t2-sbends.md`.

## 4. Do not
- Do not change production code or other tests. Do not propose rules; report tables.

## 5. Gates
- Build `build-rel` with output to a file; run once.
- Add a "Reading" of at most 8 lines, facts only, comparing with recon 2's V results: diverging legs (used / total, direction), straight legs, trunks, port and crossing-crossing S-bends.
- Commit: `Doc: Junction: S-bends of legal T1 and T2 drawings by role (recon)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; the cross-tables; the Reading; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
