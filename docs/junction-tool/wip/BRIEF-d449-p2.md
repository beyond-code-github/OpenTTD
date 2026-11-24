# BRIEF d449-p2: S-bends from v1's slot solve, sized by HiGHS, against CP-SAT's free optimum (D449 proof, step 2; a probe)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 120 minutes. Solves: per hub at most one HiGHS stage A on the locked set (15 s), one CP-SAT min-length run (`--objective`, 8 workers, at most 120 s) and the stage B/polish that follow. No retry loops.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build Release `build-rel` (`cmake --build build-rel --target openttd_test -j4`; `JT_JOBS=4`). Never touch ng1 or any openttd process. Never use git stash. OR-Tools: `/tmp/claude-1000/-home-pete-OpenTTD/06bbdccc-e14a-42a9-b25a-da0995d90c4b/scratchpad/ortools-venv/bin/python`.
- P1 (`docs/junction-tool/wip/probes/d449/p1.md`, read it whole): CP-SAT's min-length drawings use 8 (T1) / 24 (T2) inserts; node rules explain 2 / 6; the rest sit at ports (fans, close-ups), on straight legs and between crossings: what v1's slot layer computes. D449 (DECISIONS) names v1's `planar_slots` as the precedent.
- v1's slot layer: `src/junction/planar/planar_slots.{h,cpp}` (`BuildPlanarSlots(topology, channels, sites, over)`; per channel, intervals `pre`, `x<k>`, `mid<k>`, `post` cut at its channel crossings, each with tracks left to right and integer slots; `links` across sites). Read `src/junction/planar/CLAUDE.md` "Slot layer (planar_slots)" first.

## 2. The probe (decided)
Behind env `PLANAR_D449_SLOTS=1` in the `[slotsstep2]` probe (add T3 to its hub loop if that is one line: `PlanarTText(3)`; otherwise T1 and T2 only, say so):
1. **Slots:** call `BuildPlanarSlots` with the inputs the v2 variant already has (its topology, channels, resolved sites, `over`); if it cannot take them without new code (missing input, a type mismatch you would have to convert), STOP and report exactly what is missing.
2. **Map slot changes to inserts:** for every topology line (Line track) and every pair of consecutive intervals where its slot changes by Δ ≠ 0 (and every site link with a non-zero step), the S-bend lies in the line's v2 stretch(es) between those two boundaries (an interval boundary is the line's channel-crossing event; a site link is the line's node end). Choose in that stretch the flex insert whose heading moves the line toward the sign of Δ (verify the sign convention on one hand-checked insert and state it), nearest the boundary the change happens before. Every chosen insert is "on"; every other flex insert of every track is "off".
3. **Lock:** feed that on/off set through the existing lock mechanism (`curve_flex_from` reference lengths with `curve_flex_above`, as `PLANAR_STAGEA_LOCK_FILE` does: on = reference 1, off = 0): on inserts get their least length, off inserts are held at 0. Then the probe's normal path: stage A (HiGHS, 15 s, the probe's defaults), contraction, stage B, polish, judge, domain.
4. **Yardstick:** for the same hub, export stage A with the defaults and all inserts free (`PLANAR_STAGEA_EXPORT`), solve with CP-SAT `--objective` (min length), and read its objective (track length) and box (from its point values) as the free optimum. If CP-SAT does not prove optimality in 120 s, report its best and its bound.
5. **Count:** inserts on (from slots) vs CP-SAT's used inserts in its min-length solution: same / slots-only / CP-SAT-only.

## 3. Report per hub (the numbers for the engineer and the review)
- Slot solve: status, time, objective, number of non-zero changes and links.
- Locked set: inserts on; stage A status and time (HiGHS); stage B, polish, judge, domain lines; final track length and box (width x height, area).
- CP-SAT free optimum: track length, box, proven or not.
- Ratio: locked track / optimum track, locked area / optimum area.
- Overlap of the insert sets.
- If the locked stage A is Infeasible: the conflict families (the probe's conflict listing) and which locked inserts appear in it.
Write it to `docs/junction-tool/wip/probes/d449/p2.md` (at most 80 lines).

## 4. Do not
- No change to `BuildPlanarSlots`, Shape, the drawing program's rules or defaults. Only the probe knob and its mapping code (file-local in planar_domain_encoding.cpp). The slot objective is v1's as-is (steps + 1e-4 width): this measures it, it does not change it.

## 5. Commits
- `Add: Junction: D449 P2 probe, S-bends from v1's slot solve (PLANAR_D449_SLOTS)`, then `Doc: Junction: D449 P2 numbers`. End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Gate: `JT_JOBS=4 tools/junction/jt fast build-rel '[contraction]' 5` (unchanged).

## 6. Report
References read; commits; the per-hub numbers of part 3 (a compact table); the sign convention check; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.

## Run 2: P2b, slots PLUS v1's gap steps (2026-11-25, after P2: slots alone leave stage A Infeasible on T1/T2/T3; port rows in every conflict; T1 slot objective 0 while CP-SAT needs 8 inserts, 3 at ports)
- Start from wt-c2 HEAD (79e401d99b probe + 036b203084 numbers). Keep the P2 mapping as is.
- Add v1's gap steps: call `BuildPlanarGapSteps` (planar_gaps.h; read `src/junction/planar/CLAUDE.md` "Gap steps") with the same rebuilt inputs, and, if the D449 knob is set, also `PlanarCandidateSteps` (candidate straight-channel steps: v1 makes each a variant bit; here: report them, do NOT lock them, they stay free, and say how many there are). Each gap step (fan or close-up of a channel, `toward`) becomes one locked insert per lane of that channel in the stretch next to its port (fan: after the in port, before the fray; close-up: after the join, before the out port), direction from `toward` (same sign convention as P2). Every other insert stays held at 0, except the candidate-step inserts (free).
- Then exactly as P2: HiGHS stage A on the locked set (15 s), the pipeline to judge/domain, and the same comparison table against the CP-SAT min-length runs already in `docs/junction-tool/wip/probes/d449/p2.md` (do not rerun CP-SAT).
- If stage A is still Infeasible: the conflict families as before, and which locked inserts (slot or gap) are in it; plus ONE run with every insert free except the gap-step inserts forced on (HiGHS 15 s) to separate "gap steps wrong" from "more S-bends needed".
- Append the results to p2.md (section "P2b"). Commit `Add: Junction: D449 P2b, gap steps in the slot lock` and `Doc: Junction: D449 P2b numbers`.
