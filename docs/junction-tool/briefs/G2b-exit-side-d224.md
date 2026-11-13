# G2b: exit-side D224, so a main lane's branch-only splits sit in the band below its row

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2. Run `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-g2b <head>` with the head given in your launch prompt. Builds: `build` and `build-rel`. Ignore docs/junction-tool/wip/. Never use git stash.
- The ruling is DESIGN-G2-general-graph.md §G2.2 and D228: only the first node of a cascade sits on the main row; the rest sit in the band.
- Review finding F2 (REVIEW-opus-since-move.md): with pairing any, LL_RR × LL_RR gives a main lane two split nodes at its slot, the group split and then the lane split within the branch group. Both sit at level 2*row+1, the row has two legs, and structure refuses it with "a lane other than in, one leg, out".
- The join side already does the mirror image. Read junction_topology.cpp, around lines 392-406, "Levels (D204, D205)": a merge node at a main slot whose Out edge leads to another node and whose paths all come from branch slots moves to level 2*row+2.

## Read first
src/junction/CLAUDE.md, topology/CLAUDE.md, src/tests/junction/CLAUDE.md, DESIGN-G2 §G2.2, junction_topology.cpp lines 233-420, and the tests that build LL_RR × LL_RR with a lane pairing in src/tests/junction/topology/junction_topology.cpp. Find them by `JunctionLanePairing` or by "LL_RR".

## Tests (red first; tags `[junction][topology]`)
1. "exit side: a main lane's branch-only splits sit in the band (D224)": LL_RR main × LL_RR branch, pairing any (the default). Assert:
   - every split node at a main slot whose In edge comes from another node, and whose paths all go to branch slots, has level 2*row+2;
   - `StructureJunctionTrackGraph` returns no error.
   - Red: before the change, the first assertion fails, and structure returns the "in, one leg, out" refusal. Record both texts.
2. Report-only probe, not a committed assertion: run LL_RR × LL_RR (any) through programs and placement on flat ground once, and report the first stage that refuses it and its error. CX1 and G2c are expected to be the next blockers.

## Change
In the "Levels (D204, D205)" loop, for a node at a main slot, add the mirror of the D224 block for splits (`node.split`):
- If the node has an In edge that comes FROM another node (`edge.from.node`), and every path of that edge goes TO a branch slot (`!group_of(paths[p].to).main`), then `node.level = 2 * row_of[node.slot] + 2`.
- The comment is: "D224 (exit side, DESIGN-G2 §G2.2): a main lane's splits among branch lanes sit in the band below the lane; only the first split of the cascade is on the row."
- Do not change the merge block.

## Downstream
- If order or structure fails because a split node now sits at an even level (a band split node at a main slot), stop and report the error and where it is raised. Do not adapt order or structure.
- Existing tests must not change. Today's T and LL_RR × L_R have no such nodes.

## CLAUDE.md (same commit)
In topology/CLAUDE.md, change the D224 line to: "Two legs of one branch bound for one main lane merge with each other first and join as one joiner, with one prio element per lane; on the exit side a main lane's splits among branch lanes sit in the band below its row, so each main row carries one in, one leg and one out (D224, D228, DESIGN-G2 §G2.2)."

## Stop conditions
The ones above, plus any identity or existing test change.

## Gates
- Before the change, run `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root to get a baseline.
- After the change:
  - Debug and Release build, with no new warnings.
  - gate passes, with 1 case added.
  - identity is unchanged.
  - `tools/junction/jt fast build '[topology]'` passes within budget.
- One commit: `Add: Junction exit-side splits among branch lanes sit in the band (D224)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read.
- Commit hash and subject.
- Red evidence.
- Baseline and after counts.
- The probe's first refusing stage and its error.
- Stops.
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
