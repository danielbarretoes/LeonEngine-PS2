#include "Animation/CookedSkeletal.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

	namespace fs = std::filesystem;
	using json = nlohmann::json;

	constexpr std::uint32_t MaxCookedStringBytes = 1u << 20; // 1 MiB
	constexpr std::uint32_t MaxCookedVertices = 2'000'000u;
	constexpr std::uint32_t MaxCookedIndices = 6'000'000u;
	constexpr std::uint32_t MaxCookedBones = 512u;
	constexpr std::uint32_t MaxCookedAnimFrames = 100'000u;

	[[nodiscard]] bool ReadAllBytes(const std::string& Path, std::vector<std::uint8_t>& Out)
	{
		std::ifstream In(Path, std::ios::binary | std::ios::ate);
		if (!In)
		{
			std::cerr << "CookedSkeletal: cannot read '" << Path << "'\n";
			return false;
		}
		const auto End = In.tellg();
		if (End < 0)
		{
			return false;
		}
		Out.resize(static_cast<std::size_t>(End));
		In.seekg(0);
		In.read(reinterpret_cast<char*>(Out.data()), End);
		return static_cast<bool>(In);
	}

	[[nodiscard]] std::string DirOf(const std::string& Path)
	{
		return fs::path(Path).parent_path().string();
	}

	[[nodiscard]] std::string JoinRel(const std::string& BaseDir, const std::string& Rel)
	{
		return (fs::path(BaseDir) / Rel).lexically_normal().string();
	}

	[[nodiscard]] bool WriteString(std::ostream& Out, const std::string& S)
	{
		const auto Len = static_cast<std::uint32_t>(S.size());
		Out.write(reinterpret_cast<const char*>(&Len), sizeof(Len));
		if (!S.empty())
		{
			Out.write(S.data(), static_cast<std::streamsize>(S.size()));
		}
		return static_cast<bool>(Out);
	}

	[[nodiscard]] bool ReadString(const std::uint8_t*& Ptr, const std::uint8_t* End, std::string& Out)
	{
		if (Ptr + sizeof(std::uint32_t) > End)
		{
			return false;
		}
		std::uint32_t Len = 0;
		std::memcpy(&Len, Ptr, sizeof(Len));
		Ptr += sizeof(Len);
		if (Len > MaxCookedStringBytes || Ptr + Len > End)
		{
			return false;
		}
		Out.assign(reinterpret_cast<const char*>(Ptr), Len);
		Ptr += Len;
		return true;
	}

	[[nodiscard]] bool ReadF32(const std::uint8_t*& Ptr, const std::uint8_t* End, float& Out)
	{
		if (Ptr + sizeof(float) > End)
		{
			return false;
		}
		std::memcpy(&Out, Ptr, sizeof(float));
		Ptr += sizeof(float);
		return true;
	}

	[[nodiscard]] bool ReadVec3(const std::uint8_t*& Ptr, const std::uint8_t* End, glm::vec3& Out)
	{
		return ReadF32(Ptr, End, Out.x) && ReadF32(Ptr, End, Out.y) && ReadF32(Ptr, End, Out.z);
	}

	[[nodiscard]] bool WriteSimpleCharacterLmat(
		const std::string& Path, const std::string& MatName, const std::string& BaseColorMapPath = {})
	{
		std::ofstream Out(Path);
		if (!Out)
		{
			return false;
		}
		Out << "# Leon Material (.lmat)\n\n"
			<< "[Info]\n"
			<< "Name=" << MatName << "\n"
			<< "ShadingModel=DefaultLit\n\n"
			<< "[Parameters]\n"
			<< "BaseColor=0.72,0.74,0.78\n"
			<< "Specular=0.04,0.04,0.04\n"
			<< "Metallic=0.05\n"
			<< "Roughness=0.45\n"
			<< "Opacity=1.0\n"
			<< "Shininess=24.0\n"
			<< "UVScale=1.0,1.0\n"
			<< "CastsShadows=true\n"
			<< "PlanarMirror=false\n\n"
			<< "[Textures]\n"
			<< "BaseColorMap=" << BaseColorMapPath << "\n"
			<< "NormalMap=\n";
		return static_cast<bool>(Out);
	}

	void WriteVertex(std::ostream& Out, const FSkeletalVertex& V)
	{
		Out.write(reinterpret_cast<const char*>(&V.Position), sizeof(float) * 3);
		Out.write(reinterpret_cast<const char*>(&V.Normal), sizeof(float) * 3);
		Out.write(reinterpret_cast<const char*>(&V.TexCoord), sizeof(float) * 2);
		Out.write(reinterpret_cast<const char*>(&V.Tangent), sizeof(float) * 4);
		const int Bones[4] = {V.BoneIndices.x, V.BoneIndices.y, V.BoneIndices.z, V.BoneIndices.w};
		Out.write(reinterpret_cast<const char*>(Bones), sizeof(Bones));
		Out.write(reinterpret_cast<const char*>(&V.BoneWeights), sizeof(float) * 4);
	}

	[[nodiscard]] bool ReadVertex(const std::uint8_t*& Ptr, const std::uint8_t* End, FSkeletalVertex& V)
	{
		auto Take = [&](void* Dst, std::size_t N) -> bool
		{
			if (Ptr + N > End)
			{
				return false;
			}
			std::memcpy(Dst, Ptr, N);
			Ptr += N;
			return true;
		};
		int Bones[4]{};
		if (!Take(&V.Position, sizeof(float) * 3) || !Take(&V.Normal, sizeof(float) * 3) ||
			!Take(&V.TexCoord, sizeof(float) * 2) || !Take(&V.Tangent, sizeof(float) * 4) ||
			!Take(Bones, sizeof(Bones)) || !Take(&V.BoneWeights, sizeof(float) * 4))
		{
			return false;
		}
		V.BoneIndices = {Bones[0], Bones[1], Bones[2], Bones[3]};
		return true;
	}

} // namespace

