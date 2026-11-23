# BRIEF domain-25: contracted headings turn one eighth at a time through nodes (F6)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Test code only (no production change, no gate). Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- F3 (in the harness): Slots' contracted writing keeps consecutive pieces of one track at most one eighth apart; `ContractedVariant` counts writings with no such representation (`g_turn_rejected`), and `accepts` rejects any variant with two consecutive pieces more than one eighth apart.
- F6 (domain-24, Y1 report `encoding-y1-span_lead_endnode_crosspiece_grid_portzero.txt`, configuration `node2 A1#8`, the one too-loose polyline: varied `A1#8.2=2 A1#8.3=2 A1#8.4=0`): the copy A1#9 closes into its merge node at (20, 9) with its closing step heading 3 (SE), and the trunk A1#8's first contracted piece is heading 1 (NE), because contraction removed Shape's zero first piece of heading 2. Two eighths at a node; the domain refuses (turn), our rows accept.
- The general rule (F3 extended): along every route path, through nodes too, consecutive contracted headings differ by at most one eighth. At a node: every leg's heading at the node and the trunk's heading at the node differ by at most one eighth (circular, mod 8).
- Read first: `src/tests/junction/planar/planar_domain_encoding.cpp`: `ContractedVariant` (~line 748-820), the `accepts` lambda in `RunEncoding` (~line 1088), the report line using `g_turn_rejected` (~line 1422); `src/junction/planar/planar_map.h` for `expanded.nodes`, `expanded.lines[t].from/to` (`.port`, `.index`).

## 2. The check (harness only)
A function in the test file, next to `ContractedVariant`:
`bool NodeTurnsHold(const X1Fixture &f, const Contracted &cv)`, doc: `/** F6: at every node, each leg's heading at the node and the trunk's differ by at most one eighth (the contracted turn rule, F3, through nodes). */`
- For node n: the tracks ending at n (`to` not a port, `to.index == n`) and starting at n (`from` not a port, `from.index == n`). A split has one track ending and several starting; a merge the reverse. The trunk is the single track on its side; the legs are the others. If a node has more than one track on both sides, stop and report.
- A track's heading at n: arriving (track ends at n) = its last contracted piece's heading; departing = its first piece's heading; EXCEPT a track whose end point at n is not the node point (a beside track): arriving = the heading k with `node point - last point == 2 * STEP[k]`, departing = k with `first point - node point == 2 * STEP[k]` (`PlanarModel::STEP`). The node point is the end point at n of any track at n whose end point there is the same for all such tracks (the non-beside ones). If no k matches, or the non-beside end points disagree, stop and report.
- A zero-length track (all its points equal: a D412/D434 zero port line or a D433 trident track) has no heading and is transparent: as a leg it is skipped; as the trunk, the node uses the heading at the far end of the trunk of the node at the zero track's other end (for a split: the trunk arriving at that node; for a merge: the trunk departing it), recursively while that is zero too; if the zero trunk's other end is a port, the node is skipped. (Ruling on the agent's stop, 2026-11-23.)
- Holds when for every leg, `min(d, 8 - d) <= 1` with `d = ((trunk - leg) % 8 + 8) % 8`.
- `accepts` adds `&& NodeTurnsHold(f, cv)` (first, with the per-track check). Add a global `g_node_turn_rejected` counted where `accepts` finds a variant failing only this check (count once per polyline as the existing tallies do; if that cannot be told apart cleanly, count every call that fails it and say so), reset with `g_turn_rejected`, printed on the same report line: `; through nodes (F6), rows reject: N`.

## 3. Tests (red first)
fast `[junction][planar][planardomain]`: "planar domain: a trunk turning two eighths from a beside leg at its node is refused (F6)": Y1 (with its domain-24 base) plus the F6 variation; `ContractedVariant` gives a variant; assert `NodeTurnsHold` is false; and on Y1's base it is true. Red: write the function returning true first, run, record the failure, then write it.

## 4. Runs
Release (`build-rel`), every fixture X1, X2, M1, K1, L1, Y1, P1, rule set `span+lead+endnode+crosspiece+grid+portzero`, overwrite each `..._portzero` report. Timeout 300 each. Expected: Y1's too-loose polyline moves to both reject. Any NEW too-strict polyline is a finding (the domain accepts a two-eighth turn at a node): report it in full; do not change the check.

## 5. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug).
- One commit: `Add: Junction: contracted headings turn one eighth at a time through nodes in the domain suite (F6)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; red evidence; functions and globals added; per fixture: classes before and after, the F6 count; every non-zero too strict or too loose group with key and first example in full; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
