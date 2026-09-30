# Pending

What the [ps2-shipping](PLANS/ps2-shipping.md) plan (0.22.0 to 0.24.0) left open. Each item comes from a phase's
"Desviaciones"; the plan's "Pendiente / fuera de alcance" section has the detail, and
[Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md) has the measurements. ShooterGame runs at 29.95 fps in
PCSX2 (p50/p95/p99 33.5 ms), so nothing here blocks the frame rate.

## Bugs

- **Bots with no ammo attack with the knife from a distance.** When bots run dry and switch to the knife, they stay
  where they are, facing each other and swinging at range, instead of closing in to melee. The AI must move into knife
  range (as CS bots rush with the knife) or look for ammo or a weapon.
- **Bots idle when they see no enemy.** Bots fight well once they see an enemy, but without one in sight they don't
  hunt: they should push to the objective, check the corners and the sites, follow sounds and radio reports, and
  rotate, as CS bots do.

## Next features

- **Minimap.** The radar (N30d) shows dots on a black square. It needs the map's real overview image, like CS's
  overviews: a top-down render of the map, generated at cook time and paletted, drawn under the dots and rotated with
  the view.
- **Sky.** An HDR cubemap for the sky: a skybox drawn behind the world (today the sky is the renderer's dark clear
  colour). It can be generated procedurally (a desert sky gradient, sun and clouds) and cooked to paletted faces for
  the GS.
- **Main menu.** A start menu that sets up the match:
  - the map;
  - the bots' difficulty (CS's easy, normal, hard and expert: reaction time, aim error, awareness);
  - the rounds to win, best of 5 by default (the first team to 3);
  - the total number of bots.
- **Team selection.** On joining a match the player chooses CT or T, as in CS. The bots are then redistributed so the
  teams are as even as possible, counting the player.
- **In-game menu.** A pause menu during the match (Esc / Start) to resume, change team, or go back to the main menu.
- **UI.** Better menus and HUD. The font family (DejaVu Sans Condensed, `UFont`), the textured canvas and UMG's
  buttons, focus, switcher and table exist since [ps2-polish](PLANS/ps2-polish.md) P5; the HUD only changed its font.
- **The scoreboard in a table.** `AShooterHUD` still draws the scoreboard by hand, aligned with spaces (which the
  proportional font no longer lines up); UMG's `UTableView` (P5) is for it. The CS scoreboard shows per team: name,
  score (kills), deaths, latency or bot, alive/dead, and the bomb carrier for T.

ShooterGame stays single player against bots: no split screen (decided 2026-09-30).

## Render

- Batches that cross the near plane or the guard band still go through the EE's C++ clipper (13 a frame, 2.4 ms with
  the view model). Clipping on VU1, or smaller batches, was not needed at 30 fps.
- Cells and portals cull little on de_leon's open layout: the sky portals keep most cells visible.
- Fog and static mesh LODs work and are tested, but de_leon uses neither.
- de_leon's 78 pieces are not merged per cell and material. A mesh has a single `UCX_` box, because Leon folds a mesh's
  boxes into one AABB; merging needs compound collision (several boxes per mesh).
- No light probes for moving objects (pawns take the sky without occlusion), no skybox, and Movable lights do not light
  the static world.
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
- Bots don't climb ladders and throw few of the grenades they buy. The T side wins 59–61 % of rounds over seeds 1–24.
- The radio is tones, not voices, and the gamepad has no radio menu. One sound variant per surface; no smoke or
  magazine sounds. The smoke is a fixed sphere and does not block flashes.
- One locomotion stance (rifle). The feet slide a little. No arms clip for defusing. The C4 is not a first-person weapon.

## Checks only a person can do

Confirmed by playing (2026-09-30):
- the sound works;
- the pad vibrates;
- the controls are comfortable;
- matches progress, and the bots fight well except in the two AI bugs above;
- the game holds 30 fps to the eye.

Still to check:
- A DualShock 2: pressure buttons, a second pad, pulling a pad out mid-game.
- The memory card in the PCSX2 BIOS browser: the save, its icon and its title.
- XInput pads are matched to GLFW pads by order, so a DirectInput pad next to an Xbox pad can get the other pad's
  vibration.
- PAL: the region comes from ROMVER (`-PAL` forces it) and should give 25 fps, but it has never been run or measured.

## Measurements

PCSX2 is not hardware. It does not charge the EE's clock for GS drawing, and it emulates neither the data cache nor the
performance counters. So the EE/GS overlap, the scratchpad's gain and cache misses can only be measured on a console.
Every Budgets.md row records the PCSX2 version and the `Measure.ini` hash.
