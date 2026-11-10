# RECON: LL_RR main line, T junction (L_R branch, and LL_RR branch)
Sources: docs/junction-tool/{RULES-hubs,RULES-coop,DECISIONS,IMAGE-INDEX}.md. Quotes are exact short phrases. No source code read.
S (image root) = the session scratchpad of 2026-11-11 (abr/, coop/ image mirrors)

## 0. Notation and scope
- D38: "LL_RR is two lanes directly adjacent either side of a one tile gap." Gap is "a guideline rather than a mandatory distance". R41: "Taken literally, it is only one tile, but usually more tiles are used". R42: leave spaces between directions, "the more the better". Agrees with D22.
- D142: in LL_RR "the tracks of one direction are adjacent (gap 0, the doubled track of OpenTTD: S = 1 on diagonals); the underscores set the only gap". `JunctionLineSpec` carries per-neighbour gaps.
- D60: ML and branch take separate notation strings, "e.g. an LL_RR mainline with an L_R branch".
- D72: multi-lane "follow-on" after 1b; 1b shows "not supported yet". D126 extends: "L_R up to LLLL_RRRR and beyond", lane counts "per direction and per arm", independent per arm (PSG197 LLLRRR/LLRR). Evaluator works "per lane and per block"; solver variables "indexed by lane".
- R44: "Sidelines (L_R or LL_RR) reach the mainline only through a sideline hub." So an LL_RR branch is a legitimate SLH sideline. H36: "SLH branch is usually L_R or LL_RR".
- H2: SLH shape depends on ML shape (straight/diagonal/corner, gap between ML lines).
- Coop sources are written for LL_RR and wider (RULES-coop preamble); R46 and R16 were marked n/a for L_R only.

## 1. What a train must be able to do
Line choice at a join (merge):
- H1 (JN-SLH): "the side line giving priority to the main line and the side line has to be able to join any of the ML rails".
- H12: "every sideline lane must reach every ML lane" with prios; old SLHs without that "are obsolete" (status **check**).
- R58/H23: balanced = "each entering lane can choose any leaving lane"; efficient = "Trains should not have to slow down"; sized for the longest train.
- R16: a "joiner" (one direction gets priority) is SLH style; "Choices and all-to-all balancing apply only where a train has several outputs". For an L_R branch onto LL_RR ML the branch train has 2 outputs per merge, so it is a choice, not a pure joiner.
- R61/H24: SLH merge = "one line joins all others with full priority to the joined lines". ABR06 styles: not-balancing; fully prioritised 1 to N (SLH); multi-to-multi ("practically doable only with 2line join"); fully prioritised joining some lines ("doesn't balance completely"); half prioritised; all-to-all.
- H13 (ABR06, image hubs_1to2.png): "for any amount of ML lines, just multiply/expand the design - also, when expanding you should toy with the waiting bays so it performs best". L_R branch onto LL_RR = this 1-to-2 case.
- H14 fast joiner: SLH with exactly "1xTL waiting bays which allow to have full speed choices" (PSG204).
- R12: "join at only one (possibly prioritized) spot" per output line. R13: avoid a chain of prioritised joins onto the same line (downstream joiners starve).
- R14: no PBS merges. R36: where trains choose, 1-way pre-signals are best; PBS into 1-way bad. R37: "absolutely unacceptable to use block signals which lead to the same place without waiting bays" (R37 was "implied, none in L_R T"; now applies).
- For an LL_RR branch: each branch lane must reach every ML lane (H12), and per D126(2) "every joiner can reach every lane where required". Branch-to-ML is then an N->M balance with N=M=2 (H30/H36 "N->M"). Multi-to-multi is the "2line join" case ABR06 calls practical. No SLH-specific LL_RR rule in sources beyond H12/H36.
Which lanes a split may take from:
- R11/H23: "Exits (splits) need no balancing... orders do the choosing". ABR06: "just use anything to get those lines away".
- D126(1) "line choice": "which lane a joining train enters, and which lanes a splitting train can leave from". D176: on LL_RR "each train has a choice of line on the ML".
- ABR06 splits (H28 area): "you can play with the actual order - if you split outer line first or outer, or both lines inside".
- R8/R9/Splits-before-merges: split first, merge after ("make the merges after the exits"); C3BM "Split before merge. Deadlock free."
- R45: split-then-merge gives left-hand-driving crossing, "Cross the lines later".
- R46: "treat all tracks of the same direction equally".
- R33: where an ML section is split in parallel paths keep "line sync" (equal path length). D100/D105: equal pathfinder cost is the balancing rule ("the symmetry is the thing that balances the routes for the pathfinder").
T-specific (BBH vs SLH): H35: a BBH T needs ML-to-ML turns; an SLH T has only branch movements. For our T (SLH) per ML direction: branch-to-ML-lane and ML-lane-to-branch (H35 "SLH has 2 branch movements per ML direction"). D126 target (3): T = 6 directed movements (R10, H35).
Balancing requirements summary: D126(2); H12; H23 "Exits need no balancing. Joins ... hardest"; R58. Hub decomposition R10/H20: 1 crossing, 1 crossover, 3 exits, 3 joins is BBH language; for SLH T the crossing/crossover are not in H1-H2 (see Q1).

