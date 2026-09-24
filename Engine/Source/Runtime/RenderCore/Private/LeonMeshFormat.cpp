#include "LeonMeshFormat.h"

#include "MeshData.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

	constexpr char Magic[4] = {'L', 'M', 'S', 'H'};
	constexpr std::uint32_t Version = 1;

#pragma pack(push, 1)
	struct FLeonMeshHeader
	{
		char Magic[4];
		std::uint32_t Version;
		std::uint32_t Flags;
		std::uint32_t VertexCount;
		std::uint32_t IndexCount;
		std::uint32_t SubmeshCount;
		std::uint32_t MaterialSlotCount;
		float AabbMin[3];
		float AabbMax[3];
	};
#pragma pack(pop)

	struct FLeonMeshSection
	{
		std::uint32_t IndexOffset = 0;
		std::uint32_t IndexCount = 0;
		std::uint32_t MaterialIndex = 0;
	};

	[[nodiscard]] std::string ExtLower(const std::string& Path)
	{
		const auto Pos = Path.find_last_of('.');
		if (Pos == std::string::npos)
		{
			return {};
		}
		std::string E = Path.substr(Pos);
		for (char& C : E)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		return E;
	}

	void ComputeAabb(const FMeshData& Data, float OutMin[3], float OutMax[3])
	{
		OutMin[0] = OutMin[1] = OutMin[2] = 0.0f;
		OutMax[0] = OutMax[1] = OutMax[2] = 0.0f;
		if (Data.Vertices.empty())
		{
			return;
		}
		OutMin[0] = OutMax[0] = Data.Vertices[0].Position.x;
		OutMin[1] = OutMax[1] = Data.Vertices[0].Position.y;
		OutMin[2] = OutMax[2] = Data.Vertices[0].Position.z;
		for (const FVertex& V : Data.Vertices)
		{
			OutMin[0] = std::min(OutMin[0], V.Position.x);
			OutMin[1] = std::min(OutMin[1], V.Position.y);
			OutMin[2] = std::min(OutMin[2], V.Position.z);
			OutMax[0] = std::max(OutMax[0], V.Position.x);
			OutMax[1] = std::max(OutMax[1], V.Position.y);
			OutMax[2] = std::max(OutMax[2], V.Position.z);
		}
	}

} // namespace

bool IsLeonMeshPath(const std::string& Path)
{
	return ExtLower(Path) == ".lmesh";
}

bool LoadLeonMeshFile(const std::string& Path, FMeshData& Out)
{
	std::ifstream In(Path, std::ios::binary);
	if (!In.is_open())
	{
		std::cerr << "LeonMesh: cannot open " << Path << '\n';
		return false;
	}
	FLeonMeshHeader Header{};
	In.read(reinterpret_cast<char*>(&Header), sizeof(Header));
	if (!In || std::memcmp(Header.Magic, Magic, 4) != 0 || Header.Version != Version)
	{
		std::cerr << "LeonMesh: bad header in " << Path << '\n';
		return false;
	}
	if (Header.VertexCount == 0 || Header.IndexCount == 0)
	{
		std::cerr << "LeonMesh: empty mesh in " << Path << '\n';
		return false;
	}

	FMeshData Data;
	Data.Vertices.resize(Header.VertexCount);
	Data.Indices.resize(Header.IndexCount);
	In.read(reinterpret_cast<char*>(Data.Vertices.data()),
		static_cast<std::streamsize>(sizeof(FVertex) * Header.VertexCount));
	In.read(reinterpret_cast<char*>(Data.Indices.data()),
		static_cast<std::streamsize>(sizeof(std::uint32_t) * Header.IndexCount));
	if (!In)
	{
		std::cerr << "LeonMesh: truncated vertex/index data in " << Path << '\n';
		return false;
	}

	if (Header.SubmeshCount == 0)
	{
		Data.Submeshes.push_back(FMeshSection{0, static_cast<int>(Header.IndexCount), 0});
	}
	else
	{
		std::vector<FLeonMeshSection> Subs(Header.SubmeshCount);
		In.read(reinterpret_cast<char*>(Subs.data()),
			static_cast<std::streamsize>(sizeof(FLeonMeshSection) * Header.SubmeshCount));
		if (!In)
		{
			std::cerr << "LeonMesh: truncated submeshes in " << Path << '\n';
			return false;
		}
		Data.Submeshes.reserve(Subs.size());
		for (const FLeonMeshSection& S : Subs)
		{
			Data.Submeshes.push_back(FMeshSection{
				static_cast<int>(S.IndexOffset), static_cast<int>(S.IndexCount), static_cast<int>(S.MaterialIndex)});
		}
	}

	// Optional string table: material slot paths (null-terminated), one per slot.
	Data.Materials.resize(std::max<std::uint32_t>(1, Header.MaterialSlotCount));
	Data.AlbedoMapPaths.resize(Data.Materials.size());
	for (std::uint32_t I = 0; I < Header.MaterialSlotCount; ++I)
	{
		std::string Slot;
		char Ch = 0;
		while (In.get(Ch))
		{
			if (Ch == '\0')
			{
				break;
			}
			Slot.push_back(Ch);
		}
		// Slot string may be a future .lmat path; keep in albedoMapPaths unused for now
		// or encode as material name. Store as tag in albedoMapPaths if looks like texture.
		if (!Slot.empty() && (Slot.find(".png") != std::string::npos || Slot.find(".jpg") != std::string::npos))
		{
			Data.AlbedoMapPaths[I] = Slot;
		}
		(void)Slot;
	}

	Out = std::move(Data);
	return !Out.empty();
}

