#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/** The kind of a glTF punctual light (KHR_lights_punctual). */
enum class EGltfLightType : uint8
{
	Directional,
	Point,
	Spot,
};

/** A light of a glTF scene (KHR_lights_punctual), in its own units. */
struct MESHUTILITIES_API FGltfSceneLight
{
	FString Name;
	EGltfLightType Type = EGltfLightType::Point;
	/** Linear RGB. */
	FLinearColor Color = FLinearColor::White;
	/** glTF's intensity: lux for a directional light, candela for a point or spot light. */
	float Intensity = 1.0f;
	/** Distance the light reaches, cm; 0 when the file gives none (glTF: an infinite range). */
	float Range = 0.0f;
};

/** A mesh of a glTF scene: its triangle primitives, in its own space, in the engine's axes and units. */
struct MESHUTILITIES_API FGltfSceneMesh
{
	FString Name;
	/** One section and material slot per primitive (as LoadStaticMeshFromGltf), with tangents. */
	FMeshData Data;
};

/** A node of a glTF scene, placed in the engine world. */
struct MESHUTILITIES_API FGltfSceneNode
{
	FString Name;
	/** The parent node's index, or INDEX_NONE for a root. */
	int32 Parent = INDEX_NONE;
	/**
	 * The node's world transform (its parents' transforms applied) converted to the engine world: the location in cm,
	 * the engine's axes (FImportCoordinateConversion, right-handed Y up to UE). A vector of the node's mesh times it is
	 * the vector in the world.
	 */
	FTransform WorldTransform;
	/** Index into FGltfScene::Meshes, or INDEX_NONE. */
	int32 Mesh = INDEX_NONE;
	/** Index into FGltfScene::Lights, or INDEX_NONE. */
	int32 Light = INDEX_NONE;
	/** For a light node: the unit world direction it shines along (glTF lights shine along their -Z). */
	FVector LightDirection = FVector(1.0f, 0.0f, 0.0f);
	/** The node's `extras` JSON text (Blender's custom properties), or empty. */
	FString Extras;
};

/** A glTF file as a scene: its nodes in file order, the meshes and the lights they use. */
struct MESHUTILITIES_API FGltfScene
{
	TArray<FGltfSceneNode> Nodes;
	TArray<FGltfSceneMesh> Meshes;
	TArray<FGltfSceneLight> Lights;
};

/**
 * Reads a `.gltf` / `.glb` as a scene (the map importer, LeonEd's UGLTFMapFactory): every node with its world
 * transform, mesh, light and extras, each mesh once (a mesh several nodes show is one FGltfSceneMesh), and the
 * KHR_lights_punctual lights. glTF is right-handed, Y up, in metres: positions, transforms and light directions are
 * converted to the engine world (FImportCoordinateConversion, units x 100). A mesh slot takes its glTF material as
 * LoadStaticMeshFromGltf does (external images only). Edit time only.
 */
[[nodiscard]] MESHUTILITIES_API bool LoadGltfScene(const FString& Path, FGltfScene& Out, FString& OutError);
