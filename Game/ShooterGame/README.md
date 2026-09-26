# ShooterGame (Win64)

An offline Counter-Strike-style shooter, modelled on UE's ShooterGame sample: two teams (CT and T), five players a
side, on `de_leon`, a blockout map built in Blender. P17 boots it: the first-person character with CS movement, team
spawns, bots that join the teams (they stand still until P20), a crosshair. P18 brings the weapons (a pistol, a rifle,
an AWP and an HE grenade), damage, armor, death and spectating. The round rules come in P19, the bots' brains in P20.

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
| Left mouse button | Fire (held: automatic weapons keep firing) |
| Right mouse button | The AWP's zoom (two levels, then off) |
| R | Reload |
| 1 / 2 / 4 | Primary (rifle, AWP) / pistol / grenade |
| G | Drop the weapon in hand (a pawn without one in that slot picks it up by walking over it) |
| Tab / Escape | Scoreboard / menu (placeholders) |

Console commands (`-ExecCmds="cmd1;cmd2"`): `bot_add_ct [N]`, `bot_add_t [N]`, `bot_add [N]` (the smaller team),
`bot_fill` (both teams to five), `ViewFrom X Y Z Pitch Yaw` (a fixed view, for captures), `ViewPawn` (back to the
pawn), `exit`.

## Classes

