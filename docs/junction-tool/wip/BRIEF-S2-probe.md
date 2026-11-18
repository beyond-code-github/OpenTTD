# Brief S2 (probe, throwaway): lane offsets inside channels come from slots

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar` at {HEAD}. Create the branch `probe-s2` from it and commit only there. Finish with `git switch wt-planar`.
- Time box: 3 hours. This is a probe. Its product is the measurement, not code that lands.

## Read first
- `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §15.
- `/home/pete/OpenTTD/docs/junction-tool/wip/RECON-sideways-sites.md`: the whole file. Its rows are referenced as R1 to R26 below.
- `src/junction/planar/planar_slots.h`.
- `src/junction/planar/planar_components.cpp`, only the regions the recon cites for R1, R2, R4, R8, R10 and R16, plus `MappedComposer::Build` around :1151-1200.

## The change (probe-s2 only)
1. **Read the slots.** In `MappedComposer::Build`, right after `res` is computed (~:1171):
   ```cpp
   const PlanarSlots slots = BuildPlanarSlots(t, ch, res, paths.over);
   ```
   On a non-empty `why`, call `M.Fail(slots.why)` and return. Keep `slots` reachable by the builders: a local captured by the lambdas, or a member if the lambdas cannot capture it. Write a small local helper `slot_of(c, interval_name, line) -> int`, which looks up the `Line` track.
2. **Interval of a segment.** For a channel c:
   - **Core** (one heading): `pre` is the Flex lead (`piece_segs[c][lane][0]`). Every deck sits on `d0`, so all `x<k>` and `mid<k>` intervals share one segment. `post` is `run[j]`.
   - **Bundle:** piece p's lead is in interval `x<i>` when `crossing_piece[i] == p`. A piece between crossing pieces is in `mid`, a piece before the first crossing in `pre`, and a piece after the last in `post`.
   - When several intervals share one segment, their slot differences between lanes must be equal. If they are not, call `M.Fail("slot change inside one piece")` for that variant and count it.
3. **Offsets from slots.** Replace these constants with slot differences. For lane j relative to lane 0 of the same channel, in the segment's interval, the offset is `2 * (slot_of(c, iv, line_j) - slot_of(c, iv, line_0))`:
   - R1/R2/R8: in `build_core`, `offs[j]` for the Flex lead uses the `pre` slots, and the post-deck run (`:1363-1366`) uses the `post` slots. `JoinEntry` is no longer called on this path; leave the function in place.
   - R4: in `Bundle`, the `2*j` in the lane equalities (`:1101-1105`) becomes the slot difference of the interval that piece's lead is in.
   - R16: the join out gap (`:1038`) becomes `2 * sigma * (slot difference of the out channel's pre)`. Take the sign convention from the current code, which assumes a difference of 1. If the sign mapping is not clear, stop.
   - R10: the fray gap `Ge(..., prio ? 4 : 2)` becomes `Eq(..., 2 * slot difference)` using the fray channel's `post` slots, with the sign convention of the current code.
4. **S-bends from slot changes.** Where lane j's offset relative to lane 0 changes between consecutive segments of the same lane within a channel (by Δ slots), insert into lane j's strand, at the start of the later segment:
   - `AddSeg(l, h, 0)`;
   - `AddSeg(l, Turn(h, sign), 2|Δ|, 2|Δ|)`;
   - then the segment continues on h.

   Here `sign` is +1 when lane j moves right relative to lane 0, and h is the later segment's heading. On a diagonal heading, call `M.Fail("a slot step on a diagonal piece is not composed")` and count it.
5. **Left unchanged:**
   - candidate steps and gap steps (R3, R19, R20): channel-wide moves between channels, which slots do not describe;
   - Double and the parallelograms (R5, R6, R13);
   - fray legs (R11), joiner `lo` (R12), the prio close-up and rows (R14), exit steps (R15), forks (R17, R18), and the port rules (R21 to R23).

## Measure
- `tools/junction/pv run build-rel T2`, then the same for T3 and LL2 with the existing tag list, one at a time. Diff each against the baselines made before the change: run `pv run` and `pv save` on `wt-planar` at {HEAD} first, before you switch branches.
- **Report per hub:**
  - variants built and failed, with each failure reason and its count, the two new reasons separately;
  - the best area against the baseline best;
  - how many variants got better, got worse or stayed equal, and the median change;
  - the median per-variant solve time against the baseline.
- `tools/junction/jt fast build-rel '[planar]'`: report its failures. They are expected where tests pin areas; list them, do not fix them.

## Do not
- Touch candidate steps, gap steps, the variant bits, joins, frays (except the gap in R10), forks, Double or ports.
- Tune slots or weights.
- Land anything on `wt-planar`.

## Stop conditions
- A sign or lane-order mapping between slots (left to right across travel) and the composer's lane lists (far to near, `Lanes(c)`) that you cannot settle from the code. Report what you found.
- More than half of T2's variants fail with "slot change inside one piece". Report the first five cases: channel, intervals and slots.

## Commit and report
- Commit on `probe-s2`: `Probe: Junction S2 lane offsets from slots (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md` (do not commit it) with the measurements, every place where slots differed from the old constant (channel, interval, old offset, slot offset), and the failure cases.
- Reply with the per-hub summary, the commit hash and `git status`. When you have reported, stop.
