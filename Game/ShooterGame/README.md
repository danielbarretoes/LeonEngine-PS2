# ShooterGame (Win64 and PS2)

An offline Counter-Strike 1.6 clone, modelled on UE's ShooterGame sample: two teams (CT and T), five players a side,
on `de_leon`, a desert town built in Blender, on Win64 and the PS2 (30 fps in PCSX2, from its pak or a bootable ISO).
At 0.24.0 it has CS 1.6's movement (fall damage, ladders, the jump's stamina, tagging, footsteps by surface), its
arsenal (knife, Glock, USP, Desert Eagle, MP5, AK-47, M4A1, AWP; HE, flashbang and smoke grenades) with the economy,
ammunition, wall penetration and hit groups, the defusal rounds with the halftime side switch, the radar, the damage
indicator, the death cam and spectating, bots that buy by the team's plan (eco, force-buy), throw grenades, strafe and
use the radio, physical materials with per-surface sounds, and animated CT and T characters and first-person arms; all
the art and sounds are made by scripts (CC0). The player's settings are saved (the memory card on the PS2), and the
pad vibrates.

History: P17 to P21 (0.20.0) boot it with CS movement, the first weapons, the defusal rules, the bots' brains and
headless bot matches; 0.20.1 adds the waypoints' `Jump` / `Crouch` flags, escorting, hunting and rotating, and the UMG
buy menu. [ps2-shipping](../../Docs/PLANS/ps2-shipping.md) (0.22.0 to 0.24.0) brings the rest: N24 the settings' save
game and the vibration ([Settings and the pad](#settings-and-the-pad)), N27 the characters, arms and weapon models
([Characters and animation](#characters-and-animation)), N28 de_leon rebuilt with real art, N29 30 fps on the PS2
with it, and N30a to N30f CS 1.6 parity: the arsenal and economy ([Weapons](#weapons)), the flashbang and smoke
([Grenades](#grenades)), the movement ([CS 1.6's movement](#cs-16s-movement)), the halftime, radar and spectating
([Death and spectating](#death-and-spectating)), the bots' economy, grenades and radio ([Bots](#bots),
[The radio](#the-radio)) and the physical materials ([Surfaces and their sounds](#surfaces-and-their-sounds)).

## Build and run

From the repository root (Windows):

```bat
:: The game -> Game\ShooterGame\Binaries\Win64\ShooterGame.exe
Engine\Build\BatchFiles\Build.bat ShooterGame Win64 Development -Project=%CD%\Game\ShooterGame\ShooterGame.lproj

:: Play de_leon (GameDefaultMap): bots fill both teams to five a side (bFillTeamsWithBots) and the match starts
Game\ShooterGame\Binaries\Win64\ShooterGame.exe

:: The smoke test (gate G6): headless, ten pawns, exit code 0
Engine\Build\BatchFiles\SmokeTest.bat

:: A bot match (P21): ten bots, 10 rounds, seed 7, headless and unpaced, the invariants checked, played twice
Engine\Build\BatchFiles\BotMatch.bat 10 7

:: The project's tests (ShooterGameTests.exe; RunTests.bat runs them after the engine's)
Engine\Build\BatchFiles\Build.bat ShooterGameTests Win64 Development -Project=%CD%\Game\ShooterGame\ShooterGame.lproj
Game\ShooterGame\Binaries\Win64\ShooterGameTests.exe

:: A staged build: cook, stage, pak and run (Docs/TOOLS.md, "BuildCookRun")
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=Win64 -configuration=Shipping -build -cook -stage -pak -run

:: On the PS2 in PCSX2: built in Docker, cooked for the PS2 (paletted textures, SPU2 ADPCM), its pak beside the ELF,
:: played with the pad at 30 fps; -LogFrameTimes puts the frame times in the EE log every 5 s
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -pak -run "-addcmdline=-LogFrameTimes"

:: A bootable disc (Saved\StagedBuilds\PS2\ShooterGame.iso), and the frame measured in PCSX2 booting it
Engine\Build\BatchFiles\BuildCookRun.bat -project=Game\ShooterGame\ShooterGame.lproj -platform=PS2 -build -cook -stage -pak -iso
Engine\Build\BatchFiles\MeasurePS2.bat -Iso
```

On the PS2 (PCSX2, NTSC) de_leon runs at 29.95 fps, p50 / p95 / p99 33.5 ms (ps2-shipping N31; 29.96 fps from the
disc), and the first frame from the disc comes 2.93 s after the engine starts
([PS2 README](../../Engine/Platforms/PS2/README.md#measuring-in-pcsx2)).

A map URL picks the team and the seed of the rounds (the bomb's carrier): `ShooterGame.exe /Game/Maps/de_leon?team=T?seed=42`
(else the smaller team, CT on a tie, and `RandomSeed`).

## Controls

| Keyboard and mouse | DualShock (PS2), or a gamepad on the PC (Cross = A, Circle = B, Square = X, Triangle = Y) | Action |
| --- | --- | --- |
| Mouse | Right stick | Look (the mouse 0.07° per pixel, `SetMouseSensitivity <degrees per pixel>` changes it; the stick up to 150° a second across, 100° up and down: `BaseTurnRate` / `BaseLookUpRate`) |
| W / S, D / A | Left stick | Move forward / back, right / left (the stick walks slower when tilted less) |
| Space | Cross | Jump (on a ladder: off it) |
| Left Ctrl | Circle | Crouch: a press crouches and the next stands up (`SetToggleCrouch 0` holds it instead; it stays crouched under a ceiling until there is room) |
| Left Shift (held) | L3 (held) | Walk (52 % of the speed) |
| Left mouse button | R2 | Fire (held: automatic weapons keep firing) |
| Right mouse button | L2 | The secondary attack: the AWP's zoom (two levels, then off), the USP's and the M4A1's silencer, the Glock's burst mode, the knife's stab |
| R | Square | Reload |
| 1 / 2 / 3 / 4 / 5 | R1 / L1 / D-pad up / D-pad left / D-pad down | Primary (the MP5, the rifles, the AWP) / pistol / knife / grenade (again: the next grenade, HE, flashbang, smoke) / the bomb (CS's C4 in slot 5, for its carrier: the weapon is put away and the HUD shows `C4`; a weapon's key puts it back) |
| G | D-pad right | Drop the weapon in hand, not the knife nor a grenade, or the bomb when it is drawn (a pawn without one in that slot, or a terrorist for the bomb, picks it up by walking over it; the HUD says `Picked up <item>`) |
| , / . | — | A box of the primary's / the pistol's ammunition (CS: buyammo1 / buyammo2), when the player may buy |
| E (held) | Triangle (held) | Plant the bomb (its carrier, standing still in a bomb site, 3 s) or defuse it (a CT at the planted bomb, 10 s, 5 with a kit) |
| B | Start | The buy menu (the console's `buymenu` toggles it too), CS's: 1 Pistols, 2 SMGs, 3 Rifles, 4 Primary ammo, 5 Secondary ammo, 6 Equipment; a category's page lists what the team may buy (Rifles: the AK-47 for the T, the M4A1 for the CT, the AWP), and a purchase goes back to the first page. It opens only when its player may buy (alive, in the team's buy zone, within the buy time; never while spectating) and closes by itself when that stops; the HUD says why for 2 s (`BuyRefusalDuration`). While it is open: the number keys choose a line; the D-pad's up and down move the highlight (`>`) and Cross chooses it; Escape or Circle go back to the first page, and there close it (B and Start close it). It takes these keys only while it is open |
| Z / X / C | — | The radio's menus (CS 1.6's radio1, radio2, radio3; [The radio](#the-radio)): the number keys (1 to 9) send a message to the team and close the menu, Esc or the same key closes it; a menu takes these keys only while it is open, and it and the buy menu close each other |
| Tab (held) | Select (held) | The scoreboard |
| F4 (`stat unit`) | R3 (`stat unit`) | The engine's stats (FPS, MS, RAM, VRAM, TRIS, OBJ; on from the start on the PS2: `bShowStatsByDefault`) |
| Left mouse button, while spectating | R2 | The death cam ends; the next living teammate (CS 1.6's spectator keys) |
| Right mouse button, while spectating | L2 | The teammate before |
| Space, while spectating | Cross | The free look, or back to the teammates |

Console commands (`-ExecCmds="cmd1;cmd2"`): `bot_add_ct [N]`, `bot_add_t [N]`, `bot_add [N]` (the smaller team),
`bot_fill` (both teams to five), `bot_kick [name|all]`, `bot_stop [0|1]` (the bots stand still), `mp_restartgame
[seconds]`, `mp_maxrounds [N]` (the match's rounds; the halftime at half), `mp_halftime [0|1]`, `Buy <item>` (glock,
usp, deagle, mp5, ak47, m4a1, awp, flashbang, hegrenade, smokegrenade, primammo, secammo, vest, vesthelm, defuser),
`BuyAmmo1` / `BuyAmmo2`,
`buymenu` (opens or closes the buy menu, as CS), `radio1` / `radio2` / `radio3` (the radio menus, as Z, X and C),
`ThrowGrenade` (debug: draws the grenade slot and throws, for
captures), `ViewNextPlayer` /
`ViewPrevPlayer` (CS's spec_next / spec_prev), the cheats `give <weapon>` (its reserve full), `god` and `kill`, `ViewFrom X Y Z Pitch
Yaw` (a fixed view, for captures), `ViewPawn` (back to the pawn), `exit`.

## Settings and the pad

The player's options ([ps2-shipping](../../Docs/PLANS/ps2-shipping.md) N24) are a save game, `UShooterPersistentUser`
(UE ShooterGame's), in the slot `Settings`: `Saved/SaveGames/Settings.sav` on the PC, the memory card in slot 1 on the
PS2 (the folder `BASLUS-99001SHOOTER` with the title `ShooterGame Settings` and an icon in the console's browser,
`[MemoryCard]` of `DefaultGame.ini`). The player's controller loads them when it plays at a screen (not headless nor
in the tests) and each command applies its option and saves them all: `SetSensitivity <scale>` (the mouse's 0.07° a
pixel and the stick's rates times the scale, 1 by default), `SetInvertY 0|1` (up looks down, the mouse and the stick),
`SetVolume <0..1>` (the audio device's master volume), `SetCrosshairColor <r> <g> <b>` (0..1 each), `SetToggleCrouch 0|1`
(1, the default: a press of the crouch key crouches and the next stands up; 0: held, as CS 1.6). A card that is
missing, unformatted, full or pulled out logs why the options were not saved (`ESaveGameResult`), and the game plays
on with them.

The DualShock vibrates, and on Windows an Xbox pad (XInput), with UE ShooterGame's force feedback
(`APlayerController::ClientPlayForceFeedback`): the small motor
a moment for each shot, the large one when the player is hurt, and both when a grenade or the bomb explodes within
twice its radius of the player's view. The second pad is controller 1: the game has one player, so it moves no one.
The DualShock 2's pressures reach the input as the `Gamepad_*Axis` keys (`Gamepad_RightTriggerAxis` for R2, ...);
ShooterGame binds none yet.

## Classes

| Class | UE ShooterGame / CS counterpart | What it does |
| --- | --- | --- |
| `AShooterGameMode` (`AGameMode`) | `AShooterGame_TeamDeathMatch` | `GlobalDefaultGameMode` of the project: the match and its rounds, the money, buying and the bomb's events ([Rounds](#rounds-money-and-the-bomb)). Who may hurt whom (`CanDealDamage`: no friendly fire, `bFriendlyFire`) and the kills (`Killed`: the feed, the money, the stats). Teams (`ChooseTeam`: `?team=`, else the smaller team), team spawns (`ChoosePlayerStart`: the first free start tagged with the team, level order), the bot commands (`AddBots`, `FillTeamsWithBots`), `MaxPlayersPerTeam` 5; logs where each player joined and the pawn count at the end (the smoke reads it). The registries of what the game looks up every frame ([Registries](#registries)) |
| `AShooterCharacter` (`ACharacter`) | `AShooterCharacter` | First-person camera at the eyes (`UCameraComponent`, `bUsePawnControlRotation`, 74° vertical FOV), capsule 40 × 91.5 cm, eyes 163 cm (76 crouched, eased with `FInterpTo`), the team's animated body the other players see (`CTBodyMeshName` / `TBodyMeshName`, `bOwnerNoSee`) and first-person arms (`CTArmsMeshName` / `TArmsMeshName`) ([Characters and animation](#characters-and-animation)); health, armor and helmet, the hit groups, the damage rules and death ([Weapons](#weapons)); the inventory (one weapon a slot: `DefaultWeapons`, the knife, and the team's pistol, `DefaultWeaponsCT` / `DefaultWeaponsT`) |
| `UShooterCharacterMovement` (`UCharacterMovementComponent`) | `UShooterCharacterMovement` | CS 1.6 movement in centimetres (below), the walk key and the tagging through `GetMaxSpeed`, the jump's stamina, fall damage and ladders (`EMovementMode::Custom`) ([CS 1.6's movement](#cs-16s-movement)) |
| `AShooterPlayerController` | `AShooterPlayerController` | The player's input, the hit marker's state (`NotifyHitConfirmed`), where the last damage came from (`NotifyTakeDamage`) and the `ViewFrom` / `ViewPawn` commands; when its pawn dies the death cam, then spectating ([Death and spectating](#death-and-spectating)) |
| `AShooterAIController` (`AAIController`) | `AShooterAIController` | The bots' brain: a behavior tree over a typed blackboard, `UPawnSensingComponent` senses, the waypoint navigation ([Bots](#bots)) |
| `AShooterGameState` (`AGameState`) | `AShooterGameState` | The round's phase and number, the phase's end, the score, the halftime (`IsSecondHalf`, `GetHalftimeRound`), the bomb's state, the kill feed and the radio's last messages (`GetRadioLog`) |
| `AShooterPlayerState` | `AShooterPlayerState` | The team (`EShooterTeam`: None, CT, T), the money, the kills and the deaths |
| `AShooterBomb` (`AActor`) | CS's C4 | Carried, dropped, planted (beeping), defused or exploded |
| `AShooterHUD` (`AHUD`) | `AShooterHUD` | CS's crosshair (green, 4 px gap growing with the spread, 7 px arms, 2 px thick; config), health, armor and money, the weapon and its ammunition, the bomb and the kit, the round's clock (`C4` instead once the bomb is planted: no countdown, as in CS) and the score, the kill feed, the team's radio messages and the radio menu ([The radio](#the-radio)), the round's messages (the terrorists read `The bomb has been dropped` while it lies on the floor; the second half's first freeze says the teams switched sides), the plant and defuse bar, the hit marker, the AWP's scope, the scoreboard, the radar, the damage direction indicator and who a spectator watches ([The HUD's radar and damage indicator](#the-huds-radar-and-damage-indicator)); `UShooterBuyMenuWidget` (a `UUserWidget`) is the buy menu: a widget tree (a `UCanvasPanel` holding a `UBorder` around a `UVerticalBox` of `UTextBlock`s: the money, why buying is refused, the items with their prices, the last buy) built in `NativeOnInitialized` and refreshed in `NativeTick`, collapsed while the menu is closed. A line of text is formatted (and a widget's text set) only when what it shows changes (`FShooterHUDText`: the money, the health, the clock's second, the score, the kill feed, the scoreboard's players) |
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
| Safe fall | `PLAYER_MAX_SAFE_FALL_SPEED` 580 u/s (a 5.3 m drop) | `SafeFallSpeed` 1473 cm/s |
| Fall damage's scale | `PLAYER_FATAL_FALL_SPEED` 1024 u/s, times `FlPlayerFallDamage`'s 1.25: lethal at 935 u/s (a 13.9 m drop) | `FatalFallSpeed` 2601 cm/s, `FallDamageScale` 1.25 |
| Ladder | `MAX_CLIMB_SPEED` 200 u/s, a jump off it 270 u/s | `LadderClimbSpeed` 508 cm/s, `LadderJumpOffSpeed` 686 cm/s |
| Jump stamina | `fuser2` 1315.79 ms, 19 % a second | `JumpStaminaTime` 1.3158 s, `JumpStaminaSlowdown` 0.19 |
| Tagging | `m_flVelocityModifier` 0.5 | `TaggingVelocityModifier` 0.5, back in `TaggingRecoveryTime` 1 s |

## CS 1.6's movement

ps2-shipping N30c, on top of UE's velocity model (`UShooterCharacterMovement`, `AShooterCharacter`):

- **Fall damage**: a landing (`ACharacter::Landed`, which sees the speed the fall hit the floor at) faster than
  `SafeFallSpeed` takes `(speed - SafeFallSpeed) x 100 / (FatalFallSpeed - SafeFallSpeed) x FallDamageScale` health
  (CS 1.6's multiplayer `FlPlayerFallDamage`: `(v - 580) x (100 / 444) x 1.25` in units a second, N30e): nothing below
  a 5.3 m drop, all 100 from about 935 u/s (2375 cm/s, a 13.9 m drop). It is the world's damage: no
  instigator or causer, so armor does not take it, it does not tag, and a death reads `<victim> (world)` in the kill
  feed (the victim loses no money and nobody scores). A jump lands at 682 cm/s: always safe.
- **Ladders**: a trigger volume tagged `Ladder` (a map's `Ladder*` node, `DefaultEditor.ini`) is a ladder, a thin box
  against its wall as CS's `func_ladder`; its face looks along the box's thinner horizontal axis. While the capsule
  touches one, the character is in the ladder mode (`EMovementMode::Custom`): no gravity, and CS's `PM_LadderMove`. The
  forward key (at `LadderClimbSpeed`) goes along the view with its pitch and the side keys along its right; the part
  going into the ladder's face turns into climbing. Looking at the ladder, forward climbs at 508 cm/s (looking up 45
  degrees, as in CS, 1.4 times faster), looking straight down it climbs down, and backing off it on the floor steps
  away; without a key the character hangs there. Jump pushes it off the ladder at 686 cm/s and it falls; it grabs a
  ladder again only once it touches none. Climbing on past the top carries it up onto the ledge. On a ladder the
  weapons have their air spread (CS: off the floor). The bots climb too ([Bots](#bots)): they face the ladder
  (`UShooterCharacterMovement::GetLadderNormal`) and look up or down.
- **The jump's stamina** (CS's `fuser2`): a jump costs `JumpStaminaTime` (1.3158 s), which runs down with the time. On
  the floor, each 10 ms (a CS command) of a step scales the horizontal velocity by `1 - stamina x 0.19` (CS: `(100 -
  fuser2 x 0.001 x 19) / 100`; a step of `dt` by that to the power `dt / 10 ms`, the same at any step): 0.75 at the
  jump, 0.87 at a running jump's landing 0.67 s later. A running jump's landing drops to 60 % of the speed within a
  few steps, and it is back 0.5 s after it; so bunny hopping gains nothing.
- **Tagging** (CS's `m_flVelocityModifier`): a shot that hurts without killing halves the victim's horizontal velocity
  and its top speed, which come back linearly over `TaggingRecoveryTime` (1 s). The world's damage (a fall) does not
  tag.
- **Footsteps**: the skinned body's locomotion notifies (`Footstep_L`, `Footstep_R`) are the steps; one on the floor
  faster than CS's 150 units a second (381 cm/s) plays the floor's step and makes a noise the bots hear
  (`AActor::MakeNoise` at `FootstepNoiseLoudness` 0.5: 12.5 m through walls, 25 m in sight). Slower steps are silent,
  so walking (the walk key, 330 cm/s) and crouching (212 cm/s) make none, as in CS. The floor is the surface of what a
  line 50 cm down from the feet meets (`GetFloorSurface`, its [physical material](#surfaces-and-their-sounds)), and
  `FootstepSounds` has each surface's left and right step. On a ladder a climb as fast plays CS's `pl_ladder` every
  `LadderStepInterval` (0.35 s), `LadderStepSoundNames` in turn, with the same noise.

Tests (`ShooterMovementTests.cpp`, at the fixed 30 Hz step): `ShooterGame.Movement.FallDamage` (the thresholds and
the 1.25, lethal at 935 u/s; drops of 3, 9 and 16 m: unhurt, hurt by the damage of the landing speed with the armor
untouched and no tagging, and dead short of 1024 u/s, the world's kill in the feed), `.Ladder` (a ladder on a 4 m block: grabbed walking into it, 508 cm/s up, 718 looking up
45 degrees, no gravity without a key, 508 down looking down, the jump off at 686 cm/s and back on the floor, then up
over the top onto the roof; a bot walks through the ladder along the wall), `.JumpStamina` (the ratio at a full and an
empty stamina; a running jump keeps its speed in the air and its landing follows the formula step by step, then full
speed once the stamina ran out), `.Tagging` (a shot: half the velocity and the top speed, 0.75 half a second later,
recovered after a second; the world's damage does not tag) and `.Footsteps` (running, each notify is heard by an enemy
bot; walking, crouched and standing, the notifies come and nothing is heard). The surfaces' steps are in
`ShooterSurfaceTests.cpp` ([Surfaces and their sounds](#surfaces-and-their-sounds)).

## Weapons

Each weapon is an actor the pawn carries (UE ShooterGame's `AShooterWeapon`): one per slot (primary, pistol, knife,
grenade). A pawn spawns with `DefaultWeapons` (the knife) and, once its team is known, the team's pistol
(`DefaultWeaponsCT`: the USP 12/24, `DefaultWeaponsT`: the Glock 20/40, `DefaultWeaponClips` clips in the reserve;
a pawn without a team the CT's), and draws the best it has: the first of primary, pistol, grenade and knife with
ammunition (a spent rifle gives way to the pistol; the knife never runs out). The
first-person model (`FirstPersonMeshName`, `SM_<Weapon>_1P`) is a view model in the arms' hand (drawn after the scene
with its own depth, only in its owner's view), the other players see the world model (`MeshName`, `SM_<Weapon>`) in
the body's hand; the shot's tracer and the muzzle flash start at the model's `Muzzle` socket. The
flash is a light of the world's pool of eight (`UWorld::AcquirePooledPointLight`, through
`UGameplayStatics::SpawnPointLightAtLocation`): a shot spawns no actor once the pool is warm.

Counter-Strike 1.6's values (1 unit = 2.54 cm; ps2-shipping N30a). Damage: the hit's at point blank and the range
modifier per 500 units; penetration: the things a bullet may hit, its power (units) and distance (units); a box: the
rounds and the price of a box of its ammunition; every kill pays $300 (CS 1.6).

| Weapon | Class | Slot | Team | Clip / reserve | Cycle | Damage | Armor ratio | Speed (u/s) | Price | Penetration | A box | Reload |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `knife` | `AShooterWeapon_Knife` | knife | every player's | — | 0.4 s (a stab 1.1 s) | 15 a slash (48 u), 65 a stab (32 u), x3 in the back | 1.7 | 250 | — | — | — | — |
| `glock` | `AShooterWeapon_Glock` | pistol | T's first | 20 / 120, semi-automatic or a 3-round burst | 0.2 s (a burst 0.5 s) | 25, 0.75 | 1.05 | 250 | 400 | 1, 21, 800 | 30 for $20 (9 mm) | 2.2 s |
| `usp` | `AShooterWeapon_USP` | pistol | CT's first | 12 / 100, semi-automatic, a silencer | 0.225 s | 34, 0.79 (silenced 30) | 1.0 | 250 | 500 | 1, 15, 500 | 12 for $25 (.45 ACP) | 2.7 s |
| `deagle` | `AShooterWeapon_Deagle` | pistol | both | 7 / 35, semi-automatic | 0.3 s | 54, 0.81 | 1.5 | 250 | 650 | 2, 30, 1000 | 7 for $40 (.50 AE) | 2.2 s |
| `mp5` | `AShooterWeapon_MP5` | primary | both | 30 / 120, automatic | 0.075 s | 26, 0.84 | 1.0 | 250 | 1500 | 1, 21, 800 | 30 for $20 (9 mm) | 2.63 s |
| `ak47` | `AShooterWeapon_AK47` | primary | T | 30 / 90, automatic | 0.0955 s | 36, 0.98 | 1.55 | 221 | 2500 | 2, 39, 5000 | 30 for $80 (7.62 mm) | 2.5 s |
| `m4a1` | `AShooterWeapon_M4A1` | primary | CT | 30 / 90, automatic, a silencer | 0.0875 s | 32, 0.97 (silenced 33, 0.95) | 1.4 | 230 | 3100 | 2, 35, 4000 | 30 for $60 (5.56 mm) | 3.05 s |
| `awp` | `AShooterWeapon_AWP` | primary | both | 10 / 30, semi-automatic, a scope | 1.45 s | 115, 0.99 | 1.95 | 210 (150 scoped) | 4750 | 3, 45, 8000 | 10 for $125 (.338) | 3.7 s |
| `hegrenade` | `AShooterWeapon_HEGrenade` | grenade | both | 1 | — | 98 within 889 cm | 1.0 | 250 | 300 | — | — | — |
| `flashbang` | `AShooterWeapon_Flashbang` | grenade | both | 2 | — | a flash within 1500 u ([Grenades](#grenades)) | — | 250 | 200 | — | — | — |
| `smokegrenade` | `AShooterWeapon_SmokeGrenade` | grenade | both | 1 | — | a cloud for 18 s ([Grenades](#grenades)) | — | 250 | 300 | — | — | — |

The models are N27's (each weapon's world and first-person model; the pistols share the pistol's arm clips and body
montages, the MP5 and the rifles the rifle's, the grenades the grenade's, the knife has its own draw and slash).

The constructors hold Counter-Strike's values and `DefaultGame.ini`'s `[/Script/ShooterGame.ShooterWeapon_<Class>]`
sections their models, animations (the arms' idle, the stance's aim offset, the fire, reload and draw montages),
sounds and seeds; any `UPROPERTY(Config)` of the classes can be tuned there.

- **Hitscan** (`AShooterWeapon_Instant`): a line on the `Weapon` channel from the eyes within the spread cone. The
  damage falls off as CS's range modifier per 1270 cm (500 units), segment by segment through what it pierces
  (below). The spread follows CS 1.6's cases in each weapon's `PrimaryAttack` ([ps2-polish](../../Docs/PLANS/ps2-polish.md)
  P2): `WeaponSpread` standing still; plus the movement's term, which grows with the speed to `WalkingSpread` at
  `WalkingSpeed` (CS's 140 units a second, 356 cm/s: the walk key always stays below it) and on to `MovingSpread` at
  the weapon's running speed; plus `JumpingSpread` in the air; plus the firing spread (grows each shot, recovers once
  the trigger is released); all of it times `CrouchingSpreadMod` crouched. A walk costs the pistols and the AWP
  accuracy but not the AK-47, the M4A1 and the MP5 (`WalkingSpread` 0: CS's cases for them only look past 140 units
  a second; P2b). Each weapon sets its values in its constructor (`DefaultGame.ini` can tune them). In degrees:

  | Weapon | crouched still | still | walking | running | jumping | `CrouchingSpreadMod` |
  |---|---:|---:|---:|---:|---:|---:|
  | Glock | 0.29 | 0.45 | 1.38 | 3.95 | 10.95 | 0.65 |
  | USP | 0.20 | 0.30 | 1.23 | 3.30 | 9.30 | 0.65 |
  | Desert Eagle | 0.33 | 0.50 | 1.61 | 4.50 | 12.50 | 0.65 |
  | MP5 | 0.27 | 0.45 | 0.45 | 1.95 | 6.95 | 0.6 |
  | AK-47 | 0.18 | 0.35 | 0.35 | 4.85 | 12.85 | 0.5 |
  | M4A1 | 0.17 | 0.30 | 0.30 | 3.80 | 10.80 | 0.55 |
  | AWP (unscoped: + 6.0 after the rest, as CS's + 0.08) | 0.03 | 0.05 | 0.99 | 3.05 | 9.05 | 0.5 |

  The crosshair's gap follows the spread (`AShooterHUD::GetCrosshairGap`), so it closes crouched and opens walking,
  running and in the air, as CS's dynamic crosshair; crouched, its own 4 px gap closes by `CrouchingSpreadMod` too
  (CS's `ACCURACY_DUCK`). The gap on 448 lines, the Glock's: 4.1 px crouched, 6.3 still, 11.2 walking, 24.5 running,
  61.5 in the air; the AK-47's: 2.9 crouched, 5.8 still and walking, 29.2 running, 71.8 in the air.

  Each shot kicks the aim up (and sideways at random); the kick comes back down. The AK-47, the M4A1 and the MP5 kick
  by the owner's state, as CS's `KickBack` branches (`MovingRecoilScale`, `JumpingRecoilScale`,
  `CrouchingRecoilScale`: CS's arguments over standing's; `GetRecoilScale`): moving at all, up 1.5 (AK-47), 1.54
  (M4A1), 1.33 (MP5) times standing's; in the air 2.0, 1.85, 2.4; crouched 0.9, 0.92, 0.93, and less sideways. The AK-47
  and the M4A1 test the movement before the air, the MP5 after it, as in CS. The pistols and the AWP keep one kick.
  Tests: `ShooterGame.Weapons.SpreadByState` (the table, and the order crouched < still < walking < running < jumping,
  a walk as still for the rifles and the MP5), `ShooterGame.HUD.DynamicCrosshair` (the gap in the same order),
  `ShooterGame.Weapons.KickBackByState` (each state's share, and a crouched shot's kick against a standing one's). The spread's direction and the recoil come from an `FRandomStream` seeded with
  `RandomSeed`: a weapon fires the same sequence every time (the tests replay it). A hit on a surface leaves a mark
  (the pool of 64; its tint and size the surface's) and plays the surface's impact sound, a hit on a player CS's
  `bhit_` sound and the shooter's hit marker ([Surfaces and their sounds](#surfaces-and-their-sounds)).
- **Silencers and the burst**: the right button puts the USP's silencer on or off in 3 s and the M4A1's in 2 s (no shot
  meanwhile; CS): silenced, the USP hits for 30, the M4A1 for 33 with a 0.95 range modifier and a quarter more spread,
  and a shot is 0.2 as loud to the bots and quieter to hear. The Glock's right button toggles its burst mode: a press
  fires three rounds 0.1 s apart (the trigger may be released), the next press 0.5 s after the first, at five times
  the spread. The HUD adds `(silenced)` or `(burst)` to the weapon's name.
- **The knife** (`AShooterWeapon_Knife`, 3): the trigger slashes (15 within 48 units, every 0.4 s while held), the
  right button stabs (65 within 32 units, 1.1 s before the next attack); a stab in the back (CS: the attacker's
  direction and the victim's facing agree by more than 0.8) does three times as much. A cut is a line from the eyes,
  then a 10 cm sphere when the line misses, so the hit groups count as for a bullet. It is never dropped nor bought.
- **Penetration** (CS's `FireBullets3`): a bullet hits up to `PenetrationCount` things. Through a body it goes on 42
  units past the entry with three quarters of the damage and of the range left. Through a surface it must leave it
  within its penetration power, which the surface cuts for the rest of the shot (concrete to a quarter, metal to
  15 %, tile to 65 %, a computer to 40 %; wood, dirt, glass and the default keep it), keeping the surface's share of
  the damage (wood 0.6, metal and tile 0.2, a computer 0.45, the rest 0.5) and half the range left; beyond
  `PenetrationDistance` from the shooter nothing is pierced. The exit is found with a line back from the power's depth
  to the entry (a box or a closed mesh still around that point stops the bullet). The surface is the hit's physical
  material's (N30f; [Surfaces and their sounds](#surfaces-and-their-sounds)): de_leon's crates, doors and ladders
  wood, its walls, trim and signs concrete, its sand dirt, its paving tile, its lamps metal. The AK-47 goes through
  99 cm of wood, 25 cm of concrete, 64 cm of tile or 15 cm of metal; the AWP through 114 cm of wood (a 110 cm crate)
  and a second wall.
- **AWP** (`AShooterWeapon_AWP`): the right button steps through two zoom levels (30.5° and 7.5° high: CS's 40°
  and 10° wide at 4:3) and off; the HUD draws the scope and hides the view model. Unscoped it adds `UnscopedSpread`;
  scoped the carrier walks at 60 %. A shot leaves the scope for the bolt, which brings it back; a reload leaves it for
  good (CS).
- **HE grenade** (`AShooterWeapon_HEGrenade`): the press throws `AShooterProjectile` along the aim tilted up 8° at
  1500 cm/s plus the thrower's velocity; it bounces and explodes after 1.5 s: 98 damage at the centre falling linearly
  to nothing at 889 cm, blocked by walls (`ApplyRadialDamageWithFalloff` on the Visibility channel), the thrower
  included. The grenade leaves the inventory with its throw.
- **Reloads**: `R`, or firing with an empty clip; `ReloadDuration` later (CS's time, the table's) the rounds move from
  the reserve, the reload montage played at the rate that fits it (the weapons of a kind share their clip). A draw
  lasts its montage (1 s; the AWP's 1.27 s, a whole number of frames, against CS's 1.25 s). An empty weapon clicks.
- **Ammunition** (CS 1.6): a bought weapon comes with a full clip and an empty reserve; `primammo` / `secammo` (the buy
  menu's ammo lines, the `,` and `.` keys) buy a box of the primary's / the pistol's calibre while the reserve has room.

Damage (`AShooterCharacter::TakeDamage`, after `AActor::TakeDamage`):

- `AShooterGameMode::CanDealDamage` refuses a teammate's damage (CS's `mp_friendlyfire 0`; oneself is hurt).
- **Hit groups** (CS's, `AShooterCharacter::GetHitGroup`, on height bands of the capsule, standing or crouched): the
  top `HeadHeight` (28 cm) is the head (x the weapon's `HeadshotMultiplier`, 4); below it the body's bottom
  `LegsFraction` (59 %: to 91.5 cm standing) the legs (x0.75), up to `StomachFraction` (75 %: 116 cm) the stomach
  (x1.25), above it the chest (x1), and a chest hit farther than `ArmFraction` (60 %) of the radius to the pawn's side
  an arm (x1); left and right are the pawn's. A blast is the generic group (x1).
- Armor (CS): with armor, on every group but the legs and on the head only with a helmet, the health takes `ArmorRatio
  / 2` of the damage (at most all of it) and the armor half the rest; when the armor runs out the rest goes to the
  health. The grenade and the bomb have ratio 1 (CS 1.6 armors blasts too); other damage (the world's) ignores armor.
- A player hears where the damage came from (the shooter; the grenade or the bomb for a blast): the HUD's damage
  direction indicator.
- At 0 health the pawn dies: the game mode hears of the kill (`Killed`), the rifle (else the pistol) falls to the
  floor ahead of it with its rounds and the rest is lost, the corpse stops colliding and falls (a death montage: on
  its back when shot from the front, on its front from behind) and lies there (until the next round), a player gets
  the death cam and then spectates ([Death and spectating](#death-and-spectating)) and a bot
  lets go of the pawn.
- A weapon on the floor is picked up after 1 s by the first live pawn within 60 cm that has its slot free
  (`AShooterCharacter::PickUpWeapon`), with its rounds and its silencer or burst mode; a weapon not drawn at once plays
  its draw sound, and a player reads `Picked up <weapon>` on the HUD for 2 s (`PickupNoticeDuration`). G drops the
  weapon in hand ahead of the feet ([ps2-polish](../../Docs/PLANS/ps2-polish.md) P4).

Tests (N30a, `ShooterArsenalTests.cpp`): `ShooterGame.Arsenal.StatsTable` (every weapon against the table, no other
weapon class, the first pistols' reserves), `.SilencerAndBurst`, `.KnifeBackstab`, `ShooterGame.Weapons.Penetration`
(wood, concrete, metal, glass, tile and plain walls, each its material's physical material, against CS's power and
damage shares, two walls, the penetration distance, a body), `ShooterGame.Damage.HitGroups` (each group's band, multiplier and armor, the helmet, crouched),
`ShooterGame.Buy.AmmoAndPrices` and `.TeamRestrictions` (the calibres' boxes, the `,` and `.` keys, the teams' rifles,
the buy menu's pages).

The models are made in Blender by `SourceArt/Weapons/make_weapons.py` (with their `SOCKET_Muzzle` nodes) and the
sounds are synthesized by `SourceArt/Sounds/make_sounds.py` (Python's standard library); both write the same bytes
on every run and are CC0 ([SourceArt/LICENSES.md](SourceArt/LICENSES.md)).

## Grenades

ps2-shipping N30b, CS 1.6's three grenades in the grenade slot (4): one weapon of each, the HE one ($300), the
flashbang two ($200 each), the smoke grenade one ($300); a purchase of one carried adds a grenade (the buy menu's
Equipment: kevlar, kevlar and helmet, flashbang, HE, smoke, kit). The key cycles them (HE, flashbang, smoke), a bought
grenade waits in its slot, and each is thrown as the HE (1500 cm/s, 8° up, 1.5 s fuse, `AShooterProjectile` and its
classes' `Detonate`).

- **Flashbang** (`AShooterProjectile_Flashbang`, CS's `RadiusFlash`): every living player whose eyes the explosion
  sees (a line on the Visibility channel: walls hide, pawns do not) within `FlashRadius` (1500 units, 38.1 m) is
  flashed, the thrower and its teammates too. The strength falls from `FlashStrength` (4) at the centre to nothing at
  the radius; by the angle between the view and the flash the screen holds white and then fades
  (`AShooterCharacter::Flash`):

  | The view | Dot of the view and the flash | White held | Fade | White |
  | --- | --- | --- | --- | --- |
  | Looking at it | 0.5 or more | 1.5 x the strength s | 3 x | full |
  | Aside | down to -0.5 | 0.45 x | 1.75 x | 200 of 255 |
  | Behind | less | 0.2 x | 1 x | 200 of 255 |

  The HUD draws it as one full-screen white tile (two triangles, alpha blended) under the rest of the HUD. A bot is
  blind for a third of the fade (`BotBlindFadeShare`, CS's `Blind`): its sensing looks at nobody, and it stands and
  fires at random around where it last saw an enemy (its `Blind` branch; N30e, [Bots](#bots)) until it sees again.
- **Smoke grenade** (`AShooterProjectile_Smoke`, `AShooterSmokeCloud`): it leaves a cloud on the floor below, a
  sphere of `Radius` 325 cm (CS: about 128 units) 150 cm up, for `Duration` 18 s (CS 1.6's), thickening in 1 s and
  thinning out in its last 3 s. While it is thick (until half of the thinning) a line of sight through its sphere
  fails for the bots (`UShooterPawnSensingComponent::HasLineOfSightTo`, `AShooterGameMode::IsSightBlockedBySmoke`).
  It is drawn as six soft grey puffs 4.4 m across facing the camera: the world's effect sprites
  (`UWorld::EffectSprites`, `UGameplayStatics::SpawnEffectSprite`: two alpha-blended triangles each with the effects'
  round mask, farthest first, 12 triangles a cloud). The round's clean-up removes the clouds and their puffs.

Tests (`ShooterGrenadeTests.cpp`): `ShooterGame.Grenades.FlashIntensity` (the hold, the fade and the white looking
at the flash, aside, behind and farther, out of range and behind a wall; the white holds and fades out),
`.FlashBlindsBots` (a flashed bot traces to nobody and fires blind around where it saw its enemy for a third of the
fade, the same shots with the same seed, then sees and engages),
`.SmokeBlocksSight` (a cloud between a bot and its enemy hides the enemy until it thins out, its six puffs, then gone
with them) and `.CarryLimits` (two flashbangs, one HE, one smoke and the refusals; the key's cycle; a flashbang thrown
leaves the other; the HUD's white).

## Characters and animation

Low-poly and textured in the style of Counter-Strike 1.6 ([ps2-shipping](../../Docs/PLANS/ps2-shipping.md) N27;
[ART_PIPELINE.md](../../Docs/ART_PIPELINE.md#the-characters-arms-and-weapons)), made by the Blender scripts of
`SourceArt/Characters` on the shared 23-bone `SKEL_Body` and the 11-bone `SKEL_Arms`:

- **The counter-terrorist** (`SK_Body_CT`, 932 triangles): blue-grey camouflage, a vest, a helmet, black gloves;
  **the terrorist** (`SK_Body_T`, 828): a brown jacket, olive trousers, a balaclava, a backpack. Their first-person
  arms (`SK_Arms_CT` 542, `SK_Arms_T` 588) wear the same sleeves and gloves.
- **The body** (`UCharacterAnimInstance`): the 2D locomotion by speed and direction (`BS_Locomotion`: idle, walk and
  run forward, back, left and right, with footsteps), crouched (`BS_Crouch`, silent), the jump's take-off, fall and
  landing, the drawn weapon's stance and aim by the view's pitch (`AO_Rifle`, `AO_Pistol`, `AO_Grenade`), the
  upper body's fire, reload and throw, the whole body's plant, defuse and death. The weapon sits on `Weapon_R`.
- **The arms**: the drawn weapon's idle, its draw, fire and reload (the magazine's `MagOut` / `MagIn`), the grenade's
  throw, the C4's plant; the view model in their hand.
- The body evaluates its pose only when drawn and less often far from the view, the arms only when drawn (a bot's
  never are); the botmatch plays the same with or without them.
- The body, the arms and the camera are Movable, as the capsule is (UE: a character's mesh moves with it). The scene
  then assigns the body to the map's cells it walks into, and the portals draw it from any cell that sees it. A Static
  body kept the cells it spawned in, so from some angles it vanished and left its weapon floating
  ([ps2-polish](../../Docs/PLANS/ps2-polish.md) P1). A Static skeletal mesh in a map with cells is an `ensure`.

## Rounds, money and the bomb

Counter-Strike's defusal rules (`AShooterGameMode`, all in `DefaultGame.ini`'s `[/Script/ShooterGame.ShooterGameMode]`):

- **The match**: a warmup until both teams have a player (bots fill the teams as soon as the player is in), then up to
  `MaxRounds` (30, `mp_maxrounds`) rounds; a team wins at 16 (more than half), else the match ends after the last
  round (a tie is a draw). `mp_restartgame` starts over (from the warmup only with both teams in).
- **Halftime** (`bHalftime`, `mp_halftime`; CS's competitive halves, CS:GO's `mp_halftime`): after round `MaxRounds /
  2` (15) the teams switch sides. Every player, the bots too, joins the other team, and the scores go with the teams
  (CT 9 - T 6 becomes CT 6 - T 9); the money goes back to $800 and the loss streaks to none; every pawn goes, so the
  second half starts as the first, on the new side's starts with the pistol. The HUD says `Halftime: the teams have
  switched sides` in that freeze.
- **A round**: the freeze (6 s: nobody moves or shoots, a trigger held from before included, the buy menu opens), the round (1:55), the result (5 s), then
  the next. Its start cleans the map (dropped weapons, grenades, corpses, the bomb), gives the survivors their health
  back (they keep their weapons and armor), respawns the dead with the pistol, and gives the bomb to a random
  terrorist. A player who joins during a round waits for the next one; a dead player spectates (below) and plays
  again at the next round. A new round clears every weapon's recoil and spread.
- **Who wins** (the round's end, checked each tick so deaths of the same moment count together):

  | What happened | Winner | Message |
  | --- | --- | --- |
  | Every terrorist dead, no bomb planted | CT | Counter-Terrorists Win! |
  | Every counter-terrorist dead (planted or not) | T | Terrorists Win! |
  | Both teams dead at once, no bomb planted | nobody | Round Draw! |
  | The time runs out, no bomb planted | CT | Target has been saved! |
  | The planted bomb explodes (the clock stops at the plant) | T | Target Successfully Bombed! |
  | The planted bomb is defused | CT | The bomb has been defused! |

- **Money** (CS 1.6): $800 to start, $16000 at most. A kill pays the weapon's `KillReward` ($300 with every weapon,
  as in CS 1.6; per weapon in the config); a team kill costs $3300. The winners get $3250 ($3500 by the bomb or a defuse); the losers $1400, $500 more for each
  consecutive loss up to $3400, and terrorists who lose with the bomb planted $800 more. The planter and the defuser
  get $300.
- **Buying**: in the team's buy zone, during the freeze and 45 s after it (CS's `mp_buytime` counts from the freeze's
  end; any time in the warmup): the weapons at their `Price` (the AK-47 only for the T, the M4A1 only for the CT), a
  box of ammunition for the primary or the pistol (the table's; a bought weapon has only its clip), kevlar ($650),
  kevlar and helmet ($1000; the helmet alone $350 with full kevlar), the defuse kit ($200, CT only). A second weapon
  in a slot drops the first; the same weapon twice is refused, and a refusal says why (not the team's, the
  ammunition full, the money).
- **The bomb** (`AShooterBomb`, `[/Script/ShooterGame.ShooterBomb]`): the carrier plants it by holding E for 3 s,
  standing still in a bomb site (the map's `BombSite` volumes); it beeps faster and faster and explodes after 40 s,
  500 damage falling to nothing at 44.5 m (CS's 1750 units), through walls, armor taking its share as with the
  grenade. A counter-terrorist within 1.2 m defuses it by holding E for
  10 s (5 with a kit); walking off or dying stops the defuse. A dead carrier drops the bomb, and the first live
  terrorist to walk over it takes it (`Picked up C4` on its player's HUD). The carrier draws it as CS's slot 5 (5, the
  D-pad's down) and drops it with G ahead of the feet; the one who dropped it can take it back after 1 s
  (`AShooterBomb::PickupDelay`). It is planted with E, drawn or not.

Tests: `ShooterGame.Rounds.HalftimeSwitchesSides` (four rounds: after round 2 every player is on the other team, the
scores went with them, the money is back to $800, the new terrorists stand on the T starts with the pistol only and one
carries the bomb, the match checker saw it; a halftime that moved nobody is flagged) and
`ShooterGame.Rounds.MatchEndsAtTheMajority` (3 of 4 rounds end the match after round 3, won by the side the team plays
then; 1 - 1 of 2 is a draw).

## Death and spectating

CS 1.6's, in `AShooterPlayerController` (`[/Script/ShooterGame.ShooterPlayerController]`):

- **The death cam** (`StartDeathCam`, from the pawn's death): the player spectates from its corpse's eyes (the
  `ASpectatorPawn`, held there: it does not fly) looking at its killer for `DeathCamDuration` (2 s); the HUD says
  `Killed by <name>` (`You died` for a suicide or the world). Fire ends it at once.
- **Spectating**: then the player watches its living teammates through their eyes (the camera's view target), in the
  game state's order: Fire (R2) the next, the right button (L2) the one before (`ViewNextPlayer`, `ViewPrevPlayer`),
  a dead one skipped; when the watched one dies, the next. The HUD says `Spectating <name>  + <health>`.
- **Free look**: with nobody left to watch, or Jump (Cross), the spectator flies free from where the view was; Jump
  again goes back to the teammates.
- `-BotMatchSpectate` (MeasurePS2's runs): a player without a team watches anybody alive, and the next one when they
  die, as before.

Tests: `ShooterGame.Spectate.DeathCamThenTeammates` (killed by a terrorist off to the side, the view turns to it from
the corpse and stays there with W held until 0.1 s before `DeathCamDuration`, then a living CT through its eyes, then
the free look when every CT is dead; the HUD's line each time), `ShooterGame.Spectate.CyclingSkipsTheDead` (Fire, the
right button and Jump while spectating, a dead teammate skipped both ways) and `ShooterGame.Rounds.SpectateTeammates`.

## The HUD's radar and damage indicator

- **The radar** (`[/Script/ShooterGame.ShooterHUD]` `RadarSize` 88 px, `RadarRange` 25 m to the edge), top left: the
  view at the centre with its cone, ahead up (it turns with the view's yaw); the living teammates as dots in the team's
  colour; for the terrorists the bomb's carrier as a bigger red dot, or the bomb on the floor or planted; the bomb
  sites' letters. What lies beyond the range sits on the edge in its direction (`AShooterHUD::ProjectToRadar`). A
  spectator without a team (a bot match's) sees the watched player's team. It is filled rectangles, lines and letters
  of the canvas: about 10 in a 5v5 (20 at most), in the frame's memory, nothing allocated; the scoreboard hides it.
- **The damage direction indicator**: when the player is hurt, an arc of four thick lines 64 px around the centre
  toward the damage's source (ahead is up, the right to the right: `GetDamageIndicatorAngle`), 50 degrees wide, that
  narrows and darkens and is gone after `DamageIndicatorDuration` (1 s).

Tests: `ShooterGame.HUD.Radar` (the projection ahead, behind, turned, beyond the range; a 5v5 frame's 10 primitives; a
second frame allocates nothing) and `ShooterGame.HUD.DamageIndicator` (the angle ahead, right, left, behind, turned; a
shot from the right draws the arc toward 90 degrees, gone a second later).

## Bots

`AShooterAIController` is Counter-Strike's bot on Leon's AIModule: a behavior tree (`UBTComposite_Selector` of
decorated `UBTTask_Action`s) over a typed blackboard, refreshed each tick from its senses and the game. The branches,
most urgent first:

| Branch | When | What |
| --- | --- | --- |
| Idle | frozen, dead or `bot_stop` | stands; in the freeze it buys once (below) and sees nobody, so its reaction starts with the round |
| Blind | flashed (`AShooterCharacter::IsBlind`) | stands and fires at random (CS's bots): every `BlindFireMinTime` to `BlindFireMaxTime` (0.25 to 0.6 s) a new point within `BlindFireError` (25 degrees across, a quarter of it up and down) of where it last saw an enemy (else of its view), fired at with `BlindFireChance` (0.6), all from its stream |
| ThrowGrenade | a throw under way (below) | draws the grenade, turns to the throw and throws it (an enemy that shows up meanwhile waits); after a flashbang it turns its back to it until it goes off, unless an enemy is in sight |
| Engage | an enemy in sight (seen in the last three sensing updates, within `SightRadius`; one lost for `EnemyMemory` s is searched for where it was last seen, as a noise) | draws its best weapon with ammunition, never a grenade (`EquipBestWeapon(false)`: the throws are ThrowGrenade's), turns at `AimTurnRate`, fires once `ReactionTime` has passed since it came into sight; the aim error starts at `AimError` and settles toward `MinAimError`; automatic weapons fire bursts, the AWP zooms first; the recoil climbs on its aim as on a player's, `RecoilCompensation` of each kick pulled back down. It moves as CS's bots do (below): it strafes, crouches with a rifle at range, stands with the AWP, and rushes with the knife |
| Defuse | a CT and the bomb planted | walks to the bomb and holds use |
| Plant | the bomb's carrier | walks to the round's site (`AShooterGameMode::GetTerroristTargetSite`, drawn each round from the seeded stream) and plants inside it |
| FetchBomb | a T and the bomb dropped | walks over it |
| PickUp | a weapon on the floor worth the walk (below) | walks over it: its spent weapon is dropped for it |
| Escort | a T without the bomb while a live teammate carries it, still `SupportDistance` (15 m) or farther from the site | stays within `EscortDistance` (350 cm) of the carrier, watching one side of its way (`EscortWatchAngle`, 50 degrees: the even bots its right, the odd its left) |
| Investigate | an enemy's shot heard (`AActor::MakeNoise`), or a teammate's report on the radio | walks to where it came from; nobody there: "Sector clear." |
| Hunt | its team's living players outnumber the enemy's by `HuntAdvantage` (2; 0 never), or a T with `HuntTimeLeft` (30 s) of the round left (0 never), and no bomb is planted | walks to the enemy's first spawn (`AShooterGameMode::GetTeamSpawnLocation`), then to waypoints of the graph drawn from its stream, until contact |
| Objective | otherwise | the site's lookouts (below): T the round's site (its support spots once the carrier is near, the planted bomb's site to guard it); CT A for the even, B for the odd of the team, a CT that has held its site `RotateTime` (25 s) with no contact rotating to the next site |

- **The economy** (ps2-shipping N30e, CS's): when a round starts each team decides its plan once
  (`AShooterGameMode::GetTeamBuyPlan`, `ChooseBuyPlan`): the first round of each half is the **pistol** round; a team
  with half of its players carrying a primary or able to afford the full buy (the team's rifle and kevlar with a
  helmet: $3500 for the terrorists, $4100 for the counter-terrorists) buys in **full**; short of it, it **forces**
  (spends what it has) after `ForceBuyLossStreak` (2) losses in a row, after a win and in the last round of a half, and
  saves otherwise (**eco**). The log says each round's plans (`Round N buys: CT Force, T Eco`).
- **Buying** (`BuyForRound`, once a round, by the team's plan): on an eco only a bot that can afford the full buy
  spends; else, without a primary, the AWP (with `AwpChance`, when the money covers it and kevlar with a helmet) or the
  team's rifle (the AK-47 or the M4A1), else an MP5, else a Desert Eagle, each only with the money for kevlar too; then
  the primary's ammunition box by box until full; then kevlar with a helmet (the helmet alone on the kevlar kept), or
  kevlar alone; a CT buys the kit; and with what is left the grenades of `GrenadeBuyOrder` (a flashbang, the HE, the
  smoke, the second flashbang: CS's limits, `AShooterGameMode::GetPrice` refuses one more). $800 buys kevlar.
- **Grenades** (N30e): a few times a second, with nobody in sight, a bot may start a throw (`ConsiderGrenade`): a
  terrorist nearing the round's site within `EntryGrenadeDistance` (18 m) throws its flashbang (else its smoke) at it,
  a counter-terrorist nearing the planted bomb its flashbang (else its HE), once a round when the round's draw
  (`GrenadeChance`, 0.4) says so; a spot where an enemy was seen, heard or reported between `MinGrenadeDistance` (9 m,
  the HE's radius) and `MaxGrenadeDistance` (22 m) gets the HE (else a flashbang, else the smoke) when that spot's draw
  says so. The aim point is off by up to `GrenadeThrowError` (1 m) and the pitch is the throw's low arc
  (`ComputeThrowPitch`, less the grenade's 8 degrees); a throw that would hit a wall within 3 m of the bot waits.
  `GrenadeCooldown` (4 s) separates two throws. Every throw calls "Fire in the hole!".
- **Fighting on the move** (N30e): while it engages, a bot strafes left and right across the line to its enemy at the
  walk key's speed (`bStrafeWalking`), each way for `StrafeMinTime` to `StrafeMaxTime` (0.4 to 1 s, from its stream);
  with a rifle (the AK-47, the M4A1) and the enemy at `CrouchFireDistance` (15 m) or farther it crouches and stops;
  with the AWP it stands still.
- **The knife** (ps2-polish P3, CS's bots' rush; `EngageWithKnife`): with the knife drawn (nothing else has
  ammunition) a bot runs the path to its enemy (a new one as the enemy moves 1.5 m), and within the slash's reach
  (`AShooterWeapon_Knife::SlashRange`, 122 cm to the enemy's capsule, less 10) it goes straight in behind an enemy
  whose back is turned and stabs within the stab's reach (81 cm; `IsBackstab`: three times as hard), else circles it
  by the side, closing in, and slashes. It never cuts out of reach.
- **Weapons on the floor** (ps2-polish P3; `UpdatePickupTarget`, twice a second): a bot without a loaded primary goes
  for the nearest primary with ammunition on the floor (`AShooterGameMode::GetPickups`) within `PickupSearchDistance`
  (15 m); out of ammunition altogether (the knife left), for any weapon with ammunition within twice that. Walking
  over it swaps its spent weapon for it: `AShooterWeapon::CanBePickedUpBy` lets a bot take a weapon into a slot whose
  weapon has no ammunition left (`AShooterCharacter::PickUpWeapon`, `AddWeapon` dropping the spent one); a player's
  slot must be free, as CS's walk-over. A walk longer than 12 s gives that weapon up for the round.
- **No enemy in sight** (ps2-polish P3): the bots watch the sites from their **lookouts**
  (`AShooterGameMode::GetBombSiteLookouts`): the map's waypoints flagged `Lookout` (de_leon's three a site, off the
  lanes' line: behind the crates, in the corners by the houses, by the site walls), each given to its nearest site; a
  map without them uses the site's three nearest waypoints within `LookoutFallbackRadius` (15 m), else its middle.
  Each lookout has a team's directions (`FShooterLookout::GetWatchYaws`): first the main way in (the first link of the
  graph's path toward the other team's spawn: CS's approach areas), then over the site from a spot away from it and
  its other links toward that spawn, at most four, 35 degrees apart. At a lookout a bot turns between them at
  `LookTurnRate` (240 degrees a second), the main way in for twice a drawn `WatchMinTime` to `WatchMaxTime` (1 to 2.5
  s) between each of the others, and after `LookoutMinTime` to `LookoutMaxTime` (3 to 7 s) it moves to another of the
  site's lookouts, drawn from its stream; within 10 m of the next one it already watches its main way in. The team's
  bots start at different lookouts (their place in the team). A CT holding a site that hears a teammate's "Enemy
  spotted." within `SiteReportRadius` (15 m) of the other site rotates there with `RotateOnReportChance` (1). On the
  move with nobody to aim at, a bot looks along its path (`LookTurnRate`).
- **Ladders** (ps2-polish P3): the waypoint graph links a ladder's foot and top (both flagged `Ladder`; the map
  import's `AutoLinkWaypoints` links two within `MaxLadderLinkDistance`, 2 m across, however high the climb), and the
  path follower does not jump at a ladder's top. On a ladder a bot faces its face (`GetLadderNormal`) and climbs to
  the path's point at the top looking level, climbs down to one below looking down (85 degrees), and at the foot jumps
  off it (a jump on a ladder pushes off its face); standing on one with nothing to climb it lets go, unless it
  fights.
- **Senses**: `UShooterPawnSensingComponent`, UE's `UPawnSensingComponent` narrowed to what a bot acts on: sight in a
  cone (140 degrees) within `SightRadius` (35 m; ps2-polish P3: at 60 m de_leon's lanes gave the terrorists' plaza the
  duels into both sites, the terrorists winning three rounds in four) with a line of sight on the Visibility channel, hearing of the noises `AActor::MakeNoise` reports within a
  loudness-scaled range (a weapon's shot: `FireNoiseLoudness`; a noise reaches the registered sensing components, not
  every actor). Its `ShouldCheckVisibilityOf` lets through only the living shooters of the other team, while the bot's
  own pawn is alive and not frozen, and `ShouldCheckAudibilityOf` only the other team's noises: the pawns and noises
  the bot ignored after tracing to them before, so a bot never traces to a teammate, a corpse or the spectator.
- **Looks** (ps2-shipping N20): a bot looks every 0.1 s (`AShooterAIController::SensingInterval`), as before, but the
  bots take turns. `AddBots` gives each bot a slot of ten, the teams interleaved (CT 0, T 1, CT 2, ...), and the bot
  in slot N looks N hundredths of a second after slot 0 (`SetSensingSlot`, UE's `UPawnSensingComponent::SetTimer`), so
  at 60 fps one or two bots look in a frame where all ten looked in the same frame. The game mode also lets at most
  `MaxSensingUpdatesPerFrame` (2) look in a frame (`ClaimSensingUpdate`, from the component's `OnTimer`): a slower
  frame (the PS2's 30 fps) queues the other looks in turn for the next frames, so each bot looks as often as the
  others, a little less often than 10 Hz. At 60 fps nothing waits: the reaction (`ReactionTime` from the look that saw
  the enemy, the enemy in sight while seen in the last three looks) is the same as before; only the moment of each
  bot's looks within the 0.1 s moved.
- **Navigation**: `AAIController::MoveToLocation` on `UNavigationSystem`'s waypoint graph (A* over de_leon's waypoints,
  linked at import); a bot jumps when the next path point rises more than 50 cm within 1.5 m or is a waypoint flagged
  `Jump` (not at a ladder's top, which it climbs), crouches along the links on both sides of a waypoint flagged
  `Crouch` and stands up past them, and repaths when it moves less than 30 cm in 1.5 s (climbing counts).
- **Skill** (`[/Script/ShooterGame.ShooterAIController]`): `Difficulty` scales the reaction and the aim error down and
  the turn rate and the recoil control (`RecoilCompensation`, 0.5: half of each kick pulled down; 0 lets it climb, 1
  holds the spray flat) up; `EscortDistance`, `SupportDistance`, `HuntAdvantage`, `HuntTimeLeft`, `RotateTime`,
  `RotateOnReportChance`, the lookouts' and the watch's times and `PickupSearchDistance` tune the branches above. Every random
  choice comes from the bot's stream, seeded from the game mode's `RandomSeed` and the bot's index, the order the game
  mode created it in (`SetBotIndex`): a match with `?seed=N` replays.
- **Names** (N30e): CS 1.6's BotProfile names (`[/Script/ShooterGame.ShooterGameMode]` `+BotNames=`: Albert, Allen,
  Bert, Bob, Cecil, ...), given in the order the bots are created (`GetBotName`; past the list's end "Albert (2)").
  They say nothing of a team, since a bot keeps its name when the halftime moves it to the other side, and they seed
  nothing: renaming a bot never changes a match.

Tests: `ShooterGame.Bots.Buy` (kevlar for $800, the rifle with its ammunition, a force-buy's Desert Eagle),
`EcoAndForceBuy` (the plan's rules, and a 2v2 through three rounds: the pistol round, the winners forcing and the
losers saving but a rich one, the losers forcing after two losses and the equipped winners buying in full),
`BuysGrenadesWithinLimits` (two flashbangs, one HE and one smoke with the money left, none past the limits the next
round), `ThrowsGrenades` (the low arc's pitch; a terrorist flashing the site on its way in, "Fire in the hole!", its
back to the flash; a teammate's report thrown at with the HE, the same point with the same seed),
`StrafeCrouchAndStand` (the strafe both ways within its times, the same pattern with the same seed; crouched and still
with a rifle at 16 m; upright and still with the AWP), `EngageKillsAnEnemy` (no shot before the reaction time),
`RecoilKicksTheAim`,
`CarrierPlants`, `CTDefuses`,
`TerroristsEscortTheCarrier`, `OutnumberingTeamHunts`, `CTRotatesBetweenSites`, `AgentFromConfig`, ps2-polish P3's
`KnifeRushesAndKills` (a bot with the knife, and a flashbang it never draws, runs at a terrorist 8 m away with its back
turned, cuts nothing out of reach and kills it with a stab in the back), `PicksUpAWeaponOutOfAmmo` (a spent pistol
swapped for a loaded Glock on the floor 8 m away, dropped for it; not for a pawn that is not a bot's),
`VisitsLookouts` (three lookouts around a site, each team's first direction toward the other's spawn; a CT walks
between them and turns between their directions; the same walk with the same seed) and `ClimbsALadder` (a site on the
roof of a 4 m block: up the ladder facing it, onto the roof, down it looking down to the lookout at its foot, off
it),
`MatchCheckerFlagsViolations`, `MatchOnDeLeon` (ten bots, three rounds of a four-round match, across its halftime,
seed 5, under `FShooterMatchChecker`; kills happen), `SensingFilter` (no trace to a teammate, a corpse or the spectator; one to a living enemy) and
`SensingStagger` (ten bots at 60 and 30 fps: at most two looks a frame, each bot as often as the others, 10 Hz at 60
fps).

## The radio

CS 1.6's radio (ps2-shipping N30e; `AShooterGameMode::SendRadioMessage`, `EShooterRadioMessage`): a player's message
goes to its team only, at most one every `RadioCooldown` (1.5 s) and `MaxRadioMessagesPerRound` (60) a round (a
grenade's "Fire in the hole!" and a bot's "Bomb has been planted." go out regardless). The game state keeps the last
six (`AShooterGameState::GetRadioLog`), and the HUD shows the viewer's team's (a spectator without a team: the watched
player's) for `RadioMessageDuration` (6 s), bottom left above the money, CS's chat area: `<sender> (RADIO): <message>`,
the sender in the team's colour.

- **The menus** (Z, X, C: `radio1`, `radio2`, `radio3`; `AShooterPlayerController::ToggleRadioMenu`), where the buy
  menu sits: radio1 "Cover me!", "You take the point.", "Hold this position.", "Regroup team.", "Follow me.", "Taking
  fire, need assistance!"; radio2 "Go go go!", "Team, fall back!", "Stick together, team.", "Get in position and wait
  for my go.", "Storm the front!", "Report in, team."; radio3 "Affirmative.", "Enemy spotted.", "Need backup.",
  "Sector clear.", "I'm in position.", "Reporting in.", "Get out of there, it's gonna blow!", "Negative.", "Enemy
  down.". The number keys send one and close the menu (the menus' input component takes them while one is open, as
  the buy menu's does); Esc or the same key closes it. A dead player has no radio. C was a second crouch key; it is
  CS's radio3 now (Left Ctrl crouches). The pad has no radio (its buttons are taken).
- **What the bots say**: "Enemy spotted." with the enemy's place when they see a new one; "Need backup." once a round
  below `NeedBackupHealth` (40); "Sector clear." when a place they went to look at is empty; "Bomb has been planted."
  when they plant; "Fire in the hole!" with every throw (anybody's). A bot does not say what a teammate said in the
  last `RadioRepeatTime` (3 s).
- **What they do with it**: a teammate's "Enemy spotted." within `RadioReportRange` (30 m) is a place to look at, as
  a heard shot, for a bot that fights nobody, has nothing else to look at and holds no site; a CT holding a site
  rotates to the other site when the report is within `SiteReportRadius` of it, with `RotateOnReportChance`
  (ps2-polish P3); the living bot nearest the
  sender answers a request (radio1's, radio2's and "Need backup.") with "Affirmative." ("Reporting in." to "Report
  in, team.") and goes to the sender for "Need backup." and "Taking fire".
- **Its sounds** (N30f; `AShooterPlayerController::HearRadio`): the team's local players hear a message's sound, 2D:
  a squelch, a tone pattern a menu (`RadioCommandSoundName`: two tones rising; `RadioGroupSoundName`: three quick
  ones; `RadioReportSoundName`: two falling) and a squelch, and a cue each for "Fire in the hole!" (a fast warble) and
  "Bomb has been planted." (low, high, low). CS speaks its messages; these are tones made by `make_sounds.py`, not
  speech. The other team hears nothing.

Tests (`ShooterRadioTests.cpp`): `ShooterGame.Radio.BotsReportEvents` (a CT bot's "Enemy spotted." at the
terrorist's place and a teammate going there; "Need backup." once the radio lets it, answered by the nearest teammate,
who goes to it; the terrorist bot's "Bomb has been planted."; a CT player's HUD showing the CT's lines in the CT's
colour, not the terrorists'), `.SectorClear` (a report of an empty place: the bot goes, finds nobody and says so; the
team's log) and `.PlayerMenu` (C and 2 send "Enemy spotted." instead of drawing the pistol; "Cover me!" refused within
the cooldown, then sent and answered; Esc closes radio2; the buy menu and a radio menu close each other) and
`.Sounds` (each menu's sound and the two cues; a teammate's message heard, the other team's not, the player's own).

## Surfaces and their sounds

What a surface is made of is its material's physical material (ps2-shipping N30f; UE's `UPhysicalMaterial`,
`/Game/PhysicalMaterials/PM_<Surface>`), named in the Blender scripts (`leon_art.make_material`'s `surface`, the glTF
material's extras) and set by the import. The surface types are CS's texture types the game needs, named in
`DefaultEngine.ini` (`[/Script/Engine.PhysicsSettings] +PhysicalSurfaces=`) and in code as UE ShooterGame does
(`ShooterGame.h`'s `SHOOTER_SURFACE_*`): Concrete, Dirt, Metal, Wood, Tile, Glass, Computer and Flesh; a material
without one is the default (CS's concrete sounds and full penetration power).

| Surface | de_leon and the game | Step (CS) | Impact | Penetration (power, damage) |
| --- | --- | --- | --- | --- |
| Concrete | the sandstone walls and houses, their trim, the site signs | `S_Step_Concrete_L` / `_R` (`pl_step`) | `S_Impact_Concrete`: a chip and grit | 0.25, 0.5 |
| Dirt | the sand of the T side, mid and the longs | `S_Step_Dirt_*` (`pl_dirt`) | `S_Impact_Dirt`: a puff | 1, 0.5 |
| Tile | the paving of the CT spawn and the sites (its slabs; the ground over them is the sand's) | `S_Step_Tile_*` (`pl_tile`) | `S_Impact_Tile`: a crack and a ceramic ring | 0.65, 0.2 |
| Metal | the tunnel's lamps, the weapons | `S_Step_Metal_*` (`pl_metal`) | `S_Impact_Metal`: a ricochet | 0.15, 0.2 |
| Wood | the crates, the mid doors' wings, the ladders, the tunnel's ceiling | `S_Step_Wood_*` (a hollow knock) | `S_Impact_Wood`: a thock and splinters | 1, 0.6 |
| Glass | none in de_leon | the default's | `S_Impact_Glass`: a shatter | 1, 0.5 |
| Computer | the C4 | the default's | `S_Impact_Metal` | 0.4, 0.45 |
| Flesh | the characters and their arms | the default's | `S_Hit_Flesh` (`bhit_flesh`) | 1, 0.5 (a body: 3/4, the hit groups) |

- **Steps** (`AShooterCharacter::FootstepSounds`, `LadderStepSoundNames`): the floor's (a line 50 cm down from the
  feet with `bReturnPhysicalMaterial`), the left foot's and the right foot's; the ladders' `S_Step_Ladder_1` / `_2`
  in turn. de_leon's players walk on the floor slabs' own boxes (N29), so the sand is dirt and the paving tile; a
  capsule walks across the seam between two slabs (the engine takes a box whose top is at the feet for floor).
- **Impacts** (`AShooterWeapon_Instant::ImpactSounds`, `ArmorHitSoundName`, `HelmetHitSoundName`): where a bullet
  enters a surface, its sound (a variant after another) and its mark (`GetImpactMarkStyle`: dark on concrete and tile,
  a larger brown dent in dirt, dark brown in wood, a small grey one on metal, a pale star on glass). On a character
  CS's `bhit_`: a helmet on a helmeted head (`S_Hit_Helmet`), armor where it covers (`S_Hit_Kevlar`, not the legs),
  else the flesh.
- **Voices**: the steps and the impacts are imported at `Priority=0.5`: a sound below the default priority leaves the
  last 10 of the SPU2's 22 effect voices free (`FAudioDevice::NumLowPriorityVoices`), so the shots and the important
  sounds always find one; the radio is at 1.5, the bomb at 2.

The sounds are synthesized by `SourceArt/Sounds/make_sounds.py` (22 050 Hz mono, 0.16 to 0.6 s; the steps a heel and
a toe, the impacts the surface's crack, thump, ring or shatter), CC0. Tests (`ShooterSurfaceTests.cpp`):
`ShooterGame.Surfaces.Footsteps` (running on each surface: the floor under the feet, both feet's steps; no physical
material and glass: the default's), `.LadderSteps` (a step every 0.35 s while climbing, the two in turn, none hanging
still) and `.Impacts` (an AK-47 shot at a wall of each surface: the hit's surface and its sound; the marks; flesh,
kevlar and the helmet on a character); `ShooterGame.Map.TenPawnsOnDeLeon` checks de_leon's ground (the
dirt), a crate (wood), a wall (concrete) and a lamp (metal).

## Registries

The EE cannot walk the level's actors for every bot every frame (ps2-shipping N20), so `AShooterGameMode` keeps what
the game looks up, in the level's order (what walked the level before finds the same actors in the same order):

| Registry | Filled | Emptied | Read by |
| --- | --- | --- | --- |
| `GetZones` (every `ATriggerVolume`), `GetPlayerStarts` | the level's when the game mode is made (`PostInitializeComponents`), the spawned ones as they spawn (`UWorld::AddOnActorSpawnedHandler`) | the game mode's end | `FindZone` (the buy zones, the bomb sites), `ChoosePlayerStart`, the round's start, `GetTeamSpawnLocation`; the bomb sites' names (sorted by name) and places and each team's starts are sorted out once, when a volume or a start joins or goes |
| `GetPawns` | `AShooterCharacter::BeginPlay` | `EndPlay` | the pickups, the starts' occupancy, the new match, the round's clean-up, `FShooterMatchChecker` |
| `GetPickups` (weapons and the bomb on the floor) | `AShooterWeapon::OnDropped`, `AShooterBomb::Drop` | the pickup, `EndPlay` | the round's clean-up |
| `GetBombs`, the grenades in flight | `BeginPlay` | `EndPlay` | the defuse (`StartUse`), the round's clean-up |

`CountAlive` counts both teams once a frame (every bot asks for both); a death, a possession or a team change counts
again (`NotifyPawnsChanged`). `AShooterWeapon::FindWeaponClass` lists the weapon classes once (the buy menu asks for
seven prices a frame). A world without a ShooterGameMode has no registries: nothing is picked up there.

Tests: `ShooterGame.Registry.MapOnDeLeon` (de_leon's sites A and B with their floors, both buy zones, ten starts, the
ten pawns), `ShooterGame.Registry.PickupsAndPawns` (a dropped weapon leaves the pickups when taken or destroyed, a
destroyed pawn the pawns, a volume spawned later joins the zones), `ShooterGame.Effects.MuzzleFlashPool` (a rifle's
long burst spawns no light after the first, and the flashes go out) and `ShooterGame.HUD.TextCache` (nothing formatted
when nothing changed; a score formats one line, a kill three).

## Bot match

`ShooterGame -nullrhi -benchmark -botmatch [-rounds=N] [-seed=N]` plays a match of bots and exits (P21):

- `-botmatch`: the local player spectates (no team), the bots fill both teams, and the match is `-rounds=` rounds long
  (10; it sets `MaxRounds`, so the teams switch sides after half of them and a team with the majority ends it sooner);
  then the game exits. `-seed=` sets `RandomSeed` (as `?seed=`).
- Every frame `FShooterMatchChecker` checks the invariants: each round that ends has a reason and gives its winner one
  point (the scores add up to the rounds with a winner), the money stays within [0, `MaxMoney`], no team has more
  than `MaxPlayersPerTeam`, a live pawn's health is within (0, its max] and its feet are not below the waypoints'
  lowest floor; the halftime comes after round `MaxRounds / 2` and only there, every player is then on the other team,
  the scores swapped sides and nobody has more than the start money, and a round past it without the swap is an
  error too. A broken one is logged as an error; the rounds not ending within their longest time (the freeze, the
  round, the bomb's timer and the result, each) fail the match too.
- The end: `Botmatch OK: <rounds> round(s), CT <n> - T <n>, <kills> kill(s), seed <n>, sides switched after round <n>,
  reasons [...]` (the scores are the sides' at the end) and exit code 0,
  or `Botmatch FAILED` and exit code 1 (`FPlatformMisc::RequestExitWithStatus`); then `Botmatch budget:` with the
  reflected types, the UObjects' peak, the names and the heap (the PS2 port's targets:
  [Budgets.md](../../Engine/Platforms/PS2/Documentation/Budgets.md)).
- `-benchmark` (the engine's): the fixed 30 Hz steps ([ps2-shipping](../../Docs/PLANS/ps2-shipping.md) D4) run without
  waiting for the clock; ten rounds take a second or two. A run paced to the clock (a window, the PS2) plays the same
  steps, so the same seed plays the same match headless or drawn. The bots' choices, the weapons' spread and the
  rounds come from seeded streams and the steps are fixed, so a seed replays the same match: `BotMatch.bat` (10 rounds, seed 7 by default) plays it twice and fails when the
  summaries differ, and a staged Shipping build (`BuildCookRun.bat`) plays three rounds (Shipping logs nothing, so only
  the exit code tells). Since [ps2-polish](../../Docs/PLANS/ps2-polish.md) P2b (CS's accuracy crouched, walking and
  still, and its recoil by state; the bots crouch to fire at range) seed 7 logs `Botmatch OK: 10 round(s), CT 5 - T 5,
  63 kill(s), seed 7, sides switched after round 5` (P2: `CT 4 - T 6, 59 kill(s)`). At 0.24.0 (since ps2-shipping N29, the floor slabs as the ground) it logged
  `9 round(s), CT 3 - T 6, 55 kill(s)`: a team reached the majority after nine rounds (N28's de_leon on one ground box: `10 round(s), CT 5 - T 5, 65 kill(s)`, the terrorists winning every
  round; over seeds 1 to 24 then, the terrorists won 61 % of the rounds with 6.1 kills a round).
  ps2-polish P3's bots (the knife, the lookouts, the ladders, the pickups) and de_leon's CT starts out of the mid
  doors' line log `Botmatch OK: 8 round(s), CT 2 - T 6, 55 kill(s), seed 7, sides switched after round 5`; over seeds
  1 to 24 the terrorists win 49 % of the rounds (60 % after P2b, before P3) with 7.1 kills a round.

## de_leon

A 60 × 48 m desert town in the style of CS 1.6's de_dust ([ps2-shipping](../../Docs/PLANS/ps2-shipping.md) N28;
north up: the T spawn to the south, the CT spawn to the north, A to the east, B to the west; `#` walls and houses,
`^` under an arch or a lintel, `n` the B tunnel, `=` the low wall at B with its player clip, `c` / `C` crates, `H` the
ladders to two roofs, `a` / `b` the bomb sites, `+` / `t` the team starts; 1 character = 1 m across, about 4 m down):

```text
       W (-Y)                   Y=0                  E (+Y)
  30 ##################################################
  26 #######bbbbbb     ++           ++    aaaaaa#######
  22 #  bcccbbbbbb                        aaaaaaaaaa  #
  18 #  bbbbbbbbbb  ##                ##  aaccaaaaaa  #   <- the site walls
  14 #  =====bbbbb  ##          C     ##  aaaaaaaaaa  #
  10 #                                                #
   8 #####        ###########^^###########        #####   <- mid doors (3 m doorway)
   4  ####       H######            ######H       ####
   0  ####                 c                      ####   B long | short B | mid | short A | A long
  -4  ####c       ######            ######cc      ####
  -8  ######nnnn########            ######        ####
 -14  ######nnnn##########^^^^^^^^###########^^#######   <- the B tunnel, the mid arch, the A long gate
 -18 #                                                #   the T plaza
 -22 #           ccc                    cc            #
 -28 #######              t t t t t        CC   #######
 -32 ##################################################
```

Sandstone walls and houses with darker caps, sand on the T side, mid and the longs, paving at the CT spawn and the
sites, wooden crates, the mid doors' open wings, the site letters on the site walls (each material with its physical
material: [Surfaces and their sounds](#surfaces-and-their-sounds)); the textures are painted by the
script (P4 / P8), the light (the sun, the sky, the lamps of the tunnel and the mid doors) is baked into the vertices
by LeonCook. The complete sketch, the cells, the waypoint graph and the Blender pipeline are in
[Docs/LEVELS.md](../../Docs/LEVELS.md#worked-example-de_leon) and
[Docs/ART_PIPELINE.md](../../Docs/ART_PIPELINE.md#the-map-de_leon). `SourceArt/Maps/make_de_leon.py` builds the map
in Blender (`blender --background --factory-startup --python ...`), saves `de_leon.blend` and exports `de_leon.glb`;
`SourceArt/ImportList.ini` imports it to `/Game/Maps/de_leon` with its meshes, materials and textures. The project's
`DefaultEditor.ini` makes the `BombSite_*`, `BuyZone_*` and `Ladder*` nodes trigger volumes and refuses a map without
both sites, both buy zones and both teams' starts. Only CC0 art enters the project
([SourceArt/LICENSES.md](SourceArt/LICENSES.md)). The Blender scripts share `SourceArt/leon_art.py` (the budgets, the
shared skeleton, the fixed glTF export); `SourceArt/check_art_determinism.py` checks that they export the same bytes
every run ([Docs/ART_PIPELINE.md](../../Docs/ART_PIPELINE.md)).

Looking at the map: `ShooterGame.exe -ExecCmds="ViewFrom <X> <Y> <Z> <Pitch> <Yaw>" -Screenshot=<file.bmp>
-ExitAfterFrames=20` saves a view from a point (centimetres and degrees; `ViewFrom 0 0 5200 -89 0` looks down on the
whole map); the view stays after the round's spawn until `ViewPawn`.

## Layout

```text
Game/ShooterGame/
├── ShooterGame.lproj                  module ShooterGame, TargetPlatforms Win64 and PS2
├── Config/                            DefaultEngine.ini (map, game mode, Weapon channel, surface types),
│                                      DefaultGame.ini (tuning, weapons, bots, memory card, cook), DefaultInput.ini
│                                      (CS keys, mouse, DualShock), DefaultEditor.ini (map import rules)
├── Content/                           Maps/de_leon.lmap (+ de_leon/Meshes, Materials), Characters/ (CT and T bodies,
│                                      arms, animations), Weapons/ (meshes, materials), Sounds/, PhysicalMaterials/
├── SourceArt/                         Blender and Python scripts (Maps/, Characters/, Weapons/, Sounds/, leon_art.py,
│                                      check_art_determinism.py), .blend, .glb and .wav sources, ImportList.ini,
│                                      LICENSES.md
└── Source/
    ├── ShooterGame.Target.cmake       the game (Win64, PS2)
    ├── ShooterGameTests.Target.cmake  the project's test program (LeonAutomationTests + ShooterGame's tests)
    └── ShooterGame/                   the game module (Public/, Private/, Private/Tests/)
```
