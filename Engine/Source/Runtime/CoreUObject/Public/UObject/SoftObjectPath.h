#pragma once

#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"

class FOutputDevice;
class UObject;

/**
 * The path of an object that may not be loaded: the package and top-level asset, "/Game/Maps/Arena.Arena", plus the
 * path of a subobject below it, "PersistentLevel.Door" (UE: FSoftObjectPath, 4.27 layout). Reflected as a noexport
 * struct (UObject/NoExportTypes.h) whose text form is the path itself: "/Game/Maps/Arena.Arena", or
 * "/Game/Maps/Arena.Arena:PersistentLevel.Door". TryLoad loads the package when the object is not in memory. In a
 * package it is saved as its asset path name (a name table entry) and subobject string, and the package records the
 * soft reference in its soft package references table.
 */
struct COREUOBJECT_API FSoftObjectPath
{
	FSoftObjectPath() = default;
	FSoftObjectPath(const FSoftObjectPath& Other) = default;
	FSoftObjectPath(FSoftObjectPath&& Other) = default;
	FSoftObjectPath& operator=(const FSoftObjectPath& Other) = default;
	FSoftObjectPath& operator=(FSoftObjectPath&& Other) = default;

	/** From a path string; "Class'/Game/Path.Asset'" and "None" are accepted (UE). */
	FSoftObjectPath(const FString& Path)
	{
		SetPath(Path);
	}

	FSoftObjectPath(const TCHAR* Path)
	{
		SetPath(FString(Path));
	}

	FSoftObjectPath(FName InAssetPathName, const FString& InSubPathString)
		: AssetPathName(InAssetPathName)
		, SubPathString(InSubPathString)
	{
	}

	/** The path of an object in memory (its GetPathName). */
	FSoftObjectPath(const UObject* InObject);

	/** "<AssetPathName>[:<SubPathString>]", or an empty string for a null path (UE). */
	FString ToString() const;

	/** "/Game/Maps/Arena.Arena" (UE). */
	FORCEINLINE FName GetAssetPathName() const
	{
		return AssetPathName;
	}

	FORCEINLINE void SetAssetPathName(FName InAssetPathName)
	{
		AssetPathName = InAssetPathName;
	}

	/** GetAssetPathName as a string, empty for a null path (UE). */
	FString GetAssetPathString() const;

	/** The path below the asset, or empty (UE). */
	FORCEINLINE const FString& GetSubPathString() const
	{
		return SubPathString;
	}

	FORCEINLINE void SetSubPathString(const FString& InSubPathString)
	{
		SubPathString = InSubPathString;
	}

	/** The package: "/Game/Maps/Arena" (UE). */
	FString GetLongPackageName() const;

	/** The asset: "Arena" (UE). */
	FString GetAssetName() const;

	/** Splits "Package.Asset:Sub.Path" into the asset path and the subobject path (UE). */
	void SetPath(const FString& Path);

	FORCEINLINE void Reset()
	{
		AssetPathName = NAME_None;
		SubPathString.Empty();
	}

	FORCEINLINE bool IsNull() const
	{
		return AssetPathName.IsNone();
	}

	FORCEINLINE bool IsValid() const
	{
		return !IsNull();
	}

	/** A top-level asset: no subobject path (UE). */
	FORCEINLINE bool IsAsset() const
	{
		return !IsNull() && SubPathString.IsEmpty();
	}

	/** A subobject of an asset (UE). */
	FORCEINLINE bool IsSubobject() const
	{
		return !IsNull() && !SubPathString.IsEmpty();
	}

	/** The object if it exists in memory, else nullptr (UE). */
	UObject* ResolveObject() const;

	/**
	 * The object, loading its package (LoadObject) when it is not in memory; nullptr, with a warning, when it cannot
	 * be found (UE).
	 */
	UObject* TryLoad() const;

	/** Loads or saves the path (UE: Serialize, through the struct's WithSerializer trait). */
	bool Serialize(FArchive& Ar);

