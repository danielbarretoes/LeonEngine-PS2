#include "Factories/GLTFMapFactory.h"

#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavigationWaypoint.h"
#include "AssetImportUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Factories/MapImportSettings.h"
#include "Factories/StaticMeshImport.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Volume.h"
#include "GameFramework/WorldSettings.h"
#include "GltfScene.h"
#include "LeonEdLog.h"
#include "Level/Light.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{

	/** What one node of the scene becomes. */
	struct FNodePlan
	{
		int32 NodeIndex = INDEX_NONE;
		EMapImportNodeKind Kind = EMapImportNodeKind::Actor;
		/** The actor class (a static mesh actor for a mesh node no rule matches, a light class for a light). */
		UClass* ActorClass = nullptr;
		/** AActor::Tags. */
		TArray<FName> Tags;
		/** A player start's PlayerStartTag. */
		FName PlayerStartTag;
		/** The actor's name: the node's, made a valid unique object name. */
		FName ActorName;
	};

	/** The mesh a mesh node shows, and the boxes UCX_ nodes give it (in the mesh's space). */
	struct FMeshPlan
	{
		UStaticMesh* Mesh = nullptr;
		TArray<FBox> ConvexBoxes;
	};

	/** The package PackageName: in memory, else loaded from its file, else a new one. */
	UPackage* FindOrLoadPackage(const FString& PackageName)
	{
		if (UPackage* Found = FindPackage(nullptr, *PackageName))
		{
			return Found;
		}
		if (FPackageName::DoesPackageExist(PackageName))
		{
			if (UPackage* Loaded = LoadPackage(nullptr, *PackageName, LOAD_None))
			{
				return Loaded;
			}
		}
		return CreatePackage(*PackageName);
	}

	/** The bounds of a mesh's vertices, or an invalid box. */
	FBox MeshBounds(const FMeshData& Data)
	{
		FBox Box(ForceInit);
		for (const FVertex& Vertex : Data.Vertices)
		{
			Box += Vertex.Position;
		}
		return Box;
	}

	/** The name before a UCX_ node's `_<NN>` number (and Blender copy number): the mesh node it collides for. */
	FString ConvexTargetName(const FString& Suffix)
	{
		FString Target = Suffix;
		int32 Underscore = INDEX_NONE;
		if (Target.FindLastChar('_', Underscore) && Underscore + 1 < Target.Len())
		{
			bool bDigits = true;
			for (int32 Index = Underscore + 1; Index < Target.Len(); ++Index)
			{
				bDigits &= FChar::IsDigit(Target[Index]);
			}
			if (bDigits)
			{
				Target.LeftInline(Underscore);
			}
		}
		return Target;
	}

	/** Names from an extras field: a JSON array of strings, or one string of comma- or space-separated names. */
	TArray<FString> ReadNames(const FJsonObject& Extras, const FString& Field)
	{
		TArray<FString> Names;
		const TSharedPtr<FJsonValue> Value = Extras.TryGetField(Field);
		if (!Value.IsValid())
		{
			return Names;
		}
		TArray<FString> Parts;
		if (Value->Type == EJson::Array)
		{
			for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
			{
				if (Item.IsValid() && Item->Type == EJson::String)
				{
					Parts.Add(Item->AsString());
				}
			}
		}
		else if (Value->Type == EJson::String)
		{
			static const TCHAR* const Delimiters[] = {TEXT(","), TEXT(" "), TEXT(";")};
			Value->AsString().ParseIntoArray(Parts, Delimiters, static_cast<int32>(UE_ARRAY_COUNT(Delimiters)), true);
		}
		for (const FString& Part : Parts)
		{
			const FString Name = Part.TrimStartAndEnd();
			if (!Name.IsEmpty())
			{
				Names.Add(Name);
			}
		}
		return Names;
	}

	/** The node's extras as a JSON object, or null. */
	TSharedPtr<FJsonObject> ParseExtras(const FGltfSceneNode& Node)
	{
		TSharedPtr<FJsonObject> Object;
		if (!Node.Extras.IsEmpty() && !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Node.Extras), Object))
		{
			UE_LOG(LogLeonEd, Warning, "GLTFMapFactory: the extras of '%s' are not a JSON object", *Node.Name);
			return nullptr;
		}
		return Object;
	}

	/** True when Class is or derives from a class named ClassName (without its prefix: `PlayerStart`). */
	bool IsClassNamed(const UClass* Class, const FString& ClassName)
	{
		for (const UClass* Walk = Class; Walk != nullptr; Walk = Walk->GetSuperClass())
		{
			if (Walk->GetName() == ClassName)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * The RequiredTags entries (`[<Class>:]<Tag>[+<Tag>...]`) no planned actor meets: an actor of the class (when
	 * named) whose tags, its PlayerStartTag included, hold every tag of the entry.
	 */
	TArray<FString> FindMissingRequiredTags(const TArray<FString>& RequiredTags, const TArray<FNodePlan>& Plans)
	{
		TArray<FString> Missing;
		for (const FString& Entry : RequiredTags)
		{
			FString ClassName;
			FString TagList = Entry.TrimStartAndEnd();
			int32 Colon = INDEX_NONE;
			if (TagList.FindChar(':', Colon))
			{
				ClassName = TagList.Left(Colon).TrimStartAndEnd();
				TagList = TagList.Mid(Colon + 1);
			}
			TArray<FString> Tags;
			TagList.ParseIntoArray(Tags, TEXT("+"), true);
			bool bMet = false;
			for (const FNodePlan& Plan : Plans)
			{
				if (Plan.ActorClass == nullptr || (!ClassName.IsEmpty() && !IsClassNamed(Plan.ActorClass, ClassName)))
				{
					continue;
				}
				bool bHasAll = Tags.Num() > 0;
				for (const FString& Tag : Tags)
				{
					const FName TagName(*Tag.TrimStartAndEnd());
					bHasAll &= Plan.Tags.Contains(TagName) || Plan.PlayerStartTag == TagName;
				}
				if (bHasAll)
				{
					bMet = true;
					break;
				}
			}
			if (!bMet)
			{
				Missing.Add(Entry);
			}
		}
		return Missing;
	}

	/**
	 * Empties a reimported map's level: every actor is destroyed and moved out of the package (to the transient
	 * package, under a unique name), so the new actors can take the node names and the save leaves the old ones out.
	 */
	void ClearLevel(UWorld& World)
	{
		ULevel& Level = *World.PersistentLevel;
		Level.SetWorldSettings(nullptr);
		const TArray<AActor*> OldActors = Level.Actors;
		for (AActor* Actor : OldActors)
		{
			if (Actor == nullptr)
			{
				continue;
			}
			(void)World.DestroyActor(Actor);
			(void)Actor->Rename(nullptr, GetTransientPackage());
		}
		Level.Actors.Reset();
	}

	/** A spawn with the given name. */
	FActorSpawnParameters NamedSpawn(FName Name)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Name = Name;
		return SpawnInfo;
	}

} // namespace

