# Brief 2b: the composer builds jogs from the gap steps (no jog switches)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, head `{HEAD}`, build dir `build-rel` (Release).
- Plan: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-pieces.md` §3 sources 3 and 5, §7 step 2. The previous step added `src/junction/planar/planar_gaps.{h,cpp}` (`ResolvePlanarSites`, `BuildPlanarGapSteps`) and its tests `src/tests/junction/planar/planar_gaps.cpp`.
- Today the mapped composer adds every jog as an optional piece with a solver switch (`Model::opt_group` groups ending in 'j': `flex{c}j` in `Flex`, `join{s}j` in the n-lane `Join`). This step decides those jogs before the solve and emits only the jogs that exist, as required pieces. `merge{n}j` (LL²'s one-lane joins), fray '+'/'-' and bundle '+' groups are NOT changed.

## 2. Read first (once, offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md` lines 55-80.
- `src/junction/planar/planar_gaps.h` and `.cpp` (whole); `src/tests/junction/planar/planar_gaps.cpp` (whole).
- `src/junction/planar/planar_model.h` lines 215-235 and 335-380 (`opt_group`, `group_z`, `jog_count`, `AddSeg`, the `PLANAR_JOGS` knob).
- `src/junction/planar/planar_components.cpp`: 733-750 (`PlanarSitePerNode`, `MappedVariant`), 826-835, 929-1010 (n-lane `Join`, `Flex`), 1095-1140 (site resolution), 1232-1260 (`build_core`), 1300-1350 (join build, `join{s}j`), 2170-2180 (tag), 2335-2375 (`SolvePlanarMapped` variant enumeration).
- `src/tests/junction/planar/planar_components.cpp` lines 28-100.

## 3. Baseline first (before any code change), at the launch head
Run, output to files, and record area and wall time of each:
```
PLANAR_ONLY=o0t1f1 PLANAR_JOGS=1000101 <T3 mapped test>
PLANAR_ONLY=o0t1f1 PLANAR_JOGS=0000101 <T3 mapped test>
```
where `<T3 mapped test>` is `build-rel/openttd_test "planar components: T3 from its topology text (mapped) at the hand composition's area" -s` (grep the INFO line `variant ... area ... in ... ms`). Expected about 1941.25 and 1960.25, each about 0.25 s. If the area differs from these by more than 1%, stop and report.

## 4. Changes

### 4a. planar_gaps: the candidate step and the step's side
- `PlanarGapStep` gains `int8_t toward; ///< Where the step moves, +1 right of the stepping channel's travel, -1 left: a close-up toward its neighbour, a fan away from it.` For a close-up it is where the neighbour lies for the stepping channel (the §3 "where Y lies" value seen from that channel's own port line). For a fan it is minus where the neighbour lies for the fanning channel.
- New:
```cpp
/** A candidate sideways step (DESIGN-pieces §3 source 5): a straight channel over crossings may step once before its core, away from its in port's neighbour. */
struct PlanarCandidateStep {
	uint32_t channel;
	int8_t toward; ///< +1 right of travel, -1 left.
};
std::vector<PlanarCandidateStep> PlanarCandidateSteps(const PlanarTopology &topology, const PlanarChannels &channels);
```
  Rule: a channel is a candidate when it has exactly one up port and one down port, both port headings are equal (it turns 0), and `crossings` is not empty. Its in port P = the single up port. Look at P's lines in `topology.ring`: for each ring neighbour (predecessor of P's first-in-ring line, successor of its last-in-ring line, no wrap) that is on the same `PlanarSide` and belongs to another port, the neighbour lies at +1 (predecessor) or -1 (successor) for an in port (the rule of §3 in brief 2a). Exactly one such neighbour: `toward` = minus that value. None or two: no candidate. Result in channel order.
- Tests in `planar_gaps.cpp` (red first, with the new function returning `{}` and `toward` left 0 in BuildPlanarGapSteps):
  1. Extend the T3 and T2 step tests: EF's fan `toward` -1, AF's close-up +1, ED's close-up +1.
  2. **"planar gaps: the T's candidate step"**: T3 and T2 (flip 1): exactly one candidate, B's channel (`channel of B1`), `toward` -1.