## 2. Shifters
- GL/SML via H9, H19: Shift Mainlines (SML). "Trains should have shifting opportunities prior to every SLH". "Shifters sit before every SLH." With SML "ML lanes become imbalanced (inner busier)... to be taken into consideration during BBH and Mainline Station construction". SML merging: "Merging trains will attempt to enter lane C" (outermost) (RULES-coop gap 8). SML hubs "never very popular because they are just huge" (H9).
- H32 (ABR06 "Shifters and hubs", MSH 01): "trains from A have one track prioritized and one giving way (B)". Entering the balancer from the give-way side is "uncomfortable", so trains from the give-way line (C) "are shifted onto the already-joined line, then ignored in further balancing". Useful for badly accelerating trains. "Used for an LL_RR hub, could be used even in larger scales". "Shifters are placed inside the hub to add no space." Status **design / gap**, optional simplification.
- D126(3): shifters = "moving trains between lanes before a split or after a merge so later balancing can ignore lanes".
- When required: nothing says required for LL_RR. SML source says before every SLH only for shift mainlines (a different ML type). MSH 01 shifter is optional ("could be used"). H38: MSH "may reduce [balancing] by shifting".
- Images: S/abr/images/hubs_msh01shift.png (A, B, C line groups, label MSH 01, ML tracks into tunnel portals). Not a pure T; see section 7.

## 3. Waiting bays and signals for multi-lane
- D71: "when more than one line in each direction is in play we must allow trains to choose between tracks, and at this point waiting bays become a must. Deadlocks are a bit no-no." Hard rules: (a) every connection leaving the ML holds a whole train (TL) between the ML split's after-signal and the first place it can be held (its merge signal); (b) no layout may deadlock. D72: "Multi-lane lines need waiting bays".
- R15: "waiting bays are necessary everywhere". D126(4): waiting bays "wherever trains choose between tracks".
- D83: whole-train bays; signals at k x TL back from merge signal; "No bay boundary may fall on a switch"; train may stand on a bridge but boundary signal cannot sit on a bridge tile; queue keeps full SD signalling to the merge signal ("Bays add signals; they never replace SD spacing"). D88 supersedes position: boundary k = first legal piece at least B = 2*TL units back from boundary k-1, legal for a signal and not a switch; "At least one bay is still required."
- R32/D21: ML signal distance SD 2. R35: dense signalling wanted. H25: merger signals need "sensitive gaps" for max throughput. R14, R36 above.
- D176 (multi-line): "on each leg after a line-choice split a train may only stop where it fits whole: the leg from the split to the merge signal is whole bays (exact multiples of TL), or the leftover carries no signal so the next train waits before the split and can still choose another line." Reason quoted: "one full set of waiting bays spills over into the next line choice going in that same direction". "Enforced in the map layer as whole-bay edge lengths in tile units when LL_RR comes; the checker gains the rule then."
- L_R rule for contrast (D176): bays at least a train each; "a train waiting in a leftover stretch over the split is tolerated at a given level of throughput"; "for L_R the waiting bays don't matter as much". D176 also: "If a queue is exactly full for S-bound, the trains should still be able to go north. Full plus 1 then yes this will block both lanes."
- D113: "between a split and the next bridge head there is at least one signal-legal piece carrying a signal facing along the path; the same applies to a merge after a bridge's far head". Stated reason: a queued tail on a bridge shares the split's block.
- D113 vs D176 conflict note (D176 text): "D113 vs this on multi-line: to settle with LL_RR." Tension: D113 demands a signal between split and bridge head (so a signal exists in the leftover); D176 says on multi-line the leftover after the split must carry no signal unless it is a whole bay. Not resolved anywhere (Q4).
- D117: one signal may do several jobs, no back-to-back signals. D82: nearest legal piece before/after each bridge carries a signal facing along the path.
- D130: a long waiting bay "becomes indistinguishable from a brand new lane"; added track must pay for itself in throughput.
- H13/H14: bay length tuned per expansion; fast joiner = exactly 1xTL.