bool SaveSkeletonLeon(const std::string& Path, const USkeleton& Skeleton, const std::string& InName)
{
	if (Skeleton.BoneCount() <= 0)
	{
		return false;
	}
	fs::create_directories(fs::path(Path).parent_path());
	std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
	if (!Out)
	{
		std::cerr << "CookedSkeletal: cannot write skeleton '" << Path << "'\n";
		return false;
	}
	const std::uint32_t Magic = LeonSkeletonMagic;
	const std::uint32_t Version = static_cast<std::uint32_t>(CookedFormatVersion);
	const std::uint32_t BoneCount = static_cast<std::uint32_t>(Skeleton.BoneCount());
	Out.write(reinterpret_cast<const char*>(&Magic), sizeof(Magic));
	Out.write(reinterpret_cast<const char*>(&Version), sizeof(Version));
	if (!WriteString(Out, InName))
	{
		return false;
	}
	Out.write(reinterpret_cast<const char*>(&BoneCount), sizeof(BoneCount));
	for (int I = 0; I < Skeleton.BoneCount(); ++I)
	{
		if (!WriteString(Out, Skeleton.BoneNames[static_cast<std::size_t>(I)]))
		{
			return false;
		}
		const std::int32_t Parent = Skeleton.ParentIndices[static_cast<std::size_t>(I)];
		Out.write(reinterpret_cast<const char*>(&Parent), sizeof(Parent));
		const float* Ib = glm::value_ptr(Skeleton.InverseBindPose[static_cast<std::size_t>(I)]);
		Out.write(reinterpret_cast<const char*>(Ib), sizeof(float) * 16);
	}
	return static_cast<bool>(Out);
}

bool LoadSkeleton(const std::string& Path, USkeleton& Out, std::string* OutName)
{
	std::vector<std::uint8_t> Bytes;
	if (!ReadAllBytes(Path, Bytes) || Bytes.size() < 12)
	{
		return false;
	}
	const std::uint8_t* Ptr = Bytes.data();
	const std::uint8_t* End = Bytes.data() + Bytes.size();
	std::uint32_t Magic = 0;
	std::uint32_t Version = 0;
	std::memcpy(&Magic, Ptr, 4);
	Ptr += 4;
	std::memcpy(&Version, Ptr, 4);
	Ptr += 4;
	if (Magic != LeonSkeletonMagic || Version != static_cast<std::uint32_t>(CookedFormatVersion))
	{
		std::cerr << "CookedSkeletal: bad .lskel header in '" << Path << "'\n";
		return false;
	}
	std::string LocalName;
	if (!ReadString(Ptr, End, LocalName))
	{
		return false;
	}
	if (OutName != nullptr)
	{
		*OutName = LocalName;
	}
	std::uint32_t BoneCount = 0;
	if (Ptr + sizeof(BoneCount) > End)
	{
		return false;
	}
	std::memcpy(&BoneCount, Ptr, sizeof(BoneCount));
	Ptr += sizeof(BoneCount);
	Out = {};
	for (std::uint32_t I = 0; I < BoneCount; ++I)
	{
		std::string BoneName;
		if (!ReadString(Ptr, End, BoneName))
		{
			return false;
		}
		Out.BoneNames.push_back(std::move(BoneName));
		if ((Ptr + sizeof(std::int32_t) + (sizeof(float) * 16)) > End)
		{
			return false;
		}
		std::int32_t Parent = -1;
		std::memcpy(&Parent, Ptr, sizeof(Parent));
		Ptr += sizeof(Parent);
		Out.ParentIndices.push_back(Parent);
		glm::mat4 Ib(1.0f);
		std::memcpy(glm::value_ptr(Ib), Ptr, sizeof(float) * 16);
		Ptr += sizeof(float) * 16;
		Out.InverseBindPose.push_back(Ib);
	}
	return Out.BoneCount() > 0;
}

