# PS2 cook (host)

Host tools produce `LPS2` blobs consumed by `FPS2RHI::DrawCookedMesh` (PS2RHI). The call currently only
validates the blob header and draws a placeholder; the format is documented in
[ASSET_FORMATS](../../../../Docs/ASSET_FORMATS.md).

LeonCook's cook commandlet (`LeonCook -run=Cook`, `UCookCommandlet`, see [TOOLS](../../../../Docs/TOOLS.md)) has no PS2
target platform yet (P16 brings `-TargetPlatform=PS2`). Until it
does, stage cooked meshes under the game's `Content/` folder (for example `Game/ThirdPerson/Content/Meshes/`)
or embed them next to the ELF.
