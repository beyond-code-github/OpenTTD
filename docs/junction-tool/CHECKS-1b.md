# Manual checks for brief 1b step E (engineer, step F)

Use the Release build, `build-rel/openttd`. Generation takes about 0.5 s there, against about 5 s per notation or TL change in the Debug `build/openttd`. Run the game, start a new game, and open rail toolbar -> Junction.

1. The window shows both notation boxes (`L_R`), TL 4 and a size/cost. Hover shows the generated doubled-bridge T ghost, about 25x15.
2. Changing TL to 6 regenerates, and the ghost changes. Typing `LR` in Mainline shows the "needs a gap" error, with no ghost.
3. Typing `LL_RR` shows the multi-lane error. Typing `x` shows the notation error.
4. Rotate and mirror still work. Click-build builds the junction, including Entry/Combo/Exit prio signals.
5. With road side set to left in a new game, the built layout is the mirror image.
6. Close and reopen the window: the notations and TL persist. Save and load: there is no savegame difference (D69).
7. The rig's junction-check passed: `tools/ottd/ottd junction-check tools/ottd/layouts/circuit-LR-TL4.json --find-clear` (circuit from `[.circuit]`) exits 0 (verdict OK).

## D78 T4

1. Hovering over flat land shows "Working out a junction for this spot…", then the ghost within about a second, anchored with the hub at the cursor.
2. Hovering over hills or water shows an adapted layout or "No junction fits here".
3. Moving the mouse around causes no freeze.
4. Rotate and mirror re-generate for the new orientation.
5. The building matches the ghost.
6. With the left-hand road side, the junction is mirrored correctly.
7. After a build, an information message with spent and quoted appears and stays while the cursor moves.
