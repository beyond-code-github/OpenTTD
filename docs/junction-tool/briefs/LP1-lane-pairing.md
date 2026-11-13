# LP1: lane pairing as a topology option (D221): LL_RR x LL_RR and other multi-to-multi hubs seed

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-lp1 <HEAD>` (given at launch). Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/ng1 (src/junction_levels.cpp): never touch it. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- D221 (DECISIONS.md, read it): the lane pairing of multi-to-multi turning movements is a user option: A any-to-any (default), B lane keeps lane, C lane per direction. D204/D187 (read them): a branch track splits by direction first, then by lane next to the main line it feeds; the exit side mirrors it.
- Today the lane expansion (src/junction_topology.cpp ~:70-90, the paths loop) refuses `in.lanes > 1 && out.lanes > 1` for turning movements ("multi-to-multi movements are not ruled yet"); 192 of NM1's 243 notation pairs stop there (docs/junction-tool/RECON-notation-matrix.md). Single-lane cases fan i x o (all pairs) today.
- MakeJunctionHubTopology (LR1) builds the topology from two parsed notations; the window (src/junction_gui.cpp) parses the boxes and seeds from it; the key carries lane counts.

## Read first (once each)
CODINGSTYLE.md (both parts); DECISIONS D186, D187, D204, D221; RECON-notation-matrix.md; src/junction_topology.{h,cpp}: JunctionTopology, MakeJunctionHubTopology, the lane expansion; src/junction_gui.cpp the notation boxes, the key, the row with Rotate/Mirror; src/table/settings/gui_settings.ini junction settings (QL2's pattern).

## Changes
1. `enum class JunctionLanePairing : uint8_t { AnyToAny, LaneKeepsLane, LanePerDirection };` (junction_topology.h, doc comments citing D221) and `JunctionLanePairing pairing = JunctionLanePairing::AnyToAny;` on JunctionTopology; MakeJunctionHubTopology gains a pairing parameter (default AnyToAny) and stores it. The hand topologies keep the default.
2. The lane expansion, for a turning movement where both groups have more than one lane:
   - AnyToAny: every in lane to every out lane (i x o), as single-lane turning movements already do.
   - LaneKeepsLane: in lane k to out lane k; unequal lane counts -> fail "lane keeps lane needs equal lane counts".
   - LanePerDirection: a group with several turning movements deals its lanes round-robin to them in movement order (lane k serves its (k mod n)-th turning movement); within a movement the dealt lanes connect to all lanes (or the dealt lanes) of the other group; a group with fewer lanes than turning movements -> fail "lane per direction needs a lane for each direction".
   Single-lane and through movements are unchanged (identity on every existing seed).
3. Window: one push button on the Rotate/Mirror row cycling the pairing (strings: `STR_JUNCTION_LANES_ANY :{BLACK}Lanes: any`, `STR_JUNCTION_LANES_KEEP :{BLACK}Lanes: keep`, `STR_JUNCTION_LANES_DIRECTION :{BLACK}Lanes: per direction`, tooltip `STR_JUNCTION_LANES_TOOLTIP :{BLACK}How the lanes of a multi-lane branch connect to the main line's lanes`), disabled when no turning movement is multi-to-multi; persisted as client setting `junction_lane_pairing` (QL2's pattern); in the key.
4. NM1 rerun: apply /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/nm1.patch (do NOT commit it), run the 243 pairs once per pairing with `NM1_ONE` one by one (shell loop, rc per pair), extend the test locally to take the pairing from an env var: report per pairing: built clean, first-failure groups with counts, crashes (must be 0). Revert the patch before committing.

## Tests (record each red)
1. `[topology]` "topology: lane pairing for a multi-lane branch": LL_RR x LL_RR with each pairing: the lane paths of the branch-in -> W-out movement are, A: 4 paths (all pairs), B: (0,0),(1,1), C: the dealt lanes as specified; and MakeJunctionGraphSeed either seeds or fails with a named later-stage reason (report which). Red: today all three fail with "multi-to-multi movements are not ruled yet".
2. Outer identity: `tools/junction/jt identity build-rel` and the five sweep tags' outputs equal to launch HEAD's (times stripped); LL_RR circuit byte-identical.
3. If LL_RR x LL_RR seeds under any pairing: render + signals + CheckJunctionRules clean (report the box and time); if it fails a later stage, report the stage and reason (that is the next blocker, not this step's).

## Gates
- `tools/junction/jt fast build '[topology]' 5` (report time; the tag was at 5.2 s: if the new test pushes it over, move the test's heavy part to `[topologysweep]` and say so); `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time); the `openttd` target builds Debug and Release.
- One commit: `Feature: Junction lane pairing for multi-lane branches is a topology option (any, keep, per direction)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the NM1 results per pairing and what LL_RR x LL_RR does under each. When you have reported, stop.
