# Testing

What runs automatically and what a person still has to check by hand. Build and test commands:
[SETUP.md](SETUP.md#run-the-tests); coding rules for tests: [CODING_STANDARD.md](CODING_STANDARD.md).

## Automated

| Check | Command | Passes when |
| --- | --- | --- |
| Automation tests (Win64) | `Engine\Build\BatchFiles\RunTests.bat [-automation=<filter>]` | `Automation: N test(s), N passed, 0 failed` |
| LeonHeaderTool golden tests (run by `RunTests.bat` too) | `Engine\Intermediate\Build\HostTools\Win64\LeonHeaderTool.exe -Test` | `LeonHeaderTool -Test: N of N golden cases passed` |
| Core, CoreUObject, Json and Projects on PS2 | `Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build` | `TestPAL: PASSED (106 test(s), 0 failed)` in the EE log (112 on Win64) |
| Format, banned APIs (G4), Win64 build | `Engine\Build\BatchFiles\Lint.bat` | `Lint OK` |
| Reproducible reimport (G5; CI, on a clean checkout) | `Engine\Build\BatchFiles\CheckReimport.bat [<Project>.lproj ...]` | `CheckReimport OK`: `LeonCook -run=ImportAssets -reimport -all` leaves `Engine/Content` and `Game/*/Content` unchanged (`git diff --exit-code`, no new file) |
| Content loads | `Engine\Binaries\Win64\LeonCook.exe -run=ValidateAssets` | `ValidateAssets: N packages, N valid, 0 problem(s)` |
| Frame capture | `LeonGame.exe "-map=<map>" "-Screenshot=<file.bmp>" "-ExitAfterFrames=N"` | the BMP matches a reference capture byte for byte |
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

Since P13 the level content is actors. The level tests load `.llev` documents into a test world and check the
spawned actors (`GetAllActorsOfClass`), their physics bodies and, with
`System.Engine.LevelFormat.SaveWritesTheSameBytes`, that saving a loaded level gives the bytes the saver wrote before
P13 (stored MD5 hashes: update them only when the format changes on purpose). `LeonAutomationTests` links the Renderer, so test worlds have the Renderer's `FScene` (it
needs no GPU: the GPU copies are made only when a frame is drawn), and
`System.Engine.Components.SceneProxiesFollowTheComponents` checks that the proxies follow their components.

Since P13's second part the engine is a UObject and the game starts through `UEngine::LoadMap`. The tests make their
own `UGameEngine` (`TStrongObjectPtr`, `Init(nullptr)` runs it headless) when they need one:
`System.Engine.LoadMap.StarterLogsInThePlayer` opens the Starter map and checks the login (the controller, the default
pawn at the Play From Here start, the HUD and the begun play), `System.Engine.LoadMap.GameModePrecedence` the game mode
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
their `.lmat` parameters, the `.llev` key of the Starter's material, `T_Default_D`'s import data and its source's
MD5) and check that the scene keeps the assets its proxies draw alive. They only read `Engine/Content`: no test writes
there (tests write under `<Project>/Intermediate/Tests/`, the program's `Engine/Programs/LeonAutomationTests/`, which
git ignores). `System.Engine.LegacyAssetKeys.*` cover the content keys of the `.llev` levels (migrated names, candidate
packages, mount points named after folders), and `System.Engine.LevelFormat.SaveWritesTheSameBytes` loads its mesh
record from a package saved next to its level.

The editor module's tests (`System.LeonEd.*`) run the factories and the commandlets as LeonCook does, under a
`/LeonEdTest/` mount point over `<Project>/Intermediate/Tests/LeonEd/` (a fresh folder per test, deleted with its
packages at the end), from source files they write themselves (BMP, WAV, OBJ with an MTL, an ASCII FBX, a glTF with
an external buffer, `.lmat` and `.lmesh` files): each factory's asset and import data, the materials and textures a
mesh import makes, import lists and settings, importing over an asset in place, `ReimportIsReproducible` (the bytes of
a reimport from an unchanged source equal the first import's, gate G5 in small; a changed source changes the asset and
its MD5; a missing source is skipped), resave, validation (an import whose package is gone), the minimal cook (no
import data in a cooked package) and the legacy content migration.

The golden tests (`System.Engine.Golden.*`, `System.AIModule.Golden.*`, `System.JoltPhysics.Golden.*`) replay
movement, traces, navigation, cameras, shadows and reflections against tables recorded before P7 moved the world to
UE's axes, so any change of sign or unit fails them.

`-Screenshot=<file.bmp>` saves frame `-ExitAfterFrames=N` (default 60) as a 24-bit BMP and exits. A run of
`LeonGame.exe -ExitAfterFrames=300` should log `RequestEngineExit: ExitAfterFrames`, the `LogGarbage` lines of the
level load and of the exit (the world teardown in `PreExit`, which also frees the level's assets, then the
engine itself) and no errors; it exits with code 0. The same holds headless (`-nullrhi`). `-AxesGizmo` turns
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

Run `Engine\Binaries\Win64\LeonGame.exe -AxesGizmo` (the Starter level, free-look camera; the cursor is captured,
close the window to quit). The world is X forward, Y right, Z up, left-handed, 1 unit = 1 cm.

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
