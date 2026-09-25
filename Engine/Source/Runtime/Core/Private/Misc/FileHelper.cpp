#include "Misc/FileHelper.h"

#include "Math/NumericLimits.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "Templates/UniquePtr.h"

namespace
{
	/** Appends a code point as UTF-8. */
	void AppendUtf8(FString& Out, uint32 CodePoint)
	{
		if (CodePoint < 0x80)
		{
			Out.AppendChar(TCHAR(CodePoint));
		}
		else if (CodePoint < 0x800)
		{
			Out.AppendChar(TCHAR(0xC0 | (CodePoint >> 6)));
			Out.AppendChar(TCHAR(0x80 | (CodePoint & 0x3F)));
		}
		else if (CodePoint < 0x10000)
		{
			Out.AppendChar(TCHAR(0xE0 | (CodePoint >> 12)));
			Out.AppendChar(TCHAR(0x80 | ((CodePoint >> 6) & 0x3F)));
			Out.AppendChar(TCHAR(0x80 | (CodePoint & 0x3F)));
		}
		else
		{
			Out.AppendChar(TCHAR(0xF0 | (CodePoint >> 18)));
			Out.AppendChar(TCHAR(0x80 | ((CodePoint >> 12) & 0x3F)));
			Out.AppendChar(TCHAR(0x80 | ((CodePoint >> 6) & 0x3F)));
			Out.AppendChar(TCHAR(0x80 | (CodePoint & 0x3F)));
		}
	}

	/** UTF-16 (after the BOM) to UTF-8. */
	void Utf16ToString(FString& Result, const uint8* Buffer, int32 Size, bool bBigEndian)
	{
		const int32 NumUnits = Size / 2;
		Result.Reserve(NumUnits);
		for (int32 Index = 0; Index < NumUnits; ++Index)
		{
			auto Unit = [Buffer, bBigEndian](int32 UnitIndex)
			{
				const uint8 Lo = Buffer[UnitIndex * 2 + (bBigEndian ? 1 : 0)];
				const uint8 Hi = Buffer[UnitIndex * 2 + (bBigEndian ? 0 : 1)];
				return uint32(Lo) | (uint32(Hi) << 8);
			};
			uint32 CodePoint = Unit(Index);
			if (CodePoint >= 0xD800 && CodePoint <= 0xDBFF && Index + 1 < NumUnits)
			{
				const uint32 Low = Unit(Index + 1);
				if (Low >= 0xDC00 && Low <= 0xDFFF)
				{
					CodePoint = 0x10000 + ((CodePoint - 0xD800) << 10) + (Low - 0xDC00);
					++Index;
				}
			}
			AppendUtf8(Result, CodePoint);
		}
	}

	/** Writes through "<Filename>.<guid>.tmp" and a rename, or in place when appending. */
	bool WriteBytes(const uint8* Data, int64 Size, const TCHAR* Filename, IFileManager* FileManager, uint32 WriteFlags)
	{
		if (WriteFlags & FILEWRITE_Append)
		{
			TUniquePtr<FArchive> Ar(FileManager->CreateFileWriter(Filename, WriteFlags));
			if (!Ar)
			{
				return false;
			}
			Ar->Serialize(const_cast<uint8*>(Data), Size);
			return Ar->Close();
		}

		const FString TempFilename = FString(Filename) + "." + FGuid::NewGuid().ToString() + ".tmp";
		{
			TUniquePtr<FArchive> Ar(
				FileManager->CreateFileWriter(*TempFilename, WriteFlags & ~uint32(FILEWRITE_NoReplaceExisting)));
			if (!Ar)
			{
				return false;
			}
			Ar->Serialize(const_cast<uint8*>(Data), Size);
			if (!Ar->Close())
			{
				Ar.Reset();
				FileManager->Delete(*TempFilename, false, true, true);
				return false;
			}
		}

		if ((WriteFlags & FILEWRITE_NoReplaceExisting) && FileManager->FileExists(Filename))
		{
			FileManager->Delete(*TempFilename, false, true, true);
			return false;
		}

		if (!FileManager->Move(Filename, *TempFilename, true, (WriteFlags & FILEWRITE_EvenIfReadOnly) != 0))
		{
			FileManager->Delete(*TempFilename, false, true, true);
			return false;
		}
		return true;
	}
} // namespace

