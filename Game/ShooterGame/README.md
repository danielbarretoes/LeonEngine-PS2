# ShooterGame (Win64)

An offline Counter-Strike-style shooter, modelled on UE's ShooterGame sample: two teams (CT and T), five players a
side, on `de_leon`, a blockout map built in Blender. P17 boots it: the first-person character with CS movement, team
spawns, bots that join the teams (they stand still until P20), a crosshair. Weapons come in P18, the round rules in
P19, the bots' brains in P20.

## Build and run

From the repository root (Windows):

```bat
:: The game -> Game\ShooterGame\Binaries\Win64\ShooterGame.exe
Engine\Build\BatchFiles\Build.bat ShooterGame Win64 Development -Project=%CD%\Game\ShooterGame\ShooterGame.lproj

:: Play de_leon (GameDefaultMap); fill both teams with bots on the first frame
Game\ShooterGame\Binaries\Win64\ShooterGame.exe -ExecCmds=bot_fill

:: The smoke test (gate G6): headless, ten pawns, exit code 0
Engine\Build\BatchFiles\SmokeTest.bat

:: The project's tests (ShooterGameTests.exe; RunTests.bat runs them after the engine's)
Engine\Build\BatchFiles\Build.bat ShooterGameTests Win64 Development -Project=%CD%\Game\ShooterGame\ShooterGame.lproj
Game\ShooterGame\Binaries\Win64\ShooterGameTests.exe

:: A staged build: cook, stage, pak and run (Docs/TOOLS.md, "BuildCookRun")
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=Win64 -configuration=Shipping -build -cook -stage -pak -run
```

A map URL picks the team: `ShooterGame.exe /Game/Maps/de_leon?team=T` (else the smaller team, CT on a tie).

## Controls

| Input | Action |
| --- | --- |
| Mouse | Look (0.07° per pixel, `SetMouseSensitivity <degrees per pixel>` changes it) |
| W / S, D / A | Move forward / back, right / left |
| Space | Jump |
| Left Ctrl or C (held) | Crouch (it stays crouched under a ceiling until there is room) |
| Left Shift (held) | Walk (52 % of the speed) |
| Left mouse button | Fire (logs "no weapon until P18") |
| Tab / Escape | Scoreboard / menu (placeholders) |

Console commands (`-ExecCmds="cmd1;cmd2"`): `bot_add_ct [N]`, `bot_add_t [N]`, `bot_add [N]` (the smaller team),
`bot_fill` (both teams to five), `ViewFrom X Y Z Pitch Yaw` (a fixed view, for captures), `ViewPawn` (back to the
pawn), `exit`.

## Classes

| Class | UE ShooterGame / CS counterpart | What it does |
| --- | --- | --- |
| `AShooterGameMode` (`AGameMode`) | `AShooterGame_TeamDeathMatch` | `GlobalDefaultGameMode` of the project. Teams (`ChooseTeam`: `?team=`, else the smaller team), team spawns (`ChoosePlayerStart`: the first free start tagged with the team, level order), the bot commands (`AddBots`, `FillTeamsWithBots`), `MaxPlayersPerTeam` 5; logs where each player joined and the pawn count at the end (the smoke reads it) |
| `AShooterCharacter` (`ACharacter`) | `AShooterCharacter` | First-person camera at the eyes (`UCameraComponent`, `bUsePawnControlRotation`, 74° vertical FOV), capsule 40 × 91.5 cm, eyes 163 cm (76 crouched, eased with `FInterpTo`), the team body the other players see (`CTBodyMeshName` / `TBodyMeshName`) |
| `UShooterCharacterMovement` (`UCharacterMovementComponent`) | `UShooterCharacterMovement` | CS 1.6 movement in centimetres (below), and the walk key through `GetMaxSpeed` |
| `AShooterPlayerController` | `AShooterPlayerController` | The player's input and the `ViewFrom` / `ViewPawn` commands |
| `AShooterAIController` (`AAIController`) | `AShooterAIController` | The bots' controller; it has a player state (a team); no behaviour yet |
| `AShooterPlayerState` | `AShooterPlayerState` | The team (`EShooterTeam`: None, CT, T) |
| `AShooterHUD` (`AHUD`) | `AShooterHUD` | CS's crosshair (green, 4 px gap, 7 px arms, 2 px thick; config) and the scoreboard placeholder |

