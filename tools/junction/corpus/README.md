# Junction corpus extractor (X-B1)

Turns a hub in a whole-map save export into the files the junction solver loads. Python only; run everything as `python3 -I`.

- `hubs.tsv`: one row per hub: id, game (PSG number), name, author, style, source, box (x0,y0,x1,y1), lanes (the hub's lanes with directions as `N84:out N90:in ...`; blank = derived from each lane's first signal walking inward, else the component vote), status (`pinned`, `box unconfirmed`). A box and a lane list are checkpoints: Pete confirms each box once.
- `load.py`: the loader (the probes' trace code, copied: whole-map export JSON -> tiles -> piece graph, candidate lanes of a box). Corpus root: `--root`, else env `JT_CORPUS`, else the OneDrive scratchpad `.../scratchpad/psg` (it holds `maps/PSG_<game>/*.json`).
- `extract.py <hub id> --out <dir>`: stages as functions, `graph` (X2 directions by routes), `ports` (X3), `lines` (X4 route sets, switch nodes, lines), `crossings` (X5), `structure` (X6), `shape` (X7), then the writers. Writes into `<dir>`:
  - `topology.txt`: planar notation (src/junction/planar/CLAUDE.md "Topology", "Authored crossings"): `port`, `node`, `cross` with marks. Lines are `L1`.. (lines with track by first piece, then empty lines); routes are a movement letter plus the in lane's number on its port (rules in the file's header comments).
  - `structure.txt`: `tunnel`, `group`, `straight` (as tools/junction/topologies/psg197.structure.txt); undetermined straight legs are comments; bridge spans are not written (no bridge statement yet).
  - `shape.txt`, `steps.txt`: Shape's projection and V's steps (D396, as psg197.shape.txt / steps.txt).
  - `lengths.txt`: per line `lengths <line>: <len0> <len1> ...`, one length per piece of its raw heading list (before projection), in half tiles (axis tile 2, diagonal piece 1, tunnel or bridge 2 per tile); a heading appended to meet a port has length 0.
  - `report.txt`: counts, every checkpoint (ambiguous pieces, lines without geometry, undetermined straights, level and same-channel crossings, tied leg orders) and the gaps (D393).
- `compare.py psg197 <dir>`: golden check of the extraction against tools/junction/topologies/psg197*.txt (hand names mapped to lines by tiles, as the probes did); prints the probes' numbers beside ours and every difference class; writes `<dir>/compare.txt`.
- `out/<id>/`: the committed outputs for psg197 and psg186.

Run: `python3 -I tools/junction/corpus/extract.py psg197 --out tools/junction/corpus/out/psg197` then `python3 -I tools/junction/corpus/compare.py psg197 tools/junction/corpus/out/psg197` (about 2 s each).