bool FFileHelper::LoadFileToArray(TArray<uint8>& Result, const TCHAR* Filename, uint32 Flags)
{
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(Filename, Flags));
	if (!Reader)
	{
		return false;
	}

	const int64 TotalSize = Reader->TotalSize();
	if (TotalSize < 0 || TotalSize >= MAX_int32)
	{
		return false;
	}

	Result.Reset(int32(TotalSize));
	Result.AddUninitialized(int32(TotalSize));
	Reader->Serialize(Result.GetData(), TotalSize);
	const bool bSuccess = Reader->Close();
	return bSuccess;
}

void FFileHelper::BufferToString(FString& Result, const uint8* Buffer, int32 Size)
{
	Result.Empty();

	if (Size >= 2 && !(Size & 1) && Buffer[0] == 0xff && Buffer[1] == 0xfe)
	{
		// Unicode Intel byte order.
		Utf16ToString(Result, Buffer + 2, Size - 2, false);
	}
	else if (Size >= 2 && !(Size & 1) && Buffer[0] == 0xfe && Buffer[1] == 0xff)
	{
		// Unicode non-Intel byte order.
		Utf16ToString(Result, Buffer + 2, Size - 2, true);
	}
	else
	{
		if (Size >= 3 && Buffer[0] == 0xef && Buffer[1] == 0xbb && Buffer[2] == 0xbf)
		{
			// Skip the UTF-8 BOM.
			Buffer += 3;
			Size -= 3;
		}
		Result = FString(Size, reinterpret_cast<const TCHAR*>(Buffer));
	}

	// Stop at the first terminator, like UE.
	const int32 NullIndex = FCString::Strlen(*Result);
	if (NullIndex < Result.Len())
	{
		Result = Result.Mid(0, NullIndex);
	}
}

bool FFileHelper::LoadFileToString(FString& Result, const TCHAR* Filename, uint32 ReadFlags)
{
	TArray<uint8> Bytes;
	if (!LoadFileToArray(Bytes, Filename, ReadFlags))
	{
		return false;
	}
	BufferToString(Result, Bytes.GetData(), Bytes.Num());
	return true;
}

bool FFileHelper::LoadFileToStringArray(TArray<FString>& Result, const TCHAR* Filename)
{
	FString Buffer;
	if (!LoadFileToString(Buffer, Filename))
	{
		return false;
	}
	Result.Empty();
	Buffer.ParseIntoArrayLines(Result, false);
	return true;
}

bool FFileHelper::SaveArrayToFile(
	TArrayView<const uint8> Array, const TCHAR* Filename, IFileManager* FileManager, uint32 WriteFlags)
{
	return WriteBytes(Array.GetData(), Array.Num(), Filename, FileManager, WriteFlags);
}

bool FFileHelper::SaveStringToFile(const FString& String, const TCHAR* Filename, EEncodingOptions EncodingOptions,
	IFileManager* FileManager, uint32 WriteFlags)
{
	TArray<uint8> Bytes;
	if (EncodingOptions == EEncodingOptions::ForceUTF8)
	{
		Bytes.Add(0xef);
		Bytes.Add(0xbb);
		Bytes.Add(0xbf);
	}
	Bytes.Append(reinterpret_cast<const uint8*>(*String), String.Len());
	return WriteBytes(Bytes.GetData(), Bytes.Num(), Filename, FileManager, WriteFlags);
}

bool FFileHelper::SaveStringArrayToFile(const TArray<FString>& Lines, const TCHAR* Filename,
	EEncodingOptions EncodingOptions, IFileManager* FileManager, uint32 WriteFlags)
{
	FString CombinedString;
	for (const FString& Line : Lines)
	{
		CombinedString += Line;
		CombinedString += LINE_TERMINATOR;
	}
	return SaveStringToFile(CombinedString, Filename, EncodingOptions, FileManager, WriteFlags);
}
