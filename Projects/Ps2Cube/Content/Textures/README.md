# Textures

Unreal-style basenames (`T_<Name>_D`). Runtime currently embeds procedural RGBA via `Ps2Texture::CreateChecker` / `CreateGrid` (no hostfs required).

| Asset | Role |
| --- | --- |
| `T_Checker_D` | Hero cube albedo |
| `T_Grid_D` | Ground plane albedo |

Future: cook PNG → GS upload / hostfs beside the ELF.
