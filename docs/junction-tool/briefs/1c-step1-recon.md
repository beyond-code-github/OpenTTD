# Brief 1c step 1: recon of train acceleration, signals and the rig's traffic tools (read-only)

You execute this brief and nothing else. Do not edit any source file. Write exactly one file, `docs/junction-tool/RECON-1c.md`, and commit only that. If something is not covered, stop and report the question. Keep your context small: read with offset and limit, search with scoped grep, and don't dump whole files.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Plan:** `docs/junction-tool/PLAN-1c.md` (read it all, 65 lines), step 1 "Code research".
- **Why:** the engineer will judge signalling through simulation ("it will all come out in simulation. It may just be down to waiting bay sizing"). 1c needs two things:
  - a pure train performance model, for prio length and headway;
  - a way to run real traffic through generated junctions on the rig.

## 2. Questions to answer, each with file:line references

### A. Realistic acceleration (`src/ground_vehicle.hpp/.cpp`, `src/train_cmd.cpp`)
1. Trace `GroundVehicle::GetAcceleration` and what calls it each tick for trains. Give the formula in plain maths:
   - tractive force from power and speed, capped by max tractive effort;
   - air drag;
   - rolling resistance;
   - slope force;
   - the curve speed limit's effect;
   - the conversion to the speed change per tick.
   Name every input and where it comes from: `GetGroundVehicleCache`, `cached_power`, `cached_max_te`, `cached_air_drag`, the mass sum, and so on.
2. How mass depends on cargo load: loaded versus empty, and `freight_trains` multiplier.
3. How `progress` and subspeed turn speed into distance: the 192 per straight step and 128 per diagonal step (or whatever the code says), and how many movement steps per tick at a given speed. Quote the advance loop (`Train::UpdateSpeed` / `TrainLocoHandler`, `GetAdvanceDistance`, `GetAdvanceSpeed`).
4. Braking: how a train slows for a red signal or a speed limit (realistic model). Is there a modelled braking distance, or does it just decelerate when blocked? Which settings affect it?
5. The original acceleration model: what differs, in one paragraph.
6. Which `Engine`/`RailVehicleInfo` fields and settings (`train_acceleration_model`, `train_slope_steepness`, `max_train_length`, `freight_trains`) a pure model would need as inputs.

### B. Signals and blocks
7. How a train treats a block signal versus a path signal when the next block is occupied: where it stops (at the signal tile edge?), and when it may start again (the tick after the block clears?).
8. Whether the YAPF reservation for path signals affects headway: does a train reserve ahead through several blocks?

### C. The rig's traffic tools
9. In `tools/ottd/` (README, `ottd`, `ottd_core/`, and the game script, if there is one): what exists today for running trains? That covers `junction-check` (circuit), `sample` (speed drops), vehicle creation and orders. List each command with what it measures.
10. What's missing for a traffic test of a generated junction:
    - N trains per movement on a loop;
    - measuring ML trains braking for joiners;
    - queue lengths at merges;
    - trains backing up past a split.
    Propose the smallest rig additions, as a list. Don't build them.

## 3. Output

`docs/junction-tool/RECON-1c.md` with sections A, B, C. Each answer is short and has file:line refs. End with a section "Open questions for the engineer". Add only questions the code can't answer; take PLAN-1c's open items 2–6 as the starting list and refine them with what you found.

Commit message: `Doc: Junction tool 1c recon`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report

- commit
- a 10-line summary of the most important findings
- `git status --porcelain`

When you have reported, stop.