bool SaveSkeletalMeshLeon(const std::string& Path, const FSkeletalMeshData& Data, const std::string& SkeletonRelPath,
	const std::string& MaterialRelPath, const std::string& AssetName)
{
	if (Data.empty())
	{
		return false;
	}
	fs::create_directories(fs::path(Path).parent_path());
	std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
	if (!Out)
	{
		std::cerr << "CookedSkeletal: cannot write skelmesh '" << Path << "'\n";
		return false;
	}
	const std::uint32_t Magic = LeonSkelMeshMagic;
	const std::uint32_t Version = static_cast<std::uint32_t>(CookedFormatVersion);
	const std::uint32_t Vcount = static_cast<std::uint32_t>(Data.Vertices.size());
	const std::uint32_t Icount = static_cast<std::uint32_t>(Data.Indices.size());
	Out.write(reinterpret_cast<const char*>(&Magic), sizeof(Magic));
	Out.write(reinterpret_cast<const char*>(&Version), sizeof(Version));
	if (!WriteString(Out, AssetName) || !WriteString(Out, SkeletonRelPath) || !WriteString(Out, MaterialRelPath))
	{
		return false;
	}
	const float LocalMin[3] = {Data.LocalMin.x, Data.LocalMin.y, Data.LocalMin.z};
	const float LocalMax[3] = {Data.LocalMax.x, Data.LocalMax.y, Data.LocalMax.z};
	Out.write(reinterpret_cast<const char*>(LocalMin), sizeof(LocalMin));
	Out.write(reinterpret_cast<const char*>(LocalMax), sizeof(LocalMax));
	Out.write(reinterpret_cast<const char*>(&Vcount), sizeof(Vcount));
	Out.write(reinterpret_cast<const char*>(&Icount), sizeof(Icount));
	for (const FSkeletalVertex& V : Data.Vertices)
	{
		WriteVertex(Out, V);
	}
	Out.write(reinterpret_cast<const char*>(Data.Indices.data()),
		static_cast<std::streamsize>(Data.Indices.size() * sizeof(std::uint32_t)));
	return static_cast<bool>(Out);
}

bool LoadSkeletalMesh(
	const std::string& Path, FSkeletalMeshData& Out, USkeleton* SkeletonOverride, std::string* OutMaterialRelPath)
{
	std::vector<std::uint8_t> Bytes;
	if (!ReadAllBytes(Path, Bytes) || Bytes.size() < 40)
	{
		std::cerr << "CookedSkeletal: cannot read skelmesh '" << Path << "'\n";
		return false;
	}
	const std::uint8_t* Ptr = Bytes.data();
	const std::uint8_t* End = Bytes.data() + Bytes.size();
	std::uint32_t Magic = 0;
	std::uint32_t Version = 0;
	std::memcpy(&Magic, Ptr, 4);
	Ptr += 4;
	std::memcpy(&Version, Ptr, 4);
	Ptr += 4;
	if (Magic != LeonSkelMeshMagic || Version != static_cast<std::uint32_t>(CookedFormatVersion))
	{
		std::cerr << "CookedSkeletal: bad .lskm header\n";
		return false;
	}

	Out = {};
	std::string AssetName;
	std::string SkeletonRel;
	std::string MaterialRel;
	if (!ReadString(Ptr, End, AssetName) || !ReadString(Ptr, End, SkeletonRel) || !ReadString(Ptr, End, MaterialRel))
	{
		return false;
	}
	(void)AssetName;
	if (OutMaterialRelPath != nullptr)
	{
		*OutMaterialRelPath = MaterialRel;
	}
	if (!ReadVec3(Ptr, End, Out.LocalMin) || !ReadVec3(Ptr, End, Out.LocalMax))
	{
		return false;
	}
	if (Ptr + (sizeof(std::uint32_t) * 2) > End)
	{
		return false;
	}
	std::uint32_t Vcount = 0;
	std::uint32_t Icount = 0;
	std::memcpy(&Vcount, Ptr, 4);
	Ptr += 4;
	std::memcpy(&Icount, Ptr, 4);
	Ptr += 4;
	if (Vcount == 0 || Vcount > MaxCookedVertices || Icount > MaxCookedIndices)
	{
		std::cerr << "CookedSkeletal: .lskm vertex/index count out of range\n";
		return false;
	}

	if (SkeletonOverride != nullptr)
	{
		Out.Skeleton = *SkeletonOverride;
	}
	else
	{
		const std::string SkelPath = JoinRel(DirOf(Path), SkeletonRel);
		if (!LoadSkeleton(SkelPath, Out.Skeleton))
		{
			return false;
		}
	}

	Out.Vertices.resize(Vcount);
	for (std::uint32_t I = 0; I < Vcount; ++I)
	{
		if (!ReadVertex(Ptr, End, Out.Vertices[I]))
		{
			return false;
		}
	}
	const std::size_t IndexBytes = static_cast<std::size_t>(Icount) * sizeof(std::uint32_t);
	if (Ptr + IndexBytes > End)
	{
		return false;
	}
	Out.Indices.resize(Icount);
	std::memcpy(Out.Indices.data(), Ptr, IndexBytes);
	return !Out.empty();
}

