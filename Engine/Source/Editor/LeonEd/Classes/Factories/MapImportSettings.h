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
 *   one of its tags; the import fails, naming the missing entries, when one is not met. The engine's maps
 *   (/Engine/...) are not a project's: a reimport of every asset with a project (CheckReimport, gate G5) does not
 *   check them against it. Nor are the project's maps it lists in MapsWithoutRequiredTags (a front end's menu map).
 * - With bAutoLinkWaypoints the import links the map's waypoints the way the project's agent (the Engine config's
 *   [/Script/Engine.NavigationSystem], FWaypointLinkParams::FromConfig) can walk (UNavigationSystem::AutoLinkWaypoints:
 *   a capsule sweep, steps, jumps, drops), besides the links the nodes name; the links are saved in the map.
 * - With bBuildOverview the import renders the map's overview for a radar (FMapOverview, Docs/PLANS/ps2-polish.md
 *   P7) once its lighting is baked: `<Map>/T_<Map>_Overview`, OverviewResolution texels square, cut at
 *   OverviewClipHeight, kept in the world settings' OverviewSettings; not for the engine's maps nor the ones listed in
 *   MapsWithoutOverview.
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

	/** The project's maps (long package names) RequiredTags do not apply to: not played on (a main menu's map). */
	UPROPERTY(Config)
	TArray<FString> MapsWithoutRequiredTags;

	/** Links the waypoints an agent can walk between (see the class comment); off in the engine's config. */
	UPROPERTY(Config)
	bool bAutoLinkWaypoints = false;

	/** Renders each map's overview at the import (see the class comment); off in the engine's config. */
	UPROPERTY(Config)
	bool bBuildOverview = false;

	/** The overview's side, texels (FMapOverviewSettings::Resolution). */
	UPROPERTY(Config)
	int32 OverviewResolution = 128;

	/** The height the overview cuts the map at, cm (FMapOverviewSettings::ClipHeight). */
	UPROPERTY(Config)
	float OverviewClipHeight = 250.0f;

	/** The project's maps (long package names) that get no overview: not played on (a main menu's map). */
	UPROPERTY(Config)
	TArray<FString> MapsWithoutOverview;

	/** The rule of a node name: the longest matching Prefix (the first of equal ones), or null. */
	[[nodiscard]] const FMapImportNodeRule* FindRule(const FString& NodeName) const;

	/**
	 * The node name after Prefix: without a leading '_' and without a Blender copy number (`.001`);
	 * `PlayerStart_CT.001` after `PlayerStart` is `CT`.
	 */
	[[nodiscard]] static FString GetSuffix(const FString& NodeName, const FString& Prefix);

	/** Whether RequiredTags apply to the map of a package: every map but the engine's (/Engine/...) and the exempt. */
	[[nodiscard]] static bool AppliesRequiredTags(const FString& MapPackageName);

	/**
	 * Whether the import renders the overview of the map of a package: with bBuildOverview, for every map but the
	 * engine's (/Engine/...: a project's settings do not change the engine's content) and those in MapsWithoutOverview.
	 */
	[[nodiscard]] static bool BuildsOverview(const FString& MapPackageName);
};
