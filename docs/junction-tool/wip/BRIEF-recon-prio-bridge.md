# Recon: the coop prio-over-a-bridge structure, in our terms (read-only)

Read-only: no code edits, no builds, no commits except your findings file. Never use git stash, never start a game client, never touch ports 4077/4079. Time box 45 minutes; at the box, write what you have. Read files with offset/limit; command output to files under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/recon-pb/`. Cite every fact (file:line or image name); mark inferences "I". If the corpus does not show something, say so; do not invent a structure.

## Why
Ruling D347 (`/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md`, read D107, D108, D342, D345, D347 rows): a prio's coverage may contain a main-line bridge, built in the coop prio-over-a-bridge structure ("a slightly different structure from a normal one"). Today a 3-lane prio join chains its lanes' coverages because joiner j-1 crosses lane j on a doubled bridge before lane j's coverage (`wip/PROBE-prio-stagger.md`). We need the exact structure before designing the checker and composer change.

## Read first
- `wip/RESEARCH-prio-freedom.md` (34 lines) and `wip/PROBE-prio-stagger.md`.
- Corpus on OneDrive, `S=/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad`: `S/coop-site/site/Priorities.html` (the bridge sections), `S/coop/files/Presig_bridge_prio.png`, `Standard_prio_bridge.PNG`, `Used_tunnel_prio.png` (view the images), `S/junctionary-mirror/pages/Priority Merge.mediawiki`; grep `S/` for any other prio-bridge image or page. `RULES-coop.md` R25, R31, R40, R51.
- Our checker: `/home/pete/OpenTTD/.claude/ng1/src/junction/junction_rules.cpp` CheckPrio (G9, ~1300-1500, incl. G9c), CheckPrioReach (G9b, ~1690+); `docs/junction-tool/tools/sigsim.py` header (bridge = one block member).

## Answer (findings under 45 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/RECON-prio-bridge.md`)
1. Each coop variant (standard with third bridge; presignal prio over a bridge; any other shown): an ASCII sketch in tiles (ML lane, row, joiner, bridge heads, joins, every signal with type and facing), and what each element is for.
2. Constraints each states or implies: span vs TL, joins before/after the bridge, which signals are pre/exit/combo, whether the row must bridge too.
3. Our case: a joiner crossing a main-line lane inside that lane's coverage where the ML lane is a DOUBLED bridge (two copies, D80 parallelogram, a split before and a rejoin after). Does any coop variant show a doubled ML bridge inside coverage? If not, say which variant extends to it and what the switches require (I).
4. What our checker does with each variant today: which G9 clause rejects it (G9c switch in coverage, clause 1 join shape, adjacent signals, ...) and whether CheckPrioReach's semantics (bridge as one block member) would accept it. Name the exact clause and line.
5. Which of our existing signal types/patterns (D74 pre-signal only) suffice.

Report to me in under 10 lines, then stop.