	/**
	 * The asset path name, then the subobject string (UE: SerializePath). While UPackage::Save collects its package's
	 * references, a saved path adds its package to the soft package references.
	 */
	void SerializePath(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSoftObjectPath& Value)
	{
		Value.Serialize(Ar);
		return Ar;
	}

	FORCEINLINE bool operator==(const FSoftObjectPath& Other) const
	{
		return AssetPathName == Other.AssetPathName && SubPathString == Other.SubPathString;
	}

	FORCEINLINE bool operator!=(const FSoftObjectPath& Other) const
	{
		return !(*this == Other);
	}

	/** The text form: the path, quoted when PPF_Delimited; empty for a null path (UE). */
	bool ExportTextItem(FString& ValueStr, const FSoftObjectPath& DefaultValue, UObject* Parent, int32 PortFlags,
		UObject* ExportRootScope) const;

	/**
	 * Reads a path, "None", a quoted path or "Class'Path'" from Buffer and advances it; false (Buffer untouched) for
	 * anything else, such as the generic "(AssetPathName=...,SubPathString=...)" struct form (UE).
	 */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	/** The path of Object (UE). */
	static FSoftObjectPath GetOrCreateIDForObject(const UObject* Object);

	/**
	 * A counter bumped whenever objects appear, so soft pointers that failed to resolve try again (UE: bumped when
	 * packages load; Leon also bumps it when any object is created).
	 */
	FORCEINLINE static int32 GetCurrentTag()
	{
		return CurrentTag;
	}

	FORCEINLINE static int32 InvalidateTag()
	{
		return ++CurrentTag;
	}

	FORCEINLINE friend uint32 GetTypeHash(const FSoftObjectPath& Path)
	{
		return HashCombine(GetTypeHash(Path.AssetPathName), GetTypeHash(Path.SubPathString));
	}

private:
	friend struct Z_Construct_UScriptStruct_FSoftObjectPath_Statics;

	/** The package and top-level object: "/Game/Maps/Arena.Arena". */
	FName AssetPathName;
	/** The path below that object, or empty. */
	FString SubPathString;

	static int32 CurrentTag;
};

/** A soft path to a class, "/Script/Engine.Actor" (UE: FSoftClassPath). Reflected as a noexport struct too. */
struct COREUOBJECT_API FSoftClassPath : public FSoftObjectPath
{
	FSoftClassPath() = default;
	FSoftClassPath(const FSoftClassPath& Other) = default;
	FSoftClassPath& operator=(const FSoftClassPath& Other) = default;

	FSoftClassPath(const FString& PathString)
		: FSoftObjectPath(PathString)
	{
	}

	FSoftClassPath(const TCHAR* PathString)
		: FSoftObjectPath(PathString)
	{
	}

	FSoftClassPath(const UClass* InClass)
		: FSoftObjectPath((const UObject*)InClass)
	{
	}

	explicit FSoftClassPath(const FSoftObjectPath& Other)
		: FSoftObjectPath(Other)
	{
	}

	/** The class, loaded when needed, if it is a T; else nullptr (UE). */
	template <typename T>
	UClass* TryLoadClass() const
	{
		UClass* Class = Cast<UClass>(TryLoad());
		return Class && Class->IsChildOf(T::StaticClass()) ? Class : nullptr;
	}

	/** The class if it is in memory, else nullptr (UE). */
	UClass* ResolveClass() const;

	static FSoftClassPath GetOrCreateIDForClass(const UClass* InClass);

private:
	friend struct Z_Construct_UScriptStruct_FSoftClassPath_Statics;
};

template <>
struct TStructOpsTypeTraits<FSoftObjectPath> : public TStructOpsTypeTraitsBase2<FSoftObjectPath>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializer = true,
	};
};

template <>
struct TStructOpsTypeTraits<FSoftClassPath> : public TStructOpsTypeTraitsBase2<FSoftClassPath>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializer = true,
	};
};
