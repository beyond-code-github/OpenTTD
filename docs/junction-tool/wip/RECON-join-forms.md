# Recon: mixed over/under forms inside one prio join

Question: does "one over/under form bit per join, all crossings inside a join the same form" hold? Read-only. OD = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad. Method for crossings: OD/psg/anatomy/<hub>/trace/paths_all.txt (one shortest path per in>out lane pair, HEAD markers). A path carrying a HEAD pair uses the structure (over on a bridge, under in a tunnel); any other path on a tile strictly between the heads is the opposite level. Only shortest paths are seen; deck heights are absent.

## PSG 227 (bridges plus 1 tunnel): MIXED, at the E port
Prio joins (ANATOMY-psg227-bbh.md lines 60-70, 105-108): six final joins, each with 3 sources (one fixed/main lane, two choice/joiner lanes gated by entry signals). VERIFIED. Out-lane pairs: N350/N353, E342/E343, SW362/SW363. Mains: SW360 to N350 and E343, E344 to N353 and SW363, N354 to E342 and SW362. Joiners: SW361, E345, N355.

**E port (E342 + E343 joins; boxes 377-379,338-339 and 375-378,342-343): joiners cross mains in both forms.** VERIFIED from paths_all.txt (tiles listed there):
- Tunnel 371,338 to 375,338 carries joiner SW361>E342 (joiner UNDER). Surface above (373-374,338) are main N354>E342 and main SW360>E343 (main OVER).
- Bridge 372,334 to 372,339 carries joiner N355>E343 (joiner OVER). Under it, tiles 372,335-338, are main SW360>E343 (372,337-338), main N354>E342 (372,336-337) and its own sibling N355>E342.
- So SW361 goes under the mains N354 and SW360, and N355 goes over the same two mains, 1-2 tiles apart in the same crossing zone, feeding adjacent out lanes of one port. The bridge span also covers the tunnel tile 372,338 (bridge over tunnel; ORACLE-psg227.md line 47).
- Caveat: the crossings sit 3-6 tiles upstream of the join boxes, after the joiners' line-choice split (370,331 for N355; 368,337 for SW361) and before the merge nodes. INFERRED that this counts as "inside the join's coverage"; it is between the joiner's split and its merge node.

**N port (N350/N353): no joiner-over-main or joiner-under-main crossing found.** Bridges 358,336; 359,335; 360-362,334 carry joiners (SW361, E345) and main E344>N353 (362,334-338) over E-bound lanes (SW360>E343, SW361>E342/E343). Those under-lanes are not N mains or joiners. Bridge 359,330-333 carries SW361>N353 over E345>N350, joiner over joiner. VERIFIED.

**SW port (SW362/SW363): one form only.** Doubled bridge 375,346 / 376,345 carries joiner E345 (to SW362, SW363) over main E344>SW363 (surface). Joiner over main. Bridge 369,348-351 carries N355>SW363 over E345>SW362 (joiner over joiner). Mains N354>SW362 (bridges 366,333; 366,340) goes over N- and E-bound lanes, none of its own joiners. No main-over-joiner or joiner-under-main inside this port. VERIFIED, with the same caveat: the 375-379,345-346 bridge is before the E345 direction split (372-373,345-346), so it is only arguably inside the join.

Verdict for 227: the one-bit rule FAILS at the E port; N and SW ports are single-form. REPRESENT-psg227.md line 135 also flags per-bridge over/under as undecidable from shortest paths.

## PSG 197 (tunnels only): no mixed form is possible, and few crossings sit inside prio joins
- Every crossing is a tunnel (50 tunnels, 0 bridges in the hub): ANATOMY-psg197-bbh.md "(d)", tunnels2.txt. Forms can only differ as which line is under.
- Joins are all binary and cascaded (33 joins, 2 inputs). Prio-like gates: 6 entry inputs, 1 combo (110,172u into the cross 109-110,170-171), plus path/path pairs in the retrofit clusters (joins.txt). VERIFIED. Entry-gated joiners: E173 into 98-101,171; 107-109,172-173; 100-108,174; 110-111,176; E169/E173 into 86,151-156; N90 into 94-96,193-195.
- "3 -> 2 merger (note the prios)" (ANATOMY lines 52, "(c)"): E173 (entry-gated) merges with E169 at 107-109,172-173 and with E174 at 110-111,176; three lanes become two. VERIFIED from joins.txt. Tunnels do pass under these boxes (E173>N pair (111,171/172) to (107,171/172); E174>N pair (107,174/175) to (103,174/175)), but they carry the N-bound lanes, not the joiner or the main of the S-bound join. So no joiner-main crossing inside these joins. VERIFIED by the box-overlap script and paths_all.txt.
- Same-out-lane crossings in the trace: S82>N84 under S81>N84; S82>N85, S83>N85, E174>N85 each under S81>N85; N91>S89 under E169/E173>S89. Surface lane is the same in each pair; those joins are block/block (no prio gate). VERIFIED (44 of 50 tunnels parsed).
- Verdict for 197: data cannot answer the mixed-form question for prio joins. There are no bridges, and no crossing between an entry-gated joiner and its main inside a join was found. Where tunnels meet a join they belong to other flows. Not contradicted, not confirmed.

## Junctionary mirror (OD/junctionary-mirror/pages), quick grep
- Line Merge.mediawiki lines 12, 19, 27: crossover replaced by a tunnel; an alternative variant tunnels the mainlines. These are whole-design alternatives, not mixed within a join. INFERRED.
- Tetrathorp-Junction.mediawiki lines 16-39: bridge, tunnel and mixed variants ("tunnels and bridges", line 34-39; bridges replaced by short tunnels under later). Hybrid by build stage, not a prio join with mixed forms. INFERRED.
- No page describes a prio join with both forms inside.

## Where the data cannot answer
- "Inside the join" is a judgement (227 E-port crossings are upstream of the merge boxes).
- 197 has no bridges, so mixed-by-structure-type cannot occur there; the question reduces to which line is under, and no prio-join instance is available.
- Only one hub (227, E port) gives a verified mixed case; it is one port of six.
