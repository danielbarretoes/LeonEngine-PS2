# Maps (`.lmap`)

A map is a `.lmap` package that holds a world: its `UWorld` (the map's asset, named after the package), the world's
persistent level, the level's `AWorldSettings` and the actors with their components (plan decision D13,
`PKG_ContainsMap`), as UE's `.umap`. `UEngine::LoadMap` opens one; a map is made by importing a glTF scene exported from
Blender (LeonEd's `UGLTFMapFactory`, [below](#importing-a-map-from-gltf)) or by code that builds a world and saves it.
The PS2 runtime loads no map yet: the ThirdPerson demo builds its level in code (`FThirdPersonLevel`).

Code: `Engine/Source/Runtime/Engine/Classes/Engine/World.h`, `Level.h`, the actor and component classes in
`Engine/Source/Runtime/Engine/Classes/{Engine,GameFramework,Camera,Components,AI}/`,
`Engine/Source/Runtime/Engine/Private/UnrealEngine.cpp` (`UEngine::LoadMap`), `Engine/Source/Editor/LeonEd`
(`UGLTFMapFactory`, `UMapImportSettings`), `Engine/Source/Developer/MeshUtilities/Public/GltfScene.h`.
Also: [ASSET_FORMATS.md](ASSET_FORMATS.md#maps--lmap) (what a map package saves) · [TOOLS.md](TOOLS.md) (LeonCook)
· [ARCHITECTURE.md](ARCHITECTURE.md) · [SETUP.md](SETUP.md#leongame).

## Types

| Type | Header | Role |
| --- | --- | --- |
| `UWorld` | `Classes/Engine/World.h` | The map's asset: its persistent level, the physics scene and the navigation system; `FindWorldInPackage`, `InitWorld`, `UpdateWorldComponents`, `InitializeActorsForPlay`; an imported map's editor-only `AssetImportData` |
| `ULevel` | `Classes/Engine/Level.h` | The world's persistent level (`UWorld::PersistentLevel`): `Actors` in spawn order, the world settings first; `PostLoad` reconnects it to its world |
| `AWorldSettings` | `Classes/GameFramework/WorldSettings.h` | The level's settings actor: `DefaultGameMode` (plan decision D18), `KillZ` |
| `AStaticMeshActor` | `Classes/Engine/StaticMeshActor.h` | A placed mesh: its `UStaticMeshComponent` root (mesh, override materials, mobility, collision) |
| `APlayerStart` | `Classes/GameFramework/PlayerStart.h` | Where players spawn; `PlayerStartTag` carries the game's meaning (`CT`, `T`) |
| `ATargetPoint` | `Classes/Engine/TargetPoint.h` | A named point: a transform and `Tags` |
| `ABlockingVolume`, `ATriggerVolume`, `APainCausingVolume` | `Classes/Engine/`, `Classes/GameFramework/` | Boxes (plan decision D16): an invisible wall, a region gameplay code tests (`Tags`: `BombSite` + `A`), a damaging region |
| `ADirectionalLight`, `APointLight` | `Classes/Engine/` | The lights (colour, intensity, shadows, source angle / attenuation radius) |
| `ACameraActor` | `Classes/Camera/CameraActor.h` | A placed camera: its `UCameraComponent` keeps an orbit or free-look view (the legacy levels' framing, P15) |
| `ANavigationWaypoint` | `Classes/AI/Navigation/NavigationWaypoint.h` | A point of the map's waypoint graph: `Links` (one way) and `Flags`; `UNavigationSystem` builds its graph from them when the world begins play (P20) |
| `URotatingMovementComponent` | `Classes/GameFramework/RotatingMovementComponent.h` | UE's: turns its component at `RotationRate` (degrees per second), optionally about `PivotTranslation` |
| `UBobbingMovementComponent` | `Classes/GameFramework/BobbingMovementComponent.h` | Leon: sets the component's height to `BaseZ + Amplitude * (0.5 + 0.5 * sin(Speed * t))` |
| `UOrbitMovementComponent` | `Classes/GameFramework/OrbitMovementComponent.h` | Leon: moves the component on a circle around the world's vertical axis (`Radius`, `Height`, `HeightAmplitude`, `Speed`) |
| `UInteractableComponent` | `Classes/Components/InteractableComponent.h` | Leon: what a player can do at a trigger volume (`InteractRadius`, `InteractCost`, the game-defined `Payload`, `bConsumeOnUse`), read by `VolumeHelpers` |

The movement components tick with their actor: a map's spinning, bobbing or orbiting actor moves as the world ticks.
The last three have no UE counterpart; they hold what the legacy levels stored ([Engine maps](#engine-maps)).

## Opening a map

```text
UEngine::LoadMap(URL)                                     (UEngine::Browse; the startup map; `open <map>`)
  ├─ the map: a long package name (/Game/Maps/X, /Engine/Maps/X) whose .lmap exists, or a .lmap file
  │     a file outside every mount point mounts its content folder, the folder above its Maps/ folder
  │     (<Root>/Maps/X.lmap mounts <Root> as /<Root name>/); a missing map fails and keeps the current world
  ├─ the players leave their controllers; the old world ends play (LevelTransition), is destroyed, garbage collected
  ├─ LoadPackage, UWorld::FindWorldInPackage: the map's world, rooted
  ├─ UWorld::InitWorld              the level knows its world, the actors get their IDs in their saved order
  │                                 (the renderer's draw order), the renderer's scene unless -nullrhi
  ├─ UWorld::SetGameMode            ?game= in the URL, AWorldSettings::DefaultGameMode, GlobalDefaultGameMode (D18)
  ├─ UWorld::InitializeActorsForPlay
  │     ├─ UpdateWorldComponents    every component registers: bodies in the physics scene, proxies in the scene
  │     ├─ AGameModeBase::InitGame
  │     └─ each actor initializes   (UE: ULevel::RouteActorInitialize)
  ├─ every local player logs in     the game mode spawns its controller and its default pawn at a player start:
  │                                 the one whose PlayerStartTag the URL's #portal names, else the level's first
  └─ UWorld::BeginPlay, UGameInstance::LoadComplete
```

`AGameModeBase::ChoosePlayerStart` takes the level's first player start (UE picks a random free one; Leon is
deterministic). The engine's template maps have the view their legacy level opened with as their first start, so the
default pawn (`ADefaultPawn`, a free-flying camera) starts where it always did.

## Saving a map

A tool makes the world in the map's package and saves it with a `.lmap` file name:

```cpp
UPackage* Package = CreatePackage(TEXT("/Game/Maps/Arena"));
// Built and saved, never drawn: no renderer's scene.
const UWorld::InitializationValues IVS = UWorld::InitializationValues().InitializeScenes(false);
UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, TEXT("Arena"), Package, true, &IVS);
World->PersistentLevel->SetWorldSettings(World->SpawnActor<AWorldSettings>()); // the first actor, as in UE
World->SpawnActor<APlayerStart>(FVector(0.0f, 0.0f, 92.0f), FRotator::ZeroRotator);
UPackage::SavePackage(Package, World, RF_Public | RF_Standalone,
	*FPackageName::LongPackageNameToFilename(TEXT("/Game/Maps/Arena"), FPackageName::GetMapPackageExtension()));
```

LeonEd's `FAssetImportUtils::SavePackage` picks `.lmap` for a package that holds a world. The save is deterministic
(the same world saves the same bytes; a loaded map resaves byte for byte), transient actors are left out, and every
transform comes back bit for bit (a scene component saves its relative quaternion).

<a id="importing-a-map-from-gltf"></a>

## Importing a map from glTF

Maps are built in Blender, exported to glTF 2.0 and imported as maps by LeonCook:

```bat
Engine\Binaries\Win64\LeonCook.exe Game\MyGame\MyGame.lproj -run=ImportAssets -type=Map ^
    "-source=Game/MyGame/SourceArt/Maps/de_leon.glb" -dest=/Game/Maps/de_leon
```

or as a section of the project's `SourceArt/ImportList.ini` (`Source=Maps/de_leon.glb`, `Dest=/Game/Maps/de_leon`,
`Type=Map`). `-dest` is the map's package (UE's map path): the map is `Content/Maps/de_leon.lmap`, each glTF mesh a node
shows one `SM_<Mesh>` in `/Game/Maps/de_leon/Meshes` (a mesh several nodes show is one asset), each PBR material an
`M_<Material>` in `/Game/Maps/de_leon/Materials` with its external base colour and normal images as `T_` textures there.

### From Blender

- Model in metres with Blender's axes. Blender and the engine are both Z up with the same +X; Blender is right-handed
  and the engine left-handed, so Blender's +Y is the engine's −Y (the exporter and the importer do this for you:
  glTF is +Y up, and `FImportCoordinateConversion` turns glTF (x, y, z) m into the engine's (x, z, y) × 100 cm).
- Export **glTF 2.0**: *glTF Binary (.glb)*, or *glTF Separate* when materials have textures (the importer reads
  external images only, not images embedded in a `.glb`); *+Y Up* on (the default); *Include › Custom Properties* on
  for the waypoints' `links` / `flags`; *Include › Punctual Lights* on; apply the modifiers.
- Name the objects after the conventions below; Blender's copy numbers (`BuyZone_T.001`) are fine.
- The glTF source goes to the project's `SourceArt/Maps/` next to the `.blend`, both under version control; the
  imported map records it (its world's `AssetImportData`), so `-reimport` rebuilds the map from it.

| Engine (cm) | glTF / Blender |
| --- | --- |
| +X forward | glTF +X, Blender +X |
| +Y right | glTF +Z, Blender −Y |
| +Z up | glTF +Y, Blender +Z |
| 100 | 1 m |

### Naming conventions

A node takes the rule of `[/Script/LeonEd.MapImportSettings]` (the Editor config) with the longest prefix its name
starts with; the engine's rules are in `Engine/Config/BaseEditor.ini`, a project's own in its `Config/DefaultEditor.ini`:

| Node | Becomes | Rule |
| --- | --- | --- |
| a mesh no rule matches | `AStaticMeshActor` at the node's transform, static and colliding (the triangles) | — |
| `UCX_<MeshNode>_<NN>` | convex collision of the mesh node named `<MeshNode>`: the bounding box of the UCX mesh, in the render mesh's space, becomes a box of that mesh's simple collision (`UBodySetup` box elements, `CTF_UseSimpleAsComplex`); no actor | engine, `Kind=ConvexCollision` |
| `COL_*` | `AStaticMeshActor`, hidden in game, colliding | engine, `Kind=CollisionOnly` |
| `Clip_*` | `ABlockingVolume` | engine |
| `PlayerStart*` | `APlayerStart` upright, facing the node's +X; the name's suffix is its `PlayerStartTag` (`PlayerStart_CT`: `CT`) | engine, `bSuffixAsTag` |
| `NavWaypoint*` | `ANavigationWaypoint` at the node; the extras `links` (waypoint node names) and `flags` (names), each a JSON array of strings or one comma-separated string (Blender custom properties are strings) | engine |
| `BombSite_A` / `_B`, `BuyZone_CT` / `_T` | `ATriggerVolume`, `Tags` [`BombSite`, `A`] / [`BuyZone`, `CT`] | a project's (ShooterGame, P17), below |
| a KHR_lights_punctual light | `ADirectionalLight`, or `APointLight` for a point or spot light (Leon has no spot light: a warning): the colour, the glTF intensity as `Intensity`, `range` as `AttenuationRadius` (8 m without one); the directional light casts shadows (the renderer shadows the first one) | always, whatever its name |
| a node with no mesh and no rule | nothing (a group; its children are placed with its transform) | — |

A volume is the box of the node's mesh (its bounds in the mesh's space, placed and sized by the node's transform; a
node without a mesh is a 2 m cube, Blender's default cube or cube empty); a volume is axis-aligned whatever its
rotation (D16). Actors are named after their nodes (`.` becomes `_`), the world settings first, then the nodes in the
file's order; a rule may map a prefix to any engine actor class (`ATargetPoint`, `APainCausingVolume`, ...), which
takes the node's transform.

A rule is `(Prefix="...",Kind=Actor|CollisionOnly|ConvexCollision|Ignore,ActorClass=<class path>,Tags=(...),
bSuffixAsTag=True)`. The engine knows no game (plan decision D15): the game's meaning is a project's rules and tags.
ShooterGame's `Config/DefaultEditor.ini` (P17) reads:

```ini
[/Script/LeonEd.MapImportSettings]
+NodeRules=(Prefix="BombSite",ActorClass=/Script/Engine.TriggerVolume,Tags=("BombSite"),bSuffixAsTag=True)
+NodeRules=(Prefix="BuyZone",ActorClass=/Script/Engine.TriggerVolume,Tags=("BuyZone"),bSuffixAsTag=True)
+RequiredTags=TriggerVolume:BombSite+A
+RequiredTags=TriggerVolume:BombSite+B
+RequiredTags=TriggerVolume:BuyZone+CT
+RequiredTags=TriggerVolume:BuyZone+T
+RequiredTags=PlayerStart:CT
+RequiredTags=PlayerStart:T
```

### Required tags

`RequiredTags` is the project's check of its maps: each entry, `[<Class>:]<Tag>[+<Tag>...]`, must be met by some actor
of the map (of that class or a subclass, named without its prefix) that carries every tag, a player start's
`PlayerStartTag` counting as one of its tags. When an entry is not met the import fails, nothing is saved and the log
names it: `GLTFMapFactory: '<file>' has nothing with the required tags 'BombSite+B' (RequiredTags of
[/Script/LeonEd.MapImportSettings])`. The engine's own maps (under `/Engine/`) are not the project's and skip the
check (`UMapImportSettings::AppliesRequiredTags`), so `LeonCook <Project>.lproj -run=ImportAssets -reimport -all`, which
reimports the engine content too, passes with a project that requires tags.

### Waypoint links

With `bAutoLinkWaypoints` (P20; off by default), the import links, after placing the actors, every pair of waypoints
up to `MaxLinkDistance` apart that the project's agent can walk between both ways (`UNavigationSystem::AutoLinkWaypoints`:
the standing capsule, `AgentRadius` × `AgentHeight`, swept between them on the Pawn channel, the floor probed under the
way for gaps; a rise up to `AgentMaxStepHeight` is a step, up to `AgentMaxJumpHeight` a jump), and one way down a drop
higher than a jump and up to `AgentMaxDropHeight`. The agent is the Engine config's `[/Script/Engine.NavigationSystem]`
(`FWaypointLinkParams::FromConfig`), which the world's graph reads too, so the game walks the links the import made.
The links the nodes name are kept; the added ones are saved in the map. ShooterGame's `DefaultEditor.ini` turns it on
and its `DefaultEngine.ini` gives CS's hull (40 cm × 183 cm, a 45 cm step, a 112 cm jump: the character jumps 114 cm,
a 3 m drop, 20 m); de_leon's import adds 26 links to the 18 waypoints' hand-authored ones, the same on every import
(the map's bytes do not change: gate G5).

### Collision

A static mesh actor collides with its mesh's triangles (the physics scene's static triangle bodies), unless its mesh
has `UCX_` boxes, which then answer traces and physics. Leon's physics scene has boxes and triangle meshes, no convex
hulls, and gives a body the bounds of its boxes: a `UCX_` piece is its bounding box, and several pieces merge into one
box (a documented deviation). `COL_` meshes collide with their triangles and are never drawn; `Clip_` volumes are
boxes.

### Reimport

Importing over the map, or `-reimport`, rebuilds its level in place from the source: the old actors leave the package,
the new ones take the node names, the meshes are rebuilt in their packages (their materials are kept, as for any mesh
reimport), and the same file saves the same bytes: gate G5 (`CheckReimport.bat`) covers maps.

### AxisTest

`/Engine/Maps/AxisTest` is the engine's check for mirrored or swapped axes. Its source, `Engine/SourceArt/Maps/AxisTest.glb`,
is written by `Engine/SourceArt/Maps/MakeAxisTest.py` (standard-library Python: no Blender needed, the same bytes on
every run); `Engine/SourceArt/ImportList.ini` imports it:

| Node | glTF (m) | Engine (cm) |
| --- | --- | --- |
| `AxisX_Red`, a red 1 m cube | (3, 0.5, 0) | (300, 0, 50) |
| `AxisY_Green`, a green 1 m cube | (0, 0.5, 2) | (0, 200, 50) |
| `AxisZ_Blue`, a blue 1 m cube | (0, 2.5, 0) | (0, 0, 250) |
| `Marker_1m`, a yellow 20 cm cube | (1, 0, 0) | (100, 0, 0) |
| `Origin_White`, a white 40 cm cube, on a grey 12 m floor | (0, 0.2, 0) | (0, 0, 20) |
| `PlayerStart`, facing +X | (−6, 1.7, 0) | (−600, 0, 170) |
| `Sun`, a directional light shining along +X, +Y and down | | |

`LeonGame /Engine/Maps/AxisTest -AxesGizmo` shows the red cube straight ahead, the green one on the right and the blue
one above, in the colours of the gizmo's X, Y and Z; `System.Engine.AxisTestMap.NoMirroring` checks the positions and
that, in the start's view, +Y is on the right.

## Worked example: de_leon

ShooterGame's map (P17), a 60 × 48 m blockout in the style of a CS defuse map, is built by a script, so the map is
reproducible and its layout reviewable as code:

```bat
:: 1. Build the map in Blender (headless) -> de_leon.blend + de_leon.glb next to the script
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup ^
    --python Game\ShooterGame\SourceArt\Maps\make_de_leon.py

:: 2. Import it (with the other ShooterGame source art) -> Content\Maps\de_leon.lmap, de_leon\Meshes, de_leon\Materials
Engine\Binaries\Win64\LeonCook.exe Game\ShooterGame\ShooterGame.lproj -run=ImportAssets ^
    -importlist=Game/ShooterGame/SourceArt/ImportList.ini

:: 3. Play it (GameDefaultMap of the project)
Game\ShooterGame\Binaries\Win64\ShooterGame.exe -ExecCmds=bot_fill
```

`make_de_leon.py` uses only Blender's modules (`bpy`, `bmesh`): it writes the layout in the engine's axes (metres, X
north, Y east) and places every object at Blender's (x, −y, z); it saves the `.blend` (to edit by hand afterwards:
re-export with the same options, or change the script) and exports the `.glb` with *+Y Up*, *Apply Modifiers*,
*Custom Properties* (the waypoint links), *Punctual Lights* and the *Raw* lighting mode (the sun's intensity 1.0 goes
to the engine as it is). Every mesh is a 1 m cube shared by the objects of one material and scaled into place, so the
import makes one `SM_` per material (`SM_Wall`, `SM_Crate`, `SM_Floor`, the four pads) and a `M_` per colour.

```text
       W (-Y)                   Y=0                  E (+Y)          (X north up; 1 character = 1 m across, 2 m down)
  30 #################################################
  28 #                 .............                 #      .  spawn pads      C / T  team starts (5 each)
  26 #                 ..C.C.C.C.C..                 #      a  bomb site A     b      bomb site B
  24 # bbbbbbbbbbb     .............     aaaaaaaacaa #      c  crates (1.1 m)  S      crate stack (UCX_ box)
  22 # bbccbbbbbbb ### ...........c. ### aaaaaaaaaaa #      #  walls (3.5 m, the edge 4 m)
  20 # bbbbbbbbbbb ###            c  ### aaacaaaaaaa #      =  the low wall at B (1 m) and its player clip
  18 # bbbbbbbcbbb ###               ### aaacaaaaaaa #
  16 # bbbbbbbcbbb ###               ### aaaaaaaaaaa #      ### beside a site: the site wall toward the CT spawn
  14 # =====       ###         S     ###             #
  12 #                                               #
  10 #                 #####   #####                 #      mid doors (a 3 m doorway)
   8 #####       #######           #######       #####
   6 #####       #######           #######       #####
   4 #####       #######           #######       #####
   2 #####       #######           #######       #####
   0 #####                cc                     #####      the short connectors (X -2 .. 2) cross the blocks
  -2 #####       #######           #######       #####
  -4 #####   c   #######           #######  c    #####      B long | short B | mid | short A | A long
  -6 #####       #######        c  #######       #####
  -8 #####       #######           #######       #####
 -10 #####       #######           #######       #####
 -12 #####       #######           #######       #####
 -14 #####       #######           #######       #####
 -16 #                                               #      the T plaza
 -18 #                                               #
 -20 #             c                   c             #
 -22 #               .................               #
 -24 #               .................               #
 -26 #               ....T.T.T.T.T....               #
 -28 #               .................               #
 -30 #################################################
```

| Nodes | Become (the rules above) |
| --- | --- |
| `Floor`, `Wall_*`, `Block*`, `SiteWall_A` / `_B`, `MidDoor_*`, `BLowWall`, `Crate_NN`, `Pad_*` | static mesh actors colliding with their triangles |
| `CrateStack` + `UCX_CrateStack_01` | a static mesh actor whose collision is one box |
| `Clip_BLowWall` | a blocking volume: nobody jumps over the low wall at B |
| `BombSite_A` / `_B` (14 × 10 × 3 m), `BuyZone_CT` (7 × 12 m) / `_T` (7 × 16 m) | trigger volumes tagged [`BombSite`, `A`], [`BuyZone`, `CT`], ... (ShooterGame's rules) |
| `PlayerStart_CT` … `.004`, `PlayerStart_T` … `.004` | ten player starts, 2 m apart, 0.92 m up (UE's start: the capsule's centre), the CTs facing south, the Ts north; `PlayerStartTag` `CT` / `T` |
| `NavWaypoint_*` (18: `TSpawn`, `TMid`, `TPlazaA` / `B`, `Mid`, `ShortA` / `B`, `LongA` / `B`, `MidDoors`, `CTMid`, `AConnector` / `BConnector`, `SiteA` / `B`, `CTSpawn`, `CTA` / `CTB`) | navigation waypoints, each linked both ways by its `links` custom property, plus the links the import adds (above) |
| `Sun` | the directional light, shining down toward the south-east |

The project's `RequiredTags` hold (both sites, both buy zones, both teams' starts). ShooterGame's tests
(`ShooterGame.Map.*`) check the imported map, import `de_leon.glb` and the AxisTest source under the project's rules
(the second is refused: no sites, buy zones or team starts) and spawn ten pawns on the map. `Characters/make_team_bodies.py` makes the teams' placeholder bodies the
same way (`Body_CT.glb`, `Body_T.glb`, imported as static meshes).

## Engine maps

| Map | Contents |
| --- | --- |
| `/Engine/Maps/Entry` | The empty map (UE's `Entry`): world settings, a directional light, the framing camera actor and a player start at its view; the server default map |
| `/Engine/Maps/Template_Default` | The default template (UE's `Template_Default`), `GameDefaultMap`: a 20 m plane with `M_WorldGrid`, a player start, a directional light, the framing camera actor and the first player start at its view |
| `/Engine/Maps/AxisTest` | [Above](#axistest) |

`Entry` and `Template_Default` were migrated in P15 from the legacy `.llev` templates (`Blank.llev`, `Starter.llev`)
while the level reader still existed, and the packages are now their source of truth. What a legacy level stored
became: the actors (the same classes), the spin / bob / point-light orbit a `URotatingMovementComponent` /
`UBobbingMovementComponent` / `UOrbitMovementComponent`, a trigger's interaction data a `UInteractableComponent`, the
game mode string the world settings' `DefaultGameMode` ("Default": none), the camera framing an `ACameraActor` that
keeps it and an `APlayerStart` at the view it opened with (the level's first start), the level name the map's name,
and a sphere of another tessellation an `SM_` asset of the map. The frames of the migrated maps are the same pixels as
the levels'.

## Running a map

```text
Engine\Binaries\Win64\LeonGame.exe [<map>[?game=<class>][#<portal>]] [-map=<map>] [-nullrhi] [-tick=<Hz>] [-showstats]
                                   [-AxesGizmo] [-ExecCmds="<command>;<command>"] [-Screenshot=<file.bmp>] [-ExitAfterFrames=N]
```

The map is a long package name (`/Engine/Maps/Entry`, `/Game/Maps/X`) or a `.lmap` file; without one it is
`GameDefaultMap` of `[/Script/EngineSettings.GameMapsSettings]` (`BaseEngine.ini`: `/Engine/Maps/Template_Default`). A
map file outside the mount points brings its content with it: `LeonGame.exe D:\Work\RenderTest\Maps\RenderTest.lmap`
mounts `D:\Work\RenderTest` as `/RenderTest/`, where the map's own meshes and materials are. `open <map>` loads another
map at the next frame. [SETUP.md](SETUP.md#leongame) has the options.

## Deviations from UE 4.27

- Volumes are boxes, not BSP brushes (D16); `UCX_` convex hulls are boxes.
- A scene component saves its relative quaternion after its properties, so a loaded transform is bit-exact.
- The map importer is a LeonEd factory with naming rules in the config (UE: Datasmith or the glTF importer's level
  import, with metadata); light intensities are the glTF values as they are (no photometric units).
- `ChoosePlayerStart` takes the first player start; there is no Play From Here start (no editor).
- `UBobbingMovementComponent`, `UOrbitMovementComponent`, `UInteractableComponent` and `ANavigationWaypoint` are Leon's.
- An imported map keeps its source in its world's editor-only `AssetImportData` (UE keeps it on the Datasmith scene).
- No streaming levels, world composition, level blueprints or built lighting data yet (`<Map>_BuiltData` later).
