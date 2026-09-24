#include "Level/LeonLevelFormat.h"

#include "Engine/GameEngine.h"
#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Level/LevelLoader.h"
#include "Level/Light.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <utility>

namespace
{
	namespace fs = std::filesystem;

	// Caps on file-controlled allocations (corrupt / hostile .llev).
	constexpr std::uint32_t MaxLeonLevelStrings = 65536u;
	constexpr std::uint32_t MaxLeonLevelActors = 100000u;
	constexpr std::uint32_t MaxLeonLevelLights = 16384u;
	constexpr std::uint32_t MaxLeonStringBytes = 1u << 20; // 1 MiB per string

	// --- Little-endian primitive writers ---

	void WriteU8(std::vector<std::uint8_t>& Out, std::uint8_t Value)
	{
		Out.push_back(Value);
	}

	void WriteU16(std::vector<std::uint8_t>& Out, std::uint16_t Value)
	{
		Out.push_back(static_cast<std::uint8_t>(Value & 0xFFu));
		Out.push_back(static_cast<std::uint8_t>((Value >> 8) & 0xFFu));
	}

	void WriteU32(std::vector<std::uint8_t>& Out, std::uint32_t Value)
	{
		Out.push_back(static_cast<std::uint8_t>(Value & 0xFFu));
		Out.push_back(static_cast<std::uint8_t>((Value >> 8) & 0xFFu));
		Out.push_back(static_cast<std::uint8_t>((Value >> 16) & 0xFFu));
		Out.push_back(static_cast<std::uint8_t>((Value >> 24) & 0xFFu));
	}

	void WriteI32(std::vector<std::uint8_t>& Out, std::int32_t Value)
	{
		WriteU32(Out, static_cast<std::uint32_t>(Value));
	}

	void WriteF32(std::vector<std::uint8_t>& Out, float Value)
	{
		std::uint32_t Bits = 0;
		static_assert(sizeof(Bits) == sizeof(Value), "float must be 32-bit");
		std::memcpy(&Bits, &Value, sizeof(Bits));
		WriteU32(Out, Bits);
	}

	void WriteVec3(std::vector<std::uint8_t>& Out, const glm::vec3& Value)
	{
		WriteF32(Out, Value.x);
		WriteF32(Out, Value.y);
		WriteF32(Out, Value.z);
	}

	/// Deduplicating string table; index 0 is always the empty string.
	class FStringTableBuilder
	{
	public:
		FStringTableBuilder()
		{
			(void)Add(std::string{});
		}

		std::uint32_t Add(const std::string& Value)
		{
			const auto It = Lookup.find(Value);
			if (It != Lookup.end())
			{
				return It->second;
			}
			const auto Index = static_cast<std::uint32_t>(Strings.size());
			Strings.push_back(Value);
			Lookup.emplace(Value, Index);
			return Index;
		}

		void WriteTo(std::vector<std::uint8_t>& Out) const
		{
			WriteU32(Out, static_cast<std::uint32_t>(Strings.size()));
			for (const std::string& Value : Strings)
			{
				WriteU32(Out, static_cast<std::uint32_t>(Value.size()));
				Out.insert(Out.end(), Value.begin(), Value.end());
			}
		}

	private:
		std::vector<std::string> Strings;
		std::unordered_map<std::string, std::uint32_t> Lookup;
	};

	/// Bounds-checked little-endian cursor; any overrun latches `bFailed`.
	class FByteReader
	{
	public:
		explicit FByteReader(const std::vector<std::uint8_t>& InBytes)
			: Bytes(InBytes.data())
			, Size(InBytes.size())
		{
		}

		[[nodiscard]] bool HasFailed() const
		{
			return bFailed;
		}

		std::uint8_t ReadU8()
		{
			if (!Require(1))
			{
				return 0;
			}
			return Bytes[Cursor++];
		}

		std::uint16_t ReadU16()
		{
			if (!Require(2))
			{
				return 0;
			}
			const auto Value = static_cast<std::uint16_t>(Bytes[Cursor] | (Bytes[Cursor + 1] << 8));
			Cursor += 2;
			return Value;
		}

