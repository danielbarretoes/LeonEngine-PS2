#include "Level/LeonLevelFormat.h"

#include "Engine/BlockingVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "EngineLogs.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PainCausingVolume.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerStartPIE.h"
#include "GameFramework/WorldSettings.h"
#include "GameMapsSettings.h"
#include "LegacyCoordinateConversion.h"
#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Level/LegacyLevelDataComponent.h"
#include "Level/LevelLoader.h"
#include "Level/Light.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ResourceCache.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/Package.h"

namespace
{

	// Caps on file-controlled allocations (corrupt / hostile .llev).
	constexpr uint32 MaxLeonLevelStrings = 65536u;
	constexpr uint32 MaxLeonLevelActors = 100000u;
	constexpr uint32 MaxLeonLevelLights = 16384u;
	constexpr uint32 MaxLeonStringBytes = 1u << 20; // 1 MiB per string

	// --- Little-endian primitive writers (FArchive stores native order; every target is little-endian) ---

	void WriteU8(FArchive& Out, uint8 Value)
	{
		Out << Value;
	}

	void WriteU16(FArchive& Out, uint16 Value)
	{
		Out << Value;
	}

	void WriteU32(FArchive& Out, uint32 Value)
	{
		Out << Value;
	}

	void WriteI32(FArchive& Out, int32 Value)
	{
		Out << Value;
	}

	void WriteF32(FArchive& Out, float Value)
	{
		Out << Value;
	}

	void WriteVec3(FArchive& Out, const FVector& Value)
	{
		WriteF32(Out, Value.X);
		WriteF32(Out, Value.Y);
		WriteF32(Out, Value.Z);
	}

	/**
	 * Deduplicating string table; index 0 is always the empty string. Lookup is case-sensitive (a TMap<FString>
	 * would merge strings that differ only in case).
	 */
	class FStringTableBuilder
	{
	public:
		FStringTableBuilder()
		{
			(void)Add(FString());
		}

		uint32 Add(const FString& Value)
		{
			for (int32 Index = 0; Index < Strings.Num(); ++Index)
			{
				if (Strings[Index].Equals(Value, ESearchCase::CaseSensitive))
				{
					return static_cast<uint32>(Index);
				}
			}
			return static_cast<uint32>(Strings.Add(Value));
		}

		void WriteTo(FArchive& Out) const
		{
			WriteU32(Out, static_cast<uint32>(Strings.Num()));
			for (const FString& Value : Strings)
			{
				WriteU32(Out, static_cast<uint32>(Value.Len()));
				Out.Serialize(const_cast<ANSICHAR*>(*Value), Value.Len());
			}
		}

	private:
		TArray<FString> Strings;
	};

	/** Bounds-checked little-endian cursor over FMemoryReader; any overrun latches the archive error. */
	class FByteReader
	{
	public:
		explicit FByteReader(const TArray<uint8>& InBytes)
			: Ar(InBytes)
		{
		}

		[[nodiscard]] bool HasFailed() const
		{
			return Ar.IsError();
		}

		uint8 ReadU8()
		{
			return Read<uint8>();
		}

		uint16 ReadU16()
		{
			return Read<uint16>();
		}

		uint32 ReadU32()
		{
			return Read<uint32>();
		}

		int32 ReadI32()
		{
			return Read<int32>();
		}

		float ReadF32()
		{
			return Read<float>();
		}

		FVector ReadVec3()
		{
			const float X = ReadF32();
			const float Y = ReadF32();
			const float Z = ReadF32();
			return FVector(X, Y, Z);
		}

		FString ReadBytes(uint32 Count)
		{
			TArray<ANSICHAR> Buffer;
			Buffer.SetNumUninitialized(static_cast<int32>(Count));
			Ar.Serialize(Buffer.GetData(), Count);
			if (Ar.IsError())
			{
				return FString();
			}
			return FString(static_cast<int32>(Count), Buffer.GetData());
		}

		void Skip(int64 Count)
		{
			if (!Ar.IsError() && Ar.Tell() + Count <= Ar.TotalSize())
			{
				Ar.Seek(Ar.Tell() + Count);
			}
			else
			{
				Ar.SetError();
			}
		}

	private:
		template <typename T>
		T Read()
		{
			T Value = 0;
			Ar << Value;
			return Ar.IsError() ? T(0) : Value;
		}

		FMemoryReader Ar;
	};

} // namespace

FString ResolveLevelAssetPath(const FString& LevelPath, const FString& RelativeOrKey)
{
	if (RelativeOrKey.IsEmpty())
	{
		return FString();
	}
	auto Normalized = [](FString Path)
	{
		FPaths::NormalizeFilename(Path);
		FPaths::CollapseRelativeDirectories(Path);
		return Path;
	};
	if (!FPaths::IsRelative(RelativeOrKey) && FPaths::FileExists(RelativeOrKey))
	{
		return Normalized(RelativeOrKey);
	}

	// <pack>/Content/Levels/Main.llev -> content root <pack>/Content (legacy <pack>/Levels/Main.llev -> <pack>/).
	const FString ContentRoot = FPaths::GetPath(FPaths::GetPath(LevelPath));
	const FString InPack = Normalized(FPaths::Combine(ContentRoot, RelativeOrKey));
	if (FPaths::FileExists(InPack))
	{
		return InPack;
	}

	// Legacy Projects/<name>/Materials/... after the pack was copied elsewhere.
	const int32 MatPos = RelativeOrKey.Find("Materials/", ESearchCase::CaseSensitive);
	if (MatPos != INDEX_NONE)
	{
		const FString Legacy = Normalized(FPaths::Combine(ContentRoot, RelativeOrKey.Mid(MatPos)));
		if (FPaths::FileExists(Legacy))
		{
			return Legacy;
		}
	}

	return FPaths::ResolveLegacyContentPath(RelativeOrKey);
}

namespace
{

	/**
	 * Basic shape backing a stored actor class; false for PlayerStart / AISpawnPoint / StaticMesh / the volumes (a
	 * BlockingVolume's box is its brush).
	 */
	[[nodiscard]] bool BasicShapeForActorClass(ELevelActorClass InActorClass, EBasicShape& OutShape)
	{
		switch (InActorClass)
		{
			case ELevelActorClass::Cube:
				OutShape = EBasicShape::Cube;
				return true;
			case ELevelActorClass::Sphere:
				OutShape = EBasicShape::Sphere;
				return true;
			case ELevelActorClass::Plane:
				OutShape = EBasicShape::Plane;
				return true;
			default:
				return false;
		}
	}

	/** Records that become an AStaticMeshActor. */
	[[nodiscard]] bool IsMeshRecord(ELevelActorClass ActorClass)
	{
		EBasicShape Shape{};
		return ActorClass == ELevelActorClass::StaticMesh || BasicShapeForActorClass(ActorClass, Shape);
	}

