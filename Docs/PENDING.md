# Pending

What the [ps2-shipping](PLANS/ps2-shipping.md) plan (0.22.0 to 0.24.0) left open. Each item comes from a phase's
"Desviaciones"; the plan's "Pendiente / fuera de alcance" section has the detail, and
[Budgets.md](../Engine/Platforms/PS2/Documentation/Budgets.md) has the measurements. ShooterGame runs at 29.95 fps in
PCSX2 (p50/p95/p99 33.5 ms), so nothing here blocks the frame rate.

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

- Listen to the audio in Win64 and PCSX2: weapons, the bomb, steps and impacts per surface, the ladder, body hits, the
  radio.
- A DualShock 2: vibration, pressure buttons, a second pad, pulling a pad out mid-game.
- The memory card in the PCSX2 BIOS browser: the save, its icon and its title.
- An Xbox pad's vibration on Win64. XInput pads are matched to GLFW pads by order, so a DirectInput pad next to an Xbox
  pad can get the other pad's vibration.
- PAL: the region comes from ROMVER (`-PAL` forces it) and should give 25 fps, but it has never been run or measured.

## Measurements

PCSX2 is not hardware. It does not charge the EE's clock for GS drawing, and it emulates neither the data cache nor the
performance counters. So the EE/GS overlap, the scratchpad's gain and cache misses can only be measured on a console.
Every Budgets.md row records the PCSX2 version and the `Measure.ini` hash.
