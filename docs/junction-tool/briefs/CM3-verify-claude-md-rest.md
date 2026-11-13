# CM3: verify the CLAUDE.md draft lines CM2 did not check

Edit only /home/pete/OpenTTD/docs/junction-tool/CLAUDE-draft.md. Read-only otherwise. Never touch any worktree under /home/pete/OpenTTD/.claude, a game client or the rig servers. Keep your context small: grep with scope, read with offset/limit, read each source once.

## State
- CM2 checked about 70 of the draft's roughly 230 fact lines. It checked the Files list, TU5, D223, the bridge limit, D148, the hover cache, the hover time, QL2, CN1, D51, the hotkeys and the fast tags.
- About 160 lines are unchecked. They are mostly in Heights, in Signals and prio, and the placement-rule lines, plus the remaining lines in Notation/topology, Graph/order/structure and Game rules.
- Already decided, so do not reopen these:
  - router is live;
  - TU5, CX1 and TS1 are not merged;
  - P6b, QL2, CN1, LP1 (D221), TP1 (D222), OF1, OF2, MJ1 (D224) and RW1-RW6b are merged;
  - a tunnel is allowed only on a single separation. Write the D80/D174 doubled-tunnel gap as one Open line: "doubled main-line tunnels: not implemented".
  - The hover cache has no terrain hash in its key and is cleared after a build. The code wins over D93's wording.
- Sources: docs/junction-tool/DECISIONS.md, DESIGN-G1.md, the briefs in docs/junction-tool/briefs/ (HC1, HM1, IG2-IG6, P3b, P4b, P6b, TU1-TU4, RW1-RW6b, OF1, OF2, MJ1 for heights, rows and order), `git -C /home/pete/OpenTTD log --oneline -150`, and the code: src/junction_*.cpp and src/tests/junction_*.cpp, in particular junction_levels.cpp and junction_topology.cpp.

## Do
1. For every fact line CM2 did not check: open the cited source AND confirm against the code that the line states the current behaviour. Fix the line in place. Delete a line that is wrong and cannot be fixed. Move an undecided line to Open. Where the code and a decision disagree, the code wins if a later merged commit explains the change; otherwise list the disagreement.
2. Keep the format: present tense, no dates, quotes or "why", ids in brackets, 250-350 lines.

## Report
- Counts: lines checked, fixed, deleted, moved to Open.
- The material changes, one line each.
- The contradictions the engineer must rule on: only the ones where neither the code nor a merged commit settles it.

When you have reported, stop.