### 4b. The composer
1. **Site resolution:** replace composer lines 1107-1131 (the local `Resolved` struct and loop) with a call to `ResolvePlanarSites(t, ch, this->variant.ties, why)`; on a non-empty why, `M.Fail(why)` and return as the current code does. Keep the local name `res` (now `std::vector<PlanarResolvedSite>`), and fix uses whose types change (`side` is now `int8_t`). The tie counter `tie` that later code continues (per-node merges, about line 1369) must start where the site loop left it: set it to the number of sites that are not per node and have `straight == UINT32_MAX`.
2. **Steps:** after resolution, `const std::vector<PlanarGapStep> gap_steps = BuildPlanarGapSteps(t, ch, res, why)`; on a non-empty why, `M.Fail(why)` and return.
3. **Exit jog (n-lane `Join`):** add a parameter `bool exit_step` (no default). In the loop at about lines 975-979: when `exit_step`, emit `M.AddSeg(o, Turn(h, +1), 2, std::nullopt, 1.0)` (required: no `opt`); when not, emit no piece between the `FANW` run and the `STUB` run. The pairing code after the loop (`st[k]`, `f_first`) runs only when `exit_step` (it orders the step pieces; read it and stop if it does more than that). At the call site (about line 1346): `exit_step` is true when `gap_steps` holds `{site.channel, fan = false}`; when that step's `toward` is not +1, `M.Fail("an exit step to the left is not composed")`. Remove the `M.opt_group = "join{}j"` lines around the call.
4. **Straight-channel jog (`Flex`):** replace the parameter list with `Flex(const std::vector<std::string> &lanes, int h, const std::vector<int> &offs, int jog)`. Per lane: `jog == 0` → `last = AddSeg(l, h, 0)` only (one piece); `jog != 0` → `AddSeg(l, h, 0)`, `AddSeg(l, Turn(h, jog), 2, std::nullopt, std::sqrt(2.0))` (required), `last = AddSeg(l, h, 0)`. Add, for `jog != 0`, the lanes' diagonal pieces equal in length (`Eq(diag[j].L - diag[0].L, 0)`), so lanes step together. The existing offset rows on `last` stay. Remove the `flex{}j` opt_group lines in `build_core`. `jog` = the candidate's `toward` when `this->variant.steps` has the bit of that candidate (its index in `PlanarCandidateSteps`), else 0. If the bit is set for a channel that `build_core` does not build (not a core in this variant), `M.Fail("a candidate step on a channel that is not a core")`.
5. **Variants:** `MappedVariant` gains `uint64_t steps = 0; ///< Bit i set: the i-th candidate step (PlanarCandidateSteps) is taken.` `SolvePlanarMapped` computes `PlanarCandidateSteps(topology, channels)`, counts them into the 6-bit cap check (and its message: "..., {} triangles and {} candidate steps are too many variants"), and enumerates `steps` as an innermost loop. The tag becomes `o{}t{}f{}s{}` everywhere it is formatted or compared (2176, the `PLANAR_ONLY` filter).
6. **Remove** the `PLANAR_JOGS` knob and `Model::jog_count` (planar_model.h).
7. **CLAUDE.md** (`src/junction/planar/CLAUDE.md`): in the mapped-composer facts, replace "an exit's jog and a straight channel's jog stay free ('j'), being alignments, not width changes" and the `PLANAR_JOGS` mention with the new facts (exit steps from the gap steps, required; a candidate step as a variant bit, tag `s`), and add `toward` and the candidate rule to the gap-steps section. Present tense, no decision ids. Update the T3/T2 timing line only with numbers you measured in §5.

## 5. Measure after (record area, variant, wall time)
- `PLANAR_ONLY=o0t1f1s1` and `PLANAR_ONLY=o0t1f1s0` with the T3 mapped test. Targets: 1941.25 and 1960.25, each under 0.5 s.
- The full T3 mapped test (all variants) and the full T2 mapped test (`"planar components: T2 from its topology text (mapped) at the probe's area"`): area and wall time. T3 at most 1941.25; T2 at most 917.25.

## 6. Do not
- Do not touch `merge{n}j`, `Fork`, `Fray`, `Bundle`, the '+'/'-' groups, `OPTMAX`, the up-front side rule, or curve rows.
- Do not change the direction logic of `Join` beyond §4b.3.
- No new env knobs.

## 7. Stop conditions
- Baseline off by more than 1% (§3).
- After the change, o0t1f1s1 or o0t1f1s0 off the target by more than 1%, or any variant fails with a reason it did not fail with at the baseline (compare: run the full T3 with `PLANAR_TIMES=1` before and after if needed). Report the numbers; do not tune.
- `Join`'s pairing code after the exit-step loop does more than order the step pieces.

## 8. Gates
- `tools/junction/jt fast build-rel '[planargaps]'`, `'[planarmodel]'`, `'[planar]'`, `'[planarcomponents]'`.
- The slow tests named in §5, run by name, output to files.
- One commit: `Change: Junction mapped composer builds jogs from the gap steps`, body naming the removed switches and the new variant bit, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 9. Report (short)
References read; commit; red evidence (tests and failure messages); types, members and functions added/removed; baseline and after numbers (area, variant, ms) for every run in §3 and §5; gate results with count and time; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
