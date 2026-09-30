# Testing

What runs automatically and what a person still has to check by hand. Build and test commands:
[SETUP.md](SETUP.md#run-the-tests); coding rules for tests: [CODING_STANDARD.md](CODING_STANDARD.md).

## Automated

| Check | Command | Passes when |
| --- | --- | --- |
| Every local gate ([ps2-shipping](PLANS/ps2-shipping.md) N1; the repository has no CI) | `Engine\Build\BatchFiles\RunGates.bat [-PS2] [-Measure]` | `RunGates OK`: Lint (G1, G4, /W4), RunTests, CheckReimport (G5, the engine and ShooterGame content), SmokeTest (G6), BotMatch (`10 7`, played twice) and ValidateAssets (engine and ShooterGame) each print `[ OK ]` (logs in `Engine\Saved\Gates\`); `-PS2` adds `Package.bat -NoWin64` (G3), `-Measure` `MeasurePS2.bat` (below) |
| The PS2 frame in PCSX2 ([ps2-shipping](PLANS/ps2-shipping.md) N1, N9) | `Engine\Build\BatchFiles\MeasurePS2.bat [-Project Game\ShooterGame] [-Rounds 2] [-Seed 7] [-Seconds 120] [-NoBuild] [-TimeoutSeconds 900] [-Label <text>] [-Iso] [-PakOrder <order file>] [-LogFileOpenOrder] [-ExtraArgs <game arguments>]` (Docker and PCSX2, unattended; `-ExtraArgs -novu1` measures the EE's C++ emitter; the disc switches are [below](#ps2-disc-boot)) | `MeasurePS2 OK`: PCSX2 runs the staged ShooterGame without its window, from `Game\ShooterGame\Saved\PCSX2` (the user's `PCSX2.ini` with `Engine\Platforms\PS2\Build\PCSX2\Measure.ini` on top), a bot match watched through a bot's eyes (`-BotMatchSpectate -LogFrameTimes -ExitAfterSeconds`) until the game's `FrameStats Summary:` and `ProfileSummary:` lines (the cycle stats' top level scopes, N9); the figures go to `Saved\Profiling\PS2Frame.csv` (the `ProfileSummary:` pairs as `Profile_<key>` columns), and a [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md) row and the last `Profile over N frames (ms, calls):` block (the frame's scopes as a hierarchy) are printed. Three runs agree within 3 % |
| Automation tests (Win64) | `Engine\Build\BatchFiles\RunTests.bat [-automation=<filter>]` | `Automation: N test(s), N passed, 0 failed` twice, the engine's (`LeonAutomationTests`, 575 at 0.24.0, 588 at [ps2-polish](PLANS/ps2-polish.md) P5; `-nodisplay` skips the `NonNullRHI` tests, which need an OpenGL window) and ShooterGame's (`ShooterGameTests`, 105), between them `LeonHeaderTool -Test: 35 of 35 golden cases passed`, then `TestPAL: PASSED (171 test(s), 0 failed)` (TestPAL on Win64) |
| GS emulator conformance ([ps2-gs-parity](PLANS/ps2-gs-parity.md) P4, [ps2-engine](PLANS/ps2-engine.md) E2, [ps2-shipping](PLANS/ps2-shipping.md) N8) | `LeonAutomationTests -automation=GSEmulator` (it needs an OpenGL window) | `System.Renderer.GSEmulator.Conformance`: the OpenGL GS emulator draws the 21 GS conformance scenes within 2 levels per channel of the reference rasterizer, but for at most 8 pixels a scene (today 3 in StripsAndSprites, pixel centres on a shallow side, and 3 in MipmapLod, a minified bilinear weight); `System.Renderer.GSEmulator.SceneFrame`: a frame of the GS scene renderer (textured, flat and translucent meshes, two lights, a floor through the near plane; the texture as the PS2 cook makes it, PSMT8 with its mips, trilinear, since ps2-shipping N13) within one 5-bit step of the reference, but for at most 64 pixels |
| LeonHeaderTool golden tests (run by `RunTests.bat` too) | `Engine\Intermediate\Build\HostTools\Win64\LeonHeaderTool.exe -Test` | `LeonHeaderTool -Test: 35 of 35 golden cases passed` |
| Core, CoreUObject, Json, Projects and PakFile on PS2 | `Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL -Build` | `TestPAL: PASSED (N test(s), 0 failed)` in the EE log: 157 at [ps2-shipping](PLANS/ps2-shipping.md) N15, against 164 on Win64 then (the platform-file, config-cache, log-file, SaveConfig and package-file tests are desktop-only); not run again on the EE since N24's tests (171 on Win64 at 0.24.0). N15's `System.Core.Math.VectorMathVU0` compares VU0 with the scalar reference there: at most 2 units in the last place of the products' magnitudes |
| VU1's microprograms against the C++ emitter on PS2 ([ps2-shipping](PLANS/ps2-shipping.md) N14, N14b, N29, D2) | `Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program VU1Conformance -Build` (or the capture of a PCSX2 run) | `VU1Conformance: PASSED (84 batch(es), 0 failed)` in the EE log, after the line with the largest differences (XY 0, Z 5, RGBA 1, STQ 4 ulp, F 0; N15's two fogged draws, N14b's skinned batches: 1 to 24 bones turned, scaled and moved, unlit, lit and textured, mirrored and fogged; N29's lit draws with one and two point lights, static and skinned; the tolerances are XY 1, Z 8, RGBA 1, STQ 8 ulp, F 1); the screen shows the batches twice, VU1's (left) as the emitter's (right). Play!'s leonrun is not built on this host: PCSX2 is where it runs |
| Format (G1), banned APIs (G4), Win64 build | `Engine\Build\BatchFiles\Lint.bat` | `Lint OK` (the format check expects clang-format 20; 20.1.8 is the reference version) |
| PS2 builds and ELF sizes (G3) | `Engine\Build\BatchFiles\Build.bat <Target> PS2 Development` (Docker, EE `-O2`), or the root `Package.bat -NoWin64` for ShooterGame, TestPAL, GSConformance and VU1Conformance | those four and `BlankProgram` build (VU1's `.vsm` microprograms assembled by `dvp-as`); when a phase is recorded, their sections are measured with the toolchain's `mips64r5900el-ps2-elf-size` in the ps2dev image ([Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md)) |
| PS2 boot without a console BIOS (a Linux host tool, built in Docker or WSL; [PS2SDK.md](../Engine/Platforms/PS2/Documentation/PS2SDK.md)) | `sh Engine/Platforms/PS2/Build/PlayRunner/BuildPlayRunner.sh` (prints the runner), then `leonrun <Stage>/ShooterGame.elf 300` with `-nullrhi -benchmark -botmatch -rounds=2 -seed=7` in the stage's `LeonCommandLine.txt` | `Botmatch OK: 2 round(s), ...` on the terminal; an ELF alone in its folder logs `No Engine config in host:Engine/Config/` (the error screen) instead of hanging |
| ShooterGame headless on PS2 ([ps2-engine](PLANS/ps2-engine.md) E1) | `Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -run "-addcmdline=-nullrhi -benchmark -botmatch -rounds=10 -seed=7"` (Docker and PCSX2 with its host filesystem) | `Botmatch OK: N round(s), ...` in the EE log (the match ends when a team has won the majority, N30d), the same result on two runs; the `Botmatch budget:` line recorded in [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md) |
| ShooterGame on PS2 ([ps2-engine](PLANS/ps2-engine.md) E2 to E4) | `Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -pak -run "-addcmdline=-LogFrameTimes"` (Docker and PCSX2 with its host filesystem) | de_leon draws with the player, the bots, the tracers and the HUD; the DualShock plays it (the buy menu too); the EE log's `Frame times over ...` lines stay at 33.4 ms average (30 fps) |
| Reproducible reimport (G5; on a clean checkout) | `Engine\Build\BatchFiles\CheckReimport.bat [<Project>.lproj ...]` | `CheckReimport OK`: `LeonCook -run=ImportAssets -reimport -all` leaves `Engine/Content` and `Game/*/Content` unchanged, the imported maps included (`git diff --exit-code`, no new file) |
| ShooterGame smoke (G6) | `Engine\Build\BatchFiles\SmokeTest.bat` | `SmokeTest OK: 10 pawns, CT 5, T 5, exit code 0`: ShooterGame boots de_leon headless, `bot_fill` adds nine bots to the local player, and the game mode's end-of-match line counts ten pawns in their teams |
| ShooterGame bot match (P21) | `Engine\Build\BatchFiles\BotMatch.bat [Rounds] [Seed]` (10, 7) | `BotMatch OK: 10 round(s), seed 7, exit code 0, replayed identically` (the game's line at 0.24.0: `Botmatch OK: 9 round(s), CT 3 - T 6, 55 kill(s), seed 7, sides switched after round 5`; the match ends at the majority): ten bots play de_leon headless and unpaced at the fixed 30 Hz step ([ps2-shipping](PLANS/ps2-shipping.md) D4: a run paced to the clock or drawn in a window plays the same match) (`ShooterGame -nullrhi -benchmark -botmatch -rounds=10 -seed=7`), `FShooterMatchChecker` finds no broken invariant, and a second run logs the same `Botmatch OK` line ([ShooterGame README — Bot match](../Game/ShooterGame/README.md#bot-match)) |
| Content loads | `Engine\Binaries\Win64\LeonCook.exe -run=ValidateAssets` | `ValidateAssets: N packages, N valid, 0 problem(s)` |
| Frame capture | `LeonGame.exe [<map>] "-Screenshot=<file.bmp>" "-ExitAfterFrames=N"` | the BMP matches a reference capture byte for byte; a capture is unattended (`FApp::IsUnattended`) and ignores the mouse and the keyboard, so moving the mouse during it changes nothing |
| Staged build (Win64) | `BuildCookRun.bat -project=<.lproj> -platform=Win64 -build -cook -stage -pak -run "-addcmdline=-Screenshot=<file.bmp> -ExitAfterFrames=30"` | the staged Shipping game's capture matches the Development build's byte for byte; two `-cook -stage -pak` runs give the same `.lpak` (SHA-256); it also runs in Development, headless (`-nullrhi -ExitAfterFrames=60`, exit code 0), for a content-only project and for ShooterGame (`-ExecCmds=bot_fill`), and in Shipping for ShooterGame playing three rounds of a bot match (`-nullrhi -benchmark -botmatch -rounds=3 -seed=7`, exit code 0) |
| Pak tool | `LeonPak <in.lpak> -test` / `-list` | `N file(s) checked, every SHA-1 matches` |
| Console commands | `LeonGame.exe "-ExecCmds=obj gc;stat fps,stat fps" "-Screenshot=<file.bmp>" "-ExitAfterFrames=30"` | a `Cmd:` line per command, the capture unchanged |

The CoreUObject tests collect garbage (`CollectGarbage`) between their steps; they only keep objects through
`UPROPERTY` members, the root set, `FGCObject` and `TStrongObjectPtr`, and read the others through weak pointers, so
a collection in one test never touches another test's objects. The config tests build their ini layers in memory
(`FConfigFile::CombineFromBuffer`) and remove them afterwards; the SaveConfig test (desktop only) writes its user
layer under `<Project>/Intermediate/Tests/CoreUObjectConfig/` and deletes it, and
`System.Core.Config.UserLayerArraysAndRemovals` (desktop only) saves a user layer under
`<Project>/Intermediate/Tests/ConfigArrays/` and checks that it gives back the arrays (a one-value array replaces the
whole array, duplicates stay) and the removed keys.

The gameplay tests (Engine, AIModule) work on UObjects since P12. A test that spawns actors creates its
world with `FScopedTestWorld` (`Engine/Public/Tests/ScopedTestWorld.h`): `UWorld::CreateWorld` at the start of the
scope, and at its end `DestroyWorld` (every actor ends play) and a full garbage collection, so the next test starts
without them. Components, cameras, levels, HUDs and game states made outside a world come from `NewObject`; nothing is
declared by value. Such a free object is collected by the next safe point, so a test that ends a test world early
declares the objects it still needs before that world, or holds them in `TStrongObjectPtr`. Reflected fixtures (an actor
that spawns during its tick, test pawns, controllers and a component that counts its calls) live in
`Engine/Private/Tests/EngineTestTypes.h` and `AIModule/Private/Tests/GameplayTestTypes.h`. `System.Engine.World.*`,
`System.Engine.Components.*` and `System.Engine.GameFramework.*` cover the spawn and destroy sequence, ownership and
collection, attachment rules and sockets, the primitive render state, the game mode, game state and match states, the
HUD widgets and anim instances as objects, and the engine's collection timer. `System.UMG.WidgetTree.LayoutAndPaint`
lays out and paints a widget tree (a canvas panel holding a border around a vertical box of a text and images, one of
them collapsed): the desired sizes add up as UE's and every widget paints in the rectangle its slot gives it.

Since P13 the level content is actors, and since P15 a map is a `.lmap` package.
`System.Engine.MapPackage.SaveLoadRoundTripsEveryActor` saves a world with one actor of every class a map holds (world
settings, a mesh with collision and a spin, the three volumes, a tagged player start and target point, both lights, a
camera actor) under a `/MapTest/` mount point and loads it back, value by value and transform by transform;
`LoadMapOpensMapPackages` opens it with `UEngine::LoadMap` by name and by file (the content folder mount);
`RotatingMovementTurns` ticks the rotating movement component. `System.Engine.Damage.PainCausingVolume` checks that a
pain-causing volume hurts the pawns inside it every `PainInterval`.
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
`BaseEngine.ini` names, the basic shapes with their generators' geometry, the migrated materials with
their `.lmat` parameters, the template map's plane with `M_WorldGrid`, `T_Default_D`'s import data and its source's
MD5) and check that the scene keeps the assets its proxies draw alive. They only read `Engine/Content`: no test writes
there (tests write under `<Project>/Intermediate/Tests/`, the program's `Engine/Programs/LeonAutomationTests/`, which
git ignores).

The editor module's tests (`System.LeonEd.*`) run the factories and the commandlets as LeonCook does, under a
`/LeonEdTest/` mount point over `<Project>/Intermediate/Tests/LeonEd/` (a fresh folder per test, deleted with its
packages at the end), from source files they write themselves (BMP, WAV, glTF with an external image or an
embedded one) and the glTF fixtures of `MeshUtilities/Private/Tests/Fixtures` (`Cube.glb`, `SkinnedArm.glb`,
written by `MakeSkinnedFixture.py`): each factory's asset and import data, the materials and textures a mesh import makes, import
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
characters walk past each other's, and the config's named channels. The new `System.Engine.CharacterMovement.*` tests
(12, `CharacterMovementModelTests.cpp`; 14 with the later `LongFramesKeepTheFloor` and `ZeroStepKeepsTheVelocity`) run
UE's model on a test character (`AEngineTestCharacter`): acceleration to the speed, braking to a stop, ground friction
turning the velocity, air control keeping the momentum, crouching (the capsule, the speed, the agent flag, in the air)
and standing up only with room under a ceiling, the `GetMaxSpeed` hook, the pawn's input vector, the first-person camera
following the control rotation and the mouse sensitivity. The default (instant) model keeps every golden table as it
was.

ShooterGame's tests (`ShooterGame.*`, 105 now, in `ShooterGameTests.exe` with the project's config) cover, since P17, the
team choice, ten bots on ten team starts and a sixth refused, a pawn standing on its start, `bot_fill`, the character's
CS movement (UE's model, the run and walk speeds, crouching, the capsule, the first-person camera), the crosshair the
HUD draws, the project's input and channel config, and the map: `ShooterGame.Map.DeLeonHoldsTheGame` loads
`/Game/Maps/de_leon` and checks its sites, buy zones, team starts, waypoint links, player clip, ladders and sun; `RequiredTags`
imports `de_leon.glb` under the project's rules and refuses the AxisTest source; `TenPawnsOnDeLeon` opens the map in a
headless `UGameEngine`, adds nine bots and ticks 60 frames: ten pawns standing on distinct starts, on the ground.
`ShooterGame.Input.Pad` and `ShooterGame.Input.BuyMenuTakesItsKeys` (E4) drive a player with the PS2 pad's keys: the
right stick turns at `BaseTurnRate`, the left one walks, the shoulders draw the slots, and the buy menu takes the
D-pad, Cross, Circle and the number keys only while it is open (closed, 1 draws the rifle); `System.Engine.Viewport.Gamepad`
feeds a fake pad through the viewport client. [ps2-polish](PLANS/ps2-polish.md) P4 adds
`ShooterGame.Input.CrouchToggleAndHold` (a tap of Left Ctrl or Circle crouches and the next stands up, the option off
holds it, a new round stands up), `.DropAndPickUpWeapon` (G drops the rifle, walking over it takes it back with its
clip, reserve and silencer, the HUD's `Picked up m4a1` for its two seconds, a full slot leaves a weapon) and
`.DropAndPickUpBomb` (5 and the D-pad's down draw the C4, a weapon's key puts it away, G drops it ahead, its dropper
takes it back only after its delay, `Picked up C4`, a dead carrier drops it and its rifle);
`ShooterGame.Settings.RoundTrip` saves the crouch option and `ShooterGame.Config.InputAndChannels` checks the crouch,
bomb and drop keys. The fixes of [ps2-shipping](PLANS/ps2-shipping.md) N6 have one test
each: `ShooterGame.Weapons.BestWeaponSkipsEmpty`, `ShooterGame.Bomb.ExplosionRadius`,
`ShooterGame.Bomb.ExplosionRespectsArmor`, `ShooterGame.Bots.RecoilKicksTheAim`, `ShooterGame.Buy.MenuFollowsTheRules`
and `ShooterGame.Buy.BuyTimeAfterTheFreeze`. N30d's halftime, spectating and HUD have theirs:
`ShooterGame.Rounds.HalftimeSwitchesSides` (the sides, the scores, the money, the spawns and the match checker at the
halftime), `.MatchEndsAtTheMajority`, `ShooterGame.Spectate.DeathCamThenTeammates` (the death cam's aim and length,
then a teammate, then the free look), `.CyclingSkipsTheDead` (the spectator's keys), `ShooterGame.HUD.Radar` (the
projection, the primitives of a 5v5 frame, no allocation) and `.DamageIndicator` (the arc's direction and its end).
N30c's CS movement is `ShooterGame.Movement.*` (`ShooterMovementTests.cpp`, at the fixed 30 Hz step): `FallDamage`
(the thresholds with N30e's multiplayer 1.25, lethal at 935 u/s, and drops of 3, 9 and 16 m: unhurt, hurt by the
landing speed's damage without armor or tagging, dead by the world in the kill feed), `Ladder` (grabbing a tagged volume, climbing at 508 cm/s and faster looking up, hanging
without gravity, climbing down looking down, the jump off at 686 cm/s, climbing over the top onto the roof, a bot
walking through it), `JumpStamina` (the ratio, the landing's speed step by step against the formula, full speed again
when the stamina runs out), `Tagging` (half the speed at a shot, three quarters half a second later, recovered after a
second; the world's damage does not tag) and `Footsteps` (the locomotion's notifies heard by an enemy bot while
running, silent walking, crouching and standing). N30a's CS arsenal (`ShooterArsenalTests.cpp`):
`ShooterGame.Arsenal.StatsTable` (every weapon's CS 1.6 values against a table, no other weapon class, the first
pistols' 12/24 and 20/40, a bought weapon's empty reserve), `.SilencerAndBurst` (the USP's and the M4A1's silencers:
no shot while it goes on, the silenced damage, falloff, spread and noise; the Glock's three-round burst and its 0.5 s
cycle), `.KnifeBackstab` (15 a slash, 65 a stab, 195 in the back, out of reach the air),
`ShooterGame.Weapons.Penetration` (the AK-47, the AWP, the USP and the Deagle through wood, concrete, metal and glass
walls, the engine's cube with test materials, against CS's power and damage shares; two walls; the penetration
distance; through a body) and `ShooterGame.Damage.HitGroups` (the seven groups' bands, multipliers and armor, the
helmet, crouched); `ShooterGame.Buy.AmmoAndPrices` (the boxes per calibre until full, the `,` and `.` keys) and
`.TeamRestrictions` (the AK-47 and the M4A1 by team, the buy menu's pages). N30b's grenades
(`ShooterGrenadeTests.cpp`): `ShooterGame.Grenades.FlashIntensity` (CS's hold, fade and white by the view's angle and
the distance, out of range, behind a wall, then the fade), `.FlashBlindsBots` (a flashed bot traces to nobody and
fires blind around where it saw its enemy for a third of the fade, the same with the same seed, then engages), `.SmokeBlocksSight` (a cloud hides the enemy from a bot's line of
sight until it thins out, its puffs, gone with them) and `.CarryLimits` (two flashbangs, one HE, one smoke, the grenade
key's cycle, a thrown flashbang leaving the other, the HUD's white); the engine's `System.Renderer.Effects.EffectSprites`
(the sprite pool's fades, recycling and removal; two triangles square to the view, the farthest first). N30e's bots
(`ShooterBotTacticsTests.cpp`): `ShooterGame.Bots.EcoAndForceBuy` (the buy plan's rules and a 2v2's plans and
purchases through three rounds), `.BuysGrenadesWithinLimits`, `.ThrowsGrenades` (the throw's pitch, the terrorist's
flashbang on the way into the site, an HE at a reported spot, the same throw with the same seed) and
`.StrafeCrouchAndStand` (the strafe's legs and its determinism, the rifle's crouch at range, the AWP standing); the
radio (`ShooterRadioTests.cpp`): `ShooterGame.Radio.BotsReportEvents` (the bots' messages on the game's events, the
teammates acting on them, the HUD's team lines), `.SectorClear`, `.PlayerMenu` (Z, X and C with the number keys, the
cooldown, the bot's answer) and `.Sounds` (N30f: each menu's sound, the team hears its own); the surfaces
(`ShooterSurfaceTests.cpp`, N30f): `ShooterGame.Surfaces.Footsteps` (the floor's physical material and both feet's
steps on each surface), `.LadderSteps` and `.Impacts` (a shot's surface and sound on each, the marks, CS's `bhit_`);
`ShooterGame.Weapons.Penetration` reads the walls' physical materials. The engine's `System.Engine.PhysicalMaterial.*`
trace a mesh of two slots with and without `bReturnPhysicalMaterial` and name the surfaces,
`System.LeonEd.Factories.PhysicalMaterials` imports a glTF material's `physMaterial`.

Since P20 the bots are tested. The waypoint navigation (`System.AIModule.Gameplay.Navigation*`,
`System.AIModule.FrameworkHardening.NavigationAgentRadiusKeepsWideAgentsOutOfGaps`) finds paths over a small graph,
around walls, and links steps, jumps and drops (`AutoLinkWaypoints`); `System.LeonEd.MapFactory.AutoLinksWaypoints`
imports a map with the auto-linking; `System.AIModule.Blackboard.TypedKeys` and `System.AIModule.PawnSensing.*` (sight
in a cone behind a line of sight, hearing within the loudness' range) test the AI's pieces. ShooterGame's
`ShooterGame.Bots.*` (10, `ShooterBotTests.cpp`) test the bots on a small open map (buying, engaging with the reaction
time respected, the carrier planting, a CT defusing, the terrorists escorting the carrier, an outnumbering team hunting,
a CT rotating between the sites), the agent read from the config (`AgentFromConfig`), and play three rounds of de_leon
headless with ten bots and `?seed=5` under `FShooterMatchChecker` (each round ends with a reason, the scores add up, the
money stays within [0, 16000], no pawn falls through the floor) and kills happen; `MatchCheckerFlagsViolations` shows
the checker catches a score the rules did not give. The bot match (P21, `BotMatch.bat`) runs the same checker over ten
rounds and plays them twice: a seed must replay the same match, which caught a read of a freed path in `AAIController`'s
repath (the bots then diverged between runs; valgrind reports no error since the fix).
`System.AIModule.Gameplay.AIControllerPathFollowReadsWaypointFlags` checks that a controller jumps at a `Jump` waypoint
and crouches along a `Crouch` one. The round and weapon tests keep the bots still (`bot_stop`, or a controller that does
not tick) so they test the rules alone.

The golden tests (`System.Engine.Golden.*`) replay movement, traces, cameras, shadows
and reflections against tables recorded before P7 moved the world to UE's axes (the navigation's and the AI's went
with the grid navigation in P20), so any change of sign or unit fails them. They convert the tables with `FLegacyCoordinateConversion`, which
lives in the tests only since P15 (RenderCore's `Public/Tests` and `Private/Tests`); `System.Engine.Golden.StarterLevel`
reads the Starter's meshes, light and camera framing from `/Engine/Maps/Template_Default`.

`-Screenshot=<file.bmp>` saves frame `-ExitAfterFrames=N` (default 60) as a 24-bit BMP and exits. A run of
`LeonGame.exe -ExitAfterFrames=300` should log `RequestEngineExit: ExitAfterFrames`, the `LogGarbage` lines of the
level load and of the exit (the world teardown in `PreExit`, which also frees the level's assets, then the
engine itself) and no errors; it exits with code 0. The same holds headless (`-nullrhi`). ShooterGame's captures
use its view commands: `ShooterGame.exe "-ExecCmds=bot_fill;ViewFrom 0 0 5600 -89 0" "-Screenshot=<file.bmp>"
"-ExitAfterFrames=30"` shows the whole of de_leon from above (north up) with the teams on their spawns. `-AxesGizmo` turns
the axes gizmo on from the start (see below); captures without it do not change.

The asynchronous IO, the saves and the pad ([ps2-shipping](PLANS/ps2-shipping.md) N24):

- `System.Core.AsyncIO.*` (also in TestPAL, so on the EE's IO thread): `Order` (the highest priority first; within one
  the read nearest ahead of the last, then the first of the file, N24b; nothing read before the queue runs),
  `Coalesce` (N24b: the queued reads close together are one read of the file, a long gap or a far read one of its
  own, each request its own bytes), `Completion` (a read of several 64 KB chunks, into the
  request's memory and into the caller's, the size request, the callbacks on the game thread, a read past the end and
  one without a file failing) and `Cancel` (a cancelled request never reads and its callback says so; a request
  deleted unread leaves the queue); `System.PakFile.PlatformFile.AsyncRead` (a pak entry at its offset, from memory, and
  on the desktop from a pak file through its second handle without moving the game thread's);
- `System.CoreUObject.AsyncLoading.Delegates` (a package with its imports, a circular one too, loaded in the
  background: the delegate once with the package, a loaded package and a missing one at the next
  `ProcessAsyncLoading`, `CancelAsyncLoading`) and `.Files` (desktop: package files read once, asynchronously, and a
  `LoadPackage` of a package on its way taking its bytes);
- `System.Engine.SaveGame.RoundTrip` (`UGameplayStatics`' slot through the desktop's files: every kind of property,
  only what differs from the defaults written, a missing slot, damaged bytes) and `.MemoryCard`
  (`FMemoryCardSaveGameSystem` on a card in memory: no card, unformatted, no room for the folder and its icons, the
  folder with `icon.sys` and the icon, a second save in place, a card pulled out while writing, a damaged byte, the
  browser's title in Shift-JIS); `ShooterGame.Settings.RoundTrip` (the player's options in the `Settings` slot);
- `System.ApplicationCore.DualShock.Reconnect` (the analog mode, then the motors' alignment and the pressure mode, one
  command at a time and all again after a reconnection), `.ForceFeedback` (UE's four channels on the two motors, the
  motors sent when they change and not to a pad that went away) and `.Pressure` (the pressure bytes as axis keys);
  `System.Engine.ForceFeedback.PlayerController` (`ClientPlayForceFeedback`: the curves added per channel to the
  player's controller id, the end of an effect, a tag, a looping effect, vibration off); `System.Engine.Viewport.Gamepad`
  and `System.ApplicationCore.Desktop.GamepadAsDualShock` cover the second controller and the pressure axes;
  `System.ApplicationCore.Windows.XInputForceFeedback` (N24b, Win64: the channels on an Xbox pad's heavy and light
  motors, controller N on the N-th XInput pad, the motors sent when they change, a pad pulled out and plugged back, the
  motors stopped at the end; through a fake XInput, no pad needed).

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
- [ ] **Bounds**: **F1** draws each mesh's box around it, not beside it (nothing casts shadows since
  [ps2-gs-parity](PLANS/ps2-gs-parity.md) P4: the GS path has no shadow map and no mirror).
- [ ] **Free-look camera**: the view never rolls while turning or looking up and down, and the Starter level opens
  with the same view as before P7 (the frame captures match).
- [ ] **Orbit camera**: `LeonGame`'s default pawn flies with a free-look camera, so the orbit camera cannot be
  driven there. Its mapping (`(-Pitch, Yaw + 180, 0)` from the legacy angles) is covered by
  `System.Engine.Golden.OrbitCameraNdc` and the camera tests. (The PS2 ThirdPerson demo had its own orbit boom; the
  demo went in [ps2-shipping](PLANS/ps2-shipping.md) N2.)
- [ ] **Jump**: `LeonGame` has no character. The jump (+Z at `JumpZVelocity`, 700 cm/s) is covered by
  `System.Engine.Golden.JumpArc`. In ShooterGame, Space (Cross on the DualShock) jumps up.
- [ ] **Sound panning**: every platform pans with `FAudioDevice` (positions in metres, the pan across the listener's
  right, audsrv's volume steps; [ps2-shipping](PLANS/ps2-shipping.md) N19). In ShooterGame a shot on +Y of a listener
  looking along +X comes from the **right** speaker (`System.AudioMixer.Device.Spatialized` checks it).
- [ ] **Sounds on the SPU2** (N19): in PCSX2 the shots, reloads, the bomb's beeps and the explosion sound as on Win64
  (ADPCM at 22 050 Hz: a little duller than the old PCM mix, no clicks at the start or end, the pan following the
  camera); the EE log lists each sound `in SPU2 RAM` and no `audsrv_load_adpcm ... failed`.
- [ ] **PS2 conditions on the PC** ([ps2-preview](PLANS/ps2-preview.md) V1): the frame fills a 4:3 area of the window
  (black bars at the sides of a 1280x896 window), the game runs at 30 fps (`-LogFrameTimes`: 33.4 ms), a gamepad
  plays it as the DualShock (a small tilt of a stick does nothing: the dead zone), and the sound is the PS2's mix.
- [ ] **The DualShock 2** ([ps2-shipping](PLANS/ps2-shipping.md) N24; PCSX2 with a pad that rumbles, or a console): in ShooterGame the pad buzzes a moment for each shot (the small motor), shakes when the player is hurt (the large one) and when a grenade or the bomb goes off nearby; the motors stop when the pad is pulled out and do not start again when it comes back; after a reconnection the sticks are analog again. The pressures: the EE log's `PS2InputInterface: port 0 DualShock, motors, pressure` and `pressure mode` lines; a second pad in port 2 logs its own lines and moves nothing.
- [ ] **The memory card** (N24): `SetVolume 0.5` (`-ExecCmds=`) logs `Memory card: saved /BASLUS-99001SHOOTER/Settings`; the PS2's browser shows the folder as *ShooterGame Settings* with its icon (a dark tile with a green crosshair); the next boot logs `Settings loaded`. Without a card, with an unformatted one or a full one, the log says so and the game plays on.
- [ ] **PS2 boot in PCSX2** ([PS2SDK.md](../Engine/Platforms/PS2/Documentation/PS2SDK.md), findings 1 to 3): with Host
  Filesystem on, `Game\ShooterGame\Packages\PS2\ShooterGame.elf` logs `FPS2PlatformMisc: IOP reset, ...` then plays, the pad
  answers and the sound plays (the IOP reset kept `host:` and the pad); with Host Filesystem off, the red
  "THE GAME STOPPED" screen names `host:` and the setting. The same from uLaunchELF on a console, when one is at hand.
- [ ] **The Xbox pad's vibration** (N24b, Win64): with an Xbox pad plugged in, ShooterGame's shots buzz the right
  (light) motor and damage and explosions the left (heavy) one; the motors stop when the game exits.
- [ ] **PAL** (`BuildCookRun -stage -pak -iso -region=PAL`, [ps2-shipping](PLANS/ps2-shipping.md) N10, N23): the
  disc boots as `SLES_990.01`, the game draws at 25 fps and the world still steps at 30 Hz (1, 1, 1, 1, 2 steps a
  frame).

**Pending at 0.24.0.** No person has done these yet; the tests cover their logic (fakes of the pad, XInput and the
memory card), not the hardware or the ear: listening to the sounds on the SPU2, the DualShock's vibration and
pressure (and a reconnection), the memory card in the PS2's browser, the Xbox pad's vibration, and PAL, which has
never been run.

## GS parity (G8)

Gate G8 ([ps2-gs-parity](PLANS/ps2-gs-parity.md) P7): the desktop draws what the PS2 draws. `RunTests.bat` runs it
with the engine's tests (a window is needed: `-nodisplay` skips it):

- `System.Renderer.GSEmulator.Conformance`: the OpenGL GS emulator against the reference rasterizer on the 21 GS
  conformance scenes (`GSConformance::GetScenes`), within 2 levels per channel but for 8 pixels a scene. Since
  [ps2-shipping](PLANS/ps2-shipping.md) N8 they cover every feature `FGSCommandList::IsSupported` accepts: MipmapLod
  (the LOD from Q and K, the MIPMAP filters), AlphaTest (8 methods, 4 AFAIL), Fog, TexAAndFunctions (TEXA, TCC and
  the texture functions), ClampModes (the four wrap modes, on level 1 too), StripsAndSprites (XYZ3 in a strip and a
  fan, mirrored and STQ sprites, lines, points), BlendEquation (A, B, C, D, FIX and As above 0x80, COLCLAMP wrapping),
  PabeFbaDate (PABE, FBA, DATE, FBMSK bit by bit), Dither16Blend (a blend dithered against the 16-bit destination)
  and ClutLoads (CLD 0 to 5), each with its `System.GSReference.*` test of the manual's values; since N13
  ClutAndFormats also draws PSMT8 / PSMT4 MIPMAP levels through one CLUT, trilinear, loaded by CLD 2 to 4 as the
  scene renderer's texture cache loads it; since [ps2-polish](PLANS/ps2-polish.md) P5 TexturedCanvas draws the
  canvas's textured draws (a PSMT4 texture whose CLUT carries an alpha ramp, MODULATE and blended: UV sprites at
  half-pixel positions, nearest one to one, turned in V and scaled bilinear, and a rotated quad of UV triangles);
- `System.Renderer.GSEmulator.CanvasFrame` ([ps2-polish](PLANS/ps2-polish.md) P5): the canvas's text in the four
  engine fonts (Spanish, a shadow, an outline) and its textured, scaled and rotated tiles, emulated and referenced
  from the same list, within one 5-bit step but for 64 pixels; `System.Renderer.GS.Canvas.Text` draws it with the
  reference alone (the glyphs where their metrics put them, the frame's CRC as verified);
- `System.Renderer.GSEmulator.SceneFrame`: a frame of the GS scene renderer (its texture PSMT8 with mips since N13),
  within one 5-bit step but for 64 pixels (20 today);
- `System.Renderer.GS.*`: the scene renderer's lists drawn by the reference (the emitter, the texture cache with the
  PS2 cook's paletted textures and two PSMT8 textures each with its CLUT after its texels; since
  [ps2-shipping](PLANS/ps2-shipping.md) N13 its residency: `TextureCache.Oversubscribed` (12 textures in an arena
  for 4, the frame's never evicted), `.EvictionOrder` (least recently used first), `.UploadBudget` (a frame within
  the budget, the smallest level or a flat colour meanwhile), `.ClutLoads` (CLD 4 / 5 load 7 times in 10 binds; a
  texture in an evicted one's blocks forces its load and draws its own colours), `Scene.DrawsGroupedByTexture` (six
  cubes of two textures: two TEX0 writes and two CLUT loads), a lit cube, the canvas, and
  `Scene.StripsDrawTheSource`: a frame drawn from the meshes' LPS2 v2 strips, the same pixels as their source
  triangles sent one by one, in fewer GS writes);
- `System.MeshUtilities.LPS2.*` (the LPS2 v2 build, [ASSET_FORMATS.md](ASSET_FORMATS.md#lps2-v2)): the strips draw
  exactly the source's triangles with their winding, the batches fit VU1's budget, the quantization error, the same
  bytes every build; `ShooterGame.Content.MeshQuantization` checks the project's meshes within 0.5 cm;
- `System.GSCore.LocalMemory.*`: the GS local memory's layout against the manual's chapter 8 (every format's block
  table, pixel addresses, transfers, CSM1 CLUTs, the 4 MB wrap), which the emulator and the reference share.

The world's step ([ps2-shipping](PLANS/ps2-shipping.md) N18):

- `System.Engine.Tick.*`: the tick groups' order, the level's order with an actor's components before it (and back in
  place after a disable), what cannot tick costs nothing, the intervals at 30 and 25 Hz (every third step, alternating
  two and three), the prerequisites (a pawn after its controller) and a spawn during a tick;
- `System.Engine.Timers.*`: rates, looping, first delays, a callback that clears or sets timers, the order of due timers,
  a looping timer that fell behind, 20 simulated minutes without drift (the bomb at exactly step 1 200), the world's
  timers and an actor's that go with it;
- `System.Engine.FixedStep.*`: the accumulator's remainders in integer microseconds, 60 fps without drift, PAL's 1, 1,
  1, 1, 2 steps, the spiral-of-death guard, and the proxies drawn between the last two steps (a jump at once, the
  component untouched);
- `System.CoreUObject.GarbageCollection.Incremental*`: the incremental collection keeps exactly what a full one keeps on
  random graphs, stays right when references move between visited and unvisited objects, objects are made and
  destroyed between the slices (a full collection after it finds the same as the graph's reachability), and weak
  pointers resolve until its end (also in TestPAL, so on the EE).

The PS2 side is manual (PCSX2 needs a BIOS): the same scenes captured on the console's emulator.

1. `Package.bat` (or `Engine\Build\BatchFiles\Build.bat GSConformance PS2 Development`).
2. Boot `Engine\Packages\PS2\GSConformance\GSConformance.elf` in PCSX2 (`pcsx2-qt -fastboot -elf <file>`), with the
   software renderer (Settings > Graphics > Renderer: Software) for a faithful GS.
3. Take a screenshot (F8, `snaps\` in PCSX2's folder) and compare it with the reference's scenes: twenty-one cells in a
   4 x 6 grid, each with its name. A difference is a bug in the reference, the emulator or the PS2 backend; report it with
   the cell's name.
4. Keep the screenshot in `Engine/Platforms/PS2/Documentation/Captures/GSConformance.png` as the fixture the next
   captures are compared with.

## PS2 validation in PCSX2 (ps2-engine)

The checks of [ps2-engine](PLANS/ps2-engine.md) that need the console's emulator, in order. PCSX2: enable Settings >
Advanced > Enable Host Filesystem (the ELF's folder is `host:`), and keep the EE log open (Tools > Show Console, or
`emuLog.txt`). Each check says what to keep and record.

| Phase | Run | Passes when | Record |
| --- | --- | --- | --- |
| E0 | `Package.bat`, then `GSConformance.elf` and `TestPAL.elf` from `Engine\Packages\PS2\` (the ThirdPerson demo this row also booted went in [ps2-shipping](PLANS/ps2-shipping.md) N2) | GSConformance matches the reference (above); `TestPAL: PASSED (N test(s), 0 failed)` (123 then; 157 at [ps2-shipping](PLANS/ps2-shipping.md) N15) | a screenshot of each, TestPAL's `LogTestPAL` lines in [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md) |
| E1 | `BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -run "-addcmdline=-nullrhi -benchmark -botmatch -rounds=10 -seed=7"`, twice | `Botmatch OK: N round(s), ...` both times, the same result | the `Botmatch OK` and `Botmatch budget:` lines in Budgets.md |
| E2, E3 | `BuildCookRun.bat ... -platform=PS2 -build -cook -stage -pak -run "-addcmdline=-ExecCmds=bot_fill"` | de_leon draws with the player, the bots, the tracers and the HUD, from the pak (`Mounted ... ShooterGame-PS2.lpak`) | a screenshot beside the desktop's (`ShooterGame -Screenshot=`) |
| E4 | the packaged `Game\ShooterGame\Packages\PS2\ShooterGame.elf` (it passes `-LogFrameTimes`) | a full match against nine bots with the pad (move, look, fire, buy, plant); `Frame times over ...` lines near 33.4 ms average | the frame time lines in Budgets.md; if they miss 30 fps, which part (world or draw) is over |
| E5 | the same | shots, steps, the bomb's beeps and the explosion sound, panned, without drops; `PS2 audio: the SPU2's voices through audsrv` in the log (the EE's mix went with [ps2-shipping](PLANS/ps2-shipping.md) N19) | whether the frame times changed with the sound |

<a id="ps2-disc-boot"></a>**Disc boot** ([ps2-shipping](PLANS/ps2-shipping.md) N23). `Engine\Build\BatchFiles\MeasurePS2.bat -Iso
-Label <text>` stages the game with `BuildCookRun -iso` (the measuring command line on the disc, `LEONCOMM.TXT`) and
boots `Game\ShooterGame\Saved\StagedBuilds\PS2\ShooterGame.iso` in PCSX2 (`-nogui -fastboot -- <iso>`, the measuring
data folder); `-NoBuild -Iso` makes the disc again from the stage. It passes when the EE log shows the BIOS loading
`cdrom0:\SLUS_990.01;1`, `LogPakFile: Mounted 'cdrom0:/ShooterGame/Content/Paks/ShooterGame-PS2.lpak'`, `Botmatch: 2
round(s)` and `ProfileSummary:`; it prints `First frame after N s`, the load time the row in
[Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md#the-disc-n23) records. Check that no other `pcsx2-qt` is running
first. To lay the pak out in the load's order, boot a disc whose command line has `-LogFileOpenOrder` and pass the EE
log to `BuildCookRun -stage -pak -iso -pakorder=<log>`: `MeasurePS2 -Iso -LogFileOpenOrder` records it, and
`MeasurePS2 -Iso -PakOrder <that log>` measures the ordered disc ([ps2-shipping](PLANS/ps2-shipping.md) N24). It prints the
first frame's time and the worst after it (`first_ms`, `worst_later_ms`) and the frames longer than three fields
(`Long frame N`); since N24 no line `LogFileOpenOrder:` may follow `First frame after` (nothing reads the disc while
the game plays).

