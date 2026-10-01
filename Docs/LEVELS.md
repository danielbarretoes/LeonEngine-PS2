# Maps (`.lmap`)

A map is a `.lmap` package that holds a world: its `UWorld` (the map's asset, named after the package), the world's
persistent level, the level's `AWorldSettings` and the actors with their components (plan decision D13,
`PKG_ContainsMap`), as UE's `.umap`. `UEngine::LoadMap` opens one; a map is made by importing a glTF scene exported from
Blender (LeonEd's `UGLTFMapFactory`, [below](#importing-a-map-from-gltf)) or by code that builds a world and saves it.
The PS2 runtime loads maps the same way: ShooterGame opens `de_leon` or `de_harbor` from its pak on the EE, cooked by
the PS2 target platform ([ps2-engine](PLANS/ps2-engine.md)).

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
| `AWorldSettings` | `Classes/GameFramework/WorldSettings.h` | The level's settings actor: `DefaultGameMode` (plan decision D18), `KillZ`, the bake's, fog's and sky's settings, the overview (`OverviewSettings`, [below](#the-overview)) |
| `AStaticMeshActor` | `Classes/Engine/StaticMeshActor.h` | A placed mesh: its `UStaticMeshComponent` root (mesh, override materials, mobility, collision) |
| `APlayerStart` | `Classes/GameFramework/PlayerStart.h` | Where players spawn; `PlayerStartTag` carries the game's meaning (`CT`, `T`) |
| `ATargetPoint` | `Classes/Engine/TargetPoint.h` | A named point: a transform and `Tags` |
| `ABlockingVolume`, `ATriggerVolume`, `APainCausingVolume` | `Classes/Engine/`, `Classes/GameFramework/` | Boxes (plan decision D16): an invisible wall, a region gameplay code tests (`Tags`: `BombSite` + `A`), a damaging region (every `PainInterval` it deals `DamagePerSec` × `PainInterval` to each pawn it encompasses: `CausePainTo`) |
| `ADirectionalLight`, `APointLight` | `Classes/Engine/` | The lights (colour, intensity, shadows, source angle / attenuation radius) |
| `ACameraActor` | `Classes/Camera/CameraActor.h` | A placed camera: its `UCameraComponent` keeps an orbit or free-look view (the legacy levels' framing, P15) |
| `ANavigationWaypoint` | `Classes/AI/Navigation/NavigationWaypoint.h` | A point of the map's waypoint graph: `Links` (one way) and `Flags`; `UNavigationSystem` builds its graph from them when the world begins play (P20). `AAIController` reads two flags on its path: `Jump` (it jumps near that point) and `Crouch` (it crouches along the links on both sides of that point and stands up past them; the character must be able to crouch, `NavAgentProps.bCanCrouch`) |
| `URotatingMovementComponent` | `Classes/GameFramework/RotatingMovementComponent.h` | UE's: turns its component at `RotationRate` (degrees per second), optionally about `PivotTranslation` |

The rotating movement component ticks with its actor: a map's spinning actor turns as the world ticks. The glTF map
importer adds no movement component; a map saved by code can hold one.

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
`M_<Material>` in `/Game/Maps/de_leon/Materials` with its base colour and normal images (embedded or external) as `T_`
textures there. A material whose glTF extras name a physical material
(`{"physMaterial": "/Game/PhysicalMaterials/PM_Wood"}`, N30f) gets it as its `PhysMaterial`; a name that does not
exist is an error (the `PM_` assets are imported first).

### From Blender

- Model in metres with Blender's axes. Blender and the engine are both Z up with the same +X; Blender is right-handed
  and the engine left-handed, so Blender's +Y is the engine's −Y (the exporter and the importer do this for you:
  glTF is +Y up, and `FImportCoordinateConversion` turns glTF (x, y, z) m into the engine's (x, z, y) × 100 cm).
- Export **glTF 2.0**: *glTF Binary (.glb)*, its images embedded (since [ps2-shipping](PLANS/ps2-shipping.md) N21), or
  *glTF Separate* with external images; *+Y Up* on (the default); *Include › Custom Properties* on for the waypoints'
  `links` / `flags`; *Include › Punctual Lights* on; apply the modifiers. A map script exports with ShooterGame's
  `leon_art.export_glb` (the fixed options, the same bytes every run: [ART_PIPELINE.md](ART_PIPELINE.md)).
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
| `VIS_<Cell>` | `AVisibilityCellVolume`, the box of its mesh, `CellName` its suffix: a visibility cell ([cells and portals](#cells-and-portals)) | engine (N15) |
| `PORTAL_<CellA>_<CellB>` | `AVisibilityPortal` between the two cells (either name may hold an underscore), `Corners` the rectangle of its quad in the world; left out, with a warning, when the suffix names no two cells of the map | engine (N15) |
| `BombSite_A` / `_B`, `BuyZone_CT` / `_T` | `ATriggerVolume`, `Tags` [`BombSite`, `A`] / [`BuyZone`, `CT`] | a project's (ShooterGame, P17), below |
| a KHR_lights_punctual light | `ADirectionalLight`, or `APointLight` for a point or spot light (Leon has no spot light: a warning): the colour, the glTF intensity as `Intensity`, `range` as `AttenuationRadius` (8 m without one); every light casts shadows in the [static lighting](#static-lighting) | always, whatever its name |
| `WorldSettings` (an empty) | no actor: its extras set the map's `AWorldSettings` ([the world settings](#the-world-settings)) | always (ps2-polish P8) |
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
a 3 m drop, 20 m); de_leon's import adds 38 links to the 22 waypoints' hand-authored ones, the same on every import
(the map's bytes do not change: gate G5).

### Collision

A static mesh actor collides with its mesh's triangles (the physics scene's static triangle bodies), unless its mesh
has `UCX_` boxes, which then answer traces and physics. Leon's physics scene has boxes and triangle meshes, no convex
hulls, and gives a body the bounds of its boxes: a `UCX_` piece is its bounding box, and several pieces merge into one
box (a documented deviation). `COL_` meshes collide with their triangles and are never drawn; `Clip_` volumes are
boxes. A query with `FCollisionQueryParams::bReturnPhysicalMaterial` gets the surface it hit in
`FHitResult::PhysMaterial`: the physical material of the hit triangle's material slot, or of the mesh's first material
for a `UCX_` box (N30f; ShooterGame's footsteps, impacts and penetration read it).

<a id="static-lighting"></a>

### Static lighting

The last step of the import bakes the map's lighting into its static meshes, per vertex ([ps2-shipping](PLANS/ps2-shipping.md)
N22, decision D5: LeonCook bakes it, not Blender, so it is deterministic): LeonEd's `FStaticLightingSystem` (UE: Lightmass).
`LeonCook <Project>.lproj -run=ResavePackages -buildlighting [-package=/Game/Maps/X]` (UE's switch) bakes a map again
without its source (the engine's `Entry` and `Template_Default`). For each vertex of every Static, visible static mesh
component of the level, in linear RGB:

- **the sky**: the world settings' `LightmassSettings` (UE's `FLightmassWorldInfoSettings`), `EnvironmentColor` ×
  `EnvironmentIntensity` (a light blue × 0.35 by default), times the share of the vertex's hemisphere that sees no
  geometry within `MaxOcclusionDistance` (300 cm): `NumOcclusionRays` (64) cosine-weighted directions, a stratified set
  from a fixed seed, the same for every vertex; `bUseAmbientOcclusion` off lets the whole sky in;
- **each light that is not Movable** (the map's lights are Static): Lambert, a point light's range attenuation squared,
  as the renderer lights what moves; a ray toward the light (along a directional light's direction, to a point light's
  position) that meets the static geometry first leaves it out when the light `CastShadows`.

The occluders are the Static, visible, shadow-casting (`CastShadow`) static meshes' triangles, their collision
triangles at full precision, in a bounding volume tree (PhysicsCore's `FAabbTree`, the physics scene's). A ray starts
2 cm off its vertex along the normal and ignores hits within 0.05 cm of its start, so the foot of a wall on the ground
sees the ground only below it: make the ground a closed mesh (a slab), so that a ray into it meets its far side. The
light times the mesh's own colour, clamped to 1, becomes the component's baked vertex colours
(`UStaticMeshComponent::BakedVertexColors`, [ASSET_FORMATS.md](ASSET_FORMATS.md#lps2-instance-colors)), saved in the map
and made for the mesh as it was: a mesh rebuilt since (another import) draws flat until the map is baked again, and the
cook warns about it (`the lighting needs to be rebuilt`). The bake runs on one thread in a fixed order: the same map
bakes the same bytes, so the reimport stays byte for byte (gate G5). de_leon bakes 3 471 vertices of 78 meshes against
2 244 triangles and four lights with 224 885 rays in under 0.1 s.

At run time a Static mesh draws its baked colours times its material's albedo, with no light computed per frame, and
anything Movable (the pawns' bodies, the weapons, the projectiles, the bomb) is lit per frame by the scene's lights with
the same sky as its ambient ([ARCHITECTURE.md](ARCHITECTURE.md#12-rendering-the-gs-path)). Per-vertex lighting shows
only where there are vertices: a surface that should catch a shadow needs them (de_leon's ground is a grid of 3 m
cells). A light probe grid from the bake, for the light and shadow at a pawn, is a later step.

### Cells and portals

A map may split itself into visibility cells ([ps2-shipping](PLANS/ps2-shipping.md) N15; UE 4.27's closest is its
precomputed visibility volumes): a `VIS_<Cell>` node is a box around a room (or a corridor, a yard), a
`PORTAL_<CellA>_<CellB>` node a quad in the opening between two of them. Each frame the renderer finds the cell the eye
is in and walks the portals the view sees, each narrowing the screen rectangle the next cell is seen through; what the
cells it reaches hold is drawn, the rest is not, whatever the frustum says. A prop is in every cell its bounds touch
and a primitive in none is always drawn, so the cells only take away: a map without them draws as before, and so does
an eye outside every cell. Make a cell's box hold its room's walls, keep the portals as big as the openings (a portal
the eye stands within 60 cm of is open whole), and at most 64 cells.

The same tuning has two more knobs, both off by default: the world settings' `FogSettings` (`bEnableFog`,
`FogInscatteringColor`, `StartDistance`, `EndDistance`: a linear distance fog, the GS's) and a mesh's LODs
(`LODs=<share>@<screen size>,...` in `ImportList.ini`). Since [ps2-polish](PLANS/ps2-polish.md) P8 de_leon's fog is on,
from 30 m to the far plane's 100 m, in the sky's horizon colour ([the world settings](#the-world-settings)): it tints
only the far end of a long view and hides the far plane's cut against the sky. It uses no LODs: its meshes are Static
and baked, and a mesh with baked colours always draws at LOD 0 (the colours are baked for LOD 0's batches), so LODs
would change nothing; its largest piece has 146 triangles anyway.

### The world settings

A node named `WorldSettings` (an empty; Blender's custom properties become its glTF extras) sets the map's
`AWorldSettings` ([ps2-polish](PLANS/ps2-polish.md) P8): each extras key is a property path of the world settings
(`KillZ`, `FogSettings.bEnableFog`, `SkySettings.SkyCubemap`), each value its text as UE's `ImportText` reads it
(`True`, `3000`, an asset by its object path, loaded when it is not); a key that names no property, or a value that
does not parse, fails the import. de_leon's (`make_de_leon.py`'s `WORLD_SETTINGS`):

| Key | Value |
| --- | --- |
| `SkySettings.SkyCubemap` | `/Game/Sky/T_Sky_Desert.T_Sky_Desert`, the sky ([ART_PIPELINE.md](ART_PIPELINE.md#the-sky)) |
| `FogSettings.bEnableFog` | `True` |
| `FogSettings.StartDistance` / `EndDistance` | `3000` / `10000` (cm) |

**The sky** (`FWorldSkySettings::SkyCubemap`, a `UTextureCube`) is drawn after the clear and before the world: a box
around the eye (it turns with the view and never moves), its faces the cube map's, unlit, unfogged, with no depth test
and no Z written, so everything draws over it ([ARCHITECTURE.md](ARCHITECTURE.md#12-rendering-the-gs-path)). Without
one the background is the renderer's clear colour. **The fog** takes the sky's horizon colour
(`UTextureCube::HorizonColor`) while `FogSettings.bInscatteringColorFromSky` is on (the default), so the distance fades
into the sky; `FogInscatteringColor` otherwise, or without a sky.

### The overview

A project whose Editor config sets `bBuildOverview=True` in `[/Script/LeonEd.MapImportSettings]` (ShooterGame's
`DefaultEditor.ini`) gets each map's overview, the radar's picture in the style of CS 1.6's overviews
([ps2-polish](PLANS/ps2-polish.md) P7), as the import's last step, after the bake: `<Map>/T_<Map>_Overview`
(`/Game/Maps/de_leon/T_de_leon_Overview`), and the map's `AWorldSettings::OverviewSettings` names it with the square it
shows (`Center`, `Size`). The engine's maps and those in `MapsWithoutOverview` (the main menu's) get none.

- **What it shows**: the map from above in orthographic, north (+X) up and east (+Y) right, its Static, visible meshes
  as the game draws them (their baked lighting and textures) without the sky or the fog, cut at `OverviewClipHeight`
  (250 cm: roofs, a tunnel's ceiling and the walls' tops are not drawn, so the floors under them show). A point 1 m
  above the ground that stands inside something (a wall, a house, a container, a crate) is drawn as an obstacle, dark
  grey; nothing below the cut is the outside, darker; the floors keep their colours, greyer and with their shadows
  softened, and a dark line runs along every obstacle and every step of more than 40 cm.
- **Its square**: the map's visibility cells (where the players go, not the scenery around: de_harbor's water and ship
  stay out) plus 2 m on each side, or without cells the Static meshes' bounds; de_leon 64 m, de_harbor 68 m.
- **The texture**: 128 x 128 (`OverviewResolution`), PSMT8, one level: 17 KB of VRAM, about 50 cm a texel, the radar's
  own scale (88 pixels for 50 m). Rendered at 512 x 512 on GSReference's software GS and averaged down
  ([ARCHITECTURE.md](ARCHITECTURE.md#12-rendering-the-gs-path)), it is the same bytes on every machine and every
  import: the reimport (gate G5) covers it.
- **Its use**: `FWorldOverviewSettings::GetUV(Location)` gives a world point's place on it (U west to east, V north to
  south); ShooterGame's radar draws the part around the view, turned with it, under its dots.

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

ShooterGame's map (P17; rebuilt in [ps2-shipping](PLANS/ps2-shipping.md) N28), a 60 × 48 m desert town in the style of
Counter-Strike 1.6's de_dust, is built by a script, so the map is reproducible and its layout reviewable as code:

```bat
:: 1. Build the map in Blender (headless) -> de_leon.blend + de_leon.glb next to the script
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup ^
    --python Game\ShooterGame\SourceArt\Maps\make_de_leon.py

:: 2. Import it (with the other ShooterGame source art) -> Content\Maps\de_leon.lmap, de_leon\Meshes, de_leon\Materials
Engine\Binaries\Win64\LeonCook.exe Game\ShooterGame\ShooterGame.lproj -run=ImportAssets ^
    -importlist=Game/ShooterGame/SourceArt/ImportList.ini

:: 3. Play it (the main menu is GameDefaultMap; this URL skips it, joining CT with nine bots)
Game\ShooterGame\Binaries\Win64\ShooterGame.exe /Game/Maps/de_leon?team=CT
```

`make_de_leon.py` uses Blender's modules through ShooterGame's `leon_art` ([ART_PIPELINE.md](ART_PIPELINE.md)): it
writes the layout in the engine's axes (metres, X north, Y east), places every object at Blender's (x, −y, z), saves
the `.blend` and exports the `.glb` with `leon_art.export_glb` (custom properties for the waypoint links, punctual
lights in the *Raw* mode: the intensities go to the engine as they are). The textures are painted texel by texel in
the script (sandstone blocks, a darker trim, sand, paving slabs, planks, CS's crate and the sites' letters; P4 and P8,
64 texels a metre). Every piece of the map is an axis-aligned box or a few, one mesh of its own named
`<Cell>_<Piece>` (`SM_<Cell>_<Piece>`), its vertices around its centre, its faces cut on a 3 m world grid and the walls
again 1.2 m up, so the baked lighting has vertices for the shadows and the occlusion at the walls' foot (N22); the
faces nobody sees (a wall's bottom, a face against the edge) are left out. The crates share `SM_Crate` (1.1 m, a CS
jump clears it) and `SM_CrateBig` (1.6 m). The script fails when a cell has more than ART_PIPELINE's 1 500 triangles
or when a waypoint link, a start or a site's middle runs into something, and prints each cell's count.

```text
       W (-Y)                   Y=0                  E (+Y)          (X north up; 1 character = 1 m across, 2 m down)
  30 ##################################################
  28 #######           +             +          #######     #  walls and houses (3.5 to 5.5 m)
  26 #######bbbbbb      +           +     aaaaaa#######     ^  under an arch or a lintel
  24 #  bbbbbbbbCb                   +    aCaaaaacca  #     n  the B tunnel, roofed at 3 m
  22 #  bcccbbbbbb                        aaaaaaaaaa  #     =  the low wall at B (1 m) and its clip
  20 #  bbbbbbbbbb  ##            cc  ##  aaaaaaaaaa  #     c  crates (1.1 m)   C  big crates (1.6 m)
  18 #  bbbbbbbbbb  ##                ##  aaccaaaaaa  #     H  ladders to the roofs (3.5 m)
  16 #  bbcbbbbbbb  ##                ##  aaaaaaaaaa  #     a  bomb site A      b  bomb site B
  14 #  =====bbbbb  ##          C     ##  aaaaaaaaaa  #     +  CT starts        t  T starts
  12 #       bbbbb                        aaaaaaaaaa  #
  10 #                                                #     ## at X 14 .. 22: the sites' walls
   8 #####        ###########^^###########        #####     ^^ at X 9: the mid doors
   6  ####c       ######            ######       c####
   4  ####       H######            ######H       ####
   2  ####        ######            ######        ####
   0  ####                 c                      ####     B long | short B | mid | short A | A long
  -2  ####                                        ####
  -4  ####c       ######            ######cc      ####
  -6  ####        ######        ccc ######        ####
  -8  ######nnnn########            ######        ####
 -10  ######nnnn########            ######        ####
 -12  ######nnnn########            ######        ####
 -14  ######nnnn##########^^^^^^^^###########^^#######     the tunnel, the mid arch, the A long gate at X -14
 -16 #                  ##        ##      ##    ##    #
 -18 #                                                #     the T plaza
 -20 #                                                #
 -22 #           ccc                    cc            #
 -24 #                                                #
 -26 #######                                    #######
 -28 #######              t t t t t        CC   #######
 -30 #######                                    #######
 -32 ##################################################
```

| Cell | Where | Triangles | What stands in it |
| --- | --- | ---: | --- |
| `TSpawn` | X −30 … −14 | 446 | the T plaza, sand; a house in each corner, crates, the T starts and buy zone |
| `Mid` | X −14 … 9, Y −6 … 6 | 160 | the arch at its south end (a 9 m opening under a lintel at 3.2 m), crates |
| `LongA` | X −14 … 9, Y 6 … 24 | 398 | A long (8 m wide), the gate at its south end (3 m under 2.8 m), short A, the blocks and houses beside it, the ladder to the north block's roof |
| `LongB` | X −14 … 9, Y −24 … −6 | 458 | B long, the tunnel at its south end (4 m wide, 8 m long, roofed at 3 m, two lamps), short B, the blocks and houses, the ladder |
| `CTSpawn` | X 9 … 30, Y −8.5 … 8.5 | 234 | the mid doors (a 3 m doorway under a lintel at 2.7 m, two wooden wings open against the north face, a lamp), paving, the CT starts and buy zone, crates |
| `SiteA`, `SiteB` | X 9 … 30, north-east / north-west of the site walls | 266, 282 | paving, the site wall toward the CT spawn (open at both ends) with the site's letter, a house in the far corner, crates; the low wall at B |

2 244 triangles in 78 pieces (N22's blockout: 1 070 in 35 cubes). The ground is the floor slabs' own `UCX_` boxes,
their tops at Z = 0 and their materials the surfaces a step or a bullet finds: the sand's dirt and the paving's tile
(ps2-shipping N29). The seams between two slabs, whose float bounds differ by a hair, stop no one: the engine takes a
box whose top is at the capsule's feet, within the skin, for floor (`ACharacter::IsFloorEdgeHit`,
`FPhysScene::ResolveCapsuleSides`, as UE's CharacterMovementComponent walks across). Until N29 a hidden `COL_Ground`
box under the whole map was the ground, all of it dirt. Every material names its surface in the script (`SURFACES`,
through `leon_art.make_material(..., surface=)`, N30f): sandstone, trim and signs concrete, sand dirt, paving tile,
wood and crates wood, the lamps metal.

| Nodes | Become (the rules above) |
| --- | --- |
| `<Cell>_Floor*`, `_Wall*`, `_Block*`, `_House*`, `_Arch*`, `_Gate*`, `_Tunnel*`, `_MidDoor*`, `_Door*`, `_SiteWall`, `_LowWall`, `_Sign`, `_Lamp*`, `_Ladder_*`, `_Crate*` | static mesh actors, each with its `UCX_<Node>_01` box (the ladders', the signs' and the tunnel's lamps' inside their wall, so they stop nobody); the crates' box is on their shared mesh |
| `Clip_BLowWall` | a blocking volume: nobody jumps over the low wall at B |
| `BombSite_A` / `_B` (14 × 10 × 3 m), `BuyZone_CT` (7.5 × 16 m) / `_T` (7 × 16 m) | trigger volumes tagged [`BombSite`, `A`], [`BuyZone`, `CT`], ... (ShooterGame's rules) |
| `Ladder_A`, `Ladder_B` | trigger volumes tagged `Ladder` (N30c): 20 cm boxes against the north blocks' long faces, from the floor to their roofs (3.5 m); the bots climb them (ps2-polish P3: a waypoint flagged `Ladder` at each one's foot and top) |
| `PlayerStart_CT` … `.004`, `PlayerStart_T` … `.004` | ten player starts, 2 m apart or more, 0.92 m up (UE's start: the capsule's centre), the CTs facing south, the Ts north; `PlayerStartTag` `CT` / `T`. The CT starts stand 6 to 7 m to either side of the middle (ps2-polish P3: in the line of the mid doors and the arch the terrorists' spawn saw them across the map), A's side and B's in turn |
| `NavWaypoint_*` (34: `TSpawn`, `TMid`, `TPlazaA` / `B`, `LongAGate`, `TunnelB`, `Mid`, `ShortA` / `B`, `LongA` / `B`, `ALongEnd` / `BLongEnd`, `MidDoors`, `CTMid`, `AConnector` / `BConnector`, `SiteA` / `B`, `CTSpawn`, `CTA` / `CTB`; ps2-polish P3's `LookoutA1` … `B3`, flagged `Lookout`, three a site off the lanes' line, `LadderAFoot` / `BFoot` and, on the roofs, `LadderATop` / `BTop` (flagged `Ladder`) and `RoofA` / `B`) | navigation waypoints, each linked both ways by its `links` custom property and flagged by its `flags` one, plus the 66 links the import adds (above; the ladders' feet to their tops among them) |
| `Sun`, `Light_Tunnel_01` / `_02`, `Light_MidDoors_01` | the directional light (warm, high in the north-west) and three warm point lights (7 and 6 m), baked into the static meshes with the sky |
| `VIS_<Cell>` (7), `PORTAL_<CellA>_<CellB>` (24) | [cells and portals](#cells-and-portals) (box meshes up to 6 m, quads; `.001` copies for a second quad between two cells): the doorways and lanes, and the sky over the walls between two cells from 3.5 m up. The walls are low, so a view across the map keeps most cells; facing a wall only its own |

The project's `RequiredTags` hold (both sites, both buy zones, both teams' starts). ShooterGame's tests
(`ShooterGame.Map.*`) check the imported map (the sites, the buy zones, the starts, the linked waypoints, the clip, the
two ladders and the sun), import `de_leon.glb` and the AxisTest source under the project's rules (the second is
refused: no sites, buy zones or team starts) and spawn ten pawns on the map; `ShooterGame.Bots.MatchOnDeLeon` plays
three rounds on it.

## Worked example: de_harbor

ShooterGame's second map, a 64 × 56 m industrial port at the end of the afternoon, made by the same pipeline with
nothing of de_leon's but the conventions: its own script (`Maps/make_de_harbor.py`, Blender through `leon_art`), its own
painted textures and materials (`/Game/Maps/de_harbor/Materials`), its own sky (`/Game/Sky/T_Sky_Coast`, `make_sky.py`'s
coast preset) and its own section of `ImportList.ini`. An original layout in the readable style of CS 1.6's maps: the
terrorists' truck yard to the south, the counter-terrorists' yard to the north, bomb site A on the quay apron under a
gantry crane to the east, bomb site B inside a roofed warehouse to the west, and three routes:

- **A long**, the quay (10 m wide, along the water): from the T yard's apron by the customs shed's corner to a chicane
  of two container stacks at its end, so the quay's line never reaches into the site;
- **mid**, a container lane through a 3.5 m gate in a 4 m wall into the courtyard, the CT spawn's flank (a stack behind
  the gate and a fence with a 6 m opening hide the CT spawn from mid);
- **B short**, the alley to the warehouse's front door (a container 3.5 m inside it makes a vestibule, so the alley sees
  nothing of the hall), with the **connector**, a 16 m roofed passage from mid to the alley.

A is closed to the courtyard and most of the CT spawn by a wall of stacked containers (the CTs come in by a 6 m gap);
B's second door is the CT spawn's (5 m). The water east of the quay (a clip along the edge) and a ship moored off it
are scenery.

```bat
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup ^
    --python Game\ShooterGame\SourceArt\Maps\make_de_harbor.py
python Game\ShooterGame\SourceArt\Sky\make_sky.py
Engine\Binaries\Win64\LeonCook.exe Game\ShooterGame\ShooterGame.lproj -run=ImportAssets ^
    -importlist=Game/ShooterGame/SourceArt/ImportList.ini
Game\ShooterGame\Binaries\Win64\ShooterGame.exe /Game/Maps/de_harbor?team=CT
```

```text
       W (-Y)                       Y=0                         E (+Y)   (X north up; 1 character = 1 m across, 2 m down)
  34 ###########################################################~~~
  32 #CCC        ccc #        ########          XX          XX  ~~~   #  walls, sheds, blocks, fences (4 to 6.4 m)
  30 #  bbbbbbbbbbb  #  ++    ########    ++     aaaaaaaaaaaa   ~~~   ^  roofed (the connector, the alley's shed) or
  28 #  bbbbbbbbbbb  ^    ++            ++       aaaaaaaOOOaa   ~~~      under a lintel (the warehouse's doors)
  26 #  ooobbbbbbbb  ^                +       OOOaccaaaHOOOaa   ~~~   o  a container (2.6 m)  O  a stack of two
  24 #  ooobbbbbbbb  #                      ccOOOaaaaaaaOOOaa   ~~~   c  pallet crates (1.1 m)  C  big (1.6 m)
  22 #  ooobbbbbbbb  #                        OOOaaaaaaaaaacc   ~~~   X  the crane's legs   H  the ladder (5.2 m)
  20 #  ooobbbbbbbb  ##########      #########OOOaaaaaaaaaacc   ~~~   a  bomb site A        b  bomb site B
  18 #  bbbbbbbbbbb  #                        OOXXaaCCaaaaaaXX  ~~~   +  CT starts          t  T starts
  16 #ooooooobbbbbb  #         OOOOOO         OOOaaaaaaaaaaaa   ~~~   ~  the water
  14 #ooooooo      cc#         OOOOOO         OOO               ~~~
  12 #             cc# c                      OOO      OOOOOOO  ~~~   the warehouse (B): X 8 .. 32, Y -28 .. -12,
  10 ##^^^^########### c                      OOO      OOOOOOO  ~~~   roofed at 6.4 m; its front door at X 8
   8 #      #####################  #################            ~~~   <- mid's gate (3.5 m), the courtyard north of it
   6 #      ################            ############OOOOOO      ~~~   <- A long's chicane
   4 #    cc################            ############OOOOOO      ~~~
   2 #    cc################ c          ############ c          ~~~
   0 #      ################            ############            ~~~   alley | connector | mid | customs | A long
  -2 #ooo   ^^^^^^^^^^^^^^^^            ############      ooo   ~~~
  -4 #ooo   ################        cc  ############      ooo   ~~~
  -6 #ooo   ################        cc  ############      ooo   ~~~
  -8 #ooo   ################            ############      ooo   ~~~
 -10 #^^^^^^################            ############        c   ~~~
 -12 #^^^^^^                OOO      OOO                    c   ~~~   <- the aprons (X -18 .. -12), mid's stacks
 -14 #^^^^^^                OOO      OOO                 ###    ~~~
 -16 #^^^^^^                OOO      OOO                 ###    ~~~   the booth in A long's mouth
 -18 #                                                          ~~~   the T yard (asphalt)
 -22 #                                  cc                      ~~~
 -26 ######        ooo                    oooooo        OOO     ~~~
 -28 ######        ooo       tttttttttt   oooooo        OOO     ~~~
 -32 ###########################################################~~~
```

| Cell | Where | Triangles | What stands in it |
| --- | --- | ---: | --- |
| `TSpawn` | X −32 … −18 | 574 | the truck yard, asphalt; a gatehouse in its south-west corner, containers, crates, the T starts and buy zone |
| `LongA` | X −18 … 8, Y 6 … 28 | 464 | the quay (concrete) and the apron before it, the customs shed (shutters on two faces), the booth, a container, the chicane's first stack, crates |
| `Mid` | X −18 … 8, Y −6 … 6 | 312 | the asphalt lane, a stack on each side of its mouth, crates, the gate at its north end |
| `Connector` | X −18 … 8, Y −22 … −6 | 364 | the two blocks, the roofed passage between them (two lamps), the apron before the alley |
| `Alley` | X −18 … 8, Y −28 … −22 | 224 | the alley to the warehouse's front door, its shed roof (a lamp), a container |
| `Courtyard` | X 8 … 20, Y −12 … 12 | 250 | concrete, the stack across mid's line, the fence to the CT spawn with its 6 m opening |
| `CTSpawn` | X 20 … 32, Y −12 … 12 | 172 | the harbour office against the north wall, the CT starts and buy zone |
| `SiteA` | X 8 … 32, Y 12 … 28 | 790 | the quay apron, the container wall to the south, the chicane's second stack, the stack with the ladder and the A sign, crates, the crane's legs |
| `SiteB` | X 8 … 32, Y −28 … −12 | 700 | the warehouse: its walls, doors and roof, four lamps, the vestibule's container, a container and crates, the B sign over the front door |
| (none) | the quay's face, the water, the ship, the crane's beams and boom | 366 | always drawn: outside every cell, or wholly above `CELL_TOP` (7.5 m) |

4 216 triangles in 112 pieces (de_leon: 2 244 in 78), 6 394 baked vertices against 8 lights (the sun, two lamps in the
connector, one in the alley's shed, four in the warehouse). The textures (64 texels a metre; P4 but the corrugated
sheet, P8):

| Texture | Size | Colours | A repeat | On (its surface) |
| --- | --- | --- | --- | --- |
| `T_Asphalt_D` | 128 × 128 | 14 | 2 m | the T yard, mid, the aprons and the alley (Concrete) |
| `T_Concrete_D` | 128 × 128 | 14 | 2 m | the quay, the courtyard, the CT spawn, the sites and the connector: cast slabs (Concrete) |
| `T_Panel_D` | 128 × 128 | 11 | 2 m | the boundary walls, the fences and the small buildings: precast panels (Concrete) |
| `T_Corrugated_D` | 128 × 128, P8 | 82 | 2 m | the customs shed, the blocks, the warehouse: painted corrugated sheet with rust streaks (Metal) |
| `T_Girder_D` | 64 × 64 | 5 | 1 m | the roofs' tops and edges, the lintels, the ladder (Metal) |
| `T_Crane_D` | 64 × 64 | 6 | 1 m | the gantry crane's worn yellow paint (Metal) |
| `T_ContainerRed_D`, `Blue`, `Green` | 64 × 128 | 11 each | 1 × 2.6 m | the containers' corrugated walls and rails (Metal) |
| `T_Pallet_D` | 64 × 64 | 13 | a face | the pine pallet crates (Wood) |
| `T_Water_D` | 64 × 64 | 4 | 4 m | the harbour (Dirt: only a stray bullet reaches it) |
| `T_Shutter_D`, `T_Signs_D` | 64 × 64, 128 × 64 | 8, 6 | a plaque | the customs shed's roller doors, the A and B plates (Metal) |

| Nodes | Become |
| --- | --- |
| `<Cell>_*`, `Outside_*` | static mesh actors, each with its `UCX_<Node>_01` box (the sign's, the shutters', the ladder's inside their wall; the crane's top a box at its cab); the containers share `SM_Container_<Colour>_<X\|Y>` (one mesh a colour and an orientation, the second of a stack at 2.6 m), the crates `SM_Pallet` / `SM_PalletBig`, the bollards `SM_Bollard`, the lamps `SM_Lamp` |
| `Clip_Quay` | a blocking volume along the quay's edge: nobody falls into the water |
| `BombSite_A` (14 × 12 m) / `_B` (15 × 11 m), `BuyZone_CT` / `_T` | ShooterGame's trigger volumes |
| `Ladder_A` | the ladder up the stack of two containers at A (5.2 m), climbed by the bots (`LadderAFoot` / `LadderATop`, flagged `Ladder`) |
| `PlayerStart_CT` … `.004`, `PlayerStart_T` … `.004` | five starts a team; the CTs beside the courtyard's opening, out of mid's line (4.5 m and more from Y = 0), A's side and B's in turn |
| `NavWaypoint_*` (47: the three routes, the courtyard, both yards, the sites' ways in, three `Lookout`s a site off the lanes' lines and deep in the sites, the ladder's foot and top and a waypoint on the stack) | the hand links (53) both ways, plus the 112 the import adds |
| `Sun`, `Light_Connector_*`, `Light_Alley_01`, `Light_Warehouse_*` | the late sun (low, warm, from the south-west: the long shadows fall toward the water) and seven lamps, baked |
| `VIS_<Cell>` (9), `PORTAL_<CellA>_<CellB>` (22) | the cells and portals: the warehouse is closed (its two doors are its only portals) and the connector's passage has a portal at each end, so a view inside them keeps few cells |
| `WorldSettings` | `SkySettings.SkyCubemap` `/Game/Sky/T_Sky_Coast.T_Sky_Coast`, the fog from 25 to 90 m (`2500` / `9000`, hazier than de_leon's), a cooler sky light for the bake (`LightmassSettings.EnvironmentColor` `(R=0.72,G=0.82,B=1.0,A=1.0)`, `EnvironmentIntensity` `0.4`) |

The script fails when a cell has more than 1 500 triangles, when two things at walking height overlap, when a waypoint
link, a start or a site's middle runs into something, or when the hand graph is not one piece. The layout was tuned
on the bot match (`BotMatch.bat 10 <seed> /Game/Maps/de_harbor`): with mid open to the CT spawn and both sites the
terrorists won three rounds in four; the gate, the fence, the chicane, the vestibule and lookouts deep in the sites
bring it to 53 % over seeds 1 to 48 (de_leon: 56 % over 1 to 24). ShooterGame's tests check the map as de_leon's
(`ShooterGame.Map.DeHarborHoldsTheGame`, `TenPawnsOnDeHarbor`, `RequiredTags`, `NavigationCoverage`,
`ShooterGame.Bots.MatchOnDeHarbor`, the meshes' quantization); `RunGates.bat` plays its bot match twice
(`BotMatchDeHarbor`).

## Engine maps

| Map | Contents |
| --- | --- |
| `/Engine/Maps/Entry` | The empty map (UE's `Entry`): world settings, a directional light, the framing camera actor and a player start at its view; the server default map |
| `/Engine/Maps/Template_Default` | The default template (UE's `Template_Default`), `GameDefaultMap`: a 20 m plane with `M_WorldGrid`, a player start, a directional light, the framing camera actor and the first player start at its view |
| `/Engine/Maps/AxisTest` | [Above](#axistest) |

`Entry` and `Template_Default` were migrated in P15 from the legacy `.llev` templates (`Blank.llev`, `Starter.llev`)
while the level reader still existed, and the packages are now their source of truth. What a legacy level stored
became: the actors (the same classes), the spin a `URotatingMovementComponent` (the bob, the point-light orbit and a
trigger's interaction data had Leon-only components with no UE class, which no engine map used; they were removed), the
game mode string the world settings' `DefaultGameMode` ("Default": none), the camera framing an `ACameraActor` that
keeps it and an `APlayerStart` at the view it opened with (the level's first start), the level name the map's name,
and a sphere of another tessellation an `SM_` asset of the map. The frames of the migrated maps are the same pixels as
the levels'.

## Running a map

```text
Engine\Binaries\Win64\LeonGame.exe [<map>[?game=<class>][#<portal>]] [-map=<map>] [-nullrhi] [-benchmark] [-showstats]
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
- `ANavigationWaypoint` is Leon's (UE navigates a Recast navmesh; Leon a waypoint graph that `UNavigationSystem` builds).
- An imported map keeps its source in its world's editor-only `AssetImportData` (UE keeps it on the Datasmith scene).
- No streaming levels, world composition or level blueprints. The built lighting is per vertex and lives in the map's
  static mesh components (UE: lightmaps in `<Map>_BuiltData`); Leon has no stationary lights or light probes yet.
