# REVIEW-slots-lengths: review of wip/SPEC-slots.md §5 Q7 with §0, §2, §4, §7 (at b1949b0715; probe wt-slots 5334fc337e)

**Verdict.** Q7's problem is real, but it is narrower than stated, and there is an exact test for it that needs no lengths. By Stiemke's lemma (an LP alternative theorem), the lengths LP with lower bounds only closes for *every* choice of lower bounds if and only if two things hold. (a) The closure equalities hold with Slots' exact values, letting the free parts take any sign. (b) The *homogeneous* closure has a solution with every free part at least 1. "Homogeneous" means every exact quantity set to 0: pins, deck gaps, steps, cusps. Neither condition involves a length or a lower bound. Condition (b) does not even involve Slots' sizes, only which parts are exact. Where (b) holds, step sizes need only (a), a lengths-free consistency system, so Slots can size steps from Shape's model alone. Where (b) fails, every fixed answer is broken by some lower bound, so the minima must be known there. (b) does fail on the T. Once Slots fixes B's deck gap between D and F, the B-D-F face's eastward D and F parts must sum exactly to that gap (F2), so no lengths-free rule can be sound on the T.
- Option A as written cannot meet D361.
- Option B runs a relaxed Lengths inside Slots, and over the time budget.
- Option C undoes the architecture's "S-bend sizes are an earlier layer's promise".

I favour an option D. Slots finds the parts its exact set leaves non-stretchable, with one lengths-free LP. It owns their lengths, using minima that are handed down and derived from the rules. It promises (a)+(b) as L8. Lengths then closes for any lower bounds, by proof rather than by probe. L8 as written (lower bound 1, no clash rows) is not that promise. The cheapest falsifier is a homogeneous max-support LP per variant: about 30 lines in `ClosureLp`, milliseconds per variant.

## Findings

**F1 [wrong] L8 tests a weaker property than Lengths needs (confidence: high).**
- *Evidence:* L8 (SPEC-slots:89) and hand-on (2) (:79) say every face closes with lower bounds only. The oracle is `ClosureLp` (planar_shape_layer.cpp:310-547): parts >= 0 (:362), each piece's total >= 1 (:393), no clash rows. Closing at lower bound 1 says nothing about lower bound 4 in the same part.
- *The exact statement:* let M be the closure matrix (points as free columns, free parts as columns >= lb) and b the exact constants. Then {M l = b, l >= lb} is non-empty for every lb iff (a) M l = b is solvable with l of any sign, and (b) some r with every free part >= 1 has M r = 0.
  - (if) take a solution l0 of (a) and use l0 + t r with t large.
  - (only if) suppose no such r exists. Stiemke gives y with yᵀM_points = 0 and yᵀM_parts >= 0, not all zero. Any feasible l then has yᵀb >= (yᵀM)_e · lb_e, so a large enough lb on that part e breaks the LP.
- *Fix:* restate L8 as:
  - L8a: (a);
  - L8b: (b), per part, not per piece;
  - L8c: the integer and parity form of (a) (F6).

  The oracle becomes two LPs and a lattice check, all lengths-free. Hand-on (2) then holds by proof.

**F2 [missing] The T's B-D-F face cannot be stretched once B's deck gap is exact; §2.1 calls it Lengths' own (confidence: high for the identity; medium, INFERRED, that some T1 variant makes least packing infeasible).**
- *Evidence:*
  - B is the separator over D and F, and its D and F crossings are adjacent: an empty triangle (planar CLAUDE:87, :89). One run gives one structure (D371(2)), so B's part between D×B and F×B is a deck gap g that Slots fixes (U1, SPEC-slots:14).
  - B runs on an axis (W), D turns N..E and F turns E..S. With least turning, neither has a westward part (INFERRED from the objective, SPEC-shape:30; the probe can print it).
  - Walk the face in either triangle orientation and take the x-closure: **the x-components of D's and F's free parts inside the face sum to exactly g**. Every eastward part there (D's NE, F's E and SE) is bounded by g, and no monotone shape changes this. H11 is not needed for the argument.
  - In X2's homogeneous form (g = 0) those parts are forced to 0, so (b) fails.
  - Worst case (INFERRED, open crossing D×F as a variant bit, SPEC-shape §2.1 cost has no angle preference): F is over D and heads E at D×F. F's half middle plus its head (Shape's `end_run`, planar_shape_layer.h:33) plus its turn to SE must fit inside g. That needs about 4 half-tile units, so g >= 2 tiles. Least packing, with the D and F crossers adjacent, gives 1 tile.
  - §2.1 (:59) says sideways changes outside frames are free and belong to Lengths. This face is not a frame (its walls are not parallel), yet Lengths has no freedom in it.
  - U3 (:16) makes a crosser's diagonal under a deck Slots', but only "counted in U1" of its own span. Nothing ties it to B's gap.