The CS movement values, at 1 unit = 2.54 cm (CS's player is 72 units tall and 183 cm here):

| Setting | CS 1.6 | ShooterGame |
| --- | --- | --- |
| Run speed (knife) | 250 u/s | `MaxWalkSpeed` 635 cm/s |
| Crouched speed | × 0.333 | `MaxWalkSpeedCrouched` 212 cm/s |
| Walk | × 0.52 | `WalkSpeedModifier` 0.52 (`DefaultGame.ini`) |
| Acceleration | `sv_accelerate` 5 → 1250 u/s² | `MaxAcceleration` 3175 cm/s² |
| Friction | `sv_friction` 4, `sv_stopspeed` 75 | `GroundFriction` 4, `BrakingFrictionFactor` 1, `BrakingDecelerationWalking` 762 cm/s² |
| Gravity | `sv_gravity` 800 u/s² | `Gravity` 2032 cm/s² |
| Jump | 268 u/s (45 units high) | `JumpZVelocity` 682 cm/s |
| Step | `sv_stepsize` 18 | `MaxStepHeight` 45 cm |
| Walkable floor | 0.7 | `WalkableFloorZ` 0.7 |
| Air control | `sv_airaccelerate` 10 | `AirControl` 0.3 |
| Crouched height | 36 units | `CrouchedHalfHeight` 46 cm |

## de_leon

A 60 × 48 m blockout (north up; the T spawn to the south, the CT spawn to the north; `a` / `b` the bomb sites, `.`
the spawn pads, `C` / `T` the team starts, `c` the crates, `S` the crate stack, `=` the low wall at B with its player
clip, `#` the walls, 3.5 m inside and 4 m at the edge; 1 character = 1 m across, 2 m down):

```text
       W (-Y)                   Y=0                  E (+Y)
  30 #################################################
  28 #                 .............                 #
  26 #                 ..C.C.C.C.C..                 #
  24 # bbbbbbbbbbb     .............     aaaaaaaacaa #
  22 # bbccbbbbbbb ### ...........c. ### aaaaaaaaaaa #
  20 # bbbbbbbbbbb ###            c  ### aaacaaaaaaa #
  18 # bbbbbbbcbbb ###               ### aaacaaaaaaa #
  16 # bbbbbbbcbbb ###               ### aaaaaaaaaaa #
  14 # =====       ###         S     ###             #
  12 #                                               #
  10 #                 #####   #####                 #   <- mid doors (3 m doorway)
   8 #####       #######           #######       #####
   ...  B long        (short B)      mid      (short A)        A long
  -2 #####       #######           #######       #####
 -14 #####       #######           #######       #####
 -16 #                                               #
 -20 #             c                   c             #
 -22 #               .................               #
 -26 #               ....T.T.T.T.T....               #
 -30 #################################################
```

The complete sketch, the waypoint graph and the Blender pipeline are in [Docs/LEVELS.md](../../Docs/LEVELS.md#worked-example-de_leon).
`SourceArt/Maps/make_de_leon.py` builds the map in Blender (`blender --background --factory-startup --python ...`),
saves `de_leon.blend` and exports `de_leon.glb`; `SourceArt/ImportList.ini` imports it to `/Game/Maps/de_leon` with
its meshes and materials, and `SourceArt/Characters/make_team_bodies.py` makes the teams' placeholder bodies. The
project's `DefaultEditor.ini` makes the `BombSite_*` and `BuyZone_*` nodes trigger volumes and refuses a map without
both sites, both buy zones and both teams' starts. Only CC0 art enters the project
([SourceArt/LICENSES.md](SourceArt/LICENSES.md)).

## Layout

```text
Game/ShooterGame/
├── ShooterGame.lproj                  module ShooterGame, TargetPlatforms Win64
├── Config/                            DefaultEngine.ini (map, game mode, Weapon channel), DefaultGame.ini (tuning,
│                                      cook), DefaultInput.ini (CS keys, mouse), DefaultEditor.ini (map import rules)
├── Content/                           Maps/de_leon.lmap (+ de_leon/Meshes, Materials), Characters/ (team bodies)
├── SourceArt/                         Blender scripts, .blend and .glb sources, ImportList.ini, LICENSES.md
└── Source/
    ├── ShooterGame.Target.cmake       the game (Win64)
    ├── ShooterGameTests.Target.cmake  the project's test program (LeonAutomationTests + ShooterGame's tests)
    └── ShooterGame/                   the game module (Public/, Private/, Private/Tests/)
```
