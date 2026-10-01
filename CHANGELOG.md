# Changelog

All notable changes to **Leon Engine** (formerly Geon) are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project aims to follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- **Larger matches: up to 20 players** (Budgets.md, "Players"). A bot sweep in PCSX2 (`MeasurePS2 -ExtraArgs
  -teamsize=N`, both maps, 10 to 32 players) finds the PS2 at 30 fps (p95 33.5 ms) up to 20 players, 22 missing it on
  both maps; the EE's world tick limits first (about 0.45 ms a player), not VU1, the GS, the AI's perception or memory
  (7.9 MB at 32 players). The main menu offers 1 to 19 bots (9 by default, as before); the teams grow to take them
  (`MaxPlayersPerTeam` 5 up to `MaxTeamSize` 16), and `?teamsize=` / `-teamsize=N` set them (`-botmatch -teamsize=10`:
  20 bots). On Win64 the bot match replays identically at 12 to 32 players.
  - de_leon and de_harbor have sixteen starts a side (`make_de_leon.py`, `make_de_harbor.py`: CS's five first, as
    before, then eleven in the buy zones, checked 1.4 m apart, out of everything, near a waypoint, the CTs out of
    mid's line); the 5v5 bot matches replay as before.
  - Tests: `ShooterGame.Map.TwentyPawns` (the menu's largest match on each map), `ShooterGame.Menu.MatchOptions` (the
    teams' sizes), the maps' sixteen starts (131 ShooterGame tests).
- **The real minimap** ([ps2-polish](Docs/PLANS/ps2-polish.md) P7): the radar draws the map's overview under its dots,
  turned with the view, as CS 1.6's overviews ([Docs/LEVELS.md](Docs/LEVELS.md#the-overview)).
  - LeonEd's `FMapOverview` renders each map at its import, after the lighting bake: the GS scene renderer's frame of
    the Static meshes (`FGSSceneCapture`, Renderer) in orthographic views on GSReference's software GS, no GPU, the
    same bytes on every machine (`CheckReimport`). From above cut at 2.5 m for the floors, and from below, plain and
    mirrored, to tell the obstacles (walls, houses, containers, crates); styled as CS's (greyer, flatter floors, dark
    obstacles and outside, edge lines), 128 x 128 PSMT8 of one level, `<Map>/T_<Map>_Overview`, 17 KB of VRAM. LeonEd
    links Renderer and GSReference for it; nothing opens a window.
  - `AWorldSettings::OverviewSettings` (`FWorldOverviewSettings`: the texture, the square it shows, `GetUV`); the
    project's `[/Script/LeonEd.MapImportSettings] bBuildOverview`, `OverviewResolution`, `OverviewClipHeight`,
    `MapsWithoutOverview`; the engine's maps get none. The cook keeps a texture paletted already as it is and counts it
    in its VRAM report (`CookerVersion` 2).
  - `AShooterHUD::MakeRadarOverviewTriangles`: the radar square on the overview, clipped where it ends, at most six
    textured triangles (`FCanvasTriangleItem`).
  - PCSX2 (`MeasurePS2 -Label minimap`): 29.62 fps, p50 / p95 / p99 33.5 ms as 0.25.0, LoadMapMisc 710 of 1 024 KB.
  - Tests: `System.LeonEd.MapOverview.RenderAndProject`, `System.Renderer.GSEmulator.RadarFrame` (G8),
    `ShooterGame.HUD.RadarOverview`, and the maps' tests check their overviews (607 engine tests, 130 ShooterGame's).
- **de_harbor**, ShooterGame's second bomb defusal map: a 64 × 56 m industrial port at the end of the afternoon (the
  T truck yard to the south, the CT yard to the north, bomb site A on the quay apron under a gantry crane, bomb site B
  in a roofed warehouse; three routes: A long along the quay to a chicane of container stacks, mid through a gate into
  the CT spawn's courtyard, the alley to the warehouse's front door with a roofed connector from mid). An original
  layout made by the same pipeline as de_leon with none of its art, to prove the pipeline does not depend on it
  ([Docs/LEVELS.md](Docs/LEVELS.md#worked-example-de_harbor)).
  - `Game/ShooterGame/SourceArt/Maps/make_de_harbor.py` (Blender through `leon_art`, deterministic, in
    `check_art_determinism.py`'s list): 4 216 triangles in 112 pieces, 9 cells and 22 portals, 14 materials with
    their own painted textures (asphalt, concrete, wall panels, corrugated sheet, steel, the crane's paint, three
    containers' colours, pallet crates, water, shutters, the site plates) and physical materials, shared meshes for
    the containers, crates, bollards and lamps, a late sun and seven baked lamps, the quay's clip, a ladder up a
    container stack at A, 47 waypoints with three lookouts a site. The script also fails when two things at walking
    height overlap.
  - `Sky/make_sky.py` writes a second sky from presets, the coast (`Sky_Coast.hdr`, `/Game/Sky/T_Sky_Coast`): a greyer
    zenith, a cool haze, a warm glow toward the low sun, the sea below the horizon. The desert's bytes are unchanged.
  - The main menu offers both maps (`+MapNames=/Game/Maps/de_harbor`); the cook and the PS2 pak and ISO take it with
    every map under `/Game/Maps` (the ISO: 8 464 384 bytes). PCSX2 (`MeasurePS2 -Map /Game/Maps/de_harbor`): 29.92 fps,
    p50 / p95 / p99 33.5 / 33.5 / 34.0 ms, the scene 6.9 ms, GMalloc's peak 5 176 KB, `LoadMapMisc` 710 of 1 024 KB;
    the same from the disc.
  - The bots play it: over seeds 1 to 48 of the bot match the terrorists win 53 % of the rounds (de_leon 56 % over 1
    to 24); `BotMatch.bat 10 7 /Game/Maps/de_harbor`: `Botmatch OK: 8 round(s), CT 2 - T 6, 57 kill(s), seed 7, sides
    switched after round 5, reasons [3,3,4,4,4,3,3,3]`.
- `BotMatch.bat [Rounds] [Seed] [Map]` and `MeasurePS2 -Map <map>`: a bot match on another map (`-map=`, which the
  game already read); `RunGates.bat` plays de_harbor's bot match too (`BotMatchDeHarbor`).
- Tests: `ShooterGame.Map.DeHarborHoldsTheGame`, `TenPawnsOnDeHarbor`, `NavigationCoverage` (both maps' waypoint
  graphs are one piece and reach the sites and the starts), `ShooterGame.Bots.MatchOnDeHarbor`; `RequiredTags` imports
  de_harbor's source, `MeshQuantization` checks its placed meshes, the menu test steps the map list (129 ShooterGame
  tests).

### Changed

- **The team bodies are Counter-Strike 1.6's player models**: the SAS for the counter-terrorists (`SK_Body_CT`) and
  the Leet Krew for the terrorists (`SK_Body_T`), provided by the user (Valve's models, Sketchfab downloads; not CC0:
  `SourceArt/LICENSES.md`, the decision to ship them is pending). `SourceArt/Characters/make_cs16_characters.py`
  (Blender through `leon_art`, deterministic, in `check_art_determinism.py`'s list) imports `CS16/<model>/*.fbx`,
  carries the models' own GoldSrc weights from their Valve biped (`Bip01 ...` and its helpers) over to `SKEL_Body`'s
  23 bones (at most two a vertex), turns and scales them onto `SKEL_Body`'s T pose (the limbs moved onto its bones, the
  boots on the floor) so every third-person clip plays unchanged, and reduces the 512 × 512 skins to one 128 × 128
  texture each (P8 in the cook: the same VRAM as before; the chrome's few triangles take the skin's nearest texel, the
  backpack and the defuse kit are left out). 752 triangles each (were 932 and 828). Same asset names, so nothing in the
  game changes: the hit groups, the capsule and the `Flesh` physical material are as they were. The painted bodies
  are gone; `make_characters.py` keeps the skeleton, the clips and the teams' colours the first-person arms use.
  The bot matches are unchanged (`Botmatch OK: 8 round(s), CT 2 - T 6, 57 kill(s), seed 7` on both maps); PCSX2
  (`MeasurePS2`, de_leon): 29.62 fps, p50 / p95 / p99 33.5 / 33.5 / 33.5 ms, the scene 6.8 ms; each body's texture
  23 KB of VRAM, as before.
- Everything in English, code and assets: the second map, first named `de_puerto`, is `de_harbor` (its package
  `/Game/Maps/de_harbor` and folder, `make_de_harbor.py`, `de_harbor.glb` / `.blend`, the `+MapNames=` entry, the
  `BotMatchDeHarbor` gate and the tests `ShooterGame.Map.DeHarborHoldsTheGame`, `TenPawnsOnDeHarbor` and
  `ShooterGame.Bots.MatchOnDeHarbor`); reimported from the same source, so the match replays as before
  (`BotMatch.bat 10 7 /Game/Maps/de_harbor`: the same `Botmatch OK` line). `Docs/PS2OFFICIAL/RESUMEN1.md` is
  translated as `PIPELINE_SUMMARY.md`, and a font test measures "Hello". The plans under `Docs/PLANS/` stay in
  Spanish; `de_leon` keeps the engine's name.

## [0.25.0] - 2026-10-01

Polish from playing 0.24.0 ([ps2-polish](Docs/PLANS/ps2-polish.md) P0 to P10, with P2b, P3b, P5b and P8b): the
vanishing characters and the bots' knife and idle walks fixed, CS 1.6's accuracy and recoil, the bots' pickups,
lookouts, ladders and post-plant, the crouch toggle and dropped weapons, fonts, a textured canvas and UMG's widgets,
CS 1.6's HUD and a table scoreboard, a generated HDR sky, the near geometry clipped on VU1, and the main menu, team
selection and pause. The real minimap (P7) is not done ([Docs/PENDING.md](Docs/PENDING.md)). The engine and ShooterGame
content is resaved for 0.25.0. PCSX2 ([Budgets.md](Engine/Platforms/PS2/Documentation/Budgets.md), the row «0.25.0»):
every frame after the first at 33.5 ms (p50 / p95 / p99), 29.62 fps with the first frame's travel from the main menu,
the scene 6.6 ms. The ISO (7 784 448 bytes) boots in PCSX2 into the main menu. `BotMatch 10 7`: `Botmatch OK: 8
round(s), CT 2 - T 6, 57 kill(s), seed 7, sides switched after round 5`.

### Added

- A generated HDR sky ([ps2-polish](Docs/PLANS/ps2-polish.md) P8).
  - `Game/ShooterGame/SourceArt/Sky/make_sky.py` (Python's standard library) generates de_leon's desert sky as a long-lat
    Radiance HDR panorama (1 024 x 512): a zenith-to-haze gradient, sand below the horizon, the sun where the map's
    baked sun is (`SUN_DIRECTION` of `make_de_leon.py`) and seeded clouds; `check_art_determinism.py` runs it twice.
  - `UTextureCube` (UE) with its six faces as `UTexture2D` subobjects and a `HorizonColor`, and its importer
    `UTextureCubeFactory` (LeonEd, `-type=TextureCube`, `.hdr`): each face texel samples the panorama along its
    direction, tone-mapped by the ACES filmic curve at `ExposureBias` to sRGB bytes, faces of `CubeFaceSize` (128);
    `/Game/Sky/T_Sky_Desert`. `UTexture2D::AddressX` / `AddressY` (`ETextureAddress`: `Wrap`, `Clamp`) set the GS's
    CLAMP; the faces clamp.
  - `AWorldSettings::SkySettings` (`FWorldSkySettings::SkyCubemap`): the GS scene renderer draws the sky after the
    clear and before the world, a box of small batches around the eye (`FSkyBoxGeometry`), unlit, unfogged, no depth
    test, no Z written, on VU1 and never through the EE's clipper. `FWorldFogSettings::bInscatteringColorFromSky`: the
    fog takes the sky's horizon colour.
  - The map importer reads a `WorldSettings` node's extras as world settings property paths (`SkySettings.SkyCubemap`,
    `FogSettings.bEnableFog`, ...). de_leon has the sky and its fog on, from 30 m to the far plane.
- Fonts, a textured canvas and UMG's missing widgets, the base of the new UI ([ps2-polish](Docs/PLANS/ps2-polish.md)
  P5).
  - `UFont` (UE's offline font) and its importer, `UTrueTypeFontFactory` (LeonEd, `-type=Font`, `.ttf`, stb_truetype):
    the glyphs of `UnicodeRange` (ASCII and Latin-1, so Spanish) rasterized at `Height` pixels into PF_P4 pages of at
    most 256 x 256 (white texels, the coverage in the CLUT's alpha, 16 levels), with whole-pixel advances, bearings and
    kerning pairs; the same file gives the same bytes. The engine's font is DejaVu Sans Condensed 2.37
    (`Engine/SourceArt/EngineFonts`, the Bitstream Vera license with the DejaVu changes in the public domain;
    `Engine/SourceArt/LICENSES.md`), imported at 10, 14, 20 and 32 pixels as `/Engine/EngineFonts/DejaVuSansCondensed*`,
    one page each; `UEngine::GetTinyFont`, `GetSmallFont`, `GetMediumFont` and `GetLargeFont` load them
    (`[/Script/Engine.Engine] TinyFontName ...`).
  - The canvas draws text in a font, a textured SPRITE a glyph laid out by its metrics and kerning, UTF-8 decoded
    (`FCanvas::DrawText(Font, ...)`, `MeasureText(Font, ...)`, `DrawShadowedString`, `FCanvasTextItem` with a shadow or
    an outline), and textured tiles (`DrawTile` with a `UTexture`, UVs, colour and alpha; `FCanvasTileItem` with a
    rotation about a pivot: two UV triangles). `FGSSceneRenderer::DrawCanvas` samples them through the texture cache
    (UV, MODULATE, nearest one to one and bilinear otherwise), after the GS conformance scene `TexturedCanvas` (D7:
    GSReference, the OpenGL emulator and GSConformance.elf in PCSX2).
  - UMG: `UHorizontalBox` / `UHorizontalBoxSlot` (automatic and fill sizes), `UButton` (`FButtonStyle`; `OnClicked`,
    `OnPressed`, `OnReleased`, `OnHovered`, `OnUnhovered`), `UWidgetSwitcher`, `UTableView` (columns with a header, a
    width and an alignment, rows sorted by a column, a highlighted row; cached text widths), `UImage` with a texture
    brush (`FSlateBrush`, `SetBrushFromTexture`), `UTextBlock` with a font (`FSlateFontInfo`) and a shadow. Focus and
    navigation, small and UE-like: `AHUD::InputKey` gives the widgets the keys before the game, the focused widget and
    its parents first, then the arrows, the d-pad and Tab move the focus to the nearest focusable widget painted that
    way (`FHittestGrid`, explicit rules), Accept (Enter, Space, the pad's Cross) presses a button and clicks on the
    release; on Win64 the free mouse hovers and clicks (`AHUD::InputMouseMove`,
    `IRendererModule::WindowToRenderTarget`). SlateCore gains `FGeometry`, `FReply`, `FKeyEvent` / `FPointerEvent`,
    `EUINavigation` and `FNavigationConfig`.
  - Tests: `System.Engine.Font.MetricsAndKerning`, `System.Engine.Canvas.TextGlyphs`, `.TexturedTile`,
    `System.LeonEd.Factories.TrueTypeFont.Rasterize` and `.Import` (byte-identical reimport),
    `System.GSReference.Texture.TexturedCanvas`, `System.Renderer.GS.Canvas.Text` (the reference's frame),
    `System.Renderer.GSEmulator.CanvasFrame` (the emulator against it), `System.UMG.Focus.Navigation`,
    `System.UMG.Button.Activation`, `System.UMG.Panels.HorizontalBoxAndSwitcher`, `System.UMG.Image.Texture`,
    `System.UMG.TableView.SortAndLayout` (588 engine tests, 105 of ShooterGame); `ShooterGame.HUD.RoundInfo` counts
    the glyphs' sprites.

- ShooterGame's crouch toggles, and the bomb and the weapons are dropped and picked up as in CS
  ([ps2-polish](Docs/PLANS/ps2-polish.md) P4): a press of the crouch key (Left Ctrl, Circle) crouches and the next
  stands up, the default; the player's option `bToggleCrouch` (`UShooterPersistentUser`, the `Settings` slot on the
  memory card; the `SetToggleCrouch 0|1` command) holds it instead; the bots crouch as before. The carried bomb is
  CS 1.6's slot 5: 5 or the D-pad's down draws it (`AShooterCharacter::DrawBomb`: the weapon put away, the arms hidden,
  the HUD's weapon line `C4`), a weapon's key puts it away, and the drop key (G, the D-pad's right) drops it ahead of
  the feet (`DropBomb`); its dropper takes it back only after `AShooterBomb::PickupDelay` (1 s). A weapon or the bomb
  picked up from the floor shows `Picked up <item>` on the HUD for `PickupNoticeDuration` (2 s), and a weapon that is
  not drawn at once plays its draw sound (`AShooterCharacter::PickUpWeapon`); it keeps its rounds and its silencer.
  Tests: `ShooterGame.Input.CrouchToggleAndHold`, `.DropAndPickUpWeapon`, `.DropAndPickUpBomb`; `Settings.RoundTrip` and
  `Config.InputAndChannels` updated (102 ShooterGame tests). The bot match is unchanged
  (`Botmatch OK: 9 round(s), CT 3 - T 6, 55 kill(s), seed 7, sides switched after round 5`).
- ShooterGame's bots rush with the knife, pick up weapons, watch the sites and climb ladders
  ([ps2-polish](Docs/PLANS/ps2-polish.md) P3). With the knife a bot runs the path to its enemy and cuts only within
  reach: a stab when the enemy's back is turned, else slashes as it circles in (`EngageWithKnife`); a fight never draws
  a grenade (`AShooterCharacter::EquipBestWeapon(false)`). A bot without a loaded primary goes for one on the floor
  within `PickupSearchDistance` (15 m), and out of ammunition for any loaded weapon within 30 m (the `PickUp` branch):
  `AShooterWeapon::CanBePickedUpBy` lets a bot swap a spent weapon for it (the player keeps CS's walk-over into a free
  slot). With no enemy in sight the bots keep the sites' lookouts (`AShooterGameMode::GetBombSiteLookouts`: the map's
  waypoints flagged `Lookout`, else the site's nearest waypoints), turning between each team's directions there (first
  the main way in: the graph's path toward the other team's spawn) and moving from one to another; the terrorists
  escort the carrier watching its flanks and take the site's lookouts once it is near; the counter-terrorists rotate
  after `RotateTime` or on a teammate's report at the other site (`RotateOnReportChance`); the terrorists hunt with
  `HuntTimeLeft` (30 s) left; the hunt roams the waypoints; a bot on the move looks along its path. Ladders: the
  navigation links two waypoints flagged `Ladder` across the climb (`FWaypointLinkParams::MaxLadderLinkDistance`),
  the path follower does not jump at them (`AAIController::GetCurrentTargetLocation`, UE's), and the bots climb up
  and down facing the ladder (`UShooterCharacterMovement::GetLadderNormal`); `bCanClimbLadders` and
  `HoldAndLookAround` are gone (`CheckBannedApis.ps1`). de_leon gets six lookouts off the lanes' line, the ladders'
  feet and tops, and CT starts out of the mid doors' line (the terrorists' spawn saw them across the map). The bots
  see enemies within `SightRadius` (35 m). Over seeds 1 to 24 the terrorists now win 49 % of the rounds (60 % before).
  A scene component's `DetachAllChildren` no longer loops for ever on a child that points to another parent (a bot
  match hung in the exit's collection).
  Tests: `ShooterGame.Bots.KnifeRushesAndKills`, `.PicksUpAWeaponOutOfAmmo`, `.VisitsLookouts`, `.ClimbsALadder`,
  `System.AIModule.Gameplay.NavigationAutoLinkLadders`; `ShooterGame.Map.DeLeonHoldsTheGame` and
  `.Movement.Ladder` and `System.Engine.Components.AttachmentRulesAndSockets` extended (109 ShooterGame tests). The
  bot match logs `Botmatch OK: 8 round(s), CT 2 - T 6, 55 kill(s), seed 7, sides switched after round 5`.
- ShooterGame's terrorists hold the planted bomb and the counter-terrorists retake it together
  ([ps2-polish](Docs/PLANS/ps2-polish.md) P3b). The planter calls "Cover me!"; the terrorists hold from the site's
  lookouts within `PostPlantHoldRadius` (10 m) of the bomb watching the CT's ways in, call "Hold this position." and
  chase nothing farther; a defuse is heard (`AShooterBomb::DefuseNoiseLoudness`) and they come for the defuser. The
  counter-terrorists gather at a staging point toward their spawn (`RetakeStagingDistance`) until a teammate joins or
  `RetakeWaitTime` passes ("Go go go!"), defuse once the site is clear (`SiteClearTime`) or the time is short, and give
  the retake up when outnumbered by `RetakeGiveUpAdvantage` or too late ("Team, fall back!", back to their spawn). Over
  seeds 1 to 24 the terrorists win 56 % of the rounds (1 to 48: 53 %), the bomb explodes in 5 % of the rounds (none
  before) and is defused in 21 %. Tests: `ShooterGame.Bots.TerroristsHoldThePlantedBomb`,
  `.TerroristsEngageTheDefuser`, `.RetakeGathers` (112 ShooterGame tests). The bot match logs
  `Botmatch OK: 8 round(s), CT 2 - T 6, 57 kill(s), seed 7, sides switched after round 5`.
- ShooterGame's main menu, team selection and pause menu ([ps2-polish](Docs/PLANS/ps2-polish.md) P9), UMG widgets in
  CS 1.6's olive and amber (`UShooterMenuWidget`: a centred panel of `UShooterMenuButton` lines, left and right
  stepping an option), driven by the pad alone or the keyboard and the mouse, readable at 640 x 448.
  - The main menu is the new GameDefaultMap, `/Game/Maps/MainMenu` (a small desert backdrop made by the standard
    library script `SourceArt/Maps/make_main_menu.py`, exempt from the project's required map tags by the new
    `MapsWithoutRequiredTags` of `[/Script/LeonEd.MapImportSettings]`), with `AShooterGame_Menu` (by the map's prefix,
    `GameModeMapPrefixes`) and `AShooterPlayerController_Menu`: the map (`+MapNames=`), the bots' difficulty (Easy,
    Normal, Hard, Expert), the rounds to win (3, a best of 5, by default; 5, 8, 16), the number of bots (1 to 9),
    Options, Start and Quit (Win64). The choices are saved with the player's options (`UShooterPersistentUser`) and
    travel as URL options, `?bots=N?difficulty=Hard?winrounds=3`, which `AShooterGameMode::InitGame` reads
    (`MaxRounds = 2 N - 1`); `-botmatch` skips the menu.
  - The team menu (`UShooterTeamMenuWidget`): a player who joins without `?team=` spectates and the warmup waits for
    its choice (CT, T, Auto, Spectate; `?team=` takes the same four); then `AShooterGameMode::RebalanceBots` shares the
    match's bots out so the teams are as even as possible counting the player (nine bots with the player on CT: 4 CT
    and 5 T). The pause menu's Change team changes sides by CS's rule: the change takes effect at the next round, the
    player dying if alive during a fought round, and the bots even the sides out again then.
  - The pause menu (`UShooterPauseMenuWidget`, Escape or Start): Resume, Change team, Options (sensitivity, invert Y,
    volume, crouch toggle; saved once when the page is left), Quit to main menu. The game pauses as UE does.
  - The bots' difficulty is a preset each (`AShooterAIController::DifficultyPresets`, `ApplyDifficulty` when the game
    mode adds a bot; `FShooterBotSkill`): the reaction, the aim error, its settling and floor, the turn rate, the
    recoil control and the memory, modelled on CS's bot profiles. Normal is the bots' old skill: the bot match is
    unchanged.
- The engine's pause (UE's): `AGameModeBase::SetPause` / `ClearPause` / `AllowPausing` / `IsPaused` with
  `FCanUnpause`, `AWorldSettings::GetPauserPlayerState`, `UWorld::IsPaused`, `APlayerController::SetPause`, `IsPaused`,
  `CanUnpause` and the `Pause` command, `UGameplayStatics::SetGamePaused` / `IsGamePaused`. A paused world steps as
  `LEVELTICK_PauseTick`: its time, its timers, the physics step and the effects stand still and only the tick
  functions with `bTickEvenWhenPaused` run (the player controllers, which then only process their input, and the
  HUDs with their widgets); `UWorld::GetRealTimeSeconds` goes on. `UGameplayStatics::OpenLevel` (UE's, over
  `SetClientTravel`) and `GetPlayerController`; `APlayerController::SetInputMode` with `FInputModeUIOnly`,
  `FInputModeGameAndUI` and `FInputModeGameOnly` (the viewport's mouse capture: the UI's free cursor does not look).
  Tests: `System.Engine.World.Pause`, `System.Engine.Travel.OpenLevel`, `System.LeonEd.MapFactory.EngineMapsSkipRequiredTags`
  extended; `ShooterGame.Menu.MatchOptions`, `.DifficultyPresets`, `.BotSplit`, `.TeamChoiceAndChange`, `.PauseMenu`,
  `.MainMenuToMatchAndBack`, `ShooterGame.Settings.RoundTrip` extended (118 ShooterGame tests).
- ShooterGame's HUD, scoreboard and buy menu are CS 1.6's ([ps2-polish](Docs/PLANS/ps2-polish.md) P6). The HUD:
  the health and the armor bottom left with their icons, the ammunition (`clip | reserve`), the weapon and the money
  (the buy zone's cart while the player may buy) bottom right, the bomb (blinking in a site) and the defuse kit on the
  left, the clock with a stopwatch between the scores top centre, the kill feed top right with the weapons' icons, the
  headshot's and a skull for the world, and a compact frame readout under the radar (`30 fps  33.3 ms`, the option
  `bShowFrameStats`, `SetShowFrameStats 0|1`, saved on the memory card). The numbers in DejaVu Sans Condensed Bold at
  24 px and the headings at 14 px (`/Engine/EngineFonts/DejaVuSansCondensedBold24` / `14`, vendored from the same
  release, `Engine/SourceArt/LICENSES.md`); every text and icon over the world with a black drop shadow and the top
  blocks on dark bands, readable on a bright sky. The icons are one atlas, `/Game/UI/T_HUDIcons`, drawn by
  `SourceArt/UI/make_hud_icons.py` (Python's standard library, the same bytes every run; PSMT4 once cooked). The
  weapons have CS's display names (`AShooterWeapon::DisplayName`, `GetItemDisplayName`: "AK-47", "Desert Eagle",
  "HE Grenade", ...) in the HUD, the kill feed, the buy menu and the notices (`Picked up AK-47`, `Bought M4A1`). The
  scoreboard is `UShooterScoreboardWidget`: a `UTableView` a team (name, DEAD / BOMB, score, deaths, BOT or latency)
  sorted by score, the player's row highlighted, the team's score over it; the buy menu's page is a `UTableView` (key,
  name, price; categories amber, what the player cannot afford grey, the pad's line highlighted). With the C4 drawn,
  Fire plants it while held (`AShooterCharacter::OnFirePressed`), as E does. The canvas gains UE's icons
  (`FCanvasIcon`, `FCanvas::MakeIcon` / `DrawIcon`) and triangle items (`FCanvasTriangleItem`, `FCanvasUVTri`, drawn
  in the tiles' order). Captures: `-ExecCmdsAfterFrames=N` (not in Shipping) holds `-ExecCmds=` until frame N, and
  `ShowScores 0|1` shows the scoreboard. Tests: `ShooterGame.HUD.IconAtlas`, `.DisplayNames`, `.KillFeedIcons`,
  `.Scoreboard`, `.BuyMenuTable`, `.FrameStats`, `ShooterGame.Input.FirePlantsTheBomb`,
  `System.Engine.Canvas.TrianglesAndIcons`; `HUD.RoundInfo`, `HUD.TextCache`, `Settings.RoundTrip` and the pickup tests
  updated (590 engine tests, 119 ShooterGame tests). The bot match is unchanged.

### Changed

- The near geometry is clipped on VU1 and a lit draw takes two point lights, so no frame of the bot match falls to
  the EE any more ([ps2-polish](Docs/PLANS/ps2-polish.md) P8b). A vertex batch whose sphere crosses the near or far
  plane or the guard band is recorded too (`FGSVertexBatch::bClip`), and the same Static and Skinned programs clip
  it (`PS2RHI/Private/VU1/ClipTriangles.vsi`, a `CLIP_PROGRAM` macro both `.vsm` expand; LeonBuildTool passes
  `dvp-as -I` the folder): each vertex's clip space position, colour, coordinates and outcodes go to its own stream
  rows, then each triangle of the strips is rejected outside a side of the view, sent as it is inside the guard band,
  or clipped (Sutherland-Hodgman against each plane it crosses) and sent as a fan, facing the camera, in chunks of
  TRIANGLE packets that alternate under XGKICK. The C++ reference is `FGSPrimitiveEmitter::AddClippedVertexBatch`
  (the renderer's clipping loop moved there; `FGSCommandList::AppendExpanded` expands a clipped batch with it), and
  the emitter's guard band is `FGSPrimitiveEmitter::GuardExtent`. A lit draw takes the two point lights that light
  its bounds most (`FGSVertexDraw::MaxVU1PointLights`): P3's 83 ms frames were a firefight's muzzle flashes beside a
  lamp sending every lit draw near them, 145 skinned batches, to the EE's emitter. In PCSX2 the bot match goes from
  29.26 to 29.97 fps, p99 83.5 to 33.5 ms, the scene 9.32 to 5.70 ms; `MeasurePS2 -CloseUp` (a terrorist 80 cm before
  a fixed camera) measures the near geometry: the scene 3.69 to 1.82 ms (Budgets.md, the rows «ps2-polish P8b»).
  VU1Conformance clips 78 batches more (static and skinned, lit and unlit, textured, fogged, with point lights; each
  full chunk read back with the E bit and MSCNT): `VU1Conformance: PASSED (162 batch(es), 0 failed)`, the clipped
  vertices within Z 32 and 2^-16 of their STQ (seen: 24 and 0.2 of it). Tests:
  `System.Renderer.GS.Scene.PointLightsPerDraw`; `VertexBatches` and `SkinnedVertexBatches` (close up) record clipped
  batches and expand them to the same frame.
- ShooterGame's HUD, the buy menu and the debug overlay draw in the engine's 14-pixel DejaVu Sans Condensed instead of
  stb_easy_font's bars ([ps2-polish](Docs/PLANS/ps2-polish.md) P5); the HUD's layout is unchanged (P6 redesigns it,
  and its scoreboard, aligned with spaces, no longer lines up in the proportional font). The canvas now blends every
  tile and line by its colour's alpha (it drew them opaque), so the flash's white-out fades and the scoreboard's and
  the buy menu's backgrounds are as translucent as their colours say. A glyph is one sprite (four GS writes) where
  stb_easy_font drew several bars: in PCSX2 the canvas's EE time falls from 2.89 to 1.45 ms a frame (`Canvas Flush`;
  `GS Canvas` 1.33 ms) and the widgets' paint rises from 0.07 to 0.18 ms, at 29.98 fps, p50 / p95 / p99 33.5 ms
  (Budgets.md, the row «ps2-polish P5»).
- ShooterGame's accuracy follows CS 1.6's cases for crouched, still, walking, running and in the air
  ([ps2-polish](Docs/PLANS/ps2-polish.md) P2). Every hitscan weapon had the same `CrouchingSpreadMod` (0.8), and
  walking had no term of its own. Now the movement's term grows with the speed to `WalkingSpread` at `WalkingSpeed`
  (CS's 140 units a second, 356 cm/s, above the walk key's speed) and on to `MovingSpread` at the weapon's running
  speed (`AShooterWeapon_Instant::GetMovementSpread`). Each weapon sets `WalkingSpread` and `CrouchingSpreadMod` in its
  constructor: crouched is 0.5 (AK-47, AWP) to 0.65 (the pistols) of standing. The HUD's gap takes the view's height
  (`AShooterHUD::GetCrosshairGap(ViewHeight)`), and crouched its own gap closes by the same factor (CS's
  `ACCURACY_DUCK`): the AK-47's crosshair on 448 lines is 2.9 px crouched, 5.8 still, 10.9 walking, 29.2 running.
  Tests: `ShooterGame.Weapons.SpreadByState` (the table per weapon and state, and their order),
  `ShooterGame.HUD.DynamicCrosshair`; `ShooterGame.Weapons.SpreadModel` checks the movement's term (104 ShooterGame
  tests). The bots crouch to fire at range, so the bot match changes:
  `Botmatch OK: 10 round(s), CT 4 - T 6, 59 kill(s), seed 7, sides switched after round 5`.
- CS 1.6's recoil by state and the rifles' walking accuracy ([ps2-polish](Docs/PLANS/ps2-polish.md) P2b). The AK-47,
  the M4A1 and the MP5 kick by their owner's state as CS's `KickBack` branches do: `MovingRecoilScale`,
  `JumpingRecoilScale` and `CrouchingRecoilScale` (`FShooterRecoilScale`: CS's up and sideways arguments over the
  standing ones) scale each shot's kick, chosen by `AShooterWeapon_Instant::GetRecoilScale` (moving at all, in the air,
  crouched; `bRecoilMovingBeforeAir` for the AK-47 and the M4A1, which test the movement first). Crouched, the AK-47
  kicks up 0.9 times standing's, moving 1.5, in the air 2.0. The rifles and the MP5 walk as accurately as they stand
  (`WalkingSpread` 0: CS's cases for them only look past 140 units a second); the pistols and the AWP keep a walking
  term. Tests: `ShooterGame.Weapons.KickBackByState`; `SpreadByState` and `HUD.DynamicCrosshair` updated (a pistol for
  the full order, the AK-47 walking as still); `Bots.RecoilKicksTheAim` allows 2 degrees instead of 1.5, the bot
  strafing at a walk while it sprays (105 ShooterGame tests). The bot match changes:
  `Botmatch OK: 10 round(s), CT 5 - T 5, 63 kill(s), seed 7, sides switched after round 5`.

- ShooterGame's pad: Start opens the pause menu, and the buy menu moves to the D-pad's down where the player may buy
  (elsewhere the D-pad's down draws the C4, as before; CS on consoles bought from the D-pad) ([ps2-polish](Docs/PLANS/ps2-polish.md)
  P9). Escape opens the pause menu when neither the buy nor the radio menu is open. ShooterGame starts on the main menu;
  `ShooterGame.exe /Game/Maps/de_leon?team=CT` (or `-map=`) starts a match directly, as SmokeTest.bat now does.

### Removed

- `AShooterGameMode::bFillTeamsWithBots` and `AShooterAIController::Difficulty`, the single scale of the bots' skill
  ([ps2-polish](Docs/PLANS/ps2-polish.md) P9, D10): `NumBots` with `RebalanceBots`, and the difficulty presets.
  `CheckBannedApis.ps1` rejects them.
- The GS debug text's 5x7 uppercase bitmap font (`GlyphRows`, `GlyphOf`, `FGSDebugDraw::GetTextWidth` /
  `GetTextHeight`, `DrawString`'s scale; [ps2-polish](Docs/PLANS/ps2-polish.md) P5b, D10): the PS2's error screen and
  GSConformance's labels drew lowercase as uppercase but for "m" and "s". `FGSDebugDraw` now draws the game's DejaVu
  Sans Condensed compiled in (`GSDebugFontData.inl`, written by the new `LeonCook -run=EmbedFont` with
  `UTrueTypeFontFactory` at 10 and 14 pixels: two 128 x 128 PSMT4 pages and an alpha CLUT, 65 GS blocks, uploaded in
  place by `FGSDebugDraw::UploadFont`), a textured SPRITE a glyph with kerning and whole-pixel advances, UTF-8 and
  Latin-1; `MeasureString`, `GetLineHeight` and `FindLineBreak` word-wrap the error screen at its margins. No asset is
  loaded, so it still shows with a missing or damaged pak. The ELFs grow by 23 KB (ShooterGame 4 829 156 to 4 852 144
  bytes, GSConformance 1 477 128 to 1 498 852). Tests: `System.GSCore.DebugDraw.Upload`, `.Measure`, `.LineBreak`,
  `.String` rewritten, `System.LeonEd.Commandlets.EmbedFont.MatchesSource` (the checked-in file is what the generator
  makes today), `System.GSReference.DebugDraw.Text` (the frame's CRC, verified by eye) and
  `System.Renderer.GSEmulator.DebugText` (the emulator against the reference, 0 pixels apart). `CheckBannedApis.ps1`
  rejects the old names. `FChar::DecodeCodePoint` (Core) is the UTF-8 decoder `UFont` and the debug text share.
- `stb_easy_font` (the HUD's bitmap font), `HudFontScale`, `HudLineHeight`, `FCanvas::DrawTextBlock`, the canvas
  text's scale argument and `FPaintContext::MeasureTextOnly` ([ps2-polish](Docs/PLANS/ps2-polish.md) P5, D10):
  `UFont`, `FCanvas::DrawText` / `MeasureText` with a font, `UFont::GetLineHeight`. `CheckBannedApis.ps1` rejects them.
  No third-party library builds for the PS2 any more; STB is desktop-only (LeonEd's `stb_image` and `stb_truetype`).
- ShooterGame's hand-drawn scoreboard (`AShooterHUD::DrawScoreboard` and its space-aligned lines) and the HUD's `C4` /
  `KIT` text ([ps2-polish](Docs/PLANS/ps2-polish.md) P6, D10): `UShooterScoreboardWidget` and the HUD's icons.
  `CheckBannedApis.ps1` rejects them.

### Fixed

- Characters no longer vanish at some view angles and leave their weapons floating
  ([ps2-polish](Docs/PLANS/ps2-polish.md) P1). `ACharacter`'s `Mesh` stayed Static, the default of every scene
  component, and only the capsule was Movable. `FScene::GatherPrimitives` assigns only what is not Static to the cells
  again, so the body kept the cells it spawned in and the portals culled it from the others. Its pose then froze (a body
  that is not drawn is not evaluated) while the Movable weapon was still drawn. `ACharacter` now makes its mesh Movable,
  as UE does, and ShooterGame does the same for its arms (`Mesh1P`) and its camera. A Static skeletal mesh in a map with
  cells is now an `ensure`. `System.Renderer.GS.Scene.CellsAndPortals` walks a pawn from one room into the other: the
  view that culls it in the far room draws it once it is in the near one. The bot match is unchanged, and PCSX2 still
  runs at 29.95 fps, p50 / p95 / p99 33.5 ms (Budgets.md, the row «ps2-polish P1»).

## [0.24.0] - 2026-09-29

Real content, animation and CS parity ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N21 to N31, with N24b and N30a to
N30f): glTF skins and animations (FBX gone), vertex lighting baked by LeonCook, the PS2's cook with hard budgets and a
bootable ISO, asynchronous disc IO, the memory card and the DualShock 2, the animation runtime, the art pipeline and
ShooterGame's characters, arms, weapons and de_leon made by Blender scripts, 30 fps with that art, and CS 1.6's
weapons, grenades, movement, rounds, bots and physical materials. The engine and ShooterGame content is resaved for
0.24.0. PCSX2 ([Budgets.md](Engine/Platforms/PS2/Documentation/Budgets.md), the row «0.24.0»): 29.95 fps,
p50 / p95 / p99 33.5 ms, the scene 8.4 ms (the N1 baseline: 25.4 fps on the blockout map). The ISO (7 122 944 bytes)
boots in PCSX2 and reaches its first frame at 2.93 s with the pak in its open order.

### Added

- The static lighting baked into the vertices ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N22, decision D5): LeonEd's
  `FStaticLightingSystem` (UE: Lightmass) bakes, for every vertex of every Static mesh of a map, the sky
  (`AWorldSettings::LightmassSettings`, UE's `FLightmassWorldInfoSettings`: `EnvironmentColor` × `EnvironmentIntensity`
  times the hemisphere's share, cosine-weighted, that no geometry occludes within `MaxOcclusionDistance`: 64 fixed rays)
  and every light that is not Movable (the imported KHR_lights_punctual ones, each shadowed by a ray against the static
  geometry's triangles in PhysicsCore's `FAabbTree`), one thread, fixed seed: the same bytes every run. The map import
  bakes as its last step, `ResavePackages -buildlighting` (UE's switch) bakes a map again, and the cook warns about a map
  whose lighting needs rebuilding. The result is each component's own colour streams for its mesh's LPS2 v2 batches
  (`UStaticMeshComponent::BakedVertexColors`, `FLPS2ColorStreams`: the mesh's colour streams' layout, made for the mesh's
  CRC), saved in the map (`VER_LEON_BAKED_VERTEX_COLORS`, the oldest loadable version: every package saved again). A
  Static mesh draws its baked colours times its albedo with no light computed per frame; Movable ones (the pawns'
  bodies, now Movable, the weapons, the projectiles) keep the per-vertex lighting, with the sky as their ambient
  instead of 0.10 of the albedo. de_leon's floor is a grid of 3 m cells for the shadows (`make_de_leon.py`); de_leon
  bakes 1 562 vertices in under 0.1 s. PCSX2: 29.8 fps, p50 / p95 / p99 33.5 / 33.5 / 40.8 ms; the scene 15.9 ms for 794 triangles, 20.0 µs a triangle against N17's 27.7 (12.0 ms for 432): the floor grid doubles the triangles, the static meshes no longer cost any light. Tests: `System.LeonEd.StaticLighting.ShadowsAndOcclusion`
  and `.Deterministic`, `System.LeonEd.MapFactory.BakesStaticLighting`, `System.Renderer.GS.Scene.StaticLighting`;
  `System.Renderer.GSEmulator.SceneFrame` draws a baked floor (29 pixels beyond a 5-bit step, 20 before) (483 engine
  tests, 63 ShooterGame, TestPAL 156).
- The PS2's cook and a bootable ISO ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N23):
  - Load-in-place textures: a paletted texture's data is the GS's CLUT image (CSM1, alpha 0..0x80) and each level's
    IMAGE transfer payload, quadword-sized and allocated 128-byte aligned (`FByteBulkData::SetPayloadAlignment`); the
    texture cache uploads them with `FGSCommandList::UploadImageInPlace`, so the PS2's DMA chain REFs the texture's own
    bytes (no conversion, no copy; `FPS2RHI::RetireInPlaceImages` when a resident texture goes).
  - Hard budgets that fail the cook: `[/Script/LeonEd.CookSettings]` (`FCookBudgets`: per map the GS VRAM, a RAM
    estimate and the SPU2 RAM; per mesh triangles and bones; per texture size and bits a texel; `-<Key>=` overrides),
    with the reports against them.
  - Incremental cook (`-iterate`, the default; `-full`): `<Project>/Intermediate/CookCache/<Platform>/`, keyed by the
    sources of a package and its imports, the cooker version and the platform (2.4 s full, 0.12 s cached, the same
    bytes).
  - Pak order: `-LogFileOpenOrder` (`FPlatformFileOpenLog`) records the order the game opens its files in (Win64: a
    file in `Saved/Logs`, PS2: the EE log), `LeonPak -order=` / `BuildCookRun -pakorder=` lays the pak out in it.
  - `BuildCookRun -platform=PS2 -iso [-region=NTSC|PAL] [-discserial=]`: `<Project>.iso` made by xorriso (added to the
    PS2 build image) with `SYSTEM.CNF`, the ELF as `SLUS_990.01`, the pak first after them, ISO 9660 names
    (`FPaths::ToIso9660Path`, which the PS2 file layer applies to `cdrom0:` paths); `MeasurePS2 -Iso` boots it. In
    PCSX2 the game mounts its pak from `cdrom0:` and plays the bot match; the first frame 0.90 s after the engine's
    start with the pak in its open order (1.42 s in path order). `LogLaunch: First frame after ...` logs the load time.
  - Tests: `System.LeonEd.Cook.Budgets`, `.Cache`, `.StripConfig`, `System.PakFile.Format.OpenOrder`,
    `System.Core.HAL.PlatformFileOpenLog` (535 engine tests, 70 ShooterGame, TestPAL 161).
- Asynchronous disc IO, the memory card and the DualShock 2 ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N24):
  - **Asynchronous reads** (UE's API): `IPlatformFile::OpenAsyncRead` gives an `IAsyncReadFileHandle` whose
    `SizeRequest` / `ReadRequest` (a priority, the caller's memory or the request's) are `IAsyncReadRequest`s
    (`PollCompletion`, `WaitCompletion`, `Cancel`, `GetReadResults`), served by Core's `FAsyncIOSystem` in priority then
    issue order, in 64 KB chunks, their callbacks on the game thread. The PS2 reads on an EE thread one priority above
    the game's (`PS2AsyncIO`), asleep in each IOP call while the game runs; Win64 reads on the game thread in the queue's
    order (the same run every time). A pak entry reads through the pak's second handle at its offset; `-LogFileOpenOrder`
    notes the asynchronous opens too.
  - **`LoadPackageAsync`** and `ProcessAsyncLoading` / `FlushAsyncLoading` / `CancelAsyncLoading` / `IsAsyncLoading` /
    `GetNumAsyncPackages` (CoreUObject, UE's API): a package's bytes and its imports' come through the asynchronous
    reads and the game thread serializes it with the synchronous loader once they are in (a `LoadPackage` of a package
    on its way takes its bytes). `UGameEngine::Tick` processes it before the world (`[/Script/Engine.Engine]
    AsyncLoadingTimeLimit`, 8 ms on the PS2), `UEngine::LoadMap` flushes after `BeginPlay`, and ShooterGame's game mode
    asks in `InitGame` for everything its weapons, projectiles, pawn, bomb and player controller name, in structs and
    arrays too (101 packages), kept loaded while the map plays; at most 384 KB are read ahead. From the disc (on N30f's
    content, the pak in its open order) no file opens after the first frame: the match's start went from 3 053.6 ms
    (5 622.8 in path order) to 435.0 ms, the first frame from 2.39 to 7.79 s.
  - **Saves**: `USaveGame` and `UGameplayStatics::CreateSaveGameObject`, `SaveGameToMemory`, `LoadGameFromMemory`,
    `SaveGameToSlot`, `LoadGameFromSlot`, `DoesSaveGameExist`, `DeleteGameInSlot` (UE's GVAS header, the tagged
    properties) and `GetLastSaveGameResult` (`ESaveGameResult`), over `IPlatformFeaturesModule`'s `ISaveGameSystem`: the
    desktop's `FGenericSaveGameSystem` (`Saved/SaveGames/<Slot>.sav`) and the PS2's memory card, `FMemoryCardSaveGameSystem`
    over libmc on `mc0:` (the game's folder with `icon.sys` and an icon for the console's browser, a CRC header, a reason
    for a missing, unformatted, full or pulled out card). ShooterGame's options (`UShooterPersistentUser`: aim
    sensitivity, inverted Y, volume, crosshair colour) in the slot `Settings`, set with `SetSensitivity`, `SetInvertY`,
    `SetVolume`, `SetCrosshairColor`; `[MemoryCard]` of `DefaultGame.ini` names the card's folder and title.
  - **The DualShock 2**: `IInputInterface` takes a controller id (the PS2's two ports, the desktop's first two
    gamepads; the viewport sends each to the local player of that id), the pressures come as analog keys
    (`Gamepad_LeftTriggerAxis` / `Gamepad_RightTriggerAxis` and Leon's `Gamepad_*Axis`, `FDualShockPressure`; libpad's
    pressure mode on the PS2), and UE's force feedback drives the motors: `SetForceFeedbackChannelValue(s)`,
    `UForceFeedbackEffect`, `APlayerController::ClientPlayForceFeedback` / `ClientStopForceFeedback` /
    `ProcessForceFeedbackAndHaptics`, `padSetActAlign` / `padSetActDirect` on the PS2 (`FDualShockForceFeedback`,
    `FDualShockActuators`), stopped when a pad is pulled out. ShooterGame buzzes the small motor on each shot and the
    large one when hurt or near an explosion.
  - `FPlatformMisc::LoadIopModule` (PS2: a ROM module once) and `LockIop` / `UnlockIop` (the IOP's file reads, module
    loads and card calls one at a time across the EE's threads); `MeasurePS2 -PakOrder <file>` and
    `-LogFileOpenOrder`; `FrameStats Summary:` gains `first_ms` and `worst_later_ms`, and `-LogFrameTimes` names the
    frames longer than three fields (`Long frame N`); `Measure.ini` gives the measuring data folder its own memory card.
  - Tests: `System.Core.AsyncIO.Order` / `.Completion` / `.Cancel`, `System.PakFile.PlatformFile.AsyncRead`,
    `System.CoreUObject.AsyncLoading.Delegates` / `.Files`, `System.Engine.SaveGame.RoundTrip` / `.MemoryCard`,
    `System.ApplicationCore.DualShock.ForceFeedback` / `.Pressure`, `System.Engine.ForceFeedback.PlayerController`,
    `ShooterGame.Settings.RoundTrip` (572 engine tests, 99 ShooterGame, TestPAL 170 on Win64).
- Faster disc loading and the round's start ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N24b). A read of the emulated
  disc costs about 20 ms before its bytes, and N24 read the preload a few KB at a time: `FAsyncIOSystem` now takes the
  queued read nearest ahead of the last one (UE's sweep) and coalesces the close ones (`CoalesceBytes`), the pak's
  game-thread handle reads 64 KB blocks forward, a PS2 file handle skips the `lseek` it does not need, `UEngine::LoadMap`
  loads the map through the same queue (`LoadPackageAsync` and a flush), and a synchronous load of a package on its way
  raises its read (`FAsyncIOSystem::RaisePriority`). From the disc on N29's content, the pak in its open order: the
  first frame from 7.72 to 2.93 s, the worst frame after it from 384.95 to 50.05 ms, no file opened after it. The
  round's start spent 320 ms asking the file system about every pawn's and weapon's config paths: ShooterGame's
  `LoadShooterObject` / `LoadShooterAsset` resolve what is in memory and keep it. `FlushAsyncLoading:` and `The paks
  until the first frame` log what a load read; `-LogFrameTimes` logs a spike's scopes (`Frame spike:`).
- Windows vibrates: `FXInputForceFeedback` sends UE's force feedback to the Xbox pads through `XInputSetState`
  (`xinput1_4.dll` loaded at run time), the large channels on the heavy motor and the small ones on the light one;
  tests `System.Core.AsyncIO.Coalesce` and `System.ApplicationCore.Windows.XInputForceFeedback` (575 engine tests, 99
  ShooterGame, TestPAL 171 on Win64).
- The art pipeline ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N26, decision D6): `Docs/ART_PIPELINE.md` sets the
  budgets of each asset class (a character 600–1 000 triangles, the first-person arms 400–600, a weapon 100–300 in the
  world and 300–600 in view), two weights and 24 bones a palette, P4 / P8 textures of 64–128 texels and their density,
  the names (`SOCKET_Weapon_R`, `UCX_`, `VIS_` / `PORTAL_`, lights), the CT and T bodies' shared 23-bone skeleton and
  the arms' 11, the third- and first-person clips at 30 fps with their loop flags and notifies, the fixed glTF export
  options, how textures are made, the CC0 rule and the MCP → `make_*.py` workflow.
  `Game/ShooterGame/SourceArt/leon_art.py` holds the shared Blender helpers (the empty scene at 30 fps, images painted
  by code at P4 / P8 sizes, materials, armatures, sockets, weights clamped to two, clips keyed at 30 fps, the export
  with `GLTF_OPTIONS`); `make_team_bodies.py` and `make_de_leon.py` use it and export the same bytes as before.
  `check_art_determinism.py` runs each script twice and compares every `.glb` with the other run and the committed
  one. The gate's samples (`Samples/make_art_samples.py`: a crate with a 2-bone skin, a painted texture and an `Open`
  clip; a mannequin on the shared skeleton with an `Idle` clip) come out identical and import with LeonCook. The
  Blender Lab MCP extension loads in Blender 5.2; N26 used the headless scripts.
- ShooterGame's characters, arms and weapons ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N27), low-poly and textured
  in the style of CS 1.6, every file made by a Blender script with `leon_art` (D6; CC0, the textures painted by code):
  the counter-terrorist (`SK_Body_CT`, 932 triangles: blue-grey camouflage, a vest, a helmet) and the terrorist
  (`SK_Body_T`, 828: a brown jacket, olive trousers, a balaclava, a backpack) on the shared 23-bone `SKEL_Body`
  (`Characters/make_characters.py`), their first-person arms (`SK_Arms_CT` 542, `SK_Arms_T` 588) on the 11-bone
  `SKEL_Arms` (`make_arms.py`), 128 × 128 P8 textures, two weights a vertex, one palette; and a world model (116-260
  triangles, 64 × 64 P4) and a first-person model (308-556, 128 × 64 P8) of the knife, Glock, USP, Desert Eagle,
  AK-47, M4A1, MP5, AWP, HE, flash and smoke grenades and C4 (`Weapons/make_weapons.py`). The third-person clips at 30
  fps (`anim_body.py`: idle, walk and run in four directions with footsteps, crouch idle and walk, the jump's three
  states, three stances' aim offsets at five pitches, fire, reload, throw, plant, defuse, two deaths) and every
  weapon's first-person clips (`anim_arms.py`: idle, draw, fire, reload with `MagOut` / `MagIn`, the grenade's pin and
  throw, the knife's slash, the C4's plant) are posed by code: `leon_art.Rig` / `Pose` (forward kinematics and
  two-bone IK that puts a hand's `Weapon_R` socket on a weapon's grip), `leon_art.MeshBuilder` (lofts, boxes,
  cylinders and chamfered prisms mapped to a texture atlas and weighted as they are made). `ImportList.ini` imports
  them with `BS_Locomotion` (2D), `BS_Crouch`, `AO_Rifle` / `AO_Pistol` / `AO_Grenade` (all measured from the rifle's
  pose), the arms' `BS_<Weapon>_Idle`, and 26 montages; `DefaultGame.ini` names them: ShooterGame plays the skinned
  bodies, the arms, the sockets and the montages. The pistol shows the USP, the rifle the AK-47, the sniper the AWP,
  the grenade the HE; the others wait for N30. `check_art_determinism.py` checks the five scripts (identical twice and
  committed) and fails when one is missing. `S_Footstep` (`make_sounds.py`) for the footsteps.
- `UCharacterAnimInstance::SetCrouchBlendSpace` / `SetCrouched`: the crouched locomotion, crossfaded, its notifies only
  while it plays; `AShooterCharacter`'s jump clips (`JumpStartAnimName`, ...), crouch, death montages (on its back
  when shot from the front, on its front from behind, held by a looping last section), the drawn weapon's aim offset
  (`AShooterWeapon::AimOffsetName`, none while planting, defusing or dead), the arms' idle (`ArmsIdleName`) and the
  first-person model (`FirstPersonMeshName`); the team's arms (`CTArmsMeshName` / `TArmsMeshName`) follow the team,
  the halftime included. `UGLTFImportFactory::NewSkeletonName` names the skeleton the first of several meshes makes.
- Tests: `System.MeshUtilities.GltfSkeletal.AnimationLoopFlag`, `System.Engine.Animation.CharacterAnimInstance.Crouch`,
  `ShooterGame.Animation.CharacterArt` (the content's wiring, the arms' hold, the crouch, a death that holds), and
  checks of the UV convention, the shared skeleton and the halftime's meshes (537 engine tests, 76 ShooterGame);
  N30c's `ShooterGame.Movement.Footsteps` gives its test locomotion to the crouch too.
- The PS2 with the characters (`MeasurePS2 -Label N27`, Budgets.md): 10.8 fps, p50 / p95 83.5 / 150.3 ms, 2 240
  triangles a frame (the scene 66.7 ms of C++ skinning and transform on the EE); `[Core.MemoryBudgets] EngineMisc`
  3 072 KB (the frame's GS containers peaked at 2 290 KB).
- de_leon rebuilt ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N28): a 60 × 48 m desert town in the style of CS 1.6's
  de_dust, made by `Maps/make_de_leon.py` with `leon_art` (D6; CC0, no external art): sandstone walls and houses with
  darker caps, the arch into mid, the gate on A long, the tunnel on B long (roofed, two lamps), the mid doors with
  their wooden wings open, crates, the sites' letters on the site walls and two ladders to the north blocks' roofs
  (N30c's `Ladder` volumes). Seven textures painted texel by texel (sandstone P8; trim, sand, paving, wood, the crate
  and the signs P4; 64 texels a metre): 66 KB of VRAM. 78 pieces, 2 244 triangles in seven cells (`TSpawn` 446, `Mid`
  160, `LongA` 398, `LongB` 458, `CTSpawn` 234, `SiteA` 266, `SiteB` 282; the script fails a cell over ART_PIPELINE's
  1 500), each piece its own mesh with one `UCX_` box, its faces cut on a 3 m world grid (and 1.2 m up the walls) for
  the bake: the sun and three point lights, 3 471 vertices. `COL_Ground`, one box under the map, is the ground the
  players walk on. `VIS_<Cell>` (7 boxes) and `PORTAL_<A>_<B>` (24 quads) are N15's cells: facing a wall only its cell draws,
  across the open map most of them. `-LogFrameTimes`' `Frame work` line counts the cells seen and the objects they left
  out.
  The sites, buy zones and starts are where they were; 22 waypoints (38 more links from the import); the script
  checks that the hand links, the starts and the sites' middles are clear. BotMatch 10 7: `10 round(s), CT 5 - T 5,
  65 kill(s)`; over seeds 1 to 24 the terrorists win 61 % of the rounds (the old blockout: 59 %). PS2 (`MeasurePS2 -Label N28`, on N15): 8.76 fps
  (13.76 with the blockout), the scene 83.4 ms (46.7), 277 KB of GIF a frame from the EE (87); the map's cook 309 KB
  of its own, the pak 1 829 771 bytes.
- `ViewFrom` (ShooterGame's debug camera) keeps its view after the round's spawn until `ViewPawn`, which removes the
  camera: `-ExecCmds="ViewFrom <X> <Y> <Z> <Pitch> <Yaw>" -Screenshot=<file.bmp>` captures a view of the map.
- ShooterGame's arsenal and economy at CS 1.6's ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N30a). **Weapons** with
  CS's damage, range modifier, armor ratio, cycle, clip and reserve, price, speed and penetration: the knife
  (`AShooterWeapon_Knife`, slot 3: a slash 15, a stab 65, a stab in the back x3), the Glock-18 (the terrorists' first
  pistol, a three-round burst on the secondary button), the USP (the counter-terrorists', a silencer: 30 damage,
  quieter), the Desert Eagle, the MP5, the AK-47 (terrorists only), the M4A1 (counter-terrorists only, a silencer) and
  the AWP; their N27 models and animations, the pistols sharing the pistol's clips and the MP5 and the rifles the
  rifle's (a reload lasts CS's time, its montage fitted to it). **Economy**: a bought weapon comes with its clip only,
  the reserve bought by the box per calibre (`primammo` / `secammo`, the `,` and `.` keys, CS's `buyammo1` /
  `buyammo2`); the first pistol has two clips more; the buy menu has CS's pages (Pistols, SMGs, Rifles, the ammo,
  Equipment), listing the team's weapons. **Wall penetration** (CS's `FireBullets3`): a bullet goes through up to
  `PenetrationCount - 1` things, leaving a surface within its penetration power cut by the material (concrete a
  quarter, metal 15 %) with the material's share of the damage (wood 0.6, metal 0.2, the rest 0.5), and through a
  body with three quarters; the surfaces come from the hit mesh's material (`SurfaceMaterials` in DefaultGame.ini:
  de_leon's crates wood, the walls concrete). **Hit groups**: the head (x4), the chest and the arms (x1), the stomach
  (x1.25) and the legs (x0.75) on height bands and sides of the capsule, armor covering all but the legs (the head
  with a helmet). The bots buy their team's rifle and its ammunition, and a helmet for the kevlar they kept. Tests:
  `ShooterGame.Arsenal.StatsTable`, `.SilencerAndBurst`, `.KnifeBackstab`, `ShooterGame.Weapons.Penetration`,
  `ShooterGame.Damage.HitGroups`, `ShooterGame.Buy.AmmoAndPrices`, `.TeamRestrictions` (83 ShooterGame tests).
- ShooterGame's flashbang and smoke grenade ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N30b). The grenade slot holds
  one weapon of each grenade with CS 1.6's limits (two flashbangs at $200, one HE and one smoke at $300; a purchase of
  one carried adds a grenade), and its key cycles them. **Flashbang** (`AShooterProjectile_Flashbang`, CS's
  `RadiusFlash`): the players whose eyes it sees within 1500 units are flashed, the white held and faded by the
  strength (4 falling to 0 with the distance) and the view's angle (looking at it 1.5 / 3 x the strength, aside 0.45 /
  1.75, behind 0.2 / 1), drawn by the HUD as one full-screen alpha tile; a bot is blind a third of the fade (its sensing
  sees nobody, it stands still). **Smoke grenade** (`AShooterProjectile_Smoke`, `AShooterSmokeCloud`): an 18 s cloud,
  a 325 cm sphere that hides what is behind it from the bots' line of sight while thick, drawn as six grey puffs facing
  the camera (12 triangles). `ThrowGrenade` throws the drawn grenade for captures. Tests:
  `ShooterGame.Grenades.FlashIntensity`, `.FlashBlindsBots`, `.SmokeBlocksSight`, `.CarryLimits` (87 ShooterGame
  tests).
- The world's effect sprites (N30b): `FEffectSprite` / `FEffectSpritePool` (`UWorld::EffectSprites`, 32 at most, the
  oldest recycled), `UGameplayStatics::SpawnEffectSprite`: soft round sprites square to the view, fading in and out,
  drawn by the GS scene renderer after the translucent meshes, farthest first, two alpha-blended triangles each with
  the effects' mask (UE: a sprite emitter). `UPawnSensingComponent::HasLineOfSightTo` is virtual (UE's). Test:
  `System.Renderer.Effects.EffectSprites` (542 engine tests).
- ShooterGame's movement at CS 1.6's ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N30c; `UShooterCharacterMovement`).
  **Fall damage**: a landing faster than `SafeFallSpeed` (580 u/s, 1473 cm/s) takes `(speed - 1473) x 100 / (2601 -
  1473)` health (CS's `DAMAGE_FOR_FALL_SPEED`, all of it at 1024 u/s), as the world's damage: no armor, no tagging, the
  kill feed's `(world)`. **Ladders**: a trigger volume tagged `Ladder` (a map's `Ladder*` node) is climbed in a custom
  movement mode as CS's `PM_LadderMove`: no gravity, forward along the view climbs at 200 u/s (508 cm/s) looking at the
  ladder and down looking down, Jump pushes off at 270 u/s; the bots walk through ladders. **Jump stamina** (CS's
  `fuser2`): a jump costs 1.3158 s, and on the floor each 10 ms of it scales the horizontal velocity by `1 - stamina x
  0.19`, so a landing loses about 40 % of the speed and bunny hopping gains nothing. **Tagging** (CS's
  `m_flVelocityModifier`): a shot halves the victim's velocity and top speed, back linearly in 1 s. **Footsteps**: the
  body's `Footstep_L` / `Footstep_R` notifies play the footstep and make a noise the bots hear
  (`FootstepNoiseLoudness`), only above CS's 150 u/s, so walking and crouching are silent. On a ladder the weapons
  have their air spread. Tests: `ShooterGame.Movement.FallDamage`, `.Ladder`, `.JumpStamina`, `.Tagging`,
  `.Footsteps` (75 ShooterGame tests).
- `EMovementMode::Custom` and `UCharacterMovementComponent::PhysCustom` (UE: `MOVE_Custom`; a game's own modes, with
  `ACharacter::GetCustomMovementMode` and `SetMovementMode(Mode, CustomMode)`), the component's
  `SafeMoveUpdatedComponent` for them, and `ACharacter::Landed` (the fall's speed still in `GetVelocityZ`) and
  `OnJumped`; `ACharacter::Jump` is virtual (UE).
- ShooterGame's rounds and HUD at CS's ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N30d). **Halftime**
  (`AShooterGameMode::bHalftime`, `mp_halftime`; `mp_maxrounds` sets `MaxRounds`): after round `MaxRounds / 2` every
  player, the bots too, moves to the other team; the scores follow the teams (`AShooterGameState::BeginSecondHalf`,
  `IsSecondHalf`, `GetHalftimeRound`), the money goes back to `StartMoney`, the loss streaks to none, and every pawn
  respawns on the new side's starts with the pistol; the match still ends at the majority or after `MaxRounds`.
  `FShooterMatchChecker` checks the swap (the round, every player on the other team, the scores swapped, no money
  kept) and that it happens. **Radar** (top left, `RadarSize`, `RadarRange`): the teammates, for the terrorists the
  carrier or the dropped or planted bomb, the sites' letters, turning with the view; about 10 rectangles, lines and
  letters, no allocation. **Damage direction**: an arc toward the last damage's source (the shooter, the grenade, the
  bomb) that narrows and darkens within `DamageIndicatorDuration` (1 s). **Death cam and spectating**
  (`AShooterPlayerController::StartDeathCam`, `DeathCamDuration` 2 s): from the corpse's eyes, the spectator held
  there, looking at the killer; then the living teammates through their eyes (Fire or `ViewNextPlayer` the next, the
  right button or `ViewPrevPlayer` the one before, Jump the free look), the next one when the watched one dies, the
  free look with nobody left; the HUD names the killer or the watched player. `-BotMatchSpectate` keeps following
  anybody alive. Tests: `ShooterGame.Rounds.HalftimeSwitchesSides`, `.MatchEndsAtTheMajority`,
  `ShooterGame.Spectate.DeathCamThenTeammates`, `.CyclingSkipsTheDead`, `ShooterGame.HUD.Radar`, `.DamageIndicator`
  (70 ShooterGame tests); `ShooterGame.Bots.MatchOnDeLeon` crosses a halftime.
- ShooterGame's bots at CS 1.6's ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N30e; `AShooterAIController`).
  **Economy**: each team decides its buy plan when a round starts (`AShooterGameMode::GetTeamBuyPlan`,
  `ChooseBuyPlan`, `EShooterBuyPlan`): the pistol round, a full buy when half of it can have its rifle and kevlar with a
  helmet, else a force-buy after two losses in a row (`ForceBuyLossStreak`), after a win or in a half's last round, and
  an eco otherwise, where only a rich bot buys; a force-buy takes the rifle, else an MP5, else a Desert Eagle, each with
  kevlar; the money left buys the grenades of `GrenadeBuyOrder` within CS's limits. **Grenades**: a terrorist nearing
  the round's site flashes (else smokes) it, a counter-terrorist nearing the planted bomb flashes it, and a spot where
  an enemy was seen, heard or reported 9 to 22 m away gets the HE (else a flashbang), by draws from the bot's stream;
  the throw's low arc (`ComputeThrowPitch`), up to 1 m off, never into a wall at the bot's nose, and its back to its
  own flashbang. **Fighting**: the bots strafe left and right at the walk key's speed (0.4 to 1 s each way, from their
  stream), crouch and stop with a rifle at 15 m or farther, and stand still with the AWP; a flashed bot fires at random
  around where it last saw its enemy instead of standing still. **Radio** (`AShooterGameMode::SendRadioMessage`,
  `EShooterRadioMessage`: CS 1.6's three menus and "Fire in the hole!", "Bomb has been planted."): a message a player
  every 1.5 s, 60 a round, to its team only; the bots say "Enemy spotted." (with the place, which a free teammate within
  30 m goes to look at), "Need backup." (the nearest teammate answers "Affirmative." and comes), "Sector clear.", "Bomb
  has been planted." and "Fire in the hole!", never what a teammate said in the last 3 s; the HUD shows the team's
  last messages above the money in the team's colour (`RadioMessageDuration`); the player opens the menus with Z, X
  and C (`radio1`..`radio3`) and sends with the number keys. **Names**: CS's BotProfile names (`BotNames`: Albert,
  Allen, Bert, ...) in the order the bots join, team-neutral since a bot keeps its name across the halftime; a bot's
  stream is seeded from its index (`AShooterAIController::SetBotIndex`), not its name. Tests:
  `ShooterGame.Bots.EcoAndForceBuy`, `.BuysGrenadesWithinLimits`, `.ThrowsGrenades`, `.StrafeCrouchAndStand`,
  `ShooterGame.Radio.BotsReportEvents`, `.SectorClear`, `.PlayerMenu` (94 ShooterGame tests).
- Physical materials ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N30f, UE's): `UPhysicalMaterial` (PhysicsCore, `PM_`
  assets) with its `SurfaceType` (`EPhysicalSurface`: the default and `SurfaceType1..62`, named in
  `[/Script/Engine.PhysicsSettings] +PhysicalSurfaces=`, `UPhysicsSettings`) and `DetermineSurfaceType`;
  `UMaterial::PhysMaterial` (`UMaterialInterface::GetPhysicalMaterial`); `FCollisionQueryParams::bReturnPhysicalMaterial`
  and `FHitResult::PhysMaterial` / `FaceIndex`: a hit on a triangle has its section's material's (the collision
  triangles' `MaterialIndices`, `UPrimitiveComponent::GetMaterialFromCollisionFaceIndex`), a hit on a simple shape the
  first material's. The glTF import reads a material's `physMaterial` extras and sets it on the `M_` it makes or finds
  (`leon_art.make_material(..., surface=)` writes them); `UPhysicalMaterialFactoryNew` makes a `PM_` from an ImportList
  section (`Type=PhysicalMaterial`, `SurfaceType=`). Tests: `System.Engine.PhysicalMaterial.TraceSurface`,
  `.SurfaceNames`, `System.LeonEd.Factories.PhysicalMaterials` (561 engine tests).
- ShooterGame's surfaces (N30f): `PM_Concrete`, `PM_Dirt`, `PM_Metal`, `PM_Wood`, `PM_Tile`, `PM_Glass`,
  `PM_Computer`, `PM_Flesh` (CS's texture types, `SHOOTER_SURFACE_*` as UE ShooterGame names them) on de_leon's
  materials (sandstone, trim and signs concrete, sand dirt, paving tile, planks and crates wood, lamps metal), the
  weapons' (metal; the C4 a computer) and the characters' (flesh). By the surface: the footsteps (CS's `pl_step`,
  `pl_dirt`, `pl_tile`, `pl_metal`, a wooden knock; a left and a right foot, `FootstepSounds`), a ladder's steps every
  0.35 s (`pl_ladder`, `LadderStepSoundNames`), the bullets' impact sounds (`ImpactSounds`: debris, a ricochet on
  metal, a shatter on glass) and marks (their tint and size), and CS's `bhit_flesh` / `bhit_kevlar` / `bhit_helmet` on
  a character; the penetration adds CS's tile and computer. The radio's sounds: a squelched tone pattern a menu, and
  "Fire in the hole!" and "Bomb has been planted." their own, to the team's local players
  (`AShooterPlayerController::HearRadio`). 30 sounds more from `make_sounds.py` (CC0). Tests:
  `ShooterGame.Surfaces.Footsteps`, `.LadderSteps`, `.Impacts`, `ShooterGame.Radio.Sounds` (98 ShooterGame tests).

### Changed

- The engine version is 0.24.0 (`Engine/Build/Build.version`; [ps2-shipping](Docs/PLANS/ps2-shipping.md) N31): the
  engine and ShooterGame content was saved again and reimported (the same bytes a second time), and the import
  identity's `SM_Cube` is `A24A1887…0ED1FC`. The documentation matches the code at this release (the architecture's
  frame, the formats, the tests, the build, the tools, the levels, the art pipeline, the coding standard, the READMEs
  and Budgets.md's final table). `CheckBannedApis.ps1` rejects what N14b and N24b removed (the posed skinned streams,
  `bQuantizedPose`, ShooterGame's `LoadOptionalAsset`), as D10 asks.
- glTF skinned meshes and animations ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N21). `UGLTFImportFactory` imports
  `ImportType=SkeletalMesh` (`-type=SkeletalMesh`: `SK_` on `Skeleton=` or a `SKEL_` next to it) and
  `ImportType=Animation` (`-type=Animation`: an `A_<Animation>` per glTF animation on `Skeleton=`, each recording its
  `AnimationName` for the reimport), through MeshUtilities' `LoadSkeletalMeshFromGltf` / `LoadAnimSequencesFromGltf`:
  the skin's joints (parents first; a joint's non-joint ancestors folded into its reference pose), `inverseBindMatrices`,
  `JOINTS_n` / `WEIGHTS_n` normalized and reduced to the two largest weights, renormalized to 1/255 steps
  (`FSkinWeightInfo`: the PS2's budget); `SOCKET_` nodes under joints as the skeleton's sockets; TRS channels with
  LINEAR or STEP keys sampled at 30 Hz (CUBICSPLINE fails with an error); a mismatched skeleton (names, order, parents)
  fails. Images embedded in a `.glb` or a `data:` URI become `T_` textures (static meshes too).
  **Animation keys** are local-space tracks (`FCompressedAnimSequence`, AnimationCore): 48-bit smallest-three rotations,
  `int16` translations with a per-track scale and bias (floats when the range would break the tolerance), scales only
  where they are not 1, each key with a `uint16` frame, reduced to the keys the runtime's interpolation cannot rebuild
  within `[/Script/Engine.AnimationSettings]` (0.1 degrees, 0.05 cm, 0.001): 8 bytes a rotation or translation key
  where the matrices took 64 a bone a frame. The anim instances sample local transforms and blend them in local space
  (`FAnimationRuntime`); `USkeletalMeshComponent` builds the component-space matrices once per update and reads its
  sockets (by bone index), skin matrices and pose bounds from that cache. **Render data**: `USkeletalMesh` is a skinned
  LPS2 v2 blob (a header flag, a 24-bone palette and a skin stream a batch, 48 vertices a batch, batches split by
  palette), which the C++ emitter skins batch by batch; skeletal meshes are culled by their pose's bounds.
  `VER_LEON_SKELETAL_LPS2_ANIM_TRACKS` (4) is the oldest loadable package version: the content (no skeletal assets) was
  saved again. Fixtures: `Cube.glb` (the import identity, replacing `Cube.obj`) and `SkinnedArm.glb`, written by
  `MakeSkinnedFixture.py`. Tests: `System.AnimationCore.*` (5 new), `System.MeshUtilities.GltfSkeletal.*` (4),
  `System.MeshUtilities.LPS2.SkinnedBatchesAndPalettes`, `System.LeonEd.Factories.GltfSkeletalMeshAndAnimations`,
  `System.LeonEd.Commandlets.GltfSkeletalDeterministic`, `System.Engine.Components.SkeletalMeshPoseAndSockets`,
  `System.Renderer.GS.Scene.SkeletalMeshCulledByPose` (489 engine tests).
- The animation runtime ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N25). **Blend spaces**: `UBlendSpace` (2D, e.g.
  speed by direction; its samples Delaunay-triangulated on the normalized axes, `FBlendSpaceTriangulation`,
  barycentric weights inside, the nearest edge outside) next to `UBlendSpace1D`, both through
  `UBlendSpaceBase::GetSamplesFromBlendInput` (at most three `FBlendSampleData`, no allocation) and played synchronized
  (one normalized time, UE's length-based sync). **Notifies**: `FAnimNotifyEvent` on `UAnimSequenceBase::Notifies`,
  authored in the glTF animation's `extras` (`{"notifies": [{"name", "time"}]}`, imported by
  `LoadAnimSequencesFromGltf`), `UAnimNotify` objects; each fires once per crossing (a loop's wrap and several in one
  update included, none for an update of no time) after the anim update, to `UAnimNotify::Notify` and
  `UAnimInstance::OnAnimNotify`. **Montages**: `UAnimMontage` (one slot, one clip, sections, blend in / out, play rate;
  `Montage_Play` / `Stop` / `JumpToSection` / `SetNextSection` / `IsPlaying`, `OnMontageEnded`, `OnMontageBlendingOut`,
  `ACharacter::PlayAnimMontage` / `StopAnimMontage`) on `DefaultSlot` or `UpperBody`. **Layers**: the upper body from a
  branch bone as a layered blend per bone, and `UAimOffsetBlendSpace1D` (3 to 5 poses by pitch) applied as an additive
  on it (`FAnimationRuntime::BlendPosesTogether`, `BlendPosesPerBoneFilter`, `FillBranchBoneWeights`,
  `ConvertPoseToAdditive`, `AccumulateAdditivePose`, on views of poses on the frame's stack). **Pose cache and
  throttling**: `USkeletalMeshComponent` evaluates at most once per update with every temporary on `FMemStack`, sends
  the skin matrices only after a new evaluation, and may skip the evaluation while the anim instance keeps updating:
  `VisibilityBasedAnimTickOption` (`AlwaysTickPose`: not drawn in the last 0.2 s, `UPrimitiveComponent::LastRenderTime`,
  which the renderer stamps) and `bEnableUpdateRateOptimizations` (every 1 to `MaxUpdateRate` frames by the distance to
  `UWorld::ViewLocationsRenderedLastFrame`, `UpdateRateDistanceStep`, `[/Script/Engine.AnimationSettings]`);
  `GetNumPoseEvaluations` measures it. **View models**: skinned view models (first-person arms) in the view model pass
  (`FScene::GatherSkeletalMeshes`, owner flags). **Assets without a source**: ImportList.ini sections of `Type=BlendSpace`,
  `BlendSpace1D`, `AimOffsetBlendSpace1D` and `AnimMontage` (`UBlendSpaceFactoryNew`, `UBlendSpaceFactory1D`,
  `UAimOffsetBlendSpaceFactory1D`, `UAnimMontageFactory`; repeated `+Key=` values) make `BS_`, `AO_` and `AM_` assets
  from the `A_` clips, the same bytes each time. **ShooterGame** has the paths behind empty config names until N27's
  art: skinned team bodies with `UCharacterAnimInstance` (speed and direction, aim pitch), first-person arms (`Mesh1P`),
  the weapon on the `Grip` socket of both instead of its offsets, the weapons' fire / reload / draw and the bomb's plant /
  defuse montages (`FShooterWeaponAnim`; a reload or a draw lasts its montage), footstep and magazine notifies; the
  static bodies and the botmatch are unchanged (CT 4 - T 6, 75 kills, replayed identically). Cost: 10 characters of 32
  bones with the whole graph, about 70 µs a frame on Win64 (`AnimPerf:`), estimated 0.5 ms a character on the EE
  (ARCHITECTURE.md, animation runtime). Tests: `System.AnimationCore.Runtime.*` (3),
  `System.AnimationCore.BlendSpace.Triangulation`, `System.Engine.Animation.*` (12 new: 2D interpolation, notifies once
  per crossing, the wrap, several in one update, zero length, montage blend timing, interruption, sections, the layered
  blend, the aim offset at its extremes, no heap allocation per frame, the ten-character benchmark),
  `System.Engine.Components.SkeletalMeshPoseCacheReuse`, `.SkeletalMeshUpdateRate`,
  `System.Renderer.GS.Scene.SkinnedViewModelOwnerOnly`, `System.MeshUtilities.GltfSkeletal.AnimationNotifies`,
  `System.LeonEd.Commandlets.AnimationAssetsFromImportList`, `ShooterGame.Animation.SkinnedPawn`. The `SkinnedArm.glb`
  fixture's Wave carries two notifies.
- The PS2 runs at 30 fps with the real art ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N29): `MeasurePS2` on de_leon
  gives 29.81 fps with p50, p95 and p99 at 33.5 ms (N28: 8.76 fps, p95 133.5 ms), the scene 8.4 ms (83.4). The scene's
  profile has its parts (`GS Opaque`, `GS Skinned`, `GS Emitted Batches`, `GS Clipped Batches`, ...) and the run's
  summary its work (`SceneWork:`, `Scene_<key>` in MeasurePS2's CSV). A lit draw takes only the point lights whose range
  reaches its bounds, and VU1's StaticLit and SkinnedLit light with up to two point lights (N.L and the range
  attenuation squared): the characters near the map's lamps no longer fall back to the EE's emitter. de_leon's ground is
  its floor slabs' `UCX_` boxes with their materials (sand dirt, paving tile; `COL_Ground` removed), and a capsule
  walks across the seams (`FPhysScene::ResolveCapsuleSides` and `ACharacter::IsFloorEdgeHit` take a box whose top is at
  the feet for floor). A face whose UVs span more than 14 repeats fails the mesh build with an error naming the asset
  instead of crashing LeonCook, and the cook's RAM estimate is calibrated (`RuntimeBaseKB=3072`). VU1Conformance passes
  84 batches. Tests: `System.Engine.CharacterMovement.WalksAcrossFloorSeams`, `System.MeshUtilities.LPS2.Errors`,
  `ShooterGame.Map.TenPawnsOnDeLeon`.
- A bot match's `-rounds=N` is the match's length (`mp_maxrounds`, [ps2-shipping](Docs/PLANS/ps2-shipping.md) N30d):
  the teams switch sides after round N / 2 and a team with the majority ends it sooner. Its line names the halftime:
  `BotMatch 10 7` logs `Botmatch OK: 7 round(s), CT 1 - T 6, 47 kill(s), seed 7, sides switched after round 5, reasons
  [4,4,4,3,4,3,3]` (the team that started as CT won 6 - 1) and replays it. With CS's movement (N30c: the jump's stamina
  and tagging) it logs `Botmatch OK: 9 round(s), CT 3 - T 6, 65 kill(s), seed 7, sides switched after round 5,
  reasons [4,4,4,3,4,4,4,3,3]` (the team that started as CT won 6 - 3); with CS's weapons (N30a: the M4A1 for the CT,
  the bought ammunition, penetration and hit groups) `Botmatch OK: 9 round(s), CT 3 - T 6, 63 kill(s), seed 7, sides
  switched after round 5, reasons [4,4,4,3,4,2,4,3,3]`; with CS's bots (N30e: the economy, grenades, strafing, blind
  fire, the radio, the streams seeded by the bots' order) `Botmatch OK: 7 round(s), CT 6 - T 1, 45 kill(s), seed 7,
  sides switched after round 5, reasons [3,3,4,3,3,4,4]` (the team that started as T won 6 - 1).
- ShooterGame's fall damage is CS 1.6's multiplayer `FlPlayerFallDamage` (N30e): N30c's `(speed - 580) x 100 / 444`
  in units a second times 1.25 (`UShooterCharacterMovement::FallDamageScale`), so a landing at about 935 u/s (a 13.9 m
  drop) is lethal instead of 1024 u/s (16.6 m).
- ShooterGame's C key is CS 1.6's radio3 (N30e); Left Ctrl (and Circle) crouch. The bots are `Albert`, `Allen`, ...
  instead of `Bot_CT_1`, `Bot_T_1`, ... (`bot_kick <name>` takes them).
- Package version 6, `VER_LEON_COLLISION_MATERIAL_INDICES` ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N30f): a static
  mesh's collision triangles carry their material slots; the oldest loadable version, the content saved again and
  every mesh reimported. A mesh's bulk payload reads and writes with its package's version.
- The audio device (N30f): a sound below the default priority leaves the last 10 effect voices free
  (`FAudioDevice::NumLowPriorityVoices`; ShooterGame's steps and impacts are at 0.5, its radio at 1.5), and a
  spatialized sound too far to be heard takes no voice. Test: `System.AudioMixer.Device.LowPriorityAndInaudible`.

### Removed

- FBX and OBJ ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N21, plan decision D11: glTF is the only mesh, skeletal mesh
  and animation format): `UFbxFactory` (`EFBXImportType`, `MeshTypeToImport`), MeshUtilities' FBX importers
  (`FbxStaticMesh`, `FbxSkeletalImport`, `FbxImportCommon`) and OBJ importer (`ObjImport`: only tests used it), the
  UFBX (vendored) and TinyObjLoader (downloaded by `Setup.bat`) third-party modules, the Z-up import basis
  (`EImportAxes::RightHandedZUp`), the `Cube.obj` fixture and the FBX / OBJ tests; the model-space animation
  (`FSkeletalVertex`, `FSkeletalMeshData`, the matrix keys, `GetBoneWorldMatrices`, `GetBoneModelMatrix`,
  `SetRawAnimationData`, `BuildFromImportData`). `CheckBannedApis.ps1` rejects their names.
- The per-frame lighting of static meshes ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N22): a Static component is
  never lit on the EE any more (its baked colours, or the mesh's own), and the renderer's fixed 0.10 ambient share is
  gone (the world's sky replaces it for what moves).
- The texture cache's runtime conversions ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N23): the palette's reordering
  into a CLUT image and its alpha scaling at each upload, and the copy of every uploaded level into the frame's list
  (`FGSCommandList::GetImageData`, rejected by `CheckBannedApis.ps1`; `GetImage` / `GetNumImages`). Data the target never
  reads leaves the cooked output: the staged ini files' comments and editor sections, and the collision triangles of a
  mesh that collides with its simple shapes (ShooterGame's PS2 cook: 245 409 to 232 480 bytes).
- The single pad's API (N24, D10): `FPS2InputInterface::Get`, `IsPortOpen`, `GetRawButtonMask` and `GetRawSticks` (the
  debug widget that read them went in N2), and `IInputInterface`'s calls without a controller id; the IOP modules
  loaded by hand (`SifLoadModule` of the ROM's SIO2MAN / PADMAN / MCMAN / MCSERV: `FPlatformMisc::LoadIopModule`). In
  `CheckBannedApis.ps1`.
- The two-clip locomotion player ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N25): `UBlendSpace1D::Evaluate` and
  `UAnimInstance::GetBlendAlpha` (and its per-sample times and scratch poses) give way to
  `UBlendSpaceBase::GetSamplesFromBlendInput` and one synchronized player; in `CheckBannedApis.ps1`.
- The box proxies (N27, D10): the static team bodies (`SM_Body_CT` / `SM_Body_T`, their materials, `BodyMesh`,
  `GetBodyMesh`, the static `CTBodyMeshName` / `TBodyMeshName`, `make_team_bodies.py`, `team_bodies.blend`) and the
  box weapons (`SM_Pistol`, `SM_Rifle`, `SM_Sniper`, `SM_Grenade`, the old `SM_C4`, `M_Weapon*`, `M_BombDisplay`, the
  standard-library `make_weapons.py`); `ArmsMeshName` and `CTBodySkeletalMeshName` / `TBodySkeletalMeshName` become
  the team's names. Without a body the weapon sits on the capsule.
- de_leon's blockout ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N28, D10): its shared 1 m cubes and pads
  (`SM_Wall`, `SM_Floor`, `SM_CrateStack`, `SM_PadA`, `SM_PadB`, `SM_PadCT`, `SM_PadT`) and flat materials (`M_Wall`,
  `M_Floor`, `M_SiteA`, `M_SiteB`, `M_SpawnCT`, `M_SpawnT`); `SM_Crate` and `M_Crate` are now the textured crate's.
- ShooterGame's generic weapons (N30a, D10): `AShooterWeapon_Pistol`, `AShooterWeapon_Rifle` and
  `AShooterWeapon_Sniper` give way to CS's (`AShooterWeapon_USP`, `_AK47`, `_AWP` and the rest), the capsule's two hit
  groups (`EShooterHitGroup::Body`) to CS's seven, and the flat buy menu (`GetBuyMenuItems`) to its pages; in
  `CheckBannedApis.ps1`.
- ShooterGame's one grenade (N30b, D10): `AShooterWeapon_Grenade` is `AShooterWeapon_HEGrenade`, beside the flashbang
  and the smoke grenade; in `CheckBannedApis.ps1`.
- ShooterGame's surface table (N30f, D10): `SurfaceMaterials`, `EShooterSurface` and `FShooterSurfaceMaterial` are the
  physical materials (`FHitResult::PhysMaterial`, `SHOOTER_SURFACE_*`); the one footstep (`FootstepSoundName`,
  `S_Footstep`) is the surfaces' (`FootstepSounds`); in `CheckBannedApis.ps1`.

### Fixed

- The glTF import turns v over (N27): glTF's UVs start at the image's top left, the engine keeps a texture's bottom row
  first; since FBX's removal (N21) every glTF texture was drawn upside down (no textured glTF content showed it until
  the characters). de_leon's and AxisTest's meshes were imported again (their UVs change, nothing they draw).
- The glTF animation extras (N27): the import reads `loop` into the clip's `bLoop` (every clip looped: a jump's start,
  a montage's clip wrapped), and `leon_art.add_action` writes the notifies as the list of `{"name", "time"}` (seconds)
  the import reads; N26 wrote a `Name@frame` string that the import skipped silently, and a string now fails it.
- The first-person arms never ticked since N18 (a skeletal mesh component has no tick function of its own; ACharacter
  ticks its body): `AShooterCharacter` ticks `Mesh1P`, so their montages advance and their pose follows; they are
  evaluated only when drawn (a bot's never).
- The weapon socket's name: N25's code used `Grip`, N26's art `Weapon_R`; it is `Weapon_R` everywhere (the character's
  `WeaponSocketName`, the engine's skinned test character).

## [0.23.0] - 2026-09-29

VU1 and native data ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N12 to N20, with N14b): LPS2 v2 meshes drawn by VU1's
static and skinned microprograms, textures resident by blocks with mips, VU0 in macro mode and the scratchpad, LODs,
cells and portals, fog, blob shadows and the canvas as sprites, a collision broadphase, memory arenas and budgets, tick
groups, timers, a fixed 30 Hz step and an incremental garbage collector, the sounds on the SPU2, and ShooterGame's
frame on the EE. PCSX2 ([Budgets.md](Engine/Platforms/PS2/Documentation/Budgets.md), the rows «N17» and «N18»): the
bot match on the blockout map at 29.9 and 29.65 fps, p50 / p95 33.5 / 33.5 ms; N18's fixed step makes its row the first
the later ones compare with. The phases were not done in the milestones' order (N14, N14b and N15 came after the art
of 0.24.0), so some entries name later phases.

### Added

- meshoptimizer 1.2 (MIT), a pinned download wrapped as the `MeshOptimizer` External module: MeshUtilities builds the
  LPS2 v2 meshes with it, at edit time only (no game target links it); its simplifier is there for N15's LODs
  ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N12). Tests: `System.MeshUtilities.LPS2.*` (the strips draw exactly the
  source's triangles with their winding, the batches fit VU1's budget, the quantization error, the same bytes every
  build, the errors), `System.Renderer.GS.Scene.StripsDrawTheSource` and `ShooterGame.Content.MeshQuantization` (437
  engine tests, 57 ShooterGame).
- The scene's features of [ps2-shipping](Docs/PLANS/ps2-shipping.md) N15. **VU0** in macro mode: `FVectorMath`
  (`Math/VectorMath.h`; the PS2's `PS2VectorMath.cpp` issues COP2 from the EE, elsewhere the scalar reference
  `FVectorMathFPU`) multiplies matrices (`FMatrix::operator*`, so the pose's palette products too), transforms vectors
  (`TransformFVector4`) and tests a box or a sphere against planes four at a time (`FVectorPlaneSet`, which `FFrustum`
  keeps); TestPAL compares VU0 with the reference (`System.Core.Math.VectorMathVU0`: at most 2 units in the last place
  of the products' magnitudes in PCSX2). **The scratchpad** (`Misc/Scratchpad.h`: `FScratchpad`, `FScratchpadMark`,
  `TScratchpadAllocator`; the EE's 16 KB at `0x70000000`, a static buffer elsewhere and with `-nospr`) holds the scene
  renderer's frame lists (`FSceneRenderList`) and the emitter's per-batch scratch. **LODs**: a static mesh's
  `SourceModels` (UE's) are simplified from LOD 0 when it is built (`IMeshBuilderModule::SimplifyMesh`,
  `FLPS2MeshBuilder::Simplify`, `meshopt_simplify`) and saved after the collision triangles (a mesh of one LOD keeps
  its bytes); ImportList.ini's `LODs=<share>@<size>,...`; the renderer picks by screen size (`ComputeStaticMeshLOD`,
  `ComputeBoundsScreenSize`, `SceneManagement.h`, UE's) with `[/Script/Engine.RendererSettings]
  StaticMeshLODDistanceScale`. **Cells and portals**: the map importer's `VIS_<Cell>` / `PORTAL_<CellA>_<CellB>` rules
  make `AVisibilityCellVolume` / `AVisibilityPortal`; the scene gathers them into a `FVisibilityCellGraph` (RenderCore)
  and draws only what the cells the view reaches through the portals hold (screen rectangles narrowed portal by
  portal; a map without cells draws everything). **Fog**: `AWorldSettings::FogSettings` (`FWorldFogSettings`, a linear
  distance fog, off by default) is the GS's per-vertex fog (FOGCOL, XYZF2 with FGE; `FGSVertexFog`), in the emitter and
  in VU1's StaticUnlit and StaticLit alike. **Blob shadows**: `UPrimitiveComponent::bCastBlobShadow` (ShooterGame's
  bodies) traces the floor down and the renderer lays a soft dark quad there. **The canvas's rectangles** (tiles,
  glyphs, lines along an axis) go to the GS as SPRITEs (`FCanvas::GetPrimitives`, `ECanvasPrimitive`). The view model
  pass is culled by its own frustum. `FFrameStats` counts the cells seen, what they left out, the blob shadows and the
  meshes drawn at a lower LOD. Tests: `System.Core.Math.VectorMathVU0`, `System.Core.Memory.Scratchpad`,
  `System.RenderCore.VisibilityCells.*`, `System.Renderer.GS.Scene.LODByScreenSize`, `.Fog`, `.BlobShadows`,
  `.CellsAndPortals`, `.ViewModelCulling`, `System.Renderer.GS.Canvas.Sprites`,
  `System.MeshUtilities.LPS2.SimplifiedLODs`, `System.Engine.Assets.StaticMeshLODsRoundTrip`,
  `System.LeonEd.GLTFImport.StaticMeshLODs`, `System.LeonEd.MapFactory.CellsAndPortals` (the fixture
  `CellsFixture.gltf` from `MakeCellsFixture.py`); VU1Conformance draws fogged batches (42 batches, F 0).
- Memory arenas and budgets ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N17): `FMallocBinned` (UE's name) is GMalloc
  on the EE and the PC: 64 size classes 16 bytes apart up to 1 KB, in 4 KB pages of an arena reserved at start-up
  (`FPlatformProperties::SmallBlockArenaSize`: 2 MB on the PS2, 16 MB on Win64), O(1) and deterministic, 16 / 64 / 128
  byte alignment, large blocks from the system heap; live stats (current, peak, live, allocations since start-up, the
  arena's pages, each class). Memory tags (UE's LLM: `LLM_SCOPE(ELLMTag::X)`, `FLowLevelMemTracker`) charge every
  block to a tag, with budgets from `[Core.MemoryBudgets]` (PS2Engine.ini sets the EE's): over 90 % a warning, over
  the budget a fatal error that names the tag. `FMemStack` / `FMemMark` / `TMemStackAllocator` (UE) hold a frame's
  temporaries, emptied by the engine loop, which checks nothing outlives the frame; the collision queries' hits, the
  player input's lists, the canvas, the impact marks and tracers and the path search use it. A package's bytes live in
  the linker's load arena, released once its exports are serialized and returned as a block when the load ends.
  `stat unit`'s RAM is GMalloc's current / peak / budget, `stat memory` (F8) lists the tags, and `-LogFrameTimes` adds
  `heap_kb`, `allocs_per_frame` and `MemoryTags:` (MeasurePS2's CSV). The strip emitter's scratch is on the frame's
  stack too. The headless bot match allocates 9.2 times a frame instead of 25.1 and replays the same; PCSX2: 29.9 fps,
  GMalloc peak 1 527 KB, arena 504 KB of 2 MB, 8 to 10 allocations a frame in a round (Budgets.md). Tests:
  `System.Core.Memory.*` (10) (479 engine tests, TestPAL 156 on Win64, 149 on the EE).

### Changed

- A static mesh's render data is LPS2 v2 on every platform, the shape the PS2's VU1 is to read
  ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N12, D1; [ASSET_FORMATS.md](Docs/ASSET_FORMATS.md#lps2-v2)):
  `FStaticMeshLODResources::RenderData` (`FLPS2Mesh`, RenderCore) is one blob of batches of at most 64 vertices (a
  batch's input and GIF output fit half of VU1's data memory: 3 + 7 x 64 = 451 of 496 quadwords), each a run of
  triangle strips whose vertices that close no triangle carry the GS's ADC; positions as `int16` x 3 with the mesh's
  scale and bias, normals as `int8` x 3 with the strip flags, a baked RGBA8 colour (white until N22), 4.12 texture
  coordinates with a whole offset a batch, a material slot a section, every stream on a quadword for its VIF UNPACK
  (V3-16, V4-8, V4-8, V2-16): 18 bytes a vertex instead of 32. MeshUtilities builds it at import (`FLPS2MeshBuilder`,
  Engine's `IMeshBuilderModule`), the same bytes every time; the float vertices, 32-bit indices and index sections go.
  A drawn position is within half a step of the source's (0.06 cm at most on de_leon, through its actors' scale).
- The GS scene renderer draws a static mesh batch by batch as VU1 will (D8): the batch's sphere against the view's
  planes skips it, sends its strips as a TRISTRIP (only the vertices of the drawn triangles; XYZ3 for those that close
  none) when it is inside the guard band and the near and far planes, or sends its triangles through the clipper. The
  same frame as the source's triangles one by one, in 391 GS writes instead of 946 (the test's scene); SceneFrame is
  unchanged (29 pixels). On the PS2 a scene triangle costs 27.9 µs instead of 33.8 (PCSX2).
- The physics scene collides with the triangles a static mesh saves beside its render data, the source's at full
  precision (`FTriMeshCollisionData`, `UStaticMesh::GetPhysicsTriMeshData`): the bot match is unchanged (CT 8 - T 2).
  Packages are `VER_LEON_LPS2_MESH` (3), the oldest loadable version: the engine and ShooterGame content was saved
  again, and a second reimport gives the same bytes. The PS2 pak is 464 KB (456 KB): the collision triangles and the
  vertices strips share outweigh the smaller vertex. G4 bans the removed arrays.
- The GS texture cache keeps textures resident ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N13): each texture takes a
  run of blocks with its levels at their real alignment and its CLUT (`FGSTextureLayout::GetFootprint`), and when the
  arena is full the least recently used textures of earlier frames are evicted, never the frame's own and never all at
  once. `[/Script/Engine.RendererSettings] TextureUploadBudgetKB` (128) caps a frame's uploads; a texture over it
  draws from its smallest level or a flat colour until the next frames upload it. TEX0 loads a CLUT with CLD 4 / 5
  only when CBP0 / CBP1 changes, and with CLD 2 / 3 when the buffer no longer holds the CLUT a register names. The PS2
  cook makes the P4 / P8 mip chain down to 8 texels, averaged in linear space, through one palette (mips as indices:
  `GetPixelFormatMipDataSize`, `UTexture2D::AddMip`; the package layout already had mips, so no version change); the
  scene renderer samples it trilinear with the GS's LOD from Q (K from the section's texel density, documented in
  ARCHITECTURE; `UMaterial::bMipmaps`, `LodBias`), draws the opaque sections grouped by texture and writes TEX0,
  MIPTBP, TEX1 and CLAMP only when they change. `FFrameStats` and `FrameStats Summary:` add the upload bytes,
  evictions, CLUT loads and resident KB (`tex_upload_kb`, `tex_evictions`, `clut_loads`, `tex_resident_kb`); the
  GSConformance scene ClutAndFormats adds paletted mipmaps and CLD 2 to 4, and SceneFrame draws the cooked texture
  with its mips (20 pixels beyond one 5-bit step). PS2: 29.0 fps, the scene 12.5 ms, 1 518 GS writes and 23.7 KB of
  GIF a frame. Tests: `System.Renderer.GS.TextureCache.Oversubscribed` (3x, never a reset), `.EvictionOrder`,
  `.UploadBudget`, `.ClutLoads`, `System.Renderer.GS.Scene.DrawsGroupedByTexture`,
  `System.TextureCompressor.Paletted.MipChain` (456 engine tests).
- The PS2 draws the static meshes on VU1 ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N14): a LPS2 v2 batch inside
  the guard band is a command of the list (`FGSCommandList::DrawVertexBatch`, `FGSVertexBatch`, `FGSVertexDraw`,
  GSCore), which the PS2 hands to VU1's microprograms StaticUnlit and StaticLit (`PS2RHI/Private/VU1/VU1Programs.vsm`,
  assembled by `dvp-as` in the PS2 build: `LeonPlatform_PS2_ModuleSources`): transform, 1/w, the GS's 12.4 pixels,
  Z, STQ, the baked colour times the material and the ambient and one sun, CLIP and back faces, and an XGKICKed PACKED
  packet (PATH1), double buffered in VU1's memory. The frame is one VIF1 chain (`FGSGifPacket::BuildChain` with
  `IGSVertexBatchEncoder`, `FPS2VU1BatchEncoder`): the EE's writes by DIRECT (PATH2), each batch's header by CNT, its
  streams by REF where the mesh keeps them, MSCAL; FLUSH after batches, EOP before them; PATH3 is not used and the GIF
  is reset at `InitDisplay`. `FGSPrimitiveEmitter` moves to GSCore and is the reference (`AddVertexBatch`,
  `FGSCommandList::AppendExpanded`); batches across the near plane or the guard band stay on the EE's clipper (D8), and
  `-novu1` sends every batch through the emitter; N21's skinned batches are posed on the EE and handed to the same
  programs quantized around their sphere (N14b replaces that with skinning on VU1). A Static component's baked colours (N22) go to VU1 by reference in place of the mesh's colours;
  a mesh's LPS2 blob or an instance's baked colours given up while a frame is in flight wait for that frame
  (`FRHIDeferredRelease`, RHI; the PS2 numbers its frames). `VU1Conformance` (PS2 program) checks the programs against the
  emitter in the same ELF: `PASSED` in PCSX2 (30 batches, XY 0, Z 5, RGBA 1, STQ 3 ulp). PCSX2 on N27: the scene 62.9 → 41.9
  ms a frame against `-novu1`, 11.1 → 15.3 fps (N27's 10.8). `MeasurePS2.bat -ExtraArgs`; `Package.bat` packages
  VU1Conformance; G4 bans `DMA_CHANNEL_GIF` and libpacket2's VIF helpers. Tests: `System.GSCore.GifPacket.ChainBatches`,
  `System.Renderer.GS.Scene.VertexBatches`, `System.Renderer.GS.Scene.SkinnedVertexBatches`, `System.MeshUtilities.LPS2.ReleasedAfterTheFramesInFlight`.
- The PS2 skins on VU1 ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N14b): `Skinned.vsm` (SkinnedLit, SkinnedUnlit)
  takes a skinned LPS2 v2 batch as it is stored, with its skin stream and a palette of its bones' skin matrices
  (`FGSSkinMatrix`, 3 quadwords a bone, in the list's memory: `FGSCommandList::AllocateSkinPalette`), blends each
  vertex's two bone matrices, poses its position and normal and goes on as StaticLit (fog and XYZF2 as N15's). The EE
  builds the palette and places the batch by the sphere its pose keeps it in without skinning a vertex; the batches
  across a clip plane are skinned by the emitter with the same palette. The posed streams of N14 go (D10). The GS
  lists and the DMA chains have their own memory tag, `RenderLists` (4 096 KB on the PS2), and `EngineMisc` is back to
  2 048 KB. VU1Conformance passes 66 batches in PCSX2, 24 of them skinned. PCSX2 on N15's tree: 22.37 fps against
  11.42 with `-novu1` (the scene 21.9 against 59.4 ms; N15's row 13.76); on N28's map 9.78 against 8.84 (N28's 8.76).
- VU1's microprograms send XYZF2 (N15): Z shifted to bit 4 (the screen scale's w, 16), F from the header's quadword 7
  (zw), ADC as 2048 more in the F lane (the limits' w); `FPS2VU1BatchEncoder` writes the draw's fog and PRIM's FGE.
  `FFrustum` keeps its planes as an `FVectorPlaneSet` and gains `IntersectsSphere`.
- The physics scene has a broadphase ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N16): the bodies that do not move are
  in an AABB tree (`FAabbTree`, PhysicsCore: flat nodes and items, no pointers), built when static bodies are added
  (once for a whole bot match), the moving ones (dynamic bodies, movable components such as the character's capsule, a
  static body once it moves) in a list sorted on X (`FPhysSceneBroadphase`), and every triangle mesh has a tree of its
  triangles. The traces, sweeps and overlaps, `QuerySupportZ`, `ResolveCapsuleSides`, `ApplyCapsuleSweepPush` and the
  step test only what it finds, with the same hits in the same order as testing every body (the bodies keep the order
  they were added in); the step no longer pairs static bodies, `*Single*` queries allocate nothing and skip what lies
  beyond the nearest hit, and `UWorld::ResolveCharacterOverlaps` pairs the characters with a sort and sweep on X. A
  component's body is found through its unique id, and a removal moves the last body into its place. The bot match
  gives the same result (CT 8 - T 2, 78 kills) and runs in 1.15 s instead of 1.55 s on Win64; on the PS2 the world
  takes 5.9 ms a frame instead of 8.1. `SegmentAabb`, `SegmentUprightCapsule` and `SegmentFloorZ` move to
  PhysicsCore, `SegmentSlopePlane` next to `FSlopePlane`. Tests: `System.Engine.PhysScene.Broadphase.*` (6, against a
  brute force), `System.PhysicsCore.Triangle.MeshTreeMatchesEveryTriangle` (428 engine tests).
- Tick groups, timers, a fixed step and an incremental garbage collector
  ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N18, D4):
  - `FTickFunction` (UE's API: `PrimaryActorTick`, `PrimaryComponentTick`, `bCanEverTick`, `TickGroup`, `TickInterval`,
    `bStartWithTickEnabled`, `AddPrerequisite`, `SetActorTickEnabled`, `SetComponentTickEnabled`) and the world's
    `FTickTaskManager`: `TG_PrePhysics`, the physics step, `TG_DuringPhysics`, `TG_PostPhysics`, the camera managers,
    `TG_PostUpdateWork`. Each group keeps only its enabled tick functions, in the level's order with an actor's
    components before it, so placed geometry, lights and volumes cost nothing (an actor or a component ticks only when
    its class sets `bCanEverTick`, as in UE); an interval carries its remainder; a pawn and its components wait for
    their controller (`AController::AddPawnTickDependency`). The bot match played the same match as before on the
    tick functions alone.
  - `FTimerManager` (UE's API: `SetTimer` with a UObject's method or a delegate, rate, looping, first delay,
    `SetTimerForNextTick`, `ClearTimer`, `ClearAllTimersForObject`, `IsTimerActive`, `GetTimerRemaining` / `Elapsed` /
    `Rate`), owned by the world and ticked first in each step, on an integer clock (3 MHz: 1/30, 1/25 and 1/60 s are
    whole numbers of units), so a timer fires on the same step every run. `AActor::SetLifeSpan`, the pain volume, the
    pawn sensing's sight updates (a look held back to the next step turns the component's tick on for one step), the flash
    light pool (timers without a delegate: nothing allocated per shot) and `AGameState::ElapsedTime` (UE's `DefaultTimer`) run on it,
    and ShooterGame's round phases, buy time, `mp_restartgame`, bomb (explosion, defuse, beeps), planting, grenade fuse,
    equip and reload (CS's timings unchanged).
  - The world steps at a fixed 1/30 s (D4): `UEngine::UpdateTimeAndHandleMaxTickRate` turns the real time (integer
    microseconds of `FPlatformTime::Cycles64`) into whole steps (`FFixedStepClock`, `[/Script/Engine.Engine]
    FixedStepsPerSecond=30`), at most `MaxStepsPerFrame` (4) a frame, and the render draws between the last two steps:
    the scene proxies keep two steps' transforms (`FPrimitiveSceneProxy::SetStepTransform`,
    `FSceneInterface::InterpolateTransforms`) and the viewport the player camera's two views
    (`APlayerCameraManager::GetInterpolatedView`), never feeding the simulation. PAL draws 25 fps of the same 30 Hz
    steps. A headless run steps the same way (sleeping until its step), `-benchmark` one step a frame unpaced. The
    world's time counts in the timers' units. The same seed now plays the same match headless, in a window or on the
    PS2 whatever the frame rate: `BotMatch 10 7` gives `CT 8 - T 2, 70 kill(s), reasons [4,4,4,3,4,4,4,4,3,4]` (the
    variable step's 60 Hz headless steps gave CT 4 - T 6, 75 kills), and a windowed 2-round match paced at 30 fps the
    same line as the headless one (`CT 2 - T 0, 13 kill(s), reasons [4,4]`).
  - Incremental garbage collection: every 10 s of game (`gc.TimeBetweenPurgingPendingKillObjects`) a collection starts
    and visits `gc.IncrementalObjectsPerStep` (100) objects a step, a count and not a time so it advances the same on
    every machine; its marks are its own (weak pointers and iterators see every object meanwhile), each visit records
    the reference slots it read, and the end, at once, visits again the objects whose slots changed, the new objects,
    the roots and what the classes' `AddReferencedObjects` report, then sweeps and purges
    (`StartIncrementalGarbageCollection`, `IncrementalCollectGarbageStep`, `IsIncrementalReachabilityAnalysisPending`).
    A level load collects fully, and ShooterGame asks for a full collection at each round's start
    (`UEngine::ForceGarbageCollection`); the collector runs after each world step.
  - PS2 (`MeasurePS2 -Label N18`): on N22 and N25: 29.65 fps, 33.7 ms average, p50 / p95 / p99 33.5 /
    33.5 / 50.3 ms, the world 3.1 ms (the timers 0.1 to 1.1 ms with the bots' sight updates), the scene 15.6 ms (762
    triangles: N22's baked floor), GMalloc peak 1 949 KB, 27.1 allocations a frame, 934 UObjects at most. Two runs, and
    builds on N17, N22 and N25 (a render change and an animation runtime in between), all play the same match
    (`CT 0 - T 2, 13 kill(s), reasons [3,3]`): the rows of Budgets.md are comparable from now on. The EE does not play
    Win64's match (`CT 2 - T 0, 13 kill(s), reasons [4,4]`): its floats round otherwise (determinism per platform). A
    full mark of 893 objects costs 2.4 ms on the EE; an incremental collection visits them in 10 steps and ends in
    1.0 ms (purge 0.13 ms); a round's full collection takes 3.1 ms.
  - Tests: `System.Engine.Tick.*` (6), `System.Engine.Timers.*` (5), `System.Engine.FixedStep.*` (2),
    `System.CoreUObject.GarbageCollection.IncrementalMatchesFull`, `.IncrementalMutations`, `.IncrementalWeakPointers`
    (530 engine tests, ShooterGame 64, TestPAL 159).
- The sounds play on the SPU2 ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N19). The new Developer module
  AudioCompressor encodes the SPU2's ADPCM (`FSpuAdpcmEncoder`: 16-byte blocks of 28 samples, each block's filter and
  shift searched against the decoder, the loop flags; mono; resampled by a windowed sinc to the sound's
  `CompressionSampleRate`, 22 050 Hz by default; a loop on a block; the same bytes every run), and AudioMixer decodes it
  (`FSpuAdpcm`). Both target platforms cook every `USoundWave` to it (wave format `SPU2ADPCM`: the rate and loop start
  with the blocks as bulk data, no PCM) and the cook writes `<Platform>-SoundReport.txt`, failing a map whose sounds do
  not fit the SPU2 RAM; the desktop's editor builds make the same ADPCM from an uncooked sound's PCM. `FAudioDevice`
  runs the SPU2's model on every platform: the buffers resident in its RAM from the sound's load (shared by path,
  counted, freed from the top as audsrv allocates), 24 hardware voices (2 for music, the last 4 free effect voices for
  sounds above the default `USoundBase::Priority`; ShooterGame's bomb sounds have 2), plays started by its tick after
  the world, each voice's volume and pan from the listener in audsrv's steps, sent only when they change. The PS2's
  `FAudioHardware` is audsrv's ADPCM calls (`audsrv_load_adpcm`, `audsrv_ch_play_adpcm`,
  `audsrv_adpcm_set_volume_and_pan`); the desktop's decodes the buffers and mixes the voices with the SPU2's pitch and
  audsrv's levels into miniaudio, so it sounds like the PS2. In PCSX2 the EE's audio falls from 2.1 to 0.05 ms a
  frame, the cooked sounds from 242 to 68 KB and the PS2 pak from 456 to 292 KB (Budgets.md). Tests:
  `System.AudioCompressor.SpuAdpcm.*` (4), `System.AudioMixer.SpuAdpcm.*` (3), `System.AudioMixer.Device.*` (4),
  `System.AudioMixer.SoftwareMixer.SpuPitch`, `System.LeonEd.Cook.SpuAdpcmSounds` (454 engine tests).
- ShooterGame's frame on the EE ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N20): the game mode keeps registries of
  what the game looked up by walking the level's actors for every bot every frame (the map's bomb sites, buy zones and
  team starts, found once and sorted once; the pawns, the pickups, the bombs and the grenades, from their BeginPlay to
  their EndPlay) and counts the living once a frame. The bots trace only to living enemies
  (`UShooterPawnSensingComponent`, on UE's `UPawnSensingComponent::ShouldCheckVisibilityOf`,
  `ShouldCheckAudibilityOf`, `OnTimer` and `SetTimer`, new in the engine) and take turns to look: 10 Hz each as before,
  spread over the 0.1 s, at most `MaxSensingUpdatesPerFrame` (2) a frame, the rest queued in turn. A noise reaches the
  registered sensing components instead of every actor's components. The muzzle flashes and explosions reuse a pool
  of eight lights (`UWorld::AcquirePooledPointLight`; `UWorld::AddOnActorSpawnedHandler` is new too); a sound plays
  from its samples in place (`USoundWave::LockPCM` / `UnlockPCM` replace `GetPCMView`'s copy, which G4 now rejects);
  the weapon classes are listed once; the HUD and the buy menu format a line only when what it shows changes. The bot
  match with seed 7 goes from CT 8 - T 2 (78 kills) to CT 4 - T 6 (75 kills): only the moment of each bot's look moved
  (with every bot looking in the same frame, as before, the match is the same); its UObjects' peak falls from 2 557 to
  1 177. 36 000 headless frames take 1.8 s instead of 2.7 s on Win64, and
  in PCSX2 the world takes 3.9 ms a frame instead of 9.0 (27.5 fps; Budgets.md). Tests:
  `ShooterGame.Registry.MapOnDeLeon`, `.PickupsAndPawns`, `ShooterGame.Bots.SensingFilter`, `.SensingStagger`,
  `ShooterGame.Effects.MuzzleFlashPool`, `ShooterGame.HUD.TextCache` (62 ShooterGame tests).

### Removed

- `FCanvas::GetTriangles` (N15, D10): `FCanvas::GetPrimitives` gives the rectangles as the GS's sprites;
  `FScene::GatherStaticMeshes` / `GatherSkeletalMeshes` are `GatherPrimitives` (with the view's cells); both in
  `CheckBannedApis.ps1`.
- `FMallocAnsi` and `HAL/MallocAnsi.h`, with their `PLATFORM_WINDOWS` test (G4 rejects them; `FMallocBinned` replaces
  them), and PS2Engine.ini's unread `[/Script/PS2RHI.PS2Settings]` (the RAM line's budget is `[Core.MemoryBudgets]`
  `Total`) ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N17).
- The variable step ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N18): the real frame time clamped to 0.1 s through
  `FPlatformTime::Seconds`' doubles, the headless `-tick=<Hz>` pacing (`TickHz`, `NextHeadlessTick`), the world's walk
  of every actor (`AActor::bCanEverTick`, `TickActor` ticking every component), the tick-counted life span, pain,
  sensing and light pool times, the game mode ticking its game state and the periodic full garbage collection;
  `CheckBannedApis.ps1` rejects them.
- The PCM audio path ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N19): the PS2's software mix and audsrv's PCM stream
  (`PS2AudioOutput`), `FSoundWavePCM`, `USoundWave::LockPCM` / `UnlockPCM`, the UI cues' procedural tones (`UiTone`;
  a cue without a sound wave is silent) and the uncooked PCM in cooked sounds; `CheckBannedApis.ps1` rejects them.

## [0.22.0] - 2026-09-29

A measured and clean base ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N0 to N11, after
[ps2-preview](Docs/PLANS/ps2-preview.md)'s PC under the PS2's conditions): the PS2's frame measured in PCSX2 unattended
(`MeasurePS2.bat`, `RunGates.bat`), ThirdPerson, the PS2's immediate path, Linux and Jolt removed, the dead data and
the confirmed bugs gone, the GS local memory's real layout and the emulator at the reference's parity, a cycle profiler
on every platform, the vertical blank by interrupt and the frame sent as double-buffered DMA chains. PCSX2
([Budgets.md](Engine/Platforms/PS2/Documentation/Budgets.md), the rows «N1 baseline» to «N11»): 25.4 fps at the
baseline, 25.7 fps (p50 / p95 33.5 / 50.3 ms) at N11; the rows before N18 play a different match each (a variable
step), so they compare by part, not by fps.

### Added

- PS2 budgets on the PC ([ps2-preview](Docs/PLANS/ps2-preview.md) V2): `gc.MaxObjectsInGame=8192` (UE's setting, the
  PS2's capacity) caps the UObject array on every platform (`-NoMemoryLimit` lifts it; LeonCook does); the cook
  writes `<Project>/Saved/Cooked/<Platform>-RamReport.txt` (each map's cooked packages); `-LogFrameTimes` adds the
  frame's GS work (`Frame work over N frames:` triangles, register writes, texture uploads), the same on the PC and
  the PS2.
- `leonrun` (`Engine/Platforms/PS2/Build/PlayRunner/`): boots a PS2 ELF headless on Play!'s HLE BIOS, without a
  console BIOS; the first botmatch on the EE outside PCSX2.
- `[/Script/Engine.RendererSettings]` `DisplayAspectRatio` (4:3, the TV's) and `SyncInterval` (2: 30 fps), the PS2's
  values on every platform (`FRendererSettings`); `IRendererModule::GetDisplayAspectRatio`.
- The desktop's gamepad as the DualShock (`FGLFWInputInterface`), and `FDualShockAnalog` (libpad's bytes and dead zone)
  shared with the PS2.
- `FAudioOutput`: the desktop's device of miniaudio (fed through its lock-free ring buffer) and the PS2's audsrv.
- `-LogFrameTimes` splits the frame (`Frame split over N frames`: input, audio, world, GC, viewport tick, scene
  updates, scene, HUD, overlay, canvas, present, engine loop) and, on the PS2, the present (`Present over N frames`:
  the GIF packet, its DMA, the GS finishing, the vertical blank wait). The parts are integer cycles
  (`FFrameTimeClock`), read only with the flag ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N0).
- `Package.bat -Only <Name[,Name]>` packages only those artifacts.
- Tests: `System.Engine.GameFramework.RestartReplacesTheSpectator`, `ShooterGame.Rounds.DeadPlayerPlaysAgain` and 4
  more ShooterGame tests (49), `System.Renderer.PS2Preview.*`, `System.ApplicationCore.DualShock.Analog`,
  `System.ApplicationCore.Desktop.GamepadAsDualShock`, `System.AudioMixer.Device.QueuesTheMix` (414 engine tests on
  Linux, 423 on Win64).
- The PS2 frame measured in PCSX2, unattended ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N1): `MeasurePS2.bat` stages
  ShooterGame, runs a bot match watched through a bot's eyes in PCSX2 without its window, from a private data folder
  with pinned settings (`Engine/Platforms/PS2/Build/PCSX2/Measure.ini`), and records the game's
  `FrameStats Summary:` line (the run's frame time percentiles, its parts, the GS work, the heap and object peaks) in
  `Saved/Profiling/PS2Frame.csv` and a Budgets.md row. The first: 25.4 fps (p50 33.5 ms, p95 50.3 ms), the same in
  three runs. `-ExitAfterSeconds=N`, `-BotMatchSpectate` and `ViewNextPlayer` (CS's spec_next) come with it.
- `RunGates.bat [-PS2] [-Measure]`: every local gate in order with a summary (the repository has no CI).
- The GS emulator's parity ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N8): the desktop's emulator does what the
  reference rasterizer does for MIPMAP (MIPTBP1 / MIPTBP2's levels, the LOD from Q per pixel or K, MMAG / MMIN, MXL,
  trilinear by the LOD's fraction), the whole blend equation (Cd factors, Ad, FIX and As above 0x80, negative results),
  COLCLAMP wrapping, dithering after the blend against the 16-bit destination, PABE, FBA, the destination alpha test,
  FBMSK bit by bit, REGION_CLAMP / REGION_REPEAT on every level, CLD 0 to 5 with CBP0 / CBP1 (`FGSClutBuffer::Update`,
  shared), and lines and points stepped as the reference steps them (`GSStepLine`, GSCore). Draws that read the frame
  go group by group over a copy of it. `FGSCommandList::IsSupported` now accepts exactly that (AA1, FIX, CSM2, MTBA,
  CLD 6 and 7 and the reserved encodings stay out), `AddVertexNoKick` takes XYZF3, and the rule is in
  CODING_STANDARD: a GS feature enters the renderer only with its conformance scene. Ten new GSConformance scenes
  (MipmapLod, AlphaTest, Fog, TexAAndFunctions, ClampModes, StripsAndSprites, BlendEquation, PabeFbaDate,
  Dither16Blend, ClutLoads; the PS2 program shows 20 in a 4 x 5 grid), each with its `System.GSReference.*` test
  (433 engine tests, TestPAL 137).
- Cycle stats, a profiler on every platform ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N9): `Stats/Stats.h` in Core
  (UE's `DECLARE_STATS_GROUP`, `DECLARE_CYCLE_STAT` / `DECLARE_CYCLE_STAT_EXTERN` / `DEFINE_STAT`,
  `SCOPE_CYCLE_COUNTER`, `FThreadStats`; Leon's `FCycleStatsWindow`): a fixed tree of 256 nodes and 16 levels, no
  allocation per scope, recording only with `-LogFrameTimes` or `stat cycles` (F7, a page in the debug overlay). The
  clock is `FPlatformTime::Cycles()` (QPC on Win64, the COP0 Count on the EE), and on the EE the performance counters
  (PCR0 / PCR1) count the instruction and data cache misses. The engine frame, the world, the collision queries, the
  AI, the bots, the audio mix, the GS scene renderer and the PS2 present are instrumented; `-LogFrameTimes` logs a
  `Profile over N frames (ms, calls):` block every 5 s and `ProfileSummary:` at exit (MeasurePS2 adds it to its CSV),
  and works headless too. The `Frame split` and `FrameStats Summary:` come from the stats: `FFrameTimeClock`,
  `FEngineFrameSplit`, the draw times structs and the PS2's `Present over` line are gone. PS2 (PCSX2): 24.7 fps, the
  scene 12.6 ms, the world 9.2 ms, the canvas 4.6 ms, the audio mix 2.1 ms a frame; a scope costs 451 ns on the EE
  (0.11 % of the frame). Tests: `System.Core.Stats.*` (6).

### Changed

- The view projects at the display's aspect ratio (4:3), so the PS2's scene is no longer squashed on the TV, and the
  desktop shows the frame the TV's way (the lines at a whole scale, each stretched linearly).
- The desktop keeps the PS2's frame rate (`FFramePacer`; `-benchmark` does not wait); the PS2 reads `SyncInterval` from
  the renderer settings instead of `PS2Settings`.
- One `FAudioDevice` on every platform, mixing with `FSoftwareAudioMixer`: the desktop hears the PS2's mix.
- The Win64 cook makes the textures paletted, as the PS2's; the desktop draws uncooked RGBA8 textures through the same
  conversion.
- The PS2 builds at -O2 as documented: CMake's GNU Release flags appended -O3 after the toolchain's -O2; the toolchain
  now forces `CMAKE_CXX_FLAGS_RELEASE` (ShooterGame's text: -8.4 %).
- A player's camera views its new pawn when it possesses one (UE's ClientRestart), whoever it watched as a spectator.
- `RunPCSX2.ps1` runs ShooterGame by default.
- The PS2's debug text and rectangles are `FGSDebugDraw` (GSCore): recorded into an `FGSCommandList`, so every backend
  draws them; the error screen and GSConformance use it, and the error screen keeps three lines per log error. R3
  toggles `stat unit` on the DualShock. Tests: `System.GSCore.DebugDraw.String`, `.Rect` (426 engine tests, 132
  TestPAL on Win64).
- The GS local memory (`FGSLocalMemory`, GSCore) has the GS's layout instead of a linear one: pages, blocks placed by
  each storage format's table, columns and the manual's pixel order for all 13 formats (chapter 8), PSMCT24 and the
  PSMT8H / PSMT4HL / PSMT4HH formats sharing PSMCT32's words, addresses wrapping at 4 MB. Buffers are addressed by
  block (`ReadPixel(BasePointer, BufferWidth, ...)`); the emulator and GSReference share it, and the texture cache and
  the cook's VRAM report take a texture's real footprint (`FGSTextureLayout::GetNumBlocks`, `GetBaseAlignment`): one
  that fits in a page starts at any block ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N7). The existing frames are
  unchanged. Tests: `System.GSCore.LocalMemory.*` (423 engine tests, 137 TestPAL on Win64).
- The PS2 waits for the vertical blank by interrupt and follows the console's region
  ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N10): an `INTC_VBLANK_S` handler (`FPS2VerticalBlank`) counts the fields
  and signals a semaphore, and `FPS2RHI::WaitVSync` sleeps on it until the field `FGSFieldPacer` (GSCore) picks from
  the count (`SyncInterval` fields after the last flip, or the next blank for a late frame) instead of spinning on the
  GS's CSR and guessing the field from a 59.94 Hz clock. The display is PAL when ROMVER says so (`-PAL` / `-NTSC`
  choose), with the 640x448 frame centred in PAL's 512 lines: 25 fps there, 30 on NTSC. `FPS2RHI::ShutdownDisplay`
  removes the handler. G4 bans libgraph's polls and `graph_initialize`. PCSX2: 25.5 fps, p50 / p95 / p99 of 33.5 /
  50.3 / 66.8 ms (2, 3 and 4 fields). Tests: `System.GSCore.FieldPacer.*` (439 engine tests, 145 TestPAL on Win64,
  138 on the EE).
- The PS2 sends the frame as a DMA chain, double buffered ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N11):
  `FGSGifPacket::BuildChain` (GSCore, the one encoder of the packet, with `Build`) writes the frame's list straight into
  one of two 128-byte aligned buffers through the uncached accelerated segment, as CNT / END sections with each
  upload's pixels by a REF to the command list's copy (written back with `SyncDCache`), and
  `dma_channel_send_chain_ucab` kicks it without waiting. The EE builds the next frame into the other list and buffer
  while the DMA and the GS draw; `WaitVSync` waits for the GS's FINISH only right before the flip and retires a buffer
  (`dma_channel_wait`) only before its reuse. A frame reaches the screen one frame after it is recorded.
  `FlushFrame`, its `TArray` copy, `memcpy` and synchronous waits, and libpacket are gone (G4 bans them). PCSX2: the
  GIF packet's build 1.04 → 0.51 ms a frame, 25.7 fps; GSConformance draws the same. Tests:
  `System.GSCore.GifPacket.Chain` (the tags, QWC / ID / ADDR, the payload's alignment, the chain carrying `Build`'s
  bytes; 440 engine tests, 146 TestPAL on Win64, 139 on the EE).

### Removed

- ThirdPerson, the PS2 demo built without the engine, and everything only it used
  ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N2):
  - the immediate path of `FPS2RHI` (`DrawBox`, `BindMaterial`, the view and light setters, `DrawDebugText`,
    `DrawUnlitRectAlpha`, `ScreenVertex`, the Draw3D counters, `FPS2Texture`, `FPS2Material`, math3d);
  - Launch's loop without the engine and its platform hooks;
  - `FPS2StatsOverlay` (never drawn in ShooterGame) and Core's `FStatsOverlay`;
  - LeonBuildTool's `COMPILE_AGAINST_ENGINE`.

  A game target always compiles against the engine (`WITH_ENGINE` is 1 for games, 0 for programs), and G4 bans the
  removed names.
- The Linux platform (its HAL, launch, OpenGL memory query, build scripts and `PLATFORM_LINUX`) and the JoltPhysics
  plugin, with the physics backend layer that existed only for it (`IPhysicsBackend`, `EPhysicsBackend`,
  `UWorld::SetPhysicsBackend`): Win64 and the PS2 are the platforms, and `FPhysScene` is the one physics, the same on
  both ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N3). The engine ships no plugin; the plugin system stays, and
  `System.Engine.Projects.RealDescriptors` checks it with a sample plugin (416 engine tests).
- Dead data ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N4): what no renderer reads.
  - `FMaterial` / `UMaterial` keep what the GS scene renderer draws with (the shading, the base colour, the opacity, the
    UV scale and the base colour map): `Specular`, `Metallic`, `Roughness`, `Shininess`, `bPlanarMirror`, `NormalMap`,
    `RoughnessFromShininess` go, and `bCastsShadows` with its unused chain (`HasShadowCastingMaterial`,
    `FStaticMeshSceneProxy::IsShadowCaster`); `EMaterialLightingModel::BlinnPhong` is `Lit`. The importers read only
    those values; `M_SolidMetal`, which differed only in them, goes.
  - `FVertex::Tangent` and `FSkeletalVertex::Tangent` (32 and 64 bytes a vertex), with `ComputeTangents` and the tangent
    conversions. Packages are `VER_LEON_REMOVE_VERTEX_TANGENT` (2), the oldest loadable version: the engine and
    ShooterGame content was saved again.
  - `UEngine::DefaultBumpNormalTexture` and `T_Default_Bump_N` (65 KB of the PS2's texture VRAM); the PS2 cook stages
    no `Engine/Shaders` (`ITargetPlatform::GetAllTargetedShaderFormats`: only Win64 compiles them).
  - `LegacyContentYaw`: the content faces +X, so `ACharacter`'s mesh has no relative yaw.

  412 engine tests.

### Fixed

- A dead player never played again: `AGameModeBase::RestartPlayer` kept the spectator pawn as the player's pawn. The
  spectator no longer counts as a pawn to keep, nor blocks a start; spectating from the login looks from the
  controller instead of the origin; the buy menu closes on possess; no crosshair while spectating.
- ShooterGame: automatic fire keeps its rate at 30 fps (the frame's overshoot carries to the next shot); a frozen pawn
  cannot fire through a trigger held from before; a reload leaves the AWP's scope; a new round clears the recoil and
  the spread; a pawn killed in the air falls to the floor (its bomb can be picked up); a defuser who survives the blast
  stops defusing; `mp_restartgame` needs both teams from the warmup; a frozen bot sees nobody, and a lost enemy is
  searched for where it was last seen (it was its live position, through walls).
- `Package.bat -NoPS2 -Only TestPAL` packaged nothing and succeeded: the "nothing to package" check runs after `-Only`.
- The PS2's mix no longer uses doubles (software on the EE) per output frame: the voices' position is 16.16 fixed
  point. The canvas keeps its text and vertex buffers between frames.
- The HUD's text was twice the size it was designed at: `HudFontScale` (and the debug overlay's) was 2 for the old
  1280x896 canvas, and every platform draws the canvas into the GS's 640x448 frame since 0.21.0. It is 1, the font's
  own pixels; ShooterGame's HUD lays out from `HudLineHeight` (the scoreboard's box fits its lines, the kill feed
  starts below the stats).
- The stats on the PS2: Select cycled the PS2 overlay, and ShooterGame's scoreboard is Select, so looking at the scores
  hid them; L3 + R3 cycle it now. The engine's `stat unit` panel (FPS, MS, RAM, VRAM, TRIS, OBJ) and the PS2's panel
  share `FStatsOverlay`'s visibility, on from the start on the PS2 (`bShowStatsByDefault=True` in PS2Engine.ini, which
  had an unread `[/Script/Engine.StatsOverlay]` section); the PS2's panel keeps what the engine's cannot show (the
  EE's work per frame) and the gamepad widget, at the top left. The F-key help is the desktop's only, under the stats.
- The PS2 resets its IOP to the ROM's modules before loading its own (`FPS2PlatformMisc::InitializeIop`; `-NoIopReset`
  for ps2link), as the ps2sdk samples do: a launcher's pad modules no longer clash with PADMAN. A pad module that does
  not load no longer hangs `padInit`. Review of the SDK's use and what to use next:
  [PS2SDK.md](Engine/Platforms/PS2/Documentation/PS2SDK.md).
- A PS2 game that cannot read its config (PCSX2 without its host filesystem, or an ELF booted without its pak) stopped
  on a black screen: the window asked for the desktop's 1280x896, which does not fit the GS's VRAM. The PS2 window
  now keeps to the TV's modes (640x448 otherwise), PreInit stops with an error that names the folder it read when a
  console build has no Engine config, and `FPS2ErrorScreen` shows the log's last errors and what to check on the TV
  (on a failed start and on a fatal error, through `FPS2PlatformMisc::SetFatalExitHandler`). The debug font gains the
  punctuation of paths and log lines; an empty argv[0] resolves to `host:`.
- A DualShock plugged back in stayed digital: the pad is asked for its analog mode again after a reconnection, and
  whenever it reads stable but is not in that mode (`FDualShockConnection`,
  `System.ApplicationCore.DualShock.Reconnect`; [ps2-shipping](Docs/PLANS/ps2-shipping.md) N5).
- Paletted textures lost their alpha on the GS (TCC was RGB): the CLUT's cooked alpha reaches the blend.
- The PS2's audio queue no longer blocks the game thread when audsrv's buffer is full: what does not fit is dropped
  and logged. The audio device counts time in integer cycles (no doubles on the EE); the mixer's 16.16 position is
  checked over ten simulated minutes (`System.AudioMixer.SoftwareMixer.NoDrift`).
- Per-frame allocations: an actor's tick and the character overlap resolution copy into inline storage, the effects'
  mask is built once, and a translucent mesh is transformed once for all its sections.
- ShooterGame's confirmed bugs ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N6):
  - the bots kept an empty rifle drawn (they draw the best weapon every tick they engage):
    `AShooterCharacter::EquipBestWeapon` passes over a weapon without ammunition (`AShooterWeapon::HasAmmo`: clip and
    reserve empty; one that can reload counts) and keeps the weapon in hand when all are spent
    (`ShooterGame.Weapons.BestWeaponSkipsEmpty`);
  - the C4 reached 17.5 m, CS's 1750 units read as centimetres, and ignored armor: `ExplosionRadius` is 4445 cm and
    the bomb has the HE grenade's `ArmorRatio` (1: health takes half, the armor half the rest, as CS 1.6 armors
    blasts) (`ShooterGame.Bomb.ExplosionRadius`, `ShooterGame.Bomb.ExplosionRespectsArmor`);
  - the bots' sprays were laser-straight: the turn toward the target erased the recoil every tick. The bot turns its
    own aim and the kick rides on top, `RecoilCompensation` (0.5, times `Difficulty`) of each kick pulled back down
    (`AShooterWeapon_Instant::CompensateRecoil`) (`ShooterGame.Bots.RecoilKicksTheAim`);
  - the buy menu stayed open after the buy time and opened for a spectator (taking its Cross): it opens only when its
    player may buy (`AShooterPlayerController::CanOpenBuyMenu`), closes by itself when that stops (the buy time's
    end, the buy zone left), and the HUD shows why for a moment (`ShooterGame.Buy.MenuFollowsTheRules`);
  - the buy time counted from the freeze's start, leaving the live round `BuyTime - FreezeTime` (39 s) to buy: it
    counts from the freeze's end, as CS's `mp_buytime` (`ShooterGame.Buy.BuyTimeAfterTheFreeze`).
- The desktop's CLUTs overwrote each other: the texture cache puts PSMT8 CLUTs 4 blocks apart, which is right on the
  GS, but the emulator's linear local memory stored a 16 x 16 CLUT over 16 blocks, so a second paletted texture
  corrupted the first one's palette (and GSReference's) ([ps2-shipping](Docs/PLANS/ps2-shipping.md) N7). Tests:
  `System.Renderer.GS.TextureCache.TwoPalettes`, `System.GSReference.Texture.TwoPalettes` and the conformance scene
  `TwoPalettes`, which GSConformance shows on the PS2 too.

## [0.21.0] - 2026-09-26

The PS2's Graphics Synthesizer becomes the one renderer, and ShooterGame runs on the PS2. The frame is recorded as the
GS's register writes (a command list every backend consumes): the PS2 sends it to the GIF, the desktop executes it on
an OpenGL emulation of the GS that a software GS written from the manual checks. The gameplay framework and the
Renderer build for the Emotion Engine, and ShooterGame plays there from its PS2 cook, with the DualShock at 30 fps and
with sound on the SPU2 ([ps2-gs-parity](Docs/PLANS/ps2-gs-parity.md), [ps2-engine](Docs/PLANS/ps2-engine.md)). The
engine and ShooterGame content is resaved for 0.21.0. What needs the console's emulator (the captures, the frame
times, the sound) is a manual checklist in [TESTING.md](Docs/TESTING.md#ps2-validation-in-pcsx2-ps2-engine).

### Added

- **GSCore**: the GS registers and formats of the GS User's Manual, `FGSCommandList` (register writes and image
  uploads, limited to what every backend reproduces), `FGSGifPacket` (PATH3 packets), `FGSDrawEnvironment`,
  `FGSLocalMemory`, `FGSTexelDecoder`, `FGSTextureLayout` and the conformance scenes; every platform.
- **GSReference** (Developer): a software GS executing a command list by the manual's rules, the oracle of the
  emulator and of the PS2 backend. **GSConformance** (PS2 program) draws the conformance scenes on the console.
- **The GS scene renderer** (`FGSSceneRenderer`): the transform, clipping (near, far, a guard band), back face culling,
  skinning and Lambert lighting on the CPU; opaque and translucent meshes, impact marks, tracers, debug lines, the view
  model and the canvas; `FGSTextureCache` in a VRAM arena.
- **The OpenGL GS emulator** (`FGSOpenGLEmulator`): the desktop's renderer, 640x448 shown at a whole-number scale; its
  conformance and scene frame tests against the reference (`System.Renderer.GSEmulator.*`, NonNullRHI;
  `LeonAutomationTests -nodisplay` skips them).
- **PS2 renderer module** (`FPS2RendererModule`), `FPS2RHI::Submit`, `AllocateTextureArena` and `SetSyncInterval`
  (UE: `rhi.SyncInterval`; `SyncInterval=2` in PS2Engine.ini: 30 fps).
- **The engine on the EE**: Engine, UMG, SlateCore, AnimationCore, AIModule, PhysicsCore (without Jolt), RenderCore,
  the Renderer and ShooterGame build for PS2; `LeonCommandLine.txt` beside the ELF gives it its arguments.
- **The PS2 cook**: textures as `PF_P8` / `PF_P4` (TextureCompressor's `FPalettedTextureBuilder`: powers of two up to
  256, deterministic median cut), `<Project>/Saved/Cooked/PS2-VramReport.txt`; `BuildCookRun -platform=PS2 -pak`
  stages one pak aligned to 2048 bytes.
- **Pad input**: `UGameViewportClient::SetInputInterface` sends the application's gamepad to the player; ShooterGame
  maps the DualShock (`TurnRate` / `LookUpRate`, `BaseTurnRate` / `BaseLookUpRate`) and its buy menu takes the D-pad,
  Cross and Circle while it is open.
- **PS2 audio**: `FSoftwareAudioMixer` (every platform) and the PS2 `FAudioDevice` streaming it to the SPU2 through
  audsrv; LeonBuildTool's `RUNTIME_DEPENDENCIES` (UE: `RuntimeDependencies`) puts `audsrv.irx` beside the ELF.
- `-LogFrameTimes` logs the frame times every 5 seconds; `Package.bat` packages ThirdPerson, TestPAL, GSConformance
  and the playable PS2 ShooterGame.
- Tests: `System.GSCore.*`, `System.GSReference.*`, `System.Renderer.GS.*`, `System.Renderer.GSEmulator.*`,
  `System.TextureCompressor.Paletted.*`, `System.LeonEd.Cook.PalettedTextures`,
  `System.PakFile.Format.DeviceRootMountPoint`, `System.Engine.Viewport.Gamepad`, `System.AudioMixer.SoftwareMixer.*`,
  `ShooterGame.Input.Pad`, `ShooterGame.Input.BuyMenuTakesItsKeys` (407 engine tests on Linux, 416 on Win64; 44
  ShooterGame tests; TestPAL 130).

### Changed

- Every platform draws the GS frame: the desktop window defaults to 1280x896 (the frame at scale 2), and the stats
  report the GS register writes and texture uploads. `IRendererModule` gains `GetRenderTargetSize` and
  `EndDrawingViewport`.
- The PS2 frame is PSMCT16S with dithering and a PSMZ24 Z buffer; ThirdPerson draws through the command list.
- `UTexture2D` stores any format `GetPixelFormatDataSize` knows; the pak matches a device root (`host:`, `cdrom0:`)
  with or without its `/` and finds `<Project>-<Platform>.lpak` by name where a device cannot list folders.
- The buy menu's number keys live in the menu's own input component, pushed only while it is open.

### Removed

- The OpenGL scene renderer and what the GS cannot do: the shadow map, the planar mirror, Blinn-Phong specular and
  normal maps, `FGPUPassTimer`, the GPU resource cache, their shaders, `MakeReflectMatrix`, `FitLightSpaceMatrix` and
  the `r.ShadowMapResolution` / `r.PlanarReflectionScale` settings.
- `ACharacter::FaceRotation` (unused; it hid `APawn`'s).

### Fixed

- The number keys 1, 2 and 4 drew no weapon: the buy menu's bindings consumed them even with the menu closed.

## [0.20.1] - 2026-09-26

An audit of the plan's implementation (P17 to P21), and its corrections: gameplay and robustness bugs, the plan's gaps
(the bots' escort, hunt and site rotation, the waypoints' flags, a UMG widget tree for the buy menu), the gates enforced
in CI, and no legacy or dead code left. Win64 is the development and editor platform and the PS2 the target: the Win64
preview renderer loses its post processing (SSAO, FXAA, tone mapping), which the PS2's Graphics Synthesizer cannot do,
and classes with no UE counterpart are removed. The engine and ShooterGame content is resaved for 0.20.1.

### Added

- **Bots** (ShooterGame; [README](Game/ShooterGame/README.md)): the behavior tree gains `Escort` (a terrorist without
  the bomb stays within `EscortDistance` of the carrier), `Hunt` (a team outnumbering the enemy by `HuntAdvantage`
  with no bomb planted goes to the enemy's spawn) and the CT's site rotation after `RotateTime` without contact; the
  three are `[/Script/ShooterGame.ShooterAIController]` config values. `AShooterGameMode` exposes `CountAlive` and
  `GetTeamSpawnLocation`.
- **Waypoint flags**: `UNavigationSystem::FindPath` can return each point's node, and `AAIController` jumps near a
  point flagged `Jump` and crouches along the links of a point flagged `Crouch` (standing up past them).
- **UMG widget tree** (UE's shape): `UWidget` (`ESlateVisibility`, `Slot`, `GetDesiredSize`, painted in its slot's
  rectangle), `UPanelWidget` / `UPanelSlot`, `UContentWidget`, `UBorder`, `UVerticalBox` / `UVerticalBoxSlot`,
  `UCanvasPanel` / `UCanvasPanelSlot`, and `UTextBlock`, `UImage` and `UProgressBar` as widgets; `UUserWidget` is a
  `UWidget` with a `UWidgetTree` (`ConstructWidget`, `RootWidget`, `Initialize` / `NativeOnInitialized`). SlateCore
  gains `FMargin` and `EHorizontalAlignment`.
- ShooterGame's buy menu is a widget tree (canvas, border, vertical box of text blocks); `buymenu` opens it from the
  console (CS). The HUD tells the terrorists when the bomb is dropped.
- F2 (`show Collision`) and F3 (`show Navigation`) draw the capsules, the bodies and the waypoint graph into the
  world's line batch in a windowed run.
- `r.ShadowMapResolution` and `r.PlanarReflectionScale` are read from `[/Script/Engine.RendererSettings]`.
- `APainCausingVolume` hurts the pawns it holds (`CausePainTo`, `DamagePerSec` × `PainInterval` every
  `PainInterval`).
- `AShooterGameState` counts round and match serials (`GetRoundSerial`, `GetMatchSerial`).
- CI runs on every push; the PS2 job builds TestPAL and prints every ELF's sections (G3); the Win64 job checks the
  format with clang-format 20.1.8 (G1); `RunTests.bat` builds and runs TestPAL on Win64.
- Tests: `System.Engine.World.ForEachVisitsEveryActorWhenOneIsDestroyed`,
  `System.Engine.CharacterMovement.ZeroStepKeepsTheVelocity`, `System.Core.Config.UserLayerArraysAndRemovals`,
  `System.Engine.Damage.PainCausingVolume`, `System.AIModule.Gameplay.AIControllerPathFollowReadsWaypointFlags`,
  `System.UMG.WidgetTree.LayoutAndPaint`, and ShooterGame's `Bomb.SurvivingCarrierLetsGo`, `Bomb.NoPlantAfterTheRound`,
  `Rounds.RestartInTheFirstRound`, `Economy.ProjectileKillCredit`, `Bots.AgentFromConfig`,
  `Bots.TerroristsEscortTheCarrier`, `Bots.OutnumberingTeamHunts` and `Bots.CTRotatesBetweenSites`.

### Changed

- The waypoint agent comes from `[/Script/Engine.NavigationSystem]` (`FWaypointLinkParams::FromConfig`) for both the
  map import and the world's graph; ShooterGame's agent jumps 112 cm, within the character's 114 cm jump.
- `AAIController` reports UE's `EPathFollowingStatus` (`GetMoveStatus`) instead of Leon's logic state and wish
  direction.
- The config saves a user layer's array keys as `!Key=ClearArray` plus `.Key=` lines, saves `RemoveKey` /
  `EmptySection`, and `Flush(true)` reloads a global file from its hierarchy.
- The pak file's handles share their pak (valid after `Unmount`); `FTicker` passes the frame's delta time to delayed
  tickers too (UE).
- The AWP pays $300 a kill, as every CS 1.6 weapon; grenade kills are credited from the projectile.
- The banned-API check (G4) bans the D2 headers and every std container, stream and `string_view`.
- PS2 Docker builds from a Unix host run as root and hand the outputs back to the host user.
- MSVC's shadowing warnings are errors on the other compilers too (`-Werror=shadow`).
- Build.version 0.20.1; the engine and ShooterGame content resaved with it.

### Removed

- The Win64 renderer's post processing: SSAO, FXAA, the ACES composite, the HDR and LDR colour targets, early-Z and
  the quality presets (`FPostProcessSettings`, `EPostProcessQuality`), and their shaders; the GPU timers keep the
  shadow, planar and colour passes.
- Classes and helpers with no UE counterpart: `UBobbingMovementComponent`, `UOrbitMovementComponent`,
  `UInteractableComponent`, `VolumeHelpers`, `FBasicShape` / `FBasicLight`, the UMG interaction prompt and menu list
  widgets, `UButton` (Leon's widgets take no input), `AIChaseBehavior`, and the `GameplayMinimal.h` and
  `PhysicsMinimal.h` umbrella headers.
- Unused API: `AGameModeBase` world preparation helpers, `AGameStateBase::MapName` and
  `ReplicatedWorldTimeFrames`, `APlayerState::Lives`, `CreateShared` windows, `SweepCapsuleAlongSegment`,
  `EMeshDataBasis`, the renderer's editor hooks, the unread `DefaultPhysicsBackend` key, and ShooterGame's unused
  blackboard keys and accessors.

### Fixed

- `UWorld::ForEach` no longer skips an actor when the one before it is destroyed during the visit.
- A zero-length character step keeps the velocity instead of dividing by zero.
- `AAIController`'s chase repath no longer resets the stuck clock, and a failed path is retried at the repath
  interval; a path's goal must be walkable from its node.
- `APlayerController::Destroyed` destroys its spectator pawn; `UWorld::Clear` clears the navigation; pawn sensing
  skips actors destroyed during its update.
- ShooterGame: a carrier alive at the round's clean-up lets go of the bomb; planting needs a live round; bots buy
  again after `mp_restartgame` in the first round; `mp_restartgame 0` at time 0 fires; the scoreboard leaves the bot
  match's spectator out.
- `APainCausingVolume` ticks (an `AVolume` does not by default).
- `LinkerLoad` refuses table counts the file cannot hold; `SerializeIntPacked` checks its byte count before
  shifting; `LeonPak -extract` refuses entries outside the destination.
- Win64 builds of ShooterGame no longer stop on MSVC's C4458 (locals hiding inherited members).
- The shadow box used without casters is Z-up.

## [0.20.0] - 2026-09-26

The seventeenth step of the plan (P17): ShooterGame boots with a basic FPS. The engine gets UE's collision channels
and responses and UE's character movement model (acceleration, friction, braking, air control, crouching), both
opt-in or default-preserving, so the golden tests and the Win64 frames are unchanged. On them, `Game/ShooterGame` is a
Win64 code project with its own test program: two teams on `de_leon`, a blockout map built by a Blender script and
imported with the project's rules, a first-person character with CS 1.6 movement, bots that join the teams, a
crosshair, and a smoke test (gate G6) in CI. The PS2 ELFs are unchanged (ThirdPerson, BlankProgram and TestPAL link
none of the changed modules).

The eighteenth step (P18): weapons and damage. The engine gets UE's damage API (`TakeDamage` with damage events and
damage types, `ApplyDamage` / `ApplyPointDamage` / `ApplyRadialDamageWithFalloff` with a line of sight), the projectile
movement, the spectator pawn and state, actor life spans, static mesh sockets imported from glTF, a view model pass and
world effects (impact marks, tracers, short-lived lights). ShooterGame gets Counter-Strike's weapons on them: a pistol,
a rifle, an AWP with its scope and an HE grenade, with deterministic spread and recoil, reloads, headshots, armor,
friendly fire, death with a dropped weapon that others pick up, and spectating; the weapons' meshes and sounds are made
by scripts in this repository (CC0). A native class's defaults now keep what its constructor sets for inherited config
members (a CoreUObject fix the weapon classes need).

The nineteenth step (P19): Counter-Strike's defusal rules. A match of rounds (a warmup until both teams have a
player, then the freeze, the round, the result), CS 1.6's money (rewards, the loss bonus, the plant bonus), buying in
the buy zones with a UMG buy menu, the bomb (carried, dropped, planted in a site, defused or exploding), a HUD with the
clock, the score, the money and the kill feed, and CS's console commands. The engine keeps characters on the floor
through long frames (a slow frame sank them into de_leon's floor).

The twentieth step (P20): bots. The grid navmesh gives way to a waypoint graph (UE3's path nodes) that the map import
links by itself, the AIModule gets a typed blackboard and UE's pawn sensing, and ShooterGame's bots play: they buy,
walk de_leon, see and hear their enemies, fight with a human-like aim, plant, defuse and hold the sites, all from
seeded streams so a match replays.

The twenty-first step (P21) hardens it: `ShooterGame -botmatch` plays a headless match of bots at unpaced fixed steps
and fails the process when a rule's invariant breaks; CI plays ten rounds twice and requires the same result, and a
staged Shipping build plays three. The replay check found a read of freed memory in the AI's path following. The
game's reflection, object, name and heap numbers go to the PS2 budgets as the port's targets. The engine and project
content is resaved for 0.20.0 (a package records the engine version); the PS2 ELFs were not measured in these phases.

### Added

- **Collision channels** (P17, PhysicsCore and Engine; [ARCHITECTURE.md §11](Docs/ARCHITECTURE.md#11-physics)).
  - UE's `ECollisionChannel` (`ECC_WorldStatic` … `ECC_GameTraceChannel18`) and `ECollisionResponse`,
    `FCollisionResponseContainer`, `FCollisionResponseParams`, `FCollisionObjectQueryParams`; `FCollisionQueryParams`
    ignores actors and components (`AddIgnoredActor`, `AddIgnoredComponent`); `FHitResult::GetActor` /
    `GetComponent`.
  - Every body has an object type and responses (`FBodyInstance::ObjectType`, `CollisionResponses`); a query's hit is
    the smaller of the body's response to the trace channel and the query's response to the body's type. `FPhysScene`
    traces and sweeps by channel (`Single`: the first block; `Multi`: every block and overlap, nearest first) and by
    object type (`LineTraceSingleByObjectType` / `Multi`).
  - `UPrimitiveComponent::SetCollisionObjectType`, `SetCollisionResponseToChannel` / `ToChannels` / `ToAllChannels`,
    `SendPhysicsTransform`; `UCollisionProfile` reads a game's named channels (`[/Script/Engine.CollisionProfile]
    +DefaultChannelResponses`).
  - The character's capsule is a query-only `ECC_Pawn` body (UE's Pawn profile), an upright capsule the traces hit
    exactly; the character's own sweeps ignore it.
- **UE's movement model** in `UCharacterMovementComponent` (`bInstantVelocity = false`): `MaxAcceleration`,
  `GroundFriction`, `BrakingDecelerationWalking` / `Falling`, `BrakingFrictionFactor`, `BrakingFriction` +
  `bUseSeparateBrakingFriction`, `FallingLateralFriction`, `AirControl` with its boost (`CalcVelocity`,
  `ApplyVelocityBraking`); `MaxWalkSpeedCrouched`, `CrouchedHalfHeight`, `Crouch` / `UnCrouch` (standing up needs room
  above), `ACharacter::Crouch` / `UnCrouch` / `OnStartCrouch` / `OnEndCrouch`, `CrouchedEyeHeight`,
  `FNavAgentProperties::bCanCrouch`; a virtual `GetMaxSpeed` for a game's speed modifiers; `APawn::AddMovementInput`
  (UE's signature) and the pawn's input vector ([ARCHITECTURE.md — Character movement](Docs/ARCHITECTURE.md#character-movement)).
- First person: `UCameraComponent::bUsePawnControlRotation` (`GetCameraView`, `AActor::CalcCamera`) and
  `UPlayerInput::SetMouseSensitivity` (an Exec command).
- `AHUD::DrawHUD` (virtual) and `Canvas`; `UGameplayStatics::ParseOption` / `HasOption` / `GetIntOption`.
- **ShooterGame** (`Game/ShooterGame`, Win64; [README](Game/ShooterGame/README.md)): `AShooterGameMode` (teams:
  `?team=` or the smaller team; the first free start tagged with the team; `bot_add_ct`, `bot_add_t`, `bot_add`,
  `bot_fill`; five a side), `AShooterCharacter` (a first-person camera at 163 cm, 76 crouched, a 40 × 91.5 cm capsule,
  the teams' placeholder bodies for the others), `UShooterCharacterMovement` (CS 1.6 at 1 unit = 2.54 cm: 635 cm/s,
  crouched 212, the walk key at 52 %, acceleration 3175 cm/s², friction 4, gravity 2032 cm/s², jump 682 cm/s),
  `AShooterPlayerController` (`ViewFrom`, `ViewPawn`), `AShooterAIController`, `AShooterPlayerState` (the team),
  `AShooterHUD` (CS's crosshair). Its config maps CS's keys (Ctrl / C crouch, Shift walk, 0.07° a pixel) and names the
  `Weapon` channel (`ECC_GameTraceChannel1`) for P18.
- **de_leon** ([LEVELS.md](Docs/LEVELS.md#worked-example-de_leon)): `SourceArt/Maps/make_de_leon.py` builds a 60 × 48
  m blockout in Blender (headless): the T and CT spawns, bomb sites A and B, three lanes, mid doors, crates, a crate
  stack with a `UCX_` box, a player clip, the buy zones, five tagged starts a team and 18 linked waypoints, lit by one
  sun; it is imported to `/Game/Maps/de_leon` with the project's rules (`BombSite`, `BuyZone`) and `RequiredTags`, and
  is the project's `GameDefaultMap`. `make_team_bodies.py` makes the teams' placeholder bodies; `SourceArt/LICENSES.md`
  lists the art (CC0, all made here).
- `ShooterGameTests` (the project's test program) and LeonBuildTool's `AUTOMATION_TEST_MODULES`: a program collects
  only the named modules' tests.
- **Gate G6**: `Engine\Build\BatchFiles\SmokeTest.bat` runs ShooterGame headless with `-ExecCmds=bot_fill` and checks
  the exit code and the ten pawns, five a team.
- Tests: `System.Engine.CollisionChannel.*` (8), `System.Engine.CharacterMovement.*` for UE's model (12),
  `System.LeonEd.MapFactory.EngineMapsSkipRequiredTags` (371 tests), and `ShooterGame.*` (10).

- **Damage** (P18, Engine; UE's API): `FDamageEvent`, `FPointDamageEvent`, `FRadialDamageEvent` and
  `FRadialDamageParams` (`Engine/DamageEvents.h`, UE's ids and `IsOfType`), `UDamageType`; `AActor::TakeDamage` (UE's
  signature, `InternalTakePointDamage` / `InternalTakeRadialDamage`, `bCanBeDamaged`) and the `OnTakeAnyDamage` /
  `OnTakePointDamage` / `OnTakeRadialDamage` delegates; `UGameplayStatics::ApplyDamage`, `ApplyPointDamage`,
  `ApplyRadialDamage` and `ApplyRadialDamageWithFalloff` (the dynamic bodies in reach, a line of sight on a channel,
  one hit per actor at its closest component); `APainCausingVolume` damages through `ApplyDamage` with its damage type.
- `UProjectileMovementComponent` (UE's: initial speed in local space, gravity scale, sweeps on the component's object
  type, bounces with bounciness and friction, `OnProjectileBounce` / `OnProjectileStop`, `MoveIgnoreActors`).
- `ASpectatorPawn`, `USpectatorPawnMovement`, `AGameModeBase::SpectatorClass`, the controller states
  (`NAME_Playing`, `NAME_Spectating`, `ChangeState`) and `APlayerController::BeginSpectatingState` /
  `SpawnSpectatorPawn`.
- `AActor::SetLifeSpan` / `InitialLifeSpan`; `UGameplayStatics::GetWorldFromContextObject`.
- Static mesh sockets (`UStaticMeshSocket`, `UStaticMesh::FindSocket`, `UStaticMeshComponent::GetSocketTransform`):
  the glTF import makes one of each `SOCKET_<Name>` node.
- The view model pass (`UPrimitiveComponent::bRenderAsViewModel`, `UCameraComponent::ViewModelFOV`: drawn after the
  scene over a cleared depth buffer with its own field of view), `bOnlyOwnerSee` / `bOwnerNoSee` against the view's
  actor, and world effects: `UGameplayStatics::SpawnImpactMark` (a pool of 64 marks), `SpawnTracer` and
  `SpawnPointLightAtLocation` (a light that destroys itself).
- **ShooterGame's weapons** (P18; [README](Game/ShooterGame/README.md#weapons)): `AShooterWeapon` (slots,
  ammunition, fire rate, reloads, equipping, the view model and the body's mesh, the `Muzzle` socket, dropping and
  picking up), `AShooterWeapon_Instant` (hitscan on the `Weapon` channel, Counter-Strike's range falloff, spread from
  the movement, the jump, the crouch and the firing, recoil, all from a seeded `FRandomStream`), the pistol (`usp`),
  the rifle (`ak47`), the sniper (`awp`: two zoom levels, the scope, the bolt) and `AShooterWeapon_Grenade`
  (`hegrenade`: `AShooterProjectile`, a 1.5 s fuse, radial damage behind cover).
- `AShooterCharacter`: health, armor and helmet (Counter-Strike's armor ratio), head and body hits, death (the best
  weapon dropped, the corpse, spectating for a player), an inventory with one weapon a slot and `DefaultWeapons`, and
  the keys Fire, Targeting, Reload, 1 / 2 / 4 and G; `UShooterCharacterMovement` applies the weapon's speed.
  `AShooterGameMode::CanDealDamage` (`bFriendlyFire`) and `Killed`; `AShooterPlayerController::NotifyHitConfirmed`;
  the HUD's health, armor, ammunition, hit marker and scope.
- ShooterGame's art: `SourceArt/Weapons/make_weapons.py` (four weapon meshes as glTF, with their muzzle sockets) and
  `SourceArt/Sounds/make_sounds.py` (eight synthesized sounds), imported to `/Game/Weapons` and `/Game/Sounds`; both
  use only Python's standard library and write the same bytes on every run.
- Tests: `System.Engine.Damage.*`, `System.Engine.ProjectileMovement.*`, `System.Engine.SpectatorPawn.*`,
  `System.Engine.Sockets.*` and `System.Engine.Actor.LifeSpan` (8), `System.Renderer.ViewModel.OnlyWhenFlagged` and
  `System.Renderer.Effects.ImpactMarkPoolRecycles`, `System.LeonEd.Factories.GltfSockets`,
  `System.CoreUObject.Config.SubclassConstructorDefaults` (383 tests; TestPAL 119, 113 on the PS2), and
  `ShooterGame.Damage.*`, `ShooterGame.Weapons.*`, `ShooterGame.Character.DeathDropsWeapon` (19).

- **ShooterGame's rounds** (P19; [README](Game/ShooterGame/README.md#rounds-money-and-the-bomb)):
  `AShooterGameMode` runs the match (the warmup, `ReadyToStartMatch`, `StartRound`, `EndRound`, `CheckRoundEnd`, the
  match's end at more than half of `MaxRounds`), fills the teams with bots (`bFillTeamsWithBots`), cleans the map and
  respawns the dead each round, and gives the bomb to a random terrorist from a seeded stream (`RandomSeed`, `?seed=`);
  `AShooterGameState` (the round's phase, number and end, the score, the bomb, the kill feed; `EShooterRoundState`,
  `EShooterBombState`, `EShooterRoundEndReason` and CS's messages).
- The money (CS 1.6, config): kill rewards, the team kill penalty, the win rewards, the loss bonus and its streak, the
  plant bonus, the planter's and the defuser's rewards; `AShooterPlayerState`'s money, kills and deaths.
- Buying (`AShooterGameMode::Buy`, `CanBuy`, `GetPrice`): the team's `BuyZone` volumes, the buy time, the weapons,
  kevlar, kevlar and helmet, the defuse kit; `AShooterPlayerController::Buy` and the buy menu (B, 1 to 7:
  `UShooterBuyMenuWidget`, a UMG widget).
- `AShooterBomb`: carried, dropped and picked up, planted by holding E in a `BombSite` for 3 s, beeping, exploding
  after 40 s (500 damage within 17.5 m, through walls), defused in 10 s (5 with a kit); the C4's mesh and its sounds
  (`make_weapons.py`, `make_sounds.py`).
- The HUD: the round's clock and the score, the money, the bomb and the kit, the kill feed, the round's messages, the
  plant and defuse bar, the scoreboard (kills, deaths, money) and a crosshair that opens with the spread.
- Console: `bot_kick`, `mp_restartgame`, `Buy`, and the cheats `give`, `god` and `kill`; the keys E (use), B (buy menu)
  and 1 to 7 (its items).
- Tests: `System.Engine.CharacterMovement.LongFramesKeepTheFloor` (384 tests), and `ShooterGame.Rounds.*`,
  `ShooterGame.Economy.*`, `ShooterGame.Bomb.*`, `ShooterGame.Buy.Rules`, `ShooterGame.HUD.RoundInfo` (28).
- **Waypoint navigation** (P20, Engine; [LEVELS.md — Waypoint links](Docs/LEVELS.md#waypoint-links)):
  `UNavigationSystem` builds a graph from the level's `ANavigationWaypoint` actors when the world begins play (each
  node on the floor below its waypoint) and finds paths with A* between the start's and the end's nearest reachable
  waypoints; `CanWalkBetween` (the agent's capsule swept between two points, the floor probed for gaps; steps, jumps
  and drops by `FWaypointLinkParams`), `FindFloorBelow`, `AutoLinkWaypoints`, UE's
  `FindPathToLocationSynchronously` and `UNavigationPath`; F3 draws the nodes and links.
- The map import's `bAutoLinkWaypoints` and `Waypoint*` settings (`UMapImportSettings`): the links an agent can walk
  are added and saved in the map; ShooterGame turns it on with CS's hull, and de_leon gets 26 links.
- AIModule: UE's typed blackboard (`SetValueAs*` / `GetValueAs*` for bool, int, float, vector, name and object keys,
  `IsValueSet`, `ClearValue`); `UPawnSensingComponent` (sight in a cone with a line of sight, hearing within a
  loudness-scaled range; `OnSeePawn`, `OnHearNoise`) and `AActor::MakeNoise`; `AAIController`'s path following jumps
  up a rise and repaths when stuck.
- **ShooterGame's bots** (P20; [README](Game/ShooterGame/README.md#bots)): `AShooterAIController` runs a behavior tree
  (buy, engage, defuse, plant, fetch the bomb, investigate a shot, the objective) with CS's aim model (reaction time,
  an aim error that settles, bursts, the AWP's zoom), `Difficulty` and the rest in
  `[/Script/ShooterGame.ShooterAIController]`; the terrorists' site is drawn each round from the seeded stream
  (`AShooterGameMode::GetTerroristTargetSite`); weapons make noise when they fire (`FireNoiseLoudness`); `bot_stop`.
- **Bot match** (P21; [README](Game/ShooterGame/README.md#bot-match)): `ShooterGame -botmatch [-rounds=N] [-seed=N]`
  (the local player spectates, bots fill the teams, the game exits after N rounds with 0, or 1 when an invariant
  broke or the rounds did not end in time) and `FShooterMatchChecker` (the round ends and the scores, the money, the
  team sizes, the pawns' health and floor); `Botmatch budget:` logs the reflected types, the UObjects' peak, the names
  and the heap. `BotMatch.bat [Rounds] [Seed]` plays it twice and compares the summaries; CI runs `BotMatch.bat 10 7`
  and, in a parallel job, the staged Shipping ShooterGame's bot match (`BuildCookRun.bat -configuration=Shipping ...
  -run`).
- `-benchmark` (`FApp::IsBenchmarking`): a headless game's fixed steps do not wait for the clock.
- `FPlatformMisc::RequestExitWithStatus` without `bForce` records its code (`GetRequestedEngineExitCode`), and
  `FEngineLoop::GetExitCode` returns it after a clean shutdown.
- Budgets.md: ShooterGame's reflection, UObjects, names and heap in a bot match, as the PS2 port's targets.
- Tests: `ShooterGame.Bots.MatchCheckerFlagsViolations` (34); `MatchOnDeLeon` runs under `FShooterMatchChecker`.
- Tests: `System.AIModule.Gameplay.NavigationAutoLinkStepsJumpsAndDrops`, `System.AIModule.Blackboard.TypedKeys`,
  `System.AIModule.PawnSensing.*`, `System.LeonEd.MapFactory.AutoLinksWaypoints` (386 tests), and `ShooterGame.Bots.*`
  (33), among them a three-round match of ten bots on de_leon with its invariants.

### Changed

- `UGameEngine::Tick` runs `UWorld::TickGameplayFrame`, and a character moves in its movement component's tick
  (`UCharacterMovementComponent::TickComponent`, after the controller's input): the frame is the actor tick, the pawn
  separation, the physics step, the overlaps and the sync. The engine's maps have no characters, so their frames are
  unchanged.
- `ECollisionChannel` is UE's enum (the old `enum class` values `WorldStatic`, `WorldDynamic`, `Pawn`, `Visibility`
  are `ECC_WorldStatic` & co.); a body added without a component keeps the old filter (a static body answered static
  and pawn / visibility queries, a dynamic one dynamic and pawn / visibility queries) through its default responses.
- `AHUD::Paint` calls `DrawHUD` before painting the widgets (UE's order).
- A project's `RequiredTags` no longer apply to the engine's `/Engine/` maps, so `-reimport -all` with a project
  reimports the engine content too.
- `RunTests.bat` also builds and runs ShooterGame's tests, `Lint.bat` builds ShooterGame's targets, and CI runs G6, G5
  with ShooterGame and a staged ShooterGame (BuildCookRun).
- Characters' capsules and components that do not affect navigation (`CanEverAffectNavigation`) stay out of the
  navigation data (P17's grid; P20's waypoint queries run on the Pawn channel, which characters ignore).
- `ACharacter` no longer has a health of its own (`Health`, `TakeDamage(float)`, `Die`, `Revive`, `IsAlive`): damage
  goes through `AActor::TakeDamage`, and a game's pawn keeps its health (UE; `AShooterCharacter`).
- ShooterGame's pawns spawn with their health (`PostInitializeComponents`), and `AShooterGameMode::CountPawns` counts
  the players' pawns (a pawn spawned during a world tick joins the level's list when the tick ends).
- The blackboard's keys are typed (UE): `SetBool` / `GetBool` became `SetValueAsBool` / `GetValueAsBool`.
- The navigation tests use waypoint graphs; the round and weapon tests keep the bots still.
- `AShooterGameMode::RestartPlayer` makes a player without a team a spectator; `-seed=N` sets `RandomSeed` too.
- Build.version 0.20.0; the engine, ThirdPerson and ShooterGame content resaved with it (the package summary records
  the engine version).
- ShooterGame's bodies no longer hide from their own player by visibility: they are `bOwnerNoSee`, so a player sees
  its own corpse.

### Removed

- The grid navmesh (`FNavMesh`, `NavMesh.h`) and the golden tables of its paths
  (`System.Engine.Golden.NavigationFindPath`, `System.AIModule.Golden.AIControllerArrives`): the waypoint graph replaces
  it.

### Fixed

- `AAIController` no longer steers toward a freed path point in the frame it repaths when stuck (a reference into the
  path outlived `RebuildPath`); the read made seeded bot matches diverge between runs.
- A native class's default object no longer copies its parent's config members over the values its own constructor
  set (UE: a native class's defaults are not initialized from its parent's); the config then applies the parents'
  sections and the class's own as before.
- A character stays on the floor through a long frame: the floor's trace starts where the feet began the frame's
  vertical step, not inside the floor (a 0.1 s frame sank characters into de_leon's 20 cm floor and 1 cm pads).
- The Renderer's includes of `GPUPassTimer.h` and `LDRColorTarget.h` match the files' case, and the Linux platform
  file includes `<stdio.h>` for `rename`, so the engine builds on Linux.

## [0.17.0] - 2026-09-26

The fourteenth to sixteenth steps of the Core / CoreUObject plan (P14, P15, P16): the engine's assets become UObjects
saved in `.lasset` packages, an editor module (LeonEd) imports source files into them through LeonCook's commandlets,
and the engine content is migrated to `/Engine` packages; the legacy `.lmesh` / `.lmat` formats and run-time image and
WAV loading are gone. Then the levels become `.lmap` map packages that `UEngine::LoadMap` opens, LeonEd imports glTF
scenes exported from Blender as maps, the level templates become `/Engine/Maps/Entry` and
`/Engine/Maps/Template_Default`, and the `.llev` levels, their reader and the legacy content tools are deleted. Last,
the cook targets a platform and cooks what the maps use, `.lpak` files hold a staged build's content and mount in the
platform file chain, and `BuildCookRun.bat` stages a Shipping game that reads nothing but its pak. Behaviour, the
golden tests and the Win64 frames are unchanged, and the staged Shipping build renders the same frame; the engine
content is resaved for 0.17.0 (a package records the engine version). The PS2 ThirdPerson ELF grows by 8 bytes of
alignment (P14) and 40 bytes of text (P16: `UObject::IsEditorOnly` and its slot in the 8 CoreUObject vtables it
links); BlankProgram is unchanged; TestPAL grows with PakFile, SHA-1 and their tests (+76 240 bytes of text; 112 tests
on the PS2).

### Added

- **Asset classes** (P14, Engine, UE 4.27 names and headers; [ASSET_FORMATS.md](Docs/ASSET_FORMATS.md#asset-classes)).
  - `UTexture` (`SRGB`, `UpdateResource`, `ReleaseResource`) and `UTexture2D` (`CreateTransient`, `FTexturePlatformData`
    with mip 0's texels as `FByteBulkData`, `EPixelFormat`).
  - `UStaticMesh`: one LOD of geometry (`FStaticMeshLODResources`, saved as bulk data), the bounds, `StaticMaterials`
    (`FStaticMaterial`) and a `UBodySetup` inner object (`FKAggregateGeom` boxes, `ECollisionTraceFlag`), which the
    physics scene follows.
  - `UMaterialInterface` / `UMaterial`: the material parameters and maps as `UPROPERTY`s with fixed shading models
    (`EMaterialShadingModel`: `MSM_DefaultLit`, `MSM_Unlit`), `GetRenderProxy`, `UMaterial::GetDefaultMaterial`.
  - `USkeleton` (`FReferenceSkeleton`, `USkeletalMeshSocket` sockets), `USkeletalMesh`, `UAnimationAsset` /
    `UAnimSequenceBase` / `UAnimSequence` (per-bone tracks as bulk data), `UBlendSpaceBase` / `UBlendSpace1D`.
  - `USoundBase` / `USoundWave` (PCM16 bulk data, channels, rate, duration), `UDataAsset` and the `UCommandlet` base
    (`Main`, `ParseCommandLine`).
  - `UAssetImportData` (`FAssetImportInfo`: the source file relative to the engine or project, its MD5, the import
    settings; no timestamps), an editor-only instanced `AssetImportData` on textures, meshes, clips and sounds.
- **The editor module `LeonEd`** (`Engine/Source/Editor`; LeonBuildTool's new `Editor` module type, desktop only and
  rejected in game targets; [TOOLS.md](Docs/TOOLS.md#leoncook)).
  - Factories: `UFactory`, `UTextureFactory` (PNG, JPEG, TGA, BMP), `UFbxFactory` (FBX and OBJ static meshes, FBX
    skeletal meshes and animations), `UGLTFImportFactory`, `USoundFactory` (PCM16 `.wav`), `UMaterialFactoryNew`, and
    the temporary `.lmat` / `.lmesh` factories of the migration (removed in P15). A mesh import makes `M_` materials and `T_` textures
    for the named slots of its source; importing over an asset reimports it in place.
  - Reimport: `FReimportHandler`, `FReimportManager`.
  - Commandlets: `ImportAssets` (`-source` / `-dest`, `-importlist=ImportList.ini`, `-reimport -all`),
    `ResavePackages`, `ValidateAssets`, `MigrateLegacyContent` (temporary, removed in P15) and a minimal `Cook` (saves without the
    editor-only data into `Saved/Cooked/<Platform>/`; the dependency walk, target platforms and paks come in P16).
- **LeonCook `-run=`**: `LeonCook [<Project>.lproj] -run=<Commandlet> [arguments]` (UE: `UE4Editor-Cmd`) finds the
  `U<Name>Commandlet` class through reflection; engine-only without a project; an error / warning summary at the end.
- **Engine content as packages**: `/Engine/EngineMaterials/M_Default`, `M_WorldGrid`, `M_SolidMetal`, `T_Default_D`,
  `T_Default_Bump_N`, `/Engine/EngineResources/DefaultTexture` and `/Engine/BasicShapes/Cube`, `Plane`, `Sphere`;
  `Engine/SourceArt/` holds `T_Default_D.png` and `ImportList.ini`.
- **Engine defaults from the config**: `[/Script/Engine.Engine] DefaultMaterialName`, `DefaultTextureName`,
  `DefaultBumpNormalTextureName` and the UI cue sounds `UIClickSoundName`, `UIConfirmSoundName`, `UIBackSoundName`,
  `UIErrorSoundName` (empty: the procedural tones), loaded by `UEngine::InitializeObjectReferences`.
- `UGameplayStatics::PlaySound2D` / `PlaySoundAtLocation` for sound waves; `FAudioDevice` plays PCM16 samples from
  memory (`FSoundWavePCM`, `SetUiSound`).
- **Gate G5**: `Engine/Build/BatchFiles/CheckReimport.bat` reimports the content and fails when git sees a change;
  CI runs it.
- `IRendererModule::ReleaseAssetResources`: the assets free the renderer's GPU copy when their data changes and in
  `BeginDestroy`.
- 23 new tests (340 in all): every asset class saved to a package and loaded back, the factories, the commandlets
  (import lists, reproducible reimport, resave, validation, cook, migration), the engine content, the content keys and
  the scene keeping its proxies' assets.
- **Maps** (P15; [LEVELS.md](Docs/LEVELS.md), [ASSET_FORMATS.md](Docs/ASSET_FORMATS.md#maps--lmap)): a world saves
  as a `.lmap` package (`PKG_ContainsMap`) with its persistent level, `AWorldSettings` and the actors with their
  components, and loads back through `LoadPackage` and `UWorld::FindWorldInPackage`; `UWorld::InitWorld` (public, with
  `InitializationValues::InitializeScenes`), `UpdateWorldComponents` and `InitializeActorsForPlay` register and
  initialize a loaded map's actors. `UEngine::LoadMap` opens `/Game/Maps/X` and `/Engine/Maps/X`, or a `.lmap` file
  (a file outside the mount points mounts the folder above its `Maps/` folder).
- **The glTF map importer** (P15): `UGLTFMapFactory` (`LeonCook -run=ImportAssets -type=Map -source=<file.glb>
  -dest=/Game/Maps/<Map>`) with its naming rules in `[/Script/LeonEd.MapImportSettings]` (`UMapImportSettings`; the
  new `Engine/Config/BaseEditor.ini`: `UCX_` convex collision as body setup boxes, `COL_` invisible collision, `Clip_`
  blocking volumes, `PlayerStart` with its `PlayerStartTag`, `NavWaypoint` with the extras' links and flags; a
  project's own rules and its `RequiredTags` check), the shared meshes and PBR materials next to the map,
  KHR_lights_punctual lights, deterministic reimport (gate G5 covers maps). MeshUtilities' `LoadGltfScene` reads the
  scene.
- `ACameraActor`, `ANavigationWaypoint`, `URotatingMovementComponent` (UE's), and Leon's `UBobbingMovementComponent`,
  `UOrbitMovementComponent` and `UInteractableComponent`: the homes of the legacy levels' camera framing, spin, bob,
  light orbit and trigger data (P15).
- `/Engine/Maps/Entry`, `/Engine/Maps/Template_Default` (migrated from the `.llev` templates) and `/Engine/Maps/AxisTest`
  (imported from `Engine/SourceArt/Maps/AxisTest.glb`, which `MakeAxisTest.py` writes with standard-library Python),
  the axes map (P15).
- `UObject::Rename` (UE's, without flags); the map world's editor-only `AssetImportData`.
- 8 new tests and 9 removed with the `.llev` format in P15 (339 in all): a map with every actor class saved and loaded
  back, `LoadMap` of packages and files, the movement components, the engine maps resaved byte for byte, the axes map,
  and the map importer (every convention, reproducible reimport, required tags, the config rules).
- **PakFile** (P16, a Runtime module for every platform; UE: PakFile;
  [ASSET_FORMATS.md](Docs/ASSET_FORMATS.md#paks--lpak)).
  - `.lpak` files: the entries' raw bytes, an index sorted by the CRC-32 of each lowercased path (with the path, offset,
    size and SHA-1 of every entry, and the mount point), and a 44-byte `FPakInfo` footer (magic `'LPAK'` 0x4B41504C,
    version, index offset and size, the index's SHA-1). No compression or encryption; an optional alignment for CD
    sectors.
  - `FPakFile` (opens a pak through the lower-level platform file or from memory, checks the footer and the index's
    hash, binary search on the path hash, `Check` of every entry's SHA-1) and `FPakPlatformFile` (UE's platform file
    wrapper: `ShouldBeUsed`, `Initialize`, `Mount` / `Unmount`, `GetPakFolders`, `FindFileInPakFiles`,
    `IsNonPakFilenameAllowed`; reads from the paks first, the highest order winning, files in a pak read-only,
    directories and stat data from the paks too).
  - `FPakWriter` / `FPakInputPair`: deterministic paks (data in path order, the mount point as the folder every path
    shares, no time recorded) and response files.
- **LeonPak** (P16; UE: UnrealPak): `LeonPak <out.lpak> -create=<response file> [-align=<bytes>]`, `LeonPak <in.lpak>
  -list | -test | -extract=<dir>`; Lint builds it.
- **TargetPlatform** (P16, a Developer module; UE: TargetPlatform): `ITargetPlatform` (`PlatformName`, `DisplayName`,
  `IniPlatformName`, `CookedPlatformName`, `HasEditorOnlyData`, `IsLittleEndian`, `RequiresCookedData`,
  `GetAllTextureFormats`, `GetAllWaveFormats`, `GetCookNote`) and `ITargetPlatformManagerModule`
  (`GetTargetPlatformManagerRef`) with Win64, the identity target, and PS2, a stub that cooks the Win64 formats and
  says the PS2 conversion (PSMT8 / PSMT4, `LPS2` v2, ADPCM) comes later.
- **BuildCookRun** (P16; UE: `RunUAT BuildCookRun`): `Engine/Build/BatchFiles/BuildCookRun.bat -project=<.lproj>
  -platform=Win64 [-configuration=Shipping|Development] -build -cook -stage -pak [-run] [-addcmdline="..."]` builds the
  game (a content-only project's is LeonGame), cooks, paks the cooked folder under the mount point `../../../` and
  stages `<Project>/Saved/StagedBuilds/Win64/` in UE's layout (`<Project>/Binaries/Win64/<Project>-Win64-Shipping.exe`,
  `<Project>/Content/Paks/<Project>-Win64.lpak`); `-run` starts the staged game. CI runs a Development staged build
  headless.
- `FPaths::IsStaged` (P16): a staged executable (paks beside its binaries, no engine sources in its build tree's engine
  folder) uses UE's `../../../Engine/` and the project folder above `Binaries/`, and reads its `.lproj` from the pak.
- `FSHA1` / `FSHAHash` (P16, Core; UE's `Misc/SecureHash.h`).
- `UObject::IsEditorOnly` (P16, UE's): `UAssetImportData` is editor-only, and a package that filters editor-only data
  leaves such objects (and what they own) out, saving the references to them as null.
- `Engine/Config/BaseGame.ini` (P16): `[/Script/UnrealEd.ProjectPackagingSettings] +DirectoriesToAlwaysCook` of
  `/Engine/BasicShapes`, which the engine loads by path.
- `UGameViewportClient::SetIgnoreInput` / `IgnoreInput` (P16, UE's).
- 11 new tests in P16 (350 in all): the pak round trip, deterministic paks, alignment, `-test` finding corruption, the
  pak platform file in the chain (and loose files refused), an engine asset loaded from a pak, the target platforms and
  the cook's seeds, the dependency closure, an imported map cooked twice to the same bytes without import data, SHA-1,
  and a viewport client ignoring the input. TestPAL runs the PakFile tests too, on the PS2 included.

### Changed

- The components hold their assets through `UPROPERTY`s: `UStaticMeshComponent::StaticMesh`,
  `USkeletalMeshComponent::SkeletalMesh`, `UMeshComponent::OverrideMaterials` (`UMaterialInterface*`; `GetMaterial`
  returns one). A slot without a material draws with the default material.
- The runtime loads the engine's assets from their packages (`LoadObject`); a sphere of another tessellation is built
  at run time (`GetSphereMesh`).
- MeshUtilities returns mesh data with its material slots (`FStaticMeshBuilder::BuildFromFile`) instead of writing
  `.lmesh` and `.lmat` files.
- `FGenericWindow::SetIconFromFile(PngPath)` is `SetIcon(Width, Height, RGBA)`: texels, not a file.
- The Renderer's `FRenderResourceCache` is keyed by asset instead of pinning shared pointers, and `FScene` is an
  `FGCObject` that keeps its proxies' assets alive.
- `UAnimInstance` and `UCharacterAnimInstance` moved from AnimationCore to Engine (`Classes/Animation`); AnimationCore
  keeps the plain skeletal data (`FReferenceSkeleton`, `FRawAnimSequence`, `FSkeletalMeshData`) and is no longer
  reflected; the animation tests are `System.Engine.Animation.*`.
- RenderCore's `Material.h` is `MaterialShared.h` and its lighting enum `EMaterialLightingModel` (UE's
  `EMaterialShadingModel` is Engine's); its texture maps are `UTexture2D*`.
- `FBasicShape` and `MeshForBasicShape` lose their resource cache parameter.
- Headless runs (`-nullrhi`, tests) load textures too.
- `.gitattributes` marks `.lpak` and `.glb` files binary and no longer lists `.llev` and `.lmesh`.
- `GameDefaultMap` is `/Engine/Maps/Template_Default` and `ServerDefaultMap` `/Engine/Maps/Entry` (P15).
- `EComponentMobility` and `ECollisionEnabled` are reflected enum classes (`ECollisionEnabled::Type` is
  `ECollisionEnabled`); a scene component's `Mobility` and a primitive's `CollisionEnabled`, `bSimulatePhysics` and
  `bEnableGravity` are `UPROPERTY`s, and a scene component saves the quaternion of its relative transform (P15).
- `UWorld::PersistentLevel` and `ULevel::WorldSettings` are saved; `ULevel::PostLoad` reconnects the level to its
  world (P15).
- `AGameModeBase::ChoosePlayerStart` takes the level's first player start (P15).
- The renderer loads its shaders from `Engine/Shaders` (`FPaths::EngineDir()`), UE's `/Engine/Shaders` (P15).
- `FLegacyCoordinateConversion` is test only (RenderCore's `Public/Tests` / `Private/Tests`), and `CheckBannedApis.ps1`
  (G4) allows it in test folders only (P15).
- `ImportAssets` takes `-type=Map` (the destination is the map's package) and no longer `-type=Material`;
  `FAssetImportUtils::SavePackage` saves a package that holds a world as `.lmap` (P15).
- **The cook** (P16, `-run=Cook -TargetPlatform=Win64|PS2`; [TOOLS.md](Docs/TOOLS.md#the-cook)) cooks by the book
  instead of every package: seeds from the maps (`-map=`, `+MapsToCook`, else every map under `/Game/Maps`),
  `+DirectoriesToAlwaysCook` of `[/Script/UnrealEd.ProjectPackagingSettings]` and every package the target platform's
  Engine and Game config name by path (`GameDefaultMap`, `ServerDefaultMap`, `DefaultMaterialName`, ...); the
  dependency closure over the packages' tables (hard imports and soft package references, nothing loaded); cooked
  packages with `PKG_FilterEditorOnly | PKG_Cooked` and the target's name in `<Project>/Saved/Cooked/<Platform>/`
  (`Engine/Content/...`, `<Project>/Content/...`), without the assets' and the worlds' import data; the config (not the
  Editor ini), the shaders and the `.lproj` staged beside them; an emptied output folder and the same bytes every time.
- `UPackage::Save` / `SavePackage` / `SaveToMemory` take the cooked platform's name (P16): a cooked package records the
  cook's target platform instead of the running one.
- `FEngineLoop::PreInit` puts `FPakPlatformFile` on top of the platform file chain before the config loads, when the
  build has paks in `<Project>/Content/Paks/` or `Engine/Content/Paks/` (or with `-pak`; outside Shipping `-NoPak`
  never), and always in Shipping (P16, UE's `LaunchCheckForFileOverride`). A Shipping build refuses loose files (except
  under `Saved/`) and stops without a pak; the desktop Launch links PakFile, the PS2 one does not yet.
- The engine content is resaved with engine version 0.17.0 (P16).

### Fixed

- A frame capture no longer depends on the mouse (P16): a `-Screenshot=` or `-ExitAfterFrames=` run is unattended
  (`FApp::IsUnattended`, like `-unattended`), and `UGameEngine::Init` makes the viewport client ignore the OS input
  (`SetIgnoreInput`), so the keys and the mouse cannot move the view during a capture (one P15 capture differed by 441k
  pixels).

### Removed

- The `Developer/Cooker` module (the cook commandlet moved to LeonEd), the cook recipes (`FCookRecipe`, `FCookPaths`)
  and LeonCook's `staticmesh` / `recipe` modes.
- `LeonMeshFormat` (`.lmesh`) and `LeonMaterialFormat` (`.lmat`) in RenderCore, the engine's `.lmat` files, and
  run-time image and WAV loading: `stb_image` only decodes in LeonEd's texture factory, and ApplicationCore no longer
  depends on STB.
- `FResourceCache` (and `UEngine::GetResources`), and the transitional `FLegacyAssetLoader` that replaced it during
  P14; `MaterialAsset.h` with the JSON material fields (`PatchMaterialFromJson`, `HasMaterialSurfaceFields`) and its
  test; Engine's dependency on Json.
- `FAudioDevice`'s file-path playback (`PlaySound2D` / `PlaySoundAtLocation` / `PlayMusic` by path, the UI `.wav`
  lookup).
- The `.llev` levels (P15): `LeonLevelFormat` (reader and saver), `LevelLoader`, `ULegacyLevelDataComponent`,
  `FLegacyAssetKeys` / `ResolveLevelAssetObjectPath`, `APlayerStartPIE`, `Engine/Content/LevelTemplates/` and
  `FPaths::ResolveLegacyContentPath`; LeonEd's `ULegacyMaterialFactory`, `ULegacyStaticMeshFactory` and
  `MigrateLegacyContent`; the `.llev` format, content key and migration tests.

## [0.16.0] - 2026-09-25

Twelfth and thirteenth steps of the Core / CoreUObject plan (P12, P13): the gameplay framework becomes UObjects, owned
through the world and the game instance and freed by the garbage collector at safe points; Leon code builds without
RTTI or C++ exceptions. Levels become actors (static meshes, player starts, volumes, lights, target points and the
world settings), physics bodies come from the components, and the Engine talks to the Renderer only through UE's
render boundary: `FSceneInterface` and scene proxies, `IRendererModule`, `FSceneView` and `FCanvas`. The game then
starts as a UE 4.27 game does: `FEngineLoop` creates `GEngine` (a `UGameEngine` UObject), the game instance opens the
startup map with `UEngine::Browse` / `LoadMap` and picks its game mode with UE's precedence, the local player logs in
through the game mode, input comes from `BaseInput.ini` into the player controller (`FKey`, `UInputSettings`,
`UPlayerInput`, `UInputComponent`), and `UGameViewportClient` routes the input and the console commands and draws the
frame. The PS2 game boots the object system (InputCore's `FKey` is reflected). Behaviour, the golden tests, the
`.llev` bytes and the Win64 frames are unchanged.

### Added

- **The engine object and maps** (P13; plan decision D18).
  - `UEngine` / `UGameEngine` are UObjects (`Config=Engine`) and `GEngine` is the engine: `Init(IEngineLoop*)`,
    `Start`, `Tick`, `PreExit`, `DeferredCommands` / `TickDeferredCommands`, `Exec`, `Browse`, `LoadMap`,
    `SetClientTravel` / `TickWorldTravel`, `ConditionalCollectGarbage`, `AddOnScreenDebugMessage`, `GetWorldContexts`,
    `GameViewport`, `LocalPlayerClassName`, `GameViewportClientClassName`; `IEngineLoop` (`UnrealEngine.h`).
    `FEngineLoop::Init` creates `GEngine` from `[/Script/Engine.Engine] GameEngine=`.
  - `FURL` (`Map?Option=Value#Portal`) and `EngineBaseTypes.h` (`ETravelType`, `EBrowseReturnVal`, `EInputEvent`,
    `EMouseCaptureMode`).
  - `UEngine::LoadMap`: the players leave their controllers, the old world's actors end play (`LevelTransition`), the
    world is destroyed and the garbage collected; the new world (named after the map) loads the `.llev`, gets its game
    mode (`UWorld::SetGameMode(FURL)`), initializes its actors for play (`InitGame`), logs every local player in
    (`ULocalPlayer::SpawnPlayActor` → `UWorld::SpawnPlayActor`) and begins play (`UWorld::BeginPlay`).
  - The game mode is chosen as in UE (`UGameInstance::CreateGameModeForURL`): `?game=` (or an alias of
    `GameModeClassAliases`), the level's `AWorldSettings::DefaultGameMode`, `GameModeMapPrefixes`,
    `GlobalDefaultGameMode`, else `AGameModeBase`.
  - UE's login and restart flow on `AGameModeBase`: `Login`, `InitNewPlayer`, `UpdatePlayerStartSpot`, `PostLogin`,
    `GenericPlayerInitialization`, `InitializeHUDForPlayer`, `HandleStartingNewPlayer`, `RestartPlayer`,
    `RestartPlayerAtPlayerStart`, `FindPlayerStart` / `ChoosePlayerStart`, `SpawnDefaultPawnFor`,
    `FinishRestartPlayer`; `APlayerStartPIE` (the level's camera framing is the Play From Here start); `UPlayer`,
    `ULocalPlayer`; `UGameInstance::StartGameInstance`, `CreateInitialPlayer`, `AddLocalPlayer` / `RemoveLocalPlayer`.
  - The `EngineSettings` module: `UGameMapsSettings` (`GameDefaultMap`, `ServerDefaultMap`, `GlobalDefaultGameMode`,
    `GameInstanceClass`, `LocalMapOptions`, `GameModeMapPrefixes`, `GameModeClassAliases`) and
    `UGeneralProjectSettings`, both `UPROPERTY(Config)` classes.
- **Input by config** (P13, InputCore and Engine).
  - `FKey` is a `USTRUCT` named by an `FName` (config text `Key=SpaceBar`) with `FKeyDetails`; `EKeys` holds the keys
    and is initialized by `FInputCoreModule`. InputCore depends on CoreUObject.
  - `UInputSettings` reads `[/Script/Engine.InputSettings]` of `BaseInput.ini` (`ActionMappings`, `AxisMappings`,
    `AxisConfig`, `DefaultViewportMouseCaptureMode`, `DefaultPlayerInputClass`, `DefaultInputComponentClass`);
    `UPlayerInput` keeps the key state and runs the input stack (`ProcessInputStack`, the `AxisConfig` sensitivity,
    `DebugExecBindings`); `UInputComponent` has UE's `BindAction` and `BindAxis`.
  - `APlayerController` creates its `UPlayerInput` (`InitInputSystem`), its input component, its
    `APlayerCameraManager` (`FMinimalViewInfo`) and its `AHUD` (`ClientSetHUD`, `MyHUD`); `APawn` has
    `SetupPlayerInputComponent`, `AddMovementInput` and `FaceRotation`, and a possessed pawn ticks after its controller.
  - `ADefaultPawn` with `UFloatingPawnMovement` (800 cm/s) is the default pawn: mouse look, WASD and the arrows fly
    along the view, E / Q up and down.
- **The viewport client and the console** (P13).
  - `UGameViewportClient` (`GameViewportClientClassName`) and the main window's `FViewport` (`UnrealClient.h`): the
    keys and the mouse go to the first local player's controller; `Draw` renders the view family, the HUD and the
    on-screen text; screenshots are `FScreenshotRequest`s.
  - Console commands take UE's chain: `ULocalPlayer::Exec` → the viewport client (`show <Flag>`) → the game instance
    → `UEngine::Exec` (`exit`, `obj gc`, `stat unit` / `stat fps`, `RecompileShaders`, `open <map>`) →
    `FSelfRegisteringExec` → the player input, the controller's `Exec` functions (`FOV`), the pawn, the game mode, the
    game state and the world settings. `-ExecCmds="Cmd;Cmd"` queues commands for the first frame; F1–F6 are
    `DebugExecBindings` of `BaseInput.ini`. `FEngineShowFlags` gains `Collision` and `Navigation`.
  - `RHIInit` / `RHIExit` (RHI): `FEngineLoop::PreInit` starts the RHI on the main window's context.
  - `System.Engine.URL.*`, `.EngineSettings.*`, `.LoadMap.*`, `.Input.*` and `.Console.ExecChain`: 318 tests.
- **Levels as actors** (P13, Engine; plan decision D16 for the volumes).
  - `AStaticMeshActor` (root `StaticMeshComponent0`), `APlayerStart` (`CollisionCapsule` 40 / 92, `PlayerStartTag`),
    `ATargetPoint`, `AVolume` (a `UBoxComponent` brush, `EncompassesPoint`, `GetBrushBounds`), `ATriggerVolume`,
    `ABlockingVolume` (blocks by default), `APainCausingVolume` (`bPainCausing`, `DamagePerSec`, `PainInterval`),
    `ALight` / `ADirectionalLight` / `APointLight` with `ULightComponentBase`, `ULightComponent`,
    `ULocalLightComponent`, `UDirectionalLightComponent` and `UPointLightComponent`, and `AWorldSettings`
    (`DefaultGameMode`, `KillZ`; `ULevel::WorldSettings`).
  - The `.llev` reader spawns them (`AWorldSettings` first, then one actor per record in file order, then the lights:
    at most 2 directional and 4 point lights, a default sun when there is none) and the saver writes the same bytes
    back from the actors (`System.Engine.LevelFormat.SaveWritesTheSameBytes`). `ULegacyLevelDataComponent` keeps the
    record fields that have no UE counterpart yet (mesh and material paths, spin, bob, interaction data, the level
    name, the game mode name and the camera framing).
  - `EComponentMobility` and `USceneComponent::SetMobility`; `ECollisionEnabled` and
    `UPrimitiveComponent::SetCollisionEnabled` / `SetSimulatePhysics` / `SetEnableGravity`;
    `FRotationConversionCache` (a component's rotation keeps the exact quaternion it was given).
  - `UGameplayStatics::GetAllActorsOfClass` / `GetAllActorsWithTag`.
- **Physics state from the components** (P13). `UPrimitiveComponent::CreatePhysicsState` adds a body to the world's
  physics scene when collision is on and `DestroyPhysicsState` removes it; `UActorComponent::RecreatePhysicsState`;
  `FPhysScene::AddComponentBody` / `RemoveComponentBody` / `GetBodyOwner` / `SyncComponentsToBodies` /
  `RebuildRigidWorld`.
- **The render boundary** (P13, Engine and Renderer).
  - Engine headers: `SceneInterface.h` (`FSceneInterface`), `PrimitiveSceneProxy.h`, `StaticMeshSceneProxy.h`,
    `SkeletalMeshSceneProxy.h`, `LightSceneProxy.h`, `RendererInterface.h` (`IRendererModule`, `GetRendererModule`,
    `FFrameStats`), `SceneView.h` (`FEngineShowFlags`, `FSceneViewFamily`, `FSceneView`, `FSceneViewInitOptions`)
    and `CanvasTypes.h` (`FCanvas`: tiles, lines and text batched by depth sort key, `Flush_GameThread`).
  - Components create their scene proxies (`CreateSceneProxy`, `CreateRenderState_Concurrent`,
    `MarkRenderStateDirty`, `SendRenderTransform_Concurrent`, `SendRenderDynamicData_Concurrent`) in `UWorld::Scene`,
    which `UWorld::InitWorld` allocates through `IRendererModule::AllocateScene` (null in headless runs);
    `UWorld::SendAllEndOfFrameUpdates` before each frame; `UWorld::LineBatcher`.
  - The Renderer implements `FRendererModule` and `FScene` (primitives and lights in level order) and owns the GPU
    copies of the CPU assets (`FRenderResourceCache`: static and skeletal mesh buffers, textures) and the canvas pass.
  - `FModuleManager::GetModulePtr` / `LoadModuleChecked`; `System.Engine.Components.SceneProxiesFollowTheComponents`.
- **Gameplay framework as UObjects** (Engine, AIModule, UMG, AnimationCore; plan decisions D11, D12).
  - `UCLASS` types with `UPROPERTY` members: `AActor`, `AInfo`, `UActorComponent`, `USceneComponent`, `APawn`,
    `ACharacter`, `AController`, `APlayerController`, `AAIController`, `AGameModeBase`, `AGameStateBase`,
    `APlayerState`, `AHUD`, `UGameInstance`, `UWorld`, `ULevel` (`UPROPERTY() TArray<AActor*> Actors`),
    `UPlayerInput`, `USkeletalMeshComponent`,
    `UCameraComponent` (now a scene component), `USpringArmComponent`, `UUserWidget` and the UMG widgets,
    `UAnimInstance` / `UCharacterAnimInstance`.
  - New components: `UPrimitiveComponent` (render state in the world's primitive list, `GetCollisionShape`),
    `UShapeComponent`, `UCapsuleComponent`, `UBoxComponent`, `USphereComponent`, `UMeshComponent`,
    `UStaticMeshComponent` (a mesh and its materials; attached to a bone socket it replaces
    `FSkelMeshAttachment`), `UMovementComponent`, `UPawnMovementComponent`; `UCharacterMovementComponent` is a
    component holding the tunables.
  - `ACharacter`'s default subobjects: the root `UCapsuleComponent` (`CollisionCylinder`), the movement
    (`CharMoveComp`) and the mesh (`CharacterMesh0`); `GetCapsuleComponent`.
  - `AGameMode` with UE's `MatchState` (`MatchState::EnteringMap` … `Aborted`, `StartPlay`, `StartMatch`,
    `EndMatch`, `AbortMatch`, `OnMatchStateSet` and the `Handle*` hooks) and `AGameState` (`MatchState`,
    `PreviousMatchState`, `ElapsedTime`); `AGameModeBase` gains `GameStateClass`, `PlayerControllerClass`,
    `PlayerStateClass`, `DefaultPawnClass`, `HUDClass` and spawns its game state; controllers spawn their player
    state (`InitPlayerState`).
  - `UWorld::CreateWorld` / `DestroyWorld` (transient `/Temp/Untitled_<N>` package, root set), `SpawnActor(Class,
    Location, Rotation, FActorSpawnParameters)` and the `SpawnActor<T>` / `SpawnActorDeferred` templates with UE's
    spawn sequence (components registered, `PreInitializeComponents`, `InitializeComponents`,
    `PostInitializeComponents`, `BeginPlay`), `DestroyActor` (`Destroyed`, `EndPlay(EEndPlayReason)`, components
    unregistered, removed from the level, pending kill), `SetGameMode`, `GetGameState`, `EWorldType`,
    `ESpawnActorCollisionHandlingMethod`; `FWorldContext` owned by `UGameInstance` (`InitializeStandalone`).
  - Scene components: `SetupAttachment`, `AttachToComponent(Parent, FAttachmentTransformRules, SocketName)`,
    `DetachFromComponent(FDetachmentTransformRules)`, `GetSocketTransform` / `DoesSocketExist` (a skeletal mesh's
    bones are sockets), `GetComponentToWorld`, world-space setters, visibility (`Engine/EngineTypes.h`: attachment
    rules, `EEndPlayReason`, `EWorldType`).
  - Garbage collection at safe points (D11): after the world teardown (`UEngine::LoadMap`, `UGameEngine::PreExit`),
    after a level load, and `UEngine::ConditionalCollectGarbage` after the world tick through
    `FGarbageCollectionTimer` (`gc.TimeBetweenPurgingPendingKillObjects`); `LogSpawn`, `LogWorld`.
  - `FScopedTestWorld` for tests; reflected test fixtures in `Engine/Private/Tests/EngineTestTypes.h` and
    `AIModule/Private/Tests/GameplayTestTypes.h`; 15 new tests (`System.Engine.World.*`, `.Components.*`,
    `.GameFramework.*`).

### Changed

- The level POD `UStaticMeshComponent` is renamed `FLevelStaticMesh`; `ULevel::GetName` / `SetName` became
  `GetLevelName` / `SetLevelName` (the object name is `PersistentLevel`).
- Ownership: the game world belongs to the game instance's world context instead of `AGameModeBase`; the world spawns
  the game mode (`UWorld::SetGameMode`). Since P13 `GEngine` (in the root set) owns the game instance and the viewport
  client; the player controller owns its `UPlayerInput`, `AHUD` and camera manager.
- Actors: the actor transform is the root component's; components are default subobjects or `NewObject` +
  `RegisterComponent` (the actor-level `CreateDefaultSubobject<T>(Args...)` and `RegisterComponent(Component)` are
  gone); `Destroy` goes through `UWorld::DestroyActor` and `Destroyed()` replaces the virtual `Destroy()`;
  `EndPlay` takes an `EEndPlayReason`; `GetRootComponent()` returns a pointer.
- `Cast<>` replaces every `dynamic_cast`; `AHUD::AddWidget<T>()` and `USkeletalMeshComponent::SetAnimInstance<T>()`
  create UObjects (no constructor arguments).
- **No RTTI and no C++ exceptions** in Leon code on every platform (D17): MSVC `/GR-`, no `/EH`, `_HAS_EXCEPTIONS=0`;
  GCC / Clang `-fno-rtti -fno-exceptions`. CMake's MSVC defaults are stripped from `CMAKE_CXX_FLAGS`; third-party C++
  that needs them gets them back with `leon_third_party_cxx_defaults` (tinyobjloader); Jolt keeps its own flags.
- LeonBuildTool: every reflected module also has a `LeonHeaderTool.<Module>` target, which modules with a circular
  dependency on it wait for (UMG waits for Engine's reflected headers).
- Levels (P13): `ULevel` holds only its actors and `AWorldSettings`; `GetAllActorsOfClass<APlayerStart>` replaces the
  player start snapshots, the volume helpers and the navigation read the volume and static mesh actors, and
  `AGameModeBase::FindPlayerStart` returns an `APlayerStart`. Physics bodies and hits carry a `ComponentID` (the
  component's `GetUniqueID`) instead of a level mesh index (`IgnoreComponentID`, `NoComponentID`); the world keeps
  dynamic bodies' components in step (`SyncComponentsToBodies`) instead of copying the level to and from the physics
  scene.
- Assets (P13): `UStaticMesh`, `USkeletalMesh` and `UTexture2D` (now `Engine/StaticMesh.h`, `Engine/SkeletalMesh.h`,
  `Engine/Texture2D.h`), `FResourceCache`, the material assets and `FDebugDraw` / `FDebugOverlay` move from the
  Renderer to Engine as CPU data; their GPU buffers and textures live in the Renderer's cache.
  `FResourceCache::SetGpuUploadEnabled` is `SetTextureLoadingEnabled`. Their messages log as `LogEngine`, and the
  `.lmat` reader's as `LogLeonMaterial` (both were `LogRenderer`).
- Drawing (P13): `UGameViewportClient::Draw` renders a view family through `IRendererModule::BeginRenderingViewFamily`
  and draws the HUD and the debug text through a frame `FCanvas`; UMG's `FPaintContext`, `AHUD::Paint` and
  `FDebugOverlay::Draw` take an `FCanvas`. The show flags (`show Bounds` on F1, `show AxesGizmo` on F6) are the
  viewport client's `FEngineShowFlags`. `EShaderReloadResult` / `MergeShaderReload` move to RenderCore's
  `ShaderCore.h`, `MakeReflectMatrix` / `FitLightSpaceMatrix` to `ViewMatrices.h`, and the `.lmat` document and
  writer to RenderCore's `LeonMaterialFormat.h`.
- Module graph (P13): Engine no longer depends on the Renderer or includes its headers; the Renderer depends on
  Engine; Launch links both. Engine depends on UMG publicly and UMG on Engine circularly (it was the other way round).
  Engine depends on EngineSettings, InputCore on CoreUObject, and ApplicationCore no longer on the desktop RHI
  (Launch links OpenGLDrv).
- Launch (P13): `FEngineLoop` does the whole boot on desktop too: `PreInit` creates the application, the main window
  and the RHI (`RHIInit`; the window no longer creates it); `Init` creates `GEngine` and queues `-ExecCmds=`; `Tick`
  pumps the events, runs the deferred commands and `GEngine->Tick` (windowed frames at most 0.1 s, headless fixed
  steps at `-tick=` Hz); `Exit` calls `PreExit`, collects the garbage and releases the RHI. A headless run honours
  `-ExitAfterFrames=` too.
- `LeonGame` (P13): the map is the first argument (`LeonGame /Engine/LevelTemplates/Blank`) or `-map=`, a long package
  name (`GameDefaultMap=/Engine/LevelTemplates/Starter`), a `.llev` path or a content key, with URL options
  (`?game=<class>`); a map that cannot be opened exits with code 1. The level's game mode string selects the game mode
  (`Default` is the project's default).
- Input (P13): the mouse look is applied once at 0.3° per pixel (`AxisConfig` sensitivity) instead of twice at 0.15°,
  so it turns as fast as before; the look and move axes are summed without clamping (UE). The keys are `FKey`s in the
  window, the PS2 input interface, the PS2 stats overlay and ThirdPerson.
- The PS2 `ThirdPerson` links CoreUObject through InputCore and starts the object system (8 192 slots, 96 KB): its
  text grows by 223 624 bytes ([Budgets.md](Engine/Platforms/PS2/Documentation/Budgets.md)).

### Removed

- `FSkelMeshAttachment` and `USkeletalMeshComponent::AddAttachment` / `GetAttachments` / `GetAttachmentWorldMatrix`
  (unused; attach a `UStaticMeshComponent` to a bone socket instead); `ACharacter::SetCharacterMovement` (the
  movement is a component: edit `GetCharacterMovement()`); `UWorld::SubmitSkeletalDraws` (`SubmitPrimitiveDraws`).
- P13: `FLevelStaticMesh` and the level's snapshots (meshes, player starts, trigger, pain and blocking volumes,
  lights, the camera; `ULevel::Npos`), `AActor::LevelMeshIndex` / `SyncTransformToLevel`,
  `UWorld::RegisterBodiesFromLevel`, `FPhysScene::SyncFromLevel` / `SyncToLevel`, `FWorldGameplayFrameParams::Level`,
  `UWorld::Primitives` / `AddPrimitive` / `SubmitPrimitiveDraws` and every `SubmitDraw`, `UGameEngine::GetRenderer`
  and the Renderer's public `SceneRenderer.h`.
- P13: `ADefaultGameMode`, `ADefaultPlayerController` and `ADefaultCameraActor` (replaced by `AGameModeBase` with
  `ADefaultPawn`), the game mode's `OnEnter` / `Tick(Engine)` / `OnExit` hooks, `UInputMappingContext`,
  `Leon::InputActions`, `FPlayInputTarget`, `RuntimeInput.h` and the input mapping tests, the desktop
  `FGameApplication` (`Launch/Private/Desktop/GameApplication.*`), `FGenericWindow::InitRHI` / `ReleaseRHI`, the
  engine-owned HUD, camera and player input, and `UGameEngine::Initialize` / `InitializeHeadless` / `Shutdown` /
  `Render` / `HandleInput` and its debug toggles (`show` and the Exec chain replace them).

## [0.15.0] - 2026-09-25

Eighth to eleventh steps of the Core / CoreUObject plan (P8–P11): LeonHeaderTool, the UnrealHeaderTool counterpart,
with its LeonBuildTool step; CoreUObject, the `UObject` runtime its generated code runs on; garbage collection,
references, `UPROPERTY(Config)` and `UFUNCTION(Exec)` on it; and packages: UObjects saved to and loaded from
`.lasset` / `.lmap` files. No engine module is reflected yet: the gameplay classes become UObjects in P12, and the
engine's assets and maps become packages in P14 / P15.

### Added

- **Packages** (P11, CoreUObject; plan decision D13; layout in `Docs/ASSET_FORMATS.md`).
  - `.lasset` / `.lmap` files: an `FPackageFileSummary` starting with `'LEON'` (format version `ELeonPackageVersion`
    from Core's `UObject/ObjectVersion.h`, package flags, table offsets, GUID, saving engine version, cooked platform,
    bulk data offset), a sorted name table, the import and export tables (`FPackageIndex`, `FObjectImport`,
    `FObjectExport`), the soft package references, each export's data and the bulk data at the end. One file per
    package: no `.uexp` / `.ubulk`.
  - Saving: `UPackage::SavePackage` / `Save` / `SaveToMemory` through `FLinkerSave`. The exports are `Base`, the
    objects with the top-level flags and, recursively, their outers, inner objects (default subobjects) and the
    package objects they reference; other packages' objects become imports (classes are `/Script/<Module>` imports),
    transient objects are left out and references to them saved as null. Deterministic: the same objects give the
    same bytes on every run and platform (sorted tables, GUID derived from the package name, no timestamps); a golden
    hash checks it on Win64 and the PS2.
  - Loading: `LoadPackage`, `LoadObject<T>`, `StaticLoadObject`, `LoadClass<T>`, `StaticLoadClass`, `FindPackage`,
    `ResetLoaders`, through `FLinkerLoad`. Synchronous: exports are created with `StaticConstructObject` (default
    subobjects are the ones the outer's constructor built, D12) and serialized over their class defaults; an import
    of another package loads it first (circular references work); a missing import is a warning and a null
    reference; a missing or damaged package is an error. `PostLoad` runs at the outermost `EndLoad`, once every
    object of the load is serialized (imported packages first); `RF_NeedLoad`, `RF_NeedPostLoad`, `RF_WasLoaded` and
    `RF_LoadCompleted` are maintained. `FSoftObjectPath::TryLoad` and `TSoftObjectPtr::LoadSynchronous` load
    packages.
  - Tagged properties: `FPropertyTag`, `UStruct::SerializeTaggedProperties` / `SerializeBin`,
    `UScriptStruct::SerializeItem`, and `FProperty::SerializeItem` for every property type (containers, nested
    structs, bitfield bools in the tag, enums by name, object references as `FPackageIndex`, soft paths). Properties
    are saved as a delta against the archetype. Schema evolution: unknown tags are skipped by size, and
    `FProperty::ConvertFromType` converts integers, `float` / `double`, bytes to enums (by value or enumerator name),
    names / strings / text and hard to soft references; anything else is skipped with a warning.
  - `UObject::Serialize` (tagged properties, then the class's native data), `SerializeScriptProperties`,
    `ConditionalPostLoad`, `IsAsset`; `TStructOpsTypeTraits::WithSerializer`; `DECLARE_SERIALIZER`; `RF_Load`,
    `ELoadFlags`, `ESaveFlags`.
  - `FByteBulkData`: payloads stored after the exports (or inline), loaded eagerly.
  - `FPackageName`: long package names, `/Engine/` and `/Game/` mount points plus `RegisterMountPoint`, `/Script/`
    packages, `.lasset` / `.lmap`, file conversions, `DoesPackageExist`. `FLinkerLoad::RegisterInMemoryPackage` lets
    `LoadPackage` read packages from memory (the tests; TestPAL on the PS2).
  - Editor-only data (D14): `PKG_FilterEditorOnly` packages drop `#if WITH_EDITORONLY_DATA` properties; builds
    without editor-only data mark every package they save so, and skip the editor-only tags of an uncooked package.
  - 14 `System.CoreUObject.Package.*` tests: `LeonAutomationTests` runs 293 tests, TestPAL 112 on Win64 and 106 on
    the PS2, which also logs the cost of a 50-object round trip (`Budgets.md`).
- **Core archive hooks** (P11). `FArchive` gains virtual `operator<<(UObject*&)` and `GetLinker()` (no-ops in plain
  archives, as in UE) and `UEVer()` / `LicenseeUEVer()`; `FEngineVersion` in `Misc/EngineVersion.h`.
- **Garbage collection** (P10, CoreUObject `UObject/GarbageCollection.h`, `GCObject.h`; plan decision D11).
  - `CollectGarbage(KeepFlags, bPerformFullPurge)`, `TryCollectGarbage`, `IsGarbageCollecting`,
    `IncrementalPurgeGarbage`: UE4's stop-the-world mark and sweep. Roots are the root set (`AddToRoot`), native
    objects, class default objects and their subobjects, the compiled-in packages, `KeepFlags` objects and `FGCObject`
    holders; the mark follows outers, each class's strong reference properties (`UObject*`, `TSubclassOf`, and
    arrays / sets / maps / structs of them) and `AddReferencedObjects`.
  - UE 4.27 pending kill: `MarkPendingKill` objects are collected even while referenced, and the references to them
    are cleared.
  - Destruction: `BeginDestroy` (out of the name hash) → `IsReadyForFinishDestroy` → `FinishDestroy` → destructor;
    the freed `GUObjectArray` slot is reused and old weak pointers stay stale.
  - `FGCObject`, `FReferenceCollector`, `TStrongObjectPtr`, `UObject::AddReferencedObjects` (per class, through
    `IMPLEMENT_CLASS`), `LogGarbage` statistics, and `FGarbageCollectionTimer` with
    `[/Script/Engine.GarbageCollectionSettings] gc.TimeBetweenPurgingPendingKillObjects`. The engine calls it from
    P12 / P13 (`LoadMap`, round restart, timer).
- **References** (P10). `TWeakObjectPtr` is complete (`Get(bEvenIfPendingKill)`, `IsStale`, `IsExplicitlyNull`,
  comparisons, hashing). `FSoftObjectPath` has UE 4.27's layout and API (`GetLongPackageName`, `GetAssetName`,
  `ResolveObject`, `TryLoad`) and, with `FSoftClassPath`, is a reflected noexport struct whose text form is the path.
  `TPersistentObjectPtr`, `FSoftObjectPtr::LoadSynchronous`, `TSoftObjectPtr` / `TSoftClassPtr` (they load packages
  since P11).
- **Config members** (P10). `UObject::LoadConfig` / `SaveConfig` / `ReloadConfig` for `UCLASS(Config=…)` with
  `UPROPERTY(Config)` and `UPROPERTY(GlobalConfig)`, in the section `/Script/<Module>.<Class>`. Arrays follow the
  layers' `+ - . !` edits (or `Key[N]=`), C arrays read `Key[N]=`, and structs, enums, names, objects, classes and soft
  paths are parsed as text. A class default object loads its config when it is created, reading its parents' sections
  first, and instances copy it. `PerObjectConfig` classes read one section per object. `SaveConfig` writes the
  desktop user layer (`Saved/Config`); on the PS2 it only logs.
- **Console commands** (P10). `UObject::CallFunctionByNameWithArguments` calls `UFUNCTION(Exec)` functions with
  arguments parsed by `ImportText`; `UObject::ProcessConsoleExec`; Core's `FExec`, `FSelfRegisteringExec` and
  `FStaticSelfRegisteringExec` (`Misc/Exec.h`, `Misc/CoreMisc.h`).
- **UObject delegates** (P10). `BindUObject` / `CreateUObject` / `AddUObject` hold a `TWeakObjectPtr`: the binding is
  inert once the object is gone, and multicast `Add` drops dead bindings. Core names the pointer through
  `UObject/WeakObjectPtrTemplatesFwd.h`.
- **Struct text operations.** `TStructOpsTypeTraits::WithExportTextItem` / `WithImportTextItem` let a struct use its
  own text form (`STRUCT_ExportTextItemNative` / `STRUCT_ImportTextItemNative`).
- **LeonHeaderTool `PerObjectConfig`** (`CLASS_PerObjectConfig`) and the `ConfigAndExec` golden case (35 cases).
- **P10 tests.** 21 new `System.CoreUObject.*` tests (GarbageCollection, Delegates, SoftObject, Config, Exec):
  `LeonAutomationTests` runs 279 tests, TestPAL 98 on Win64 and 93 on the PS2. TestPAL logs the GC cost and a final
  collection; the PS2 numbers are in `Budgets.md`.

- **CoreUObject** (`Engine/Source/Runtime/CoreUObject`, every platform, PS2 included; depends on Core only). See its
  `README.md`.
  - Object model: `UObjectBase`, `UObjectBaseUtility` and `UObject`, with UE's object flags, names, outer chains and
    path names.
  - Types: `UField`, `UStruct`, `UScriptStruct`, `UClass`, `UEnum` (with `_MAX`), `UFunction` and `UPackage`. They are
    UE's intrinsic classes, written by hand.
  - Properties: `FField` / `FProperty` and every property type: numeric, bool (including bitfields), byte / enum,
    string / name / text, object / class / weak / soft references, struct, array / set / map. Properties export and
    import text.
  - Objects: `NewObject`, `FObjectInitializer`, `CreateDefaultSubobject` (every instance builds its own subobjects,
    D12), class default objects, `StaticFindObject` / `FindObject`, `MakeUniqueObjectName`, `CreatePackage` and the
    transient package.
  - Storage: `GUObjectArray` with a fixed capacity of `FPlatformProperties::MaxObjectsInGame` (8192 on the PS2,
    131072 on desktop), the name hash and `TObjectIterator`.
  - Casts and references: `Cast`, `CastChecked`, `ExactCast`, `TSubclassOf`, `TWeakObjectPtr`, and minimal
    `TSoftObjectPtr` / `FSoftObjectPath`.
  - Calls: `FFrame`, the `P_GET_*` macros and `UObject::ProcessEvent` over the generated exec thunks.
  - Registration: `RegisterCompiledInInfo` records a module's types, and `ProcessNewlyLoadedUObjects` constructs the
    packages, enums, structs, classes (supers first) and class default objects.
  - NoExport Core structs in `NoExportTypes.h`: `FVector`, `FVector2D`, `FVector4`, `FPlane`, `FRotator`, `FQuat`,
    `FTransform`, `FColor`, `FLinearColor`, `FGuid`, `FIntPoint`, `FIntVector` and `FBox`.
  - 27 `System.CoreUObject.*` automation tests with reflected fixtures in `Private/Tests`. They run in
    `LeonAutomationTests` (258 tests) and in TestPAL on every platform (73 on the PS2).
- **Script containers in Core.** `FScriptArray`, `FScriptSparseArray`, `FScriptSet`, `FScriptMap` and
  `TScriptBitArray` are type-erased views with the exact layout of the Core containers, checked with `static_assert`s.
  Core also gains `TEnumAsByte`, `WITH_EDITORONLY_DATA` (1 on desktop outside Shipping) and
  `PRAGMA_DISABLE/ENABLE_DEPRECATION_WARNINGS`.
- **LeonHeaderTool `USTRUCT(NoExport)`.**
  - NoExport structs are declared inside `#if !CPP` and have no `GENERATED_BODY`.
  - The generated code takes the offsets from the real C++ type and `static_assert`s the declared size, member types
    and offsets against it.
  - There are 4 new golden cases, so `LeonHeaderTool -Test` now runs 34 cases.
- **Module registration hook.** `FModuleManager::StartupStaticallyLinkedModules` calls each module's
  `RegisterReflection`, then `OnProcessLoadedObjectsCallback` (bound by CoreUObject), before `StartupModule`.
- **TestPAL budget lines.** TestPAL links CoreUObject and logs the reflected types, the heap used to construct them,
  the object array and the live object count. The PS2 figures are in `Budgets.md`: reflection is about 220 KB of its
  400 KB budget, and the object array takes 96 KB.

- **LeonHeaderTool** (`Engine/Source/Programs/LeonHeaderTool`) is a std-only C++17 host program with its own
  `CMakeLists.txt` (tokenizer, header parser, type model, code generator, manifest).
  - It reads UCLASS / USTRUCT / UENUM (+UMETA) / UPROPERTY / UFUNCTION, GENERATED_BODY (and the legacy
    GENERATED_UCLASS_BODY / GENERATED_USTRUCT_BODY) and `#if WITH_EDITORONLY_DATA` property blocks.
  - It writes UE 4.27-shaped `<Header>.generated.h` / `<Header>.gen.cpp` and a `<Module>.init.gen.cpp`: the package,
    plus an explicit `RegisterReflection_<Module>()` in place of static `FCompiledInDefer` objects.
  - Errors are printed as `file(line): error: message`. Outputs are only rewritten when they change.
  - `LeonHeaderTool -Test` runs 30 golden cases, 18 of them error cases.
  - The generated-code contract for P9 is in its `README.md`.
- **Host tools tree.** LeonBuildTool builds LeonHeaderTool into `Engine/Intermediate/Build/HostTools/<Host>/` before
  configuring any target, with MSVC on Win64 and g++ inside the ps2dev Docker image. It passes
  `-DLEON_HEADER_TOOL=<path>` to the target configure.
- **Reflection rules.** `Configuration/ReflectionRules.cmake` reflects any module with a header that includes its
  `.generated.h` (and `Private/Tests/**.h` for test targets):
  - it writes a `<Module>.lhtmanifest`;
  - a stamped custom command runs the tool;
  - the `.gen.cpp` files compile into the module;
  - `<tree>/Inc/<Module>` becomes a public include path.

  Only CoreUObject (and its test fixtures, in test targets) is reflected so far.
- **Module table.** `FStaticallyLinkedModuleInfo` gains `RegisterReflection` (`nullptr` for modules without reflected
  types), filled by the generated module table.
- **Runs.** `RunTests.bat`, and therefore the CI win64 job, runs the LeonHeaderTool golden tests after the automation
  tests.

### Changed

- **Default subobject archetypes** (P11). `UObject::GetArchetype` of a default subobject is the subobject of the same
  name in its outer's archetype, as in UE; packages save a subobject's properties against it.
- **PS2 ELF sizes** (P11). ThirdPerson and BlankProgram still do not link CoreUObject, but the two new `FArchive`
  virtuals add their slots to every archive vtable they link: ThirdPerson's text grows by 56 bytes.
- **Object flags at registration** (P10). `RF_MarkAsRootSet` and `RF_MarkAsNative` become the `RootSet` and `Native`
  internal flags when an object enters `GUObjectArray`, as in UE.
- **Reference properties** (P10). `FProperty::ContainsObjectReference` takes an `EPropertyObjectReferenceType`
  (strong by default); `RefLink` lists weak and soft references too, and the collector only follows strong ones.
- **Config strings** (P10). With `PPF_ConfigOnly`, a top-level `FString` / `FText` exports without quotes.
- **Host g++.** The PS2 Docker entry point, the optional Dockerfile and the CI ps2 job install `g++ musl-dev`, the
  host compiler for LeonHeaderTool.
- **Include order.** `.clang-format` keeps a reflected header's `"<Name>.generated.h"` after its other engine includes,
  as LeonHeaderTool requires.
- **PS2 ELF sizes.** ThirdPerson and BlankProgram do not link CoreUObject. The registration hook adds 40 bytes of text
  to `FModuleManager`; ThirdPerson's stripped ELF is unchanged.

## [0.14.0] - 2026-09-25

Fifth to seventh steps of the Core / CoreUObject plan (P5–P7): every module uses Unreal Engine 4.27's Core types, and
the world uses UE's axes and units (X forward, Y right, Z up, left-handed, 1 unit = 1 cm) with UE's view and
projection matrices.

Seventh step (P7): the world moves from Y-up metres with glm's GL matrices to UE's space. Legacy data (`.llev`
levels, version-1 `.lmesh` meshes) is converted where it is read; importers write UE space. Golden tests recorded in
the legacy world before the switch check every step, and `LeonGame` frames match the legacy captures up to float
rounding at texel and shadow edges.

### Changed

- **Axes and units**: the world is X forward, Y right, Z up, left-handed, 1 unit = 1 cm. Every metre constant in
  Engine, AIModule, PhysicsCore, RenderCore, Renderer and the shaders is now in centimetres: movement tuning, capsule,
  step and skin, camera near / far (10 / 10 000) / distance / zoom, spring arm, physics probes and epsilons, navigation
  cells and bands, lights, shadow fit, AO radius / bias, debug draw sizes. Primitives are 100 cm; code that reads a
  scale as a size uses 50 × |Scale| half extents; masses stay in kg.
- Movement, queries, navigation, the camera boom and reflections work on Z up, with UE-style names: `QuerySupportZ`,
  `FloorZ`, `VelXY` / `VelocityZ`, `ClampPositionXY`, `SeparateAabbXY`, NavMesh `OriginY`, `BobBaseZ`,
  `FSceneRenderer::MakeReflectMatrix(PlaneZ)`.
- **Transforms**: components, level data, lights, volumes, player starts and skeletal attachments hold UE's
  `FTransform`; `AActor` stores an `FRotator`; `USceneComponent` has `RelativeLocation`, `RelativeRotation`
  (`FRotator`) and `RelativeScale3D`, and children compose as `Relative * ParentWorld` (a non-uniform parent scale no
  longer shears). A light shines along its rotation's forward axis. Legacy content meshes face +Y, so a character's
  mesh sits at `RelativeRotation.Yaw = LegacyContentYaw` (-90).
- **Legacy data**: `FLegacyCoordinateConversion` (RenderCore) converts `.llev` data and version-1 `.lmesh` meshes at
  load, and the level saver converts back, so `.llev` files stay Y up in metres. Basis UE = 100 × (X, Z, Y) (the
  same swap as UE's glTF importer and ufbx's `left_handed_z_up`); rotations (-X, -Z, -Y, W), tangents (X, Z, Y, -W),
  scale (X, Z, Y). Angles: actor yaw ψ → 90 − ψ; orbit camera (yaw Y, pitch P) → `FRotator(-P, Y + 180, 0)`; free
  look → `FRotator(P, Y, 0)`; light → `FRotator(-P, 90 - Y, 0)`. The Euler extraction is now the true inverse of
  `Rx * Ry * Rz` (the old one inverted `Rz * Rx * Ry`; pure yaw or roll give the same result).
- **Renderer**: render matrices are composed with `FMatrix` operators (UE's row-vector order) and uploaded as they are
  (`FShader::SetMat4(Name, const FMatrix&)`). Views are in UE view space (x right, y up, z forward, left-handed;
  `RenderCore/ViewMatrices.h`: `MakeViewMatrix`, `MakeLookAtView`). `UCameraComponent::ProjectionMatrix()` is Core's
  `FPerspectiveMatrix` (vertical field of view kept) or `FOrthoMatrix` with depth in [0, 1]; the renderer converts it
  to GL clip space last with `ToGLClipSpace` (`RenderCore/GLClipSpace.h`, z_gl = 2z − w), so frustum culling, the
  shadow lookup, SSAO depth and the debug light frustum keep GL clip space. The shadow fit works in a left-handed
  light view, `ssao.frag` follows the mirrored view space, the planar mirror reflects about z = PlaneZ and
  `glFrontFace(GL_CCW)` is explicit (winding is unchanged).
- **Camera and control**: `UCameraComponent` holds a view `FRotator` (`SetViewRotation` / `GetViewRotation` /
  `AddViewRotation`: a positive yaw turns right, a positive pitch looks up, clamped to ±89°). `AController` has
  `ControlRotation`; `APawn` has `AddControllerYawInput` / `AddControllerPitchInput` / `GetViewRotation`;
  `APlayerController` applies look input and clamps the pitch (`ViewPitchMin` / `ViewPitchMax`). Move input is an
  `FVector2D` (X forward, Y right) from `UPlayerInput::GetMoveInput`. `AActor` takes `FRotator`s instead of float
  yaws. `USpringArmComponent` has `TargetOffset` and `SocketOffset` vectors and follows the pawn's control rotation
  with `bUsePawnControlRotation`.
- **MeshUtilities**: `FImportCoordinateConversion` is every importer's last step: OBJ and glTF are right-handed Y up
  ((X, Z, Y) × 100, bit for bit `FLegacyCoordinateConversion`), FBX is right-handed Z up after ufbx resolves the file
  axes ((X, −Y, Z) × the file unit, UE's `FFbxDataConverter`); ufbx no longer scales. The skeletal import conjugates
  the inverse bind pose and the sampled clips (B⁻¹ M B) and builds bone rotations with `FQuatRotationMatrix`; tangents
  are computed after the conversion.
- **`.lmesh` version 2**: writers store UE space; the reader loads version 2 as stored and converts version 1. The
  `Cube.obj` cook identity is now `EFF1459AE46710C6F1B44C0B1ECB2D739CB590F2492B9DF3EC11A03ECA7757C9`, the same data
  as the version-1 cook converted at load.
- **JoltPhysics and AudioMixer** keep their own spaces (Y up, metres) behind a boundary that swaps Y and Z and scales
  by 0.01; Jolt's own constants stay in metres.
- **Gate G4** (`CheckBannedApis.ps1`) bans `LegacyGL`, `FLegacyTransform` and `LegacyAxes` everywhere, tests included,
  and allows `FLegacyCoordinateConversion` only in its own files, the `.llev` and `.lmesh` readers, `Private/Tests`
  and `LegacyGolden.h`. Messages read `<file>:<line>: G4 <rule>: <code> -> <replacement>`; `-Root` scans another tree.
- Tests: 231 (179 after P6). The golden tables are unchanged since they were recorded; the LegacyGLMath and
  LegacyTransform tests became ViewMatrices, RenderMatrices and LegacyCoordinateConversion tests.

### Added

- Golden tests of the legacy behaviour (`System.Engine.Golden.*`: character movement, traces, navigation, spring arm,
  yaw-relative input, orbit and free-look camera NDC, shadow light space, planar reflection, frustum culling, the
  Starter level; `System.AIModule.Golden.AIControllerArrives`, `System.JoltPhysics.Golden.BoxDrop`) with their
  adapters in `Engine/Public/Tests/LegacyGolden.h`; `-GoldenRecord` prints the tables instead of checking them.
- `LeonGame -Screenshot=<file.bmp> [-ExitAfterFrames=N]` saves frame N (default 60) as a 24-bit BMP and exits;
  `FSceneRenderer::ReadFramebufferBgr`.
- Axes gizmo: **F6** in `LeonGame` (or `-AxesGizmo`) draws 1 m world axes at the origin and a view-orientation gizmo
  in the bottom-left corner, X red, Y green, Z blue; `FDebugDraw::AddAxes(FVector or FTransform, Length = 100)`,
  `AddViewAxes(View)`, `FSceneRenderer::SetAxesGizmoEnabled`. Off by default.
- `Docs/TESTING.md` with the manual checklist for the axes and units.
- `RenderCore/Public/LegacyCoordinateConversion.h`, `ViewMatrices.h`, `GLClipSpace.h`,
  `MeshUtilities/Public/ImportCoordinateConversion.h`, `Renderer/Private/RenderMatrices.h` (normal matrix, 2D overlay
  projection).

### Removed

- `RenderCore/Public/LegacyGLMath.h` (`LegacyGL`) and `Engine/Public/Level/LegacyTransform.h` (`FLegacyTransform`).
- `LightDirectionFromRotation` / `RotationFromLightDirection`, the unused model-matrix override of level meshes,
  `UCameraComponent::Orbit` / `AddLook` / `SetYawPitch` / `GetYawDegrees` / `GetPitchDegrees`, the spring arm's boom
  angles and `AActor`'s float yaw overloads.

### Fixed

- `FShadowMap::FitLightSpaceMatrix` put the near plane beyond the corner nearest the light, so the casters closest to
  the sun cast no shadow; the near plane now sits in front of it.
- Win64 builds under a non-UTF-8 console code page recorded no header dependencies (cl.exe's localized
  `/showIncludes` prefix never matched), so header edits rebuilt nothing; LeonBuildTool now configures and builds in
  code page 65001 and restores the caller's.

Sixth step of the Core / CoreUObject plan (P6): Renderer, Engine, AIModule, MeshUtilities, Cooker, LeonCook, the
JoltPhysics plugin and the desktop Launch code use Unreal Engine 4.27's Core types; glm, nlohmann/json and Catch2 are
gone. The world is still Y-up in metres (the Z-up centimetre switch is P7).

### Changed

- **Engine, Renderer, AIModule, desktop Launch**: glm is replaced by `FVector` / `FMatrix`; `std::vector` / `string` /
  `map` / `function` / smart pointers by `TArray`, `FString`, `TMap`, `TFunction`, `TUniquePtr` / `TSharedPtr`.
  Input mapping takes `FName` action names, the debug draw / overlay take `FLinearColor` colors and `FString` text,
  `FResourceCache` returns `TSharedPtr<UStaticMesh>` / `TSharedPtr<UTexture2D>`, mesh bounds are `FVector`, `ULevel`
  uses `TArray` / `FString` and `SIZE_T` mesh indices (`ULevel::Npos`), the navigation A* open set is a `TArray` heap.
- Render matrices keep glm's GL memory layout until P7: the new `LegacyGLMath.h` (RenderCore, namespace `LegacyGL`)
  composes them with `Mul(A, B)` (glm's `A * B`) and builds them with glm's formulas term by term (`Perspective`,
  `Ortho`, `LookAt`, `Translate`, `Rotate`, `Scale`, `QuatToMatrix`, `NormalMatrix3x3`). The Starter level renders
  pixel-identical to the P1 and P5 captures (outside the stats text).
- `FLegacyTransform` moves from Core (`Migration/LegacyTransform.h`) to Engine (`Level/LegacyTransform.h`) on Core
  math, until P7.
- The `.llev` reader / writer uses `FMemoryReader` / `FMemoryWriter` and `FFileHelper` and writes the same bytes; the
  string table stays case-sensitive. The level loader no longer uses try / catch.
- **Renderer**: the `.lmat` reader / writer works on `FString` through `FFileHelper` with its own line parser (same
  rules); `PatchMaterialFromJson` / `HasMaterialSurfaceFields` take an `FJsonObject` from the native `Json` module, and
  missing or mistyped fields keep their value instead of throwing. Renderer depends privately on `Json`.
- **MeshUtilities / Cooker / LeonCook**: the OBJ, FBX and glTF importers take `FString` paths and build `FVector` /
  `FMatrix` data (the skeletal import through `LegacyGL::QuatToMatrix`); vertex dedup uses `TMap`, and cooked `.lmesh`
  files are byte-identical. Files go through `IFileManager` / `FPaths`, cook recipes are read with the `Json` module,
  and the commandlet parses switches with `FCString`.
- **JoltPhysics**: bodies in a `TArray`, allocator and job system in `TUniquePtr`, Jolt's trace hook routed to
  `UE_LOG`; the floor plane tracks "no floor yet" with a flag instead of a NaN.
- iostream, `printf` and `std::chrono` become `UE_LOG` and `FPlatformTime`: new categories `LogEngine`, `LogLevel`,
  `LogPath`, `LogPhysics` (`EngineLogs.h`), `LogRenderer`, `LogMeshUtilities`, `LogCook`, `LogJolt`, `LogLaunch` and
  `LogBlankProgram`. Headless `LeonGame` flushes `GLog` every tick so redirected output stays current.
- Every test is a UE automation test (179: the 90 Engine, Renderer and AIModule cases, the two OBJ import cases and
  the seven Jolt cases migrated). `LeonAutomationTests` runs only `FAutomationTestFramework` and keeps
  `-automation=<filter>`; `RunTests.bat [-automation=<filter>]`. The glm comparison tests became
  `System.RenderCore.LegacyGLMath.Builders` / `Composition` (values glm 1.0.1 printed), and
  `System.Core.Migration.LegacyTransform.*` became `System.Engine.LegacyTransform.*`. Two level-format tests declare
  their expected errors with `AddExpectedError`.
- C++17 on every platform (Win64 and Linux were C++20), like UE 4.27.
- **Core**: `TIsDerivedFrom` takes UE's `<Derived, Base>` order; `FPlatformProcess::Sleep` on Windows and Linux; the
  Windows HAL and OpenGLDrv include `<Windows.h>` through `Windows/WindowsHWrapper.h` (UE's name), which keeps Core's
  `TEXT`. MSVC no longer warns about `alignas` padding (C4324), as in UE.
- `FString ==` ignores case (UE), so exact-case comparisons use `Equals(…, ESearchCase::CaseSensitive)`; the spring
  arm lag uses `FMath::Lerp`, whose last bit can differ from `glm::mix`.
- ThirdPerson keeps its game mode in a `TUniquePtr`; BlankProgram prints through `UE_LOG`.

### Removed

- ThirdParty modules `GLM`, `NlohmannJson` and `Catch2`.
- Core's `Migration/` folder: `GlmInterop.h` (`ToGlm` / `FromGlm`), `LegacyAxes.h`, `LegacyContentPath.h` and
  `LegacyTransform`. `FPaths::ResolveLegacyContentPath` stays until P15.
- The `-noautomation` / `-automationonly` switches and the Catch2 arguments of `LeonAutomationTests` / `RunTests.bat`.

### Added

- `Engine\Build\BatchFiles\CheckBannedApis.ps1` (gate G4): rejects glm, nlohmann, `std::vector` / `string` / `map` /
  `unordered_map` / `function` / `shared_ptr` / `unique_ptr`, iostream and the `printf` family outside ThirdParty, the
  platform HAL sources, Core's `printf` wrappers, LeonHeaderTool and the test program mains. `Lint.bat` runs it after
  the format check; the CI win64 job runs it with `pwsh`.
- `RenderCore/Public/LegacyGLMath.h`, `Engine/Public/Level/LegacyTransform.h`, `Engine/Public/EngineLogs.h`,
  `Core/Public/Windows/WindowsHWrapper.h`.

Fifth step of the Core / CoreUObject plan (P5): the modules below Engine use Unreal Engine 4.27's Core types.

### Changed

- **ApplicationCore**: `GenericApplication::MakeWindow` returns `TSharedRef<FGenericWindow>`; windows own their RHI
  in a `TUniquePtr`, report the cursor as an `FVector2D` and the mouse wheel through the `FOnWindowMouseWheel`
  delegate (`OnMouseWheel`); the GLFW and PS2 backends log through `LogApplicationCore`.
- **RHI / OpenGLDrv / PS2RHI**: `PlatformCreateDynamicRHI` returns an owned pointer like UE's; `LogRHI` replaces
  `printf` / iostream (the PS2 `[Draw3D]` stats line keeps its text); handle ids are `uint32`.
- **Launch**: the engine loop owns the application and the desktop session in `TUniquePtr`; the PS2 stats overlay
  formats with `FCString`.
- **PhysicsCore** (and `FPhysScene` in Engine): `FVector` / `FVector2D` / `TArray` API, `FCapsuleShape` became UE's
  `FCollisionShape`, the body shape enum is `EBodyCollisionShape`, backends are `TUniquePtr` and take `TArray` /
  `FVector`; hits are sorted with a stable sort.
- **RenderCore**: `FVertex`, `FMeshData`, `FMaterial`, `FFrustum` and `TransformLocalBox` use Core math and
  containers (`TSharedPtr` textures); the `.lmesh` reader / writer uses `IFileManager` archives and produces the same
  bytes.
- **AnimationCore**: skeletons, clips, blend spaces and anim instances use `TArray<FMatrix>`, `FName` names and the
  new `FIntVector4` (Core) for bone indices; bone matrices keep the glm memory layout.
- **AudioMixer**: `TCHAR` sound paths, `FVector` listener / emitter, `LogAudioMixer`.
- **SlateCore / UMG**: `FLinearColor` colors, `FText` display text (`UTextBlock::SetText(FText)`), `FName` ids.
- The PhysicsCore, RenderCore and AnimationCore tests are automation tests now (80 automation + 99 Catch2 test
  cases, 179 as before); the tests that need Engine or MeshUtilities moved into those modules.
- Engine, Renderer, AIModule, MeshUtilities and JoltPhysics convert with `ToGlm` / `FromGlm` where they call the
  migrated modules (until P6).

### Added

- `FIntVector4` (UE 4.27's) in `Core/Public/Math/IntVector.h`.

## [0.13.0] - 2026-09-25

Second to fourth steps of the Core / CoreUObject plan (P2–P4): Unreal Engine 4.27's Core foundations, float math and
platform services (files, archives, paths, config, command line) in `Engine/Source/Runtime/Core`, plus native `Json`
and `Projects` modules, on every platform including the PS2.

### Added

- **Build / defines**: `Misc/Build.h` (`UE_BUILD_DEBUG/DEVELOPMENT/SHIPPING` from `LEON_BUILD_<CONFIG>`, `DO_CHECK`,
  `DO_GUARD_SLOW`, `DO_ENSURE`, `NO_LOGGING` in Shipping, `WITH_DEV_AUTOMATION_TESTS`), `Misc/CoreMiscDefines.h`
  (`INDEX_NONE`, `EForceInit`, `ENoInit`, `EInPlace`, `UE_NONCOPYABLE`); `CoreTypes.h` includes both.
- **Characters**: `TCHAR` is UTF-8 on every platform (`TEXT(x)` is `x`; `WIDECHAR` only in the Windows HAL);
  `HAL/Platform.h` adds `LIKELY` / `UNLIKELY`, `PLATFORM_BREAK`, `LEON_PRINTF_FORMAT`.
- **HAL**: `FPlatformMisc` (`LowLevelOutputDebugString`, `LocalPrint`, `IsDebuggerPresent`, `RequestExit`; a forced
  exit halts the EE on PS2), `FPlatformAtomics` (Windows intrinsics, Linux `__atomic`, PS2 generic: one EE thread),
  integer / bit helpers on `FPlatformMath`, `FMath` integer helpers (`Math/UnrealMathUtility.h`), `FMemory` over
  `GMalloc` = `FMallocAnsi` with current / peak byte tracking, `FPlatformProperties::NamePool*` limits.
- **Assertions**: `check`, `checkf`, `verify`, `verifyf`, `checkNoEntry`, `checkNoReentry`, `unimplemented`,
  `checkSlow`, `ensure` / `ensureMsgf` / `ensureAlways` (reported once per call site through `GLog`), `FDebug`.
- **Templates / Algo**: `UnrealTemplate`, `UnrealTypeTraits`, `TypeHash`, `MemoryOps`, `AlignmentTemplates`,
  `TTuple` / `TPair`, `TUniquePtr`, `TSharedPtr` / `TSharedRef` / `TWeakPtr` (`ESPMode::NotThreadSafe` default),
  `TFunction` / `TUniqueFunction` / `TFunctionRef`, `Sort` / `StableSort`, `TOptional`, `ENUM_CLASS_FLAGS`,
  `Algo::IntroSort` / `BinarySearch` / `LowerBound` / `UpperBound` / binary heap.
- **Containers**: allocator policies (heap, inline, fixed, set, sparse array), `TArray` (UE API with heap functions;
  ranged-for catches a resize), `TArrayView`, `TBitArray`, `TSparseArray`, `TSet`, `TMap` / `TMultiMap`, `FString`
  (case-insensitive `==` / `<` / `GetTypeHash` like UE, `Printf`, `ParseIntoArray`, path `/`, `LexToString`),
  `StringConv` (`TCHAR_TO_UTF8` & co. are identities), `FCString` / `FChar`, `FCrc`.
- **Names and text**: `FName` (8 bytes, case-insensitive, numeric suffix, global pool sized per platform; PS2:
  16 KB blocks, 256 KB max, 4096 buckets, exhaustion is fatal); minimal `FText` (`Format` with `{0}` arguments,
  `AsNumber`, `AsPercent`, `Join`; `LOCTEXT` / `NSLOCTEXT` / `INVTEXT` keep the source text).
- **Logging**: `UE_LOG`, `UE_CLOG`, log categories (`DECLARE_LOG_CATEGORY_EXTERN`, `DEFINE_LOG_CATEGORY(_STATIC)`,
  run-time verbosity), `FOutputDevice`, `GLog` (`FOutputDeviceRedirector`), stdout device (EE console / PCSX2 log
  on PS2) and a Windows debugger device. Lines read `Category: Verbosity: Message`.
- **Delegates**: `TDelegate`, `TMulticastDelegate` (static / lambda / raw / SP bindings with payload, safe removal
  during `Broadcast`), `DECLARE_DELEGATE*` / `DECLARE_MULTICAST_DELEGATE*` / `DECLARE_EVENT*`, `FDelegateHandle`.
- **Automation tests** (`Misc/AutomationTest.h`): `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `FAutomationTestBase`
  (`TestEqual`, `TestTrue`, `AddExpectedError`, …), `FAutomationTestFramework::RunTests`; an unexpected error logged
  during a test fails it.
- **TestPAL** program (all platforms): runs the automation tests without Catch2 and prints
  `TestPAL: PASSED (N test(s), 0 failed)` plus memory and name-pool numbers; `RunPCSX2.ps1 -Program <Name>` runs an
  engine program's PS2 ELF. `Engine/Platforms/PS2/Documentation/Budgets.md` records ELF / heap / name-pool numbers.
- **Math** (`Math/UnrealMath.h`, P3): float `FMath` (constants, `Clamp`, `Lerp`, `FInterpTo` / `VInterpTo` /
  `RInterpTo` / `QInterpTo`, `ClampAngle`, `VRand` / `VRandCone`, `LinePlaneIntersection`, `LineBoxIntersection`,
  `ClosestPointOnSegment`, …), `FVector`, `FVector2D`, `FVector4`, `FIntPoint`, `FIntVector`, `FRotator`, `FQuat`,
  `FMatrix` (row vectors, `V * M`) with `FRotationMatrix` (`MakeFromX` & co.), `FRotationTranslationMatrix`,
  `FQuatRotationTranslationMatrix`, `FScaleRotationTranslationMatrix`, `FTranslationMatrix`, `FScaleMatrix`,
  `FInverseRotationMatrix`, `FRotationAboutPointMatrix`, perspective / ortho (normal and reversed Z) and
  `FLookFromMatrix` / `FLookAtMatrix`; `FPlane`, `FBox`, `FBox2D`, `FSphere`, `FBoxSphereBounds`, a scalar
  `FTransform`, `FColor` / `FLinearColor` (sRGB table, HSV, hex) and `FRandomStream`. Automation tests with
  reference values pass on Win64 and on PS2 (TestPAL); desktop tests also compare against glm.
- **Migration bridges** (`Core/Public/Migration/`): `GlmInterop.h` (`ToGlm` / `FromGlm`, desktop, until P6) and
  `LegacyAxes.h` (the Y-up metre world directions, until P7).
- **Platform file layer** (P4): `IPlatformFile`, `IFileHandle`, `IPhysicalPlatformFile`, `FPlatformFileManager`, with
  Win32, POSIX (Linux) and PS2 backends (read-only newlib POSIX on `host:`); `IFileManager` with buffered file
  archives, `FindFiles`, directory iteration, copy / move / delete; `FFileHelper` loads and saves strings and arrays
  (saves write a temporary file, then move it).
- **Archives**: `FArchive` with `<<` for scalars, `FString` (UTF-8), `FName`, `FText`, `TArray`, `TSet`, `TMap` and
  the math types; `FMemoryArchive`, `FMemoryReader`, `FMemoryWriter`, `FBufferArchive`.
- **Command line and app**: `FCommandLine` (every `main` builds it from `argv`), `FParse` (`Param`, `Value`, `Token`,
  `Command`, `Bool`, `Line`, …), `FApp`, `FPlatformProcess` (`BaseDir`, `SetArgV0`, current directory).
- **Misc types**: `FGuid` (`NewGuid`, `NewDeterministicGuid`), `FMD5` / `FMD5Hash`, `FDateTime` / `FTimespan`
  (integer ticks), `FPlatformTime::SystemTime` / `UtcTime`, `FPlatformMisc::CreateGuid`, `BytesToHex` / `HexToBytes`.
- **Config**: `FConfigCacheIni` / `GConfig` with `GEngineIni`, `GGameIni`, `GInputIni`, `GEditorIni`, loading UE's
  layers (engine base, engine platform, project default, project platform, and the desktop-only
  `Saved/Config/<Platform>` user layer that `Flush` writes); `+ - . !` operators, quoted values and
  `-ini:<Name>:[Section]:Key=Value` overrides.
- **Log file and verbosity**: `FOutputDeviceFile` writes `<Project>/Saved/Logs/<Project>.log` on desktop (flushed per
  line, previous run kept as a backup); `FLogSuppressionInterface` applies `[Core.Log]` and `-LogCmds=`.
- **Json module** (all platforms, no third-party code): `FJsonValue` and its subclasses, `FJsonObject`, streaming
  `TJsonReader`, `TJsonWriter` with pretty and condensed print policies, `FJsonSerializer`.
- **Projects module** (all platforms): `FProjectDescriptor` (`.lproj`), `FPluginDescriptor` (`.lplugin`),
  `FModuleDescriptor` (`EHostType`, `ELoadingPhase`, platform allow / deny lists), `FPluginReferenceDescriptor`,
  `IProjectManager`, `IPluginManager` (discovers engine and project plugins).
- `RunPCSX2.ps1` stages the ini files and the `.lproj` beside the ELF (`-NoStage` skips it) and documents that
  PCSX2's host filesystem must be enabled for the PS2 build to read them.

### Changed

- `CoreMinimal.h` includes the new Core set, math included.
- The glm-based `FTransform` is now `FLegacyTransform` (`Migration/LegacyTransform.h`, desktop only);
  `FTransform` is UE's. RenderCore's `FBox` and frustum plane are Core's `FBox` / `FPlane`:
  `FBox::FromLocalTransformed` became `TransformLocalBox`, and the ray test became `FMath::LineBoxIntersection`.
- PS2 modules compile with `-Werror=double-promotion`; `FTicker` converts to its `double` clock explicitly.
- Core's tests are automation tests (`System.Core.*`: 51 on Win64, 43 on PS2), and Json and Projects add their own;
  the other modules keep Catch2 (124 test cases). `LeonAutomationTests` runs the automation tests first, then Catch2, and fails if either fails;
  new arguments `-automation=<filter>`, `-noautomation`, `-automationonly`.
- `FTicker` uses UE's `FTickerDelegate` (a `TDelegate`) and `FDelegateHandle`, with an optional delay; the
  `std::function` API is gone. ThirdPerson logs through `UE_LOG(LogThirdPerson, …)` and ticks through
  `FTickerDelegate::CreateLambda`.
- PS2 toolchain compiles with `-ffunction-sections -fdata-sections` and links with `-Wl,--gc-sections`
  (ThirdPerson text 430 KB → 362 KB).
- `FEngineLoop::PreInit` follows UE's order on every platform: command line, project (`-project=`, a first `.lproj`
  argument or the target's project), config, log file and verbosity, `.lproj` descriptor, then modules. `Exit`
  flushes `GConfig`.
- `FPaths` is rewritten over `FString` with UE's API (`EngineDir`, `ProjectDir`, `ProjectContentDir`,
  `ProjectSavedDir`, `Combine`, …) and no `std::filesystem`; desktop directories come from new generated globals in
  `<Target>.ModuleInit.gen.cpp`, the PS2 uses a staged layout under the ELF folder. `FFileHelper` and `FPaths` build
  on the PS2 too.
- `LeonGame` reads its flags with `FParse`: `-tick=<Hz>` and `-showstats` replace `--tick <Hz>` and `--show-stats`;
  the default map, the window size and the stats default come from the engine config (`GameDefaultMap`,
  `DefaultResolutionX/Y`, `bShowStatsByDefault`).
- ThirdPerson reads `MoveSpeed`, `Gravity` and `JumpSpeed` from `DefaultGame.ini` (compiled defaults when the file
  cannot be read) and logs where they came from.
- Math `InitFromString` uses `FParse`.
- LeonBuildTool re-runs the configure step when `Engine/Build/Build.version` changes, so a version bump reaches
  existing build trees.
- `Launch` depends privately on `Projects`; `TestPAL` depends on Core and Projects.
- PS2: `-fno-threadsafe-statics`, and `PS2PlatformRuntime.cpp` routes the global `operator new` / `delete` through
  `FMemory` and defines the pure-virtual handlers, which keeps libstdc++'s unwinder and demangler out of the ELF
  (ThirdPerson text 561 KB → 442 KB).

### Removed

- `FCString::ToLower(std::string_view)`.
- `FJsonUtils` and the `Json` module's nlohmann dependency (Engine, Renderer and Cooker still use nlohmann until P6).
- `FPaths::ResolveAssetPath` (the legacy loaders use `FPaths::ResolveLegacyContentPath` until P15) and the
  `std::filesystem` code in Core.
- `Math/MathStringParsing.h`.

## [0.12.0] - 2026-09-25

First step of the Core / CoreUObject plan: UE-style descriptor extensions and the removal of the features the
new Core replaces or postpones (environment maps, lightmaps, networking, the pack / session layer and the cooked
skeletal formats).

### Changed

- Project and plugin descriptors use the UE-style extensions `.lproj` (`.uproject`) and `.lplugin`
  (`.uplugin`); LeonBuildTool rejects `.leonproject` with a rename hint.

### Removed

- HDR environment maps (skybox pass, cubemap IBL, `Engine/Content/Hdr/`); `blinn_phong.frag` keeps the
  procedural sky. `.llev` still reads and writes the environment field for compatibility but ignores it.
- `.lm` lightmaps (`LightmapIO`, `uLightmap`); they return as `<Map>_BuiltData.lasset` with static lighting.
- Networking: `NetCore`, vendored ENet, `UNetDriver`, root replication and the listen / dedicated / join flow
  (`--listen`, `--host`, `--join`, `--port`, `--dedicated`). The last networked state is tagged
  `archive/net-enet-0.11`.
- The pack / session layer: `FGameHostSession`, `FWorldRuntime`, `FLevelDirector` (level browser), `FLevelCatalog`,
  `FGameplayRouter`, `FLevelAnimation`, `FProjectDescriptor` packs (`Projects/<Name>/leon.game.json`, `--pack`),
  `FPaths` pack content roots, travel on `UGameInstance` / `AGameModeBase`, `ContentValidator`, `FArenaCamera`
  and the editor ids.
- Cooked skeletal formats (`.lskel`, `.lskm`, `.lanim`, `.lchar`, blend space JSON), their loaders on
  `USkeletalMeshComponent` and the `character` / `anim` modes and recipe steps of LeonCook. The FBX skeletal
  import moved to `Developer/MeshUtilities` (`FbxSkeletalImport.h`); AnimationCore no longer links ufbx.

### Changed (runtime)

- `LeonGame [-map=<.llev>] [-nullrhi] [--tick <Hz>] [--show-stats]` loads one level (default
  `Engine/Content/LevelTemplates/Starter.llev`) and runs `ADefaultGameMode`; `-nullrhi` runs headless.
- `AActor` keeps a spawn serial as `GetUniqueID()` (was the editor id).

## [0.11.0] - 2026-09-24

Restructure to the **Unreal Engine 4.27** layout, architecture and coding standard, built with CMake
through **LeonBuildTool**. Rename tables: `Docs/UnrealEngine427/LeonMapping.md`.

### Changed (breaking)

- **Layout**: modules under `Engine/Source/{Runtime,Developer,Programs,ThirdParty}` with
  `Public/Private/Classes`; the PS2 code is a platform extension (`Engine/Platforms/PS2`, including the
  `PS2RHI` module); Jolt is a plugin (`Engine/Plugins/Runtime/JoltPhysics`, Win64); content in
  `Engine/Content`, GLSL in `Engine/Shaders`; tests in `<Module>/Private/Tests`.
- **Build**: LeonBuildTool (UnrealBuildTool homologue, pure CMake) replaces the root CMake build and
  `Scripts/`: `<Module>.Build.cmake` / `<Target>.Target.cmake` / `.leonproject` / `.leonplugin`,
  `Engine/Build/BatchFiles/{Build,Clean,Rebuild,RunTests,Cook,FormatCode,Lint,GenerateProjectFiles}.bat`,
  `Setup.bat` (pinned third-party downloads), PS2 builds in the pinned ps2dev Docker image, generated
  statically linked module table (`IMPLEMENT_MODULE` / `FModuleManager`).
- **Game**: the only game is `Game/ThirdPerson` (from `Projects/Ps2ThirdPerson`), isolated from the engine
  and built separately (`Build.bat ThirdPerson PS2 Development -Project=...`); primary game module
  `FThirdPersonModule` ticking `FThirdPersonGameMode` through `FTicker`.
- **HAL / ApplicationCore / RHI / Launch**: `FPlatformMemory/Time/Math`, `FStatsOverlay`, `FTicker`,
  `EKeys` (UE names), `GenericApplication` / `FGenericWindow` / `IInputInterface` (GLFW desktop, PS2
  application + window + DualShock input), `FDynamicRHI` / `GDynamicRHI` (`FOpenGLDynamicRHI`,
  `FPS2RHI` static API), `GuardedMain` + `FEngineLoop` driving both the PS2 game and the desktop
  `UGameEngine` session one frame at a time.
- **Naming**: no `namespace leon`; UE type prefixes (`UGameEngine`, `UWorld`, `AActor`, `ACharacter`,
  `FSceneRenderer`, `UTexture2D`, `FPaths`, `UGameplayStatics`, `UCookCommandlet`, …); members,
  functions, parameters and locals in PascalCase with `b` bools (clang-tidy over every translation
  unit); `<MODULE>_API` on public classes; shadowing is a compile error (MSVC and GCC).
- **Formatting**: Epic `.clang-format` (tabs, Allman braces, 120 columns) applied to all engine and game
  C++; `.editorconfig`; formatting commit listed in `.git-blame-ignore-revs`.
- **Tooling / CI**: clangd and VS Code read the root `compile_commands.json`; CI builds ThirdPerson for
  PS2 (ELF artifact) and runs the Win64 build + automation tests.

### Added

- `Docs/UnrealEngine427/` knowledge base (UE 4.27 source layout, key headers, Leon mapping, next steps),
  `Docs/BUILD.md`, `Docs/CODING_STANDARD.md` (replaces `NAMING.md`).
- Config placeholders with UE names (`Engine/Config/Base*.ini`, `Engine/Platforms/PS2/Config/PS2Engine.ini`,
  `Game/ThirdPerson/Config/Default*.ini`), not loaded yet.

### Removed

- Editor, Templates (including the bot content and its tests), `Projects/Ps2Cube`, `Projects/Ps2Lab`,
  `Samples/`, `leon-cli`, the legacy root CMake build and `Scripts/`.

### Fixed

- LeonGame was built without the engine framework (target default `COMPILE_AGAINST_ENGINE` ignored).
- `FHelloMsg` protocol version initialised from itself after the rename (now `CurrentProtocolVersion`).

### Earlier in this cycle (before the restructure; paths and names as they were then)

#### Added

- **Projects/Ps2ThirdPerson** — PS2 third-person gameplay (`leon-Ps2ThirdPerson.elf`): orbit SpringArm camera, Character move/jump, primitive sandbox level; DualShock sticks (left move / right camera) + Cross jump; `Scripts/build-ps2-docker.ps1 tp`
- PS2 `InputPad`: `PollPad`, `GetPadLeftStick` / `GetPadRightStick` (DualShock analog mode)
- **Projects/Ps2Cube** — PS2 3D scene (`leon-Ps2Cube.elf`): ViewTarget, DirectionalLight, `M_*` / `T_*_D` materials+textures, FPS/ms HUD; `Scripts/build-ps2-docker.ps1 cube`
- PS2 RHI 3D: `Ps2DrawBox`, `Ps2Texture`, `Ps2Material` (`BaseColor` / `BaseColorMap` / `EShadingModel`), `Ps2SetViewTarget` / `Ps2SetDirectionalLight`, `Ps2DrawDebugHudText`; GS z-buffer in `Ps2InitDisplay`
- Editor viewport **View Mode**: Lit / Player Collision (Alt+5/6); camera-following Show Grid (View menu); `Renderer::SetSceneGeometryEnabled`
- Editor Play: Number of Players + Net Mode; multiplayer via Shipping `--listen`/`--join` + `--map <LevelKey>`; AssetTools rename/move/delete; Content Browser click UX
- Pack `GameplayLib.cmake` + `RegisterModes` / `GameHostSession`; templates copy fixed type names (no `{{NAME}}` rename)
- Unreal-lite match framework: `GameMode::PrepareMatchWorld` / `RebuildNavigation` / `SnapCharacterToFloor` / `EstimateFloorY`; `Character` health (`TakeDamage`/`Die`/`Revive`); `ApplyPointDamage` / `ApplyRadialDamage`; `VolumeHelpers`; `GameplayStatics` traces; `ArenaCamera`; `InteractionPromptWidget`; `PlayerController` button latches; `PlayerState` Lives
- `.llev` **v2**: `TriggerVolume` / `PainCausingVolume` / `AISpawnPoint` (reader still accepts v1); Editor Place/Details/Outliner/Viewport + **Build Paths** (NavMesh bake); PIE ticks pain volumes
- Net protocol **v4**: `InputCmdMsg::buttons` → `uint16` (16 bits); `InputButtons::Scoreboard` / `Sprint` aliases
- **Projects/ThirdPerson** — fixed Unreal-style sample pack (`ThirdPersonCharacter`, no rename)
- **Templates** — New Project copies the template folder as-is (fixed `ThirdPerson*` / `Blank*` types); stamps `name` / `displayName` / `templateId` only
- Net protocol **v3**: `ENetMsg::Rpc` + `RpcHeader` / `EncodeRpc` / `DecodeRpc` (`ERpcId::Notify`); pack AI `AIChaseBehavior` + snapshot AI relevancy cull; RHI opaque ids on PostProcess/EnvMap/meshes/UBO
- Travel E2E + editor-style level save/load headless tests; `Samples/README.md` points at ThirdPerson pack
- **Projects/Furytoon** — party fighter (War of the Whiskers lite): Menu→Lobby→Kitchen; 3D move + double jump; LMB/RMB light/heavy combos; AI bots fill to 4 fighters; stocks/KO; shared top-down arena camera (midpoint + auto zoom); Cube/Sphere mesh fighters; `leon-Furytoon-server`
- `kMaxPlayers` raised to **4**; listen host accepts `kMaxPlayers-1` remotes; `CharacterMovement.MaxJumpCount` (double jump)
- Net protocol **v2**: `InputCmdMsg` locomotion + `buttons` (`InputButtons::*`); `SnapshotHeader` + optional `SnapshotMatchMeta` ext; `PawnSnap` user payload; `AcceptInboundPacket` / `SanitizeInputCmd`; `SendTravelToPeers`; host `PeerPacketWindow` rate-limit
- `ActorComponent` base (`SceneComponent` derives); `RegisterComponent` / `CreateDefaultSubobject`; World ticks components
- **Projects/Zombies** — co-op FPS COD Town loop: Menu→Lobby→Town; points / doors / wall guns / perks / PaP (lava risk); typed Trigger/Pain/AISpawn volumes; round waves; hold-LMB fire + R reload + F interact; COD-style HUD; tracers + F2 traces; `leon-Zombies-server`
- NavigationSystem + grid NavMesh (Unreal-lite, no Recast): bake from static PhysScene AABBs; `FindPath` / `ProjectPointToNavigation`; `AIController` follows waypoints when nav is set; CoopTp builds nav on match prepare
- F3 toggles NavMesh debug draw (walkable green / blocked red); HUD stats moved to F4
- CoopTp arena ramps: rotated Cube prisms with TriangleMesh collision (no separate SlopePlane — avoids yaw/sign mismatch); F2 draws TriangleMesh wire tris (cyan) instead of the fat world AABB; skip fat AABB side-resolve for TriangleMesh
- Planar mirror reflection pass draws skeletal characters; mirror plane uses mesh AABB top
- CoopTp AI spawn plate: orange `AISpawnPlate` cube; stand on it once to spawn 2 chase bots; AI pawns use snapshot slots `kMaxPlayers+` (`kMaxAiPawns` / `kMaxSnapshotPawns`) so clients see them
- `AIController::MoveToActor` chase helper; CoopTp authority AI wave via plate (`SpawnAIWave` / `AIController::Possess`)
- CoopTp: Main Menu **Join Dedicated**; shipping `leon-CoopTp-server` (headless `LEON_DEDICATED_DEFAULT`); `build-project.bat/.sh --with-server`; Project Settings `buildDedicatedServer`
- HUD widgets: `ProgressBarWidget` (UProgressBar lite) and `ImageWidget` (UImage lite / solid tint); CoopTp Main Menu uses Image backdrop + join ProgressBar
- Jolt narrow-phase traces: when `HasNarrowPhaseTraces()`, PhysScene Line/Sphere/Capsule Multi delegate to CastRay/CastShape; Arcade still appends floor plane + slopes; tests `[physics][jolt][trace]`
- Content audio cues: `assets/Audio/UI/*.wav` preferred by `PlayUiSound` (procedural fallback); `PlayMusic`/`StopMusic` looping bed; CoopTp menu/lobby `MenuBed.wav`
- GPU RHI accessors PascalCase (`Shader`/`Texture`/`StaticMesh`/`EnvMap`/targets: `Create`/`Destroy`/`Valid`/`Bind`/`Set*`)
- `World` / `GameMode::SetPhysicsBackend`: opt-in Jolt for match dynamics (CoopTp match uses Jolt when linked; default World stays Arcade); incremental Jolt prepare + MeshShape statics
- Jolt Physics backend (`LEON_WITH_JOLT`, FetchContent v5.3.0): `Plugins/Physics/Jolt`, `PhysScene(EPhysicsBackend::Jolt)` drives rigid-body Step; incremental `RigidPrepareStep` (no rebuild each frame); static `TriangleMesh` → Jolt `MeshShape` on `SyncFromLevel`; CMC stays Arcade; tests in `JoltPhysicsTests`
- Arcade triangle-mesh collision (Unreal ComplexAsSimple lite): `ECollisionShape::TriangleMesh`, `TriangleMeshCollision` / `SegmentTriangle*`; static CPU meshes bake on `SyncFromLevel`; Line/Sphere/Capsule traces + `QuerySupportY` refine vs tris; tests in `TriangleMeshCollisionTests`
- Unreal-like `AudioDevice` (miniaudio): `PlaySound2D` / `PlaySoundAtLocation` / `PlayUiSound`; CoopTp menus play click/confirm/back/error cues
- Unreal-like HUD widgets: `ButtonWidget` (UButton lite) and `VerticalBoxWidget` (UVerticalBox lite); CoopTp Main Menu / Lobby / Pause use VerticalBox + Buttons

#### Fixed

- Character capsules collide with each other (players / AI) via pairwise XZ depenetration in `World::TickGameplayFrame`
- NavMesh bake: ignore wide floor slabs / `Plane` (not plates); force-block `AISpawnPlate`; `SlopeRamp` stays walkable so AI can path/climb (CMC); TriangleMesh blockers use XZ tri footprint instead of fat world AABB
- AIController: tight waypoint arrive (ignore large goal `arriveRadius` for path corners) so chase no longer shortcuts through plates/ramps; no straight-line fallback when a NavMesh is set; CoopTp agent radius 0.45
- CMC walk shove: apply Dynamic push from capsule sweep hits (SafeMove stops at skin so ResolveCapsuleSides never saw contact; jump-side overlap was the only accidental path)
- Dedicated / headless console logs: replace Unicode dashes/arrows with ASCII (`--`, `->`) so Windows cmd does not mojibake UTF-8
- Dedicated server exe opened a window: `LEON_DEDICATED_DEFAULT` on `*-server` never reached `GameApplication` in `leon_runtime`; pass `dedicatedByDefault` from pack `main` (also accept `--server`)
- CoopTp spawn: SyncFromLevel before RestartPlayer; floorY/walkBounds from PlayerStarts + mesh extents; QuerySupportY snap so Rooftops no longer drops pawns to world Y=0
- CoopTp MakeCoopLevels: synthesize MainMenu/Lobby (plane + PlayerStart); snap arena PlayerStarts to floor plane
- CoopTp Join: suppress ghost menu activate after travel (Connecting… until Welcome; 350ms lockout); GameplayRouter re-syncs GameMode same frame after ClientTravel
- `Scripts\smoke-coop-dedicated.bat` — dedicated headless smoke (Courtyard); headless `std::cout` uses unitbuf so redirected logs survive process kill

#### Changed

- Renderer / DebugOverlay / DebugDraw public frame API → PascalCase (`Initialize`, `BeginFrame`, `DrawScene`, `GetDebugOverlay`, …); PIE uses project `defaultGameMode` when the level has no GameMode override
- Dropped obsolete path fallbacks: `games/` projects root, lowercase `engine/` asset roots, and pre-Content `Templates/ThirdPerson/assets/`
- Esc no longer quits the runtime (close window / Quit Game); CoopTp **Esc**/**Backspace**: pause/resume in match, back (or Quit on Main Menu) in menus
- CoopTp Main Menu: **Host Game** / **Join Game** (default `127.0.0.1`; `--join <ip>` for LAN); dedicated via `--dedicated` only
- Default post-process quality is **Low** (light SSAO, no FXAA, 1024 shadows) instead of Medium
- `RunLeonGame` / `GameApplication::Run` register callback is now `(Engine&, GameplayRouter&)` so packs can `SetGameInstance<T>()` before modes run
- CoopTp: removed `CoopSession` singleton (session lives on `CoopGameInstance`); soft map re-enter renamed `OnTravelFinished`
- Coop polish (UE-like): single session source on `CoopGameInstance`; match gate via `GameState::HasMatchStarted`; `FinishClientJoin` unifies Welcome/Travel; shared `CoopNet::SendTravelToPeers`; `GameState::Reset` no longer clears `PlayerArray`; guarded `LoginPlayer` Logout

#### Added

- Character CMC lite (phase 0–5): `FindFloor` / `WalkableFloorZ`; capsule sweep + slide; `tryStepUp`; `EMovementMode` + `AirControl`; `AddSlopeRamp` / steep reject; UE-like tunables (`MaxWalkSpeed`, `JumpZVelocity`, `MaxStepHeight`); tests in `CharacterMovementTests`
- Unreal-like coop framework: `GameState::PlayerArray` (`Add`/`Remove`/`GetNumPlayers`), `GameMode::HandleStartingNewPlayer`, `CoopGameInstance` (session survives travel), `CoopTpPlayerState`, `LoginPlayer` → PostLogin → RestartPlayer; Lobby `ServerTravelToMatchMap`; Tab scoreboard reads PlayerArray
- CoopTp: hold **Tab** for in-match player list (`TextBlockWidget` scoreboard) on listen host, client, and standalone
- Unreal-lite HUD API: `HUD::AddWidget` / `RemoveWidget` / `Tick`, `UserWidget` Construct/Tick/Paint, `WidgetPaintContext::DrawText`, `MenuListWidget`, `TextBlockWidget`; CoopTp menus use `GetHUD()` widgets
- Editor **Project Settings** panel (Edit / Window): Game Default Map, Default GameMode, display name, description — pack-wide `leon.game.json`; **World Settings** stays per-level only
- Unreal-aligned gameplay API (no legacy aliases): `GameState` (`HasMatchStarted`, `GetServerWorldTimeSeconds`, `GetNumPlayers`, `GetMapName`, `HandleMatchHasStarted/Ended`), `GameMode` (`StartMatch`/`EndMatch`, `ServerTravel`/`ClientTravel`, `FindPlayerStart`, `GetGameState<T>`), `GameInstance::ServerTravel`/`ClientTravel`; `LevelCatalog`/`LevelDirector` PascalCase; CoopTp uses `CoopTpGameState` + seamless travel helpers
- `Projects/CoopTp` LAN 2P: **MainMenu** → **Lobby** (Courtyard/Rooftops) → match; Listen as Host / Join / Dedicated (or `--dedicated`); Esc pause → Continue / Exit to Main Menu; Welcome/Travel sync maps. Pack GameModes are not linked into the Editor (PIE uses Default/`third-person` only; `leon.game.json` `gameModes` lists authoring ids)
- Welcome: Open Project picks a **folder**; recent list shows last Engine version; stamps `engineVersion` in `leon.game.json`
- Forward post stack: HDR `SceneColorTarget`, half-res SSAO + bilateral blur, ACES tonemap/exposure, FXAA (`SetPostProcessQuality` Off/Low/Medium/High; optional Early-Z; `GpuPassTimer` Ssao/Post)
- `SpringArmComponent` collision probe (`bDoCollisionTest`, sphere sweep vs PhysScene) so third-person camera pulls in instead of clipping meshes
- `SetActiveContentRoot` / `ActiveContentRoot` and `WriteFileAtomic` / `WriteTextFileAtomic` (`leon/core`)
- Editor toolbar **Stats** toggle (FPS / ms overlay on Viewport); View → Show Stats
- Editor layout persistence (`editor_layout.ini` beside exe): Window → Save Layout / Reset Layout
- Editor applies built-in dock layout when no usable saved layout exists (waits for viewport size; ignores incomplete ini)
- Content Browser Contents: folder tile icons, full-mesh thumbnails; drag material onto Viewport mesh shows live preview and applies on drop
- Content Browser defaults to Contents view (gallery) instead of Hierarchy
- Editor **Build → Build Project** (also File + Ctrl+B): cmake-builds the open game pack via `Scripts/build-project.bat`; log in Output Log
- Build Project resolves repo root by walking parents (fixes `build/Release` depth); Output Log / Console Copy + Ctrl+C + right-click
- Portable packaging: `Scripts/package-editor.bat` → `Dist/LeonEditor/` (editor + SDK); **Build Game** exports `<project>/Shipping/`
- Build menu is **Build Game** only (`<project>/Shipping/`); editor packaging stays a Scripts/`package-editor.bat` step
- Editor branding under `Editor/Resources/Brand/` + `Icons/` (`LeonLogo.png`, `LeonLogoUi.png`, `LeonEditor.png` / `.ico`); Welcome + window / taskbar
- Regenerated `LeonEditor.ico` as BMP/DIB (not PNG-in-ICO) so `rc.exe` embeds the lion; `Scripts/make-editor-icon.py`
- Editor UI: Inter font (`assets/fonts/`), World Settings GameMode dropdown, Details material combo + sphere preview, Content Browser Hierarchy/Contents view with path breadcrumbs and tile previews
- Binary Leon Level format `.llev` (`LLEV`, little-endian, version 1) — `LeonLevelFormat.h` / `.cpp` with `LevelDocument`, `BuildLevelDocument`, `SerializeLeonLevel` / `DeserializeLeonLevel`, `SaveLeonLevelFile` / `LoadLeonLevelFile`, `ApplyLevelDocument`
- Editor New Level templates under `Engine/Assets/LevelTemplates/` (`Blank`, `Starter`)
- Project templates under `Templates/` (`Blank`, `ThirdPerson`); distinct from level templates
- Engine BasicShapes place with textured materials (`M_Default` / `M_WorldGrid` → `Textures/T_Default_D.png`)
- `LightmapIO` (Engine): runtime `.lm` load; `LoadLevelFile` hydrates lightmaps after commit
- Stable `lightmapId` → bake files `Lightmaps/LM_<id>.lm` (reorder-safe)
- Level camera `eye` / `mode` (`Orbit` \| `FreeLook`)
- Level authoring docs: [`Docs/LEVELS.md`](Docs/LEVELS.md)
- Fast editor builds: `Scripts/build-fast.bat`, `Editor/pch.h`, `Build/LeonCompileOptions.cmake` (`/MP`), `Build/SyncDirectory.cmake`
- Session-stable editor selection (`editorId`, `ResolveSelectionIndices`)
- Skinned shadow pass (`skinned_shadow_depth.vert`)
- [`Docs/LIBRARIES.md`](Docs/LIBRARIES.md)
- [`Docs/NAMING.md`](Docs/NAMING.md): folder / file / type / include / CMake conventions (Engine → Runtime → Editor → Templates → Tools)
- [`Docs/TOOLS.md`](Docs/TOOLS.md): offline cook / CLI / ResourceTools (lean `leon_engine_cook`, recipe schema)

#### Fixed

- `ResolveAssetPath` no longer picks the newest file across all `Projects/*` (cross-pack leaks); uses `SetActiveContentRoot` (Editor open project / runtime pack) then Engine/staging
- `--dedicated` uses `initializeHeadless` + `runHeadless` (no OpenGL window); WorldRuntime skips overlay chrome when headless
- `.llev` / `.lskm` / `.lanim` deserializers reject oversized counts before `reserve`/`resize`
- Level / material / lightmap saves write via temp + rename (`WriteFileAtomic`)
- Editor import targets open project `Content/assets/`; dependency copy failures surface as warnings
- Editor undo/redo applies snapshots before mutating history stacks
- Details material picker guards null `ResourceCache`
- `.lmat` parser logs bad/unknown keys instead of silent defaults
- Lightmap bake reports mkdir / `.lm` write failures (no false “saved under lightmaps/”)
- `package-editor.bat`: unique TEMP keep-dir; required vs optional robocopy sources
- Docs: `LeonEngine.exe` path; ThirdPerson cook example under `Content/assets/`
- `.gitignore`: `**/_deps/`, `Projects/*/build-fast/`, `Tools/build-fast/`, `Dist/`
- **Build Game**: resolve `LEON_REPO_ROOT` via walk-up / `-DLEON_REPO_ROOT` (projects under `Editor/build/*/Projects/` no longer point at a fake root); prefer real repo `Projects/` over staged beside-exe copy; report failure via `LEON_BUILD_EXIT` (was false “succeeded” when cmake failed)
- **Build Game** POST_BUILD: force-refresh `LEON_ENGINE_ASSETS` from repo root (stale CACHE pointed at `Editor/build/Release/Engine/Assets` and broke Shipping sync)
- Shipping / runtime: `ResolveAssetPath` finds staged `<exe>/assets/` for paths like `Hdr/…` (skybox HDR was packaged but not resolved)
- Shipping parity with Viewport / PIE: HUD stats off by default (F4 / `--show-stats`); no MSAA on game window; hide level-switcher chrome when pack has one level
- Build Game: show + focus Output Log; toast on start / success / failure
- Project layout: Unreal-like `<project>/Content/` (`levels`, `materials`, `assets`); Content Browser roots there (legacy `<project>/levels` still loads)
- Editor product exe renamed to `LeonEngine.exe` (CMake target still `leon-editor`); portable package: `Dist/LeonEditor/LeonEngine.exe`
- Dist/editor: resolve `Materials/…` under `Projects/<name>/Content/` (Details preview no longer looks beside the exe)

#### Changed

- `PlayInputTarget` (`leon/core/PlayInputTarget.h`): groups PIE / multi-window play input (window override + mouse-look gate); `Engine` convenience API unchanged
- Scripts: shared `Scripts/_vsenv.bat` (vcvars/vsdev); Editor trees documented (`build-fast` vs `build-ninja` vs `build`); `format`/`lint` include `Templates/`; PIE/`editorId` contract in ARCHITECTURE
- `leon_import` (`Engine/Import`, `leon/import/`): OBJ/FBX/glTF load + staticmesh cook; linked by Editor + `leon_engine_cook` only — not shipping `leon_engine`. `ResourceCache` accepts `.lmesh` only
- Lightmap bake is Editor-only: `leon::editor::BakeLevelLightmaps` (`Editor/SceneEditing/LightmapBaker.cpp`); Engine keeps `LightmapIO` load for shipping (Unreal-like Build Lights vs runtime)
- Runtime is opt-in: Engine no longer `add_subdirectory(Runtime)`; Projects/Templates (and Editor Blank fallback) add it after Engine so Editor/Tools do not build `leon_runtime`
- Docs: ARCHITECTURE dependency diagram — Editor does not depend on Runtime (PIE is Editor-owned)
- Editor Selected Viewport PIE hides and locks the cursor (`GLFW_CURSOR_DISABLED`) so mouse look is not clamped by screen edges; Esc / Pause restores it
- Editor New Window PIE ticks after ImGui so skinned draws are not cleared by panel `drawScene`; Play controls only in Toolbar (removed menu Play strip)
- Selected Viewport PIE blocks ImGui mouse (`NoMouse` + scrubbed mouse state) so hidden cursor cannot hover/click editor chrome
- New Window PIE renders on the editor GL context into a shared texture, then blits to the play window (fixes black screen from non-shared FBOs/VAOs)
- Default editor docking: large Viewport, right column Outliner|World Settings + Details, bottom Content Browser|Output Log
- World Settings: GameMode dropdown is `Default` / `third-person` only (removed confusing `Pie` id); Skybox dropdown lists Engine + project HDRs
- Content Browser: material sphere thumbs in Contents gallery; Create Material/Folder via right-click (Unreal-like), not toolbar
- Docs: [`Docs/ARCHITECTURE.md`](Docs/ARCHITECTURE.md) covers templates, BasicShapes, PIE, build helpers; naming points to NAMING.md
- Docs: [`Docs/SETUP.md`](Docs/SETUP.md) documents full `Scripts/` table, Tools/`leon-cli cook`, format skip rules, lightmap shipping tip, **clangd / compile_commands** (MSVC path escapes + Tools DB)
- Docs: [`Docs/ASSET_FORMATS.md`](Docs/ASSET_FORMATS.md) documents `.lm` lightmaps; cross-links [LEVELS](Docs/LEVELS.md) / [TOOLS](Docs/TOOLS.md)
- Docs: [`Docs/NAMING.md`](Docs/NAMING.md) § Tools — ResourceTools live (`leon/tools/`); cook links `leon_engine_cook`
- Docs: [`Docs/TARGET_ARCHITECTURE.md`](Docs/TARGET_ARCHITECTURE.md) is a stub pointing at ARCHITECTURE
- Scripts: `configure-ninja.bat` also exports `Tools/build-ninja` compile DB; `fix-compile-commands.ps1` for clangd
- `.clangd` / `.vscode`: QueryDriver for `cl.exe`; Tools PathMatch; quieter Catch2 clang-tidy noise
- Runtime headers at `Runtime/include/leon/runtime/`; entry `leon::runtime::RunLeonGame` only
- Engine free functions PascalCase only (camelCase aliases removed)
- Enums: `EShadingModel`, `EShaderReloadResult`, `GpuPassTimer::EPass`, `EValidationSeverity`
- Editor types in `namespace leon::editor`; `EditorFileDialog.h`; `LightmapIO` directly
- Tools folder `Cli/` (was `CLI/`)
- Levels are binary `.llev` everywhere: `LoadLevelFile` rejects any other extension, `LevelCatalog` scans `*.llev`, Editor save / undo-redo / drag-drop / templates and the project scaffold all use the binary format; all source levels migrated
- `ValidateLevelDocument` takes a decoded `LevelDocument` (asset references + value ranges); magic / version / class enums are enforced by the reader
- `LevelSaver`: `LevelToJson` / `loadLevelFromJson` replaced by `SerializeLevelSnapshot` / `LoadLevelSnapshot`
- Scripts: correct VS edition discovery; format/lint skip build trees; `cook.bat` Ninja→VS fallback; `leon-cli cook` runs sibling `leon-cook`
- Tools: `leon-cook staticmesh --fbx`/`--gltf` + recipe `staticmesh`; `leon-cli` no longer links Engine; cook POST_BUILD uses `SyncDirectory`
- `leon_engine_cook` INTERFACE (lean cook deps); `leon_resource_tools` (`RunCookRecipeFile` / `ResolveBeside`)
- Level class-name parsers moved to `Content/LevelClassNames.cpp` so cook/validator need not link Scene gameplay stack
- `PieGameMode` under `Editor/Gameplay/` (not Engine / not shipping)
- Unreal-first PascalCase on Core, `GameplayRouter`, `LevelCatalog` / `LevelDirector` (aliases kept)
- `ResolveProjectsDirectory`, `IsPendingKill`; deque anim storage; deferred `World` spawn
- PhysScene mesh AABB sync; Jolt → Arcade fallback; NetDriver ENet refcount + Hello magic
- GLFW init refcount; PIE scroll window; dirty gate Close/Exit; FreeLook camera JSON
- `GpuPassTimer` non-blocking resolve; Renderer honors `shaderDirectory`

#### Fixed

- BlendSpace pointer invalidation; Prop/Pawn empty-mesh load; skeletal shadow uniforms
- clangd false errors on Windows (MSVC `\Users` path escapes in `compile_commands.json`)
- Dead `writeBytes` helper; duplicated material shininess sync; empty validator branches

#### Removed

- JSON level format entirely (sources, loader, saver, validator, catalog) — levels are `.llev` only
- Inline material fields on level actors and the `Prop` / `Pawn` placeholder actor classes
- Migration body of [`Docs/TARGET_ARCHITECTURE.md`](Docs/TARGET_ARCHITECTURE.md) (stub → ARCHITECTURE); misspelled `LIBRERIES.md` → `LIBRARIES.md`
- Editor `LightmapPersist.h` (use `<leon/level/LightmapIO.h>`)
- CamelCase free-function aliases; `leon::games` / `leon_game_host` / `Leon::GameHost`

## [0.10.0] - 2026-08-08

### Added

- Dual-target **PS2 Emotion Engine** path: `Build/toolchains/ps2-ee.cmake`, `LEON_PLATFORM=PS2`, lean `Engine/CMakeLists.Ps2.txt`
- `Plugins/RHI/PS2` (`leon_rhi_ps2`): GS clear/VBlank, unlit triangle/rect primitives, `LPS2` header check
- Platform PS2: `WindowPs2`, `InputPad` (`InitializePad` / `EPadButton`), `Ps2CoreAnchor`
- Canonical pack **`Projects/Ps2Lab`** (`leon-Ps2Lab.elf`): Showcase / PadPilot / StressGrid / ClearOnly demos
- Toolchain smoke **`Samples/Ps2Hello`**; Docker builds via `Scripts/build-ps2-docker.ps1` / `.sh` (`ghcr.io/ps2dev/ps2dev`)
- Host scancode enums `EKey` / `EPadButton` / `EPlatform`; Window no longer exposes `GLFWwindow*`
- Docs: SETUP PS2 section, ASSET_FORMATS `LPS2`, NAMING notes for `leon_rhi_ps2` / Ps2Lab

### Changed

- README and build scripts are PS2-first; Editor no longer links removed host pack gameplay libs
- PS2 RHI layout polish: `Ps2GsContext`, `Ps2DrawPrimitives`, `Ps2DrawUnlitTriangleAt`

### Removed

- In-repo host sample packs: CoopTp, Zombies, Furytoon, ThirdPerson, Smoke (use `Templates/` for New Project)
- Dedicated pack smoke scripts (`smoke-coop-dedicated`, `smoke-packs`)

### Fixed

- PS2 display: real GIF `draw_clear` (bgcolor-only left a black framebuffer in PCSX2)
- Triangle screen-space coords (draw2d +2048 / XYOFFSET); pad init no longer blocks first painted frames

## [0.9.0] - 2026-07-31

### Added

- Physical Engine modules (`Core/`, `Platform/`, `Renderer/`, `Content/`, `Animation/`, `Network/`, `Scene/`, `Gameplay/`, `Utilities/`, `Serialization/`, `RHI/`, `Physics/`)
- Runtime `Application/` (`GameApplication`), `Project/`, `WorldRuntime/`, `Input/`
- Editor layout §6.3 (`Application/`, `Panels/`, `Gizmos/`, `SceneEditing/`, `Importers/`, `Preview/`, `Debug/`)
- Tools `CLI/` (`leon-cli`) + `ResourceTools/` reserved
- `leon_serialization` (`JsonUtil`); expanded `IRHIDevice` (`SetViewport`, `QueryGpuMemory`, active device)

### Changed

- Engine modules no longer include or link glad; DebugDraw/DebugOverlay GPU impl lives in `Plugins/RHI/OpenGL`
- `RunLeonGame` is a thin facade over `GameApplication`
- Docs: as-built layout matched; Phase 3/4 backends deferred (see also [Unreleased] for post-0.9 editor/build hardening)

## [0.8.0] - 2026-07-31

### Added

- Target top-level layout: `Engine/`, `Runtime/`, `Editor/`, `Tools/`, `Plugins/`, `Projects/`, `ThirdParty/`, `Build/`, `Scripts/`, `Tests/`
- Static plugins: `Plugins/RHI/OpenGL` (`IRHIDevice` + Renderer GPU impl), `Plugins/Physics/Arcade` (`IPhysicsBackend` + PhysScene)
- Thin Runtime library `leon_runtime` (alias `leon_game_host`)
- Smoke project `Projects/Smoke` (shipping-style exe, zero Editor)
- Canonical docs under `Docs/` (`ARCHITECTURE.md`)

### Changed

- Independent CMake entries: `cmake -S Editor|Tools|Projects/Smoke` (root still not a mega-project)
- Assets live in `Engine/Assets/`; project packs under `Projects/`
- Facade `leon_engine` links core + default plugins

### Removed

- Pre-migration `engine/` monolith tree and `games/` content root

## [0.7.1] - 2026-07-31

### Removed

- Level JSON aliases: `objects`, `gameplay`, `primitive`, `planeSize`, `solid`, `pushable`, `color`/`direction` on lights, `directionalLights`/`pointLights`, `mesh` as shape name
- LevelCatalog fallbacks to `lvl/` and `scenes/` (only `Levels/`)

### Changed

- Level schema is current-only; ContentValidator errors on removed keys
- Game POST_BUILD stages pack runtime only (`CopyGamePack.cmake`); no nested `build/` / `src/` / CMake under `games/<pack>/`
- `games/build-all.bat` builds every standalone game; per-pack `build.bat` always reconfigures CMake

## [0.7.0] - 2026-07-31

### Added

- `SceneComponent` attach tree (`AttachToComponent` / world transform); Actor root component
- `SkeletalMeshComponent` and `SpringArmComponent` inherit SceneComponent; Character mesh + pack spring arms attach to root
- Editor **Play In Editor** via `PieGameMode` (toolbar Play / Esc Stop); Outliner Actors + Details root transform
- Native Save As dialog; Place Actors **Prop** / **Pawn** placeholders
- `EPhysicsBackend` seam on `PhysScene`; `net::CaptureActorRoot` / `ApplyActorRoot`
- Architecture docs: Actor → SceneComponent → Renderer; pack checklist

### Changed

- Skeletal draws use SceneComponent world matrix (`submitSkeletalDraw` mat4 path)
- Actor↔Level `SyncTransformToLevel` documented as PhysScene mesh sync (not a second visual path)
- ContentValidator accepts Prop/Pawn editor classes
- Architecture docs in English

## [0.6.5] - 2026-07-31

### Changed

- Industry-standard monorepo split: `engine/` and `games/` only own product code
- **Independent builds:** CMake entry is `engine/` (`engine/build`, `engine/build-ninja`); each `games/<name>/` has its own `build/` + `build.bat`
- Root `CMakeLists.txt` refuses configure (not an entry point)
- `leon_game_host` (`engine/host/RunLeonGame`) shared by standalone games
- `third_party/`, `tests/`, scripts, and docs live under `engine/`
- Content paths `game/...` → `games/...`; cook via `engine/scripts/cook.bat` + recipe JSON

### Removed

- Root aggregating all games into one build; `games/CMakeLists.txt` umbrella
- Root `build/` / `build-ninja/` as the engine output location
- Root `third_party/`, `tests/`, `scripts/`, root `.bat` wrappers, `cook-bot*.bat`
- Root `ARCHITECTURE.md` / `SETUP.md` / `LIBRERIES.md` (now `engine/docs/`)

## [0.6.4] - 2026-07-30

### Added

- Unreal-like level editor `leon-editor` (`leon_editor` lib): Dear ImGui docking, Viewport FBO, Outliner, Details, Toolbar TRS (ImGuizmo), Content Browser, Place Actors, File Open/Save
- Asset Import (OBJ / FBX character cook / FBX anim cook) + Asset Preview panel with material override
- `LevelSaver` + loader provenance (`editorClass`, mesh/material paths, `bob`, point-light `orbit`) for JSON round-trip
- `Renderer::setDrawFramebuffer` so shadow/planar passes restore the editor viewport FBO

### Changed

- `ARCHITECTURE.md` / `README.md` / `SETUP.md` document the editor host and controls

## [0.6.3] - 2026-07-30

### Added

- Linux headless build: `scripts/build-linux.sh`, CMake `LEON_BUILD_CLIENT`, UNIX link of Threads/OpenGL
- SETUP.md Linux section (apt packages + configure flags)
- Vendored ENet under `third_party/enet` (stable IDE includes; no FetchContent for ENet)

### Changed

- `MemoryStats` reports process RSS via `/proc/self/statm` on Linux
- GLFW Wayland build disabled by default on Linux (fewer link deps for VPS)

## [0.6.2] - 2026-07-30

### Added

- Headless dedicated server `leon-server.exe` (no OpenGL/window): CPU meshes, fixed tick, Ctrl+C quit; `--port` / `--tick`
- `Engine::initializeHeadless` / `runHeadless`; `ResourceCache` GPU-off path; `StaticMesh`/`SkeletalMesh::CreateCpu`
- Pack `coop-tp` LAN 2P (listen `H`, dedicated `F9` / `--dedicated`, join `C`/`V`) on ENet `NetDriver`
- `game::RegisterGamePacks` — packs own GameMode registration; `app/main.cpp` no longer includes pack headers
- `World::RegisterBodiesFromLevel`, `GameMode::ResolvePlayerStart`, `LevelCatalog::FindIndexByGameModeOrPack`
- `WorldGameplayFrameParams::overridePhysicsStep` for multi-pawn physics; virtual `Character::PerformMovement`

### Changed

- `--dedicated` accepts optional `--gameMode=<id>` (defaults to `coop-tp`)
- Neutral `SpringArmComponent` defaults; packs set boom feel in Character ctors

### Fixed

- Coop dedicated: do not bind server start to **D** (MoveRight); use **F9**; ignore net role keys after connect

## [0.5.44] - 2026-07-30

### Fixed

- TPS M4 aim: blend hand-to-hand barrel with look direction + small yaw bias so the muzzle tracks the crosshair (was biased left from left-handguard offset)

## [0.5.43] - 2026-07-30

### Fixed

- TPS M4 orientation: barrel follows `LeftHand - RightHand` (rifle pose), grip locked to RightHand — no more Mixamo-axis guesswork leaving the gun vertical

## [0.5.42] - 2026-07-30

### Fixed

- TPS M4 socket rotation: map barrel +Z to Mixamo RightHand +X (was +Z→+Z, gun stood vertical in the palm)

## [0.5.41] - 2026-07-30

### Fixed

- TPS M4 fully parented to `mixamorig:RightHand` (bone TRS × mesh socket), no longer reoriented by look aim (was drifting / inverse yaw around the hand)

## [0.5.40] - 2026-07-30

### Fixed

- TPS M4 draws as a Level `StaticMeshComponent` (same opaque path as props) with per-frame model-matrix override from RightHand + aim — fixes ghost/AABB from the old attachment queue path

## [0.5.39] - 2026-07-30

### Fixed

- TPS weapon draw: unlit-only attachments, opaque alpha, face cull (two-sided z-fight looked like a grey ghost); removed debug AABB; pull grip forward into palm

## [0.5.38] - 2026-07-30

### Fixed

- TPS M4: socket uses RightHand **position** + look **aim** (barrel +Z), so the gun is not buried in Mixamo hand axes; attachment draw sets `uModel`/`uUseClipPlane`, polygon offset, bright unlit albedo

## [0.5.37] - 2026-07-30

### Fixed

- TPS weapon visibility: bake M4 to meters with grip at origin; queued static draws use unlit + no cull; attachment world AABB debug

## [0.5.36] - 2026-07-30

### Fixed

- TPS weapon attach: bone space is meters (not cm); palm offset was ~5m and sent gun/muzzle into the sky

## [0.5.35] - 2026-07-30

### Fixed

- TPS M4 scale (was microscopic after character fit); attach offset in bone space

### Added

- TPS hold-LMB auto-fire (~12 rps)

## [0.5.34] - 2026-07-30

### Changed

- TPS polish: continuous look, ADS FOV/boom/lag/speed, crosshair spread, fire muzzle→crosshair aim point
- `Camera::SetFieldOfView` / persistent FOV across resize

## [0.5.33] - 2026-07-30

### Added

- Pack `tps`: rifle Idle/Run cooked clips (`BotRifle.character.json`), `TpsAnimInstance`, M4A1 bone attachment on `mixamorig:RightHand`, LineTrace from muzzle tip
- `leon-cook anim`; `SkelMeshAttachment` + `Renderer::submitStaticDraw`; `AnimInstance::GetBoneWorldMatrices`

## [0.5.32] - 2026-07-30

### Added

- `UserWidget` / `HUD` / `WidgetPaintContext` + screen-space lines/rects on `DebugOverlay`
- Pack `tps`: `CrosshairWidget` at true screen center; character always faces boom yaw (`bOrientRotationToMovement` + `FaceRotation`); LineTrace from eye height

### Removed

- `Engine::SetHudCenterText` (replaced by HUD widgets)

## [0.5.31] - 2026-07-30

### Added

- Pack `tps`: over-right-shoulder bot, RMB ADS zoom, LMB `LineTraceSingleByChannel` with 3s debug draw + HUD crosshair
- `SpringArmComponent::SocketOffsetX`; `Engine::SetHudCenterText`

## [0.5.30] - 2026-07-29

### Changed

- `World` owns `PhysScene`; `TickGameplayFrame` runs Character move → Step → overlaps → Actor Tick → sync → skeletal draw
- TP/Showcase GameMode ticks no longer orchestrate PhysScene/draw/camera; `PlayerController::UpdateCamera`
- `AnimInstance` is locomotion-only; jump SM lives in framework `CharacterAnimInstance`; pack `ThirdPersonAnimInstance` wires clips + rates
- `SkeletalMeshComponent` stores named `AnimSequence` map; pack `NativeInitializeAnimation` binds BlendSpace/jump clips
- Engine public API PascalCase (`GetLevel`, `GetCamera`, `SetOrbitMouseEnabled`, …); camelCase aliases kept

## [0.5.29] - 2026-07-29

### Changed

- Pack-owned AnimClass: `SkeletalMeshComponent` holds polymorphic `AnimInstance`; Mixamo jump/land rates + Idle↔Run ease live in `ThirdPersonAnimInstance` (game pack), not engine defaults
- Engine `AnimInstance` defaults are neutral (play rate 1, snap blend, short crossfade); `NativeInitializeAnimation` hook for subclasses

## [0.5.28] - 2026-07-29

### Improved

- Jump/Land play rates (~2.6× / ~3×) so Mixamo clips match Character jump timing
- Longer Land → Locomotion crossfade; idle↔run BlendSpace input eases instead of snapping

## [0.5.27] - 2026-07-29

### Added

- AnimInstance jump state machine: `JumpStart` → `FallLoop` → `Land` → `Locomotion` with smoothstep crossfade between clips
- `AnimSequence::bLooping` (one-shot Jump / Land clamp; Fall loops)
- Character `GetVelocityZ()` / `ConsumeJustLanded()`; `NotifyJumped` on leave-ground
- Cook optional `--jump` / `--fall` / `--land` FBX → `Anims/JumpingUp|FallingIdle|FallingToLanding` + `character.json` keys
- Bot Mixamo jump clips wired via `cook-bot.bat`

### Changed

- `SkeletalMeshComponent::LoadFromCooked` loads jump clips and `SetupJumpStateMachine`
- Version `0.5.27`

## [0.5.26] - 2026-07-29

### Added

- Unreal-like collision traces on `PhysScene`: `LineTraceSingle/MultiByChannel`, `SphereTraceSingle/MultiByChannel`, `CapsuleTraceSingle/MultiByChannel` (`HitResult`, `ECollisionChannel`, `CollisionQueryParams`)
- `EDrawDebugTrace::ForOneFrame` + `DrawDebugLine/Sphere/CapsuleTrace` (F2 shows Character floor SphereTrace in third-person)
- `Character::IsFalling()` (pair of `IsMovingOnGround`); floor snap uses `SphereTraceSingleByChannel`
- Third-person template drives BlendSpace idle while `IsFalling()`

### Improved

- Trace `HitResult`: keep Unreal-like `Location` (sweep center) vs `ImpactPoint` (surface); Single helpers share `takeNearestHit`
- F2 / `EDrawDebugTrace::ForOneFrame` docs in README + ARCHITECTURE; dedicated `TraceDebugDrawTests`
- `LoadSkeletalMeshCooked` returns optional material path (no second JSON parse in `LoadFromCooked`)
- `format.bat` / `lint.bat` cover `tools/` and `tests/`; `test.bat` finds VS 18 (2026) VsDevCmd
- Docs: `animation/` + `content/` domains, cook-bot tooling, AnimSequence pose comments

## [0.5.25] - 2026-07-29

### Added

- Unreal-like skeletal cook pipeline: `leon-cook character` → Skeleton / SkelMesh / Material / Anims / BlendSpace1D / `*.character.json`
- Runtime `SkeletalMeshComponent::LoadFromCooked` (TP pack uses `Bot.character.json`)

### Changed

- Moved Iron Man sample to `assets/characters/iron-man/` (kebab-case); removed empty `assets/samples/`

## [0.5.24] - 2026-07-29

### Added

- FBX skeletal meshes via vendored ufbx: `SkeletalMesh`, `AnimSequence`, `BlendSpace1D`, `AnimInstance`
- Unreal-like `SkeletalMeshComponent` on `Character::GetMesh()` (TickComponent → NativeUpdateAnimation)
- GPU skinning shader (`skinned_lit.vert`) + `Renderer::submitSkeletalDraw` / `Character::SubmitMeshDraw`
- Third-person template loads Mixamo bot on GetMesh() (`Breathing Idle.fbx` + `Running.fbx`)
- Catch2 unit tests (`leon_tests` / `test.bat`): BlendSpace1D, AnimInstance, FBX bot load
- Fix FBX triangulation (`ufbx_triangulate_face` returns triangle count, not index count)
- Fix skeletal idle T-pose: bake `node_to_world` skins; prefer longest Mixamo anim stack (`mixamo.com`)

## [0.5.23] - 2026-07-29

### Improved

- Physics pass: AABB separation on min-penetration axis (X/Y/Z) so crates stack instead of sliding apart
- Dynamic bodies can rest on other dynamics; landing snap + resting friction; mass-weighted capsule push
- Character vertical land window mirrors prop snap (no side-mount teleport)

## [0.5.22] - 2026-07-29

### Fixed

- Dynamic props no longer teleport onto tall static AABBs when pushed into their sides (floor snap only when landing from above; XZ resolve before snap)

## [0.5.21] - 2026-07-29

### Changed

- Layout: `Transform` → `core/`, `Frustum`/`Aabb` → `render/`; `LevelAnimation` own header
- `LevelEntry.gameMode` read once at catalog scan (GameplayRouter no longer reopens JSON)
- F2 collision debug is an Engine tool flag (not Renderer)
- `PhysScene::Step` runs from GameMode tick; `Character::PerformMovement` is movement-only (Unreal-like)

## [0.5.20] - 2026-07-29

### Changed

- TP template: camera comes from pawn SpringArm + PlayerStart (no level `camera` JSON); `camera` remains optional for Showcase/Default framing

## [0.5.19] - 2026-07-29

### Changed

- Lights match Unreal Details: `rotation`/`position`/`scale`, `lightColor`, `intensity`, `castShadows`, `sourceAngle` (no `direction` field; legacy `direction`/`color` still load)

## [0.5.18] - 2026-07-29

### Changed

- HUD: F1/F2 AABB/Collision status bottom-left; active level browser bottom-right (stats stay top-right)

## [0.5.17] - 2026-07-29

### Added

- Unreal-like `PlayerStart` level actor (transform only) — GameMode spawns the possessed pawn there
- `Level::PlayerStarts()` / `FindPlayerStart()`; TP template spawns default Character mesh from GameMode

### Removed

- Level `"tag": "player"` mesh as the pawn — use `PlayerStart` + GameMode spawn instead

## [0.5.16] - 2026-07-29

### Removed

- `plantOnGround` level JSON flag — use `fitHeight` (scales and ground-aligns) or explicit `position` / `scale`

## [0.5.15] - 2026-07-29

### Changed

- Removed deprecated `solid` / `pushable` API fields — use Unreal-like `collisionEnabled` + `simulatePhysics`
- Loader still accepts legacy JSON keys `solid`→`collisionEnabled` and `pushable`→`simulatePhysics` for old levels

## [0.5.14] - 2026-07-29

### Added

- Unreal-like `enableGravity` on level actors / PhysScene Dynamic bodies (`bEnableGravity`)
- Dynamic bodies with gravity fall onto static supports / `floorY`

## [0.5.13] - 2026-07-29

### Changed

- Level JSON / `StaticMeshComponent`: Unreal-like `simulatePhysics` (dynamic PhysScene body)
- Legacy `"pushable": true` still accepted as an alias of `simulatePhysics`
- `solid` = static collision; `simulatePhysics` wins (Dynamic) when both are set

## [0.5.12] - 2026-07-29

### Added

- `"class": "BlockingVolume"` — Unreal-like invisible solid box (`solid` + `hidden` by default)
- `StaticMeshComponent::hidden` — skipped by the renderer; still collides in PhysScene
- Third-person level: four BlockingVolumes around the floor edge

### Notes

- Character jump uses arcade gravity: `velocityY += jumpSpeed`, then `velocityY -= gravity * dt` (not a full rigid-body solver)

## [0.5.11] - 2026-07-29

### Changed

- Showcase orbit (Demo / Iron Man): `ShowcaseOrbitActor` drives the view via `SpringArmComponent` (camera lag, rotation lag, smoothed zoom) — same feel as third-person

## [0.5.10] - 2026-07-29

### Changed

- `SpringArmComponent`: Unreal-like camera lag, rotation lag, and smoothed arm length (TP feel)
- Third-person move uses desired boom yaw (responsive) while the view lags slightly

## [0.5.9] - 2026-07-29

### Changed

- HUD: compact top-right two-column stats (`FPS`/`MS`, `RAM`/`VRAM`, `TRIS`/`OBJ`, `RES`)
- On-screen debug console moved to top-left (smaller font); default color red; newest stays put, older lines shift down
- Level browser chrome offset below the stats block

## [0.5.8] - 2026-07-29

### Changed

- Showcase (Demo / Iron Man): orbit + scroll only — no WASD pivot move
- LevelDirector: digit keys `1`–`9` (and keypad) jump to catalog levels

## [0.5.7] - 2026-07-29

### Changed

- Cursor captured + hidden by default (`GLFW_CURSOR_DISABLED`) for all levels — continuous mouse look/orbit without holding LMB
- Level browser chrome mouse clicks disabled while cursor is captured (use `[` `]` to switch levels)

## [0.5.6] - 2026-07-29

### Fixed

- Showcase: disable keyboard orbit so WASD only moves the pivot (no yaw/pitch tumble conflict)
- Showcase `OnExit` restores orbit mouse/keyboard flags like other modes
- Third-person scroll zoom: `Engine::ConsumeScrollY` + `SpringArmComponent::AddArmLengthInput` (boom length)

### Changed

- `SpringArmComponent`: arm length min/max, `ClampPitch` when seeding from level camera
- Docs: showcase custom GameMode; 0.5.4 wording aligned with later rename

## [0.5.5] - 2026-07-29

### Changed

- Renamed pack `walk` → `third-person-template` (kebab-case folder)
- Classes: `ThirdPersonTemplateGameMode` / `Character` / `PlayerController`; GameMode id `"third-person-template"`

## [0.5.4] - 2026-07-29

### Added

- `SpringArmComponent` (Unreal-like camera boom) for third-person follow
- Showcase pack GameMode: `ShowcaseGameMode` + `ShowcaseOrbitActor` + `ShowcasePlayerController` (orbit viewer)
- Third-person pack (then named `walk`): boom look (LMB), camera-relative move, Jump

### Changed

- Showcase levels `01`/`02` use `"gameMode": "showcase"`; `03_default_camera` stays `"Default"` (freelook)
- Third-person level display name set to "Third Person"

## [0.5.3] - 2026-07-29

### Changed

- `DefaultCameraActor`: Unreal-like free-look fly (LMB mouse look, WASD along view, Q/E up/down) — no orbit pivot
- `Camera`: `ECameraMode::Orbit` | `FreeLook` with `ForwardVector` / `RightVector` / `SetEyeLocation`
- Engine: `setOrbitMouseEnabled`; FreeLook uses LMB for look and disables scroll zoom

## [0.5.2] - 2026-07-29

### Added

- Input mapping system (Unreal-like): `InputMappingContext`, `PlayerInput`, `InputActions::*`
  - Default context: WASD+arrows → `MoveForward`/`MoveRight`, Q/E → `MoveUp`, Space → `Jump`
  - `Engine::input()`; sampled once per frame via `PlayerInput::Update`
  - Remap by `ClearContexts` / `AddMappingContext` (no hardcoded WASD in gameplay controllers)

### Changed

- Walk / DefaultPlayerController / keyboard orbit tumble read mapped axes/actions instead of raw GLFW keys

## [0.5.1] - 2026-07-29

### Added

- `DefaultCameraActor` + `DefaultPlayerController`: default pawn for `DefaultGameMode` (free-look fly)
- `Engine::AddOnScreenDebugMessage` / DebugOverlay timed Print-String messages (bottom-left, fade on expiry)
- Showcase level `game/showcase/Levels/03_default_camera.json` (`"gameMode": "Default"`)

### Changed

- `DefaultGameMode` spawns/possesses `DefaultCameraActor` and disables keyboard orbit while active

## [0.5.0] - 2026-07-29

### Changed

- **Breaking:** Unreal-style public naming (no `U`/`A`/`F` prefixes)
  - `GameWorld` → `World`; `getGameWorld` → `GetWorld`
  - Gameplay methods PascalCase where UE uses them (`BeginPlay`/`EndPlay`/`Tick`, `Possess`/`UnPossess`, `SpawnActor`, `GetActorLocation`, …)
  - Level visual `StaticMeshActor` → `StaticMeshComponent`; `actors()` → `StaticMeshes()` / `AddStaticMesh` / …
  - `PhysScene` / `Level` kept (≈ `FPhysScene` / `ULevel`); APIs PascalCase (`SyncFromLevel`, `Step`, …)
  - `levelActorIndex` → `levelMeshIndex` / `LevelMeshIndex`
  - `CharacterMotor` → `CharacterMovement`; `AddMovementInput` / `Jump` / `IsMovingOnGround` / `PerformMovement`
  - GPU `Mesh` → `StaticMesh`; `LoadStaticMesh` / `GetCubeMesh` / …
  - `Renderer::drawScene` takes `const Level& level` (not `scene`)
  - Removed unused `resolveScenesDirectory` (use `resolveGamesDirectory`)
- Docs: `World` (gameplay Actors) vs `Level` (map content) vs `PhysScene`; UE name mapping without prefixes

## [0.4.44] - 2026-07-29

### Added

- `ContentValidator`: validates level + material JSON on load (structure, types, referenced assets)
- Errors reject the level/material; warnings (missing optional textures/HDR) still allow load

## [0.4.43] - 2026-07-29

### Fixed

- LevelCatalog: use `Levels/` exclusively when present (legacy `lvl/` / `scenes/` only as fallback) — stops 3× duplicate levels from stale POST_BUILD copies

## [0.4.42] - 2026-07-29

### Added

- Engine `BasicLight` (`DirectionalLight` / `PointLight`); level JSON `"lights": [{ "class": "..." }]`
- Legacy `directionalLights` / `pointLights` still work when `lights` is absent
- `leon::AsciiToLower` helper for case-insensitive class / primitive name parsing
- `scripts/configure-ninja.bat` — Ninja build + `compile_commands.json` for clang-tidy / clangd
- `SETUP.md` — new-machine dependencies, first build, troubleshooting

### Changed

- Showcase + Walk levels use unified `lights[]` with class
- `build.bat` / `format.bat` / `lint.bat` detect VS 2022/2026 Community (or Professional) paths

## [0.4.41] - 2026-07-29

### Changed

- UV tiling (`uvScale` / `tiling`) lives on `Material` (shader `uUvScale`), not on Plane/BasicShape — Unreal-like

## [0.4.40] - 2026-07-29

### Added

- Engine `BasicShape` (`Cube` / `Sphere` / `Plane`): unit meshes + transform + material (Unreal Basic Shapes style)
- Level JSON `"class": "Cube"|"Sphere"|"Plane"` (alias `"primitive"`; legacy `"mesh": "cube"` still works)

### Changed

- Showcase + Walk levels place primitives via `"class"`; Plane size uses `scale` (unit mesh)

## [0.4.39] - 2026-07-29

### Added

- Material assets: `assets/Materials/` + `game/<pack>/Materials/*.json`; actor `"material": "path"`
- Engine default `M_Default` grayscale checker (Unreal-like) when procedural meshes omit material/MTL
- `ResourceCache::loadMaterial` / `defaultMaterial`; `MaterialAsset` helpers

### Changed

- Showcase + Walk levels reference material assets instead of inline albedo/specular/…

## [0.4.38] - 2026-07-29

### Changed

- Walk pack: `WalkCharacter` + `WalkPlayerController` (motor/capsule/input in C++; level JSON only places actors)
- `PlayerController::tickInput` is virtual (default no-op; Walk overrides)

## [0.4.37] - 2026-07-29

### Changed

- Pack level folders: `game/<pack>/scenes/` → `game/<pack>/Levels/` (catalog falls back to legacy `lvl/` / `scenes/` only if `Levels/` is empty)
- Level GameMode override: JSON `"gameMode"` (legacy `"gameplay"`); `DefaultGameMode` when unset
- `GameplayRouter::setDefaultMode`; Walk registered as override; showcase uses Default

## [0.4.36] - 2026-07-29

### Changed

- Level JSON: `actors[]` (legacy `objects[]`); per-actor `tag` / `solid` / `pushable` on `StaticMeshActor`
- Walk: PhysScene bodies from Level metadata; player via `tag: "player"`/`"pawn"`; motor config under `"walk": {}`
- `Level::findActorIndexByTag`; LevelAnimation `ActorSpin`/`ActorBob`

## [0.4.35] - 2026-07-29

### Added

- `GameInstance` (session, owned by `Engine`) and `PlayerState` (per-player, owned by `PlayerController`)
- `GameState` remains on `GameMode`; Walk resets/ticks GameState + PlayerState and notifies GameInstance on enter

### Changed

- `GameMode::onEnter` path arg renamed `levelJsonPath`
- `Engine::setGameInstance` calls shutdown/init when replacing a live instance
- `PlayerController.h` forward-declares `Engine` (include only in `.cpp`)

## [0.4.34] - 2026-07-29

### Changed

- Unreal-style rename: `Scene` → `Level` (`StaticMeshActor`, `LevelCatalog`/`Loader`/`Director`; folder `scene/` → `level/`)
- Physics: `CapsuleShape`, `EBodyType`, `BodyInstance`, `PhysScene`; sync `syncFromLevel`/`syncToLevel`; `levelActorIndex`
- `Engine::level()`; Actor `levelActorIndex` / `syncToLevel`; disk path `game/*/scenes/*.json` unchanged

## [0.4.33] - 2026-07-29

### Fixed

- `Controller` destructor defined out-of-line; idempotent `Pawn`/`Actor` destroy
- `AIController` arrive radius clamped; steer uses explicit length (no zero normalize)

### Changed

- Docs: Scene vs GameWorld vs PhysicsWorld table; README `modelYawOffset` default note

## [0.4.32] - 2026-07-29

### Changed

- Renamed gameplay `World` → `GameWorld` (`getGameWorld`) to distinguish from `Scene` and `PhysicsWorld`

## [0.4.31] - 2026-07-29

### Added

- `GameState` (match timer / in-progress) owned by `GameMode`; `setGameState<T>()` for subclasses
- `AIController` — wish direction or move-to-target steering for a possessed Character

### Changed

- Destroying / clearing a possessed `Pawn` auto-`unPossess`es (`destroy` + `endPlay`)
- Walk uses `player_.getCharacter()` and ticks `GameState`

## [0.4.30] - 2026-07-29

### Added

- Essential Unreal-style layer: `World` (spawn/tick/destroy), `Pawn`, `Controller`
- `GameMode` owns a `World`; `Character` is a `Pawn`; `PlayerController` extends `Controller`

### Changed

- Walk: spawns `Character` via `world_.spawnActor`, then `player_.possess`

## [0.4.29] - 2026-07-29

### Changed

- Unreal-style gameplay hierarchy: `Actor` → `Character`, `PlayerController`, `GameMode` (replaces `SceneMode` / `CharacterController`)
- Walk pack: `WalkController` → `WalkGameMode` (owns `PhysicsWorld` + `Character` + `PlayerController` possession)

## [0.4.28] - 2026-07-29

### Changed

- Project renamed **Geon → Leon Engine** (`leon` namespace, `include/leon/`, targets `leon_engine` / `leon_game` / `leon-engine`)

## [0.4.27] - 2026-07-29

### Changed

- Top-center HUD always shows `F1 AABB ON/OFF` and `F2 Collision ON/OFF`

## [0.4.26] - 2026-07-29

### Fixed

- Capsule vs dynamic: split depenetration + cancel velocity into contact (no post-step overlap stick)
- Body–body: remove velocity into MTV instead of blunt damp
- JSON: `solid` wins over `pushable` when both are set

### Changed

- `skipSceneIndex` renames the old ignore index; capsule F2 debug shows end-cap rings

## [0.4.25] - 2026-07-29

### Changed

- Physics bodies own position + AABB; explicit `syncFromScene` / `syncToScene`
- `PhysicsWorld::step()` unifies dynamics + body collisions; character uses `move()`
- Collision debug draws into `DebugDraw` (no Renderer dependency in physics)

## [0.4.24] - 2026-07-29

### Changed

- Physics/collision moved to engine domain `leon/physics/` (`Collider`, `PhysicsBody`, `PhysicsWorld`)
- `CharacterController` is a motor that consumes `PhysicsWorld`; Walk registers static/dynamic bodies from JSON

## [0.4.23] - 2026-07-29

### Fixed

- Capsule vs solid side contacts no longer vibrate (MTV separation; player always depenetrates)
- Pushable props collide with solids and each other (AABB XZ resolve by inverse mass)

## [0.4.22] - 2026-07-29

### Added

- Top-center HUD label `Collision Debug ON` while F2 collision overlay is active

## [0.4.21] - 2026-07-29

### Added

- **F2** toggles collision debug: character capsule + solid/pushable prop AABBs (orange = pushable, blue = solid)

## [0.4.20] - 2026-07-29

### Added

- `ARCHITECTURE.md` — layers, domain folders, runtime/render flow, pack rules

### Changed

- README links architecture; documents mirrored `src/` + tooling `cd /d "%~dp0"`
- `format.bat` / `lint.bat` / `build.bat` always run from the repo root

## [0.4.19] - 2026-07-29

### Changed

- `src/` mirrors `include/leon/` domains: `core/`, `scene/`, `render/`, `debug/`, `gameplay/` (`Engine.cpp` stays at `src/` root)

### Removed

- Unused `scripts/` (one-shot demo texture generator)

## [0.4.18] - 2026-07-29

### Changed

- Public headers organized by domain under `include/leon/`:
  - `core/` — Window, Paths, Camera, Input, MemoryStats
  - `scene/` — Scene, catalog/loader/director, Light, Transform, Frustum
  - `render/` — Renderer, meshes, shaders, textures, shadows, IBL, GL helpers
  - `debug/` — DebugDraw, DebugOverlay
  - `gameplay/` — SceneMode, GameplayRouter, CharacterController (unchanged)
- Root keepers: `Engine.h`, `Gameplay.h` (facades)

## [0.4.17] - 2026-07-29

### Changed

- Gameplay framework (`SceneMode`, `GameplayRouter`, `CharacterController`) moved into engine core: `include/leon/gameplay/` + `src/`
- `game/` holds only pack business logic; each pack uses `scenes/` + `src/` (headers beside sources)
- Walk: `game/walk/scenes/` + `game/walk/src/{WalkController.h,cpp}`

## [0.4.16] - 2026-07-28

### Fixed

- Capsule no longer snaps onto cube tops when approaching from the side (support only if feet are near the top)
- Pushable props get a visible shove + short slide velocity; immovable `solid` platforms block the capsule

### Changed

- Walk arena: large `solid` climb platforms + small `pushable` crates/spheres

## [0.4.15] - 2026-07-28

### Added

- `game::CharacterController`: reusable kinematic capsule (movement / jump / solids); visual mesh is cosmetics only via `syncVisual`
- Walk mode is a thin SceneMode around CharacterController (Unity-style CharacterController pattern)

## [0.4.14] - 2026-07-28

### Changed

- Walk uses a logical capsule for movement; Iron Man is visuals-only (`syncVisual`)
- Capsule can land on `"pushable"` / `"solid"` cube tops after a jump; green debug wire shows the collider

### Added

- Walk JSON: `capsuleRadius`, `capsuleHeight`, `floorY`, `stepUp`, `visualIndex`

## [0.4.13] - 2026-07-28

### Added

- Walk pack layout: `game/walk/scenes/` + `game/walk/src/` (headers beside the pack)
- Pushable cubes (`"pushable": true`) shoved by the walk player on XZ contact
- Walk Space jump + exponential yaw turn (already wired; facing offset fixed to 180°)

### Changed

- Default / scene `modelYawOffset` set to 180 so Iron Man faces the move direction

## [0.4.12] - 2026-07-28

### Changed

- Scenes live under each game pack: `game/<pack>/scenes/*.json` (no top-level `scenes/`)
- Walk facing: default `modelYawOffset` 0; smooth exponential yaw turn; Space to jump

### Added

- `SceneCatalog::scanGamePacks` + `resolveGamesDirectory()`

## [0.4.11] - 2026-07-28

### Added

- Layered build: `leon_engine` (core) + `leon_game` (scene modes) + `app/main` (host)
- `game::SceneMode` + `GameplayRouter` for pluggable per-scene logic
- Walk mode (`game/walk`, scene `03_walk.json` with `"gameplay": "walk"`)
- Engine `Input` helpers: `readWasdAxes`, `cameraRelativeMoveXZ`

### Changed

- SceneLoader ignores gameplay-only JSON keys; modes read their own config
- Project layout documents `app` → `game` → `leon_engine` dependency rule

## [0.4.8] - 2026-07-28

### Added

- HUD memory stats: process RAM working set + private (`ram` / `priv`), GPU VRAM via DXGI (fallback NVX/ATI)

## [0.4.7] - 2026-07-28

### Added

- GPU pass timers in the HUD (`sh` / `mir` / `col` ms, previous-frame, non-blocking)

### Changed

- Planar mirror at half resolution (`Renderer::kPlanarReflectionScale = 0.5`)
- Reflection pass: frustum cull, no shadows, flat normals (cheaper mirror)
- Lit pass binds environment + shadow map once per pass (not per object)

## [0.4.6] - 2026-07-28

### Added

- Planar mirror reflections for ground planes (`"planarMirror": true`): scene objects (spheres, Iron Man, …) appear in the floor, not only HDR/env lighting

### Changed

- Env cubemap anti-flicker uses lock-risk from `fwidth(R)` so large floors stay sharp instead of max-LOD blur

## [0.4.5] - 2026-07-28

### Changed

- Restored the pre-IBL lighting look (ambient 0.10, per-sample env tonemap, no final film curve / sRGB / irradiance ambient)
- Demo + Iron Man scenes restored to the UBO/roughness-era balance
- Kept UBO camera/lights, cubemap roughness LOD, and frustum culling
- Skybox exposure ×1.45 vs material env samples (brighter backdrop without washing spheres)
- Diffuse irradiance IBL: replaces flat `0.10` ambient (`k=0.20`, same per-sample tonemap; no final film curve)
- Flat-face env flicker: roughness floor 0.18 + `fwidth(R)` LOD AA (specular cubemap only)

## [0.4.4] - 2026-07-28

### Changed

- Demo/Iron Man lighting and materials dialed back (exposure, lights, albedo/specular/shininess)
- Procedural bump softer; floor no longer uses bump
- Lit pass: uniform albedo/specular converted sRGB→linear; weaker specular env strength

### Fixed

- Blinn specular no longer blows to white crescents (`(N+8)/8` energy scale removed)
- Specular fireflies from bump: stronger lobe AA + softened tangent normals
- Dark HDR sky: higher scene exposure + skybox exposure boost

## [0.4.3] - 2026-07-28

### Fixed

- Hot-reload: compile/link/read failures advance timestamps; F5 reports Failed vs Reloaded correctly
- Shader paths resolve per-file (newest wins) so repo edits beat a stale POST_BUILD copy
- Scene load aborts on any mesh failure (no partial commit); `"version": 1.0` accepted
- Multi-MTL JSON patches slots instead of collapsing; light overflow warns + truncates
- OBJ index bounds-check; near-zero scale sanitized; shadow pass disables cull (one-sided casters)
- Albedo textures as sRGB; cutout discard before lighting; HUD/chrome included in F5 reload

## [0.4.2] - 2026-07-28

### Fixed

- Hot-reload: advance file timestamps when UBO accept fails (no per-frame relink spam)
- Shadow pass draws only caster submeshes (skips transparent / unlit / `castsShadows: false`)
- Unified lit tonemap with skybox; cutout discard for opaque textured alpha
- Shadow/IBL balance (less crushed umbra); finer light AABB snap (0.5 m); milder bias/offset
- Debug lines: drop invalid `GL_POLYGON_OFFSET_LINE` (use `GL_LEQUAL`)
- `bob` defaults `baseY` to planted Y; missing scene `camera` resets to defaults

## [0.4.1] - 2026-07-28

### Fixed

- Draw order: opaque → skybox → transparent (glass no longer erased by the sky)
- Ground z-fighting: plant epsilon + slight demo lifts; softer procedural bump / less floor tiling
- Shadow bias uses geometric normal (not bump); fill lights dampen inside the primary shadow
- Specular Blinn screen-space AA; safer `normalize` when L≈−V
- Equirect→cubemap U wrap (horizon seam)
- Tangent `w` handedness for mirrored UVs (Iron Man / OBJ)
- Transparent sort by world AABB center; lit hot-reload reverts if UBO bind fails
- IBL ambient scaled when directional lights are present (less double-counting)

## [0.4.0] - 2026-07-28

### Added

- Diffuse irradiance IBL: CPU Lambertian convolution → low-res cubemap; lit ambient uses `uIrradianceMap`
- Shader hot-reload: timestamp watch + **F5** force-reload (failed compiles keep the previous program)

### Changed

- Env HDR load builds specular mips and an irradiance cubemap
- Ambient term no longer a fixed `0.10 * diffuseColor` when IBL is present

### Fixed

- Flat shiny faces no longer flicker bright/dark at distance (env LOD floor + screen-space reflection AA)
- Shadow acne/flicker: polygon offset, stronger bias, quantized caster AABB for stable light frustum

## [0.3.0] - 2026-07-28

### Added

- Scene JSON `"version": 1` (required; loader rejects missing/unsupported versions)
- Camera frustum culling by world AABB (`Frustum` / `Aabb`) with measurable `FrameStats`
- HUD: `visible/total` objects, culled count, submitted tris/draws
- std140 UBOs for camera and lights (`UniformBuffer`, bindings 0/1) on the lit pass
- Material `roughness` (JSON or derived from `shininess`) → cubemap `textureLod` blur

### Changed

- Shadow depth texture filter is `GL_NEAREST` (correct for manual PCF)
- Env cubemap reports mip count / max LOD for roughness sampling
- Demo metals expose explicit roughness steps (mirror → brushed)

## [0.2.0] - 2026-07-28

### Added

- JSON scene system under `scenes/` (`SceneCatalog`, `SceneLoader`, `SceneDirector`)
- Top-right scene browser UI (`< name >`) with mouse arrows and `[` / `]` keys
- Multi-material OBJ/MTL loading (submeshes, `Kd` / `Ks` / `Ns` / maps)
- Material `specular` and `metallic` with Blinn-Phong highlights and HDR cubemap reflections
- HDR environment maps (`EnvMap`: equirect `.hdr` → RGB16F cubemap + mips), skybox shaders,
  scene JSON `environment` / `environmentExposure`
- Debug draw layer (`DebugDraw`): object world AABBs + directional light 0 ortho frustum (**F1**)
- HUD stats: FPS, ms, tris, draws, materials, lights, resolution
- Sample scenes: `01_demo.json`, `02_ironman.json`
- Iron Man sample under `assets/samples/IronMan/`
- Sample HDR sky: `assets/Hdr/AutumnFieldPuresky1k.hdr`
- Tooling: `build.bat`, `format.bat`, `lint.bat`, `.clang-tidy`, `.clang-format`
- English README (build, format/lint, controls, JSON schema)
- nlohmann/json dependency for scene files

### Changed

- Demo content moved out of C++ into JSON (removed hard-coded `DemoScene`)
- Lit shader uses per-material specular/metallic instead of a fixed specular tint
- Procedural fake sky kept as fallback when no environment map is set
- clang-tidy config tuned for OpenGL / GLFW / GLM noise
- `Vertex` vs `MeshData`/`SubMesh` headers split for clearer naming

### Fixed

- Sphere triangle winding (outward normals / back-face culling)
- Scene switch only advances catalog index after a successful load
- JSON material fields set `materialOverride` so they win over MTL
- Scene load is atomic (staging `Scene` + deferred camera on success)
- `fitHeight` / `plantOnGround` keep JSON `position` as a post-fit offset
- HiDPI scene-browser hit-test: separate window vs framebuffer sizes

## [0.1.0] - 2026-07-28

### Added

- Initial forward renderer (OpenGL 3.3 / C++20): window, camera, meshes, textures
- Blinn-Phong and unlit shading, directional + point lights
- Shadow map (directional light 0) with PCF
- Normal mapping (TBN / tangents)
- Resource cache, procedural primitives (cube, plane, sphere)
- Transparent object queue (back-to-front)
- CMake + Visual Studio build (`build.bat`)

[0.10.0]: https://github.com/danielbarretoes/geon/compare/v0.9.0...v0.10.0
[0.9.0]: https://github.com/danielbarretoes/geon/compare/v0.8.0...v0.9.0
[0.4.5]: https://github.com/danielbarretoes/geon/compare/v0.4.4...v0.4.5
[0.4.4]: https://github.com/danielbarretoes/geon/compare/v0.4.3...v0.4.4
[0.4.3]: https://github.com/danielbarretoes/geon/compare/v0.4.2...v0.4.3
[0.4.2]: https://github.com/danielbarretoes/geon/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/danielbarretoes/geon/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/danielbarretoes/geon/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/danielbarretoes/geon/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/danielbarretoes/geon/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/danielbarretoes/geon/releases/tag/v0.1.0
