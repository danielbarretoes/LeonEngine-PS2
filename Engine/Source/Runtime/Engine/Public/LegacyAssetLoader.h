#pragma once

#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "UObject/SoftObjectPath.h"

class UClass;
class UMaterial;
class UObject;
class USoundWave;
class UStaticMesh;
class UTexture2D;

/**
 * The transitional loader of the legacy content (Leon, P14 part 1): until the content is migrated to `.lasset`
 * packages (P14 part 2, which deletes this class), it turns the legacy files into asset UObjects and makes the
 * engine's default assets, which have no package yet. Everything that reads legacy content goes through it: the `.llev`
 * reader (meshes and materials), the basic shapes, UMaterial::GetDefaultMaterial and UEngine's default textures.
 *
 * **Legacy files.** LoadStaticMesh (`.lmesh`), LoadTexture (PNG, JPEG, TGA, ... through stb_image), LoadMaterial
 * (`.lmat`) and LoadSoundWave (PCM16 `.wav`) make a transient asset in a transient package named after the file
 * (GetLegacyPackageName): `/Temp/LegacyAssets/<Root>/<folders>/<File>_<ext>`, where the root is `Engine` for a file of
 * the engine content, `Game` for one of the project content and `External` (the drive and folders follow) for any
 * other; the object is named after the file's base name. Loading the same file again returns that object while it
 * lives: the package is the cache. Only its users keep it alive (a component's mesh and materials, a material's maps),
 * so the garbage collector frees what nothing references and the next load reads the file again. The objects are
 * transient: a package that references one saves a null reference.
 *
 * **Engine assets.** LoadEngineObject resolves an object path as LoadObject will once the packages exist: the object
 * in memory, else its `.lasset` package, else the legacy source this table maps it to, created in its final package
 * in memory and added to the root set (made once, never collected; UE roots its default materials too):
 *
 * | Object path | Made from |
 * | --- | --- |
 * | `/Engine/EngineMaterials/<Name>` | `Engine/Content/Materials/<Name>.lmat`, else
 * `Engine/Content/Textures/<Name>.png` | | `/Engine/EngineMaterials/M_Default` | as above; without the file, the grey
 * checker material (DefaultTexture) | | `/Engine/EngineMaterials/T_Default_Bump_N` | the procedural bump normal map
 * (256 x 256), the `.lmat` `bump` map | | `/Engine/EngineResources/DefaultTexture` | the procedural grey checker (64 x
 * 64), the `.lmat` `checker` map | | `/Engine/BasicShapes/Cube`, `Plane`, `Sphere` | the procedural 100 cm cube, plane
 * (Z up) and 24 x 16 UV sphere |
 *
 * `[/Script/Engine.Engine]` names the defaults (DefaultMaterialName, DefaultTextureName, DefaultBumpNormalTextureName)
 * with these paths, which stay valid once part 2 saves the packages there. Once the objects exist, LoadObject,
 * FindObject and TSoftObjectPtr find them like any loaded asset.
 */
class ENGINE_API FLegacyAssetLoader
{
public:
	/**
	 * A cooked `.lmesh` as a transient UStaticMesh: its geometry, and one transient UMaterial per material slot with
	 * the slot's diffuse map (the file's slot string) loaded as its base colour map. Null, with an error, for another
	 * extension or a file that cannot be read.
	 */
	[[nodiscard]] static UStaticMesh* LoadStaticMesh(const FString& Filename);

	/** An image file as a transient UTexture2D (RGBA8, bottom row first); null, with an error, on failure. */
	[[nodiscard]] static UTexture2D* LoadTexture(const FString& Filename);

	/**
	 * A `.lmat` as a transient UMaterial: its parameters, and its maps loaded as textures (content-relative paths,
	 * FPaths::ResolveLegacyContentPath) or the engine's `checker` / `bump` maps. Null, with an error, when it cannot be
	 * read.
	 */
	[[nodiscard]] static UMaterial* LoadMaterial(const FString& Filename);

	/** A `.wav` of 16-bit PCM samples as a transient USoundWave; null, with an error, for anything else. */
	[[nodiscard]] static USoundWave* LoadSoundWave(const FString& Filename);

	/**
	 * The object at ObjectPath (`/Engine/EngineMaterials/M_Default.M_Default`) if it is a Class: in memory, else from
	 * its package, else made from the table above. Null, with a warning, when none of them has it.
	 */
	[[nodiscard]] static UObject* LoadEngineObject(UClass* Class, const FSoftObjectPath& ObjectPath);

	template <typename T>
	[[nodiscard]] static T* LoadEngineObject(const FSoftObjectPath& ObjectPath)
	{
		return Cast<T>(LoadEngineObject(T::StaticClass(), ObjectPath));
	}

	/**
	 * A UV sphere of Segments x Rings: /Engine/BasicShapes/Sphere for the default 24 x 16, else a transient mesh per
	 * tessellation (`/Temp/LegacyAssets/BasicShapes/Sphere_<Segments>x<Rings>`; the `.llev` spheres may ask for one).
	 */
	[[nodiscard]] static UStaticMesh* GetSphereMesh(int32 Segments, int32 Rings);

	/** The transient package that holds a legacy file's asset (above). */
	[[nodiscard]] static FString GetLegacyPackageName(const FString& Filename);

	/** The root of the legacy files' packages. */
	static const TCHAR* const LegacyPackageRoot;
};