## 4. Prios on multi-lane merges
- R18/D27: "Mainline merges only"; ML highest priority, joiners yield. D159: ML merges always have a prio; "A branch merge has one when the throughput estimate ... says the side with priority misses its target without one".
- R19: "A prio is a check". R20: prio length depends on merger type: SLH joiner longer; "a 'line is 100% full' check in a BBH merger needs a very short one". R21-R27 length rules (R22 two-signal reach; R23 extra track; R25 segments no longer than shortest TL; R26 extension past merge point; R27 acceleration run).
- D28: prio "Outside" (far side from other direction). RULES-coop gap 8: D28 is the project's own call, no source states it. R30/BBH: prios "can be hacked in any place", late pass.
- H12: SLH needs prios on the merge; every sideline lane must reach every ML lane.
- H24/H26/H37: multi-lane merges: "fully prioritised joining all lines (SLH, 1 to N)"; multi-to-multi (2-line) ; "half prioritised" (two equal lines; prio gives way "because it could make trains be stuck in them for too long"); all-to-all "later joiners can struggle with prios". H26: policy per merge, ML-protecting/equal/none; D27 covers only ML-protecting. PF traps behind a merger "improve balance but are not needed". Image hubs_173Mark2.png: "a prio that would be normally considered broken, because only one of these tunnels is prioritized".
- R12/R13: one join point per output line, no chained prios. R13 matters for multi-lane: a 1-to-2 fan has two output lines, each with one join.
- R51/R31/D85: "No prio ever joins a bridge block of a doubled section." Prio on a bridge cannot use an entry check.
- R28 high-speed merge needs a two-way entry signal and alternate track; conflicts with pure one-way block-signal layout (coop gap 3).

## 5. Bridges and tunnels on multi-lane main lines
- D80: "Every ML bridge is doubled: split before it, two parallel bridges with equal path lengths (line sync, R33), merge after it, all block-signalled (D19). The bridge merge is a plain merge with no prio."
- D174: "Main-line bridges are always doubled (D80), whatever they cross". SL bridge single at basic throughput, doubling is a throughput decision (D159). "Tunnels follow the same rule: main-line tunnels always doubled, SL tunnels single at basic throughput."
- D19: "each track that crosses the mainline splits into two parallel tracks... then merges back". R48: doubled bridges and tunnels are standard. R49: bridges add throughput only if enough in number. R56: use "two or more" tunnels. R52: doubling gives evil mode risk and more waiting space.
- Nothing states per-lane doubling on an LL_RR ML. D80/D174 are written per "ML bridge" and per "track" on L_R (one lane per direction). Whether "doubled" means two bridges per lane (4 per direction pair) or the LL_RR's two lanes already count as the doubling is not stated (Q5).
- D126 consequence: evaluator per lane and per block. D118 stagger and D105/D100 doubled-lane geometry (symmetric Y, parallelogram) are described for single lane per direction; "more complex hubs will often need to shift each doubled lane around" (D105).
- R47: bridges at least 4 tiles. R38: no signals on bridge/tunnel tiles. R53: equal length pair. R54/R57 bridge the straight line.
- Evil mode: R50-R51: "Not bridging the prioritized line overcomes this problem entirely"; D23 conflicts (coop gap 1). With LL_RR, 2 lane ML bridged and doubled vs ABR06 H27: "doubled therefore have half value".
- H15/H16 invisible hubs: "unfriendly to larger line counts"; tunnel count rises as TL falls.
- ABR06 H28: "Directional Tunnels": the straight line stays bridged.

