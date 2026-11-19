# Brief G3e (probe): slot-layer S-bends in the track composer (TM2 as DESIGN-tracks §15.2 / §16.3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch -c probe-g3e wt-g3b` (wt-g3b at cfdd91a517: G3b, G3b2, G3c as WIP commits). Commit only on probe-g3e. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 100 minutes. One heavy job at a time. Each run under `timeout -s KILL 900`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first, in the main worktree: `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-G3b-composer.md`, `BRIEF-G3b2-steps-decks.md`, `BRIEF-G3c-copies.md` (what `TrackComposer` is); DESIGN-tracks §15.1-15.2 and §16.3 ("Sideways steps"); DECISIONS D351, D359.

## Why
The track composer builds T2 o1t3f1s0 but it is base infeasible (`<S>/g3c-iis.txt`): after the CB join, B1 (moved one lane toward the joiners by its D351 copy over C2) and B2 (where C2 lands, beyond B1) end at least 3 half-tiles apart, yet the CB port pins CB1 and CB2 2 apart, and CB1/CB2 are straight: a lane spacing change on an axis-only stretch with nowhere to go. §16.3: then TM2 adds the step as a slot change (§15.2's S-bend). The landed slot layer (`BuildPlanarSlots`) already computes every line's slot per channel interval and each site link's step; `MappedComposer` reads it. This probe makes the track composer take its S-bends from it, behind `PLANAR_SLOTBEND=1`, and measures.

## Read first (code)
- `src/junction/planar/planar_slots.h` (`PlanarSlots`, intervals `pre, x<k>, mid<k>, post`, `PlanarSlotTrack` kinds, `PlanarSlotLink`); `src/junction/planar/CLAUDE.md` "Slot layer" (the rules: a doubled line moves onto its copy's slot after its last doubled crossing, uncharged; a link's step is the line's sideways move across a site in lanes, + = right of travel).
- `src/tests/junction/planar/planar_slots.cpp` `LineSlot`, `NetStep`, the T2 o1t3f1s0 test.
- `TrackComposer` on probe-g3e (working copies `heads`, `cpiece`, the doubled-piece cutting `a_j`, the deck walk).

## Step 1: dump (before any composer change)
In the hidden probe test below, print for T2 o1t3f1s0: per channel, per interval, its tracks (kind, line name, slot); every link (from, to, step); every map copy track (name, parent, side `sg`, whether its crossings are site crossings). Save to `<S>/g3e-slots.txt`.

## Step 2: the S-bends (`PLANAR_SLOTBEND=1` only; without it `TrackComposer` is unchanged)
`slots = BuildPlanarSlots(t, ch, res, paths.over)` (fail with `slots.why`). For each line l (channel c, intervals `slots.channels[c]`):
1. **Inside its channel:** for consecutive intervals i, i+1 both holding l's `Line` track, `D = s(i+1) - s(i)`; when interval i holds l's `Copy` track (slot `sc`) and i+1 holds none, `D -= sc - s(i)` (the bridge's own move, already the D351 parallelogram). `D != 0` is an S-bend of `D` at the boundary: into `x<k>`: just before l's first event that belongs to channel crossing k (`ch.channels[c].crossings[k]`, its `line_crossings`); out of `x<k>`: just after l's last such event.
2. **Across a site:** each link `(from, to, step)` with `step' = step - (sum of sg over the map copies of from whose crossings are site crossings)` (the join copies the slot layer does not model) `!= 0` is an S-bend of `step'` on `to`, just before its first event (at its start when it has none).
3. **Placing an S-bend of D at a position:** the piece holding l's next event after the position (the last piece when none): its heading h must be axis (else fail `"a slot step on a diagonal piece: " + name`); that piece (or the `a_j` part of a doubled piece holding the position; a position between one copy's crossings fails `"a slot step inside a doubled stretch: " + name`) is cut at the position into `h` (events before), `Turn(h, D > 0 ? +1 : -1)` exactly `2 * |D|`, `h` (events after); each `h` part least 2, the port, `STUB` and coverage minimums staying with the first / last part as before. Several S-bends on one piece cut it in event order.
4. G3b2's port steps (D359) are unchanged.

## Probe test (hidden)
In `src/tests/junction/planar/planar_components.cpp`, `"planar probe: track composer slot bends"`, tags `[junction][planarprobe][.]`: the step-1 dump, then with `PLANAR_COMPOSER=tracks` and `PLANAR_SLOTBEND=1` solve T2 o1t3f1s0 and print `slotbend T2 o1t3f1s0 area <a> size <WxH> track <t> wasted <w> empty <e> ms <ms>` or `... failed <why>`.

## Measure
1. The probe test into `<S>/g3e-probe.txt`. If it fails: rerun with `PLANAR_IIS=1` into `<S>/g3e-iis.txt` and report the rows grouped (strands, rows).
2. `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1 tools/junction/pv run build-rel T2 --out <S>/g3e-pv-T2.txt`, then T3: ok / failed by reason, best by area (area, size, track, wasted, empty, ms), largest ms; beside `<S>/g3a-after-T2.txt` and `-T3.txt` (MappedComposer).

## Do not
- Add any row or bound beyond step 2. Change `MappedComposer`, the slot layer, the track map, G2, or the composer's behaviour without `PLANAR_SLOTBEND`. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction track composer S-bends from the slot layer (PLANAR_SLOTBEND)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; the step-1 dump summarised for the B, CB, E, ED, EF and AB channels (slots per interval, links); the S-bends placed on T2 o1t3f1s0 (line, D, piece); the probe line or failure with IIS groups; the measure tables; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
