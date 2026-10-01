# Pending

What is left after [ps2-polish](PLANS/ps2-polish.md) (0.25.0), on top of what the
[ps2-shipping](PLANS/ps2-shipping.md) plan (0.22.0 to 0.24.0) left open. Each item comes from a phase's "Desviaciones";
the plans have the detail, and [Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md) has the measurements.
ShooterGame runs at 30 fps in PCSX2 (Budgets.md, the row "0.25.0"), so nothing here blocks the frame rate.

## Next features

- **Minimap** (ps2-polish P7, not done). The radar still shows dots on a black square. It needs the map's real overview
  image, like CS's overviews: a top-down render of the map, generated at cook time and paletted, drawn under the dots
  and rotated with the view. An uncompiled draft (a `BuildOverview` commandlet, the scene capture, the radar and its
  tests) is on the branch `wip/ps2-polish-p7-minimap`, under `Docs/PLANS/ps2-polish-p7-draft/`; it predates P8's
  `AWorldSettings` and renderer changes, and it leaves open whether LeonCook should link Renderer and GSReference or
  the overview should be a separate tool.

- **Death and spectator camera.** On death the view does not jump straight to a teammate: for about 3 seconds a
  third-person camera orbits the player's body (CS's death cam), then it moves to a live teammate. Spectating a
  teammate is not first person only: a button toggles between their first-person view and a third-person orbit camera
  around them (CS's in-eye and chase modes), and another cycles the teammate.

ShooterGame stays single player against bots: no split screen (decided 2026-09-30).

## Engine

Proposed 2026-10-01, in priority order after the minimap; some expand items of the sections below.
- **The engine knows nothing of the game.** The one leak: `MemoryCardSaveGameSystem.cpp` draws the memory card icon with
  ShooterGame's green crosshair. The icon should be a project setting or asset (the project's save icon), not engine
  code. The other mentions of ShooterGame in `Engine/Source` are comments and tests.
- **Compound collision** (several `UCX_` boxes per mesh), so de_leon's pieces can be merged per cell and material:
  fewer draws.
- **LODs for baked meshes**: bake the vertex lighting per LOD, so LODs work on baked maps (today a baked mesh always
  draws LOD 0).
- **Per-bone hitboxes** instead of capsule height bands, closer to CS.
- **Disc streaming** with fileXio or `sceCdRead`, and music streaming with an IOP module; it also cuts the 2.9 s load.
- **UObject arenas** and write barriers for the incremental GC.
- **Four point lights per draw** on VU1, with a new VU1 memory layout.
- **Movable lights on the static world** and light probes for pawns.
- **An editor.** The foundation is there (reflection with LeonHeaderTool, versioned packages, LeonEd's factories and
  commandlets, Slate/UMG and the OpenGL RHI); missing are an editor application (viewport, outliner, a details panel
  generated from `UPROPERTY`, gizmos, selection), undo/redo transactions (`FTransaction`), saving an edited map to a
  package (maps come from glTF made by Blender scripts today), asset hot reload, and edit metadata (`EditAnywhere`,
  `Category`, `ClampMin`) read by LeonHeaderTool. The cheapest start: Dear ImGui over the OpenGL RHI with panels
  generated from reflection, placing actors in a map and saving it.
- **A navmesh** instead of hand-placed waypoints, now that there is more than one map.

## Render

- A draw takes at most two point lights (the two that light its bounds most, ps2-polish P8b): VU1 lights two. It could
  take four with a new VU1 memory layout.
- Cells and portals cull little on de_leon's open layout: the sky portals keep most cells visible.
- Static mesh LODs work and are tested, but de_leon uses none: its meshes are baked, and a baked mesh draws at LOD 0.
- de_leon's 78 pieces are not merged per cell and material. A mesh has a single `UCX_` box, because Leon folds a mesh's
  boxes into one AABB; merging needs compound collision (several boxes per mesh).
- No light probes for moving objects (pawns take the sky without occlusion), and Movable lights do not light the static
  world.
- The `GSH_Capture` headless GS dump (plan D3) was never built; VU1 is validated with VU1Conformance in PCSX2.
- The 512-line PAL frame (448 lines, centred, today) and CSM2 CLUTs are not supported.

## Disc and memory

- Loading is bound by the disc: 2.9 s to the first frame with the ordered pak, because everything the match can need is
  preloaded.
- Reads go through the ROM's FILEIO, not fileXio or `sceCdRead`. No music streaming (it needs an IOP module).
- The ISO has no license logo or system sectors, so it boots in PCSX2 only. That is enough: the target is the emulator.
- UObjects are allocated one at a time, and the incremental GC has no write barriers.

## Game

- Hit groups are capsule height bands, not per-bone boxes. Spread and recoil are chosen per weapon, not CS's formulas.
- Bots throw few of the grenades they buy, hear shots but not steps, and see enemies within 35 m only
  (`SightRadius`: a compromise, de_leon's lanes gave the terrorists' plaza the long duels into the sites).
- The radio is tones, not voices, and the gamepad has no radio menu. One sound variant per surface; no smoke or
  magazine sounds. The smoke is a fixed sphere and does not block flashes.
- One locomotion stance (rifle). The feet slide a little. No arms clip for defusing. The C4 has no first-person view
  model: with it out, fire plants the bomb (ps2-polish P6), but the arms show nothing.
- The menus move with the D-pad and the arrows, not the left stick (UMG's navigation, ps2-polish P5 and P9); the main
  menu lists de_leon and de_puerto (`+MapNames=`), with no preview image of the map.
- de_puerto's rounds reach a plant less often than de_leon's: the bots' duels at A's chicane and B's vestibule decide
  most rounds by elimination (the terrorists win 53 % over seeds 1 to 48, de_leon 56 %).
- A match launched straight into a map without `?winrounds=` (the map on the command line) plays the
  config's `MaxRounds=30`, while the main menu's default is `?winrounds=5`.

## Checks only a person can do

Confirmed by playing (2026-09-30):
- the sound works;
- the pad vibrates;
- the controls are comfortable;
- matches progress, and the bots fight well (the two AI bugs this play found, the knife at range and the bots with no
  enemy in sight, are fixed in [ps2-polish](PLANS/ps2-polish.md) P3 and wait for another play);
- the game holds 30 fps to the eye.
- overall, the game plays well and is enjoyable (2026-10-01).

Still to check:
- Playing from the main menu in PCSX2 with the pad: the menus, the team choice, the pause (ps2-polish P9), the new HUD
  and scoreboard (P6) and the sky (P8); de_puerto chosen in the menu and played by a person (its routes, the ladder,
  the quay's clip).
- A DualShock 2: pressure buttons, a second pad, pulling a pad out mid-game.
- The memory card in the PCSX2 BIOS browser: the save, its icon and its title.
- XInput pads are matched to GLFW pads by order, so a DirectInput pad next to an Xbox pad can get the other pad's
  vibration.
- PAL: the region comes from ROMVER (`-PAL` forces it) and should give 25 fps, but it has never been run or measured.

## Measurements

PCSX2 is not hardware. It does not charge the EE's clock for GS drawing, and it emulates neither the data cache nor the
performance counters. So the EE/GS overlap, the scratchpad's gain and cache misses can only be measured on a console.
Every Budgets.md row records the PCSX2 version and the `Measure.ini` hash.