	/** Writes a world transform into a record's legacy position, XYZ Euler degrees and scale. */
	void SetLegacyTransform(FLevelActorRecord& Record, const FTransform& Transform)
	{
		Record.Position = FLegacyCoordinateConversion::ToLegacyPosition(Transform.GetLocation());
		Record.RotationDegrees = FLegacyCoordinateConversion::ToLegacyEulerXYZ(Transform.GetRotation());
		Record.Scale = FLegacyCoordinateConversion::ToLegacyScale(Transform.GetScale3D());
	}

	/** Player starts and AI spawn points face their +X in the world; the legacy records faced their +Z. */
	[[nodiscard]] bool IsActorLikeRecord(ELevelActorClass ActorClass)
	{
		return ActorClass == ELevelActorClass::PlayerStart || ActorClass == ELevelActorClass::AISpawnPoint;
	}

	/** SetLegacyTransform for an actor-like record (FLegacyCoordinateConversion::ToLegacyActorEulerXYZ). */
	void SetLegacyActorTransform(FLevelActorRecord& Record, const FTransform& Transform)
	{
		SetLegacyTransform(Record, Transform);
		Record.RotationDegrees = FLegacyCoordinateConversion::ToLegacyActorEulerXYZ(Transform.GetRotation());
	}

	/** Writes a light transform into a record's legacy position and (pitch, yaw, 0) degrees. */
	void SetLegacyLightTransform(FLevelLightRecord& Record, const FTransform& Transform)
	{
		float Pitch = 0.0f;
		float Yaw = 0.0f;
		FLegacyCoordinateConversion::ToLegacyLightRotation(Transform.GetRotation(), Pitch, Yaw);
		Record.Position = FLegacyCoordinateConversion::ToLegacyPosition(Transform.GetLocation());
		Record.RotationDegrees = FVector(Pitch, Yaw, 0.0f);
	}

	/** The actor's first tag as the record's tag (the reader stores a non-empty record tag as the only tag). */
	[[nodiscard]] FString RecordTag(const AActor& Actor)
	{
		return Actor.Tags.Num() > 0 ? Actor.Tags[0].ToString() : FString();
	}

	/** The record tag as the actor's Tags (plan decision D15: meaning lives in Tags). */
	void ApplyRecordTag(AActor& Actor, const FString& Tag)
	{
		if (!Tag.IsEmpty())
		{
			Actor.Tags.Add(FName(*Tag));
		}
	}

	/** The actor's `.llev` data, or the record defaults for an actor the reader did not spawn. */
	[[nodiscard]] const ULegacyLevelDataComponent& LegacyData(const AActor& Actor)
	{
		if (const ULegacyLevelDataComponent* Data = Actor.FindComponentByClass<ULegacyLevelDataComponent>())
		{
			return *Data;
		}
		return *GetDefault<ULegacyLevelDataComponent>();
	}

	/** Gives a spawned actor its `.llev` data component. */
	ULegacyLevelDataComponent& AddLegacyData(AActor& Actor, ELevelActorClass ActorClass)
	{
		ULegacyLevelDataComponent* Data = NewObject<ULegacyLevelDataComponent>(&Actor, TEXT("LegacyLevelData"));
		Data->RegisterComponent();
		Data->ActorClass = ActorClass;
		return *Data;
	}

	/** The mesh record an AStaticMeshActor or an ABlockingVolume writes; false when it cannot be written. */
	[[nodiscard]] bool BuildMeshRecord(const AActor& Actor, FLevelActorRecord& Record)
	{
		const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor.GetRootComponent());
		if (Primitive == nullptr)
		{
			return false;
		}
		const ULegacyLevelDataComponent& Data = LegacyData(Actor);
		const bool bBlockingVolume = Actor.IsA<ABlockingVolume>();
		Record.ActorClass = bBlockingVolume ? ELevelActorClass::BlockingVolume : Data.ActorClass;
		if (!bBlockingVolume && !IsMeshRecord(Record.ActorClass))
		{
			Record.ActorClass = ELevelActorClass::StaticMesh;
		}
		// Imported meshes are only reloadable through their path.
		if (Record.ActorClass == ELevelActorClass::StaticMesh && Data.MeshPath.IsEmpty())
		{
			UE_LOG(LogLevel, Warning, "LeonLevelFormat: skipping StaticMesh actor without mesh path");
			return false;
		}

		Record.Mobility = Primitive->Mobility;
		Record.bCollisionEnabled = Primitive->IsCollisionEnabled();
		Record.bSimulatePhysics = Primitive->IsSimulatingPhysics();
		Record.bEnableGravity = Primitive->IsGravityEnabled();
		Record.bHidden = Actor.IsHidden();

		SetLegacyTransform(Record, Primitive->GetComponentTransform());

		Record.Tag = RecordTag(Actor);
		Record.MaterialPath = Data.MaterialPath;
		if (Record.ActorClass == ELevelActorClass::StaticMesh)
		{
			Record.MeshPath = Data.MeshPath;
		}

		Record.SphereSegments = Data.SphereSegments;
		Record.SphereRings = Data.SphereRings;

		Record.bHasSpinYaw = Data.SpinYaw != 0.0f;
		Record.SpinYaw = Record.bHasSpinYaw ? FLegacyCoordinateConversion::ToLegacyYawRate(Data.SpinYaw) : 0.0f;

		Record.bHasBob = Data.bHasBob;
		Record.BobBaseY = FLegacyCoordinateConversion::ToLegacyLength(Data.BobBaseZ);
		Record.BobAmplitude = FLegacyCoordinateConversion::ToLegacyLength(Data.BobAmplitude);
		Record.BobSpeed = Data.BobSpeed;
		return true;
	}

	/** The level-content classes a `.llev` spawns; a reload replaces them (gameplay actors stay). */
	[[nodiscard]] bool IsLevelContentActor(const AActor& Actor)
	{
		return Actor.IsA<AStaticMeshActor>() || Actor.IsA<APlayerStart>() || Actor.IsA<AVolume>() ||
			Actor.IsA<ALight>() || Actor.IsA<ATargetPoint>() || Actor.IsA<AWorldSettings>();
	}

} // namespace

