# Testing

What runs automatically and what a person still has to check by hand. Build and test commands:
[SETUP.md](SETUP.md#run-the-tests); coding rules for tests: [CODING_STANDARD.md](CODING_STANDARD.md).

## Automated

| Check | Command | Passes when |
| --- | --- | --- |
| Automation tests (Win64) | `Engine\Build\BatchFiles\RunTests.bat [-automation=<filter>]` | `Automation: N test(s), N passed, 0 failed` twice: the engine's (`LeonAutomationTests`, 383) and ShooterGame's (`ShooterGameTests`, 19) |
| LeonHeaderTool golden tests (run by `RunTests.bat` too) | `Engine\Intermediate\Build\HostTools\Win64\LeonHeaderTool.exe -Test` | `LeonHeaderTool -Test: N of N golden cases passed` |
| Core, CoreUObject, Json, Projects and PakFile on PS2 | `Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build` | `TestPAL: PASSED (113 test(s), 0 failed)` in the EE log (119 on Win64) |
| Format, banned APIs (G4), Win64 build | `Engine\Build\BatchFiles\Lint.bat` | `Lint OK` |
| Reproducible reimport (G5; CI, on a clean checkout) | `Engine\Build\BatchFiles\CheckReimport.bat [<Project>.lproj ...]` | `CheckReimport OK`: `LeonCook -run=ImportAssets -reimport -all` leaves `Engine/Content` and `Game/*/Content` unchanged, the imported maps included (`git diff --exit-code`, no new file) |
| ShooterGame smoke (G6; CI) | `Engine\Build\BatchFiles\SmokeTest.bat` | `SmokeTest OK: 10 pawns, CT 5, T 5, exit code 0`: ShooterGame boots de_leon headless, `bot_fill` adds nine bots to the local player, and the game mode's end-of-match line counts ten pawns in their teams |
| Content loads | `Engine\Binaries\Win64\LeonCook.exe -run=ValidateAssets` | `ValidateAssets: N packages, N valid, 0 problem(s)` |
| Frame capture | `LeonGame.exe [<map>] "-Screenshot=<file.bmp>" "-ExitAfterFrames=N"` | the BMP matches a reference capture byte for byte; a capture is unattended (`FApp::IsUnattended`) and ignores the mouse and the keyboard, so moving the mouse during it changes nothing |
| Staged build (Win64) | `BuildCookRun.bat -project=<.lproj> -platform=Win64 -build -cook -stage -pak -run "-addcmdline=-Screenshot=<file.bmp> -ExitAfterFrames=30"` | the staged Shipping game's capture matches the Development build's byte for byte; two `-cook -stage -pak` runs give the same `.lpak` (SHA-256); CI runs it in Development, headless (`-nullrhi -ExitAfterFrames=60`, exit code 0), for a content-only project and for ShooterGame (`-ExecCmds=bot_fill`) |
| Pak tool | `LeonPak <in.lpak> -test` / `-list` | `N file(s) checked, every SHA-1 matches` |
| Console commands | `LeonGame.exe "-ExecCmds=obj gc;stat fps,stat fps" "-Screenshot=<file.bmp>" "-ExitAfterFrames=30"` | a `Cmd:` line per command, the capture unchanged |

The CoreUObject tests collect garbage (`CollectGarbage`) between their steps; they only keep objects through
`UPROPERTY` members, the root set, `FGCObject` and `TStrongObjectPtr`, and read the others through weak pointers, so
a collection in one test never touches another test's objects. The config tests build their ini layers in memory
(`FConfigFile::CombineFromBuffer`) and remove them afterwards; the SaveConfig test (desktop only) writes its user
layer under `<Project>/Intermediate/Tests/CoreUObjectConfig/` and deletes it.

The gameplay tests (Engine, AIModule, JoltPhysics) work on UObjects since P12. A test that spawns actors creates its
world with `FScopedTestWorld` (`Engine/Public/Tests/ScopedTestWorld.h`): `UWorld::CreateWorld` at the start of the
scope, and at its end `DestroyWorld` (every actor ends play) and a full garbage collection, so the next test starts
without them. Components, cameras, levels, HUDs and game states made outside a world come from `NewObject`; nothing is
declared by value. Such a free object is collected by the next safe point, so a test that ends a test world early
declares the objects it still needs before that world, or holds them in `TStrongObjectPtr`. Reflected fixtures (an
actor that spawns during its tick, test pawns, controllers and a component that counts its calls) live in
`Engine/Private/Tests/EngineTestTypes.h` and `AIModule/Private/Tests/GameplayTestTypes.h`. `System.Engine.World.*`,
`System.Engine.Components.*` and `System.Engine.GameFramework.*` cover the spawn and destroy sequence, ownership and
collection, attachment rules and sockets, the primitive render state, the game mode, game state and match states, the
HUD widgets and anim instances as objects, and the engine's collection timer.

Since P13 the level content is actors, and since P15 a map is a `.lmap` package.
`System.Engine.MapPackage.SaveLoadRoundTripsEveryActor` saves a world with one actor of every class a map holds (world
settings, a mesh with collision, a spin and a bob, the three volumes, a tagged player start and target point, both
lights, an orbit, a trigger's interaction data, a camera actor) under a `/MapTest/` mount point and loads it back,
value by value and transform by transform; `LoadMapOpensMapPackages` opens it with `UEngine::LoadMap` by name and by
file (the content folder mount); `MovementComponentsMove` ticks the movement components.
`System.AIModule.LevelAndAISmoke.EditorStyleMapResaveHeadless` resaves the engine's maps and compares the bytes with
their files, and `System.Engine.AxisTestMap.NoMirroring` checks the axes map ([LEVELS.md](LEVELS.md#axistest)).
`LeonAutomationTests` links the Renderer, so test worlds have the Renderer's `FScene` (it needs no GPU: the GPU copies
are made only when a frame is drawn), and `System.Engine.Components.SceneProxiesFollowTheComponents` checks that the
proxies follow their components.

Since P13's second part the engine is a UObject and the game starts through `UEngine::LoadMap`. The tests make their
own `UGameEngine` (`TStrongObjectPtr`, `Init(nullptr)` runs it headless) when they need one:
`System.Engine.LoadMap.StarterLogsInThePlayer` opens `/Engine/Maps/Template_Default` and checks the login (the
controller, the default pawn at the map's first player start, the view the legacy framing opened with, the HUD and the
begun play), `System.Engine.LoadMap.GameModePrecedence` the game mode
choice (D18), `System.Engine.URL.*` and `System.Engine.EngineSettings.*` the URL and the map settings.
`System.Engine.Input.*` feed keys and mouse deltas to a player controller and check the mappings of `BaseInput.ini`,
the mouse look and the default pawn's flight; `System.Engine.Console.ExecChain` walks the console chain (`show`,
`stat`, `FOV`, the F1 binding, a deferred command and `open`). A world made with `FScopedTestWorld` begins play at
once, as the worlds did before `LoadMap`.

The package tests (`System.CoreUObject.Package.*`) name their packages `/PackageTest/...`, a mount point they register
for their duration, save them to memory (`UPackage::SaveToMemory` + `FLinkerLoad::RegisterInMemoryPackage`, so they
run on the PS2 too), destroy them (pending kill and a full collection, as a new process would start) and load them
back. Only `System.CoreUObject.Package.Files` (desktop) writes real `.lasset` / `.lmap` files, under
`<Project>/Intermediate/Tests/CoreUObjectPackage/`, and deletes them. `System.CoreUObject.Package.Deterministic`
compares the MD5 of a saved package (its engine version cleared) with a stored hash on every platform: update the hash
when the package format or its fixture changes on purpose.

Since P14 the asset classes are tested the same way: `System.Engine.Assets.*RoundTrip` save every class (a texture; a
static mesh with its body setup, whose slot's material and the material's texture are in two more packages; a
skeleton, a skeletal mesh, a clip and a blend space in four packages; a sound; a game's data asset) to memory under
the `/AssetTest/` mount point, destroy the packages and load them back, checking the bulk data (texels, geometry,
tracks, samples) and the references between the packages. The animation tests (`System.Engine.Animation.*`) build
their skeletons and clips as UObjects.

Since P14's second part the engine content is packages: `System.Engine.EngineContent.*` load them (the defaults
`BaseEngine.ini` names, the basic shapes and the runtime spheres of other tessellations, the migrated materials with
their `.lmat` parameters, the template map's plane with `M_WorldGrid`, `T_Default_D`'s import data and its source's
MD5) and check that the scene keeps the assets its proxies draw alive. They only read `Engine/Content`: no test writes
there (tests write under `<Project>/Intermediate/Tests/`, the program's `Engine/Programs/LeonAutomationTests/`, which
git ignores).

The editor module's tests (`System.LeonEd.*`) run the factories and the commandlets as LeonCook does, under a
`/LeonEdTest/` mount point over `<Project>/Intermediate/Tests/LeonEd/` (a fresh folder per test, deleted with its
packages at the end), from source files they write themselves (BMP, WAV, OBJ with an MTL, an ASCII FBX, a glTF with
an external buffer): each factory's asset and import data, the materials and textures a mesh import makes, import
lists and settings, importing over an asset in place, `ReimportIsReproducible` (the bytes of a reimport from an
unchanged source equal the first import's, gate G5 in small; a changed source changes the asset and its MD5; a missing
source is skipped), resave, validation (an import whose package is gone) and the cook of a folder for PS2 (no import
data in a cooked package, its platform recorded, an unknown platform refused). `System.LeonEd.MapFactory.*` import `MapFixture.gltf`
(`Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/`, written by `MakeMapFixture.py` next to it: a node of
every naming convention, lights, a textured material, waypoint extras) with a project's rules, check every actor, mesh,
collision box, material and light, that importing over the map and `-reimport` save the same bytes, and that a
missing required tag fails the import.

Since P16 the cook and the paks are tested too. `System.PakFile.*` (5, in `LeonAutomationTests` and in TestPAL on every
platform, the PS2 included) build paks in memory with `FPakWriter`: the round trip (every entry's bytes, an empty file,
lookups that ignore case, the footer's magic, the index sorted by hash), deterministic output whatever the order the
files come in, `-align=2048`, `Check` finding a changed byte (and a changed index, a wrong magic or a truncated file
refusing to open), and `FPakPlatformFile` mounted at a folder that does not exist on disk: files that exist, read,
seek, list and stat there, read-only, reached through `IFileManager` and `FFileHelper` once it is the topmost platform
file, a patch pak with a higher order winning, loose files refused (desktop), and `../../../` taken from the executable's
folder. `System.Engine.PakFile.LoadsAssetFromPak` loads an engine static mesh from a pak through `LoadPackage`.
`System.LeonEd.Cook.*` check the target platforms and the default seeds (the maps the config names, the default assets,
`BaseGame.ini`'s basic shapes, not a map nothing opens), the dependency closure (hard imports, soft references, what
nothing references, a dangling soft reference, a missing import), and an imported map cooked twice for PS2: the same
bytes, no import data in the map or its textures, `CookedPlatform` "PS2", the config staged without the Editor ini, the
shaders. `System.CoreUObject.Package.EditorOnlyData` checks that a filtered package leaves an editor-only object out.
`System.Engine.Viewport.IgnoreInput` checks that an ignored mouse sample leaves the view as the map put it.

Since P17 the collision channels and UE's movement model are tested. `System.Engine.CollisionChannel.*` (8) check the
response container, the raw bodies' defaults (the old channel filter), traces following the responses (the smaller of
the body's and the query's), object-type queries, a component's settings, traces hitting a pawn's capsule while
characters walk past each other's, and the config's named channels. The new `System.Engine.CharacterMovement.*`
tests (12, `CharacterMovementModelTests.cpp`) run UE's model on a test character (`AEngineTestCharacter`): acceleration
to the speed, braking to a stop, ground friction turning the velocity, air control keeping the momentum, crouching
(the capsule, the speed, the agent flag, in the air) and standing up only with room under a ceiling, the
`GetMaxSpeed` hook, the pawn's input vector, the first-person camera following the control rotation and the mouse
sensitivity. The default (instant) model keeps every golden table as it was.

ShooterGame's tests (`ShooterGame.*`, 10, in `ShooterGameTests.exe` with the project's config) cover the team choice,
ten bots on ten team starts and a sixth refused, a pawn standing on its start, `bot_fill`, the character's CS movement
(UE's model, the run and walk speeds, crouching, the capsule, the first-person camera), the crosshair the HUD draws,
the project's input and channel config, and the map: `ShooterGame.Map.DeLeonHoldsTheGame` loads `/Game/Maps/de_leon`
and checks its sites, buy zones, team starts, waypoint links, player clip and sun; `RequiredTags` imports
`de_leon.glb` under the project's rules and refuses the AxisTest source; `TenPawnsOnDeLeon` opens the map in a
headless `UGameEngine`, adds nine bots and ticks 60 frames: ten pawns standing on distinct starts, on the spawn pads.

The golden tests (`System.Engine.Golden.*`, `System.AIModule.Golden.*`, `System.JoltPhysics.Golden.*`) replay
movement, traces, navigation, cameras, shadows and reflections against tables recorded before P7 moved the world to
UE's axes, so any change of sign or unit fails them. They convert the tables with `FLegacyCoordinateConversion`, which
lives in the tests only since P15 (RenderCore's `Public/Tests` and `Private/Tests`); `System.Engine.Golden.StarterLevel`
reads the Starter's meshes, light and camera framing from `/Engine/Maps/Template_Default`.

`-Screenshot=<file.bmp>` saves frame `-ExitAfterFrames=N` (default 60) as a 24-bit BMP and exits. A run of
`LeonGame.exe -ExitAfterFrames=300` should log `RequestEngineExit: ExitAfterFrames`, the `LogGarbage` lines of the
level load and of the exit (the world teardown in `PreExit`, which also frees the level's assets, then the
engine itself) and no errors; it exits with code 0. The same holds headless (`-nullrhi`). ShooterGame's captures
use its view commands: `ShooterGame.exe "-ExecCmds=bot_fill;ViewFrom 0 0 5600 -89 0" "-Screenshot=<file.bmp>"
"-ExitAfterFrames=30"` shows the whole of de_leon from above (north up) with the teams on their spawns. `-AxesGizmo` turns
the axes gizmo on from the start (see below); captures without it do not change.

## Axes gizmo

**F6** in `LeonGame` (or `-AxesGizmo` on the command line) draws two things over the frame, X red, Y green and Z
blue as in UE:

- 1 m axes at the world origin, drawn in front of the scene;
- a 96-pixel gizmo in the bottom-left corner showing the view orientation: the world axes through the camera
  rotation only, projected orthographically. An axis pointing into the screen shrinks to a dot; the axis nearest the
  viewer is drawn last.

`FDebugDraw::AddAxes(Origin or FTransform, Length = 100)` and `FDebugDraw::AddViewAxes(View)` (Engine,
`Public/Debug/DebugDraw.h`) draw them; the viewport client's `EngineShowFlags.AxesGizmo` switches the gizmo
(`show AxesGizmo`). It is off by default.

## Manual checklist: axes and units (P7)

Run `Engine\Binaries\Win64\LeonGame.exe -AxesGizmo` (the default template map, free-look camera; the cursor is
captured, close the window to quit), or `LeonGame.exe /Engine/Maps/AxisTest -AxesGizmo` ([LEVELS.md](LEVELS.md#axistest):
the red cube ahead on +X, the green one on the right on +Y, the blue one up on +Z). The world is X forward, Y right, Z
up, left-handed, 1 unit = 1 cm.

- [ ] **Gizmo colours**: red, green and blue lines leave the world origin; blue points straight up. In the corner
  gizmo blue points up the screen whenever the view is level.
- [ ] **Handedness**: look straight down (pull the mouse towards you until the pitch stops at -89°) and turn until red
  points up the screen: green must point to the **right**. With green on the left the world is right-handed.
- [ ] **Mouse X**: moving the mouse right turns the view right (clockwise seen from above; the yaw grows). The view
  turns about 0.3° per pixel, as before P7 (since P13 once, through the `AxisConfig` sensitivity of `BaseInput.ini`).
- [ ] **Mouse Y**: moving the mouse away from you looks up (positive pitch); the pitch stops at ±89°.
- [ ] **WASD**: turn until red points into the screen (the corner red line shrinks to a dot). **W** moves towards
  +X (the origin axes come closer if you are behind them), **S** away, **D** to the right (+Y), **A** to the left.
  The movement follows the view, including its pitch.
- [ ] **Q / E**: **E** moves up (+Z, the floor drops away), **Q** down.
- [ ] **Shadows**: shadows fall away from the sun and stay attached to the objects that cast them; **F1** draws the
  shadow volume (yellow) around the level, not beside it.
- [ ] **Mirror**: on a level with a `PlanarMirror=true` material ([ASSET_FORMATS.md](ASSET_FORMATS.md)), objects above
  the mirror appear upside down under them, with left and right kept (a red object on the left of a blue one stays on
  the left in the reflection).
- [ ] **Free-look camera**: the view never rolls while turning or looking up and down, and the Starter level opens
  with the same view as before P7 (the frame captures match).
- [ ] **Orbit camera**: `LeonGame`'s default pawn flies with a free-look camera, so the orbit camera cannot be
  driven there. Its mapping (`(-Pitch, Yaw + 180, 0)` from the legacy angles) is covered by
  `System.Engine.Golden.OrbitCameraNdc` and the camera tests. The PS2 ThirdPerson orbit boom (right stick) keeps its
  own frame and did not change in P7.
- [ ] **Jump**: `LeonGame` has no character. The jump (+Z at `JumpZVelocity`, 700 cm/s) is covered by
  `System.Engine.Golden.JumpArc`. On PS2, Cross still jumps up in ThirdPerson.
- [ ] **Sound panning**: the audio device converts the listener and sound positions (Y and Z swap, ×0.01 to metres),
  but no level or game mode plays a positioned sound yet, so there is nothing to listen to. When one does, a sound on
  +Y of a listener looking along +X must come from the **right** speaker.
