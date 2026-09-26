# ShooterGame source art licenses

Only CC0 (public domain) art may enter ShooterGame (the plan's rule for the game's content). Every file here is listed
with its origin and license.

| Files | Origin | License |
| --- | --- | --- |
| `Maps/de_leon.blend`, `Maps/de_leon.glb` | Made by `Maps/make_de_leon.py` (this repository) in Blender 5.2 from Blender's primitives: boxes, empties and a sun. No external model, texture or sound | CC0 1.0 |
| `Characters/team_bodies.blend`, `Characters/Body_CT.glb`, `Characters/Body_T.glb` | Made by `Characters/make_team_bodies.py` (this repository) from boxes: the teams' bodies | CC0 1.0 |
| `Weapons/Pistol.glb`, `Weapons/Rifle.glb`, `Weapons/Sniper.glb`, `Weapons/Grenade.glb` | Written by `Weapons/make_weapons.py` (this repository, Python's standard library) from boxes with plain colours | CC0 1.0 |
| `Sounds/*.wav` | Synthesized by `Sounds/make_sounds.py` (this repository, Python's standard library) from seeded noise and sine waves; no recording | CC0 1.0 |

No file here comes from outside this repository: the materials are plain colours (glTF base colour factors), with no
images, and the sounds are generated. Art from elsewhere enters only with a CC0 license and a row here naming its
source URL.

The plan named the Bot model of the removed ThirdPerson template (`ee1cdde^:Templates/ThirdPerson`) for the teams'
bodies. Its FBX files (BreathingIdle, Running, JumpingUp, ...) appear to be Mixamo exports and no CC0 license is
recorded for them, so they cannot enter this project; the teams keep the box bodies above until a CC0 model is found.
