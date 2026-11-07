# Milestone 1b: T-junction generator (outline)

**Status:** outline. It gets a full plan, at 1a's level of detail, after 1a is done and the open items below are settled.

The generator produces the first real junction: an L_R full T (D17, D22). The layout is derived from the rules and parameters rather than drawn by hand (D26). Its output is the junction description from 1a, extended with legs and prio pieces, so placement, the ghost, pricing and building work unchanged.

Decisions this rests on: D16–D30, D33, D35–D38. Rules: `RULES-coop.md`. Research: `RECON-junctionary.md`.

## What the player gets

- The Junction window gains a junction family list (only the L_R T for now) and parameter controls. Each is a persistent client setting (`RECON-placement.md`, "Picker windows"):
  - **CL**, using +/- buttons (default 3)
  - **TL**, using +/- buttons
  - **SD**, using +/- buttons (default 2, D21)
  - **leg length**
  - **line notation** for the mainline and for the branch, e.g. `L_R` or `L__R` (D38)
  - **driving side**, defaulting from the road-side setting (D37)
  - **signal type**, defaulting to one-way block (D20)
- Changing a parameter regenerates the layout, and the ghost updates.
- If no legal layout exists for the parameters, the window says why: "CL too large for this gap", for example.

## Design outline

1. **`JunctionSpec`**, the input:
   - the legs (mainline both ends, branch), each with a direction and a parsed line notation
   - the movements (all six)
   - the parameters
   - the handedness

   The generator always works right-hand. Left-hand is the mirror (D37).
2. **The rule checker,** `CheckJunctionRules(const JunctionLayout &, const JunctionSpec &) -> std::vector<RuleViolation>`. This is one function per rule, each returning where and why it failed. It's used two ways: as the generator's filter, and as the test oracle. The rules come from the decisions and from `RULES-coop.md`:
   - **CL:** between consecutive same-direction 45° turns along every route, at least CL tiles, counting diagonal pieces as ½ (D18, D36). S-bends are exempt. No 90° turns (R1–R3).
   - **Splits before merges** along every route (D24, R8).
   - **Every crossing is grade-separated.** There are no level crossings of track.
   - **No switch, signal or junction piece on a bridge head or span** (R38). Bridge heads are on straight track.
   - **Signals** sit at SD spacing along the legs, one-way, facing traffic (D20, D21). There's a signal before every split and after every merge.
   - **Exit legs after the last switch** are at least TL long (R9).
   - **A prio** goes on every mainline merge (D25, D27), on the outside (D28):
     - its connection pieces are spaced so the shortest train always covers one (R25)
     - it extends past the merge point (R26)
     - for 1b its length is the parameter TL; 1c replaces this with the physics (D29, D31, D34)
   - **Bridges:** the span covers the tracks it crosses, and the length suits a bridge type fast enough for the rail type (D35).
3. **The generator** enumerates and then checks.
   - **The fixed frame:** the mainline runs along x, with tracks at the y positions given by its notation. The branch comes from the south along y, with tracks at the x positions given by its notation.
   - **Each movement is a path template.** Right turns are straight → 45° → diagonal run → 45° → straight. Left turns are the same, but cross under the eastbound mainline bridge. Each template has integer free variables: the split or merge position along the source track, and the length of the diagonal run (≥ the CL equivalent).
   - **The design choices:**
     - which left-turn connection bridges over the other (the second grade separation needed by D24)
     - the order of crossing points under the mainline bridge
     - in 1b, only a single mainline bridge (D32's other variants come in 1c)
   - **The search:** for each choice it enumerates the free variables within bounds (the bounding box grows from a minimum until a legal layout is found, or a cap is hit). It builds the layout, runs `CheckJunctionRules`, and scores it by **fewest occupied tiles, then cost** (D30). It keeps the best.
   - The search is bounded and deterministic, so the same parameters always give the same layout. It runs when parameters change, not on every hover.
4. **ASCII renderer,** `RenderJunctionAscii(const JunctionLayout &)`:
   - one character per tile-half for tracks, plus markers for signals, bridge heads and spans, and prio pieces
   - it's printed in test output and written to `docs/junction-tool/layouts/` for the engineer to review
   - the tests don't compare pictures (D26). They check rules.

## Steps (outline, outermost first)

1. **The outer test:** "generator produces a rule-clean L_R T for CL3/SD2/`L_R`", asserting that `CheckJunctionRules` comes back empty and that all six movements are routable. It's red against a stub generator.
2. **Rule checks, one step per group:** curves; splits and merges; grade separation and bridges; signals and legs; prio. Each check gets hand-built passing and failing fixtures. Each failing fixture breaks only that one rule, so it shows the rule is what catches it.
3. **Route tracing:** follow every movement through a layout, as the input the rule checks need.
4. **Path templates and geometry construction.**
5. **The search and scoring.** This makes the outer test green. Further outer tests sweep CL 2–5, SD 2–3, the gaps 0–3 and both notations; every result must be rule-clean.
6. **Prio pieces** in the layout, with block signals as pre-signals or path signals according to the signal type (D33).
7. **Window parameter controls** and the persistent settings.
8. **ASCII renderer** and layout review files.
9. **Manual checks in game:** build the generated T and run trains through every movement. Check speeds through the curves, that the prio holds joining trains, and that nothing deadlocks.

## Open items

All six settled (2026-11-07): D58 margin, D59 prio past merge, D60 separate notations, D61 diagonal under bridge, D62 search limits, D63 hover cache.
