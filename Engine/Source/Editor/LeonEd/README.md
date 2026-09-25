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
| Factories | `UFactory` (`SupportedClass`, `Formats`, `FactoryCreateNew` / `FactoryCreateBinary` / `FactoryCreateFile`, `StaticImportObject`, `ApplyImportSettings`, `CreateOrOverwriteAsset`), `UTextureFactory` (stb_image: the engine's only image decoder), `UFbxFactory` (FBX, OBJ: static / skeletal meshes, animations), `UGLTFImportFactory`, `UGLTFMapFactory` (glTF scenes as `.lmap` maps, with its rules in `UMapImportSettings`), `USoundFactory`, `UMaterialFactoryNew` | `Classes/Factories/` |
| Reimport | `FReimportHandler` (the import factories implement it), `FReimportManager`, `EReimportResult` | `Public/EditorReimportHandler.h` |
| Commandlets | `UImportAssetsCommandlet`, `UResavePackagesCommandlet`, `UValidateAssetsCommandlet`, `UCookCommandlet` (minimal before P16) | `Classes/Commandlets/` |
| Helpers | `FAssetImportUtils` (UE prefixes, package files and saves, content scans), `CommandletHelpers` (the `-run=` lookup) | `Public/` |

Imported assets keep their source in their `UAssetImportData` (Engine, editor-only): the file relative to the engine or
project folder, its MD5 and the import settings, never a timestamp, so a reimport saves the same bytes.

Dependencies (`LeonEd.Build.cmake`): public `Core`, `CoreUObject`, `Engine`; private `RenderCore`, `AnimationCore`,
`MeshUtilities`, `Json` (the map nodes' extras), `STB`. Log category: `LogLeonEd` (the cook logs to `LogCook`). Tests: `Private/Tests`
(`System.LeonEd.*`), writing under the program's `Intermediate/Tests/LeonEd` through a `/LeonEdTest/` mount point.
