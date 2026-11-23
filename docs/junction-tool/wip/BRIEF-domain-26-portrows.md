# BRIEF domain-26: port rows back in the domain suite (batch 2b, part 2)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. One production change (a row tag of an off-by-default probe), so no wide gate. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Every suite fixture sets `options.skip_rows = {"port "}` (planar_domain_encoding.cpp ~line 147), so the program's port rows (`port <side> edge <line>`: a port end on its box edge; `port <side> gap <line>`: exact spacing along the edge; planar_draw.cpp ~682-740) are never judged. STATUS records one leak hidden that way: two port lines ending on one point.
- Port rows are the hub's INTERFACE (where the structure says the ports are), not tile legality; the domain (`CheckPlanarDomain`) has no ports. So a polyline the domain accepts that breaks only port rows is outside the interface, not too strict: it gets its own class.
- Read first: `CODINGSTYLE.md`; planar_draw.cpp ~600-610 (the D434 `port zero` row) and ~682-740 (port rows); in the test file: the fixture constructor (~140-150), the tally (`counts`, `CLASS_NAMES`, ~626-644), where a polyline is classed (the `accepts` lambda and its caller, ~1088-1200), the report printing (~1500-1550), and the D434 test that clears `skip_rows` (~1815).

## 2. Changes
1. **Tag rename (production):** the D434 row's tag `port zero <line>` becomes `portzero <line>` (planar_draw.cpp ~606; update the `port_zero` doc in planar_draw.h, which names the tag). The D434 test looks for the new tag; remove its `skip_rows.clear()` line and comment.
2. **Fixtures:** delete `options.skip_rows = {"port "};`. Ruling on the agent's stop (2026-11-23): the gap row stays exact (a port's lanes are one lane apart unless the port says where they are, D410). X1, X2 and M1 place their ports wider, so their topology texts declare the base's positions with D410's syntax `port <side> <in|out> <heading> @ <tile> <tile>: <lines>` (tiles along the side, one per line in `lines` order; a half-tile coordinate c is tile (c - 1) / 2): X1 and X2 `port N out N @ 3 5: C1, B1` and `port E out E @ 2 4: A1, D1`; M1 `port E out E @ 2 5: AB1, D1`. Verify each against the base (the base test below); if Shape's headings for any of the three change, or the values above do not make the base hold, stop and report. Nothing else in the fixtures changes.
3. **A new class "off interface":** a polyline whose contracted rows reject it, the domain accepts it, and EVERY failing item is a program row whose tag starts with `port ` (no keep-apart pair, no lead row, no other program row fails). Add it as a seventh class in the tally (`counts` and `CLASS_NAMES` grow by one; printed in each configuration's block and in the WARN line after "other shape"), with the first example per configuration printed as the too-strict ones are (polyline, contracted pieces, the failing port tags). A polyline that also fails any non-port row stays in its old class.

## 3. Tests (red first)
- fast `[junction][planar][planardomain]`: "planar domain: every fixture's base keeps its port rows": for each fixture X1, X2, M1, K1, L1, Y1, P1, the base polyline's program (as the read-out builds it) breaks no row tagged `port `. If a base breaks one, STOP and report the fixture and the rows (a hand-placed base off its interface is a finding for me, not for you to fix).
- fast: "planar domain: two port lines on one point are rejected by the port rows": on a fixture with two port lines on one side (find one among the seven; if none has two lines on one port side, say so and skip this test), move one line's port end onto the other's by changing ONE axis piece parallel to that side, on either line (L1's N side: A1 or A2's east/west piece), choosing the line and piece whose new length is non-negative, and assert the program breaks a `port ` row. Red: run it with `skip_rows = {"port "}` set in that test first, record the failure, then remove it.

## 4. Runs
Release (`build-rel`), X1, X2, M1, K1, L1, Y1, P1, rule set `span+lead+endnode+crosspiece+grid+portzero`, overwrite each `..._portzero` report. Timeout 300 each.

## 5. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planardraw]' 30` (Debug).
- One commit: `Add: Junction: port rows judged in the domain suite, an off-interface class (D428)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; red evidence; members and functions changed; per fixture: classes before and after (all seven), the off-interface count and first example; every too strict or too loose group with key and first example in full; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
