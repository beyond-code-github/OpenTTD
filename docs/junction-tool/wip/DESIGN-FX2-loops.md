# FX2 draft: stage 2 of the frame experiment, inside the SSS contracts (2026-11-20, orchestrator)

Implements DESIGN-tracks §17.3 and the layer table in src/junction/planar/CLAUDE.md "Architecture". Evidence: wip/PROBE-FX1.md, wip/RECON-FX1-loops.md.

## What FX1 shows, read against the contracts
Every failing loop runs between two neighbouring pinned port tracks on one box side (T2 AB2/ED2, T3 CD3/AF3, LL² WO1/WO2). Its sideways drift comes from node-leg diagonals (and the diagonals after them) that the probe left as free lengths, one-signed, so the pin cannot be met.
- Shape "makes no sideways moves": the node leg's 45-degree heading is Shape's; how far the leg moves sideways is not.
- Slots "decides every track's sideways position in every stretch, hence every S-bend ... and every close-up at a pinned port": at a lane split or merge the leg's offset is a slot change, sized by Slots.
- Lengths: "every exact length (... S-bend sizes) is an earlier layer's promise": a diagonal that moves a track sideways is never free in Lengths.
So FX1's setup breached Shape's contract (sideways moves left to free diagonal lengths). Withdrawn: an earlier draft of this file proposed per-face loop rows in Slots over Lengths' minimum lengths; that had Slots model Lengths' freedom to patch the breach.

## Stage 2 (the review's frames)
- **Slots:** a node-group frame per lane split or merge (the in line and its legs share one zero); a port-side frame per box side (its port tracks share one zero, the D313 pins as rows). A leg's offset at a node and every close-up at a port are exact slot steps in these frames, so loops inside a frame close by construction.
- **Shape:** unchanged: the node leg's heading only.
- **Lengths:** builds each slot step as an S-bend with its diagonal exactly 2|d|. The only free diagonals left are those of net turning, which lie between frames, where gaps are free from below (review §5 L3).

## Open before a brief
- Which existing node-leg diagonals are slot steps (lane split/merge: legs reach the same port or the same channel) and which are net turning (a fray's leg toward another port); the topology's site kinds should give it (Terms: fray vs lane split, join vs lane merge). To be checked against the code, not assumed.
- How the frames join the existing per-channel frames: one shared zero per frame and channel intervals as members, with steps between members charged as today.
