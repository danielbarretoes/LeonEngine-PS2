#include "MemoryCardSaveGameSystem.h"

#include "Misc/Crc.h"

DEFINE_LOG_CATEGORY_STATIC(LogMemoryCard, Log, All);

// The memory card's save game system (Docs/PLANS/ps2-shipping.md N24).

namespace
{
	/** Appends little-endian values (the EE and x64 are both little-endian). */
	template <typename T>
	void Append(TArray<uint8>& Bytes, const T& Value)
	{
		const int32 Offset = Bytes.AddUninitialized(int32(sizeof(T)));
		FMemory::Memcpy(Bytes.GetData() + Offset, &Value, sizeof(T));
	}

	template <typename T>
	T ReadAt(const TArray<uint8>& Bytes, int32 Offset)
	{
		T Value{};
		FMemory::Memcpy(&Value, Bytes.GetData() + Offset, sizeof(T));
		return Value;
	}

	/** A character's full-width Shift-JIS code (the browser's titles), '?' for what has none here. */
	uint16 ToShiftJis(TCHAR Char)
	{
		if (Char >= 'A' && Char <= 'Z')
		{
			return uint16(0x8260 + (Char - 'A'));
		}
		if (Char >= 'a' && Char <= 'z')
		{
			return uint16(0x8281 + (Char - 'a'));
		}
		if (Char >= '0' && Char <= '9')
		{
			return uint16(0x824F + (Char - '0'));
		}
		switch (Char)
		{
			case ' ':
				return 0x8140;
			case '.':
				return 0x8144;
			case ':':
				return 0x8146;
			case '!':
				return 0x8149;
			case '_':
				return 0x8151;
			case '-':
				return 0x817C;
			case '(':
				return 0x8169;
			case ')':
				return 0x816A;
			default:
				return 0x8148;
		}
	}

	/** A 16-bit texel of the icon's texture (A1 B5 G5 R5), from 8-bit channels. */
	uint16 Texel(uint8 Red, uint8 Green, uint8 Blue)
	{
		return uint16(0x8000 | ((Blue >> 3) << 10) | ((Green >> 3) << 5) | (Red >> 3));
	}

	/** The file header: tag, payload size, the payload's CRC. */
	constexpr int32 HeaderSize = 12;
} // namespace

FMemoryCardSaveGameSystem::FMemoryCardSaveGameSystem(
	IMemoryCardDevice& InDevice, const FString& InDirectory, const FString& InTitle)
	: Device(InDevice)
	, Directory(InDirectory.Left(32))
	, Title(InTitle.Left(32))
{
}

FString FMemoryCardSaveGameSystem::GetSavePath(const TCHAR* Name) const
{
	return FString::Printf(TEXT("/%s/%s"), *Directory, *FString(Name).Left(31));
}

int32 FMemoryCardSaveGameSystem::GetFileKB(int32 Size)
{
	// A cluster is 1 KB; an empty file still takes its entry's.
	return FMath::Max(1, (Size + 1023) / 1024);
}

ESaveGameResult FMemoryCardSaveGameSystem::CheckCard(int32& OutFreeKB)
{
	bool bChanged = false;
	switch (Device.GetState(OutFreeKB, bChanged))
	{
		case EMemoryCardState::NoCard:
			return ESaveGameResult::NoStorage;
		case EMemoryCardState::Unformatted:
			return ESaveGameResult::Unformatted;
		case EMemoryCardState::Ready:
			break;
	}
	return ESaveGameResult::Succeeded;
}

ISaveGameSystem::ESaveExistsResult FMemoryCardSaveGameSystem::DoesSaveGameExistWithResult(
	const TCHAR* Name, int32 /*UserIndex*/)
{
	int32 FreeKB = 0;
	LastResult = CheckCard(FreeKB);
	if (LastResult != ESaveGameResult::Succeeded)
	{
		return ESaveExistsResult::UnspecifiedError;
	}
	if (!Device.FileExists(GetSavePath(Name)))
	{
		LastResult = ESaveGameResult::NotFound;
		return ESaveExistsResult::DoesNotExist;
	}
	return ESaveExistsResult::OK;
}