## 6. Images of multi-lane hubs
(all under S/abr/images unless noted; none shows an LL_RR SLH T specifically; sources are text-light)
- hubs_1to2.png: single line fanning to two ML lines via chain of diagonal crossovers, two yellow arrows. Closest to L_R branch onto LL_RR.
- hubs_msh01merge.png: 2-line to 2-line merge with signal gaps (closest to LL_RR branch onto LL_RR ML).
- hubs_msh01shift.png: shifters (A, B, C groups, label MSH 01).
- hubs_A2A.png: all-to-all merge fan of diagonal crossovers, PF traps behind it.
- hubs_173Mark1.png / hubs_173Mark2.png: Mark all-to-all with a single tunnel prioritised.
- hubs_186msh03.png, hubs_msh03.png: MSH 03 mergers only, stacked tunnels/bridges.
- hubs_bbh04.png, hubs_bbh02.png, hubs_slh01/04/06.png: composition styles (directional tunnels).
- hubs_split.png: split styles (outer-first etc).
- hubs_PZ05.png: fully prioritised joining some lines (not viewed per RULES-hubs).
- S/coop/files/BBH_building_1..5.PNG: "LL5RR to LL5RR 3-way" build sequence (crossing, crossover, exits, balanced joins, prios). Closest to a labelled LL_RR T though BBH not SLH.
- S/coop/files/JunctionaryMergerLLL.png / LLLL.png are 75x75 icons only. Junctionary BBH table: "Simple Classic LLRR hub, following the standard coop building rules" (PSG227, mfb); "Simple Ancient T-BBH ... Also not balanced" (Mucht).
- RULES-coop viewed: junctionary-mirror/images/6x8 3 Way Split before Merge Junction.png (compact split-first 3-way with bridge; too small).

## 7. Open, contradictory, or missing (questions)
Q1. No source gives an SLH T design for LL_RR ML with L_R or LL_RR branch. Is an SLH T on LL_RR to be built from the BBH skeleton (R10: crossing, crossover, 3 exits, 3 joins) or from SLH rules only (H1, H12, H35)? Does the ML-to-ML straight-through need any crossing at all when the branch only exits/joins?
Q2. L_R branch onto LL_RR: R16 says a joiner with no choice needs no balancing, but here the branch train has two ML lanes to choose from while ML trains do not choose. Does H12 ("every sideline lane must reach every ML lane") also require the ML-to-branch split to take from both ML lanes of a direction (D126(1) "which lanes a splitting train can leave from")? Sources say only "exits need no balancing".
Q3. LL_RR branch: H36 lists it but no rule for how the two branch lanes balance onto two ML lanes (multi-to-multi vs fully prioritised joining some lines vs 1-to-N per lane). H24 says multi-to-multi is "practically doable only with 2line join"; which style, and with prio on both joins (R12/R13 one join point per output)?
Q4. D113 vs D176 on multi-line: signal required between split and bridge head, versus leftover after a line-choice split may carry no signal. "to settle with LL_RR." Unresolved; also what D88's "at least one bay required" means when D176 requires exact multiples of TL.
Q5. Doubling an LL_RR ML bridge (D80/D174): are the 2 lanes of a direction each doubled (4 bridges per direction pair), or is one doubled pair spanning both lanes, or is the doubling replaced by the two lanes? D105/D100 geometry is for one lane. D174 SL-bridge rule vs an LL_RR branch (two lanes: single bridge each? one shared?).
Q6. Prio length rule: R20 says BBH-type merge needs a very short prio, SLH joiner a longer one; which applies to an LL_RR branch joining two ML lanes. D29 "between 1 and 2 TL" is unsupported by PM (coop gap 3).
Q7. D159 says branch merges optionally get prios by throughput; for an LL_RR branch is "half prioritised" (H24, equal) allowed or ML-wins only (D27)? H37: "full prio optional (not a mistake)" for BBH only.
Q8. Shifters: no rule makes them mandatory for LL_RR SLH; SML says before every SLH only on shift mainlines. Does D126(3) apply to the first LL_RR T or is it later (D126 says only "in scope")? Where would they sit (H32: inside the hub)?
Q9. Gap size: D38 default one tile; D105 says exits can be "1 tile shifted - we just then shift back when safe". No rule on minimum ML gap for an SLH T with LL_RR (R41/R42 give none beyond >= 1; D58 margin).
Q10. D23 (bridge the ML so joins pass under) vs R51 (do not bridge the prioritised line): still unresolved; D80 makes ML bridges doubled.
Q11. Ties in pathfinder cost: D100 needs equal cost per lane, but D126 line choice needs both lanes reachable; how to guarantee equal cost when branch geometry is asymmetric (branch comes from one side) is not covered.
Q12. No numeric footprint for a BBH/SLH LL_RR (H29, H40 gap); "MSH sizing... not stated" (H34) irrelevant to T.
