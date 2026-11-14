# REVIEW-width-overprescribed: review of DESIGN-width steps 2-5c and the block interface (at 9234c0ae08)

Read-only. Nothing was built or run. HEAD has since moved to cf621a8fef (step 6, render). It changes no placement line cited here: the diff starts at :3524. pl = `src/junction/placement/junction_placement.cpp`, tp = `src/junction/topology/junction_topology.cpp`. INFERRED marks reasoning that was not run.

## Verdict
D296 is right. The width design makes the second track of a bundle a rigid copy of the first: exactly one line over, Corners shifted by a fixed amount, bridge heads a fixed tile upstream. It also makes a block a fixed rectangle with fixed entry lines. No coop or game rule asks for any of this.
- Both refusals come from these invented rules. The square crossing fails (5b) because two of them contradict each other. The downstream-leaning bridge pair fails (D293) because the stagger's direction is fixed.
- The general replacement is small. Each track keeps its own lengths and is held by the rule families we already have (curve gaps, no touching, heads hug what they cross, train-length runs). We add one genuine bundle fact: tracks of a bundle cross the same things in the same order and never cross each other inside the core.
- The cost lands in the cached seed, not the hover, and the T stays far inside 5 s (D297).
- The review also found one real rule the design dropped: main-lane bridges are doubled (D80, D188).

## Q1/Q2. Every rule the width design adds, its backing, and what replaces it
| # | Rule (where) | Backing | General replacement |
|---|---|---|---|
| R1 | The second track of a turning or S-bending bundle is the first track shifted, with no lengths of its own (5a.2; pl:2337-2362) | Invented (D292 open, D296). Not D100: no train chooses between a bundle's two tracks in a keep or 227 core (DESIGN-modules §4.1) | Own lengths. Kept to the right of track 0 by the no-touch rule (pl:1438-1439 already treats the other track of the same edge as unrelated). Curve gaps, runs and hug apply per track |
| R2 | Tracks exactly one line apart on every edge (DESIGN-width Conventions, "Track order") | Notation, at ports only. topology/CLAUDE.md: "spacing may vary along a line". mfb's a and e run 3-7 tiles apart (RECON-mfb-core Q1) | At least the no-touch gap. The side is fixed by "never cross inside the core" |
| R3 | The outer track's Corner is the inner one shifted, same size (D292) | Invented (open) | R1. Each Corner sizes itself. Diagonals may share a tile Upper/Lower, so the tight form appears where it is cheaper |
| R4 | The second deck of a bridge pair sits exactly one tile upstream (4c pl:2483-2486; 5a heads D) | Invented for bundles. D118 gets the stagger from crossing a diagonal. D105: "nothing in the rules says bridges have to be in this arrangement". D287 keeps the parallelogram of a lane's doubled pair, which a bundle pair (no S-bend between the tracks) is not. mfb's 4-deck stack B2 over a straight pair has all heads level (RECON-mfb-core Q2) | Hug only: each deck's heads close on what it crosses on its own line. The stagger appears over a diagonal and is 0 over a straight crosser |
| R5 | Refuse a bridge pair over an S-bend that leans downstream (D293; pl:2814) | Invented: an artefact of R4's fixed direction | Goes with R4 |
| R6 | Refuse a square crossing of a bundle (pl:344; 5b parked) | Invented. 5b's contradiction is R3 against R4 (parked note: "heads at track 0's + (+1,+1) while track 1's run row moves by -eta") | Goes with R1 and R4 (INFERRED; test T1) |
| R7 | Both tracks take the same crossing form (5c.2) | Invented, but saves one binary per track | Keep as a default (toggle candidate, D227 style) |
| R8 | The second track's port fan equals the first's (pl:2594-2599) | Invented. Harmless while the spans are equal | Own fan plus no touching. Low priority |
| R9 | A block is a rectangle 2 x width lines across, with the diverging bundle on lines beside the trunk (D294; tp:240-247; pl:229 "across != 4") | Invented placeholder | Q4 interface |
| R10 | Each track enters or leaves a block on a fixed line exactly at its end (tp:256-265; pl:2283-2286) | Invented | Per-track ports with floors from the template |
| R11 | Fixed block length (spec placeholder; pl:2276-2277) | Invented. It contradicts DESIGN-modules §2: blocks stretch (split spacing, prio length) | A length variable with the template's floor |
| R12 | A band merge block's side is opposite its hand (tp:249-254) | A convention taken from width-1 shapes | Goes with R9 |
| R13 | Refuse a row block whose diverging bundle goes north (tp:964) | Follows from R9's lines south of the lanes. Never fires today (branch ports are S only) | Goes with R9 |
| R14 | Refuse width > 2 (tp:928) | Scope, not geometry: one second-track program list and 2-slot arrays | Per-track lists (after B2, D296) |
| R15 | Refuse a plain node in a bundle (tp:931) | D289 (bundle nodes are blocks) | Keep |
| R16 | A slot's tracks on adjacent lines, groups one line apart (4b; pl:2252-2257) | Genuine: notation (D142, topology/CLAUDE.md) | Keep |
| R17 | The second track has the same crossings, order and over/under (step 3) | Genuine: lane chords interleave exactly when their bundles do | Keep |
| R18 | No prio or approach line at a block (D291) | Genuine: D282 (prio belongs to the join pattern) | Keep |
| R19 | One deck per main lane over a crossing (4c; 2.5 "no lane split or merge at width 2") | MISSING rule: D80, D174 and D188 double every main-lane bridge, which makes four decks per LL crossing. mfb crosses b and c as stacks of 4 (RECON-mfb-core Q1, B1/B2). INFERRED: the checker's "ML bridge is not doubled" fires at H1 | Each through track crosses on its own doubled pair (the width-1 doubled-leg shape, per track) |

