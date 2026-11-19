# Brief: holistic architecture review of the planar solver (independent, read-only)

## The engineer's ask (2026-11-19, verbatim)
- "Cast the net really wide with this review - make it holistically review the architecture with regards to the whole problem. Go back to basics, describe the problem that we're trying to solve in terms that you understand from the brief and check that we're on the right track architecturally."
- "You can absolutely give it context of where we're up to and what we've tried. But ultimately we're looking for a series of layers that help us solve the problem efficiently and concretely. We want each layer to be valid by construction."

## The lens: layers valid by construction
The target is a stack of layers. Each layer takes the output of the one before and makes only its own decisions. Its output satisfies, by construction, every constraint that layer owns. A later layer never repairs, retries or discovers infeasibility in an earlier layer's decisions. Every hand-off carries a stated invariant, such as "every channel interval has room for its tracks at lane spacing", and the next layer relies on it.

Judge the architecture by this, and name every place that is generate-and-check instead. Examples:
- a lazy clash loop;
- an IIS-driven fix;
- a forced local rule that can make the next layer's model infeasible;
- a checker that catches what a layer should have guaranteed;
- variant enumeration that discovers infeasibility by solving.

## Where we are up to, and what has been tried
This is the orchestrator's account. Treat it as evidence, not as direction. The orchestrator has been anchoring on patching one composer; form your own view.
- **MappedComposer** (the shipped composer in src/junction/planar/planar_components.cpp) solves T2, T3 and LL². LL² is a two-lane hub; its best area is 2162.25.
  - It builds each component (fray, join, doubled bridge, prio row) with hand-reserved offsets and optional steps.
  - It separates clashes in a lazy loop (`SolveLazy`); ladders are a newer, partial replacement.
  - D340 later forbade optional bends.
- **The slot layer** (planar_slots, DESIGN-tracks §15) is built: a min-cost integer program giving every track a sideways slot per channel interval.
  - It treats sites (splits and merges) as points.
  - It does not model forks (lane splits whose legs go to one port with no straight leg).
  - §15.5 (sites as slot intervals) was drafted, never built, and is now adopted (D360).
- **The track map** (planar_regions, §16) is built: the skeleton expanded with copies and rows, with sides per stretch.
- **The TrackComposer probe** (branch probe-g3e, not landed; §16.7-16.8) builds one strand per topology line. Each line gets a heading list from `BuildPlanarLineHeadings` (planar_paths, "G2"). The chain of probes:
  1. Base infeasible.
  2. Port S-bends added by a rule (D359).
  3. Decks per run of crossings.
  4. Doubled bridges as parallelograms in series.
  5. Slot bends from the slot layer (TM2).
  6. G2's heading-drop rule loosened twice.
  7. One copy per crossed joiner (D322).
- **Results** (all solves under 8 s):
  - T2: 22/32 variants, best 920, against Mapped's 862, with less track.
  - T3: 14/32, best 1929, against 1907.
- **LL² fails every variant.** Each infeasibility certificate is a stack of forced diagonals, added by G2 at lane splits and merges, against out ports pinned 2 half-tiles apart.
  - Exhaustive masks choosing which fork or merge leg runs straight fixed none of them.
  - The binding merges were outside the masks' definition.
- **Unflipped variants of T2 and T3** fail because a doubled bridge's deck pair must span four lanes under the 24-tile deck maximum.
- **No end-to-end yet.** No track-composer layout has passed the end-to-end checker; that needs prio rows that follow the lane, and a signal pass that reads them.
- **The orchestrator's current reading, to test, not to adopt:** G2's fork/merge diagonals and D359 are sideways moves made outside the slot layer, against §15 ("nothing else creates an S-bend"). The fix is §15.5 plus G2 keeping only channel-path headings.

## Read (repo /home/pete/OpenTTD, feature/junction-tool)
Read with offset and limit and use grep. Skim, then dig where it matters.

**The problem and its rules:**
- src/junction/CLAUDE.md, src/junction/planar/CLAUDE.md and src/junction/topology/CLAUDE.md;
- docs/junction-tool/RULES-coop.md and RULES-hubs.md;
- docs/junction-tool/ROADMAP.md;
- DECISIONS.md (later rows win; skim it all, closely from D297 to D360). Grep these:
  - D326: any topology at any scale, never per-hub composition;
  - D297: a seed solve under 5 s, hover under 100 ms;
  - D340: no optional bends;
  - D354, D355, D357, D358: compactness is judged as architecture on empty tiles, wasted and track;
  - D360.