- *Fix:* add **U12** to §0.1: free parts that the closure fixes or bounds once Slots' exact set is fixed. They are computed lengths-free (Q3) and become Slots' exact parts. Their rows link frames: here the eastward x-sum = g, a difference row between B's deck slots and the D and F parts. Their minima come from Shape (`end_run`, H6 widths) and from Structure (rule fields).

**F3 [wrong] X2e's "Slots always has an answer (L9 is reachable)" (SPEC-slots:119) rests on looser geometry than Lengths has (confidence: high on the facts, medium on the impact).**
- *The probe's relaxations:*
  - (a) parts >= 0 (:362), so a crossing may sit exactly on a bend;
  - (b) a step adds only `lane * Right(h)` (:374-384), with no travel along the track, although an n-lane S-bend on an axis also moves 2n units along it;
  - (c) no clash rows;
  - (d) points and lengths are continuous, with no parity;
  - (e) every lower bound is 1 half-tile unit, and the separators' end parts get no `end_run`, though Shape hands it down.
- *Impact:* each relaxation is harmless on stretchable parts (F1) and decisive on non-stretchable ones. With (a) and (e), F2's head and diagonal fit any g >= 0. With (b), a step inside a bounded interval looks free when its own travel along the track should count against the bound. X2b already shows non-closure at lower bound 1 from decks alone (T3 10/16, LL2 8/16) and from cusps alone (T3 12/16, LL2 8/16). Those cases mix L8a-type and L8b-type failures, which the probe cannot tell apart.
- *Fix:* rerun X2e/X2f with parts >= 1, separator end parts >= `end_run`, and each step's travel along the track. Cite only that run as L9's evidence.

**F4 [missing] Clearances (U11) cannot stay outside L8 (confidence: medium; no counterexample established).**
- *Evidence:* U11 is marked out of scope (:24). The rungs come from a rough solve (planar CLAUDE:163, `LadderRungs`), so they depend on lengths. A rung is a row s(l) >= g with mixed signs. Stretching along r meets it only if s(r) > 0.
- *Fix:* Lengths' spec defines rungs from Structure's sides and faces, without lengths. L8b then carries them as strict rows (s(r) >= 1) in the same LP. Until then, "Lengths closes" is unproven for clearances.

**F5 [wrong] §0.2's always-feasible network claim and L9 break once U12 rows exist (confidence: medium).**
- *Evidence:*
  - F2's gap minimum (g >= 2 tiles in the worst case) makes B's span the D bundle plus g plus the F bundle. Shape's H6 assumes adjacent crossers (SPEC-shape:28), and Structure's grouping counts crosser widths only (planar CLAUDE:176), so F1 of §0.2 can fail at the limit.
  - U12 rows are not difference rows in general (geometric coefficients), so the integrality argument of §0.2 (:31) no longer covers them.
  - The fork binary (§2.2, :66) is not a difference row either. Each of its two settings is, so feasibility needs an argument per setting.
- *Fix:* Shape's capacity rows count the gaps a face forces between consecutive crossers. Shape has the headings and `end_run`, so this stays in its own model (D367). L9 then needs a stated proof for U12 rows, or the experiment's measure.

