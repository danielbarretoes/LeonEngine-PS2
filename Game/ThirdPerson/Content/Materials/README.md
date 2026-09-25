# Materials

Empty on purpose. The demo's materials are `FPS2Material` values set up in `FThirdPersonGameMode::StartPlay` (`Source/ThirdPerson/ThirdPersonGameMode.cpp`), not material assets:

| Member | Used for | Base color map |
| --- | --- | --- |
| `GroundMaterial` | Ground tiles | `GridTexture` |
| `PlatformMaterial` | Platforms, stairs, walls | `CheckerTexture` |
| `CrateMaterial` | Crates | `CheckerTexture` |
| `CharacterMaterial` | Character | none (solid color) |

`FPS2Material` mirrors a subset of `UMaterial` (`BaseColor`, `BaseColorMap`, `ShadingModel`); see [Docs/ASSET_FORMATS.md](../../../../Docs/ASSET_FORMATS.md#ps2).