bool SaveAnimSequenceLeon(
	const std::string& Path, const UAnimSequence& Anim, int BoneCount, const std::string& SkeletonRelPath)
{
	if (Anim.FrameCount() <= 0 || BoneCount <= 0)
	{
		return false;
	}
	fs::create_directories(fs::path(Path).parent_path());
	std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
	if (!Out)
	{
		return false;
	}
	const std::uint32_t Magic = LeonAnimMagic;
	const std::uint32_t Version = static_cast<std::uint32_t>(CookedFormatVersion);
	const std::uint32_t Frames = static_cast<std::uint32_t>(Anim.FrameCount());
	const std::uint32_t Bones = static_cast<std::uint32_t>(BoneCount);
	Out.write(reinterpret_cast<const char*>(&Magic), sizeof(Magic));
	Out.write(reinterpret_cast<const char*>(&Version), sizeof(Version));
	if (!WriteString(Out, Anim.Name) || !WriteString(Out, SkeletonRelPath))
	{
		return false;
	}
	Out.write(reinterpret_cast<const char*>(&Anim.DurationSeconds), sizeof(Anim.DurationSeconds));
	Out.write(reinterpret_cast<const char*>(&Anim.FramesPerSecond), sizeof(Anim.FramesPerSecond));
	Out.write(reinterpret_cast<const char*>(&Frames), sizeof(Frames));
	Out.write(reinterpret_cast<const char*>(&Bones), sizeof(Bones));
	for (int F = 0; F < Anim.FrameCount(); ++F)
	{
		const auto& Frame = Anim.LocalPoseFrames[static_cast<std::size_t>(F)];
		if (static_cast<int>(Frame.size()) != BoneCount)
		{
			return false;
		}
		for (int B = 0; B < BoneCount; ++B)
		{
			const float* P = glm::value_ptr(Frame[static_cast<std::size_t>(B)]);
			Out.write(reinterpret_cast<const char*>(P), sizeof(float) * 16);
		}
	}
	return static_cast<bool>(Out);
}

