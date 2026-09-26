# PS2 cook (host)

Host tools produce `LPS2` blobs consumed by `FPS2RHI::DrawCookedMesh` (PS2RHI). The call currently only
validates the blob header and draws a placeholder; the format is documented in
[ASSET_FORMATS](../../../../Docs/ASSET_FORMATS.md).

LeonCook's cook commandlet (`LeonCook -run=Cook -TargetPlatform=PS2`, `UCookCommandlet`, P16, see
[TOOLS](../../../../Docs/TOOLS.md)) runs for PS2, but its target platform is a stub that cooks the Win64 formats: it
writes no `LPS2` blob, and the PS2 game mounts no pak yet. Until it does, stage cooked meshes under the game's
`Content/` folder (for example `Game/ThirdPerson/Content/Meshes/`) or embed them next to the ELF.
