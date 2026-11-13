# Junction tool: terrain/ (snapshot, facts, terraform, foundations)

- The terrain snapshot is `junction_terrain.h`; `junction_terrain_json` is console only (D112).

## Terrain facts
- Only hard facts fail a solve: houses or objects kept, water, the map edge, unpurchasable tiles; heights cost instead of failing (D218).
- Blocked tile classes: houses, other companies' property, unclearable objects; own track is blocked for track and Spannable for bridges; water is allowed only under spans; trees and rough land cost their clearing price (D93, D104).

## Foundations and terraform
- Terraform is allowed by default and costed with the game's price per moved corner, shared corners counted once (D218, D95).
- The levelled foundation is offered only where `GetRailFoundation` says Leveled (HM1).
- Terraform steps are the game's: simulated corner by corner they reach exactly the plan's corners (H2, HC1).
- Rail foundations and slopes: `GetRailFoundation` (a piece along an incline is a ramp, D152b); `build_on_slopes` setting (D152b).
