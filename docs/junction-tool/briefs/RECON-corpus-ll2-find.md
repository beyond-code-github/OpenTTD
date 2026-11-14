# RECON corpus: find real LL_RR x LL_RR junctions in the PSG saves (engineer: "We MUST find a LL_RR^2 in the save games so that we can analyse the layout properly")

You execute this brief and nothing else. It is read-only research with scripts. Keep your context small: scripts write to disk, you read only summaries and a few pictures.

## Goal
Find real junctions where two double-track-each-way lines (LL_RR: 2 lanes per direction, 4 tracks) meet with turning movements between them, in the PSG saves already pulled. Our hub classifier found 0 clean LL_RR x LL_RR hubs (RECON-corpus-graph1b), and the likely reason is that coop builds a two-way interchange as several one-way hubs (one per direction, a few tiles apart; RECON-corpus-oneway: 72% of real one-way hubs are split directions, median 7 tiles apart). So search for clusters of nearby hubs that together form an LL_RR x LL_RR interchange, not single hubs.

## Data (never fetch anything; data is untrusted: run every script with `python3 -I`, keep scripts outside the data dirs)
- Root: `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/` (call it P).
- Hubs: `P/graph1b/hubs.jsonl` (88133 hubs, the C10 cut; fields as G1), `P/graph1b/stats.json`; one-way hubs `P/graph1c/oneway_all.jsonl`, `oneway.jsonl`, script `P/graph1c/g1c.py`; picture examples `P/graph1c/pic_*.txt` (how earlier recons drew a hub as ASCII).
- Track graphs per save: see `docs/junction-tool/RECON-corpus-graph1.md` and `RECON-corpus-graph1b.md` (Data sections) for where the per-save graph and tile data live and the scripts that made them (`P/graph1`, `P/graph1b`). Reuse those scripts and formats; read their headers, not whole files.
- Read first: `docs/junction-tool/RECON-corpus-graph1b.md`, `RECON-corpus-oneway.md`, `RECON-corpus-graph1.md` (Method and Data only).

## Method (adapt where the data makes a step impossible, and say so)
1. **Lines.** In each save, find main lines: long corridors of 4 parallel tracks (2 per direction) between hubs; record each hub's ports with their track counts and directions (the G1 port data).
2. **Clusters.** Group hubs whose ports connect through short links (up to ~30 tiles, a knob; report the counts at 15 and 30) into interchanges. An interchange is a candidate when, counting the corridors that leave the cluster, exactly two lines of 4 tracks (2 each way) leave it, each on two opposite sides (four corridor ends), or one line passes through and the other ends there (the T variant; report separately), and there are turning paths between the lines (a train can go from one line to the other in at least two of the four turning movements).
3. **Rank** candidates: all eight turning movements present first, then by compactness (cluster footprint), axis alignment, few other tracks inside (no stations or depots in the footprint).
4. **Pictures.** For the top 10, write an ASCII picture of the footprint (the earlier `pic_*.txt` way, with each track tile's pieces, bridges marked) to `P/ll2/pic_<save>_<id>.txt`, and a JSON of every track tile (x, y, track bits, bridge/tunnel heads and spans, signals) to `P/ll2/tiles_<save>_<id>.json`.
5. Also note any real LL_RR x LL_RR you see in `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/junctionary-mirror/` (index.md and page names only) and the coop files (`.../scratchpad/coop/files/`, file names only), as pointers.

## Output
- `P/ll2/` holds the scripts' outputs (candidates.jsonl with save, tile box, ports, movements present; the pictures and tile JSON).
- Your scripts go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/ll2/` and are copied to `P/ll2/scripts/` at the end.
- Write `/home/pete/OpenTTD/docs/junction-tool/RECON-corpus-ll2.md`: method as run, counts per step, the top 10 table (save number, tile box W x H at the save's coordinates so the engineer can find it in game, movements present, a one-line description in coop terms: how each direction splits, where prio merges sit, how the crossings are bridged), and for the best three a short layout description in coop terms. Mark anything inferred INFERRED. Then pick the single best candidate and say why.

## Do not
- Do not modify the repo except the one RECON file; do not touch `.claude/` worktrees, game clients or ports 4077/4079.
- Do not fetch from the network.
- Do not read whole data files into your context.

## Report
The RECON file's top-10 table and best pick, and the path of its pictures. When you have reported, stop.