bool LoadAnimSequence(const std::string& Path, UAnimSequence& Out)
{
	std::vector<std::uint8_t> Bytes;
	if (!ReadAllBytes(Path, Bytes) || Bytes.size() < 28)
	{
		std::cerr << "CookedSkeletal: cannot read anim '" << Path << "'\n";
		return false;
	}
	const std::uint8_t* Ptr = Bytes.data();
	const std::uint8_t* End = Bytes.data() + Bytes.size();
	std::uint32_t Magic = 0;
	std::uint32_t Version = 0;
	std::memcpy(&Magic, Ptr, 4);
	Ptr += 4;
	std::memcpy(&Version, Ptr, 4);
	Ptr += 4;
	if (Magic != LeonAnimMagic || Version != static_cast<std::uint32_t>(CookedFormatVersion))
	{
		std::cerr << "CookedSkeletal: bad .lanim header\n";
		return false;
	}

	Out = {};
	std::string LocalName;
	std::string SkeletonRel;
	if (!ReadString(Ptr, End, LocalName) || !ReadString(Ptr, End, SkeletonRel))
	{
		return false;
	}
	(void)SkeletonRel;
	Out.Name = std::move(LocalName);
	if ((Ptr + (sizeof(float) * 2) + (sizeof(std::uint32_t) * 2)) > End)
	{
		return false;
	}
	std::memcpy(&Out.DurationSeconds, Ptr, sizeof(float));
	Ptr += sizeof(float);
	std::memcpy(&Out.FramesPerSecond, Ptr, sizeof(float));
	Ptr += sizeof(float);
	std::uint32_t FrameCount = 0;
	std::uint32_t BoneCount = 0;
	std::memcpy(&FrameCount, Ptr, 4);
	Ptr += 4;
	std::memcpy(&BoneCount, Ptr, 4);
	Ptr += 4;
	if (FrameCount == 0 || BoneCount == 0 || FrameCount > MaxCookedAnimFrames || BoneCount > MaxCookedBones)
	{
		std::cerr << "CookedSkeletal: .lanim frame/bone count out of range\n";
		return false;
	}
	const std::size_t Need =
		static_cast<std::size_t>(FrameCount) * static_cast<std::size_t>(BoneCount) * 16u * sizeof(float);
	if (Ptr + Need > End)
	{
		return false;
	}

	Out.LocalPoseFrames.resize(FrameCount);
	for (std::uint32_t F = 0; F < FrameCount; ++F)
	{
		Out.LocalPoseFrames[F].resize(BoneCount);
		for (std::uint32_t B = 0; B < BoneCount; ++B)
		{
			float* Dst = glm::value_ptr(Out.LocalPoseFrames[F][B]);
			std::memcpy(Dst, Ptr, sizeof(float) * 16);
			Ptr += sizeof(float) * 16;
		}
	}
	return Out.FrameCount() > 0;
}

bool SaveBlendSpace1DJson(const std::string& Path, const FBlendSpace1DAssetDesc& Desc)
{
	json Root;
	Root["version"] = CookedFormatVersion;
	Root["name"] = Desc.Name;
	Root["axisMin"] = Desc.AxisMin;
	Root["axisMax"] = Desc.AxisMax;
	Root["samples"] = json::array();
	for (const auto& S : Desc.Samples)
	{
		Root["samples"].push_back({{"anim", S.AnimRelPath}, {"position", S.Position}});
	}
	std::ofstream Out(Path);
	if (!Out)
	{
		return false;
	}
	Out << Root.dump(2) << '\n';
	return true;
}

bool LoadBlendSpace1DJson(const std::string& Path, FBlendSpace1DAssetDesc& Out)
{
	std::ifstream In(Path);
	if (!In)
	{
		return false;
	}
	json Root;
	try
	{
		In >> Root;
	}
	catch (const std::exception& Ex)
	{
		std::cerr << "CookedSkeletal: bad blendspace JSON '" << Path << "': " << Ex.what() << '\n';
		return false;
	}
	catch (...)
	{
		std::cerr << "CookedSkeletal: bad blendspace JSON '" << Path << "'\n";
		return false;
	}
	Out = {};
	Out.Name = Root.value("name", std::string{"BlendSpace1D"});
	Out.AxisMin = Root.value("axisMin", 0.0f);
	Out.AxisMax = Root.value("axisMax", 1.0f);
	if (!Root.contains("samples") || !Root["samples"].is_array())
	{
		return false;
	}
	for (const json& S : Root["samples"])
	{
		FBlendSpace1DAssetDesc::FSample Sample;
		Sample.AnimRelPath = S.at("anim").get<std::string>();
		Sample.Position = S.value("position", 0.0f);
		Out.Samples.push_back(std::move(Sample));
	}
	return !Out.Samples.empty();
}

bool SaveCharacterVisualLchar(const std::string& Path, const FCharacterVisualDesc& Desc)
{
	std::ofstream Out(Path);
	if (!Out)
	{
		return false;
	}
	Out << "# Leon Character (.lchar) — version " << CookedFormatVersion << "\n\n";
	Out << "[Info]\n";
	Out << "Name=" << (Desc.Name.empty() ? "Character" : Desc.Name) << "\n";
	Out << "FitHeight=" << Desc.FitHeight << "\n\n";
	Out << "[Mesh]\n";
	Out << "SkeletalMesh=" << Desc.SkeletalMeshRel << "\n\n";
	Out << "[Animation]\n";
	Out << "BlendSpace=" << Desc.BlendSpaceRel << "\n";
	if (!Desc.JumpStartAnimRel.empty())
	{
		Out << "JumpStart=" << Desc.JumpStartAnimRel << "\n";
	}
	if (!Desc.FallLoopAnimRel.empty())
	{
		Out << "FallLoop=" << Desc.FallLoopAnimRel << "\n";
	}
	if (!Desc.LandAnimRel.empty())
	{
		Out << "Land=" << Desc.LandAnimRel << "\n";
	}
	return static_cast<bool>(Out);
}