FLevelDocument BuildLevelDocument(const ULevel& Level, const UCameraComponent& InCamera)
{
	FLevelDocument Doc;
	if (const AWorldSettings* WorldSettings = Level.GetWorldSettings())
	{
		const ULegacyLevelDataComponent& Data = LegacyData(*WorldSettings);
		Doc.Name = Data.LevelName;
		Doc.GameMode = Data.GameModeName;
	}

	Doc.Camera.Mode = InCamera.GetMode();
	Doc.Camera.Target = FLegacyCoordinateConversion::ToLegacyPosition(InCamera.GetTarget());
	Doc.Camera.Eye = FLegacyCoordinateConversion::ToLegacyPosition(InCamera.EyeLocation());
	Doc.Camera.Distance = FLegacyCoordinateConversion::ToLegacyLength(InCamera.GetDistance());
	// Legacy yaw / pitch meant the eye's offset from the target in Orbit and the look direction in FreeLook.
	if (InCamera.GetMode() == ECameraMode::FreeLook)
	{
		FLegacyCoordinateConversion::ToLegacyFreeLookRotation(
			InCamera.GetViewRotation(), Doc.Camera.Yaw, Doc.Camera.Pitch);
	}
	else
	{
		FLegacyCoordinateConversion::ToLegacyOrbitRotation(
			InCamera.GetViewRotation(), Doc.Camera.Yaw, Doc.Camera.Pitch);
	}

	// The records go out grouped by class in this order, as the format always wrote them; each group keeps the actors'
	// spawn order.
	TArray<const AActor*> LiveActors;
	for (const AActor* Actor : Level.Actors)
	{
		if (Actor != nullptr && !Actor->IsPendingKillPending())
		{
			LiveActors.Add(Actor);
		}
	}

	for (const AActor* Actor : LiveActors)
	{
		// A Play From Here start (UEngine::LoadMap's, from the camera framing) is not a level record.
		if (!Actor->IsA<APlayerStart>() || Actor->IsA<APlayerStartPIE>())
		{
			continue;
		}
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::PlayerStart;
		SetLegacyActorTransform(Record, Actor->GetActorTransform());
		Record.bEnableGravity = false;
		Doc.Actors.Add(MoveTemp(Record));
	}

	for (const AActor* Actor : LiveActors)
	{
		if (!Actor->IsA<ATargetPoint>())
		{
			continue;
		}
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::AISpawnPoint;
		SetLegacyActorTransform(Record, Actor->GetActorTransform());
		Record.Tag = RecordTag(*Actor);
		Record.bEnableGravity = false;
		Doc.Actors.Add(MoveTemp(Record));
	}

	for (const AActor* Actor : LiveActors)
	{
		if (!Actor->IsA<ATriggerVolume>())
		{
			continue;
		}
		const ULegacyLevelDataComponent& Data = LegacyData(*Actor);
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::TriggerVolume;
		SetLegacyTransform(Record, Actor->GetActorTransform());
		Record.Tag = RecordTag(*Actor);
		Record.InteractCost = Data.InteractCost;
		Record.InteractRadius = FLegacyCoordinateConversion::ToLegacyLength(Data.InteractRadius);
		Record.Payload = Data.Payload;
		Record.bConsumeOnUse = Data.bConsumeOnUse;
		Record.bEnableGravity = false;
		Doc.Actors.Add(MoveTemp(Record));
	}

	for (const AActor* Actor : LiveActors)
	{
		const APainCausingVolume* Volume = Cast<APainCausingVolume>(Actor);
		if (Volume == nullptr)
		{
			continue;
		}
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::PainCausingVolume;
		SetLegacyTransform(Record, Volume->GetActorTransform());
		Record.Tag = RecordTag(*Volume);
		Record.DamagePerSecond = Volume->DamagePerSec;
		Record.DamageInterval = Volume->PainInterval;
		Record.bEnableGravity = false;
		Doc.Actors.Add(MoveTemp(Record));
	}

	for (const AActor* Actor : LiveActors)
	{
		if (!Actor->IsA<AStaticMeshActor>() && !Actor->IsA<ABlockingVolume>())
		{
			continue;
		}
		FLevelActorRecord Record;
		if (BuildMeshRecord(*Actor, Record))
		{
			Doc.Actors.Add(MoveTemp(Record));
		}
	}

	for (const AActor* Actor : LiveActors)
	{
		const ADirectionalLight* Light = Cast<ADirectionalLight>(Actor);
		if (Light == nullptr)
		{
			continue;
		}
		const UDirectionalLightComponent& Component = *Light->GetDirectionalLightComponent();
		FLevelLightRecord Record;
		Record.LightClass = ELevelLightClass::DirectionalLight;
		Record.bCastShadows = Component.CastShadows;
		SetLegacyLightTransform(Record, Component.GetComponentTransform());
		Record.LightColor = FVector(Component.LightColor.R, Component.LightColor.G, Component.LightColor.B);
		Record.Intensity = Component.Intensity;
		Record.SourceAngle = Component.LightSourceAngle;
		Doc.Lights.Add(Record);
	}
	for (const AActor* Actor : LiveActors)
	{
		const APointLight* Light = Cast<APointLight>(Actor);
		if (Light == nullptr)
		{
			continue;
		}
		const UPointLightComponent& Component = *Light->GetPointLightComponent();
		const ULegacyLevelDataComponent& Data = LegacyData(*Light);
		FLevelLightRecord Record;
		Record.LightClass = ELevelLightClass::PointLight;
		Record.bCastShadows = Component.CastShadows;
		SetLegacyLightTransform(Record, Component.GetComponentTransform());
		Record.LightColor = FVector(Component.LightColor.R, Component.LightColor.G, Component.LightColor.B);
		Record.Intensity = Component.Intensity;
		Record.Range = FLegacyCoordinateConversion::ToLegacyLength(Component.AttenuationRadius);
		Record.bHasOrbit = Data.bHasOrbit;
		Record.OrbitRadius = FLegacyCoordinateConversion::ToLegacyLength(Data.OrbitRadius);
		Record.OrbitHeight = FLegacyCoordinateConversion::ToLegacyLength(Data.OrbitHeight);
		Record.OrbitHeightAmp = FLegacyCoordinateConversion::ToLegacyLength(Data.OrbitHeightAmp);
		Record.OrbitSpeed = Data.OrbitSpeed;
		Doc.Lights.Add(Record);
	}

	return Doc;
}