bool FMemoryCardSaveGameSystem::SaveGame(
	bool /*bAttemptToUseUI*/, const TCHAR* Name, int32 /*UserIndex*/, const TArray<uint8>& Data)
{
	int32 FreeKB = 0;
	LastResult = CheckCard(FreeKB);
	if (LastResult != ESaveGameResult::Succeeded)
	{
		UE_LOG(LogMemoryCard, Warning, "Memory card: %s not saved: %s", Name, LexToString(LastResult));
		return false;
	}
	const FString Path = GetSavePath(Name);
	TArray<uint8> File;
	File.Reserve(HeaderSize + Data.Num());
	Append(File, FileTag);
	Append(File, uint32(Data.Num()));
	Append(File, FCrc::MemCrc32(Data.GetData(), Data.Num()));
	File.Append(Data);

	// The room it needs: the folder with its icons the first time, else the file's growth.
	const bool bHasDirectory = Device.DirectoryExists(TEXT("/") + Directory);
	const TArray<uint8> IconSys = bHasDirectory ? TArray<uint8>() : MakeIconSys(Title, GetIconFilename());
	const TArray<uint8> Icon = bHasDirectory ? TArray<uint8>() : MakeIcon();
	int32 NeededKB = GetFileKB(File.Num());
	if (!bHasDirectory)
	{
		NeededKB += DirectoryKB + GetFileKB(IconSys.Num()) + GetFileKB(Icon.Num());
	}
	else if (Device.FileExists(Path))
	{
		TArray<uint8> Old;
		if (Device.ReadFile(Path, Old) == ESaveGameResult::Succeeded)
		{
			NeededKB = FMath::Max(0, NeededKB - GetFileKB(Old.Num()));
		}
	}
	if (NeededKB > FreeKB)
	{
		LastResult = ESaveGameResult::NoSpace;
		UE_LOG(LogMemoryCard, Warning, "Memory card: %s not saved: %d KB needed, %d KB free", Name, NeededKB, FreeKB);
		return false;
	}

	ESaveGameResult Result = ESaveGameResult::Succeeded;
	if (!bHasDirectory)
	{
		Result = Device.MakeDirectory(TEXT("/") + Directory);
		if (Result == ESaveGameResult::Succeeded)
		{
			Result = Device.WriteFile(FString::Printf(TEXT("/%s/icon.sys"), *Directory), IconSys);
		}
		if (Result == ESaveGameResult::Succeeded)
		{
			Result = Device.WriteFile(FString::Printf(TEXT("/%s/%s"), *Directory, GetIconFilename()), Icon);
		}
	}
	if (Result == ESaveGameResult::Succeeded)
	{
		Result = Device.WriteFile(Path, File);
	}
	// The card that was written is still there (not pulled out, not swapped).
	int32 FreeAfter = 0;
	bool bChanged = false;
	const EMemoryCardState After = Device.GetState(FreeAfter, bChanged);
	if (After != EMemoryCardState::Ready || bChanged)
	{
		Result = ESaveGameResult::Removed;
	}
	LastResult = Result;
	if (Result != ESaveGameResult::Succeeded)
	{
		UE_LOG(LogMemoryCard, Warning, "Memory card: %s not saved: %s", Name, LexToString(Result));
		return false;
	}
	UE_LOG(LogMemoryCard, Log, "Memory card: saved %s (%d bytes)", *Path, Data.Num());
	return true;
}

