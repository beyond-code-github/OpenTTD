# Milestone 1c: traffic and train physics (outline)

**Status:** outline. It needs code research and engineer decisions before a full plan. See open items.

1c makes the generator traffic-aware:
- the prio length comes from real train performance (D29, D31, D34)
- the bridge arrangement is chosen by traffic level (D32)

Decisions: D29, D31–D34, D36, D39.

## What the player gets

- **A train picker in the Junction window:** an engine and a wagon count (TL follows from the consist length). It's stored as a client setting. The picked train also sets the bridge type: the cheapest bridge at least as fast as that train (D47), replacing D45's fastest-available-engine default.
- **A traffic-level control.** How it's expressed is still open (D39).
- **Prios sized to the picked train,** with a tooltip note when a prio is longer than 2 TL (D34).
- **High-traffic settings give the higher-capacity bridge arrangement:** a doubled mainline bridge, or the connections bridging over the mainline. Low-traffic settings (e.g. sideline-to-sideline hubs) give the simplest and smallest (D32).

## Design outline

1. **Train performance model (pure).**
   - The inputs come from the picked engine and wagons: power, tractive effort, mass (empty or loaded, see open items), max speed, and the air-drag and rolling-resistance coefficients.
   - It reproduces OpenTTD's realistic acceleration for flat track. The source is `GroundVehicle::GetAcceleration` and its callers, which still need tracing.
   - It steps tick by tick, as the game does, and yields distance-against-time from standstill.
   - Travel distance uses the game's step costs: a straight step is 192 progress, a diagonal step 256, so a diagonal piece is ⅔ of a straight tile in time (D36).
   - It handles the original acceleration model, or refuses it (open item).
2. **Prio length.** It's the shortest prio, in tiles, that still keeps the R25 segment spacing, such that a mainline train arriving at line speed when the merge signal clears never has to brake behind the joining train while it accelerates (R27). It's solved by simulation against the performance model and then rounded up to whole tiles. If it's over 2 TL, build it anyway and warn (D34).
3. **Throughput measure.**
   - The minimum headway per route is the time for a train to clear its longest signal block, plus the clearance to the next signal, at the speed it's doing there.
   - A bridge counts as one block (R38). A doubled bridge alternates trains, which halves the effective block.
   - The junction's capacity is the worst route's headway. The traffic level maps to a required headway.
4. **Bridge arrangement variants.**
   - Variants: a doubled mainline bridge (block-signalled, so evil mode recovers, D19/D32), or the connections bridging over the mainline.
   - The generator adds them as design choices. It keeps only variants that meet the required headway, then scores by fewest tiles, then cost (D30).
5. **Tests:**
   - the performance model checked against hand-worked cases and against the game's own acceleration code
   - prio length monotonic in train mass and line speed
   - headway figures for known layouts
   - variant selection switching at the right traffic thresholds

7. **Waiting-bay count** (engineer, 2026-11-07: "The number of waiting bays also has an impact on traffic backing up"):
   - A queue's capacity is whole TL bays (D83). When it overflows, trains back up past the upstream split:
     - on SL joiners, that blocks the other SL movement (the shared inbound line) and then the SL upstream
     - on connections leaving the ML, it reaches the ML split, which is never allowed (D71a)
   - The traffic model sizes each queue's bay count from the traffic level and how long the merge stays blocked (the prio hold time from the headway model). Score the extra footprint and cost against the risk of backing up.
   - 1b uses fixed minimums: 1 bay on SL joiners, and enough on ML-leaving connections to satisfy G7.

## Steps (outline)

1. **Code research:** OpenTTD's acceleration model and its inputs (engine and wagon properties, settings).
2. **Outer test:** "prio for a reference train is the shortest that avoids braking", against a hand-worked reference.
3. **The performance model,** with its inner tests.
4. **Prio length solver,** wired into the 1b generator.
5. **Headway measure,** with its inner tests.
6. **Bridge arrangement variants** in the generator, plus traffic-level selection.
7. **Train picker and traffic control** in the window.
8. **Manual checks in game:** run the reference train through the generated junction at the chosen traffic level, and confirm mainline trains don't brake for joiners.

## Open items

1. **D39, settled by D84:** Low/Medium/High presets, each a throughput bracket. Still open: the bracket values (trains per hour per movement), to be measured with the headway model and rig runs.
2. **Loaded or empty train mass** for the acceleration run. Loaded is the conservative choice (slower acceleration), which gives longer prios.
3. **Which mainline speed:** the rail type's max, the picked train's max, or the lower of the two.
4. **Original acceleration model:** support it (simpler maths, no curve limits) or require realistic acceleration.
5. **Whether the train picker lists only engines available now,** or all engines for the rail type.
6. **Headway definition details:** signal sighting and braking distance. Whether to model braking or use block occupancy only.