bool LoadCharacterVisualLchar(const std::string& Path, FCharacterVisualDesc& OutDesc)
{
	std::ifstream In(Path);
	if (!In)
	{
		return false;
	}
	OutDesc = {};
	std::string Section;
	std::string Line;
	while (std::getline(In, Line))
	{
		// strip comments
		if (const auto Hash = Line.find('#'); Hash != std::string::npos)
		{
			Line = Line.substr(0, Hash);
		}
		if (const auto Semi = Line.find(';'); Semi != std::string::npos)
		{
			Line = Line.substr(0, Semi);
		}
		// trim
		while (!Line.empty() && std::isspace(static_cast<unsigned char>(Line.front())))
		{
			Line.erase(Line.begin());
		}
		while (!Line.empty() && std::isspace(static_cast<unsigned char>(Line.back())))
		{
			Line.pop_back();
		}
		if (Line.empty())
		{
			continue;
		}
		if (Line.front() == '[' && Line.back() == ']')
		{
			Section = Line.substr(1, Line.size() - 2);
			for (char& C : Section)
			{
				C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
			}
			continue;
		}
		const auto Eq = Line.find('=');
		if (Eq == std::string::npos)
		{
			continue;
		}
		std::string Key = Line.substr(0, Eq);
		std::string Value = Line.substr(Eq + 1);
		while (!Key.empty() && std::isspace(static_cast<unsigned char>(Key.back())))
		{
			Key.pop_back();
		}
		while (!Value.empty() && std::isspace(static_cast<unsigned char>(Value.front())))
		{
			Value.erase(Value.begin());
		}
		for (char& C : Key)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}

		if (Section == "info")
		{
			if (Key == "name")
			{
				OutDesc.Name = Value;
			}
			else if (Key == "fitheight")
			{
				try
				{
					OutDesc.FitHeight = std::stof(Value);
				}
				catch (const std::exception& Ex)
				{
					std::cerr << "CookedSkeletal: bad FitHeight '" << Value << "': " << Ex.what() << '\n';
				}
			}
		}
		else if (Section == "mesh")
		{
			if (Key == "skeletalmesh")
			{
				OutDesc.SkeletalMeshRel = Value;
			}
		}
		else if (Section == "animation")
		{
			if (Key == "blendspace")
			{
				OutDesc.BlendSpaceRel = Value;
			}
			else if (Key == "jumpstart")
			{
				OutDesc.JumpStartAnimRel = Value;
			}
			else if (Key == "fallloop")
			{
				OutDesc.FallLoopAnimRel = Value;
			}
			else if (Key == "land")
			{
				OutDesc.LandAnimRel = Value;
			}
		}
	}
	return !OutDesc.SkeletalMeshRel.empty() && !OutDesc.BlendSpaceRel.empty();
}

namespace
{

	bool LoadCharacterVisualJsonLegacy(const std::string& Path, FCharacterVisualDesc& Out)
	{
		std::ifstream In(Path);
		if (!In)
		{
			return false;
		}
		json Root;
		try
		{
			In >> Root;
		}
		catch (const std::exception& Ex)
		{
			std::cerr << "CookedSkeletal: bad character JSON '" << Path << "': " << Ex.what() << '\n';
			return false;
		}
		catch (...)
		{
			std::cerr << "CookedSkeletal: bad character JSON '" << Path << "'\n";
			return false;
		}
		Out = {};
		Out.SkeletalMeshRel = Root.at("skeletalMesh").get<std::string>();
		Out.BlendSpaceRel = Root.at("blendSpace").get<std::string>();
		Out.FitHeight = Root.value("fitHeight", 1.85f);
		Out.JumpStartAnimRel = Root.value("jumpStart", std::string{});
		Out.FallLoopAnimRel = Root.value("fallLoop", std::string{});
		Out.LandAnimRel = Root.value("land", std::string{});
		return true;
	}

} // namespace

bool LoadCharacterVisual(const std::string& Path, FCharacterVisualDesc& Out)
{
	const fs::path P(Path);
	const std::string Ext = P.extension().string();
	std::string ExtLower = Ext;
	for (char& C : ExtLower)
	{
		C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
	}
	if (ExtLower == ".lchar")
	{
		return LoadCharacterVisualLchar(Path, Out);
	}
	if (ExtLower == ".json")
	{
		return LoadCharacterVisualJsonLegacy(Path, Out);
	}
	// Path without / unknown ext: try .lchar semantics by content.
	if (LoadCharacterVisualLchar(Path, Out))
	{
		return true;
	}
	return LoadCharacterVisualJsonLegacy(Path, Out);
}

