# Levels

Empty on purpose. The PS2 runtime does not load `.lmap` maps yet, so the demo level is built in code by `FThirdPersonLevel::Build` (`Source/ThirdPerson/ThirdPersonLevel.cpp`): an 18 × 18 grid of ground tiles and 17 box props (crates, platforms, stairs, walls).

The `.lmap` maps and how the desktop runtime loads them are described in [Docs/LEVELS.md](../../../../Docs/LEVELS.md).
