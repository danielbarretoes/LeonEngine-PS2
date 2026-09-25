#include "UObject/SoftObjectPath.h"

#include "Misc/PackageName.h"
#include "UObject/Object.h"
#include "UObject/PropertyHelpers.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoftObjectPath, Log, All);

int32 FSoftObjectPath::CurrentTag = 1;

namespace
{
	/** "Class'/Game/Path.Asset'" (an exported object reference) to "/Game/Path.Asset" (UE: ExportTextPathToObjectPath).
	 */
	FString ExportTextPathToObjectPath(const FString& Path)
	{
		int32 QuoteIndex = INDEX_NONE;
		if (Path.Len() > 1 && Path[Path.Len() - 1] == '\'' && Path.FindChar('\'', QuoteIndex) &&
			QuoteIndex < Path.Len() - 1)
		{
			return Path.Mid(QuoteIndex + 1, Path.Len() - QuoteIndex - 2);
		}
		return Path;
	}
} // namespace

FSoftObjectPath::FSoftObjectPath(const UObject* InObject)
{
	if (InObject)
	{
		SetPath(InObject->GetPathName());
	}
}

FString FSoftObjectPath::ToString() const
{
	if (IsNull())
	{
		return FString();
	}
	FString Result = AssetPathName.ToString();
	if (SubPathString.Len() > 0)
	{
		Result += SUBOBJECT_DELIMITER;
		Result += SubPathString;
	}
	return Result;
}

FString FSoftObjectPath::GetAssetPathString() const
{
	return IsNull() ? FString() : AssetPathName.ToString();
}

FString FSoftObjectPath::GetLongPackageName() const
{
	const FString AssetPath = GetAssetPathString();
	int32 DotIndex = INDEX_NONE;
	return AssetPath.FindChar('.', DotIndex) ? AssetPath.Left(DotIndex) : AssetPath;
}

FString FSoftObjectPath::GetAssetName() const
{
	const FString AssetPath = GetAssetPathString();
	int32 DotIndex = INDEX_NONE;
	return AssetPath.FindChar('.', DotIndex) ? AssetPath.RightChop(DotIndex + 1) : FString();
}

void FSoftObjectPath::SetPath(const FString& InPath)
{
	if (InPath.Len() == 0 || InPath.Equals(TEXT("None")))
	{
		Reset();
		return;
	}
	// An exported reference ("Class'/Game/Path.Asset'") names its object between the quotes (UE).
	const FString Path =
		(InPath[0] != '/' || InPath[InPath.Len() - 1] == '\'') ? ExportTextPathToObjectPath(InPath) : InPath;
	// "Package.Asset:Sub.Path": the asset path is up to the first ':' (UE: SUBOBJECT_DELIMITER).
	int32 ColonIndex = INDEX_NONE;
	if (Path.FindChar(':', ColonIndex))
	{
		AssetPathName = FName(*Path.Left(ColonIndex));
		SubPathString = Path.RightChop(ColonIndex + 1);
	}
	else
	{
		AssetPathName = FName(*Path);
		SubPathString.Empty();
	}
}

UObject* FSoftObjectPath::ResolveObject() const
{
	if (IsNull())
	{
		return nullptr;
	}
	return StaticFindObject(nullptr, nullptr, *ToString());
}

UObject* FSoftObjectPath::TryLoad() const
{
	UObject* Object = ResolveObject();
	if (!Object && IsValid())
	{
		// Loads the package, then finds the object (and warns when it is not there) (UE: LoadObject).
		Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ToString());
	}
	return Object;
}

bool FSoftObjectPath::Serialize(FArchive& Ar)
{
	SerializePath(Ar);
	return true;
}

void FSoftObjectPath::SerializePath(FArchive& Ar)
{
	if (Ar.IsSaving() && !IsNull())
	{
		if (TArray<FName>* SoftPackageReferences = FUObjectThreadContext::Get().SoftPackageReferenceCollector)
		{
			const FString PackageName = GetLongPackageName();
			if (!FPackageName::IsScriptPackage(PackageName))
			{
				SoftPackageReferences->AddUnique(FName(*PackageName));
			}
		}
	}
	Ar << AssetPathName;
	Ar << SubPathString;
}

bool FSoftObjectPath::ExportTextItem(FString& ValueStr, const FSoftObjectPath& DefaultValue, UObject* Parent,
	int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	(void)Parent;
	(void)ExportRootScope;
	const FString Path = ToString();
	if (PortFlags & PPF_Delimited)
	{
		UE::CoreUObject::Private::AppendQuoted(ValueStr, Path);
	}
	else
	{
		ValueStr += Path;
	}
	return true;
}

bool FSoftObjectPath::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	(void)PortFlags;
	(void)Parent;
	const TCHAR* Start = UE::CoreUObject::Private::SkipWhitespace(Buffer);
	// The generic struct form is for FStructProperty's own parser (UE falls back to it too).
	if (*Start == '(')
	{
		return false;
	}
	FString ImportedPath;
	bool bQuoted = false;
	const TCHAR* End = UE::CoreUObject::Private::ReadToken(Start, ImportedPath, bQuoted);
	if (!End)
	{
		UE::CoreUObject::Private::ReportImportError(ErrorText, TEXT("Soft object path: missing closing '\"'"));
		return false;
	}
	SetPath(ImportedPath);
	Buffer = End;
	return true;
}

FSoftObjectPath FSoftObjectPath::GetOrCreateIDForObject(const UObject* Object)
{
	return FSoftObjectPath(Object);
}

// FSoftClassPath

UClass* FSoftClassPath::ResolveClass() const
{
	return Cast<UClass>(ResolveObject());
}

FSoftClassPath FSoftClassPath::GetOrCreateIDForClass(const UClass* InClass)
{
	return FSoftClassPath(InClass);
}

// FSoftObjectPtr

UObject* FSoftObjectPtr::LoadSynchronous() const
{
	if (UObject* Object = Get())
	{
		return Object;
	}
	return ToSoftObjectPath().TryLoad();
}
