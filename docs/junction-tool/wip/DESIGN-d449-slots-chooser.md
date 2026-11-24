# DESIGN: B's slot layer as SSS v2's chooser (D449 step 1 of the three-approach comparison)

Status: orchestrator design, 2026-11-25, for the engineer's "Sure lets do those" (RECON-v1-v2.md, plan items 1-2). Probe behind a flag; nothing default.

## Why
- A and B were fast because the sideways decisions (lane order, gaps, which lines step where) were made before geometry by something small: B's slot layer (`BuildPlanarSlots`: per channel interval pre / x<k> / mid<k> / post, every track's integer slot; minimises steps; no binaries; a few ms on T2). SSS v2's drawing program makes those decisions inside one big program (S-bend inserts + per-pair keep-apart binaries): slow, and its choices are arbitrary (the stretched T2).
- D449 names B's slot layer as the precedent for the bend pre-program. Locking C's flex inserts from an S-bend set failed (t2-d458.md follow-up 7: the insert set alone does not fix the contraction). So the hand-over must be geometric relations, not insert lengths.

## The hand-over: channel-crossing offset rows
B decides, per channel and per channel crossing k (interval x<k>), every track's slot. C's drawing program gets, per channel X, per channel crossing k of X with channel Y, per two tracks a, b of X in B's x<k> interval (lines or copies, both present in C's track map):
- `STEP[(h + 2) % 8] . (P_b - P_a) == 2 (slot_b - slot_a)`, where h is the heading of a and b at that crossing (equal by Shape's spine rows, H14; if not equal, skip the pair and count it), and P_a, P_b are the crossing points of a and b with the FIRST line of Y they both cross (C's `cross_point` of those map crossings). Tag `slot <X> <k> <a> <b>`. Only consecutive tracks in B's order (n - 1 rows per interval).
- Nothing at pre/post: ports already sit at `side_gap` spacing (P8), the same rule B uses.
- Nothing about where the S-bends go: C's flex inserts stay free between crossings; the program sizes them to meet the offsets (stage A with flank rows, then contraction, then stage B).
- Keep-apart: pairs between two tracks of one channel whose walls lie between two consecutive channel crossings of that channel and that are adjacent in B's order keep their rows (B's offset fixes their side; a later option may fix it as a plain row).
Mapping B's tracks to C's: a B Line track is C's track with the same topology line index (C's lines come first at their topology index); a B Copy is C's copy over the same run (C's copies are made per run with full share, D331/D332; match by (line, crossing run)); B has no Row tracks, so prio rows get no slot rows (they follow their lane by the corridor family). Unmatched B tracks: counted, no rows.

## Probe
1. `PlanarDrawOptions::slot_offsets` (pointer to a list of (a, b, crossing a-y, crossing b-y, lanes) entries, null = off), filled by a test-side function from `BuildPlanarSlots` on the same variant's channels, sites and over choice (C's Structure variant carries `channels`, sites and the over bits; if C's over differs from what B needs, stop and report).
2. T2: B's slots on C's top variant; stage A (no keep-apart, flank rows, curves) with the slot rows; contraction; stage B; judge and domain; time each.
3. Compare: box, track length, ports' offsets, against C's P6 witness (68 x 50, length 1205) and B's own T2 (862.25, 94 x 29).
Expected risks: (i) B's slots assume B's channel paths; C's Shape headings may make an offset unreachable between two crossings (then stage A Infeasible names `slot` rows: a Shape/Slots mismatch, report); (ii) copies matched wrongly.

## Not in this step
Bundles as offset curves through the whole interval (D388 spine rows, `sibling_lanes`), the outline-area objective, terrain.
