# Research: coop bridge length rule (2026-11-18, read-only)
Corpus root: OneDrive scratchpad/coop-site/site/ (CS).

## Facts
1. THE RULE (CS/Automatic_Signalling.html, "How many alternate routes", ~l.170-190; same text in Signal_gap.html):
   SignalGapLength = (TL + 2) * LineCount - (TL - 2); LineCount = ceil((Gap + TL - 2) / (TL + 2)).
   TL = shortest train that meets the gap; LineCount = parallel routes incl. the original. Bots: Webster `@gap`, PublicServer `!gap`.
   "Gap" = unsignalled tiles between two signals (l.162); bridges, tunnels, stations make gaps >1 (l.157); the alternate route must be a parallel line kept in line sync until the gap is restored to 1. Page text: "Two bridges for this gap is enough for any train length" (image caption).
   Coop mainline: gap 1, i.e. signals every 2 tiles (SD 2; "Make sure drag density is two", Signal_gap.html).
2. Table, SD 2 (gap = bridge head-to-head tiles, signals on the tiles just outside, D82): N=1 -> 4; N=2 -> TL+6; N=3 -> 2TL+10. Worked TL4: 4 / 10 / 16. TL1: 4/7/12... (N=2 -> TL+6 is the "doubled" limit; N=3 -> 2TL+10 is the "tripled" limit.)
3. Evil_mode.html l.19: long bridges "1.5x the train length or longer" contribute to destructive interference of doubled bridges (with merges, high holding capacity). Basic_Junctions.html l.13: bridges are doubled only where traffic shows slowdowns, "unless it is a mainline or the bridges are longer than the distance between trains".
4. Breakup vs tripling: corpus is SILENT. Only mentions: "double bridges need tripling" (RECON-hub-bridges.md l.31,38), D103/D208/D228a (engineer: triple, or break up by one signalled tile, "cost decides"). No coop page found stating a rule for choosing.
5. Our D228a (B <= 2d + tl, doubled) is the same idea, derived independently.

## Inferences (mark as such)
- Per-SD generalisation of the wiki formula (inference; wiki only gives SD 2): headway on a block of gap G is ~ G + TL - c, mainline headway TL + SD. At SD 2, c = 2. Hypothesis: N >= (G + TL - SD) / (TL + SD). Unverified for SD != 2; D97's default SD 4 (gap 3) is not covered by the wiki. Rig test needed (D208 queued).
- D228a vs wiki at SD 2: D228a doubled limit B <= TL+2 is 4 tiles stricter than the wiki's TL+6 (N(d+tl) >= B+tl vs N(TL+2) >= G+TL-2, d = SD). The wiki figure counts 'gap' which may differ from B by the head tiles; check counting on the rig before using.
- Breakup: a landing tile with a signal splits span B into two blocks of about (B-1)/2; each must satisfy the formula for its own N. Per-N max span: break into k blocks with k-1 landing tiles gives total up to k*Gmax(N) + (k-1).
- Choice (inference): tripling costs one more lane of width (D333) and a third bridge; breakup costs one landing tile where terrain and the crossed lanes' heights allow it, and a bridge-head pair per span. Prefer breakup when a landing tile exists in the crossed set (cheap, no width); triple when no tile in the span can host a landing (crossed lanes dense) or one break still leaves a half above the doubled limit.

## Solver rule (proposal)
For each crossing bridge with head-to-head span B on lane with signal spacing SD and design TL:
 1. N_needed = ceil((B + TL - 2) / (TL + 2)) (SD 2; use hypothesis above for other SD).
 2. N=1: single (only for quiet/choice legs, D299/D32); N=2: double (D80); N=3: either 3 copies or split B at a landing into spans each needing N<=2; N>3: split.
 3. Pick by cost: width reserved (D333), landing tiles, bridge cost; both remedies stay legal.
Open for the engineer: confirm the formula counts B or B+2; the SD != 2 form; breakup-vs-triple preference.
