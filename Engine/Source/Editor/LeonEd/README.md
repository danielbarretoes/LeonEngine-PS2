# LeonEd

The editor module (UE: `Engine/Source/Editor/UnrealEd`), `TYPE Editor`: asset factories, reimport and the commandlets
that LeonCook runs (`LeonCook [<Project>.lproj] -run=<Commandlet>`). Desktop only and edit time only: LeonBuildTool
refuses it in a game target, and it needs `WITH_EDITORONLY_DATA` (no Shipping build). Programs link it: LeonCook and
LeonAutomationTests.

Canonical docs: **[Docs/TOOLS.md](../../../../Docs/TOOLS.md)** (the commandlets, `ImportList.ini`, reimport, gate G5)
and [Docs/ASSET_FORMATS.md](../../../../Docs/ASSET_FORMATS.md#importing-assets) (the import pipeline and the assets it
makes).

| Area | Types | Headers |
| --- | --- | --- |
| Factories | `UFactory` (`SupportedClass`, `Formats`, `FactoryCreateNew` / `FactoryCreateBinary` / `FactoryCreateFile`, `StaticImportObject`, `ApplyImportSettings`, `CreateOrOverwriteAsset`), `UTextureFactory` (stb_image: the engine's only image decoder), `UGLTFImportFactory` (glTF, the only mesh format: static and skeletal meshes, their animations, embedded images), `UGLTFMapFactory` (glTF scenes as `.lmap` maps, with its rules and the waypoint auto-linking, `bAutoLinkWaypoints`, in `UMapImportSettings`), `USoundFactory`, `UMaterialFactoryNew`, `UPhysicalMaterialFactoryNew` (the `PM_` assets), `UBlendSpaceFactoryNew` / `UBlendSpaceFactory1D` / `UAimOffsetBlendSpaceFactory1D`, `UAnimMontageFactory` | `Classes/Factories/` |
| Reimport | `FReimportHandler` (the import factories implement it), `FReimportManager`, `EReimportResult` | `Public/EditorReimportHandler.h` |
| Commandlets | `UImportAssetsCommandlet`, `UResavePackagesCommandlet`, `UValidateAssetsCommandlet`, `UCookCommandlet` (cook by the book for a target platform since P16: seeds from the maps and the config, the dependency closure, cooked packages without editor-only data, the config and shaders staged; the hard budgets, the incremental cook cache and the PS2's paletted textures: ps2-shipping N23; [TOOLS.md](../../../../Docs/TOOLS.md#the-cook)) | `Classes/Commandlets/` |
| Static lighting | `FStaticLightingSystem` (UE: Lightmass): the sun, the sky and the point lights baked into a map's static mesh vertex colours when the map is imported or resaved, deterministic (ps2-shipping N22) | `Public/StaticLightingSystem.h` |
| Helpers | `FAssetImportUtils` (UE prefixes, package files and saves, content scans), `CommandletHelpers` (the `-run=` lookup) | `Public/` |

Imported assets keep their source in their `UAssetImportData` (Engine, editor-only): the file relative to the engine or
project folder, its MD5 and the import settings, never a timestamp, so a reimport saves the same bytes.

Dependencies (`LeonEd.Build.cmake`): public `Core`, `CoreUObject`, `Engine`, `TargetPlatform` (the cook's platforms),
`RenderCore`; private `AnimationCore`, `MeshUtilities`, `Json` (the map nodes' extras), `STB`, `TextureCompressor` and
`GSCore` (the PS2 cook's paletted textures and their VRAM report). Log category: `LogLeonEd` (the cook logs to
`LogCook`). Tests: `Private/Tests`
(`System.LeonEd.*`), writing under the program's `Intermediate/Tests/LeonEd` through a `/LeonEdTest/` mount point.