TArray<uint8> SerializeLeonLevel(const FLevelDocument& Doc)
{
	FStringTableBuilder LocalStrings;
	const uint32 NameIdx = LocalStrings.Add(Doc.Name);
	const uint32 GameModeIdx = LocalStrings.Add(Doc.GameMode);
	const uint32 EnvironmentIdx = LocalStrings.Add(Doc.EnvironmentPath);

	struct FActorIndices
	{
		uint32 Flags = 0;
		uint32 Tag = 0;
		uint32 Material = 0;
		uint32 Mesh = 0;
		uint32 LightmapId = 0;
		uint32 LightmapPath = 0;
		uint32 Payload = 0;
	};
	TArray<FActorIndices> ActorIndices;
	ActorIndices.Reserve(Doc.Actors.Num());
	for (const FLevelActorRecord& Actor : Doc.Actors)
	{
		FActorIndices Indices;
		if (Actor.bCollisionEnabled)
		{
			Indices.Flags |= LevelActorFlagCollisionEnabled;
		}
		if (Actor.bSimulatePhysics)
		{
			Indices.Flags |= LevelActorFlagSimulatePhysics;
		}
		if (Actor.bEnableGravity)
		{
			Indices.Flags |= LevelActorFlagEnableGravity;
		}
		if (Actor.bHidden)
		{
			Indices.Flags |= LevelActorFlagHidden;
		}
		if (Actor.bHasBob)
		{
			Indices.Flags |= LevelActorFlagHasBob;
		}
		if (Actor.bHasSpinYaw)
		{
			Indices.Flags |= LevelActorFlagHasSpinYaw;
		}
		if (Actor.bHasFitHeight)
		{
			Indices.Flags |= LevelActorFlagHasFitHeight;
		}
		if (!Actor.Tag.IsEmpty())
		{
			Indices.Flags |= LevelActorFlagHasTag;
			Indices.Tag = LocalStrings.Add(Actor.Tag);
		}
		if (!Actor.MaterialPath.IsEmpty())
		{
			Indices.Flags |= LevelActorFlagHasMaterial;
			Indices.Material = LocalStrings.Add(Actor.MaterialPath);
		}
		if (!Actor.MeshPath.IsEmpty())
		{
			Indices.Flags |= LevelActorFlagHasMesh;
			Indices.Mesh = LocalStrings.Add(Actor.MeshPath);
		}
		if (!Actor.LightmapId.IsEmpty())
		{
			Indices.Flags |= LevelActorFlagHasLightmapId;
			Indices.LightmapId = LocalStrings.Add(Actor.LightmapId);
		}
		if (!Actor.LightmapPath.IsEmpty())
		{
			Indices.Flags |= LevelActorFlagHasLightmapPath;
			Indices.LightmapPath = LocalStrings.Add(Actor.LightmapPath);
		}
		if (Actor.ActorClass == ELevelActorClass::TriggerVolume || Actor.InteractCost != 0 ||
			Actor.InteractRadius != 2.0f)
		{
			Indices.Flags |= LevelActorFlagHasInteractCost;
		}
		if (Actor.ActorClass == ELevelActorClass::PainCausingVolume || Actor.DamagePerSecond != 12.0f ||
			Actor.DamageInterval != 0.35f)
		{
			Indices.Flags |= LevelActorFlagHasPainData;
		}
		if (!Actor.Payload.IsEmpty())
		{
			Indices.Flags |= LevelActorFlagHasPayload;
			Indices.Payload = LocalStrings.Add(Actor.Payload);
		}
		if (Actor.bConsumeOnUse)
		{
			Indices.Flags |= LevelActorFlagConsumeOnUse;
		}
		ActorIndices.Add(Indices);
	}

	TArray<uint8> Bytes;
	FMemoryWriter Out(Bytes);
	WriteU32(Out, LeonLevelMagic);
	WriteU32(Out, LeonLevelVersion);
	WriteU32(Out, 0u); // flags (reserved)

	LocalStrings.WriteTo(Out);

	WriteU32(Out, NameIdx);
	WriteU32(Out, GameModeIdx);
	WriteU32(Out, EnvironmentIdx);
	WriteF32(Out, Doc.EnvironmentExposure);

	WriteU8(Out, static_cast<uint8>(Doc.Camera.Mode));
	WriteU8(Out, 0u);
	WriteU8(Out, 0u);
	WriteU8(Out, 0u);
	WriteVec3(Out, Doc.Camera.Target);
	WriteVec3(Out, Doc.Camera.Eye);
	WriteF32(Out, Doc.Camera.Distance);
	WriteF32(Out, Doc.Camera.Yaw);
	WriteF32(Out, Doc.Camera.Pitch);

	WriteU32(Out, static_cast<uint32>(Doc.Actors.Num()));
	for (int32 I = 0; I < Doc.Actors.Num(); ++I)
	{
		const FLevelActorRecord& Actor = Doc.Actors[I];
		const FActorIndices& Indices = ActorIndices[I];

		WriteU8(Out, static_cast<uint8>(Actor.ActorClass));
		WriteU8(Out, static_cast<uint8>(Actor.Mobility));
		WriteU16(Out, 0u);
		WriteU32(Out, Indices.Flags);
		WriteVec3(Out, Actor.Position);
		WriteVec3(Out, Actor.RotationDegrees);
		WriteVec3(Out, Actor.Scale);

		if ((Indices.Flags & LevelActorFlagHasTag) != 0u)
		{
			WriteU32(Out, Indices.Tag);
		}
		if ((Indices.Flags & LevelActorFlagHasMaterial) != 0u)
		{
			WriteU32(Out, Indices.Material);
		}
		if ((Indices.Flags & LevelActorFlagHasMesh) != 0u)
		{
			WriteU32(Out, Indices.Mesh);
		}
		if ((Indices.Flags & LevelActorFlagHasLightmapId) != 0u)
		{
			WriteU32(Out, Indices.LightmapId);
		}
		if ((Indices.Flags & LevelActorFlagHasLightmapPath) != 0u)
		{
			WriteU32(Out, Indices.LightmapPath);
		}
		WriteU32(Out, Actor.LightmapResolution);

		if (Actor.ActorClass == ELevelActorClass::Sphere)
		{
			WriteI32(Out, Actor.SphereSegments);
			WriteI32(Out, Actor.SphereRings);
		}
		if ((Indices.Flags & LevelActorFlagHasSpinYaw) != 0u)
		{
			WriteF32(Out, Actor.SpinYaw);
		}
		if ((Indices.Flags & LevelActorFlagHasBob) != 0u)
		{
			WriteF32(Out, Actor.BobBaseY);
			WriteF32(Out, Actor.BobAmplitude);
			WriteF32(Out, Actor.BobSpeed);
		}
		if ((Indices.Flags & LevelActorFlagHasFitHeight) != 0u)
		{
			WriteF32(Out, Actor.FitHeight);
		}
		if ((Indices.Flags & LevelActorFlagHasInteractCost) != 0u)
		{
			WriteI32(Out, Actor.InteractCost);
			WriteF32(Out, Actor.InteractRadius);
		}
		if ((Indices.Flags & LevelActorFlagHasPainData) != 0u)
		{
			WriteF32(Out, Actor.DamagePerSecond);
			WriteF32(Out, Actor.DamageInterval);
		}
		if ((Indices.Flags & LevelActorFlagHasPayload) != 0u)
		{
			WriteU32(Out, Indices.Payload);
		}
	}

	WriteU32(Out, static_cast<uint32>(Doc.Lights.Num()));
	for (const FLevelLightRecord& Light : Doc.Lights)
	{
		uint32 LocalFlags = 0;
		if (Light.bCastShadows)
		{
			LocalFlags |= LevelLightFlagCastShadows;
		}
		if (Light.bHasOrbit)
		{
			LocalFlags |= LevelLightFlagHasOrbit;
		}

		WriteU8(Out, static_cast<uint8>(Light.LightClass));
		WriteU8(Out, 0u);
		WriteU8(Out, 0u);
		WriteU8(Out, 0u);
		WriteU32(Out, LocalFlags);
		WriteVec3(Out, Light.Position);
		WriteVec3(Out, Light.RotationDegrees);
		WriteVec3(Out, Light.LightColor);
		WriteF32(Out, Light.Intensity);
		WriteF32(Out, Light.Range);
		WriteF32(Out, Light.SourceAngle);
		if ((LocalFlags & LevelLightFlagHasOrbit) != 0u)
		{
			WriteF32(Out, Light.OrbitRadius);
			WriteF32(Out, Light.OrbitHeight);
			WriteF32(Out, Light.OrbitHeightAmp);
			WriteF32(Out, Light.OrbitSpeed);
		}
	}

	return Bytes;
}