**F6 [missing] Integer and parity closure (confidence: medium; the algebra is certain, how often it occurs on T and LL2 is unmeasured).**
- *Evidence:* Lengths is integer with parity (planar CLAUDE:132). Axis parts are whole tiles, and an axis, diagonal, axis passage needs an even diagonal count (RECON-corpus-steps: n = 2 per tile). Diagonals put one length into both axes, so 2x2 minors are ±2.
  - *Example:* free SE and NE parts closing an exact x offset of 1 unit solve to 1/2 each.
  - Stretching by an even integer r keeps parity, so only the base solution l0 of L8a matters.
- *Fix:* L8c is the integer solution of the L8a equalities, with parity. It is lengths-free (Hermite normal form, milliseconds). Alternatively, prove that whole-lane values keep b on the lattice.

**F7 [wording] Option B's cost (SPEC-slots:100).**
- "0.3-2.8 s per variant" is X2f's *maximum* on T3 and LL2 (T1 7.7 ms, T2 12.8 ms), measured on a relaxed MILP (F3). X2e's mask 31 reached 10.9 s on LL2.
- At keep 16, that is 5-45 s on LL2 against the 5 s seed (D297), with Shape already at 8.8 s there (:118). Say "up to", and add the relaxation.

## Answers to the questions

**1. Is the problem real; is there a lengths-free sufficient condition?**
- **Yes, but only where (b) fails.** On parts that can be stretched, any sizes that satisfy L8a close for every lower bound, so Q7's "a step's size depends on the room the free lengths leave" does not apply there.
- **Our data:** X2a's `closes_free` (mask 0) is exactly the per-piece homogeneous test with nothing exact (no constants: :329-393, cusp w free). It gave 16/16 on every hub. So Shape's output can be stretched, and only Slots' exact set breaks (b). F2 shows that on the T it cannot be avoided: Slots must make B's gap exact (U1), and no monotone shape gives D or F a westward part.
- **Known results** (from memory, not checked here):
  - *Orthogonal drawings* (topology-shape-metrics, Tamassia 1987): the lengths along each axis form a circulation in the dual graph. Hoffman's circulation theorem (1960) then says the drawing closes for every lower bound iff every dual cut has a free arc leaving it. That is the VLSI-compaction rule that every free edge lies on a stretch line. It is the lengths-free condition there, and it is the orthogonal case of (b).
  - *8 directions:* diagonals couple the axes, so per-axis cuts are necessary but not sufficient, and integrality is lost (F6). The general form is Stiemke's alternative (F1): one LP, lengths-free, with exact pieces (port pins, deck spacing, S-bend sizes, cusps) entering only as removed columns and constants.
  - *Turn-regularity* (Bridgeman et al. 2000) has no octilinear form that I know of.
  - Optimal compaction is NP-hard even in the orthogonal case (Patrignani 2001), but that concerns Lengths' objective, not feasibility.

**2. Options A, B, C and a better D.**
- **A** (rules, closure LP as oracle):
  - Fits D362, D366 and the budget.
  - Fails D361. Its oracle cannot show Lengths closes (F1), and F2 is a T case where no fixed lengths-free answer works for every lower bound.
  - It is sound only if the experiment finds no part that cannot be stretched.
- **B** (Slots solves steps with lengths):
  - Puts a relaxation of Lengths inside Slots, so the D362 roles blur.
  - Needs Lengths' rules (curve windows, bays, coverage, clearances) handed down, not just numbers (D366).
  - Lengths re-solving with clash and parity rows can still fail on Slots' choice: generate-and-check across layers (D361).
  - Over budget (F7). It also solves more than needed: lengths matter only on the parts that cannot be stretched.
