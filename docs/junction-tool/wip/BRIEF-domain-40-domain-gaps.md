# BRIEF domain-40: two domain gaps found on V's 197, and V's terrain in the answer key

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. Changes to the domain (the game truth, `planar_domain.cpp`) and the 197 test; the wide gate once. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Read first: `docs/junction-tool/wip/probes/domain/triage-197.md` (domain-39) in full; DECISIONS D155, D365, D390; `src/junction/planar/planar_domain.h` and `.cpp` (`AddStructure`, structure middles, the missing-crossing rule, `ends`); `src/tests/junction/planar/planar_domain.cpp` "hand cases from the game" (how a case is pinned in the game, `[planardomaingame]`); the 197 hidden test and `triage197.py`.
- domain-39 found 57 of V's 64 breaks are the domain refusing V's real build. Two are domain gaps (the game allows what the domain refuses); one is our input (V's terrain).

## 2. Changes
1. **Shared node tiles are not crossings (gap A2):** `AddStructure` counts any tile the separator shares with a crosser as a crossing, including the tile of a split/merge node the two tracks share (L4/L5 at game (81..82,173); L69/L70 at (104,180)); the crosser then "meets" the separator at piece 0 or 1 and the structure is dropped. A tile where the two tracks share a node (both list that node) is not a crossing for this purpose. Hand-case test (domain only, no game build needed: it is about which tile is the crossing): two lines from one split, one tunnelling under the other mid-run: the structure is kept and the domain accepts.
2. **Tunnel under tunnel (gap C, D390 (2)):** a listed crossing between two TUNNEL structures whose middles cross on a tile is realised by depth (the game allows it: V's L30/L31 under L32/L47/L48 at game (91..93,163..164)). The missing-crossing rule accepts it; bridge over bridge stays refused. Pin it in the game if the hand-case harness can build two tunnels at different depths (terrain raised in steps); if it cannot, a domain-only test with a comment citing V's save tiles, and say so.
3. **V's terrain in the 197 answer key (cause A, D):** V's tunnel heads sit on sloped ground, so the end run is the portal alone (1), not portal plus ramp (2, D155/D365's flat-ground case). With `triage197.py` (V's heights from the save export) list every 197 tunnel's two heads: sloped (1) or flat (2). If every head agrees within a structure, the 197 test sets each structure's `ends` from that list (a data file next to the triage, read by the test); if a structure's two ends differ, stop and report them (the domain has one `ends` per structure). The planner's own end-run rule is NOT changed (that is the engineer's call; card below).
4. Rerun the 197 test in Release: report the domain's remaining breaks (expected: the 7 artefacts of the triage, or fewer) and the judge's refusals by suite rule name with first examples, as domain-36 did.

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`, `tools/junction/jt fast build '[planardomaingame]' 30` (or the tag the game cases use), `tools/junction/jt gate build-rel` once; all eight exactness tests still 0/0.
- One commit: `Fix: Junction: the domain keeps a structure at a shared node and lets tunnels cross by depth; V's terrain in the 197 answer key (D390, D393)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Update the domain bullet in `src/junction/planar/CLAUDE.md` (D225).

## 4. Report (short)
References read; commit; the hand cases and whether pinned in game; the head-slope list summary (how many sloped, flat); 197 domain breaks after; judge refusals after; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