bool DeserializeLeonLevel(const TArray<uint8>& InBytes, FLevelDocument& Out)
{
	Out = FLevelDocument();

	FByteReader Reader(InBytes);
	if (Reader.ReadU32() != LeonLevelMagic)
	{
		UE_LOG(LogLevel, Error, "LeonLevelFormat: bad magic (expected 'LLEV')");
		return false;
	}
	const uint32 Version = Reader.ReadU32();
	// Flow: .llev load — accept v1 (legacy) and v2 (typed volumes); reject unknown.
	if (Version != 1u && Version != 2u)
	{
		UE_LOG(LogLevel, Error, "LeonLevelFormat: unsupported version %u (expected 1 or 2)", Version);
		return false;
	}
	(void)Reader.ReadU32(); // flags (reserved)

	const uint32 StringCount = Reader.ReadU32();
	if (Reader.HasFailed() || StringCount > MaxLeonLevelStrings)
	{
		return false;
	}
	TArray<FString> LocalStrings;
	LocalStrings.Reserve(static_cast<int32>(StringCount));
	for (uint32 I = 0; I < StringCount; ++I)
	{
		const uint32 Length = Reader.ReadU32();
		if (Reader.HasFailed() || Length > MaxLeonStringBytes)
		{
			return false;
		}
		LocalStrings.Add(Reader.ReadBytes(Length));
		if (Reader.HasFailed())
		{
			return false;
		}
	}
	// Out-of-range indices resolve to the empty string instead of failing the load.
	const auto StringAt = [&LocalStrings](uint32 Index) -> FString
	{ return Index < static_cast<uint32>(LocalStrings.Num()) ? LocalStrings[static_cast<int32>(Index)] : FString(); };

	Out.Name = StringAt(Reader.ReadU32());
	Out.GameMode = StringAt(Reader.ReadU32());
	Out.EnvironmentPath = StringAt(Reader.ReadU32());
	Out.EnvironmentExposure = Reader.ReadF32();

	const uint8 CameraMode = Reader.ReadU8();
	Reader.Skip(3);
	Out.Camera.Mode = CameraMode == 1 ? ECameraMode::FreeLook : ECameraMode::Orbit;
	Out.Camera.Target = Reader.ReadVec3();
	Out.Camera.Eye = Reader.ReadVec3();
	Out.Camera.Distance = Reader.ReadF32();
	Out.Camera.Yaw = Reader.ReadF32();
	Out.Camera.Pitch = Reader.ReadF32();
	if (Reader.HasFailed())
	{
		return false;
	}

	const uint32 ActorCount = Reader.ReadU32();
	if (Reader.HasFailed() || ActorCount > MaxLeonLevelActors)
	{
		return false;
	}
	Out.Actors.Reserve(static_cast<int32>(ActorCount));
	for (uint32 I = 0; I < ActorCount; ++I)
	{
		FLevelActorRecord Actor;
		const uint8 LocalActorClass = Reader.ReadU8();
		const uint8 LocalMobility = Reader.ReadU8();
		(void)Reader.ReadU16();
		const uint32 LocalFlags = Reader.ReadU32();
		if (Reader.HasFailed())
		{
			return false;
		}
		if (LocalActorClass > static_cast<uint8>(ELevelActorClass::AISpawnPoint))
		{
			UE_LOG(LogLevel, Error, "LeonLevelFormat: unknown actor class %d", static_cast<int32>(LocalActorClass));
			return false;
		}
		Actor.ActorClass = static_cast<ELevelActorClass>(LocalActorClass);
		Actor.Mobility = LocalMobility == 1 ? EComponentMobility::Movable : EComponentMobility::Static;
		Actor.bCollisionEnabled = (LocalFlags & LevelActorFlagCollisionEnabled) != 0u;
		Actor.bSimulatePhysics = (LocalFlags & LevelActorFlagSimulatePhysics) != 0u;
		Actor.bEnableGravity = (LocalFlags & LevelActorFlagEnableGravity) != 0u;
		Actor.bHidden = (LocalFlags & LevelActorFlagHidden) != 0u;
		Actor.bHasBob = (LocalFlags & LevelActorFlagHasBob) != 0u;
		Actor.bHasSpinYaw = (LocalFlags & LevelActorFlagHasSpinYaw) != 0u;
		Actor.bHasFitHeight = (LocalFlags & LevelActorFlagHasFitHeight) != 0u;
		Actor.bConsumeOnUse = (LocalFlags & LevelActorFlagConsumeOnUse) != 0u;

		Actor.Position = Reader.ReadVec3();
		Actor.RotationDegrees = Reader.ReadVec3();
		Actor.Scale = Reader.ReadVec3();

		if ((LocalFlags & LevelActorFlagHasTag) != 0u)
		{
			Actor.Tag = StringAt(Reader.ReadU32());
		}
		if ((LocalFlags & LevelActorFlagHasMaterial) != 0u)
		{
			Actor.MaterialPath = StringAt(Reader.ReadU32());
		}
		if ((LocalFlags & LevelActorFlagHasMesh) != 0u)
		{
			Actor.MeshPath = StringAt(Reader.ReadU32());
		}
		if ((LocalFlags & LevelActorFlagHasLightmapId) != 0u)
		{
			Actor.LightmapId = StringAt(Reader.ReadU32());
		}
		if ((LocalFlags & LevelActorFlagHasLightmapPath) != 0u)
		{
			Actor.LightmapPath = StringAt(Reader.ReadU32());
		}
		Actor.LightmapResolution = Reader.ReadU32();

		if (Actor.ActorClass == ELevelActorClass::Sphere)
		{
			Actor.SphereSegments = Reader.ReadI32();
			Actor.SphereRings = Reader.ReadI32();
		}
		if (Actor.bHasSpinYaw)
		{
			Actor.SpinYaw = Reader.ReadF32();
		}
		if (Actor.bHasBob)
		{
			Actor.BobBaseY = Reader.ReadF32();
			Actor.BobAmplitude = Reader.ReadF32();
			Actor.BobSpeed = Reader.ReadF32();
		}
		if (Actor.bHasFitHeight)
		{
			Actor.FitHeight = Reader.ReadF32();
		}
		if ((LocalFlags & LevelActorFlagHasInteractCost) != 0u)
		{
			Actor.InteractCost = Reader.ReadI32();
			Actor.InteractRadius = Reader.ReadF32();
		}
		if ((LocalFlags & LevelActorFlagHasPainData) != 0u)
		{
			Actor.DamagePerSecond = Reader.ReadF32();
			Actor.DamageInterval = Reader.ReadF32();
		}
		if ((LocalFlags & LevelActorFlagHasPayload) != 0u)
		{
			Actor.Payload = StringAt(Reader.ReadU32());
		}
		if (Reader.HasFailed())
		{
			return false;
		}
		Out.Actors.Add(MoveTemp(Actor));
	}

	const uint32 LightCount = Reader.ReadU32();
	if (Reader.HasFailed() || LightCount > MaxLeonLevelLights)
	{
		return false;
	}
	Out.Lights.Reserve(static_cast<int32>(LightCount));
	for (uint32 I = 0; I < LightCount; ++I)
	{
		FLevelLightRecord Light;
		const uint8 LocalLightClass = Reader.ReadU8();
		Reader.Skip(3);
		const uint32 LocalFlags = Reader.ReadU32();
		if (Reader.HasFailed())
		{
			return false;
		}
		if (LocalLightClass > static_cast<uint8>(ELevelLightClass::PointLight))
		{
			UE_LOG(LogLevel, Error, "LeonLevelFormat: unknown light class %d", static_cast<int32>(LocalLightClass));
			return false;
		}
		Light.LightClass = static_cast<ELevelLightClass>(LocalLightClass);
		Light.bCastShadows = (LocalFlags & LevelLightFlagCastShadows) != 0u;
		Light.bHasOrbit = (LocalFlags & LevelLightFlagHasOrbit) != 0u;

		Light.Position = Reader.ReadVec3();
		Light.RotationDegrees = Reader.ReadVec3();
		Light.LightColor = Reader.ReadVec3();
		Light.Intensity = Reader.ReadF32();
		Light.Range = Reader.ReadF32();
		Light.SourceAngle = Reader.ReadF32();
		if (Light.bHasOrbit)
		{
			Light.OrbitRadius = Reader.ReadF32();
			Light.OrbitHeight = Reader.ReadF32();
			Light.OrbitHeightAmp = Reader.ReadF32();
			Light.OrbitSpeed = Reader.ReadF32();
		}
		if (Reader.HasFailed())
		{
			return false;
		}
		Out.Lights.Add(Light);
	}

	return !Reader.HasFailed();
}

