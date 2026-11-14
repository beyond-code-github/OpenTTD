# Design amendment: DESIGN-G4 with the crossing form as a choice (D260, 2026-11-14)

Run as `junction-reviewer` with model opus and effort xhigh (engineer approved the amendment, 2026-11-14). This is a design task, read-only on committed code: commit nothing. Throwaway probes in the worktree named below are allowed and reverted. Write the amendment as a new section "§11 Amendment: the crossing form is a choice (D260)" appended to `docs/junction-tool/DESIGN-G4-static.md`, at most about 150 lines, plus any line-level corrections to §4 and §8 marked "(amended, D260)". Keep it tight: this amends your own design; it does not redo it.

## What happened
- You wrote DESIGN-G4 (`docs/junction-tool/DESIGN-G4-static.md`). The engineer chose D256 (square crossings) and D258 (one run under consecutive same-hand crossings).
- Step 2a1 (brief `docs/junction-tool/briefs/G4-2a1-square-crossing.md`) STOPPED. With the programs emitting Corner, Under, Corner and the family "bridge over a horizontal run", **T no longer seeds**.
  - Edge 8 goes over edge 4 just before edge 4's S port.
  - Edge 8's bridge column is pinned to its own S port column. Edge 4's port is 1-2 columns away (the S ports keep the notation spacing).
  - A Corner pair (k ≥ 1, curve rules) needs more sideways room than that.
  - The fixed path's conflict was: "chain run (edge 10) <- chain run (edge 4) <- Corner size (edge 4) <- bridge over run (edge 8, node 4) <- chain run (edge 8) <- chain run (edge 1) <- S port spacing <- S port spacing". With the new relations disabled it fails on occupancy, edge 4 against edge 8 on row 8.
  - The uncommitted work is in `/home/pete/OpenTTD/.claude/ng1` (branch wt-2a1) and in the patch `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2a1/wip-stopped.patch`.
- **D260 (engineer):** per single-bridge band crossing, the joint model chooses the form by one binary, either the S-bend form (today's family with t and before/after) or the square form (D258 for runs). Legality and flat cost decide, not look. Read D256, D258 and D260 in `docs/junction-tool/DECISIONS.md` (the last rows), and D257 (expansion guidance).

## The amendment must give
1. **Programs.** How an edge's program represents a crossing whose form is not fixed. For example, a neutral crossing token pair that placement expands by the binary, or both token forms with a selector. The choice must keep the edge ends, node shapes and the arrangement (ARR1, ARR3) well defined. State what the fixed path and the hover read: the chosen form as data.
2. **Placement families.** Both families are guarded by the binary with a tight M from bounds (§5's helper):
   - S-bend: the over-S-bend rows, t, and before/after (D248) when two S-bends flank the crossing;
   - square: the run rows.
   
   Say how the bends' lengths behave in the unchosen form. A bend of the unchosen form has length 0 and is no bend (zero-size bends already exist), or say what else is needed. The curve gaps must stay exact under both forms.
3. **D258 runs under the choice.** Consecutive same-hand crossings either all take one run, or each chooses. Say which, and why.
4. **The band model (§3).** With S-bend crossings back, the crossing can lie inside a bend (ARR2's finding). Say how the profile rows and the event y handle a crossing inside a bend, and whether the binary count changes. Give the new totals for T, LL_RR x L_R, L_R x LL_RR, LL_RR x LL_RR and LLL_RRR x LLL_RRR.
5. **Measurements, where cheap.** With the 2a1 WIP plus a selector probe in ng1:
   - T seeds with the S-bend form;
   - which form the joint picks on LL_RR x L_R and L_R x LL_RR, and its objective against 27300 / 86350;
   - LL_RR x LL_RR round 0: does it go feasible with the choice available (status and objective)?
6. **The revised sub-steps** replacing 2a1 and 2a2, each with its red test and re-pin expectations. Their order with 2b1-2b5. What 2a2 still retires.
7. **Decision cards,** only for what is genuinely the engineer's. Otherwise, your recommendation.

## Read
- `DESIGN-G4-static.md` (all, your own); the 2a1 brief and the WIP patch;
- `junction_topology.cpp` ~1195-1225; `junction_placement.cpp` (the over-S-bend block ~1812-1870, the 2a1 WIP's run family); `junction_joint.cpp` (t, w, `over`, `over_after`; the z binaries of zero-size bends).

## Worktree
`/home/pete/OpenTTD/.claude/ng1` holds the uncommitted 2a1 WIP on branch wt-2a1. Probe on top of it. At the end, restore it to exactly the WIP state: `git diff` must equal the WIP patch. Delete stale `.pch` files if the build complains. Outputs go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g4a/`.

Never touch other worktrees (`.claude/nodes2` is reserved for the engineer's game). Never use any form of git stash, pkill or a broad kill. Never start a game client. Use `timeout` on every run. Leave no background processes.

## Output
§11 appended to `docs/junction-tool/DESIGN-G4-static.md` in the main checkout (it is tracked; leave it uncommitted). Report back a 12-line summary, any decision cards' titles, and the sub-step list. When you have reported, stop.
