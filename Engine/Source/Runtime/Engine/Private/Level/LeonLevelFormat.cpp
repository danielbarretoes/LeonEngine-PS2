#include "Level/LeonLevelFormat.h"

#include "Engine/GameEngine.h"
#include "EngineLogs.h"
#include "LegacyCoordinateConversion.h"
#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Level/LevelLoader.h"
#include "Level/Light.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

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

	[[nodiscard]] ELevelActorClass ActorClassFromEditorClass(const FString& EditorClass)
	{
		if (EditorClass.Equals("Cube", ESearchCase::CaseSensitive))
		{
			return ELevelActorClass::Cube;
		}
		if (EditorClass.Equals("Sphere", ESearchCase::CaseSensitive))
		{
			return ELevelActorClass::Sphere;
		}
		if (EditorClass.Equals("Plane", ESearchCase::CaseSensitive))
		{
			return ELevelActorClass::Plane;
		}
		if (EditorClass.Equals("BlockingVolume", ESearchCase::CaseSensitive))
		{
			return ELevelActorClass::BlockingVolume;
		}
		if (EditorClass.Equals("TriggerVolume", ESearchCase::CaseSensitive))
		{
			return ELevelActorClass::TriggerVolume;
		}
		if (EditorClass.Equals("PainCausingVolume", ESearchCase::CaseSensitive))
		{
			return ELevelActorClass::PainCausingVolume;
		}
		if (EditorClass.Equals("AISpawnPoint", ESearchCase::CaseSensitive))
		{
			return ELevelActorClass::AISpawnPoint;
		}
		if (EditorClass.Equals("PlayerStart", ESearchCase::CaseSensitive))
		{
			return ELevelActorClass::PlayerStart;
		}
		return ELevelActorClass::StaticMesh;
	}

	[[nodiscard]] const TCHAR* EditorClassFromActorClass(ELevelActorClass InActorClass)
	{
		switch (InActorClass)
		{
			case ELevelActorClass::Cube:
				return "Cube";
			case ELevelActorClass::Sphere:
				return "Sphere";
			case ELevelActorClass::Plane:
				return "Plane";
			case ELevelActorClass::BlockingVolume:
				return "BlockingVolume";
			case ELevelActorClass::TriggerVolume:
				return "TriggerVolume";
			case ELevelActorClass::PainCausingVolume:
				return "PainCausingVolume";
			case ELevelActorClass::AISpawnPoint:
				return "AISpawnPoint";
			case ELevelActorClass::PlayerStart:
				return "PlayerStart";
			case ELevelActorClass::StaticMesh:
				break;
		}
		return "StaticMesh";
	}

	/**
	 * Basic shape backing a stored actor class; false for FPlayerStart / FAISpawnPoint / UStaticMesh.
	 * FTriggerVolume / FPainCausingVolume map to Cube (editor debug mesh); runtime apply uses PODs only.
	 */
	[[nodiscard]] bool BasicShapeForActorClass(ELevelActorClass InActorClass, EBasicShape& OutShape)
	{
		switch (InActorClass)
		{
			case ELevelActorClass::Cube:
			case ELevelActorClass::BlockingVolume:
			case ELevelActorClass::TriggerVolume:
			case ELevelActorClass::PainCausingVolume:
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

	/** Writes a world transform into a record's legacy position, XYZ Euler degrees and scale. */
	void SetLegacyTransform(FLevelActorRecord& Record, const FTransform& Transform)
	{
		Record.Position = FLegacyCoordinateConversion::ToLegacyPosition(Transform.GetLocation());
		Record.RotationDegrees = FLegacyCoordinateConversion::ToLegacyEulerXYZ(Transform.GetRotation());
		Record.Scale = FLegacyCoordinateConversion::ToLegacyScale(Transform.GetScale3D());
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

	void ApplyDocumentLights(const FLevelDocument& Doc, ULevel& Staged)
	{
		for (const FLevelLightRecord& Record : Doc.Lights)
		{
			FBasicLight Light;
			Light.Type =
				Record.LightClass == ELevelLightClass::PointLight ? EBasicLight::Point : EBasicLight::Directional;
			// Legacy lights keep pitch in X and yaw in Y; the roll in Z was never used.
			Light.Transform = FTransform(
				FLegacyCoordinateConversion::ConvertLightRotation(Record.RotationDegrees.X, Record.RotationDegrees.Y),
				FLegacyCoordinateConversion::ConvertPosition(Record.Position));
			Light.LightColor = Record.LightColor;
			Light.Intensity = Record.Intensity;
			Light.bCastShadows = Record.bCastShadows;
			Light.SourceAngle = Record.SourceAngle;
			Light.Range = Record.Range;

			if (Light.Type != EBasicLight::Point)
			{
				Light.AddTo(Staged);
				continue;
			}

			const int32 LightIndex = Staged.GetPointLights().Num();
			Light.AddTo(Staged);
			if (!Record.bHasOrbit || !Staged.GetPointLights().IsValidIndex(LightIndex))
			{
				continue;
			}
			FPointLight& Live = Staged.GetPointLights()[LightIndex];
			Live.bHasOrbit = true;
			Live.OrbitRadius = Record.OrbitRadius;
			Live.OrbitHeight = Record.OrbitHeight;
			Live.OrbitHeightAmp = Record.OrbitHeightAmp;
			Live.OrbitSpeed = Record.OrbitSpeed;
		}

		if (Staged.GetDirectionalLights().Num() == 0)
		{
			FBasicLight::Directional().AddTo(Staged);
		}
		if (Staged.GetDirectionalLights().Num() > MaxDirectionalLights)
		{
			UE_LOG(LogLevel, Warning, "LeonLevelFormat: truncating directional lights from %d to %d",
				Staged.GetDirectionalLights().Num(), MaxDirectionalLights);
			Staged.GetDirectionalLights().SetNum(MaxDirectionalLights);
		}
		if (Staged.GetPointLights().Num() > MaxPointLights)
		{
			UE_LOG(LogLevel, Warning, "LeonLevelFormat: truncating point lights from %d to %d",
				Staged.GetPointLights().Num(), MaxPointLights);
			Staged.GetPointLights().SetNum(MaxPointLights);
		}
	}

} // namespace

FLevelDocument BuildLevelDocument(const ULevel& Level, const UCameraComponent& InCamera)
{
	FLevelDocument Doc;
	Doc.Name = Level.GetName();
	Doc.GameMode = Level.GetGameMode();

	Doc.Camera.Mode = InCamera.GetMode();
	Doc.Camera.Target = InCamera.GetTarget();
	Doc.Camera.Eye = InCamera.EyeLocation();
	Doc.Camera.Distance = InCamera.GetDistance();
	Doc.Camera.Yaw = InCamera.GetYawDegrees();
	Doc.Camera.Pitch = InCamera.GetPitchDegrees();

	for (const FPlayerStart& Start : Level.GetPlayerStarts())
	{
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::PlayerStart;
		SetLegacyTransform(Record, Start.Transform);
		Record.bEnableGravity = false;
		Doc.Actors.Add(MoveTemp(Record));
	}

	for (const FAISpawnPoint& Spawn : Level.AISpawnPoints())
	{
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::AISpawnPoint;
		SetLegacyTransform(Record, Spawn.Transform);
		Record.Tag = Spawn.Tag;
		Record.bEnableGravity = false;
		Doc.Actors.Add(MoveTemp(Record));
	}

	for (const FTriggerVolume& Volume : Level.GetTriggerVolumes())
	{
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::TriggerVolume;
		SetLegacyTransform(Record, Volume.Transform);
		Record.Tag = Volume.Tag;
		Record.InteractCost = Volume.InteractCost;
		Record.InteractRadius = Volume.InteractRadius;
		Record.Payload = Volume.Payload;
		Record.bConsumeOnUse = Volume.bConsumeOnUse;
		Record.bEnableGravity = false;
		Doc.Actors.Add(MoveTemp(Record));
	}

	for (const FPainCausingVolume& Volume : Level.GetPainCausingVolumes())
	{
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::PainCausingVolume;
		SetLegacyTransform(Record, Volume.Transform);
		Record.Tag = Volume.Tag;
		Record.DamagePerSecond = Volume.DamagePerSecond;
		Record.DamageInterval = Volume.DamageInterval;
		Record.bEnableGravity = false;
		Doc.Actors.Add(MoveTemp(Record));
	}

	for (const UStaticMeshComponent& LocalMesh : Level.GetStaticMeshes())
	{
		FLevelActorRecord Record;
		Record.ActorClass = ActorClassFromEditorClass(LocalMesh.EditorClass);
		// Imported meshes are only reloadable through their path.
		if (Record.ActorClass == ELevelActorClass::StaticMesh && LocalMesh.MeshPath.IsEmpty())
		{
			UE_LOG(LogLevel, Warning, "LeonLevelFormat: skipping StaticMesh actor without mesh path");
			continue;
		}

		Record.Mobility = LocalMesh.Mobility;
		Record.bCollisionEnabled = LocalMesh.bCollisionEnabled;
		Record.bSimulatePhysics = LocalMesh.bSimulatePhysics;
		Record.bEnableGravity = LocalMesh.bEnableGravity;
		Record.bHidden = LocalMesh.bHidden;

		SetLegacyTransform(Record, LocalMesh.Transform);

		Record.Tag = LocalMesh.Tag;
		Record.MaterialPath = LocalMesh.MaterialPath;
		if (Record.ActorClass == ELevelActorClass::StaticMesh)
		{
			Record.MeshPath = LocalMesh.MeshPath;
		}

		Record.SphereSegments = LocalMesh.SphereSegments;
		Record.SphereRings = LocalMesh.SphereRings;

		Record.bHasSpinYaw = LocalMesh.SpinYaw != 0.0f;
		Record.SpinYaw = LocalMesh.SpinYaw;

		Record.bHasBob = LocalMesh.bHasBob;
		Record.BobBaseY = LocalMesh.BobBaseY;
		Record.BobAmplitude = LocalMesh.BobAmplitude;
		Record.BobSpeed = LocalMesh.BobSpeed;

		Doc.Actors.Add(MoveTemp(Record));
	}

	for (const FDirectionalLight& Light : Level.GetDirectionalLights())
	{
		FLevelLightRecord Record;
		Record.LightClass = ELevelLightClass::DirectionalLight;
		Record.bCastShadows = Light.bCastShadows;
		SetLegacyLightTransform(Record, Light.Transform);
		Record.LightColor = Light.LightColor;
		Record.Intensity = Light.Intensity;
		Record.SourceAngle = Light.SourceAngle;
		Doc.Lights.Add(Record);
	}
	for (const FPointLight& Light : Level.GetPointLights())
	{
		FLevelLightRecord Record;
		Record.LightClass = ELevelLightClass::PointLight;
		Record.bCastShadows = Light.bCastShadows;
		SetLegacyLightTransform(Record, Light.Transform);
		Record.LightColor = Light.LightColor;
		Record.Intensity = Light.Intensity;
		Record.Range = Light.Range;
		Record.bHasOrbit = Light.bHasOrbit;
		Record.OrbitRadius = Light.OrbitRadius;
		Record.OrbitHeight = Light.OrbitHeight;
		Record.OrbitHeightAmp = Light.OrbitHeightAmp;
		Record.OrbitSpeed = Light.OrbitSpeed;
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

bool ApplyLevelDocument(UGameEngine& Engine, const FLevelDocument& Doc, const FString& SourcePath)
{
	ULevel Staged;
	Staged.Clear();
	FResourceCache& Resources = Engine.GetResources();

	Staged.SetName(Doc.Name);
	Staged.SetGameMode(Doc.GameMode);

	int32 FailedMeshes = 0;
	for (const FLevelActorRecord& Record : Doc.Actors)
	{
		const FTransform Transform =
			FLegacyCoordinateConversion::ConvertTransform(Record.Position, Record.RotationDegrees, Record.Scale);

		if (Record.ActorClass == ELevelActorClass::PlayerStart)
		{
			FPlayerStart Start;
			Start.Transform = Transform;
			Staged.AddPlayerStart(Start);
			continue;
		}
		if (Record.ActorClass == ELevelActorClass::AISpawnPoint)
		{
			FAISpawnPoint Spawn;
			Spawn.Transform = Transform;
			Spawn.Tag = Record.Tag;
			Staged.AddAISpawnPoint(MoveTemp(Spawn));
			continue;
		}
		if (Record.ActorClass == ELevelActorClass::TriggerVolume)
		{
			FTriggerVolume Volume;
			Volume.Transform = Transform;
			Volume.InteractRadius = Record.InteractRadius;
			Volume.InteractCost = Record.InteractCost;
			Volume.Payload = Record.Payload;
			Volume.Tag = Record.Tag;
			Volume.bConsumeOnUse = Record.bConsumeOnUse;
			Staged.AddTriggerVolume(MoveTemp(Volume));
			continue;
		}
		if (Record.ActorClass == ELevelActorClass::PainCausingVolume)
		{
			FPainCausingVolume Volume;
			Volume.Transform = Transform;
			Volume.DamagePerSecond = Record.DamagePerSecond;
			Volume.DamageInterval = Record.DamageInterval;
			Volume.Tag = Record.Tag;
			Staged.AddPainCausingVolume(MoveTemp(Volume));
			continue;
		}

		UStaticMeshComponent Actor;
		EBasicShape ShapeType{};
		const bool bIsBasicShape = BasicShapeForActorClass(Record.ActorClass, ShapeType);
		if (bIsBasicShape)
		{
			FBasicShape Shape;
			Shape.Type = ShapeType;
			Shape.Transform = Transform;
			Shape.SphereSegments = Record.SphereSegments;
			Shape.SphereRings = Record.SphereRings;
			Actor = Shape.MakeStaticMesh(Resources);
			Actor.SphereSegments = Record.SphereSegments;
			Actor.SphereRings = Record.SphereRings;
		}
		else
		{
			Actor.Mesh = Resources.LoadStaticMesh(ResolveLevelAssetPath(SourcePath, Record.MeshPath));
			Actor.Transform = Transform;
			Actor.MeshPath = Record.MeshPath;
		}
		Actor.EditorClass = EditorClassFromActorClass(Record.ActorClass);

		if (Actor.Mesh == nullptr)
		{
			UE_LOG(LogLevel, Error, "LeonLevelFormat: failed mesh for actor in %s", *SourcePath);
			++FailedMeshes;
			continue;
		}

		if (Record.bHasFitHeight && Record.FitHeight > 0.0f)
		{
			ApplyFitHeight(Actor, Record.FitHeight);
		}

		Actor.Tag = Record.Tag;
		Actor.bSimulatePhysics = Record.bSimulatePhysics;
		Actor.bCollisionEnabled = Record.bCollisionEnabled || Record.bSimulatePhysics;
		Actor.bEnableGravity = Record.bEnableGravity;
		Actor.bHidden = Record.bHidden;
		Actor.Mobility = Record.Mobility;
		Actor.SpinYaw = Record.bHasSpinYaw ? Record.SpinYaw : 0.0f;
		Actor.MaterialPath = Record.MaterialPath;

		if (!Record.MaterialPath.IsEmpty())
		{
			FMaterial Base = Resources.LoadMaterial(ResolveLevelAssetPath(SourcePath, Record.MaterialPath));
			if (Actor.Mesh->HasMaterials())
			{
				Actor.Materials.Init(Base, Actor.Mesh->GetMaterials().Num());
			}
			else
			{
				Actor.bMaterialOverride = true;
				Actor.Material = MoveTemp(Base);
			}
		}
		else if (!Actor.Mesh->HasMaterials())
		{
			Actor.bMaterialOverride = true;
			Actor.Material = Resources.DefaultMaterial();
		}

		// BlockingVolume: invisible collision box, never a shadow caster.
		if (Record.ActorClass == ELevelActorClass::BlockingVolume)
		{
			Actor.Material.bCastsShadows = false;
			for (FMaterial& LocalMaterial : Actor.Materials)
			{
				LocalMaterial.bCastsShadows = false;
			}
		}

		const int32 ActorIndex = Staged.GetStaticMeshes().Num();
		Staged.AddStaticMesh(MoveTemp(Actor));

		if (Record.bHasBob)
		{
			UStaticMeshComponent& Live = Staged.GetStaticMeshes()[ActorIndex];
			Live.bHasBob = true;
			Live.BobBaseY = Record.BobBaseY;
			Live.BobAmplitude = Record.BobAmplitude;
			Live.BobSpeed = Record.BobSpeed;
		}
	}

	if (FailedMeshes > 0)
	{
		UE_LOG(LogLevel, Error, "LeonLevelFormat: aborting '%s' (%d mesh failure(s); refusing partial load)",
			*SourcePath, FailedMeshes);
		return false;
	}

	ApplyDocumentLights(Doc, Staged);

	// Blank / lights-only levels are valid (editor New Level → Blank).
	if (Staged.GetStaticMeshes().Num() == 0 && Staged.GetPlayerStarts().Num() == 0 &&
		Staged.GetTriggerVolumes().Num() == 0 && Staged.GetPainCausingVolumes().Num() == 0 &&
		Staged.AISpawnPoints().Num() == 0 && Staged.GetDirectionalLights().Num() == 0 &&
		Staged.GetPointLights().Num() == 0)
	{
		UE_LOG(LogLevel, Error, "LeonLevelFormat: completely empty level in %s", *SourcePath);
		return false;
	}

	Engine.GetLevel() = MoveTemp(Staged);

	UCameraComponent& LocalCamera = Engine.GetCamera();
	LocalCamera.SetTarget(Doc.Camera.Target);
	LocalCamera.SetDistance(Doc.Camera.Distance);
	LocalCamera.SetYawPitch(Doc.Camera.Yaw, Doc.Camera.Pitch);
	LocalCamera.SetEyeLocation(Doc.Camera.Eye);
	LocalCamera.SetMode(Doc.Camera.Mode);

	const FString& Label = Doc.Name.IsEmpty() ? SourcePath : Doc.Name;
	UE_LOG(LogLevel, Log, "LevelLoader: loaded '%s' (%d actors)", *Label, Engine.GetLevel().GetStaticMeshes().Num());
	return true;
}
