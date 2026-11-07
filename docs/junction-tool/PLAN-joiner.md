# Endpoint joiner: plan sketch (draft, needs the engineer)

**Rests on:** D87 (route search), D93 (terrain), and `RECON-joiner.md`.

## Goal

The player tags two connection points. The tool routes and builds a coop-quality line between them over real terrain, and later puts a junction where a new line meets an existing ML.

## What already carries over

- **`FindJunctionRoute`:** start and goal sets (tile plus heading, many-to-many, as in the AI library), the curve grammar, bridges, terrain cost and fences.
- **`PlaceJunctionSignals`:** switch isolation, bridge signals, bays and fill. Prios only at ML merges.
- **The rule checker:** connectivity, curves, bridges and signals generalise. The ports are tagged endpoints instead of the four T legs.
- **The terrain snapshot, the worker thread and hover caching.**

## The hard part: multi-track lines

An L_R line is two tracks that should stay parallel at their gap through curves, as coop lines do.

| Option | Description | Assessment |
|---|---|---|
| A | Route one track, then derive the other as a parallel offset of it (shift each piece perpendicular by gap+1, fixing up the diagonal runs at the corners) | cheap, but corners need care, and terrain or obstacles may block the offset |
| B | A "line router" whose state is the whole line: the moves turn all tracks together, so the footprint is the union | correct by construction; a larger state and more pieces per move |
| C | Route each direction independently with the same costs | simplest, but tracks drift apart; not coop-like |

**Recommendation:** B, with A as a fast first try. Settle this with the engineer.

## Proposed first cut (J1–J4)

| Step | What |
|---|---|
| J1 | A UI tool: click an existing own track end (the tile and its heading are snapped); click a second end; the line notation comes from the Junction window. A ghost preview and the cost; build on confirm. Single-track lines first (`L` / one direction) to prove the UI and the build path. |
| J2 | L_R lines (option A or B) with the signal pass (2-tile signalling, no prios), plus checker connectivity for 2-port lines. |
| J3 | Joining an existing ML mid-line: the endpoint is on a line, not at a track end, so the tool places the 1b T generator at the junction point and routes the branch from its S leg. Needs a coop rule for the distance between junctions (none documented; ours: at least one train of signalled straight track). |
| J4 | Rig check: connect two stations across a hilly map, then run trains. |

## Questions for the engineer

1. Which multi-track approach (A, B or C)?
2. Endpoint picking: snap to existing track ends only, or also free tiles with a heading drag?
3. Should J3 (joining an ML mid-line with a generated T) be in the first milestone?