UGLTFMapFactory::UGLTFMapFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UWorld::StaticClass();
	Formats.Add(TEXT("gltf;GL Transmission Format scene"));
	Formats.Add(TEXT("glb;GL Transmission Format scene (binary)"));
	bEditorImport = 1;
	// A glTF file is a static mesh unless a map is asked for (-type=Map).
	ImportPriority = 50;
}

UObject* UGLTFMapFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled)
{
	(void)InClass;
	(void)Parms;
	bOutOperationCanceled = false;
	AdditionalImportedObjects.Reset();
	UPackage* const Package = InParent != nullptr ? InParent->GetOutermost() : nullptr;
	if (Package == nullptr)
	{
		UE_LOG(LogLeonEd, Error, "GLTFMapFactory: a map needs a package");
		return nullptr;
	}
	FGltfScene Scene;
	FString Error;
	if (!LoadGltfScene(Filename, Scene, Error))
	{
		UE_LOG(LogLeonEd, Error, "GLTFMapFactory: %s", *Error);
		return nullptr;
	}
	const UMapImportSettings& Settings = *GetDefault<UMapImportSettings>();
	const FString MapPackageName = Package->GetName();

	// 1. What every node becomes, before anything changes: a bad rule or a missing required tag leaves the map as it
	// was.
	TArray<FNodePlan> Plans;
	TSet<FName> UsedNames;
	UsedNames.Add(FName(TEXT("WorldSettings")));
	TMap<FString, int32> MeshNodeByName;
	bool bRulesValid = true;
	for (int32 NodeIndex = 0; NodeIndex < Scene.Nodes.Num(); ++NodeIndex)
	{
		const FGltfSceneNode& Node = Scene.Nodes[NodeIndex];
		FNodePlan Plan;
		Plan.NodeIndex = NodeIndex;
		if (Node.Light != INDEX_NONE)
		{
			// A light is a light whatever its name.
			const FGltfSceneLight& Light = Scene.Lights[Node.Light];
			Plan.ActorClass = Light.Type == EGltfLightType::Directional ? ADirectionalLight::StaticClass()
																		: APointLight::StaticClass();
			if (Light.Type == EGltfLightType::Spot)
			{
				UE_LOG(LogLeonEd, Warning, "GLTFMapFactory: the spot light '%s' becomes a point light", *Node.Name);
			}
		}
		else if (const FMapImportNodeRule* Rule = Settings.FindRule(Node.Name))
		{
			Plan.Kind = Rule->Kind;
			if (Plan.Kind == EMapImportNodeKind::Ignore)
			{
				continue;
			}
			if (Plan.Kind == EMapImportNodeKind::ConvexCollision)
			{
				Plans.Add(Plan);
				continue;
			}
			if (Plan.Kind == EMapImportNodeKind::CollisionOnly)
			{
				Plan.ActorClass = AStaticMeshActor::StaticClass();
			}
			else
			{
				Plan.ActorClass = Rule->ActorClass.TryLoadClass<AActor>();
				if (Plan.ActorClass == nullptr || Plan.ActorClass->HasAnyClassFlags(CLASS_Abstract))
				{
					UE_LOG(LogLeonEd, Error, "GLTFMapFactory: the rule '%s' names no actor class ('%s')", *Rule->Prefix,
						*Rule->ActorClass.ToString());
					bRulesValid = false;
					continue;
				}
			}
			Plan.Tags = Rule->Tags;
			const FString Suffix = UMapImportSettings::GetSuffix(Node.Name, Rule->Prefix);
			if (Rule->bSuffixAsTag && !Suffix.IsEmpty())
			{
				if (Plan.ActorClass->IsChildOf(APlayerStart::StaticClass()))
				{
					Plan.PlayerStartTag = FName(*Suffix);
				}
				else
				{
					Plan.Tags.AddUnique(FName(*Suffix));
				}
			}
		}
		else if (Node.Mesh != INDEX_NONE)
		{
			Plan.ActorClass = AStaticMeshActor::StaticClass();
		}
		else
		{
			// A group or an empty no rule names.
			continue;
		}
		if ((Plan.Kind == EMapImportNodeKind::CollisionOnly ||
				Plan.ActorClass->IsChildOf(AStaticMeshActor::StaticClass())) &&
			(Node.Mesh == INDEX_NONE || Scene.Meshes[Node.Mesh].Data.IsEmpty()))
		{
			UE_LOG(LogLeonEd, Warning, "GLTFMapFactory: '%s' has no triangles; left out", *Node.Name);
			continue;
		}
		const FString BaseName = FAssetImportUtils::SanitizeName(Node.Name);
		FName ActorName(*BaseName);
		for (int32 Suffix = 2; UsedNames.Contains(ActorName); ++Suffix)
		{
			ActorName = FName(*FString::Printf(TEXT("%s_%d"), *BaseName, Suffix));
		}
		UsedNames.Add(ActorName);
		Plan.ActorName = ActorName;
		if (Plan.ActorClass->IsChildOf(AStaticMeshActor::StaticClass()))
		{
			MeshNodeByName.Add(Node.Name, NodeIndex);
		}
		Plans.Add(MoveTemp(Plan));
	}
	if (!bRulesValid)
	{
		return nullptr;
	}
	// The project's check of its maps; the engine's maps are not the project's (UMapImportSettings).
	const TArray<FString> Missing = UMapImportSettings::AppliesRequiredTags(MapPackageName)
		? FindMissingRequiredTags(Settings.RequiredTags, Plans)
		: TArray<FString>();
	if (Missing.Num() > 0)
	{
		for (const FString& Entry : Missing)
		{
			UE_LOG(LogLeonEd, Error,
				"GLTFMapFactory: '%s' has nothing with the required tags '%s' (RequiredTags of "
				"[/Script/LeonEd.MapImportSettings])",
				*Filename, *Entry);
		}
		return nullptr;
	}

	// 2. The meshes: one SM_ per glTF mesh a mesh node shows, and the boxes of the UCX_ nodes.
	TMap<int32, FMeshPlan> Meshes;
	TSet<FString> UsedMeshNames;
	for (const FNodePlan& Plan : Plans)
	{
		const FGltfSceneNode& Node = Scene.Nodes[Plan.NodeIndex];
		if (Plan.ActorClass == nullptr || !Plan.ActorClass->IsChildOf(AStaticMeshActor::StaticClass()) ||
			Meshes.Contains(Node.Mesh))
		{
			continue;
		}
		const FString BaseName =
			FAssetImportUtils::MakeAssetName(UStaticMesh::StaticClass(), Scene.Meshes[Node.Mesh].Name);
		FString AssetName = BaseName;
		for (int32 Suffix = 2; UsedMeshNames.Contains(AssetName); ++Suffix)
		{
			AssetName = FString::Printf(TEXT("%s_%d"), *BaseName, Suffix);
		}
		UsedMeshNames.Add(AssetName);
		UPackage* MeshPackage = FindOrLoadPackage(MapPackageName + TEXT("/Meshes/") + AssetName);
		FMeshPlan MeshPlan;
		MeshPlan.Mesh = CreateOrOverwriteAsset<UStaticMesh>(MeshPackage, FName(*AssetName), RF_Public | RF_Standalone);
		if (MeshPlan.Mesh == nullptr)
		{
			return nullptr;
		}
		Meshes.Add(Node.Mesh, MeshPlan);
	}
	for (const FNodePlan& Plan : Plans)
	{
		if (Plan.Kind != EMapImportNodeKind::ConvexCollision)
		{
			continue;
		}
		const FGltfSceneNode& Node = Scene.Nodes[Plan.NodeIndex];
		const FMapImportNodeRule* Rule = Settings.FindRule(Node.Name);
		const FString Target = ConvexTargetName(UMapImportSettings::GetSuffix(Node.Name, Rule->Prefix));
		const int32* TargetIndex = MeshNodeByName.Find(Target);
		if (TargetIndex == nullptr || Node.Mesh == INDEX_NONE)
		{
			UE_LOG(LogLeonEd, Warning, "GLTFMapFactory: '%s' names no mesh node '%s' (or has no mesh); left out",
				*Node.Name, *Target);
			continue;
		}
		// The collision mesh's vertices in the target mesh's space.
		const FGltfSceneNode& TargetNode = Scene.Nodes[*TargetIndex];
		FBox Box(ForceInit);
		for (const FVertex& Vertex : Scene.Meshes[Node.Mesh].Data.Vertices)
		{
			Box += TargetNode.WorldTransform.InverseTransformPosition(
				Node.WorldTransform.TransformPosition(Vertex.Position));
		}
		Meshes[TargetNode.Mesh].ConvexBoxes.Add(Box);
	}
	const FString MaterialPath = MapPackageName + TEXT("/Materials");
	for (TPair<int32, FMeshPlan>& Pair : Meshes)
	{
		UStaticMesh& Mesh = *Pair.Value.Mesh;
		StaticMeshImport::BuildStaticMesh(
			Mesh, Scene.Meshes[Pair.Key].Data, true, AdditionalImportedObjects, MaterialPath);
		// UCX_: Leon has no convex hulls; each piece's bounding box is a box of the simple collision, and the body uses
		// it for traces too (a static body would use the triangles otherwise).
		UBodySetup& BodySetup = *Mesh.GetBodySetup();
		BodySetup.AggGeom.EmptyElements();
		BodySetup.CollisionTraceFlag = Pair.Value.ConvexBoxes.Num() > 0 ? CTF_UseSimpleAsComplex : CTF_UseDefault;
		for (const FBox& Box : Pair.Value.ConvexBoxes)
		{
			const FVector Size = Box.GetSize();
			FKBoxElem& Elem = BodySetup.AggGeom.BoxElems.Add_GetRef(FKBoxElem(Size.X, Size.Y, Size.Z));
			Elem.Center = Box.GetCenter();
		}
		AdditionalImportedObjects.AddUnique(&Mesh);
	}

	// 3. The world: the package's (a reimport), emptied, or a new one; nothing draws it (no renderer's scene).
	const UWorld::InitializationValues IVS = UWorld::InitializationValues().InitializeScenes(false);
	UWorld* World = UWorld::FindWorldInPackage(Package);
	if (World == nullptr)
	{
		if (StaticFindObjectFast(nullptr, Package, InName) != nullptr)
		{
			UE_LOG(LogLeonEd, Error, "GLTFMapFactory: %s holds another object named %s", *MapPackageName,
				*InName.ToString());
			return nullptr;
		}
		World = UWorld::CreateWorld(EWorldType::Editor, false, InName, Package, /*bAddToRoot =*/false, &IVS);
	}
	else
	{
		World->InitWorld(IVS);
		ClearLevel(*World);
	}
	World->SetFlags(Flags);

	// 4. The actors, the world settings first, then the nodes in file order.
	AWorldSettings* WorldSettings =
		World->SpawnActor<AWorldSettings>(AWorldSettings::StaticClass(), NamedSpawn(FName(TEXT("WorldSettings"))));
	World->PersistentLevel->SetWorldSettings(WorldSettings);
	TMap<FString, ANavigationWaypoint*> WaypointsByName;
	TArray<TPair<ANavigationWaypoint*, int32>> WaypointNodes;
	for (const FNodePlan& Plan : Plans)
	{
		if (Plan.ActorClass == nullptr)
		{
			continue;
		}
		const FGltfSceneNode& Node = Scene.Nodes[Plan.NodeIndex];
		const FTransform& NodeTransform = Node.WorldTransform;
		const FActorSpawnParameters SpawnInfo = NamedSpawn(Plan.ActorName);
		AActor* Actor = nullptr;
		if (Node.Light != INDEX_NONE)
		{
			const FGltfSceneLight& Light = Scene.Lights[Node.Light];
			const FVector Location = NodeTransform.GetLocation();
			const FRotator Rotation = Node.LightDirection.Rotation();
			ALight* LightActor = World->SpawnActor<ALight>(Plan.ActorClass, Location, Rotation, SpawnInfo);
			ULightComponent& Component = *LightActor->GetLightComponent();
			Component.SetLightColor(Light.Color);
			Component.SetIntensity(Light.Intensity);
			// Leon's renderer shadows the first directional light only.
			Component.SetCastShadows(Light.Type == EGltfLightType::Directional);
			if (UPointLightComponent* Point = Cast<UPointLightComponent>(&Component))
			{
				Point->SetAttenuationRadius(Light.Range > 0.0f ? Light.Range : DefaultPointLightRange);
			}
			Actor = LightActor;
		}
		else if (Plan.ActorClass->IsChildOf(AStaticMeshActor::StaticClass()))
		{
			AStaticMeshActor* MeshActor =
				World->SpawnActor<AStaticMeshActor>(Plan.ActorClass, NodeTransform, SpawnInfo);
			UStaticMeshComponent& Component = *MeshActor->GetStaticMeshComponent();
			(void)Component.SetStaticMesh(Meshes[Node.Mesh].Mesh);
			Component.SetMobility(EComponentMobility::Static);
			Component.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			if (Plan.Kind == EMapImportNodeKind::CollisionOnly)
			{
				MeshActor->SetActorHiddenInGame(true);
			}
			Actor = MeshActor;
		}
		else if (Plan.ActorClass->IsChildOf(AVolume::StaticClass()))
		{
			// Plan decision D16: a box, the bounds of the node's mesh (a 2 m cube without one), sized by the scale.
			FBox LocalBox = Node.Mesh != INDEX_NONE ? MeshBounds(Scene.Meshes[Node.Mesh].Data) : FBox(ForceInit);
			if (!LocalBox.IsValid)
			{
				LocalBox = FBox(FVector(-100.0f, -100.0f, -100.0f), FVector(100.0f, 100.0f, 100.0f));
			}
			const FVector Extent = LocalBox.GetExtent().ComponentMax(FVector(0.5f, 0.5f, 0.5f));
			const FTransform VolumeTransform(NodeTransform.GetRotation(),
				NodeTransform.TransformPosition(LocalBox.GetCenter()),
				NodeTransform.GetScale3D() * (Extent / AVolume::BrushExtent));
			Actor = World->SpawnActor<AActor>(Plan.ActorClass, VolumeTransform, SpawnInfo);
		}
		else if (Plan.ActorClass->IsChildOf(APlayerStart::StaticClass()))
		{
			// Upright, facing the node's +X (UE's player starts face +X).
			const FVector Forward = NodeTransform.GetRotation().GetForwardVector();
			const FRotator Rotation(0.0f, FVector(Forward.X, Forward.Y, 0.0f).Rotation().Yaw, 0.0f);
			APlayerStart* Start =
				World->SpawnActor<APlayerStart>(Plan.ActorClass, NodeTransform.GetLocation(), Rotation, SpawnInfo);
			Start->PlayerStartTag = Plan.PlayerStartTag;
			Actor = Start;
		}
		else if (Plan.ActorClass->IsChildOf(ANavigationWaypoint::StaticClass()))
		{
			ANavigationWaypoint* Waypoint = World->SpawnActor<ANavigationWaypoint>(
				Plan.ActorClass, FTransform(NodeTransform.GetLocation()), SpawnInfo);
			WaypointsByName.Add(Node.Name, Waypoint);
			WaypointNodes.Add(TPair<ANavigationWaypoint*, int32>(Waypoint, Plan.NodeIndex));
			Actor = Waypoint;
		}
		else
		{
			Actor = World->SpawnActor<AActor>(Plan.ActorClass, NodeTransform, SpawnInfo);
		}
		if (Actor == nullptr)
		{
			UE_LOG(LogLeonEd, Error, "GLTFMapFactory: could not spawn '%s' as %s", *Node.Name,
				*Plan.ActorClass->GetName());
			return nullptr;
		}
		for (const FName& Tag : Plan.Tags)
		{
			Actor->Tags.AddUnique(Tag);
		}
	}

	// 5. The waypoints' links and flags, now that every waypoint exists.
	for (const TPair<ANavigationWaypoint*, int32>& Entry : WaypointNodes)
	{
		const FGltfSceneNode& Node = Scene.Nodes[Entry.Value];
		const TSharedPtr<FJsonObject> Extras = ParseExtras(Node);
		if (!Extras.IsValid())
		{
			continue;
		}
		for (const FString& LinkName : ReadNames(*Extras, TEXT("links")))
		{
			if (ANavigationWaypoint* const* Linked = WaypointsByName.Find(LinkName))
			{
				Entry.Key->Links.AddUnique(*Linked);
			}
			else
			{
				UE_LOG(
					LogLeonEd, Warning, "GLTFMapFactory: '%s' links '%s', which is no waypoint", *Node.Name, *LinkName);
			}
		}
		for (const FString& Flag : ReadNames(*Extras, TEXT("flags")))
		{
			Entry.Key->Flags.AddUnique(FName(*Flag));
		}
	}

	// 6. The waypoints an agent can walk between, linked (the project's setting; the links are saved in the map).
	if (Settings.bAutoLinkWaypoints)
	{
		// The project's agent, the same the game's graph uses (the Engine config's NavigationSystem section).
		const int32 Added = UNavigationSystem::AutoLinkWaypoints(*World, FWaypointLinkParams::FromConfig());
		UE_LOG(LogLeonEd, Log, "GLTFMapFactory: %d waypoint link(s) added by the auto-linking", Added);
	}

	UpdateAssetImportData(World, Filename);
	UE_LOG(LogLeonEd, Log, "GLTFMapFactory: %s from '%s': %d actors, %d meshes", *World->GetPathName(), *Filename,
		World->PersistentLevel->Actors.Num(), Meshes.Num());
	return World;
}

bool UGLTFMapFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return FactoryCanReimport(Obj, OutFilenames);
}

void UGLTFMapFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	FactorySetReimportPaths(Obj, NewReimportPaths);
}

EReimportResult::Type UGLTFMapFactory::Reimport(UObject* Obj)
{
	return FactoryReimport(Obj);
}

void UGLTFMapFactory::GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
{
	FactoryGetAdditionalReimportedObjects(OutObjects);
}