| Class | UE ShooterGame / CS counterpart | What it does |
| --- | --- | --- |
| `AShooterGameMode` (`AGameMode`) | `AShooterGame_TeamDeathMatch` | `GlobalDefaultGameMode` of the project. Who may hurt whom (`CanDealDamage`: no friendly fire, `bFriendlyFire`) and the kills (`Killed`, logged). Teams (`ChooseTeam`: `?team=`, else the smaller team), team spawns (`ChoosePlayerStart`: the first free start tagged with the team, level order), the bot commands (`AddBots`, `FillTeamsWithBots`), `MaxPlayersPerTeam` 5; logs where each player joined and the pawn count at the end (the smoke reads it) |
| `AShooterCharacter` (`ACharacter`) | `AShooterCharacter` | First-person camera at the eyes (`UCameraComponent`, `bUsePawnControlRotation`, 74° vertical FOV), capsule 40 × 91.5 cm, eyes 163 cm (76 crouched, eased with `FInterpTo`), the team body the other players see (`CTBodyMeshName` / `TBodyMeshName`, `bOwnerNoSee`); health, armor and helmet, the damage rules and death ([Weapons](#weapons)); the inventory (one weapon a slot, `DefaultWeapons`) |
| `UShooterCharacterMovement` (`UCharacterMovementComponent`) | `UShooterCharacterMovement` | CS 1.6 movement in centimetres (below), and the walk key through `GetMaxSpeed` |
| `AShooterPlayerController` | `AShooterPlayerController` | The player's input, the hit marker's state (`NotifyHitConfirmed`) and the `ViewFrom` / `ViewPawn` commands; spectates when its pawn dies (`NAME_Spectating`) |
| `AShooterAIController` (`AAIController`) | `AShooterAIController` | The bots' controller; it has a player state (a team); no behaviour yet |
| `AShooterPlayerState` | `AShooterPlayerState` | The team (`EShooterTeam`: None, CT, T) |
| `AShooterHUD` (`AHUD`) | `AShooterHUD` | CS's crosshair (green, 4 px gap, 7 px arms, 2 px thick; config), health and armor, the weapon and its ammunition, the hit marker, the AWP's scope, and the scoreboard placeholder |
| `AShooterWeapon` (`AActor`) and its classes | `AShooterWeapon`, `_Instant`, `_Projectile`; `AShooterProjectile` | The weapons ([Weapons](#weapons)) |

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

## Weapons

Each weapon is an actor the pawn carries (UE ShooterGame's `AShooterWeapon`): one per slot (primary, pistol, grenade).
A pawn spawns with `DefaultWeapons` (the pistol) and draws the best it has. The first-person mesh is a view model (drawn
after the scene with its own depth, only in its owner's view), the other players see the same mesh on the body; the
shot's tracer and the muzzle flash start at the mesh's `Muzzle` socket.

| Weapon | Class | Slot | Clip / reserve | Rate | Damage | Armor ratio | Speed | Price |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `usp` | `AShooterWeapon_Pistol` | pistol | 12 / 100, semi-automatic | 0.15 s | 34, range modifier 0.79 | 1.0 | 100 % | 500 |
| `ak47` | `AShooterWeapon_Rifle` | primary | 30 / 90, automatic | 0.1 s | 36, 0.98 | 1.55 | 88 % | 2500 |
| `awp` | `AShooterWeapon_Sniper` | primary | 10 / 30, semi-automatic | 1.45 s | 115, 0.99 | 1.95 | 84 %, 60 % scoped | 4750 |
| `hegrenade` | `AShooterWeapon_Grenade` | grenade | 1 | — | 98 within 889 cm | 1.0 | 100 % | 300 |

The constructors hold Counter-Strike's values and `DefaultGame.ini`'s `[/Script/ShooterGame.ShooterWeapon_<Class>]`
sections their meshes, sounds and seeds; any `UPROPERTY(Config)` of the classes can be tuned there.

- **Hitscan** (`AShooterWeapon_Instant`): a line on the `Weapon` channel from the eyes within the spread cone. The
  damage falls off as CS's range modifier per 1270 cm (500 units). The spread is `WeaponSpread`, plus `MovingSpread`
  in proportion to the speed, plus `JumpingSpread` in the air, plus the firing spread (grows each shot, recovers once
  the trigger is released), times `CrouchingSpreadMod` crouched. Each shot kicks the aim up (and sideways at random);
  the kick comes back down. The spread's direction and the recoil come from an `FRandomStream` seeded with
  `RandomSeed`: a weapon fires the same sequence every time (the tests replay it). A hit on a surface leaves a mark
  (the pool of 64), a hit on a player shows the shooter's hit marker.
- **AWP** (`AShooterWeapon_Sniper`): the right button steps through two zoom levels (30.5° and 7.5° high: CS's 40°
  and 10° wide at 4:3) and off; the HUD draws the scope and hides the view model. Unscoped it adds `UnscopedSpread`;
  scoped the carrier walks at 60 %. A shot leaves the scope for the bolt, which brings it back.
- **HE grenade** (`AShooterWeapon_Grenade`): the press throws `AShooterProjectile` along the aim tilted up 8° at
  1500 cm/s plus the thrower's velocity; it bounces and explodes after 1.5 s: 98 damage at the centre falling linearly
  to nothing at 889 cm, blocked by walls (`ApplyRadialDamageWithFalloff` on the Visibility channel), the thrower
  included. The grenade leaves the inventory with its throw.
- **Reloads**: `R`, or firing with an empty clip; `ReloadDuration` later the rounds move from the reserve. An empty
  weapon clicks.

Damage (`AShooterCharacter::TakeDamage`, after `AActor::TakeDamage`):

- `AShooterGameMode::CanDealDamage` refuses a teammate's damage (CS's `mp_friendlyfire 0`; oneself is hurt).
- A hit in the capsule's top `HeadHeight` (28 cm) is a headshot: × `HeadshotMultiplier` (4).
- Armor (CS): with armor, on the body or with a helmet, the health takes `ArmorRatio / 2` of the damage (at most all
  of it) and the armor half the rest; when the armor runs out the rest goes to the health. Damage that is not a
  weapon's or a grenade's ignores armor.
- At 0 health the pawn dies: the game mode hears of the kill (`Killed`), the rifle (else the pistol) falls to the
  floor ahead of it with its rounds and the rest is lost, the corpse stops colliding and lies down (until the round's
  restart, P19), a player spectates (a free-flying `ASpectatorPawn`) and a bot lets go of the pawn.
- A weapon on the floor is picked up after 1 s by the first live pawn within 60 cm that has its slot free.

The meshes are boxes made by `SourceArt/Weapons/make_weapons.py` (with their `SOCKET_Muzzle` nodes) and the sounds are
synthesized by `SourceArt/Sounds/make_sounds.py`; both need only Python's standard library, write the same bytes on
every run and are CC0 ([SourceArt/LICENSES.md](SourceArt/LICENSES.md)).

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
│                                      weapons, cook), DefaultInput.ini (CS keys, mouse), DefaultEditor.ini (map
│                                      import rules)
├── Content/                           Maps/de_leon.lmap (+ de_leon/Meshes, Materials), Characters/ (team bodies),
│                                      Weapons/ (meshes, materials), Sounds/
├── SourceArt/                         Blender and Python scripts, .blend, .glb and .wav sources, ImportList.ini,
│                                      LICENSES.md
└── Source/
    ├── ShooterGame.Target.cmake       the game (Win64)
    ├── ShooterGameTests.Target.cmake  the project's test program (LeonAutomationTests + ShooterGame's tests)
    └── ShooterGame/                   the game module (Public/, Private/, Private/Tests/)
```