bool FMemoryCardSaveGameSystem::LoadGame(
	bool /*bAttemptToUseUI*/, const TCHAR* Name, int32 /*UserIndex*/, TArray<uint8>& Data)
{
	int32 FreeKB = 0;
	LastResult = CheckCard(FreeKB);
	if (LastResult != ESaveGameResult::Succeeded)
	{
		return false;
	}
	const FString Path = GetSavePath(Name);
	if (!Device.FileExists(Path))
	{
		LastResult = ESaveGameResult::NotFound;
		return false;
	}
	TArray<uint8> File;
	LastResult = Device.ReadFile(Path, File);
	if (LastResult == ESaveGameResult::Succeeded)
	{
		bool bChanged = false;
		if (Device.GetState(FreeKB, bChanged) != EMemoryCardState::Ready || bChanged)
		{
			LastResult = ESaveGameResult::Removed;
		}
	}
	if (LastResult != ESaveGameResult::Succeeded)
	{
		UE_LOG(LogMemoryCard, Warning, "Memory card: %s not loaded: %s", Name, LexToString(LastResult));
		return false;
	}
	const uint32 Size = File.Num() >= HeaderSize ? ReadAt<uint32>(File, 4) : 0;
	if (File.Num() < HeaderSize || ReadAt<uint32>(File, 0) != FileTag ||
		int64(Size) != int64(File.Num() - HeaderSize) ||
		FCrc::MemCrc32(File.GetData() + HeaderSize, int32(Size)) != ReadAt<uint32>(File, 8))
	{
		LastResult = ESaveGameResult::Corrupt;
		UE_LOG(LogMemoryCard, Warning, "Memory card: %s is damaged", *Path);
		return false;
	}
	Data.Reset(int32(Size));
	Data.Append(File.GetData() + HeaderSize, int32(Size));
	return true;
}

bool FMemoryCardSaveGameSystem::DeleteGame(bool /*bAttemptToUseUI*/, const TCHAR* Name, int32 UserIndex)
{
	if (DoesSaveGameExistWithResult(Name, UserIndex) != ESaveExistsResult::OK)
	{
		return false;
	}
	LastResult = Device.DeleteFile(GetSavePath(Name));
	return LastResult == ESaveGameResult::Succeeded;
}

TArray<uint8> FMemoryCardSaveGameSystem::MakeIconSys(const FString& InTitle, const FString& IconFilename)
{
	TArray<uint8> Bytes;
	Bytes.Reserve(IconSysSize);
	Bytes.Append(reinterpret_cast<const uint8*>("PS2D"), 4);
	Append(Bytes, uint16(0)); // saved data
	// The browser breaks the title in two lines at the last space of its first 16 characters.
	int32 Break = 0;
	for (int32 Index = 0; Index < FMath::Min(InTitle.Len(), 16); ++Index)
	{
		Break = InTitle[Index] == ' ' ? Index + 1 : Break;
	}
	if (InTitle.Len() <= 16)
	{
		Break = 0;
	}
	Append(Bytes, uint16(Break * 2));
	Append(Bytes, uint32(0));
	Append(Bytes, uint32(0x40)); // background transparency
	// The background's corners: a dark blue, lighter at the top.
	const int32 Corners[4][4] = {{16, 32, 64, 0}, {16, 32, 64, 0}, {0, 0, 16, 0}, {0, 0, 16, 0}};
	for (const auto& Corner : Corners)
	{
		for (const int32 Channel : Corner)
		{
			Append(Bytes, Channel);
		}
	}
	const float LightDirections[3][4] = {
		{0.5f, 0.5f, 0.5f, 0.0f}, {0.0f, -0.4f, -0.1f, 0.0f}, {-0.5f, -0.5f, 0.5f, 0.0f}};
	const float LightColors[3][4] = {{0.5f, 0.5f, 0.5f, 0.0f}, {0.7f, 0.7f, 0.7f, 0.0f}, {0.5f, 0.5f, 0.5f, 0.0f}};
	const float Ambient[4] = {0.5f, 0.5f, 0.5f, 0.0f};
	for (const auto& Direction : LightDirections)
	{
		for (const float Value : Direction)
		{
			Append(Bytes, Value);
		}
	}
	for (const auto& Color : LightColors)
	{
		for (const float Value : Color)
		{
			Append(Bytes, Value);
		}
	}
	for (const float Value : Ambient)
	{
		Append(Bytes, Value);
	}
	// The title: 34 Shift-JIS characters, each big-endian, the rest 0.
	for (int32 Index = 0; Index < 34; ++Index)
	{
		const uint16 Code = Index < InTitle.Len() ? ToShiftJis(InTitle[Index]) : uint16(0);
		Bytes.Add(uint8(Code >> 8));
		Bytes.Add(uint8(Code & 0xff));
	}
	// The list, copy and delete icons: the same file.
	for (int32 Icon = 0; Icon < 3; ++Icon)
	{
		for (int32 Index = 0; Index < 64; ++Index)
		{
			Bytes.Add(Index < IconFilename.Len() ? uint8(IconFilename[Index]) : uint8(0));
		}
	}
	Bytes.AddZeroed(IconSysSize - Bytes.Num());
	return Bytes;
}

