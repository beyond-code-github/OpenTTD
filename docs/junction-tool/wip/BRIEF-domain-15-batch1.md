# BRIEF domain-15: configurations batch 1, nodes (a merge; a cascade and a trident)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Test code only: no wide gate. One Release measurement per configuration, rule set `span+lead+endnode+crosspiece`. Record findings; fix nothing. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The domain suite (harness `src/tests/junction/planar/planar_domain_encoding.cpp`, contracted judging per D432 since domain-14) covers X1 (a tunnel crossing between two splits) and X2 (a 45-degree crosser). The plan (engineer: "expand our coverage with more configurations, and tease out all the issues as we went"): add configurations breadth first, record findings, fix rules later across all of them.

## 2. Two new fixtures (same pattern as X1Fixture / X2Fixture; texts verbatim; if the parser, the structure loader or the shape loader refuses a text, stop and report its message, do not edit the topology)

**M1, a merge whose legs come from splits** (so neither leg is a D412 in-port line and the lead rule's END rows are exercised):
- topology: `topology M1` / `port W in E: AC1` / `port S in N: BD1` / `port N out N: C1` / `port E out E: AB1, D1` / `node AC1 -> C1, A1` / `node BD1 -> B1, D1` / `node A1, B1 -> AB1`
- structure: `straight AC1: A1` / `straight BD1: B1` / `straight AB1: A1`
- shape: `shape AC1: 2` / `shape C1: 1 0` / `shape A1: 2` / `shape BD1: 0` / `shape B1: 0 1` / `shape D1: 1 2` / `shape AB1: 2`
- Base (half tiles; place each run on the first piece of its heading, the last run of a track on its last piece of that heading): AC1 (0,5)-(4,5); C1 (4,5) NE 3 to (7,2), N 2 to (7,0); A1 (4,5)-(16,5); BD1 (13,14)-(13,12); B1 (13,12) N 4 to (13,8), NE 3 to (16,5); AB1 (16,5)-(24,5); D1 (13,12) NE 1 to (14,11), E 10 to (24,11).
- Configurations: **M1a** B1's pieces (all, after flex) varied: its N run length in {2,4,6} split over its N pieces, its NE run in {1,2,3}, each flex insert of B1 in {0,1,2} (axis inserts even only); A1's last two pieces before the node in {0,2,4} each (keeping A1's end at (16,5) by adjusting its first piece). **M1b** the same for A1 against a fixed B1.

**K1, a cascade and a trident** (two splits on one trunk; D400, D424 node gaps):
- topology: `topology K1` / `port W in E: ABC1` / `port N out N: A1` / `port E out E: B1` / `port S out S: C1` / `node ABC1 -> A1, BC1` / `node BC1 -> B1, C1`
- structure: `straight ABC1: BC1` / `straight BC1: B1`
- shape: `shape ABC1: 2` / `shape A1: 1 0` / `shape BC1: 2` / `shape B1: 2` / `shape C1: 3 4`
- Base: ABC1 (0,9)-(4,9); A1 (4,9) NE 3 to (7,6), N 6 to (7,0); BC1 (4,9)-(8,9); B1 (8,9)-(20,9); C1 (8,9) SE 3 to (11,12), S 6 to (11,18).
- Configuration **K1a**: BC1's total length in {0, 2, 4, 6} (0 is the trident: the two nodes coincide); A1's NE run in {1..3}; C1's SE run in {1..3}; each flex insert of A1 and C1 within their first three pieces in {0,1,2}.
- `node_gaps` on in K1's read-out (D424: a node-to-node track travels 0 or at least 2 half tiles); say in the report that it is on.

## 3. For each fixture
- A fast test `"planar domain: <M1|K1> loads and its base is legal"` (`[junction][planar][planardomain]`): loads, base on the grid, domain-legal, accepted by the contracted judgement under `span+lead+endnode+crosspiece`. If the rows reject the base, the test FAILS with the failing rows, and you stop and report (a finding; change no rule).
- The slow test (`[.][planardomainenc]`, env `PLANAR_DOMAIN_REPORT_<M1|K1>`) through `RunEncoding`.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` (report the time; it must stay under 1 s Debug).
- Release once each: M1 and K1 with `span+lead+endnode+crosspiece`, reports `encoding-m1-...txt` and `encoding-k1-...txt` in docs/junction-tool/wip/probes/domain/, timeout 300.
- One commit: `Add: Junction: domain configurations M1 (a merge) and K1 (a cascade and a trident) (D428)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; per configuration the classes (other shape, both accept, both reject, too strict, too loose) with group keys, counts and the first example of each group; wall times; fast-tag time; state. When you have reported, stop.