bool CookAnimSequenceFromFbx(const std::string& FbxPath, const std::string& SkeletonPath,
	const std::string& OutAnimPath, const std::string& AnimName, bool bLooping)
{
	USkeleton Skeleton;
	if (!LoadSkeleton(SkeletonPath, Skeleton))
	{
		std::cerr << "CookAnimSequenceFromFbx: failed skeleton '" << SkeletonPath << "'\n";
		return false;
	}

	UAnimSequence Anim;
	if (!LoadAnimSequenceFromFbx(FbxPath, Skeleton, Anim))
	{
		std::cerr << "CookAnimSequenceFromFbx: failed FBX '" << FbxPath << "'\n";
		return false;
	}
	Anim.Name = AnimName.empty() ? fs::path(FbxPath).stem().string() : AnimName;
	Anim.bLooping = bLooping;

	const fs::path AnimPath(OutAnimPath);
	fs::create_directories(AnimPath.parent_path());

	const fs::path SkelAbs = fs::absolute(SkeletonPath).lexically_normal();
	const fs::path AnimDir = fs::absolute(AnimPath.parent_path()).lexically_normal();
	std::string SkelRel = fs::relative(SkelAbs, AnimDir).generic_string();
	if (SkelRel.empty())
	{
		SkelRel = "../" + SkelAbs.filename().string();
	}

	if (!SaveAnimSequenceLeon(AnimPath.string(), Anim, Skeleton.BoneCount(), SkelRel))
	{
		return false;
	}
	std::cout << "Cooked anim '" << Anim.Name << "' → " << OutAnimPath << "\n";
	return true;
}