bool SaveLeonLevelFile(const FString& Path, const FLevelDocument& Doc)
{
	const TArray<uint8> LocalBytes = SerializeLeonLevel(Doc);
	if (!FFileHelper::SaveArrayToFile(LocalBytes, *Path))
	{
		UE_LOG(LogLevel, Error, "LeonLevelFormat: cannot write: %s", *Path);
		return false;
	}
	return true;
}

bool LoadLeonLevelFile(const FString& Path, FLevelDocument& Out)
{
	TArray<uint8> LocalBytes;
	if (!FFileHelper::LoadFileToArray(LocalBytes, *Path))
	{
		UE_LOG(LogLevel, Error, "LeonLevelFormat: cannot open %s", *Path);
		return false;
	}
	if (!DeserializeLeonLevel(LocalBytes, Out))
	{
		UE_LOG(LogLevel, Error, "LeonLevelFormat: failed to parse %s", *Path);
		return false;
	}
	return true;
}

namespace
{

	/** A record with its world transform and the resources its actor needs, resolved before anything spawns. */
	struct FResolvedActorRecord
	{
		const FLevelActorRecord* Record = nullptr;
		FTransform Transform;
		TSharedPtr<UStaticMesh> Mesh;
		/** The `.lmat` material, or the default material for a mesh without materials of its own. */
		FMaterial Material;
		bool bHasMaterial = false;
	};

	/**
	 * Resolves a record: its transform, and for a mesh or a blocking volume its mesh (a basic shape or the `.lmesh`)
	 * and material, then the fit height. False when the mesh cannot be loaded.
	 */
	[[nodiscard]] bool ResolveActorRecord(const FLevelActorRecord& Record, FResourceCache& Resources,
		const FString& SourcePath, FResolvedActorRecord& Out)
	{
		Out.Record = &Record;
		Out.Transform =
			FLegacyCoordinateConversion::ConvertTransform(Record.Position, Record.RotationDegrees, Record.Scale);
		if (IsActorLikeRecord(Record.ActorClass))
		{
			Out.Transform.SetRotation(FLegacyCoordinateConversion::ConvertActorEulerXYZ(Record.RotationDegrees));
		}
		const bool bBlockingVolume = Record.ActorClass == ELevelActorClass::BlockingVolume;
		if (!IsMeshRecord(Record.ActorClass) && !bBlockingVolume)
		{
			return true;
		}

		EBasicShape ShapeType{};
		// A blocking volume's brush is a 100 cm box, the basic cube's size: its fit height measures that cube.
		const bool bIsBasicShape = bBlockingVolume || BasicShapeForActorClass(Record.ActorClass, ShapeType);
		if (bBlockingVolume)
		{
			ShapeType = EBasicShape::Cube;
		}
		if (bIsBasicShape)
		{
			Out.Mesh = MeshForBasicShape(Resources, ShapeType, Record.SphereSegments, Record.SphereRings);
			Out.Material = Resources.DefaultMaterial();
			Out.bHasMaterial = true;
		}
		else
		{
			Out.Mesh = Resources.LoadStaticMesh(ResolveLevelAssetPath(SourcePath, Record.MeshPath));
		}
		if (Out.Mesh == nullptr)
		{
			UE_LOG(LogLevel, Error, "LeonLevelFormat: failed mesh for actor in %s", *SourcePath);
			return false;
		}

		if (Record.bHasFitHeight && Record.FitHeight > 0.0f)
		{
			ApplyFitHeight(Out.Transform, *Out.Mesh, FLegacyCoordinateConversion::ConvertLength(Record.FitHeight));
		}

		if (!Record.MaterialPath.IsEmpty())
		{
			Out.Material = Resources.LoadMaterial(ResolveLevelAssetPath(SourcePath, Record.MaterialPath));
			Out.bHasMaterial = true;
		}
		else if (!Out.Mesh->HasMaterials())
		{
			Out.Material = Resources.DefaultMaterial();
			Out.bHasMaterial = true;
		}
		return true;
	}