**Genuine bundle facts, checked:**
- "Same crossings in the same order" is genuine. The programs already carry it (step 3), so placement needs nothing more for it.
- **"Never cross each other inside the core" is genuine and needed.** Crossings inside a bundle live in the exit and join blocks (DESIGN-modules §4.1, D286). This fact fixes which side each track is on, so the no-touch rule between them is a one-sided floor and needs no order choice.
- **D100 equal pathfinder cost is not a fact of keep or 227 cores.** No train there chooses between the two tracks of a bundle. It is genuine only for choice crossings and tunnel bundles (DESIGN-modules §3). There it is one sum per route (length and curve count from the choice split to the join), not a rule per bridge.
- The shifted copy would not deliver D100 anyway. On e4 the second track is on the inside of the right Corner and arrives at N4 about 2 tiles shorter (INFERRED from the 5a.2 offset table and 4a's attach points).
- No other new fact is needed. D80/D188 (R19) is an existing rule, not a bundle fact.

## Q3. Cost
- **Variables.** The joint already keeps one length per (bend, track) and ties the two tracks with the copy rows (5c.2). Freeing them adds no variable. On the T it frees about 7 bend lengths and 2 head pairs (2.2 token table: e4 C, XS, XC, XC; e6 C; e7 C; e8 C, heads). With R7 the form binaries do not change. The no-touch floors between a bundle's two tracks have a known side, so they need no binaries.
- **Seed (D297, 5 s).** The whole `[bundle]` tag, including placement and the joint loop on the bundle T, runs 13 tests in 0.10 s Debug (TESTS.md:37). The width-1 T joint is 10-16 ms and LL_RR x L_R 89-120 ms (REVIEW-G4 Q4 (d)). INFERRED: the T stays at tens of ms. The risk is extra loop rounds, because the second track now meets new clashes, not model size. At width 3 the pairs per crossing grow about 2.25x, still small. R19 adds 2 chains per crossed through track, with exact shapes.
- **Hover (100 ms, D235/D242).** The second track's points already exist (5a). Equalities become floors, so the per-axis solves stay linear. R19 adds points linearly.
- **Steps.**
  - Survive as is: 2 (its side lines are revisited at B2), 3, 5c (re-pin), 6.
  - 4a: replaced at B2; it stays as the T fixture's stub until then.
  - 4b: ports per track survive; the exact block ties become floors at B2.
  - 4c: survives without the stagger (pl:2483-2486), plus R19.
  - 5a: change it. Delete the copy rows (pl:2337-2362), the lean refusal (pl:2814), the square refusal (pl:344), the track-0-only push (pl:971) and the size copy. Keep the second-track build, occupancy and bridge collection. Run the hug per deck against every crosser track, which is 5b's pair loop.
  - 5b: becomes the acceptance test.
  - Nothing needs reverting wholesale.

## Q4. The block interface
Yes, it is over-prescribed for mfb's join blocks:
- They are cascades of small merges with gaps between them, not one rectangle. Join N350 is 358,330-331 then 356,328-329; E342 is 377-378,338 then 378-379,339 (ANATOMY block table).
- Their inputs arrive from different headings and spacings. N353 takes c's two tracks from x360-363 heading N and e's choice track after its bridge over c at 359,330-333 (RECON-mfb-core Q2).
- Join pieces sit between core crossings: c's line split 361,340 lies between stacks B2 and A (RECON-mfb-core Q5).
- A fixed length contradicts the blocks' own stretch (R11).

The minimal join-block interface:
1. Per track end, a port: a tile point, a heading, and its place in the across order.
2. The template's relations between its ports, as floors derived from its rules (D193). They are exact only where the template is rigid.
3. Its pieces as occupants for the no-touch rule, sized by its stretch.
4. Its stretch variables (split spacing, prio length) with floors.

No rectangle, no side lines, no fixed length. For the first 227 join the ports can still lie after the bundle's last crossing (D286). mfb's mid-core split is a later compaction.

## Open questions of the draft
- Q-W1 (D292): moot under R1. The solver takes the tight Corner where no-touch allows it.
- Q-W2 (D293): moot under R4. The hug decides which way the second deck sits.
- D295 (render): fine as is.

## Q5. Recommendation (in order, before the join block)
1. **One card for Pete, in plain terms.** A bundle's tracks cross the same things in the same order and never cross each other inside the core. Otherwise each is its own track under the usual rules. Equal pathfinder cost applies only where a train chooses between them. Two sub-questions:
   - (a) Does D287 "keep our parallelograms" cover a bundle's lane pair, or only a lane's doubled pair (R4)?
   - (b) Does the through bundle cross on four decks (D188) or two (R19)?
   D292 and D293 become moot. D294 gives way to item 5.
2. **Free the band second track** (R1-R3, R5, R6). Measure seed and hover.
3. **Drop the fixed stagger on row bridge pairs** (R4). Hug only.
4. **Double the through lanes per track** (R19) if card (b) says four.
5. **Design B2 against the Q4 interface.** The 4a functions become the T fixture's stub.
6. **After B2, per-track lists for width 3** (R14).

## Suggested tests
- T1 "bundle: the T places with a square crossing": 5b's choices place. Red: "a square crossing of a bundle is not ruled yet".
- T2 "bundle: a bridge pair over a downstream-leaning S-bend places". Red: the D293 message.
- T3 "bundle: each deck of e5 hugs its own crossers": each deck's span is the crossers' extent plus 2. Red: the stagger adds a tile per deck. INFERRED: e8 and e4 pass under e5 straight, so the stagger costs exactly that tile.
- T4 "bundle: a bundle's tracks never touch": checked on the render, by the occupancy rules.
- T5 (if four decks) "bundle: the through bundle crosses on four decks". Red: two.
- T6 seed time of the bundle T under 5 s and hover under 100 ms, pinned (D297).