**The corpus:** docs/junction-tool/RECON-psg-hubs.md and RECON-corpus-ll2.md (skim).

**The design:**
- DESIGN-tracks.md, the whole thing;
- a skim of DESIGN-planar.md and DESIGN-components.md;
- the earlier reviews wip/REVIEW-architecture-tracks.md, REVIEW-channels.md and REVIEW-sideways-modelling.md, to see what they said and what happened.

**Where things are:**
- docs/junction-tool/STATUS.md;
- the probe briefs wip/BRIEF-G3*.md, skim as the record of what was tried.

**The code:** src/junction/planar/
- planar_topology, planar_channels, planar_paths, planar_regions, planar_slots;
- planar_components.cpp: MappedComposer, SolveLazy, ladders and the variant enumeration/search. It's large, so map its structure;
- the checker and the signal pass, briefly;
- src/tests/junction/planar.

**The probe:**
- `git log --oneline feature/junction-tool..probe-g3e`;
- `git diff --stat feature/junction-tool...probe-g3e`;
- the TrackComposer diff in planar_components.cpp;
- `git log --oneline feature/junction-tool..wt-g3b`.

## Questions
Cite file:line, a section or Dnnn for every claim.

**0. The problem, from first principles, in your own words.**
- Inputs, outputs, hard constraints (track geometry, coop rules, signals, bridges, ports, the checker), the objective, and the scale and speed targets.
- Which decisions are combinatorial and which continuous, and how they grow with hub size.
- What a good architecture for this problem must have.

**1. The current architecture as a decomposition.**
- The pipeline as it really is in code, layer by layer: each layer's inputs, outputs and decisions.
- Per decision kind, its owner. Cover at least: crossing order and over/under, sides and flips, doubling, sideways moves, bend placement, lengths, clash separation, prio rows and signals.
- Is each decision owned exactly once, where the information to decide it exists?
- Per hand-off, is there a stated invariant, and is it guaranteed by construction or checked later?

**2. Prior art.** VLSI global/detailed and channel routing; topology-shape-metrics drawing (Tamassia); octilinear metro maps by MILP (Nöllenburg and Wolff); rail layout optimisation; anything else that fits. Which structure does this resemble? What do they guarantee per layer that this doesn't? What to borrow, concretely.

**3. The history as evidence.** Churn, rules added then loosened, hand-reserved space, reviews whose advice recurred. Name the pattern.

**4. Sideways moves.** Per the agreed design (§3a, §15, §15.5, D327, D335, D340, D360), the owner of each kind of sideways move, and where the code departs from it. Is §15.5 enough for LL²'s forks and merges? Consider the frame problem: fork legs sit in separate channels with free offsets, and the only cross-channel ties are link rows and port pins.

**5. The layer stack you would build, valid by construction.**
- Each layer: its decisions, its model (rules, an integer program, or a linear program), the invariant it guarantees to the next layer, and why that invariant makes the next layer feasible by construction.
- Where variants are enumerated, and why.
- Where speed and scale come from: the any-hub, LL³+ target in under 5 s.
- Map each existing module to a layer: keep, rework or drop.

**6. Probe branch: keep or drop.** Each mechanism in probe-g3e and wt-g3b, in one or two lines, judged against question 5.

**7. Verdict.**
- Is the project on the right track?
- The critical path to an end-to-end, checker-passing T2, T3 and LL², riskiest first.
- The cheapest experiment that tests the stack's riskiest invariant.
- Anything in the design or the rulings you think is wrong. Be direct; the engineer wants the truth, not reassurance.

## Output
- Write the review to docs/junction-tool/wip/REVIEW-architecture-holistic.md, about 4000-6000 words, with headings per question.
- Reply with a summary of at most 300 words: the problem in three lines, the verdict, the proposed layer stack with one invariant per layer, the top three structural findings, and the next experiment.
- Do not edit any other file, build, run tests, touch ports 4077/4079 or start a game client. When you have reported, stop.
