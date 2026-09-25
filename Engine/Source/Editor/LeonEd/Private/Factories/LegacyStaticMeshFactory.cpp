#include "Factories/LegacyStaticMeshFactory.h"

#include "AssetImportUtils.h"
#include "Engine/StaticMesh.h"
#include "Factories/StaticMeshImport.h"
#include "LeonEdLog.h"
#include "MeshData.h"
#include "Misc/Paths.h"

namespace
{

	/** The `.lmesh` layout (ASSET_FORMATS.md before P14 part 2): a 52-byte header, then the arrays. */
	constexpr ANSICHAR LegacyMeshMagic[4] = {'L', 'M', 'S', 'H'};
	constexpr uint32 EngineWorldVersion = 2;
	constexpr int64 HeaderSize = 52;
	constexpr int64 VertexSize = 48;
	static_assert(sizeof(FVertex) == VertexSize, "a .lmesh vertex is FVertex's 48 bytes");

	/** Reads raw bytes at Offset (advancing it); false past the end. */
	bool ReadAt(const uint8* Buffer, int64 Size, int64& Offset, void* Out, int64 Length)
	{
		if (Length < 0 || Offset + Length > Size)
		{
			return false;
		}
		FMemory::Memcpy(Out, Buffer + Offset, static_cast<SIZE_T>(Length));
		Offset += Length;
		return true;
	}

	/** A version 2 `.lmesh` into Data; false with OutError. */
	bool ReadLegacyMesh(const uint8* Buffer, int64 Size, FMeshData& Data, FString& OutError)
	{
		uint32 Header[13] = {};
		int64 Offset = 0;
		if (!ReadAt(Buffer, Size, Offset, Header, HeaderSize) || FMemory::Memcmp(Header, LegacyMeshMagic, 4) != 0)
		{
			OutError = TEXT("not a .lmesh");
			return false;
		}
		const uint32 Version = Header[1];
		const uint32 VertexCount = Header[3];
		const uint32 IndexCount = Header[4];
		const uint32 SubmeshCount = Header[5];
		const uint32 SlotCount = Header[6];
		if (Version != EngineWorldVersion)
		{
			OutError = FString::Printf(
				"a version %u .lmesh (only version 2, in the engine world, migrates: import its source)", Version);
			return false;
		}
		if (VertexCount == 0 || IndexCount == 0)
		{
			OutError = TEXT("an empty .lmesh");
			return false;
		}
		Data.Vertices.SetNum(static_cast<int32>(VertexCount));
		Data.Indices.SetNum(static_cast<int32>(IndexCount));
		if (!ReadAt(Buffer, Size, Offset, Data.Vertices.GetData(), VertexSize * VertexCount) ||
			!ReadAt(Buffer, Size, Offset, Data.Indices.GetData(), static_cast<int64>(sizeof(uint32)) * IndexCount))
		{
			OutError = TEXT("a truncated .lmesh");
			return false;
		}
		for (uint32 Index = 0; Index < SubmeshCount; ++Index)
		{
			uint32 Section[3] = {};
			if (!ReadAt(Buffer, Size, Offset, Section, sizeof(Section)))
			{
				OutError = TEXT("truncated .lmesh sections");
				return false;
			}
			Data.Submeshes.Add(FMeshSection{
				static_cast<int32>(Section[0]), static_cast<int32>(Section[1]), static_cast<int32>(Section[2])});
		}
		if (SubmeshCount == 0)
		{
			Data.Submeshes.Add(FMeshSection{0, static_cast<int32>(IndexCount), 0});
		}
		const int32 NumSlots = static_cast<int32>(FMath::Max<uint32>(1, SlotCount));
		Data.Materials.SetNum(NumSlots);
		Data.MaterialSlotNames.SetNum(NumSlots);
		Data.AlbedoMapPaths.SetNum(NumSlots);
		Data.NormalMapPaths.SetNum(NumSlots);
		for (uint32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			FString SlotString;
			ANSICHAR Char = 0;
			while (ReadAt(Buffer, Size, Offset, &Char, 1) && Char != '\0')
			{
				SlotString.AppendChar(Char);
			}
			// The runtime kept a slot string only as a diffuse map.
			if (SlotString.Contains(TEXT(".png"), ESearchCase::CaseSensitive) ||
				SlotString.Contains(TEXT(".jpg"), ESearchCase::CaseSensitive))
			{
				Data.AlbedoMapPaths[static_cast<int32>(Slot)] = SlotString;
			}
		}
		return true;
	}

} // namespace

ULegacyStaticMeshFactory::ULegacyStaticMeshFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStaticMesh::StaticClass();
	Formats.Add(TEXT("lmesh;Leon legacy cooked static mesh"));
	bEditorImport = 1;
}

UObject* ULegacyStaticMeshFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd,
	bool& bOutOperationCanceled)
{
	(void)InClass;
	(void)Context;
	(void)Type;
	bOutOperationCanceled = false;
	AdditionalImportedObjects.Reset();
	FMeshData Data;
	FString Error;
	if (!ReadLegacyMesh(Buffer, BufferEnd - Buffer, Data, Error))
	{
		UE_LOG(LogLeonEd, Error, "LegacyStaticMeshFactory: '%s' is %s", *CurrentFilename, *Error);
		return nullptr;
	}
	// Every slot had a material of its own at run time (the default parameters, the slot's map).
	FString MeshBase = InName.ToString();
	MeshBase.RemoveFromStart(TEXT("SM_"), ESearchCase::CaseSensitive);
	for (int32 Slot = 0; Slot < Data.MaterialSlotNames.Num(); ++Slot)
	{
		Data.MaterialSlotNames[Slot] = FString::Printf("%s_Slot%d", *MeshBase, Slot);
		if (!Data.AlbedoMapPaths[Slot].IsEmpty() && FPaths::IsRelative(Data.AlbedoMapPaths[Slot]))
		{
			Data.AlbedoMapPaths[Slot] = FPaths::Combine(FPaths::GetPath(CurrentFilename), Data.AlbedoMapPaths[Slot]);
		}
	}
	UStaticMesh* Mesh = CreateOrOverwriteAsset<UStaticMesh>(InParent, InName, Flags);
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	StaticMeshImport::BuildStaticMesh(*Mesh, Data, true, AdditionalImportedObjects);
	Buffer = BufferEnd;
	return Mesh;
}