		std::uint32_t ReadU32()
		{
			if (!Require(4))
			{
				return 0;
			}
			const std::uint32_t Value = static_cast<std::uint32_t>(Bytes[Cursor]) |
				(static_cast<std::uint32_t>(Bytes[Cursor + 1]) << 8) |
				(static_cast<std::uint32_t>(Bytes[Cursor + 2]) << 16) |
				(static_cast<std::uint32_t>(Bytes[Cursor + 3]) << 24);
			Cursor += 4;
			return Value;
		}

		std::int32_t ReadI32()
		{
			return static_cast<std::int32_t>(ReadU32());
		}

		float ReadF32()
		{
			const std::uint32_t Bits = ReadU32();
			float Value = 0.0f;
			std::memcpy(&Value, &Bits, sizeof(Value));
			return Value;
		}

		glm::vec3 ReadVec3()
		{
			glm::vec3 Value{0.0f};
			Value.x = ReadF32();
			Value.y = ReadF32();
			Value.z = ReadF32();
			return Value;
		}

		std::string ReadBytes(std::size_t Count)
		{
			if (!Require(Count))
			{
				return {};
			}
			std::string Value(reinterpret_cast<const char*>(Bytes + Cursor), Count);
			Cursor += Count;
			return Value;
		}

		void Skip(std::size_t Count)
		{
			if (Require(Count))
			{
				Cursor += Count;
			}
		}

	private:
		bool Require(std::size_t Count)
		{
			if (bFailed || Cursor + Count > Size)
			{
				bFailed = true;
				return false;
			}
			return true;
		}

		const std::uint8_t* Bytes = nullptr;
		std::size_t Size = 0;
		std::size_t Cursor = 0;
		bool bFailed = false;
	};

} // namespace

std::string ResolveLevelAssetPath(const std::string& LevelPath, const std::string& RelativeOrKey)
{
	namespace fs = std::filesystem;
	std::error_code Ec;
	if (RelativeOrKey.empty())
	{
		return {};
	}
	const fs::path Key(RelativeOrKey);
	if (Key.is_absolute() && fs::exists(Key, Ec) && !Ec)
	{
		return Key.lexically_normal().string();
	}

	// `<pack>/Content/Levels/Main.llev` → content root `<pack>/Content`
	// (legacy `<pack>/Levels/Main.llev` → `<pack>/`).
	const fs::path ContentRoot = fs::path(LevelPath).parent_path().parent_path();
	const fs::path InPack = (ContentRoot / Key).lexically_normal();
	if (fs::exists(InPack, Ec) && !Ec)
	{
		return InPack.string();
	}

	// Legacy `Projects/<name>/Materials/...` after the pack was copied elsewhere.
	const auto MatPos = RelativeOrKey.find("Materials/");
	if (MatPos != std::string::npos)
	{
		const fs::path Legacy = (ContentRoot / RelativeOrKey.substr(MatPos)).lexically_normal();
		if (fs::exists(Legacy, Ec) && !Ec)
		{
			return Legacy.string();
		}
	}

	return FPaths::ResolveAssetPath(RelativeOrKey);
}

namespace
{

	[[nodiscard]] ELevelActorClass ActorClassFromEditorClass(const std::string& EditorClass)
	{
		if (EditorClass == "Cube")
		{
			return ELevelActorClass::Cube;
		}
		if (EditorClass == "Sphere")
		{
			return ELevelActorClass::Sphere;
		}
		if (EditorClass == "Plane")
		{
			return ELevelActorClass::Plane;
		}
		if (EditorClass == "BlockingVolume")
		{
			return ELevelActorClass::BlockingVolume;
		}
		if (EditorClass == "TriggerVolume")
		{
			return ELevelActorClass::TriggerVolume;
		}
		if (EditorClass == "PainCausingVolume")
		{
			return ELevelActorClass::PainCausingVolume;
		}
		if (EditorClass == "AISpawnPoint")
		{
			return ELevelActorClass::AISpawnPoint;
		}
		if (EditorClass == "PlayerStart")
		{
			return ELevelActorClass::PlayerStart;
		}
		return ELevelActorClass::StaticMesh;
	}

