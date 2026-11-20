# BRIEF S3: where coop hubs put their sideways steps (corpus recon, Sonnet)

You execute this brief and nothing else. Read-only on the game and the repo: you write scripts and output only under the study folder named below, and one report file. If something is not covered, or you would have to choose between approaches, stop and report the question. Keep your context small: send script output to files; read summaries, not raw dumps.

## Why
Implements wip/SPEC-slots.md §2 (Slots decides every sideways step: fans at ports, close-ups after joins, cusps, landing steps, steps under spans, D313, D375). A probe (X2f) gives the mathematically fewest steps; this study gives where coop players actually step, so Slots' rule matches how hubs are built. Prior corpus work: docs/junction-tool/RECON-corpus-crossings.md (study S1: method, hub boxes, caveats) and RECON-corpus-sbend-crossings.md (S1b: S-bends under bridges only). This study covers every sideways step in a hub.

## Inputs (already on disk; fetch nothing)
- S = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg
- S/census2/census2.py (`load(save)` gives tiles; `TEDGES`, `D`, `OPP`) and S/census2/census2.tsv (hub boxes, types). Use the same hub set and grouping as S/study1/s1.py (read its first 60 lines: groups 1 classified, 2 unclassified, mesh-large kept apart as context only).
- S/study1/s1.py and S/study1b/s1b.py: reuse their tile and track walking.

## Definitions
- A **step**: a rail track walked tile by tile (follow single track pieces through plain rail tiles, signals allowed; stop at switches, bridge/tunnel heads, the box edge + 3) that runs on axis heading h, then diagonal pieces only (2 or more half pieces), then axis heading h again (same h): a sideways move. Its **size** in lanes = the sideways offset in tiles. Also count the diagonal form: diagonal heading d, an axis run, diagonal d again.
- **Context** of a step, the first that applies, checked in this order, within 3 tiles of the step's diagonal part along the track:
  1. `under-span`: the diagonal lies under a bridge span (S1b covers these; count them only).
  2. `landing`: between two bridge or tunnel heads of the same axis in series on this track (S1's series walk).
  3. `port-fan`: within 6 tiles of the hub box edge (+3 margin), before the track's first switch going inwards.
  4. `after-merge` / `before-split`: a merge switch (two tracks joining in travel order is unknown without signals; use geometry: the switch tile has 3 track ends; call it `at-switch` and record the distance) within 6 tiles.
  5. `mid-run`: none of the above.
- **Bundle step**: the parallel neighbour track(s) (same axis, adjacent row or column, overlapping run) also step at the same place with the same size; else **single**.

## Output
- Scripts and raw output in S/study3/ (s3.py, sum3.py, out.pkl).
- Report: /home/pete/OpenTTD/docs/junction-tool/RECON-corpus-steps.md, in the form of RECON-corpus-crossings.md: method and caveats; totals (hubs, steps); table by context (count, %); size distribution (1, 2, 3, 4+ lanes) per context; bundle vs single per context; for `at-switch`, the distance histogram; for `port-fan`, steps per port side; three example hubs per context (save, hub id, tile) so they can be viewed later.
- No interpretation beyond the numbers and one short "what this suggests for Slots" paragraph clearly marked as the agent's reading.

## Do not
- Load or run the game, touch the repo's code, or fetch anything from the network.
- Use more than 4 CPU processes (another probe is running on this machine).

## Stop conditions
- census2.py or the saves do not load: report the error and stop.
- The walk cannot tell a step from a curve (e.g. axis h, diagonal, axis h' != h): those are curves, not steps; count them separately as `turns` and do not stop.

## Report (short)
- Report file path; the totals and the context table; anything not done and why. When you have reported, stop.
