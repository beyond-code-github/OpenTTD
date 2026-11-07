# Making blank.scn

`ottd start` uses `tools/ottd/scenarios/blank.scn` by default when it exists (`ottd start --scenario PATH` uses another file). Make it by hand in the OpenTTD GUI:

1. Main menu, Scenario Editor, set map size 128 x 128, choose "Flat land".
2. Leave the land flat (no terraforming).
3. Open the Town Directory (Towns menu). For each town: click its name to open the town window and press its "Delete" button (scenario editor only). Repeat until the directory is empty.
4. Build no industries (leave the Industry Generation window alone).
5. File, Save Scenario, name it `blank`. Copy `~/.local/share/openttd/scenario/blank.scn` to `tools/ottd/scenarios/blank.scn`.