	[[nodiscard]] const char* EditorClassFromActorClass(ELevelActorClass InActorClass)
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

	/// Basic shape backing a stored actor class; false for FPlayerStart / FAISpawnPoint / UStaticMesh.
	/// FTriggerVolume / FPainCausingVolume map to Cube (editor debug mesh); runtime apply uses PODs only.
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

	void ApplyDocumentLights(const FLevelDocument& Doc, ULevel& Staged)
	{
		for (const FLevelLightRecord& Record : Doc.Lights)
		{
			FBasicLight Light;
			Light.Type =
				Record.LightClass == ELevelLightClass::PointLight ? EBasicLight::Point : EBasicLight::Directional;
			Light.Transform.Position = Record.Position;
			Light.Transform.RotationDegrees = Record.RotationDegrees;
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

			const std::size_t LightIndex = Staged.GetPointLights().size();
			Light.AddTo(Staged);
			if (!Record.bHasOrbit || LightIndex >= Staged.GetPointLights().size())
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

		if (Staged.GetDirectionalLights().empty())
		{
			FBasicLight::Directional().AddTo(Staged);
		}
		if (Staged.GetDirectionalLights().size() > static_cast<std::size_t>(MaxDirectionalLights))
		{
			std::cerr << "LeonLevelFormat: truncating directional lights from " << Staged.GetDirectionalLights().size()
					  << " to " << MaxDirectionalLights << '\n';
			Staged.GetDirectionalLights().resize(static_cast<std::size_t>(MaxDirectionalLights));
		}
		if (Staged.GetPointLights().size() > static_cast<std::size_t>(MaxPointLights))
		{
			std::cerr << "LeonLevelFormat: truncating point lights from " << Staged.GetPointLights().size() << " to "
					  << MaxPointLights << '\n';
			Staged.GetPointLights().resize(static_cast<std::size_t>(MaxPointLights));
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
		Record.Position = Start.Transform.Position;
		Record.RotationDegrees = Start.Transform.RotationDegrees;
		Record.Scale = Start.Transform.Scale;
		Record.bEnableGravity = false;
		Doc.Actors.push_back(std::move(Record));
	}

	for (const FAISpawnPoint& Spawn : Level.AISpawnPoints())
	{
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::AISpawnPoint;
		Record.Position = Spawn.Transform.Position;
		Record.RotationDegrees = Spawn.Transform.RotationDegrees;
		Record.Scale = Spawn.Transform.Scale;
		Record.Tag = Spawn.Tag;
		Record.bEnableGravity = false;
		Doc.Actors.push_back(std::move(Record));
	}

	for (const FTriggerVolume& Volume : Level.GetTriggerVolumes())
	{
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::TriggerVolume;
		Record.Position = Volume.Transform.Position;
		Record.RotationDegrees = Volume.Transform.RotationDegrees;
		Record.Scale = Volume.Transform.Scale;
		Record.Tag = Volume.Tag;
		Record.InteractCost = Volume.InteractCost;
		Record.InteractRadius = Volume.InteractRadius;
		Record.Payload = Volume.Payload;
		Record.bConsumeOnUse = Volume.bConsumeOnUse;
		Record.bEnableGravity = false;
		Doc.Actors.push_back(std::move(Record));
	}

	for (const FPainCausingVolume& Volume : Level.GetPainCausingVolumes())
	{
		FLevelActorRecord Record;
		Record.ActorClass = ELevelActorClass::PainCausingVolume;
		Record.Position = Volume.Transform.Position;
		Record.RotationDegrees = Volume.Transform.RotationDegrees;
		Record.Scale = Volume.Transform.Scale;
		Record.Tag = Volume.Tag;
		Record.DamagePerSecond = Volume.DamagePerSecond;
		Record.DamageInterval = Volume.DamageInterval;
		Record.bEnableGravity = false;
		Doc.Actors.push_back(std::move(Record));
	}

	for (const UStaticMeshComponent& LocalMesh : Level.GetStaticMeshes())
	{
		FLevelActorRecord Record;
		Record.ActorClass = ActorClassFromEditorClass(LocalMesh.EditorClass);
		// Imported meshes are only reloadable through their path.
		if (Record.ActorClass == ELevelActorClass::StaticMesh && LocalMesh.MeshPath.empty())
		{
			std::cerr << "LeonLevelFormat: skipping StaticMesh actor without mesh path\n";
			continue;
		}

		Record.Mobility = LocalMesh.Mobility;
		Record.bCollisionEnabled = LocalMesh.bCollisionEnabled;
		Record.bSimulatePhysics = LocalMesh.bSimulatePhysics;
		Record.bEnableGravity = LocalMesh.bEnableGravity;
		Record.bHidden = LocalMesh.bHidden;

		Record.Position = LocalMesh.Transform.Position;
		Record.RotationDegrees = LocalMesh.Transform.RotationDegrees;
		Record.Scale = LocalMesh.Transform.Scale;

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

		Doc.Actors.push_back(std::move(Record));
	}

	for (const FDirectionalLight& Light : Level.GetDirectionalLights())
	{
		FLevelLightRecord Record;
		Record.LightClass = ELevelLightClass::DirectionalLight;
		Record.bCastShadows = Light.bCastShadows;
		Record.Position = Light.Transform.Position;
		Record.RotationDegrees = Light.Transform.RotationDegrees;
		Record.LightColor = Light.LightColor;
		Record.Intensity = Light.Intensity;
		Record.SourceAngle = Light.SourceAngle;
		Doc.Lights.push_back(Record);
	}
	for (const FPointLight& Light : Level.GetPointLights())
	{
		FLevelLightRecord Record;
		Record.LightClass = ELevelLightClass::PointLight;
		Record.bCastShadows = Light.bCastShadows;
		Record.Position = Light.Transform.Position;
		Record.RotationDegrees = Light.Transform.RotationDegrees;
		Record.LightColor = Light.LightColor;
		Record.Intensity = Light.Intensity;
		Record.Range = Light.Range;
		Record.bHasOrbit = Light.bHasOrbit;
		Record.OrbitRadius = Light.OrbitRadius;
		Record.OrbitHeight = Light.OrbitHeight;
		Record.OrbitHeightAmp = Light.OrbitHeightAmp;
		Record.OrbitSpeed = Light.OrbitSpeed;
		Doc.Lights.push_back(Record);
	}

	return Doc;
}

std::vector<std::uint8_t> SerializeLeonLevel(const FLevelDocument& Doc)
{
	FStringTableBuilder LocalStrings;
	const std::uint32_t NameIdx = LocalStrings.Add(Doc.Name);
	const std::uint32_t GameModeIdx = LocalStrings.Add(Doc.GameMode);
	const std::uint32_t EnvironmentIdx = LocalStrings.Add(Doc.EnvironmentPath);

	struct FActorIndices
	{
		std::uint32_t Flags = 0;
		std::uint32_t Tag = 0;
		std::uint32_t Material = 0;
		std::uint32_t Mesh = 0;
		std::uint32_t LightmapId = 0;
		std::uint32_t LightmapPath = 0;
		std::uint32_t Payload = 0;
	};
	std::vector<FActorIndices> ActorIndices;
	ActorIndices.reserve(Doc.Actors.size());
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
		if (!Actor.Tag.empty())
		{
			Indices.Flags |= LevelActorFlagHasTag;
			Indices.Tag = LocalStrings.Add(Actor.Tag);
		}
		if (!Actor.MaterialPath.empty())
		{
			Indices.Flags |= LevelActorFlagHasMaterial;
			Indices.Material = LocalStrings.Add(Actor.MaterialPath);
		}
		if (!Actor.MeshPath.empty())
		{
			Indices.Flags |= LevelActorFlagHasMesh;
			Indices.Mesh = LocalStrings.Add(Actor.MeshPath);
		}
		if (!Actor.LightmapId.empty())
		{
			Indices.Flags |= LevelActorFlagHasLightmapId;
			Indices.LightmapId = LocalStrings.Add(Actor.LightmapId);
		}
		if (!Actor.LightmapPath.empty())
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
		if (!Actor.Payload.empty())
		{
			Indices.Flags |= LevelActorFlagHasPayload;
			Indices.Payload = LocalStrings.Add(Actor.Payload);
		}
		if (Actor.bConsumeOnUse)
		{
			Indices.Flags |= LevelActorFlagConsumeOnUse;
		}
		ActorIndices.push_back(Indices);
	}

	std::vector<std::uint8_t> Out;
	WriteU32(Out, LeonLevelMagic);
	WriteU32(Out, LeonLevelVersion);
	WriteU32(Out, 0u); // flags (reserved)

	LocalStrings.WriteTo(Out);

	WriteU32(Out, NameIdx);
	WriteU32(Out, GameModeIdx);
	WriteU32(Out, EnvironmentIdx);
	WriteF32(Out, Doc.EnvironmentExposure);

	WriteU8(Out, static_cast<std::uint8_t>(Doc.Camera.Mode));
	WriteU8(Out, 0u);
	WriteU8(Out, 0u);
	WriteU8(Out, 0u);
	WriteVec3(Out, Doc.Camera.Target);
	WriteVec3(Out, Doc.Camera.Eye);
	WriteF32(Out, Doc.Camera.Distance);
	WriteF32(Out, Doc.Camera.Yaw);
	WriteF32(Out, Doc.Camera.Pitch);

	WriteU32(Out, static_cast<std::uint32_t>(Doc.Actors.size()));
	for (std::size_t I = 0; I < Doc.Actors.size(); ++I)
	{
		const FLevelActorRecord& Actor = Doc.Actors[I];
		const FActorIndices& Indices = ActorIndices[I];

		WriteU8(Out, static_cast<std::uint8_t>(Actor.ActorClass));
		WriteU8(Out, static_cast<std::uint8_t>(Actor.Mobility));
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

	WriteU32(Out, static_cast<std::uint32_t>(Doc.Lights.size()));
	for (const FLevelLightRecord& Light : Doc.Lights)
	{
		std::uint32_t LocalFlags = 0;
		if (Light.bCastShadows)
		{
			LocalFlags |= LevelLightFlagCastShadows;
		}
		if (Light.bHasOrbit)
		{
			LocalFlags |= LevelLightFlagHasOrbit;
		}

		WriteU8(Out, static_cast<std::uint8_t>(Light.LightClass));
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

	return Out;
}

bool DeserializeLeonLevel(const std::vector<std::uint8_t>& InBytes, FLevelDocument& Out)
{
	Out = FLevelDocument{};

	FByteReader Reader(InBytes);
	if (Reader.ReadU32() != LeonLevelMagic)
	{
		std::cerr << "LeonLevelFormat: bad magic (expected 'LLEV')\n";
		return false;
	}
	const std::uint32_t Version = Reader.ReadU32();
	// Flow: .llev load — accept v1 (legacy) and v2 (typed volumes); reject unknown.
	if (Version != 1u && Version != 2u)
	{
		std::cerr << "LeonLevelFormat: unsupported version " << Version << " (expected 1 or 2)\n";
		return false;
	}
	(void)Reader.ReadU32(); // flags (reserved)

	const std::uint32_t StringCount = Reader.ReadU32();
	if (Reader.HasFailed() || StringCount > MaxLeonLevelStrings)
	{
		return false;
	}
	std::vector<std::string> LocalStrings;
	LocalStrings.reserve(StringCount);
	for (std::uint32_t I = 0; I < StringCount; ++I)
	{
		const std::uint32_t Length = Reader.ReadU32();
		if (Reader.HasFailed() || Length > MaxLeonStringBytes)
		{
			return false;
		}
		LocalStrings.push_back(Reader.ReadBytes(Length));
		if (Reader.HasFailed())
		{
			return false;
		}
	}
	// Out-of-range indices resolve to the empty string instead of failing the load.
	const auto StringAt = [&LocalStrings](std::uint32_t Index) -> std::string
	{ return Index < LocalStrings.size() ? LocalStrings[Index] : std::string{}; };

	Out.Name = StringAt(Reader.ReadU32());
	Out.GameMode = StringAt(Reader.ReadU32());
	Out.EnvironmentPath = StringAt(Reader.ReadU32());
	Out.EnvironmentExposure = Reader.ReadF32();

	const std::uint8_t CameraMode = Reader.ReadU8();
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

	const std::uint32_t ActorCount = Reader.ReadU32();
	if (Reader.HasFailed() || ActorCount > MaxLeonLevelActors)
	{
		return false;
	}
	Out.Actors.reserve(ActorCount);
	for (std::uint32_t I = 0; I < ActorCount; ++I)
	{
		FLevelActorRecord Actor;
		const std::uint8_t LocalActorClass = Reader.ReadU8();
		const std::uint8_t LocalMobility = Reader.ReadU8();
		(void)Reader.ReadU16();
		const std::uint32_t LocalFlags = Reader.ReadU32();
		if (Reader.HasFailed())
		{
			return false;
		}
		if (LocalActorClass > static_cast<std::uint8_t>(ELevelActorClass::AISpawnPoint))
		{
			std::cerr << "LeonLevelFormat: unknown actor class " << static_cast<int>(LocalActorClass) << '\n';
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
		Out.Actors.push_back(std::move(Actor));
	}

	const std::uint32_t LightCount = Reader.ReadU32();
	if (Reader.HasFailed() || LightCount > MaxLeonLevelLights)
	{
		return false;
	}
	Out.Lights.reserve(LightCount);
	for (std::uint32_t I = 0; I < LightCount; ++I)
	{
		FLevelLightRecord Light;
		const std::uint8_t LocalLightClass = Reader.ReadU8();
		Reader.Skip(3);
		const std::uint32_t LocalFlags = Reader.ReadU32();
		if (Reader.HasFailed())
		{
			return false;
		}
		if (LocalLightClass > static_cast<std::uint8_t>(ELevelLightClass::PointLight))
		{
			std::cerr << "LeonLevelFormat: unknown light class " << static_cast<int>(LocalLightClass) << '\n';
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
		Out.Lights.push_back(Light);
	}

	return !Reader.HasFailed();
}

bool SaveLeonLevelFile(const std::string& Path, const FLevelDocument& Doc)
{
	const std::vector<std::uint8_t> LocalBytes = SerializeLeonLevel(Doc);
	if (!FFileHelper::WriteFileAtomic(Path, LocalBytes))
	{
		std::cerr << "LeonLevelFormat: cannot write: " << Path << '\n';
		return false;
	}
	return true;
}

bool LoadLeonLevelFile(const std::string& Path, FLevelDocument& Out)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In)
	{
		std::cerr << "LeonLevelFormat: cannot open " << Path << '\n';
		return false;
	}
	const std::vector<std::uint8_t> LocalBytes((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
	if (!DeserializeLeonLevel(LocalBytes, Out))
	{
		std::cerr << "LeonLevelFormat: failed to parse " << Path << '\n';
		return false;
	}
	return true;
}

bool ApplyLevelDocument(UGameEngine& Engine, const FLevelDocument& Doc, const std::string& SourcePath)
{
	ULevel Staged;
	Staged.Clear();
	FResourceCache& Resources = Engine.GetResources();

	try
	{
		Staged.SetName(Doc.Name);
		Staged.SetGameMode(Doc.GameMode);

		int FailedMeshes = 0;
		for (const FLevelActorRecord& Record : Doc.Actors)
		{
			FTransform Transform;
			Transform.Position = Record.Position;
			Transform.RotationDegrees = Record.RotationDegrees;
			Transform.Scale = Record.Scale;

			if (Record.ActorClass == ELevelActorClass::PlayerStart)
			{
				FPlayerStart Start{};
				Start.Transform = Transform;
				Staged.AddPlayerStart(Start);
				continue;
			}
			if (Record.ActorClass == ELevelActorClass::AISpawnPoint)
			{
				FAISpawnPoint Spawn{};
				Spawn.Transform = Transform;
				Spawn.Tag = Record.Tag;
				Staged.AddAISpawnPoint(std::move(Spawn));
				continue;
			}
			if (Record.ActorClass == ELevelActorClass::TriggerVolume)
			{
				FTriggerVolume Volume{};
				Volume.Transform = Transform;
				Volume.InteractRadius = Record.InteractRadius;
				Volume.InteractCost = Record.InteractCost;
				Volume.Payload = Record.Payload;
				Volume.Tag = Record.Tag;
				Volume.bConsumeOnUse = Record.bConsumeOnUse;
				Staged.AddTriggerVolume(std::move(Volume));
				continue;
			}
			if (Record.ActorClass == ELevelActorClass::PainCausingVolume)
			{
				FPainCausingVolume Volume{};
				Volume.Transform = Transform;
				Volume.DamagePerSecond = Record.DamagePerSecond;
				Volume.DamageInterval = Record.DamageInterval;
				Volume.Tag = Record.Tag;
				Staged.AddPainCausingVolume(std::move(Volume));
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
				std::cerr << "LeonLevelFormat: failed mesh for actor in " << SourcePath << '\n';
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

			if (!Record.MaterialPath.empty())
			{
				FMaterial Base = Resources.LoadMaterial(ResolveLevelAssetPath(SourcePath, Record.MaterialPath));
				if (Actor.Mesh->HasMaterials())
				{
					Actor.Materials.assign(Actor.Mesh->GetMaterials().size(), Base);
				}
				else
				{
					Actor.bMaterialOverride = true;
					Actor.Material = std::move(Base);
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

			const std::size_t ActorIndex = Staged.GetStaticMeshes().size();
			Staged.AddStaticMesh(std::move(Actor));

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
			std::cerr << "LeonLevelFormat: aborting '" << SourcePath << "' (" << FailedMeshes
					  << " mesh failure(s); refusing partial load)\n";
			return false;
		}

		ApplyDocumentLights(Doc, Staged);
	}
	catch (const std::exception& Ex)
	{
		std::cerr << "LeonLevelFormat: failed while building " << SourcePath << ": " << Ex.what() << '\n';
		return false;
	}

	// Blank / lights-only levels are valid (editor New Level → Blank).
	if (Staged.GetStaticMeshes().empty() && Staged.GetPlayerStarts().empty() && Staged.GetTriggerVolumes().empty() &&
		Staged.GetPainCausingVolumes().empty() && Staged.AISpawnPoints().empty() &&
		Staged.GetDirectionalLights().empty() && Staged.GetPointLights().empty())
	{
		std::cerr << "LeonLevelFormat: completely empty level in " << SourcePath << '\n';
		return false;
	}

	Engine.GetLevel() = std::move(Staged);

	UCameraComponent& LocalCamera = Engine.GetCamera();
	LocalCamera.SetTarget(Doc.Camera.Target);
	LocalCamera.SetDistance(Doc.Camera.Distance);
	LocalCamera.SetYawPitch(Doc.Camera.Yaw, Doc.Camera.Pitch);
	LocalCamera.SetEyeLocation(Doc.Camera.Eye);
	LocalCamera.SetMode(Doc.Camera.Mode);

	const std::string Label = Doc.Name.empty() ? SourcePath : Doc.Name;
	std::cout << "LevelLoader: loaded '" << Label << "' (" << Engine.GetLevel().GetStaticMeshes().size()
			  << " actors)\n";
	return true;
}
