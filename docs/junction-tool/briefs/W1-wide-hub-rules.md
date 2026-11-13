# W1: the two wide-hub rules, D238 and D239 (RECON-wide-hubs)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Replace two "not ruled yet" refusals with their rules, so that LLL_RRR and LLLL_RRRR hubs get past the structure and programs stages. Placement may still refuse them; that is expected and reported, not fixed.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-w1 <HEAD given at launch>`. Builds exist.
- Another agent works in `.claude/nodes2`: never touch it.
- Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/w1/`. The recon's reverted experiment, `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/rwide/experiments.diff`, shows the two code sites. Read it, but write the rules as stated below, not its hard-coded tie-break.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/RECON-wide-hubs.md` (all);
- `docs/junction-tool/DECISIONS.md` rows D223, D227, D238, D239;
- `src/junction/topology/junction_topology.cpp` 930-960 (refusal 2) and 1150-1200 (refusal 1, and the steps "a", "b" and "c" around it);
- `src/junction/topology/CLAUDE.md`;
- `src/tests/junction/topology/junction_topology.cpp` 340-380.

## Rules
1. **D238 (programs, jt ~1171).** A diverging joiner that has crossings starts with `SBend(Diverge)` on its side hand, as every other diverging edge does. Its crossings come from step "b", unchanged, and its arrival from step "c", unchanged. The Corner-at-start form stays only for a diverging joiner with no crossings.
2. **D239 (structure, jt ~944).** Two joiners crossing in the centre band with equal `near_end`: the edge whose branch in slot is lower (west) goes over. Name the slot field you compare, and cite its definition. If the two edges' branch in slots cannot be read without choosing between fields, STOP.

## Tests (`src/tests/junction/topology/junction_topology.cpp`, `[junction][topology]`), red first
1. **"programs: a diverging joiner that crosses starts with a Diverge S-bend"** (outer). LLL_RRR x L_R under pairing any: programs succeed. The edge the recon names (edge 21: N5 to N20) starts with SBend, role Diverge, and holds an Under or Over token for each of its crossings. Red: it fails today with the refusal text; record it.
2. **"structure: joiners crossing at one level break the tie by branch slot"** (outer). LLL_RRR x LL_RR under pairing any: structure succeeds. For the pair the recon names (edges 13 and 39), the edge from the lower branch in slot is the one with the bridge. Red: it fails today with the refusal text.
3. **"wide hubs: how far each gets"** (characterisation, WARN only, tag `[topologysweep]`). For LLL_RRR x L_R, LLL_RRR x LL_RR, LLL_RRR x LLL_RRR, LLLL_RRRR x L_R, LLLL_RRRR x LLLL_RRRR, LLL_RRR x LLLL_RRRR and LLLL_RRRR x LL_RR, under pairing any, WARN the first stage that fails and its error, or "seed places".

## Changes
Only the two sites above, plus the tests. CLAUDE.md, with no D-ids or design, brief or recon references, in `src/junction/topology/CLAUDE.md`:
- add "- A diverging joiner that crosses starts with a Diverge S-bend, then crosses and arrives as any joiner; only a diverging joiner with no crossings starts with a Corner."
- add "- Two joiners crossing at the same main-line distance: the one from the lower (west) branch in slot goes over."
- Remove any line these two make false.

## Do not
- Touch placement, or the F2 and F3 refusals ("a crossed lane in edge", "a bridge over several band crossings"). They are reported, not fixed.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Identity or any existing test changes. Neither rule fires on any hub that works today, so nothing may move.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`.

One commit on wt-w1: `Add: Junction rules for crossing diverging joiners and joiner crossing ties`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- test 3's lines verbatim;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