	/**
	 * A mesh record's material on its component: every slot of a mesh with materials when the record names one, else
	 * every section's slot of a mesh without materials; a mesh with materials and no record material keeps its own.
	 */
	void ApplyRecordMaterial(UStaticMeshComponent& Component, const FResolvedActorRecord& Resolved)
	{
		if (!Resolved.bHasMaterial)
		{
			return;
		}
		const UStaticMesh& Mesh = *Resolved.Mesh;
		int32 NumSlots = Mesh.GetMaterials().Num();
		if (!Mesh.HasMaterials())
		{
			NumSlots = 1;
			for (const FMeshSection& Section : Mesh.GetSubmeshes())
			{
				NumSlots = FMath::Max(NumSlots, Section.MaterialIndex + 1);
			}
		}
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			Component.SetMaterial(Slot, Resolved.Material);
		}
	}

	/** The collision flags of a mesh or blocking volume record (simulating implies collision). */
	void ApplyRecordCollision(UPrimitiveComponent& Component, const FLevelActorRecord& Record)
	{
		Component.SetMobility(Record.Mobility);
		Component.SetSimulatePhysics(Record.bSimulatePhysics);
		Component.SetEnableGravity(Record.bEnableGravity);
		Component.SetCollisionEnabled(Record.bCollisionEnabled || Record.bSimulatePhysics
				? ECollisionEnabled::QueryAndPhysics
				: ECollisionEnabled::NoCollision);
	}

	/** The spin / bob animation and the tessellation of a mesh or blocking volume record. */
	void ApplyRecordMeshData(ULegacyLevelDataComponent& Data, const FLevelActorRecord& Record)
	{
		Data.MaterialPath = Record.MaterialPath;
		Data.SphereSegments = Record.SphereSegments;
		Data.SphereRings = Record.SphereRings;
		Data.SpinYaw = Record.bHasSpinYaw ? FLegacyCoordinateConversion::ConvertYawRate(Record.SpinYaw) : 0.0f;
		if (Record.bHasBob)
		{
			Data.bHasBob = true;
			Data.BobBaseZ = FLegacyCoordinateConversion::ConvertLength(Record.BobBaseY);
			Data.BobAmplitude = FLegacyCoordinateConversion::ConvertLength(Record.BobAmplitude);
			Data.BobSpeed = Record.BobSpeed;
		}
	}

	/** Spawns the actor of a resolved record in World. */
	void SpawnRecordActor(UWorld& World, const FResolvedActorRecord& Resolved)
	{
		const FLevelActorRecord& Record = *Resolved.Record;
		const FTransform& Transform = Resolved.Transform;
		switch (Record.ActorClass)
		{
			case ELevelActorClass::PlayerStart:
				(void)World.SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Transform);
				return;
			case ELevelActorClass::AISpawnPoint:
				if (ATargetPoint* Point = World.SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), Transform))
				{
					ApplyRecordTag(*Point, Record.Tag);
				}
				return;
			case ELevelActorClass::TriggerVolume:
				if (ATriggerVolume* Volume = World.SpawnActor<ATriggerVolume>(ATriggerVolume::StaticClass(), Transform))
				{
					ApplyRecordTag(*Volume, Record.Tag);
					ULegacyLevelDataComponent& Data = AddLegacyData(*Volume, Record.ActorClass);
					Data.InteractRadius = FLegacyCoordinateConversion::ConvertLength(Record.InteractRadius);
					Data.InteractCost = Record.InteractCost;
					Data.Payload = Record.Payload;
					Data.bConsumeOnUse = Record.bConsumeOnUse;
				}
				return;
			case ELevelActorClass::PainCausingVolume:
				if (APainCausingVolume* Volume =
						World.SpawnActor<APainCausingVolume>(APainCausingVolume::StaticClass(), Transform))
				{
					ApplyRecordTag(*Volume, Record.Tag);
					Volume->DamagePerSec = Record.DamagePerSecond;
					Volume->PainInterval = Record.DamageInterval;
				}
				return;
			case ELevelActorClass::BlockingVolume:
				if (ABlockingVolume* Volume =
						World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(), Transform))
				{
					// Plan decision D16: the brush box stands for the legacy cube; it is never drawn.
					ApplyRecordTag(*Volume, Record.Tag);
					Volume->SetActorHiddenInGame(Record.bHidden);
					ApplyRecordMeshData(AddLegacyData(*Volume, Record.ActorClass), Record);
					ApplyRecordCollision(*Volume->GetBrushComponent(), Record);
				}
				return;
			default:
				break;
		}

		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
		if (Actor == nullptr)
		{
			return;
		}
		ApplyRecordTag(*Actor, Record.Tag);
		Actor->SetActorHiddenInGame(Record.bHidden);
		ULegacyLevelDataComponent& Data = AddLegacyData(*Actor, Record.ActorClass);
		if (Record.ActorClass == ELevelActorClass::StaticMesh)
		{
			Data.MeshPath = Record.MeshPath;
		}
		ApplyRecordMeshData(Data, Record);
		UStaticMeshComponent& Component = *Actor->GetStaticMeshComponent();
		(void)Component.SetStaticMesh(Resolved.Mesh);
		ApplyRecordMaterial(Component, Resolved);
		ApplyRecordCollision(Component, Record);
	}

	/**
	 * Spawns the document's lights: at most MaxDirectionalLights directional and MaxPointLights point lights (the
	 * first ones of each kind), and the default sun when there is no directional light.
	 */
	void SpawnDocumentLights(UWorld& World, const FLevelDocument& Doc)
	{
		int32 NumDirectional = 0;
		int32 NumPoint = 0;
		for (const FLevelLightRecord& Record : Doc.Lights)
		{
			const bool bPoint = Record.LightClass == ELevelLightClass::PointLight;
			int32& Count = bPoint ? NumPoint : NumDirectional;
			++Count;
			if (Count > (bPoint ? MaxPointLights : MaxDirectionalLights))
			{
				continue;
			}
			FBasicLight Light;
			Light.Type = bPoint ? EBasicLight::Point : EBasicLight::Directional;
			// Legacy lights keep pitch in X and yaw in Y; the roll in Z was never used.
			Light.Transform = FTransform(
				FLegacyCoordinateConversion::ConvertLightRotation(Record.RotationDegrees.X, Record.RotationDegrees.Y),
				FLegacyCoordinateConversion::ConvertPosition(Record.Position));
			Light.LightColor = Record.LightColor;
			Light.Intensity = Record.Intensity;
			Light.bCastShadows = Record.bCastShadows;
			Light.SourceAngle = Record.SourceAngle;
			Light.Range = FLegacyCoordinateConversion::ConvertLength(Record.Range);
			ALight* Spawned = Light.SpawnIn(World);
			if (!bPoint || Spawned == nullptr || !Record.bHasOrbit)
			{
				continue;
			}
			ULegacyLevelDataComponent& Data = AddLegacyData(*Spawned, ELevelActorClass::StaticMesh);
			Data.bHasOrbit = true;
			Data.OrbitRadius = FLegacyCoordinateConversion::ConvertLength(Record.OrbitRadius);
			Data.OrbitHeight = FLegacyCoordinateConversion::ConvertLength(Record.OrbitHeight);
			Data.OrbitHeightAmp = FLegacyCoordinateConversion::ConvertLength(Record.OrbitHeightAmp);
			Data.OrbitSpeed = Record.OrbitSpeed;
		}

		if (NumDirectional == 0)
		{
			(void)FBasicLight::Directional().SpawnIn(World);
		}
		if (NumDirectional > MaxDirectionalLights)
		{
			UE_LOG(LogLevel, Warning, "LeonLevelFormat: truncating directional lights from %d to %d", NumDirectional,
				MaxDirectionalLights);
		}
		if (NumPoint > MaxPointLights)
		{
			UE_LOG(
				LogLevel, Warning, "LeonLevelFormat: truncating point lights from %d to %d", NumPoint, MaxPointLights);
		}
	}

} // namespace