- **C** (steps as Lengths' integers):
  - Contradicts planar CLAUDE:25 ("S-bend sizes" are an earlier layer's promise) and D362 (Slots the one owner of sideways moves).
  - Pins and spans become equality and upper rows in Lengths.
  - The measured cost is X2e's joint MILP, up to 10.9 s per LL2 variant with no clash rows.
  - In effect it is the old composer with optional steps, which D360 retired.
- **D** (favoured): Slots, per variant:
  1. Fixes its exact set by rule (deck strips, cusps, pins, copies, rows, U8 diagonals).
  2. Runs one lengths-free max-support LP on the homogeneous closure, giving N, the free parts that cannot be stretched.
  3. Projects the closure off the span of the stretchable parts. That part has a positive null vector, so its columns span a subspace, and the projection leaves linear equalities linking N's parts to Slots' variables.
  4. Solves its slot program with those rows and N's minima. N's parts become exact.
  5. Promises L8a-c.

  Cost: one LP the size of `ClosureLp` (X2a's ran 5-27 ms at most), a projection, and a few extra rows: milliseconds per variant. It meets D361 (no trial of a later layer), D366 (rule minima flow down), D367 (L8a-c tested at Slots' output), and D362 (Lengths keeps lower bounds only). It needs one ruling: Slots owns the lengths of N's parts (U12). If N turns out empty, D reduces to A with a proof.

**3. For D: what must flow down, its failure modes, and the cheapest falsifier.**
- **What flows down** (only for parts that can land in N), all derived from the rules:
  - from Shape, already present: headings, event pieces, `end_run`, H6 widths, `copy_middles`;
  - from Structure, as rule fields like `limit` and `side_gap` (D377 Q3 precedent):
    - the least part between a crossing and the crosser's next bend;
    - the least node-leg part before its next event (today's layout rule: 2 units, planar CLAUDE:96);
    - the curve-spacing window (`JunctionCurveGaps` at the spec's train), needed only when a whole window lies in N plus the exact set;
    - the landing's least length and parity (D375).
  - The least S-bend (1 lane) is a constant.
  - Prio coverage and bays are path sums: needed only if a whole path is in N.

  Lengths' spec must not add a lower-bound kind that can apply to N without updating this list (a forward contract under D368).
- **Failure modes:**
  - N large or spread across the hub, as X2c's conflict set was across three sides: Slots would then re-solve Lengths, and B's cost returns.
  - N's minima exceed the span limit: L9 fails, and Shape's capacity or Structure's split is missing a promise (F5).
  - U12 rows are not network rows: the slot program becomes a small ILP with no feasibility proof.
  - Clearances (F4) and parity (F6) stay outside the proof until those parts of the spec exist.
- **Cheapest falsifier:** the max-support LP below. Prediction: on every T1 variant, D's or F's eastward parts in the B-D-F face are in N.
  - If N is empty everywhere, F2 is wrong, and A is provable with L8a alone.
  - If N spans most faces, D's premise (N small and local) is falsified, and the engineer must choose between B's cost and new Structure or Shape promises (for example, a structure never closes a triangle face, at +1 structure).

**4. The two checks in §0.**
- *§0.1 is incomplete:*
  - It needs U12 (F2).
  - U5's "where the step sits stays free" holds only in stretchable intervals; elsewhere a step's travel along the track bounds its size.
  - U11 belongs in L8 (F4).
  - U2's end runs are minima on separator parts that can fall in N (F2's worst case). Slots already has them from Shape, so D366 is respected.
- *§0.2 is right for rows that only involve slots, and wrong as stated once U12 rows exist (F5):*
  - F1 of §0.2 needs H6 to count face-forced gaps.
  - F3's port-end argument should not cite the old layout program's 4 units (planar CLAUDE:95). The right reason is that moving a box side outward lengthens every port-end part on that side; all of them hold the port heading (H2), so they can be stretched while they are free.
  - "Slots never fails (L9)" rests on F3's relaxed probe.
- *§0.3 has the right idea but is too narrow.* The condition is (b): over unions of faces with any headings, not face strips with parallel walls. Q7 is right that a face-by-face rule is not enough, but one global LP is.

## Suggested tests
1. **X2g, the falsifier.** Add bit 64 to `ClosureLp`:
   - make every constant 0 (cusp w = 0, deck rows 0, pins d = 0, no steps);
   - exempt separator parts strictly inside a span;
   - for each other part, add t with t <= part and 0 <= t <= 1, and maximise the sum of t.

   Run it with families 7 on T1-T3 and LL2 at keep 16. Print per variant |N| and each N part (track, stretch, heading, from/to as X2f prints). Pass and fail as in Q3.
2. **X2f with real minima.** Rerun X2f with parts >= 1, separator end parts >= `end_run`, and each step's travel along the track. Count closes, and print the T1 variants whose least packing on B fails (expected: F over D, heading E at D×F).
3. **Hand oracle case.** A one-lane T triangle with B's gap at 1 tile and F over D heading E:
   - L8b names F's parts;
   - at g = 2 tiles, L8a and L8b pass;
   - each failing alone gives one message.
4. **L8c on X2f's answers.** An integer solve of the equalities with parity. Count variants where the LP closes but the integer solve does not.

## Follow-up: X2g (2026-11-20)
Read: wip/BRIEF-X2g-max-support.md; commit 64922b062d (`ClosureLp` bit 64, the planar_shape_layer.cpp diff); .claude/ng1/build-rel/x2g.txt (351 lines).

**Verdict.** X2g is a valid test of (b), and it falsifies D as I framed it: on a dense hub most parts cannot be stretched, so "Slots owns N's lengths" is option B. The orchestrator's causal reading holds on T1, where it can be shown by hand. A face closed by a pinned box segment, with Shape's monotone headings, has no free part going back along that side. The root is in the spec, not in Slots' rules: exact step sizes add no free direction, so no Slots answer under U5 can make those faces stretchable. Before the merge is ruled, test option E (Slots fixes each step's existence, side and order; Lengths sizes it). E keeps Lengths binary-free with lower bounds only, and it can be proven by L8a/b. One homogeneous run (X2h) decides it.

**G1 [validity] X2g tests (b) correctly for the spec's exact set; if anything it undercounts N (confidence: high).**
- *Pins at d = 0:* a stretch must keep every pinned difference, so a side's port ends move together. Collapsing them is the correct homogeneous form, not an artefact.
- *Piece totals >= 0:* redundant with parts >= 0, harmless.
- *t:* the system is a cone, so supports add and rescale. At the optimum t is exactly 0 or 1, and the 0.5 threshold is exact.
- *Exemptions:* deck interiors only, which is right: they are the only parts the spec makes exact (U1).
- *Steps:* an exact step splits a part of heading h into two parts of heading h plus a constant. It adds no free direction, so under U5 steps cannot shrink N.
- *Undercount:* U6's exact rows (copy and prio row exactly 1 from the parent) are missing. They are tied only at nodes (diff at :436), so the spec's true N contains X2g's.
- *Calibration:* mask 64 alone gives 0 everywhere, which reproduces X2a's `closes_free`.

**G2 [wrong] D's premise fails (confidence: high).**
- Non-exempt parts that cannot be stretched, on every variant: T1 8/26, T2 60/77, T3 71-125/149, LL2 71-91/111 (x2g.txt summary lines).
- Owning N in Slots means solving most lengths there. That is B with extra steps, the falsifying outcome I named in Q3.

**G3 [cause] Pins alone explain T1's set (confidence: high for T1; medium elsewhere until the ablation).**
- *The T1 face:* for T1 o3t0f0m0, the RIGID lines B1 stretch=1 (SW) and F1 stretch=2 pieces 0, 1 (E, SE) are exactly the eastward parts of one face.
  - That face is closed by the pinned S segment from CD1 to AF1 and walks: CD1 N, D1 stretch 0 N, B1 stretch 1 backward (NE), F1 stretch 2 E then SE, AF1 S.
  - No free part goes west, so the x-closure reads: B1's and F1's eastward parts = the pinned gap (gap + 1 lanes) + any exact steps.
  - No deck and no cusp lies on that face.
- *General form:* a face closed by a pinned box segment, whose free walls' components along that side all have one sign. Least-turning monotone headings make this the norm. T2's RIGID list is mostly diagonal main-line parts (B at heading 7, E2 at 3, F at 3 or 4), so any stretch cut must cross a pinned segment or a part not parallel to it.
- *Sharpened reading:* the driver is the gap between an in-port and an out-port on one side (the face between those ports). A port's own lanes form a frame and are harmless. Decks and cusps add rigidity but are not needed for it.
- *Ablation, with my predictions:*
  - families 68 (pins only): about equal to 71 on T1, and most of T2;
  - families 66 (decks only): small;
  - families 65 (cusps only): about 0 on T1;
  - the coordinator's 70, 69 and 67: 70 and 69 about equal to 71, and 67 much smaller.
  - If 67 (no pins) is not far below 71, the sharpening is wrong.

**G4 [missing] Making such a face stretchable needs a sideways move of free size inside it, and U5 forbids that (confidence: high, from G1 and G3).**
- A face between two ports becomes stretchable only if some free part in it can go back along the side, that is, a step whose size is free.
- U5 and CLAUDE:25 make S-bend sizes an earlier layer's exact promise. Q7's "step sizes depend on lower bounds" therefore comes from that rule itself, not from any choice of Slots' rules.
- The step and the parts it trades against are one degree of freedom: an n-lane step on an axis is 2n units of diagonal.

### Answers
1. **Valid (G1).** Should steps be free columns? Under the spec as written, no (G1). As a test of a different split of duties (E below), yes, and that is the next run.
2. **It falsifies D (G2).** The orchestrator's reading holds on T1 by hand (G3): pins suffice there, and decks and cusps add. The ablation confirms per hub.
3. **The options**, judged on D361, D362, D366, D367 and the 5 s seed (D297):
   - **B is dominated.** It solves the same coupling twice: Slots' relaxed MILP, then Lengths. Lengths can still fail on clashes and parity (generate-and-check across layers, D361). It took 2.8-10.9 s per LL2 variant (X2f, X2e) at lower bound 1, with no clash rows.
   - **Loosening pins contradicts D313/D378** (ports at notation spacing; fans as far from the ports as possible) and the corpus (52% of steps are port fans, RECON-corpus-steps). It would only move the fan outside the box. Reject, unless the engineer rules otherwise.
   - **The merge is the honest answer if E fails.**
     - One layer owns steps and lengths: D362 changes and needs a ruling; D366 and D367 are fine.
     - Its integer solve carries each step's existence and sign (n = p - m), and curve rows that apply only when a step exists, which makes them disjunctions.
     - X2e/X2f's joint MILPs already take up to 5.0 s (T3) and 10.9 s (LL2) per variant without clash, curve or parity rows. So "1-3 variants under 5 s" is not shown for LL2.
     - It also inherits F4: without rungs defined from faces it falls back to lazy rounds (D361).
   - **E is better, if X2h passes.**
     - Slots keeps the sideways structure: frames and order; exact pins, decks within the limit, copies, rows and cusps; and per step its existence, side and order along the stretch (D313/D378 rules at ports, and a node-group rule from X2f's tally).
     - Lengths sizes each step (integer lanes, at least 1) with every length. Neighbour order adds difference rows, which stretching satisfies.
     - A step that stays in N even with free sizes is exact and sized by L8a.
     - Slots promises L8a, plus L8b computed with the step columns free. Lengths then closes for every lower bound, by proof, with no binaries and no upper bounds.
     - E is Q7's option C without C's two costs. Slots keeps ownership of which sideways moves exist, which is D362's intent.
     - Ruling needed: S-bend sizes move to Lengths (CLAUDE:25, U5).
     - Cost: rules plus one homogeneous LP per variant in Slots (milliseconds); a few integer columns added to Lengths' own solve.
   - **X2h, E's falsifier:** under bit 64, add sign-fixed free step columns (sign from X2f's n) at X2f's step places only, and include them in the max-support objective. Report N.
     - If N is empty, or holds only step columns, on T1-LL2: E stands.
     - If N stays near X2g's counts: free-size steps at coop places do not loosen the hub, and the merge remains.
     - A companion run with free-sign steps on every stretch (bit 8 under bit 64) bounds what any step pattern can achieve.