bool SaveLeonMeshFile(const std::string& Path, const FMeshData& Data)
{
	if (Data.empty())
	{
		std::cerr << "LeonMesh: refusing to save empty mesh\n";
		return false;
	}
	std::ofstream Out(Path, std::ios::binary | std::ios::trunc);
	if (!Out.is_open())
	{
		std::cerr << "LeonMesh: cannot write " << Path << '\n';
		return false;
	}

	FLeonMeshHeader Header{};
	std::memcpy(Header.Magic, Magic, 4);
	Header.Version = Version;
	Header.Flags = 0;
	Header.VertexCount = static_cast<std::uint32_t>(Data.Vertices.size());
	Header.IndexCount = static_cast<std::uint32_t>(Data.Indices.size());
	Header.SubmeshCount = static_cast<std::uint32_t>(Data.Submeshes.empty() ? 1 : Data.Submeshes.size());
	Header.MaterialSlotCount = static_cast<std::uint32_t>(std::max<std::size_t>(1, Data.Materials.size()));
	ComputeAabb(Data, Header.AabbMin, Header.AabbMax);

	Out.write(reinterpret_cast<const char*>(&Header), sizeof(Header));
	Out.write(reinterpret_cast<const char*>(Data.Vertices.data()),
		static_cast<std::streamsize>(sizeof(FVertex) * Data.Vertices.size()));
	Out.write(reinterpret_cast<const char*>(Data.Indices.data()),
		static_cast<std::streamsize>(sizeof(std::uint32_t) * Data.Indices.size()));

	if (Data.Submeshes.empty())
	{
		FLeonMeshSection S{0, Header.IndexCount, 0};
		Out.write(reinterpret_cast<const char*>(&S), sizeof(S));
	}
	else
	{
		for (const FMeshSection& Sm : Data.Submeshes)
		{
			FLeonMeshSection S{static_cast<std::uint32_t>(Sm.IndexOffset), static_cast<std::uint32_t>(Sm.IndexCount),
				static_cast<std::uint32_t>(Sm.MaterialIndex)};
			Out.write(reinterpret_cast<const char*>(&S), sizeof(S));
		}
	}

	for (std::uint32_t I = 0; I < Header.MaterialSlotCount; ++I)
	{
		std::string Slot;
		if (I < Data.AlbedoMapPaths.size())
		{
			Slot = Data.AlbedoMapPaths[I];
		}
		Out.write(Slot.c_str(), static_cast<std::streamsize>(Slot.size() + 1));
	}
	return static_cast<bool>(Out);
}