UClass* ResolveLegacyLevelGameMode(const FString& GameModeName)
{
	const FString Name = GameModeName.TrimStartAndEnd();
	if (Name.IsEmpty() || Name.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
	{
		return nullptr;
	}
	const FString ClassPath = UGameMapsSettings::GetGameModeForName(Name);
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (GameModeClass == nullptr)
	{
		UE_LOG(LogLevel, Warning, "LeonLevelFormat: game mode '%s' is not a game mode class; the project's is used",
			*Name);
	}
	return GameModeClass;
}

bool ApplyLevelDocument(
	UWorld& InWorld, FResourceCache& Resources, const FLevelDocument& Doc, const FString& SourcePath)
{
	UWorld* World = &InWorld;
	if (World->PersistentLevel == nullptr)
	{
		UE_LOG(LogLevel, Error, "LeonLevelFormat: no level to load '%s' into", *SourcePath);
		return false;
	}
	ULevel& Level = *World->PersistentLevel;

	// Every resource first: a failure leaves the current level untouched (no partial loads).
	TArray<FResolvedActorRecord> Resolved;
	Resolved.Reserve(Doc.Actors.Num());
	int32 FailedMeshes = 0;
	for (const FLevelActorRecord& Record : Doc.Actors)
	{
		FResolvedActorRecord Entry;
		if (!ResolveActorRecord(Record, Resources, SourcePath, Entry))
		{
			++FailedMeshes;
			continue;
		}
		Resolved.Add(MoveTemp(Entry));
	}
	if (FailedMeshes > 0)
	{
		UE_LOG(LogLevel, Error, "LeonLevelFormat: aborting '%s' (%d mesh failure(s); refusing partial load)",
			*SourcePath, FailedMeshes);
		return false;
	}

	// The previous level content goes (the gameplay actors stay, as before levels were actors).
	const TArray<AActor*> Previous = Level.Actors;
	for (AActor* Actor : Previous)
	{
		if (Actor != nullptr && !Actor->IsPendingKillPending() && IsLevelContentActor(*Actor))
		{
			Actor->Destroy();
		}
	}
	Level.SetWorldSettings(nullptr);

	// The world settings come first (UE spawns them as the level's first actor).
	AWorldSettings* WorldSettings = World->SpawnActor<AWorldSettings>(AWorldSettings::StaticClass());
	Level.SetWorldSettings(WorldSettings);
	ULegacyLevelDataComponent& LevelData = AddLegacyData(*WorldSettings, ELevelActorClass::StaticMesh);
	LevelData.LevelName = Doc.Name;
	LevelData.GameModeName = Doc.GameMode;
	// The level's game mode (plan decision D18); the saver writes the string back.
	WorldSettings->DefaultGameMode = ResolveLegacyLevelGameMode(Doc.GameMode);

	for (const FResolvedActorRecord& Entry : Resolved)
	{
		SpawnRecordActor(*World, Entry);
	}
	SpawnDocumentLights(*World, Doc);

	// The camera framing, kept on the world settings (UEngine::LoadMap starts the player there).
	LevelData.CameraMode = Doc.Camera.Mode;
	LevelData.CameraTarget = FLegacyCoordinateConversion::ConvertPosition(Doc.Camera.Target);
	LevelData.CameraDistance = FLegacyCoordinateConversion::ConvertLength(Doc.Camera.Distance);
	// Legacy yaw / pitch meant the eye's offset from the target in Orbit and the look direction in FreeLook.
	LevelData.CameraViewRotation = Doc.Camera.Mode == ECameraMode::FreeLook
		? FLegacyCoordinateConversion::ConvertFreeLookRotation(Doc.Camera.Yaw, Doc.Camera.Pitch)
		: FLegacyCoordinateConversion::ConvertOrbitViewRotation(Doc.Camera.Yaw, Doc.Camera.Pitch);
	LevelData.CameraEye = FLegacyCoordinateConversion::ConvertPosition(Doc.Camera.Eye);

	int32 NumStaticMeshes = 0;
	for (const AActor* Actor : Level.Actors)
	{
		// The legacy count: the placed meshes and the blocking volumes' cubes.
		if (Actor != nullptr && !Actor->IsPendingKillPending() &&
			(Actor->IsA<AStaticMeshActor>() || Actor->IsA<ABlockingVolume>()))
		{
			++NumStaticMeshes;
		}
	}
	const FString& Label = Doc.Name.IsEmpty() ? SourcePath : Doc.Name;
	UE_LOG(LogLevel, Log, "LevelLoader: loaded '%s' (%d actors)", *Label, NumStaticMeshes);
	// A level (re)load is a garbage collection safe point (plan decision D11): the replaced actors go now.
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return true;
}
