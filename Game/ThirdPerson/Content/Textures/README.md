# Textures

Empty on purpose. The PS2 runtime has no image file loading; the demo generates its textures in `FThirdPersonGameMode::StartPlay`:

| Member | Created with |
| --- | --- |
| `GridTexture` | `FPS2Texture::CreateGrid(64)` |
| `CheckerTexture` | `FPS2Texture::CreateChecker(64)` |

See [Docs/ASSET_FORMATS.md](../../../../Docs/ASSET_FORMATS.md#ps2).
