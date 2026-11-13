# Junction tool: tool/ (hover planning, window, ghost drawing)

- `junction_hover`: `PlanJunctionHover` and `PlanJunctionHoverNear`; `PlanJunctionHoverAt` is file-static. `junction_gui` is the window and hover always plans on the graph pipeline; `junction_ghost_draw` draws the outline.

## Hover and GUI
- Topology is chosen in a separate Topology window opened from the junction window; the main window holds a presets dropdown (which only sets the Topology window's choices, "Custom" when none match) and a Topology... button; lanes and gaps use steppers.
- Cursor spot first: the hover plan solves at the cursor; a nearby offset is used only when the cursor spot has no layout, shown as moved.
- The window has a prio length slider from one TL up to the sized length; below the maximum the spec carries `prio_strict = false`.
- Generation on hover is under 100 ms for both topologies (T and LL_RR x L_R); `[realterrainsweep]` requires it of the plan and of the near search over the 48 fixtures.
- The window caches up to 32 hover results per (anchor, rotation, mirror, notations, TL, signal type, SD, train, prio tiles, lane pairing); the cache is cleared when the window's settings change and after a build, and has no terrain hash (`JunctionGenerateKey`).
- The near search checks `cancel` before each offset and returns "cancelled" when set; the GUI passes the job's cancel flag; a superseded hover is cancelled when the cursor moves.
- The near search rejects an offset before placing it when its X solve must refuse it; a tunnel separation counts as a bridge there; the reject never rejects a placeable offset.
- The near search moves the box by at most 8 columns and 4 rows and only when the cursor spot fails.
- Junction generation requires a picked train; the tool does not generate without one; the programmatic interface takes the train's loaded parameters.
- The player picks a train by clicking one of their trains, as the depot's clone tool does; the tool reads power, tractive effort, loaded weight, air drag, max speed and length (TL) from the game's caches.
- The picked train, the prio slider, notations, TL, signal type and lane pairing persist across sessions in the client config.
- Design speed is `min(train, rail type)`.
- Persistent values are client settings (`openttd.cfg`, `gui.junction_*`), never the savegame.
- The window shows a persistent cost line (`Cost: {YELLOW}{CURRENCY_LONG}`), red when unaffordable or invalid.
- Shift-click shows the estimated cost of the whole junction at the clicked tile and never starts building.
- Invalid-placement text is generic ("tiles shown in red can't be used"); only "too close to edge of map" stays specific.
- Rotation (`.`) and mirroring (`,`) are by hotkey.
- Only tiles the layout uses are outlined, while hovering and while building: white for track tiles, blue for tiles that are only terraformed; no full grid is drawn.
- Cost and placement errors appear in the cursor tooltip, not in the window.
- On a spec change the window sizes the prio on a worker thread (`EnsureLibrary`); it builds no legacy library, because the hover plans on the projection pipeline only.
