# Art pipeline (Blender to the PS2)

How ShooterGame's art is made: low-poly and textured in the style of Counter-Strike 1.6, built in Blender by scripts,
exported to glTF, imported by LeonCook and cooked for the PS2 ([ps2-shipping](PLANS/ps2-shipping.md) N26; N27 made
the characters, arms and weapons to this spec, N28 rebuilt de_leon, N30f added the physical materials and the
sounds; de_harbor, the second map, was made after 0.25.0 to the same spec without anything of de_leon's). The decisions behind it: D5 (LeonCook bakes the lighting, not Blender), D6 (the Blender scripts are the source
of truth) and D11 (glTF is the only mesh, skeletal mesh and animation format).

Code: `Game/ShooterGame/SourceArt/leon_art.py` (the shared Blender helpers), `check_art_determinism.py`,
`Samples/make_art_samples.py`, `Characters/make_characters.py` (the skeleton and its clips), `make_cs16_characters.py`
(the bodies) and `make_arms.py` (with `anim_body.py` and `anim_arms.py`), `Weapons/make_weapons.py`, `Maps/make_de_leon.py`, `Maps/make_de_harbor.py`, `Sounds/make_sounds.py`
and `Sky/make_sky.py` (no Blender); the import in [ASSET_FORMATS.md](ASSET_FORMATS.md#skeletal-meshes-and-animations--gltf-import)
and [LEVELS.md](LEVELS.md#importing-a-map-from-gltf); the commandlets in [TOOLS.md](TOOLS.md#importlistini).

## Workflow

1. **Explore in Blender with the MCP.** An MCP client drives an interactive Blender; every MCP call is `bpy` code.
   Nothing made this way is kept as it is.
2. **Consolidate into a script (D6).** The steps that survive go into `make_<asset>.py` next to the asset's output
   (`SourceArt/<folder mirroring the package path>/`), on `leon_art` (below). Whatever was done by hand in the
   interactive session is written again as code; the `.blend` the script saves is for looking and trying, never the
   source.
3. **Run it headless** (the output next to the script; `-- --out <folder>` writes elsewhere):

   ```bat
   "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup ^
       --python Game\ShooterGame\SourceArt\Characters\make_characters.py
   ```

   The helpers fail the script when it breaks a budget (`check_triangles`, a skeleton over 24 bones, a texture side
   that is not a power of two from 8 to 256).
4. **Check it is deterministic**: `python Game\ShooterGame\SourceArt\check_art_determinism.py [script ...]` runs each
   script (default: every `make_*.py` that uses `leon_art`) twice and compares every `.glb` byte for byte between the
   runs and with the committed one; it prints `check_art_determinism: PASSED` or exits 1.
5. **Import it**: a section in `Game/ShooterGame/SourceArt/ImportList.ini`, then `LeonCook ... -run=ImportAssets
   -importlist=...` and `CheckReimport.bat` (gate G5); the PS2 cook enforces its hard budgets (N23:
   `[/Script/LeonEd.CookSettings]`).
6. **License it**: a row in `Game/ShooterGame/SourceArt/LICENSES.md` for every file (below). Commit the script, the
   `.glb` and the `.blend`.

**The MCP** (checked 2026-09-28): Blender Lab's *MCP* extension 1.0.0 (`bl_ext.user_default.mcp`,
[blender.org/lab/mcp-server](https://www.blender.org/lab/mcp-server/), GPL-3.0, Blender 5.1 and later) is installed
in Blender 5.2's user extensions (`%APPDATA%\Blender Foundation\Blender\5.2\extensions\user_default\mcp`) and
enabled in the preferences. It loads under Blender 5.2.0 LTS; its server listens on `localhost:9876` only in an
interactive Blender with *Allow Online Access* on (headless it stays off). The client is a stdio bridge set up for
Cursor (`%USERPROFILE%\.cursor\blender-mcp-bridge`, Cursor's `mcp.json`); Claude Code has no Blender MCP server
configured, so N26 was written with the headless fallback (scripts only). The community `blender-mcp` add-on
(ahujasid) is not installed and is not needed.

## Budgets

The asset classes' targets; the cook's hard limits (N23, `[/Script/LeonEd.CookSettings]`: `MaxMeshTriangles=4096`,
`MaxMeshBones=64`, `MaxTextureSize=256`, `MaxTextureBitsPerPixel=8`) only stop what is far over them.

| Class | Asset | Triangles | Bones | Texture | Texels a metre |
| --- | --- | --- | --- | --- | --- |
| Character (CT, T) | `SK_Body_<Team>` on `SKEL_Body` | 600–1 000 | 23 (the shared skeleton) | one 128 × 128 P8 | 64 |
| First-person arms | `SK_Arms_<Team>` on `SKEL_Arms` | 400–600 (both arms) | 11 | one 128 × 128 P8 | 128 |
| Weapon, world model | `SM_<Weapon>` | 100–300 | — | 64 × 64 P4 | 64 |
| Weapon, view model | `SM_<Weapon>_1P` | 300–600 | — | 128 × 64 or 128 × 128 P8 | 128–256 |
| Prop | `SM_<Prop>` | 12–300 | — | 64 × 64 P4, shared between props | 64 |
| Map section (an N15 cell) | the map's nodes | ≤ 1 500 a cell, ≤ 3 000 in view | — | 64 or 128 tiles, P4 / P8 | 64 |

- **Skins**: at most 2 weights a vertex (the skinned LPS2 v2 vertex has two palette indices) and at most 24 bones in a
  batch's palette; a skeleton of 24 bones or fewer shares one palette in every batch, so a character draws in the
  fewest batches (48 vertices each). `leon_art.clamp_weights` keeps a vertex's two largest weights in the source, so
  Blender shows what the PS2 draws; the import would do the same.
- **Textures**: sides are powers of two from 8 to 256 (`leon_art.paint_image` refuses others), 64 and 128 as a rule.
  P4 holds 16 colours exactly and P8 256; the cook quantizes more by median cut, so author to the palette you want.
  Each material is a section and a texture switch on the GS: one material a character, at most two an asset.
- **Texel density**: the table's texels a metre (0.64 texels a centimetre for 64), so the GS's mip level (N13: from
  texels a centimetre and the pixels a centimetre) is the same across the world. The view models are closer, so
  denser.
- The frame: with this art (ten characters, the view model and de_leon) the PS2 holds 30 fps in PCSX2 (N29: the
  scene 8.4 ms, the VU1 transforming and lighting every batch inside the guard band); no mesh uses LODs yet. The rows
  are in [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md).

## Naming

| What | Name | Notes |
| --- | --- | --- |
| Source files | `<Name>.glb`, `make_<name>.py`, `<name>.blend`, in `SourceArt/<package folder>/` | the import adds the prefix: `AK47.glb` → `SM_AK47`, `Body_CT.glb` → `SK_Body_CT` (`Type=SkeletalMesh`) |
| Assets | `SM_` static mesh, `SK_` skeletal mesh, `SKEL_` skeleton, `A_` animation, `T_` texture, `M_` material | [CODING_STANDARD.md](CODING_STANDARD.md) |
| Blender materials and images | `<Asset>` and `<Asset>_D` | a material becomes `M_<Material>`, an image embedded in the `.glb` `T_<Image>` |
| Bones | UE's mannequin names, lower case, `_l` / `_r` | `hand_r`, `spine_02` |
| Clips (Blender actions) | `<Clip>` on the body, `<Weapon>_<Clip>` on the arms | the glTF animation's name; imported as `A_<Clip>` |
| Notifies | PascalCase, `_L` / `_R` for a side | `Footstep_L`, `MagOut` |
| Sockets | an empty `SOCKET_<Name>` under a mesh node (static mesh) or parented to a bone (skeleton) | the socket `<Name>`; a bone's name is a socket too |
| Collision | `UCX_<MeshNode>_<NN>` boxes, `COL_*` collision-only meshes, `Clip_*` volumes | [LEVELS.md](LEVELS.md#naming-conventions); one box a mesh (a mesh's boxes merge into their bounds) |
| Cells and portals | `VIS_<Cell>` (the cell's volume: a box mesh around the room, axis aligned), `PORTAL_<CellA>_<CellB>` (a quad in the opening between two cells, as big as the opening) | the engine's import rules (N15, [ASSET_FORMATS.md](ASSET_FORMATS.md#cells-and-portals)): the renderer draws what the cells the view sees through the portals hold; a map without cells draws everything. Cells may overlap a little at their walls; a prop in two cells is drawn when either is seen; at most 64 cells |
| Lights | `Sun` (the directional light), `Light_<Place>_<NN>` (point lights) | KHR_lights_punctual, exported RAW; every light is imported whatever its name and bakes (N22) |
| Gameplay nodes | `PlayerStart_<Team>`, `BombSite_<A\|B>`, `BuyZone_<Team>`, `NavWaypoint_<Name>` | [LEVELS.md](LEVELS.md#naming-conventions) |

The game's sockets: `Muzzle` on each weapon (`AShooterWeapon::MuzzleSocketName`, a `SOCKET_Muzzle` at the muzzle) and
**`Weapon_R`** on `hand_r` of the body and of the arms (`AShooterCharacter::WeaponSocketName`), where the weapon's
models go (`Mesh3P` / `Mesh1P`; UE's mannequins use a `weapon_r` socket for the same; N25's `Grip` is gone). Its
frame (`leon_art.hand_socket_frame`): at the palm, x along the hand (wrist to knuckles), z to the thumb's side; a
weapon model has its grip at the origin, its barrel along +X and its top along +Z, so it sits in the fist as it is.

## Skeletons

Both are in `leon_art` (`HUMANOID_BONES`, `ARMS_BONES`: name, parent, head, tail in engine metres) and built by
`leon_art.build_armature`, parents first, no roll. The engine's axes: X forward, Y right, Z up; Blender's (x, −y, z).

**`SKEL_Body`**, the CT and T bodies' shared skeleton: 23 bones, standing on the origin (the capsule's feet), facing
+X, 1.83 m tall, T pose (the palms down, the thumbs forward); the socket `Weapon_R` on `hand_r`. Both teams' meshes
and every third-person clip use it; the import makes it as `SKEL_Body` with the first body (`NewSkeletonName`).

```text
root
└ pelvis (0.95 m)
  ├ spine_01 ─ spine_02 ─ spine_03
  │   ├ neck_01 ─ head (to 1.83 m)
  │   ├ clavicle_l ─ upperarm_l ─ lowerarm_l ─ hand_l
  │   └ clavicle_r ─ upperarm_r ─ lowerarm_r ─ hand_r (SOCKET_Weapon_R)
  ├ thigh_l ─ calf_l ─ foot_l ─ ball_l
  └ thigh_r ─ calf_r ─ foot_r ─ ball_r
```

**`SKEL_Arms`**, the first-person arms: 11 bones, the origin at the camera (the eye) looking along +X; the socket
`Weapon_R` on `hand_r` (the thumbs up at rest). One skeleton for every weapon and both teams' arms; each weapon has
its own clips.

```text
root
├ upperarm_l ─ lowerarm_l ─ hand_l ─ thumb_01_l, fingers_01_l
└ upperarm_r ─ lowerarm_r ─ hand_r (SOCKET_Weapon_R) ─ thumb_01_r, fingers_01_r
```

## Animations

Every clip is a Blender action keyed at 30 fps (`leon_art.add_action`: LINEAR keys, a bone's quaternions kept on one
hemisphere, its own NLA track), exported sampled at every frame and imported at 30 Hz (`A_<Clip>`). The glTF
animation's extras carry what glTF has no field for, in the shape the import reads
([ASSET_FORMATS.md](ASSET_FORMATS.md#animation-notifies)): `loop` (1 or 0: the import sets the clip's `bLoop`, so a
one-shot holds its last frame) and `notifies`, a list of `{"name": "Footstep_L", "time": 0.333333}` (seconds from
the clip's start) made from the action's pose markers; Blender's ID properties hold the list as groups and the
exporter writes it as JSON.

The clips are posed by code (`leon_art.Rig` / `Pose`): rotations about the armature's axes, forward kinematics
over the rest pose and two-bone IK (`Pose.reach`) that puts a hand's socket on a weapon's grip and the other hand
on its support; `Pose.keys` gives `add_action`'s local keys. A clip is a function of its time, keyed every frame or
every other one; a looping clip ends on its first pose.

**Third person** (`SKEL_Body`, `Characters/anim_body.py`, `/Game/Characters/Animations`). The locomotion, the crouch
and the jump hold a rifle; the stances of the other weapons come from their aim offset (below):

| Clip | Frames | Loop | Notifies | Use |
| --- | --- | --- | --- | --- |
| `Idle`, `Crouch_Idle` | 60 | yes | — | `BS_Locomotion` / `BS_Crouch` at speed 0 |
| `Walk_F`, `Walk_B`, `Walk_L`, `Walk_R` | 20 | yes | `Footstep_L`, `Footstep_R` | `BS_Locomotion` at 330 cm/s (CS's walk) |
| `Run_F`, `Run_B`, `Run_L`, `Run_R` | 16 | yes | `Footstep_L`, `Footstep_R` | `BS_Locomotion` at 560 cm/s (a rifle's run) |
| `Crouch_Walk_F`, `_B`, `_L`, `_R` | 30 | yes | — (silent, as in CS) | `BS_Crouch` at 200 cm/s |
| `Jump_Start`, `Jump_Loop`, `Jump_Land` | 6, 20, 8 | no, yes, no | `Land` on `Jump_Land` | the jump states |
| `Aim_<Stance>_<Pitch>`: `Rifle`, `Pistol`, `Grenade` × `Down90`, `Down45`, `Center`, `Up45`, `Up90` | 1 (a pose) | yes | — | `AO_<Stance>` |
| `Fire_Rifle`, `Fire_Pistol` | 8 | no | — | `AM_Fire_*` (upper body; the sniper fires the rifle's) |
| `Reload_Rifle`, `Reload_Pistol`, `Reload_Sniper` | 75, 81, 111 | no | `MagOut`, `MagIn` | `AM_Reload_*` (upper body) |
| `Throw_Grenade` | 20 | no | `Release` | `AM_Throw_Grenade` (upper body) |
| `Plant_C4` | 90 | no | `Plant` | `AM_Plant_C4` (whole body) |
| `Defuse` | 45 | no | — | `AM_Defuse`: kneels in its `Kneel` section, then loops `Work` (frames 15 to 45) |
| `Death_Back`, `Death_Front` | 40 | no | — | `AM_Death_*`: falls on its back (shot from the front) or front, its last frame's `Dead` section loops |

**First person** (`SKEL_Arms`, `Characters/anim_arms.py`, `/Game/Characters/Arms/Animations`; one set per weapon,
`<Weapon>_<Clip>`; the pistols, the rifles and the SMG, the AWP and the grenades share their kind's):

| Clip | Frames | Loop | Notifies |
| --- | --- | --- | --- |
| `Pistol_Idle`, `Rifle_Idle`, `Sniper_Idle`, `Grenade_Idle`, `C4_Idle`, `Knife_Idle` | 60 | yes | — (`BS_<Weapon>_Idle`: the arms' base pose while it is drawn) |
| `Pistol_Draw`, `Rifle_Draw`, `Sniper_Draw`, `Grenade_Draw`, `C4_Draw`, `Knife_Draw` | 30, 30, 38, 15, 15, 30 | no | `Deploy` |
| `Pistol_Fire`, `Rifle_Fire` | 8 | no | — |
| `Sniper_Fire` | 45 | no | `BoltBack`, `BoltForward` |
| `Pistol_Reload`, `Rifle_Reload`, `Sniper_Reload` | 81, 75, 111 | no | `MagOut`, `MagIn` (`BoltBack` on the rifle and the sniper) |
| `Grenade_PullPin`, `Grenade_Throw` | 20, 15 | no | `PinPull`, `Release` |
| `Knife_Slash` | 15 | no | — |
| `C4_Plant` | 90 | no | `Plant` |

The lengths are the game's times, as a reload and a draw last their montage (frames = seconds × 30): the USP's reload
2.7 s and draw 1 s, the AK-47's 2.5 s and 1 s, the AWP's 3.7 s and 1.27 s (CS's 1.25 s is not a whole frame), the HE
grenade's draw 0.5 s. Each clip starts and ends on its weapon's idle pose, so a montage blends in and out of it
without a jump.

## The glTF export

`leon_art.export_glb` exports with `GLTF_OPTIONS`, every option that changes the file named, so the bytes do not
depend on the exporter's defaults or on settings saved in a `.blend`:

- glTF Binary (`.glb`), +Y up, modifiers applied, positions, normals and UVs; no tangents, morph targets, GPU
  instances, Draco or meshopt compression, copyright or cameras;
- materials exported, images embedded in the `.glb` (`AUTO`: PNG), no unused images; a material's physical material
  in its extras (below);
- skins with **2 influences** (`export_influence_nb`), every bone a joint (no leaf bones), the rest pose as the bind
  pose;
- animations from the actions (`ACTIONS` mode, one per NLA track), **sampled at every frame with LINEAR** (the
  import rejects CUBICSPLINE), slid to frame 0, constant channels reduced;
- custom properties (`extras`) and punctual lights only when asked: a map exports both (its waypoints' `links`, its
  lights in the `RAW` lighting mode), a clip its `loop` and `notifies`, and every mesh its materials' `physMaterial`
  (the characters, the arms and the weapons export extras for it since N30f).

glTF has no timestamps or UUIDs, and the same script in the same Blender writes the same bytes: the committed `.glb`
files (the characters, the arms, the weapons, de_leon, de_harbor and the samples) come out identical, twice in a row
(`check_art_determinism.py`). The exporter writes its version in `asset.generator` (`Khronos glTF Blender I/O
v5.2.39`), so the Blender version is pinned (5.2): another version is a new export of everything, checked by eye and
committed together. The `.blend` files are not deterministic (a save writes different bytes every time) and are never
compared.

## Physical materials

What a surface is made of, for the game's steps, impacts and bullets ([ps2-shipping](PLANS/ps2-shipping.md) N30f,
UE's physical materials; [ASSET_FORMATS.md](ASSET_FORMATS.md#physical-materials)), is authored with the material:
`leon_art.make_material(..., surface="Wood")` sets the material's custom property `physMaterial` to the physical
material's package (`leon_art.phys_material`: `/Game/PhysicalMaterials/PM_Wood`), the export writes it in the glTF
material's extras (`{"physMaterial": "/Game/PhysicalMaterials/PM_Wood"}`), and the import sets it as the `M_`
material's `PhysMaterial`: on the material it makes and, the script being the source (D6), on the one it finds again
(a material's other values are kept as they are); a slot whose material names none keeps what it has, and one that
does not exist is an error. The `PM_` assets are ImportList sections (`Type=PhysicalMaterial`, `SurfaceType=Wood`)
listed before the meshes; the surface types are named in the project's `DefaultEngine.ini`
(`[/Script/Engine.PhysicsSettings]`). ShooterGame's, CS's texture types:

| Surface | Materials |
| --- | --- |
| Concrete | de_leon's `Sandstone`, `Trim`, `Signs`; de_harbor's `Asphalt`, `Concrete`, `Panel` |
| Dirt | de_leon's `Sand`; de_harbor's `Water` |
| Tile | de_leon's `Paving` |
| Wood | de_leon's `Wood`, `Crate`; de_harbor's `Pallet` |
| Metal | de_leon's `Lamp`; de_harbor's `Corrugated`, `Girder`, `Crane`, `Container*`, `Shutter`, `Signs`, `Lamp`, `Bollard`, `Hull`, `ShipWhite`; every weapon but the C4 |
| Computer | the C4 |
| Flesh | the bodies and the arms |
| Glass | none yet |

A collision-only mesh (`COL_`) takes the physical material of its material, as a `UCX_` box takes its mesh's: de_leon's
floor slabs are dirt (the sand) and tile (the paving).

## Textures

- **Painted or generated in Blender**: `leon_art.paint_image(name, width, height, painter)` fills a packed sRGB image
  texel by texel from code (a palette and a pattern: planks, camouflage, a face), deterministic and exactly the
  colours given; texture painting in the interactive Blender is for exploring, and what is kept is written as code,
  or saved as a PNG in `SourceArt` (a source file with its own license row) and loaded by the script.
- **Baked in Blender** from a high-poly model or procedural nodes into the low-poly's UVs: albedo only. Light, shadows
  and ambient occlusion are baked by LeonCook into the vertices (D5, [LEVELS.md](LEVELS.md#static-lighting)); a
  texture with light in it would be lit twice.
- **Palettes**: the cook makes every texture P4 or P8 with its mips (N13); a texture authored within 16 or 256 colours
  keeps them exactly. `leon_art.count_colours` checks it in the script.
- Colour is sRGB; alpha only for cut-outs (the GS's alpha test, N8).

## Licenses: CC0 only

Only CC0 (public domain) art enters ShooterGame, but for one exception awaiting a decision: the bodies are Counter-Strike
1.6's player models (Valve's), provided by the user, and are not CC0 (LICENSES.md). Every source file has a row in
`Game/ShooterGame/SourceArt/LICENSES.md` with its origin: made by a script of this repository, or a CC0 download
(its URL and author; the downloaded file is committed in `SourceArt`, never fetched at build time). Art whose license
is unclear stays out, whatever its quality (the removed ThirdPerson template's Mixamo animations, for example).

## The characters, arms and weapons

[ps2-shipping](PLANS/ps2-shipping.md) N27, low-poly and textured in the style of Counter-Strike 1.6, every file made
by a script with `leon_art`: the arms and the weapons modelled and painted texel by texel by code; the bodies are
Counter-Strike 1.6's own player models, provided by the user and rigged to `SKEL_Body` by a script (below; not CC0:
LICENSES.md):

| Asset | Script | Triangles | Texture | Notes |
| --- | --- | --- | --- | --- |
| `SK_Body_CT` | `Characters/make_cs16_characters.py` | 752 | 128 × 128, P8 | CS 1.6's SAS (`CS16/sas/cs_sas.fbx`): black fatigues, a vest, a gas mask with its goggles |
| `SK_Body_T` | same | 752 | 128 × 128, P8 | CS 1.6's Leet Krew (`CS16/leet/cs_leet.fbx`): an olive jacket over brown trousers, sunglasses |
| `SK_Arms_CT` | `Characters/make_arms.py` | 542 | 128 × 128, P8 | the camouflage sleeves to the wrist, black gloves |
| `SK_Arms_T` | same | 588 | 128 × 128, P8 | the jacket's sleeves rolled up over bare forearms, a watch, brown gloves |
| `SM_<Weapon>` (world) | `Weapons/make_weapons.py` | 116-260 | 64 × 64, P4 | knife 120, Glock 156, USP 156, Deagle 160, AK-47 196, M4A1 216, MP5 180, AWP 260, HE 140, flash 116, smoke 116, C4 140 |
| `SM_<Weapon>_1P` (view) | same | 308-556 | 128 × 64, P8 | knife 308, Glock 384, USP 356, Deagle 328, AK-47 488, M4A1 448, MP5 364, AWP 520, HE 376, flash 336, smoke 336, C4 556 |

- **The bodies** (`make_cs16_characters.py`): each FBX (a GoldSrc model: a Valve biped, `Bip01 ...` and its helper
  bones, one bone a vertex) is imported, turned to face +X and scaled so its shoulders are at `SKEL_Body`'s 1.45 m (the
  SAS 1.83 m tall, the Leet 1.78 m). Its own skin weights are carried over: every biped bone and helper goes to the
  `SKEL_Body` bone it moves with (the fingers and twists to their hand or limb, the spine's lower bone split by height
  between `spine_01` and `spine_02`), or half and half to the two it sits between (the shoulders', elbows', wrists',
  hips', knees' and ankles' helpers): at most two weights a vertex. The models already rest in a T pose with the palms
  down, so the bind pose is `SKEL_Body`'s rest by moving only the limbs: each arm and leg bone of the model is turned
  and stretched along itself onto its `SKEL_Body` bone (the legs, which stand apart, come together and straight), the
  hands and feet only moved (the boots keep their shape, their soles on the floor), the torso and the head as they
  are. The 512 × 512 skin is averaged 4 × 4 to 128 × 128 (the cook makes it P8: about 2 000 and 9 000 colours before);
  the texture has no free block, so the few chrome triangles (GoldSrc's reflection-mapped goggles and sunglasses)
  take the skin's texel nearest the chrome texture's average colour: one material and one texture a body, the same
  VRAM as the painted bodies. The C4 backpack and the defuse kit (body groups CS draws only on their carrier) and
  their textures are left out. The script renders nothing; the poses were checked in Blender and in the game (idle,
  run, crouch, aim up and down, death, the rifle on `Weapon_R`).
- **Modelling** (`leon_art.MeshBuilder`, the arms and the weapons): a mesh made part by part in the engine's axes:
  lofts of rings along a limb (smooth), boxes, cylinders and chamfered prisms (flat), each part mapping a region of the
  texture atlas and weighting its vertices to one bone or two (a joint's ring half and half). Every face turns to face
  out by itself. The weapons' first-person models add the small parts (triggers, sights, rails, the pins' rings) and
  more sides to the cylinders.
- **The stances** (`anim_body.STANCES`): the body's locomotion holds a rifle; `AO_Rifle`, `AO_Pistol` and
  `AO_Grenade` pose their stance at five pitches and are all measured from the rifle's straight-ahead pose
  (`BasePose=A_Aim_Rifle_Center`), so a pistol's aim offset also turns the rifle's arms into the pistol's (UE: an aim
  offset per weapon over one locomotion). The upper body montages of a pistol or a grenade are stored in the rifle's
  space (for each upper-body bone: rifle centre × stance centre⁻¹ × pose) so that the stance's aim offset on top gives
  the intended pose. The whole-body montages (plant, defuse, death) play without an aim offset.
- **The weapons** of the game: every model is a weapon's since ps2-shipping N30a / N30b (the knife, the Glock, the
  USP, the Desert Eagle, the MP5, the AK-47, the M4A1, the AWP, the three grenades) and the bomb's (the C4); the
  pistols share the pistol's clips, the MP5 and the rifles the rifle's, the grenades the grenade's (`/Game/Weapons` is
  cooked whole).
- **UVs**: the scripts write Blender's (v up from the image's bottom row); glTF turns them over and the import turns
  them back (v = 0 is the bottom, as the textures are stored:
  [ASSET_FORMATS.md](ASSET_FORMATS.md#skeletal-meshes-and-animations--gltf-import)).
- Each weapon model is exported from a scene of its own, so its socket keeps the name `SOCKET_Muzzle` (Blender renames
  a second object `SOCKET_Muzzle.001`).

## The map: de_leon

[ps2-shipping](PLANS/ps2-shipping.md) N28 rebuilt de_leon as a desert town in the style of CS 1.6's de_dust
(`Maps/make_de_leon.py`; the plan and the nodes in [LEVELS.md](LEVELS.md#worked-example-de_leon)):

| Texture | Size | Colours | A repeat | On |
| --- | --- | --- | --- | --- |
| `T_Sandstone_D` | 128 × 128, P8 | 30 | 2 m | the walls and houses: ashlar blocks 1 × 0.5 m in staggered courses, mortar, a lit top edge |
| `T_Trim_D` | 64 × 64, P4 | 5 | 1 m | the caps of the walls, the lintels, the arch, the tunnel's roof |
| `T_Sand_D` | 128 × 128, P4 | 14 | 2 m | the T side, mid and the longs: packed sand, redder drifts, pebbles |
| `T_Paving_D` | 128 × 128, P4 | 16 | 2 m | the CT spawn and the sites: 0.5 m slabs, grout |
| `T_Wood_D` | 64 × 128, P4 | 10 | 1 × 2 m | the mid doors' wings, the ladders, the tunnel's ceiling |
| `T_Crate_D` | 64 × 64, P4 | 10 | a face | CS's crate (`SM_Crate` 1.1 m, `SM_CrateBig` 1.6 m) |
| `T_Signs_D` | 128 × 64, P4 | 5 | a sign | the letters A and B painted on the site walls |

- **Pieces**: every wall, house, floor slab, lintel and prop is a mesh node of its own (`<Cell>_<Piece>`), its
  vertices in the world's axes around its centre, with one `UCX_` box: Leon's physics merges a mesh's boxes into one,
  so a node that needs two boxes is two nodes (an arch is two pillars and a lintel). The crates share their meshes.
- **Faces for the bake**: the faces are cut on a 3 m world grid, the walls again 1.2 m up (the occlusion at their
  foot); the grid is the world's, so the pieces' vertices meet. The faces nobody sees are left out. 2 244 triangles
  (N22's blockout: 1 070).
- **The ground**: the floor slabs' own boxes (their tops at Z = 0) are what the players walk on, with the slabs'
  materials (N29): a capsule walks across a seam between two of them (their tops differ by a hair in float).
- **Surfaces**: each material names its physical material ([Physical materials](#physical-materials)).
- **Cells** (`VIS_`, 7: box meshes up to 6 m) and **portals** (`PORTAL_`, 24: quads), N15's conventions (the
  fixture `MakeCellsFixture.py`); several quads between two cells are Blender copies (`PORTAL_Mid_LongA.001`). The
  portals are the doorways and lanes and the sky over every wall on a boundary from 3.5 m up (the lowest such wall's
  top). de_leon's walls are low, so a view across the map keeps most cells: in Win64, facing a wall at a spawn or a
  site 1 cell is seen and 65-92 objects are left out, looking along mid 6 and 23, from the tunnel 5 and 31, down a
  long or across a spawn all 7. Each cell within 1 500 triangles (the script fails otherwise): `TSpawn` 446, `Mid`
  160, `LongA` 398, `LongB` 458, `CTSpawn` 234, `SiteA` 266, `SiteB` 282.
- **Lights**: the sun (warm, high in the north-west) and three warm point lights (the tunnel's two lamps, the mid
  doors'), exported in the RAW mode and baked by LeonCook (D5); the sky is the world settings'. No fog and no LODs
  (N29: the map fits in the far plane and its largest piece has 146 triangles).

## The map: de_harbor

The second map, an industrial port (`Maps/make_de_harbor.py`; the plan, the cells, the nodes and the textures in
[LEVELS.md](LEVELS.md#worked-example-de_harbor)), is made to the same spec as de_leon and shares none of its art: its
own textures painted by its script (asphalt, cast concrete, precast wall panels, corrugated sheet with rust, painted
steel, the crane's yellow, red, blue and green containers, pine pallet crates, water, roller shutters, the site plates;
14 materials, P4 but the corrugated sheet's P8), its own physical materials' choice (the asphalt, the concrete and the
panels concrete, the steel and the containers metal, the pallets wood), its own sky (the coast preset of
`Sky/make_sky.py`, below) and lighting (a late, low sun from the south-west, a cooler sky light and seven lamps). The
script is a copy of de_leon's geometry helpers with this map's textures (a piece of boxes cut on the 3 m grid, the
floor slabs, the plaques, the ladder), plus shared meshes for the props: a container is `SM_Container_<Colour>_<X|Y>`
(its faces cut on its own 3 m grid, so each instance's bake has vertices), stacked by placing a second instance 2.6 m
up. What is wholly above the cells' top (the crane's beams and boom) or outside the field (the water, the ship) is in
no cell and always drawn. A mesh must not be flat (the water and the quay's face carry a hidden face for thickness):
the PS2 mesh's quantization of a flat axis is 1 cm (`ShooterGame.Content.MeshQuantization`). 4 216 triangles in 112
pieces, every cell within its 1 500.

## Sounds

`Sounds/make_sounds.py` (Python's standard library, no Blender) synthesizes every ShooterGame sound as a 22 050 Hz
mono 16-bit WAV from seeded noise and sine waves (a fixed LCG: the same bytes on every run and machine), CC0 like the
rest; no recording and no speech. `ImportList.ini` imports each as `/Game/Sounds/S_<Name>` (the cook makes it SPU2
ADPCM at that rate: [ASSET_FORMATS.md](ASSET_FORMATS.md)), and `DefaultGame.ini` names them:

- the weapons' and the bomb's (`Pistol_Fire`, `Rifle_Fire`, `Sniper_Fire`, `Reload`, `Empty`, `Equip`, `Throw`,
  `Explosion`, `Bomb_Beep`, `Bomb_Plant`, `Bomb_Defuse`); the bomb's at `Priority=2`;
- the footsteps per surface, left and right (`Step_<Concrete|Dirt|Tile|Metal|Wood>_L` / `_R`) and the ladder's
  (`Step_Ladder_1` / `_2`), and the bullets' impacts per surface (`Impact_<Surface>`, Glass too) and on a character
  (`Hit_Flesh`, `Hit_Kevlar`, `Hit_Helmet`), all at `Priority=0.5` so they leave voices free for the shots;
- the radio (`Radio_Command`, `Radio_Group`, `Radio_Report`: a squelched tone pattern a menu;
  `Radio_FireInTheHole`, `Radio_BombPlanted`), at `Priority=1.5`.

## The sky

`Sky/make_sky.py` ([ps2-polish](PLANS/ps2-polish.md) P8; Python's standard library, no Blender, about 6 s a sky)
generates the maps' skies, one preset each (`PRESETS`: the desert for de_leon, `Sky/Sky_Desert.hdr`; the coast for
de_harbor, `Sky/Sky_Coast.hdr`, a greyer zenith, a cool haze that climbs higher, a warm glow toward its lower sun, the
sea below the horizon and more clouds from another seed). de_leon's desert sky is a high dynamic range long-lat panorama, `Sky/Sky_Desert.hdr` (1 024 x 512, Radiance RGBE with
run-length encoded scanlines and no date in the header): a deep blue zenith fading to a pale, warm haze at the horizon
(brighter toward the sun), distant sand below it, the sun (a disc 1.5 degrees in radius, 60 times the horizon's
radiance, and its glow) where de_leon's baked sun is (the script reads `SUN_DIRECTION` from `Maps/make_de_leon.py`),
and soft clouds of seeded value noise on a plane over the map, lit from the sun's side. Integer hashes and fixed
formulas only: `check_art_determinism.py` runs it twice with Python and compares the `.hdr` with the committed one.

`ImportList.ini` imports it as the cube map `/Game/Sky/T_Sky_Desert` (`Type=TextureCube`, `UTextureCubeFactory`):
six faces of 128 x 128 (`CubeFaceSize`), each texel four bilinear samples of the panorama along its direction, the
radiance tone-mapped by the ACES filmic curve (Narkowicz's fit) at `ExposureBias=0` and stored as sRGB bytes
([ASSET_FORMATS.md](ASSET_FORMATS.md#cube-maps)); the PS2 cook palettes each face to PSMT8 (23 KB with its mips and CLUT; the ground below the horizon,
6 colours, PSMT4 of 12 KB: 127 KB of the 1 856 KB texture arena). Faces of 256 x 256 look sharper but made de_leon's load
take 1 038 KB of the PS2's 1 024 KB `LoadMapMisc` budget. de_leon's `WorldSettings` node names it
([LEVELS.md](LEVELS.md#the-world-settings)); `/Game/Sky/T_Sky_Coast` is the coast's the same way, named by de_harbor's
(each map loads only its own sky).

## The samples (N26's gate)

`Game/ShooterGame/SourceArt/Samples/make_art_samples.py` builds, with `leon_art` only:

- `ArtSample_Crate.glb`: a 0.6 m crate, 36 triangles, a painted 64 × 64 texture of 5 colours (P4), a 2-bone skin
  (`Base`, `Lid`; the hinge strip weighted half and half), the socket `Top` on the lid and one clip, `Open` (1 s,
  one-shot, the notify `Creak`);
- `ArtSample_Mannequin.glb`: `SKEL_Body`'s 23 bones skinned to a box a bone (264 triangles), the socket `Weapon_R`
  (with its frame since N27) and one clip, `Idle` (2 s, looping).

They are not game content: no ImportList imports them. `check_art_determinism.py` passes on them (and on every
`make_*.py` that uses `leon_art`: the characters, the arms, the weapons, de_leon and de_harbor). N27 exported them again: the
crate's notify in the list shape, the mannequin's socket with its frame. LeonCook imports them into a
scratch project in the ignored `Engine/Saved` (the `.lproj` need not exist):

```bat
Engine\Binaries\Win64\LeonCook.exe Engine\Saved\ArtPipeline\ArtPipeline.lproj -run=ImportAssets ^
    -source=Game/ShooterGame/SourceArt/Samples/ArtSample_Crate.glb -dest=/Game/Samples -type=SkeletalMesh
Engine\Binaries\Win64\LeonCook.exe Engine\Saved\ArtPipeline\ArtPipeline.lproj -run=ImportAssets ^
    -source=Game/ShooterGame/SourceArt/Samples/ArtSample_Crate.glb -dest=/Game/Samples -type=Animation ^
    -Skeleton=/Game/Samples/SKEL_ArtSample_Crate.SKEL_ArtSample_Crate
```

which saves `SK_ArtSample_Crate`, `SKEL_ArtSample_Crate` (with the socket `Top`), `M_ArtSample_Crate`,
`T_ArtSample_Crate_D` and `A_Open`; the mannequin, `SK_`, `SKEL_` (with `Weapon_R`), `M_` and `A_Idle`, with no
error or warning.

## Related docs

[LEVELS.md](LEVELS.md) · [ASSET_FORMATS.md](ASSET_FORMATS.md) · [TOOLS.md](TOOLS.md) ·
[CODING_STANDARD.md](CODING_STANDARD.md) · [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md)
