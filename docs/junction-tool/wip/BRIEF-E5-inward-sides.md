# BRIEF: E5 inward-only keep-apart candidates, checked against V's 197 and the T3 certificates (probe, read-outs)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 60 minutes in total. Each solve keeps its cap (15 s for single solves). Run each measurement once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Context: in the pairwise keep-apart block, a pair (i, j) of face walls has candidates c = axis_index * 2 + side. axes[0] is wall i's inward normal; axes[1] is wall j's inward normal when the walls differ mod 4, else their shared direction; side 0 = from wall i to wall j. So **inward candidates** are c0 (j at least the spacing beyond i along i's inward normal) and, only when the walls differ mod 4, c3 (i beyond j along j's inward normal). The read-out ed29dcf5d3 ("planar draw: T3 exact sides against the pipeline's", log docs/junction-tool/wip/probes/e5land/exactsides-a973e7081f.log) shows that the pipeline's side picks that T3's exact drawing contradicts are mostly outward candidates. This step tests the rule "only inward candidates" without guessing: against V's real PSG 197 drawing (the answer key), and as an exact solve on T3.

## 2. Read first (once each)
- `CODINGSTYLE.md`, C++ part.
- `src/junction/planar/planar_draw.cpp`: the pairwise block from `if (options.clearance && options.pairwise) {`: `axes`, the `sides_from` candidate loop (and how `facing_sides` restricts it), and the binary branch with no `sides_from` (one binary per candidate, the `sepdir` row).
- `src/junction/planar/planar_draw.h`: `facing_sides`, `short_sides`, `PlanarDrawPipelineOptions`.
- `src/tests/junction/planar/planar_draw.cpp`: the test "PSG 197 clearance rows evaluated on V's drawing" (how V's points become a `sides_from` reference, its sections such as "sepV allfaces W1"), the certificate test (exact face options), and the hub pipeline test's env reads.

## 3. Changes
### 3.1 One option, two read-outs
- `PlanarDrawOptions`: `bool inward_sides = false;` doc `///< A probe: in pairwise mode every pair's candidates are only the inward ones: wall i's inward normal from i to j, and, when the walls differ mod 4, wall j's inward normal from j to i (with sides_from: the better of these on the reference; without: binaries over these only).`
- `PlanarDrawResult`: `uint32_t inward_short = 0;` doc `///< With inward_sides and sides_from: pairs whose best inward candidate's margin on the reference is negative.` and `std::vector<std::string> inward_short_tags;` (the first 40 such tags, each with ` best inward margin <m>, best overall margin <m'>`).
- In the `sides_from` branch: when `inward_sides`, restrict the best-margin choice to the inward candidates (as `facing_sides` restricts its pairs), count and record `inward_short`. The free/forced/probe branches keep their precedence. If both `facing_sides` and `inward_sides` are on, `inward_sides` decides (do not combine).
- In the binary branch (no `sides_from`): when `inward_sides`, write binaries and rows only for the inward candidates; the `sepdir` row sums those binaries (= 1).
- `PlanarDrawPipelineOptions`: `bool inward_sides = false;` passed as `.inward_sides` wherever `.facing_sides` is passed.

### 3.2 Measurements (hidden tests)
- **M1, the answer key:** in the test "PSG 197 clearance rows evaluated on V's drawing", add a section (env `PLANAR_PROBE_INWARD` set) that runs the same evaluation as "sepV allfaces W1" with `inward_sides = true`, and WARNs: total slack and sep slack (as that section prints), `inward_short`, and every `inward_short_tags` entry, decoded with the walls (track#piece h<heading>, as the hub test decodes) if the test can do so; plus the same section's numbers with `inward_sides = false` for comparison (one run each).
- **M2, the T3 certificate:** in the test "planar draw: crowding certificates on T3 and LL2", when `PLANAR_PROBE_INWARD` is set, run the T3 cases {75}, {29}, {66} and {75, 29, 66} (B 256) and the LL2 case {33} again with `inward_sides = true`. WARN `cert inward <hub> faces <F>: <status> ms <ms> binaries <n>`.
- **M3, the pipeline:** in the hub pipeline test, `o.inward_sides = std::getenv("PLANAR_PROBE_INWARD") != nullptr;` and WARN `<hub> inward: short <inward_short>` from the last round. Run T1, T2, T3, LL2.
- `src/junction/planar/CLAUDE.md`: one bullet for `inward_sides` (a probe, off by default), same commit.

## 4. Do not
- No change with the option off. No interpretation. No PSG 197 pipeline run (only the answer-key evaluation).

## 5. Stop conditions
- The V's-drawing test cannot take `inward_sides` without changing what it evaluates otherwise: report what differs.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- M1: `PLANAR_PROBE_INWARD=1 build-rel/openttd_test "PSG 197 clearance rows evaluated on V's drawing" > build-rel/inward-197v.log 2>&1` (use the exact test name from the file).
- M2: `PLANAR_PROBE_INWARD=1 build-rel/openttd_test "planar draw: crowding certificates on T3 and LL2" > build-rel/inward-cert.log 2>&1`.
- M3: `PLANAR_PROBE_INWARD=1 build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/inward-hubs.log 2>&1`.
- Copy the three logs to `docs/junction-tool/wip/probes/e5land/inward-{197v,cert,hubs}-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: inward-only keep-apart candidates, a probe (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed; members added.
- Suite results (count and time per gate).
- M1: the comparison numbers (inward on and off), `inward_short`, every short tag line verbatim.
- M2: the cert inward lines.
- M3: per hub, the last-round slack by family, `inward_short`, final status, stopped stage; for T3 and LL2 the stretched rows decoded.
- Join Catch's 80-column wraps first. Stops and anything left undone. State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