bool CookCharacterFromFbx(const std::string& CharacterName, const std::string& MeshFbxPath,
	const std::string& RunFbxPath, const std::string& OutDirectory, const FCookJumpAnimPaths& JumpAnims)
{
	fs::create_directories(OutDirectory);
	fs::create_directories(fs::path(OutDirectory) / "Anims");
	fs::create_directories(fs::path(OutDirectory) / "Materials");

	FSkeletalMeshData MeshData;
	if (!LoadSkeletalMeshFromFbx(MeshFbxPath, MeshData))
	{
		return false;
	}

	UAnimSequence Idle = std::move(MeshData.EmbeddedAnim);
	Idle.Name = "BreathingIdle";
	MeshData.EmbeddedAnim = {};

	UAnimSequence Run;
	if (!LoadAnimSequenceFromFbx(RunFbxPath, MeshData.Skeleton, Run))
	{
		std::cerr << "CookCharacterFromFbx: run anim failed\n";
		return false;
	}
	if (Run.Name.empty())
	{
		Run.Name = "Running";
	}

	UAnimSequence JumpStart;
	UAnimSequence FallLoop;
	UAnimSequence Land;
	if (!JumpAnims.JumpStartFbx.empty())
	{
		if (!LoadAnimSequenceFromFbx(JumpAnims.JumpStartFbx, MeshData.Skeleton, JumpStart))
		{
			std::cerr << "CookCharacterFromFbx: jumpStart anim failed\n";
			return false;
		}
		JumpStart.Name = "JumpingUp";
		JumpStart.bLooping = false;
	}
	if (!JumpAnims.FallLoopFbx.empty())
	{
		if (!LoadAnimSequenceFromFbx(JumpAnims.FallLoopFbx, MeshData.Skeleton, FallLoop))
		{
			std::cerr << "CookCharacterFromFbx: fallLoop anim failed\n";
			return false;
		}
		FallLoop.Name = "FallingIdle";
		FallLoop.bLooping = true;
	}
	if (!JumpAnims.LandFbx.empty())
	{
		if (!LoadAnimSequenceFromFbx(JumpAnims.LandFbx, MeshData.Skeleton, Land))
		{
			std::cerr << "CookCharacterFromFbx: land anim failed\n";
			return false;
		}
		Land.Name = "FallingToLanding";
		Land.bLooping = false;
	}

	const std::string SkeletonFile = CharacterName + ".lskel";
	const std::string SkelMeshFile = CharacterName + ".lskm";
	const std::string MaterialRel = "Materials/M_" + CharacterName + ".lmat";
	const std::string IdleAnimRel = "Anims/BreathingIdle.lanim";
	const std::string RunAnimRel = "Anims/Running.lanim";
	const std::string JumpAnimRel = "Anims/JumpingUp.lanim";
	const std::string FallAnimRel = "Anims/FallingIdle.lanim";
	const std::string LocalLandAnimRel = "Anims/FallingToLanding.lanim";
	const std::string BlendRel = CharacterName + "_Locomotion.blendspace1d.json";
	const std::string CharacterRel = CharacterName + ".lchar";

	const fs::path OutDir(OutDirectory);
	if (!SaveSkeletonLeon((OutDir / SkeletonFile).string(), MeshData.Skeleton, CharacterName))
	{
		return false;
	}
	if (!SaveSkeletalMeshLeon((OutDir / SkelMeshFile).string(), MeshData, SkeletonFile, MaterialRel, CharacterName))
	{
		return false;
	}

	// Prefer the pack albedo when present (e.g. <Pack>/Content/Textures/T_Bot_D.png).
	std::string CharacterFolder = CharacterName;
	for (char& C : CharacterFolder)
	{
		C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
	}
	const std::string AlbedoFile = "T_" + CharacterName + "_D.png";
	const std::string AlbedoRel = "assets/characters/" + CharacterFolder + "/Textures/" + AlbedoFile;
	const bool bHasAlbedo = fs::exists(OutDir / "Textures" / AlbedoFile);
	if (!WriteSimpleCharacterLmat(
			(OutDir / MaterialRel).string(), "M_" + CharacterName, bHasAlbedo ? AlbedoRel : std::string{}))
	{
		return false;
	}

	const std::string SkelFromAnims = std::string("../") + SkeletonFile;
	if (!SaveAnimSequenceLeon((OutDir / IdleAnimRel).string(), Idle, MeshData.Skeleton.BoneCount(), SkelFromAnims))
	{
		return false;
	}
	if (!SaveAnimSequenceLeon((OutDir / RunAnimRel).string(), Run, MeshData.Skeleton.BoneCount(), SkelFromAnims))
	{
		return false;
	}
	if (JumpStart.FrameCount() > 0)
	{
		if (!SaveAnimSequenceLeon(
				(OutDir / JumpAnimRel).string(), JumpStart, MeshData.Skeleton.BoneCount(), SkelFromAnims))
		{
			return false;
		}
	}
	if (FallLoop.FrameCount() > 0)
	{
		if (!SaveAnimSequenceLeon(
				(OutDir / FallAnimRel).string(), FallLoop, MeshData.Skeleton.BoneCount(), SkelFromAnims))
		{
			return false;
		}
	}
	if (Land.FrameCount() > 0)
	{
		if (!SaveAnimSequenceLeon(
				(OutDir / LocalLandAnimRel).string(), Land, MeshData.Skeleton.BoneCount(), SkelFromAnims))
		{
			return false;
		}
	}

	FBlendSpace1DAssetDesc Bs;
	Bs.Name = CharacterName + "_Locomotion";
	Bs.Samples.push_back({IdleAnimRel, 0.0f});
	Bs.Samples.push_back({RunAnimRel, 1.0f});
	if (!SaveBlendSpace1DJson((OutDir / BlendRel).string(), Bs))
	{
		return false;
	}

	FCharacterVisualDesc Character;
	Character.Name = CharacterName;
	Character.SkeletalMeshRel = SkelMeshFile;
	Character.BlendSpaceRel = BlendRel;
	Character.FitHeight = 1.85f;
	if (JumpStart.FrameCount() > 0)
	{
		Character.JumpStartAnimRel = JumpAnimRel;
	}
	if (FallLoop.FrameCount() > 0)
	{
		Character.FallLoopAnimRel = FallAnimRel;
	}
	if (Land.FrameCount() > 0)
	{
		Character.LandAnimRel = LocalLandAnimRel;
	}
	if (!SaveCharacterVisualLchar((OutDir / CharacterRel).string(), Character))
	{
		return false;
	}

	std::cout << "Cooked character '" << CharacterName << "' → " << OutDirectory << "\n"
			  << "  " << SkeletonFile << "\n"
			  << "  " << SkelMeshFile << "\n"
			  << "  " << MaterialRel << "\n"
			  << "  " << IdleAnimRel << " / " << RunAnimRel << "\n";
	if (JumpStart.FrameCount() > 0)
	{
		std::cout << "  " << JumpAnimRel << "\n";
	}
	if (FallLoop.FrameCount() > 0)
	{
		std::cout << "  " << FallAnimRel << "\n";
	}
	if (Land.FrameCount() > 0)
	{
		std::cout << "  " << LocalLandAnimRel << "\n";
	}
	std::cout << "  " << BlendRel << "\n"
			  << "  " << CharacterRel << "\n";
	return true;
}
