# BRIEF domain-53: deck and order rows apply only between crossings at the same level (finishes D438 on 197)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. One production change; the wide gate once. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- domain-52 (`docs/junction-tool/wip/probes/domain/along-deck-order.md`, read it): on V's 197 at tile (95,167) L57's tunnel (N-S), L46's tunnel (E-W, under L57 by depth, D390) and L40 (surface, along L57, D438) stack. `deck 1`/`order L57 0` compare crossing 77 (L40 x L57, surface) with 87 (L46 x L57, tunnel by depth); `deck 20` compares crossers L40 (surface) and L57 (a tunnel, by depth) of L46. V's geometry is legal.
- The rule (orchestrator default, D227, recorded as D440): `deck <s>` and `order <t> <piece>` relate two crossings only when both are at the same level; a crossing between two TUNNEL structures (a crossing that is in two structures, each track the separator of one: D390 (2)) is at a different level from every other crossing of those tracks, and is neither spaced nor ordered against them.
- Read first: planar_draw.cpp ~1170-1182 (`order`), ~1255-1290 (`deck`), how a crossing in two structures is represented (`v.structure.structures[s].crossings`, the separators); D390, D415, D438.

## 2. Change
- `order`: skip the pair (c0, c1) when exactly one of them is a tunnel-by-tunnel crossing (both tracks separators of tunnel structures that list it). (Two tunnel-by-tunnel crossings in a row keep their order, as two surface ones do.)
- `deck`: when building `u` for structure s, a crossing whose crosser is itself the separator of a tunnel structure listing that crossing is a different level from the surface crossers: the deck rows chain only crossings of the same level (chain the surface crossings among themselves and the by-depth ones among themselves).
- Comment on each: `/* D440: crossings at different levels (a tunnel by a tunnel, D390) are neither spaced nor ordered against each other. */`

## 3. Tests
- Behaviour (through the judge): on TT1 (tunnel under tunnel) plus a surface crosser at the same crossing tile is not expressible in TT1's topology; so pin with the 197 result: the hidden 197 run shows no `deck`/`order` refusal. Add a normal fast test only if a suite fixture can express a surface crossing and a by-depth crossing of one separator within one deck gap (look at TS2/TT1; if none, say so).
- All eleven exactness tests 0/0.

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`, `[planardraw]` 30, `tools/junction/jt gate build-rel` once (the intermittent 'joint: a recording at a point detects there' crash under parallel load: report, pass alone). The 197 hidden run; regenerate `docs/junction-tool/wip/probes/domain/answerkey-197.txt`.
- One commit: `Fix: Junction: deck and order rows relate crossings at one level only (D440)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; planar CLAUDE.md bullets for deck/order updated (D225).

## 5. Report (short)
References read; commit; 197 refusals after (by rule); exactness; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
