# FX2 draft: Slots closes the pinned loops (2026-11-20, orchestrator; for the engineer)

Implements DESIGN-tracks §17.3 (Slots owns every sideways move; promise "positions agree around every loop a pin closes"). Evidence: wip/PROBE-FX1.md, wip/RECON-FX1-loops.md.

## What FX1 shows
Every failing loop is a **boundary face**: the region between two neighbouring pinned port tracks on one side of the box, bounded by the walk from one port track through the hub to the other.
- LL² o0t0f0s0: WO1 and WO2 (one port, pinned 2 apart) come from one split. The legs' diagonals (B1a, CBj1 SW; B1b, CBj2 NW) drift apart by at least 5 and nothing brings them back: 5 > 2.
- T2 o0t0f1s0: AB2 (in) and ED2 (out), pinned 4 apart on the E side: five one-signed diagonals at least 1 each, so at least 5 > 3 of slack.
- T3 o0t0f0s0: CD3 and AF3 on the S side, 4 apart: three diagonals use exactly the slack; one bound above 1 breaks it.
In each, Shape's headings force a one-signed sideways drift along the side; the pin requires less; only an opposite-signed move (a close-up S-bend) can close it. D359's port steps were those close-ups, by hand. Today's Slots cannot see the loop: every channel has its own free zero, so a link step across a site compares slots from two unrelated zeros.

## Proposal: loop rows per boundary face (generic, from the track map's faces)
- For each box side and each pair of neighbouring pinned port tracks on it, the face between them (the track map's faces, planar_regions) gives one loop: its boundary walk from one port track to the other.
- Along the side's own coordinate (y for E/W sides, x for N/S), each piece on the walk contributes dir × L, with dir in {-1, 0, +1} from its heading (Shape's output) and L ≥ its minimum (game: axis 2, diagonal 1, slot bend exactly 2|d|).
- Slots adds integer S-bends (its steps) on the walk's axis stretches and requires: residual = pin − Σ dir·L_min − Σ steps lies in the cone of the walk's free directions (both signs: any; one sign s: s·residual ≥ 0; none: residual = 0). Minimise Σ|step| as today.
- This is still a min-cost tension problem in the steps. It reads only Shape's headings and the map's faces (D366) and makes the promise at Slots' own output, testable there (D367).
- Interior faces (no pin) need only separation, which lengths give from below; they add no loop row.

## What it does not settle
- A piece borders two faces, so per-face rows are necessary, not sufficient: two boundary faces sharing a free piece could each want it longer in opposite ways. The probe measures whether that happens; if it does, the next step is one joint LP over the side coordinates (the review's L4 "one read of an LP relaxation"), still with no lazy loop.
- Where the S-bend sits on the walk (which axis stretch) is the cheapest by step cost; ties go to the stretch nearest the port (the corpus close-up). To be confirmed on pictures.

## Stage 2 probe (riskiest first, time-boxed, on probe-fx1)
Slots gets the loop rows; the track composer builds Slots' steps via PLANAR_SLOTBEND; R1 switches stay on. Measure base-infeasible counts on T2, T3, LL² against FX1's 32/32/7168; every remaining IIS is classified as (i) two boundary faces sharing a piece, (ii) an interior face, or (iii) something else. Pass: base infeasible falls to (i) cases only.
