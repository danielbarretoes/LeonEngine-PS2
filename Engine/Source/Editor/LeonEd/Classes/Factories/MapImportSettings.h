#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "MapImportSettings.generated.h"

/** What UGLTFMapFactory makes of a node a rule matches (Leon). */
UENUM()
enum class EMapImportNodeKind : uint8
{
	/**
	 * An actor of the rule's ActorClass at the node: a volume (AVolume) takes the box of the node's mesh (a 2 m cube
	 * for a node without one), a player start the node's location and yaw, any other actor the node's transform.
	 */
	Actor,
	/** A static mesh actor that only collides: hidden in game, with its collision on (`COL_`). */
	CollisionOnly,
	/**
	 * Convex collision of the mesh node its name names, UE's convention (`UCX_<MeshNode>_<NN>`): the bounding box of
	 * the node's mesh becomes a box of that mesh's simple collision (UBodySetup; Leon has no convex hulls).
	 */
	ConvexCollision,
	/** Left out of the map. */
	Ignore,
};

/**
 * A naming rule of the map importer: the nodes whose name starts with Prefix become Kind (Leon; UE's importers hard
 * code UCX_ and friends, Datasmith reads metadata instead).
 */
USTRUCT()
struct LEONED_API FMapImportNodeRule
{
	GENERATED_BODY()

	/** The start of the node names the rule matches (compared without case). */
	UPROPERTY()
	FString Prefix;

	/** What the node becomes. */
	UPROPERTY()
	EMapImportNodeKind Kind = EMapImportNodeKind::Actor;

	/** For Kind Actor: the engine class of the actor (`/Script/Engine.TriggerVolume`). */
	UPROPERTY()
	FSoftClassPath ActorClass;

	/** Tags the actor gets (AActor::Tags; plan decision D15: the game meaning of a map lives in tags). */
	UPROPERTY()
	TArray<FName> Tags;

	/**
	 * The rest of the node name after Prefix (without a leading '_' or a Blender `.NNN` copy number) is a tag too,
	 * when there is one: `BombSite_A` gives `A`. A player start takes it as its PlayerStartTag instead (`CT`).
	 */
	UPROPERTY()
	bool bSuffixAsTag = false;
};

/**
 * How UGLTFMapFactory (`LeonCook -run=ImportAssets -type=Map`) turns the nodes of a glTF scene into actors (Leon),
 * from `[/Script/LeonEd.MapImportSettings]` of the Editor config: the engine's rules in Engine/Config/BaseEditor.ini
 * (UCX_, COL_, Clip_, PlayerStart, NavWaypoint), a project's own in its Config/DefaultEditor.ini (`+NodeRules=`,
 * `+RequiredTags=`).
 *
 * - A node takes the rule with the longest Prefix its name starts with. A node no rule matches becomes an
 *   AStaticMeshActor when it has a mesh and is left out otherwise (a group); a KHR_lights_punctual light becomes a
 *   light whatever its name.
 * - RequiredTags is the project's check of a map: each entry is `[<Class>:]<Tag>[+<Tag>...]`, and some actor (of that
 *   class, named without its prefix: `PlayerStart`) must carry every tag, a player start's PlayerStartTag counting as
 *   one of its tags; the import fails, naming the missing entries, when one is not met.
 */
UCLASS(Config = Editor)
class LEONED_API UMapImportSettings : public UObject
{
	GENERATED_BODY()

public:
	UMapImportSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The naming rules (engine's first, then the project's). */
	UPROPERTY(Config)
	TArray<FMapImportNodeRule> NodeRules;

	/** What every map of the project must hold (see the class comment). */
	UPROPERTY(Config)
	TArray<FString> RequiredTags;

	/** The rule of a node name: the longest matching Prefix (the first of equal ones), or null. */
	[[nodiscard]] const FMapImportNodeRule* FindRule(const FString& NodeName) const;

	/**
	 * The node name after Prefix: without a leading '_' and without a Blender copy number (`.001`);
	 * `PlayerStart_CT.001` after `PlayerStart` is `CT`.
	 */
	[[nodiscard]] static FString GetSuffix(const FString& NodeName, const FString& Prefix);
};
