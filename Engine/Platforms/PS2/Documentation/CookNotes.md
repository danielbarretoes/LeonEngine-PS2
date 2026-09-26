# PS2 cook (host)

Host tools are to produce `LPS2` blobs (format in [ASSET_FORMATS](../../../../Docs/ASSET_FORMATS.md)); the PS2 runtime
does not read them yet (the scene renderer on the GS command list will, [ps2-gs-parity](../../../../Docs/PLANS/ps2-gs-parity.md)
P5 and P6).

LeonCook's cook commandlet (`LeonCook -run=Cook -TargetPlatform=PS2`, `UCookCommandlet`, P16, see
[TOOLS](../../../../Docs/TOOLS.md)) runs for PS2, but its target platform is a stub that cooks the Win64 formats: it
writes no `LPS2` blob, and the PS2 game mounts no pak yet. Until it does, stage cooked meshes under the game's
`Content/` folder (for example `Game/ThirdPerson/Content/Meshes/`) or embed them next to the ELF.