TArray<uint8> FMemoryCardSaveGameSystem::MakeIcon()
{
	constexpr int32 TextureSize = 128;
	// Two triangles: a square 2 units wide standing on the floor (the icon's y grows down; 4096 is 1.0).
	struct FIconVertex
	{
		int16 X, Y, U, V;
	};
	const FIconVertex Vertices[6] = {{-4096, 0, 0, 4096}, {4096, 0, 4096, 4096}, {4096, -8192, 4096, 0},
		{-4096, 0, 0, 4096}, {4096, -8192, 4096, 0}, {-4096, -8192, 0, 0}};
	TArray<uint8> Bytes;
	Bytes.Reserve(20 + 6 * 24 + 20 + 24 + TextureSize * TextureSize * 2);
	Append(Bytes, uint32(0x00010000)); // the file's id
	Append(Bytes, uint32(1)); // one shape
	Append(Bytes, uint32(0x07)); // an uncompressed texture
	Append(Bytes, 1.0f);
	Append(Bytes, uint32(6));
	for (const FIconVertex& Vertex : Vertices)
	{
		// The shape's position, its normal (toward the viewer), its texture coordinates and its colour (0x80: 1.0).
		Append(Bytes, Vertex.X);
		Append(Bytes, Vertex.Y);
		Append(Bytes, int16(0));
		Append(Bytes, int16(0));
		Append(Bytes, int16(0));
		Append(Bytes, int16(0));
		Append(Bytes, int16(-4096));
		Append(Bytes, int16(0));
		Append(Bytes, Vertex.U);
		Append(Bytes, Vertex.V);
		Append(Bytes, uint32(0x80808080));
	}
	// The animation: one frame of the one shape.
	Append(Bytes, uint32(0x01));
	Append(Bytes, uint32(1));
	Append(Bytes, 1.0f);
	Append(Bytes, uint32(0));
	Append(Bytes, uint32(1));
	Append(Bytes, uint32(0)); // shape
	Append(Bytes, uint32(1)); // keys
	Append(Bytes, uint32(1));
	Append(Bytes, uint32(0));
	Append(Bytes, 0.0f);
	Append(Bytes, 1.0f);
	// The texture: a dark tile with a lighter frame and ShooterGame's green crosshair in the middle.
	for (int32 Y = 0; Y < TextureSize; ++Y)
	{
		for (int32 X = 0; X < TextureSize; ++X)
		{
			const bool bFrame = X < 6 || Y < 6 || X >= TextureSize - 6 || Y >= TextureSize - 6;
			const int32 DX = FMath::Abs(X - TextureSize / 2);
			const int32 DY = FMath::Abs(Y - TextureSize / 2);
			const bool bCross = (DX < 4 && DY > 12 && DY < 48) || (DY < 4 && DX > 12 && DX < 48);
			const uint16 Color = bCross ? Texel(40, 240, 40) : bFrame ? Texel(120, 130, 150) : Texel(24, 28, 40);
			Append(Bytes, Color);
		}
	}
	return Bytes;
}
