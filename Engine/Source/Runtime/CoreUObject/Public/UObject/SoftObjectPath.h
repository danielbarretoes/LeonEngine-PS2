#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class UObject;

/**
 * The path of an object that may not be loaded: "/Game/Maps/Arena.Arena" plus an optional subobject path
 * (UE: FSoftObjectPath). Minimal until P10 / P11: it resolves objects that already exist (StaticFindObject) and does
 * not load packages; it is not a reflected USTRUCT yet.
 */
struct COREUOBJECT_API FSoftObjectPath
{
	FSoftObjectPath() = default;

	explicit FSoftObjectPath(const FString& Path)
	{
		SetPath(Path);
	}

	explicit FSoftObjectPath(const UObject* Object);

	/** "<AssetPathName>[:<SubPathString>]" (UE). */
	FString ToString() const;

	FORCEINLINE FName GetAssetPathName() const
	{
		return AssetPathName;
	}

	FORCEINLINE const FString& GetSubPathString() const
	{
		return SubPathString;
	}

	/** Splits "Package.Object:Sub.Path" into the asset path and the subobject path (UE). */
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

	/** The object if it exists in memory, else nullptr (UE; P11 adds TryLoad). */
	UObject* ResolveObject() const;

	FORCEINLINE bool operator==(const FSoftObjectPath& Other) const
	{
		return AssetPathName == Other.AssetPathName && SubPathString == Other.SubPathString;
	}

	FORCEINLINE bool operator!=(const FSoftObjectPath& Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE friend uint32 GetTypeHash(const FSoftObjectPath& Path)
	{
		return HashCombine(GetTypeHash(Path.AssetPathName), GetTypeHash(Path.SubPathString));
	}

private:
	/** The package and top-level object: "/Game/Maps/Arena.Arena". */
	FName AssetPathName;
	/** The path below that object, or empty. */
	FString SubPathString;
};
