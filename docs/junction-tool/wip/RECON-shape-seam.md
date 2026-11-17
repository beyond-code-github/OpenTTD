# RECON S0: where the composer decides headings (wt-planar 72c03f4196)
Files: P = src/junction/planar/planar_paths.cpp, C = planar_components.cpp (MappedComposer 775-1813; Build 1144).
Key fact: every heading of a bundled/core channel is decided in ONE place, BuildPlanarPaths (P). The composer (C) only reads it, except the fixed diagonals in Fray/Join/Fork/Flex/Bundle, whose headings are `h` +- 1 from a port/path heading.

## 1. Decision sites (stretch end = at event/node, inside = between, jog = net zero)
P:143-170 head(): channel heading run = short turn from `from` (port heading, or in-channel's last heading P:155) to `to` (out port heading). Inputs: route turn (port headings). Stretch ends (channel start/end at node/port) + inside (arc 45 deg a piece). Fails on turn -4.
P:175,217,272 fork_side/partner_could/fork-leg jog: a straight fork leg (no straight leg at node) with 1 piece becomes {h, h+side, h}. Input: crossing suit. JOG.
P:224-293 crossing pieces (settled <=6 passes): per crossing a piece index (crossing_piece) = first piece >= last that suits: over => axis (P:233), under => not parallel to partner's current piece (P:234,239). Input: crossing angle, over/under (variant.over bit via P:117-124), partner piece. Heading AT an event (the crossing); sets stretch end heading.
P:249-261 perpendicular-under-prio rule: under a prio over-channel, cross at right angles, inserting a back-and-forth {h[at-1],h[at]} (P:259). JOG (inside stretch before the event).
P:262-280 detour: when no piece suits, insert {h[at-1],h[at]} after `at` (P:278). JOG. Same shape as above.
C:1240-1262 build_bundle: heads copied from path.headings (C:1246); `doubled` pieces = crossing_piece of over crossings with full-share lanes, must be axis (C:1256), 2 lanes only. Decides: copy step pairs inside Bundle (C:~430-470: sg turn Turn(h,+-1) twice). JOG (D333 copy step), from width change.
C:1271 build_core: single-heading channel, h = headings.front(). Flex (C:~385) jog from variant.steps bit (cand_steps, C:1281-1285; PlanarCandidateSteps planar_gaps.cpp). Diagonal Turn(h,jog) then h: JOG inside stretch. Doubled copy: Double() C:~320 JOG (sg steps), lanes offset offs[] from JoinEntry (C:1274) = offsets not headings.
C:1218-1233 build_fray: h = PortHeading(channel.up) (C:1221); Fray(): V_k diag Turn(h,sigma) (Fray ~C:350 `Turn(h, sigma)`), prio spread step Lt=Turn(h,-1) (C:~347), prio doubled S_j steps Turn(h,-sigma). Inputs: width change, site side sigma (ResolvePlanarSites: prio/straight, ties bit variant.ties). Fray legs: stretch START heading h+sigma (at node) , spread = JOG; V's diagonal is the leg's turn (net, balanced by route turn).
C:1362-1391 build_join: h = PortHeading(channel.down) (C:1365), must equal both channels' last heading (C:1366). Join() (~C:365-435): J_k diag Turn(h,-sigma) into node (stretch END heading h-sigma at node); prio Double steps; exit step Turn(h,exit_step) (close-up, from BuildPlanarGapSteps gap_steps, C:1388). Exit step JOG (D335); prio out rows step Turn(h,-sigma) JOG.
C:1398-1425 build_nodes (per-node): Fork(h=in channel's last heading, C:1408): two diagonals Turn(h,-1/+1), solver-optional (AddSeg opt=true) = free binaries not shape; Join with step = PlanarMergeExitStep / PlanarInnerMergeExitStep (C:1423). Exit step JOG.
Not heading decisions: Flex/Bundle offsets, deck chains, ladders (C:1621-1665, read-only from sides.Events()), clashes.

## 2. Data flow
Only structure holding per-line (really per-CHANNEL) heading sequences pre-model: `PlanarPaths` (planar_paths.h:18-28): paths[ch].headings (vector<PlanarHeading>, one per piece, incl. jog inserts) + crossing_piece (per entry of channel.crossings) + over[x]. Built at C:1155 (`this->paths`), read in build_bundle/core/join/nodes/UnderSegs/deck_groups. Lines in a channel share it (lanes step together). Other headings (fray diag, join diag, exit/close-up/copy/cand steps, fork diagonals) are NOT stored: they are literal Turn(h,..) args in AddSeg calls inside Fray/Join/Fork/Flex/Bundle/Double, derived from h plus res[] sites (ResolvePlanarSites, C:1161), gap_steps (C:1171), cand_steps (C:1177). Stretch-to-segment mapping exists only after the model (sides.Events(), event_seg C:1625).

## 3. Is (heading at stretch start, heading at stretch end) per (line, stretch) enough?
For the channel-level pieces: nearly. Headings are per channel; stretches of lanes inherit. Needed additionally:
- Per CHANNEL not per line (bundle lanes step together); fray legs/join joiners/fork legs/port lines are single-line fixed diagonals with no freedom.
- Heading at crossing = piece heading, so crossing_piece (which piece of the sequence) must be derivable: store the heading list or (event heading + piece index). Start/end alone loses: jog count per stretch (the P:259/278 back-and-forth inserts, and fork jog P:272). Jog rules re-derive them only from partner headings (partner_could, passes), i.e. a rule over the shape, not the shape; to reproduce today bit for bit carry the jog flag/count per stretch, or define the default shape as the full heading list = paths.headings.
- Stretch end at a node: the channel's first heading = in channel's last (P:155) and last = out port heading or join `h` (C:1366): carry (fixed by the topology; check equal, do not choose).
- Doubled piece set (derived from crossing_piece + over + FullLanes; must be axis, C:1256): derivable.
- Not shape, stay as rules: fray diag/spread, join diag, exit steps, close-ups, copy steps, cand step bit (variant.steps), fork diagonal binaries, tie bit (variant.ties), over bits (variant.over), flips (variant.flips).
Verdict: end headings + jog rules unchanged suffices ONLY if the stretch's jog inserts are re-derived identically; safest S1 `PlanarShape` = per channel {headings (with jog inserts), crossing_piece} == PlanarChannelPath; per (line, stretch) end headings are a view of it (heading at event k = headings[crossing_piece[k]]).

## 4. Smallest seam
Split BuildPlanarPaths (P:117-295) into (a) `DecidePlanarShape(topology, channels, choice) -> PlanarShape` = today's body from `head` to crossing_piece (P:143-294), and (b) `PlanarPathsOf(shape, channels, choice)` filling `over` and wrapping. S1: BuildPlanarPaths(topology, channels, choice, const PlanarShape *given = nullptr): given != null skips head/passes and validates (consistent turning, crossing suits, doubled pieces axis). MappedComposer reads one thing: this->paths (C:1155), so replacing that line is the whole seam. Extract (a) first, assert consume(extract(x)) == x, then add a field `PlanarShape shape` to MappedVariant/SolveMappedVariant (C:2543) with default = DecidePlanarShape. Fray/Join/Fork/Flex/Bundle keep taking `int h` from paths: no change needed in S1. For S2 stretch-turn freedom, Fray/Join diagonals and Join entry check C:1366/1419 must accept shape headings (they require channel last heading == port heading).

## 5. Risks (order dependence)
- Settling is joint across channels (partner_heading/partner_could, 6 passes, P:224): a shape must be extracted AFTER settling; a given shape cannot be built channel by channel.
- Shape depends on variant.over (P:117-124) and flips (FlipPlanarTriangles changes `ch` event order before paths): one shape per (over, flips); cache key must include them.
- Jog pieces: p.headings.insert shifts piece indices of earlier-decided crossings of the same channel; crossing_piece is only valid after the final pass (P:292-294).
- build_bundle needs headings.size()==1 to choose core vs bundle (C:1240): a shape with an inserted jog turns a core into a bundle (changes the whole construction, Flex cand step vs Bundle).
- Fork jog (P:272) is only decided when size==1; fork diagonals are optional AddSeg: solved geometry decides actual turn (free binaries C:~290 Fork) - heading at the fork leg is NOT known before the solve.
- Candidate step (variant.steps) and gap steps (PlanarGapStep) add jogs on top of paths: they depend on res[]/ties but not geometry; fine. Ladders (C:1621+) read segment indices after the model: post-shape.
- Doubled-bridge axis check (C:1256) fails the whole variant when a given shape puts an over crossing on a diagonal.

## 6. Derived vs hard-coded, per item-1 site (MappedComposer path has no named lines/hubs; all names come from t/ch/res)
| site | derived from / hard-coded (why) |
| P:143 head | derived (port headings, short turn). Special case: from-less channel takes `to`; turn -4 refused |
| P:175,272 fork jog | HARD-CODED rule: "a straight fork leg jogs toward its own side, only when 1 piece" - a construction, not a crossing rule; one-kind special case (fork leg); not from topology |
| P:233 over=axis | derived (crossing rules) |
| P:234,239 under not parallel | derived (crossing rule), but "first piece at or after the last" ordering is a heuristic |
| P:249-261 perp under prio | HARD-CODED: "cross prio at right angles" is the hand T3 D path copied (comment P:248), only under prio over, one detour max (`detour < 2`) |
| P:262-280 detour | heuristic: back-and-forth insert is a fixed 2-piece pattern, max 3 tries (magic), pass cap 6 (P:225) |
| C:1256 axis check, 2-lane-only (C:1250) | HARD-CODED limits ("two full-share lanes only"): refuses, not decides |
| Bundle copy step C:~430-470 | derived from D333 doubled-bridge shape (component fixed shape); sg = -1 left lane, +1 right lane hard-wired (j==0 ? -1 : +1) |
| Flex/cand step C:1281 | derived (DESIGN-pieces src 5) but variant bit; the "away from in port's neighbour" rule is a special case for a straight channel over crossings (T3 B) |
| Fray C:~347,350 | component shape; prio spread uses Lt = Turn(h,-1) regardless of sigma: HARD-CODED left; STUB, FANW, gap 4/2 constants |
| Join C:~365-435 | component shape; magic lo=6/4, lo0=6/2 (lo0 passed 2 from build_nodes), 2*P+2 prio coverage (D-rule); `exit_step == -1 && n>1` refused (not composed) |
| Exit steps (gap_steps, PlanarMergeExitStep, InnerMergeExitStep) | derived-ish from D335 pinned-gap rule; inner variant = "+1 when another merge of its site has its out line to the right": special case, one kind (inner one-lane merge) |
| Fork C:~290 | component shape, +-1 diagonals both optional (solver) |
| C:1366/1419 | assertion that join channels arrive on `h`; not a decision |
Legacy Composer (C:157-735, wired by name: "E1","EF1", HE/HW...) is fully hard-coded to T2/LL2 and is NOT on the mapped path; shape seam need not touch it.
