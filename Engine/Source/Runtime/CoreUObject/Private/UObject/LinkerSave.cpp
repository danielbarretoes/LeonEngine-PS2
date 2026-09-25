#include "UObject/LinkerSave.h"

#include "UObject/Object.h"

FLinkerSave::FLinkerSave(UPackage* InParent, const TCHAR* InFilename, bool bInFilterEditorOnly)
	: FLinker(ELinkerType::Save, InParent, InFilename)
{
	SetIsSaving(true);
	SetIsPersistent(true);
	SetFilterEditorOnly(bInFilterEditorOnly);
	SetUEVer(VER_LEON_LATEST);
	SetLicenseeUEVer(VER_LEON_LATEST_LICENSEE);
}

FLinkerSave::~FLinkerSave()
{
}

FPackageIndex FLinkerSave::MapObject(const UObject* Object) const
{
	if (Object)
	{
		if (const FPackageIndex* Found = ObjectIndicesMap.Find(const_cast<UObject*>(Object)))
		{
			return *Found;
		}
	}
	return FPackageIndex();
}

void FLinkerSave::Serialize(void* V, int64 Length)
{
	if (Length <= 0)
	{
		return;
	}
	const int64 End = Pos + Length;
	if (End > MAX_int32)
	{
		SetCriticalError();
		return;
	}
	if (End > Bytes.Num())
	{
		Bytes.AddUninitialized(int32(End - Bytes.Num()));
	}
	FMemory::Memcpy(Bytes.GetData() + Pos, V, SIZE_T(Length));
	Pos = End;
}

int64 FLinkerSave::Tell()
{
	return Pos;
}

int64 FLinkerSave::TotalSize()
{
	return Bytes.Num();
}

void FLinkerSave::Seek(int64 InPos)
{
	check(InPos >= 0 && InPos <= Bytes.Num());
	Pos = InPos;
}

FArchive& FLinkerSave::operator<<(FName& Name)
{
	const int32* Index = NameIndices.Find(Name.GetComparisonIndex());
	if (!Index)
	{
		// UPackage::Save collects every name its exports serialize before writing them: a name missing here is a
		// Serialize that writes something else the second time (UE: a fatal error too).
		UE_LOG(LogLinker, Fatal,
			TEXT("%s: the name '%s' is not in the name table (a Serialize that is not deterministic?)"), *Filename,
			*Name.ToString());
		return *this;
	}
	int32 NameIndex = *Index;
	int32 Number = Name.GetNumber();
	*this << NameIndex << Number;
	return *this;
}

FArchive& FLinkerSave::operator<<(UObject*& Object)
{
	FPackageIndex Index = MapObject(Object);
	*this << Index;
	return *this;
}

FLinker* FLinkerSave::GetLinker()
{
	return this;
}

FString FLinkerSave::GetArchiveName() const
{
	return Filename;
}
