# Recon O3: band occupancy, segments (A4), event order and clearance before 2b2b (read-only)

You are a read-only recon. You change no committed file. Throwaway probe code is allowed in your worktree and must be reverted before you finish (save it as a patch under `docs/junction-tool/wip/` in the main worktree, untracked). Keep your context small: send command output to files, read with offset and limit, and report only conclusions.

## Goal
G4's item rows (O1, with crossers' pieces) and port fans (O2) are recorded as static rows. The last occupancy family is the bands: band-edge pieces separated by O3 (DESIGN-G4 §3 O3, §11.4), over A4 segments and A3 event-order binaries, plus clearance (D271 moved it here). 2b2b will be split into steps designed from this recon. The orchestrator needs facts, not a design. Write them to `/home/pete/OpenTTD/docs/junction-tool/RECON-O3-bands.md`, short and factual. Mark anything you inferred rather than read or ran as INFERRED.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4c2`: `git -C /home/pete/OpenTTD/.claude/o4c2 checkout -B wt-recon-o3 <HEAD given at launch>`. It has `build-rel` (Release, stale: rebuild `openttd_test`; delete stale `.pch` if the build complains). Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/recon-o3/`.
- Other agents may work in `.claude/ng1`. Touch no worktree but `o4c2`; `.claude/nodes2` is reserved for the engineer's game. A GUI game runs from `ng1/build-rel/openttd`: never touch it.

## Read first (once each)
- `docs/junction-tool/DESIGN-G4-static.md`: §1 A1-A4 (lines 54-66), §2 table rows "clearance" and "occupancy" (71-94), §3 O3 (104-128), §5 (150-178), §11.2 and §11.4 (328-376);
- `docs/junction-tool/DECISIONS.md` rows D255, D256, D260, D263, D271 (grep);
- `src/junction/topology/junction_topology.h` lines 260-300 (the arrangement: band_replacements, band_neighbours, band_event_pairs);
- `src/junction/placement/junction_placement.h` lines 130-260 (the static records added by 2b2a2-2b2a3b);
- `src/junction/placement/junction_placement.cpp`: `add_occupancy` (the flat sort and clash path for pieces that are not on item rows), the clearance X family (grep `clearance`), `detect_y` / clearance Y, `plan_clearance`;
- `src/tests/junction/topology/junction_topology.cpp`: `RecordItemRowSeed` and the 2b2a3 tests (how seeds are built).

## Questions (answer each, with line numbers)
1. **Band occupancy today.** At the three seeds (T, LL_RR x L_R, L_R x LL_RR; at `system.solved`), list the occupancy pairs (recorded clashes) that are not on an item row: count per hub, by kind of the two pieces (band-edge Corner/SBend/run/Heads tile, node, row piece, fan), and how many are band-edge vs band-edge of two different edges. Which of them are already implied by the static rows of 2b2a2-2b2a3b (same pair on an item row or O2 end row)?
2. **Neighbour pairs.** For each band (arrangement.band_neighbours), list per hub the neighbour pairs (west, east, extension_only). For every band-edge vs band-edge clash of question 1, is the pair a neighbour pair? Count clashes between non-neighbours, and say what lies between them at the seed.
3. **Segments (A4).** For each band edge at the seeds, its boundary events in travel order (band nodes, band crossings, Under pairs of doubled bridges, node and port ends) and the chain elements between them. Do bends ever straddle an event row other than a crossing's S-bend (§11.4)? For each neighbour pair, the segments that coexist (overlapping row ranges at the seed).
4. **O3 check at the seeds.** Evaluate O3 (i) run order and (ii) boundary rows as DESIGN-G4 §3 defines them for every neighbour pair at the seed (by a probe, as values, not rows): how many hold, which fail and why. For (ii), which bends approach a partner other than the pair's other edge?
5. **Event order.** arrangement.band_event_pairs per hub (count) and, at the seed, which order each pair takes (y of the two events). Which O3 rows would be conditional on an event-order pair (the pair's coexistence begins or ends at one of its events)?
6. **Square form.** At the LL² seed at form 100100000 (as the 2a1/2a2 tests build it; grep `100100000` in the tests), repeat 1-2 for the square crossing's run and Corners: how many band pairs involve the square pieces.
7. **Clearance.** List today's clearance X and Y families: what they separate (single-bridge deck vs other bridges' heads; head row sides), how many relations at each seed, and which DESIGN-G4 §2 maps to O3 (deck as occupant) or to a static band side. Is any clearance relation point-dependent (chosen from a flat solve)?
8. **Test time.** The sweep is at 55 s of 60 and the gate at 26 s of 30. List the 5 slowest test cases of each (from a `jt` run's timing output or Catch2 durations) so the orchestrator can budget 2b2b's tests.

## Do not
- Commit anything; leave `o4c2` clean on `wt-recon-o3` at the launch head.
- Use `pkill`, a broad `kill`, or any form of `git stash`.
- Spend more than about 2 hours; report what is done.

## Report (short)
- the recon file's path and an eight-line summary (one per question, in numbers);
- the probe patch's path;
- `git -C /home/pete/OpenTTD/.claude/o4c2 status --porcelain` and `git log -1`.

When you have reported, stop.
