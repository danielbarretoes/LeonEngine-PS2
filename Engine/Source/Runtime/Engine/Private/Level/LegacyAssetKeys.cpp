#include "Level/LegacyAssetKeys.h"

#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{

	/** Letters, digits and '_' kept in each folder and the leaf; other characters become '_'. */
	FString SanitizePath(const FString& Path)
	{
		FString Out;
		Out.Reserve(Path.Len());
		for (int32 Index = 0; Index < Path.Len(); ++Index)
		{
			const TCHAR Char = Path[Index];
			Out.AppendChar(FChar::IsAlnum(Char) || Char == '_' || Char == '/' ? Char : '_');
		}
		return Out;
	}

	/** "/Root" + "/" + Path, or empty for an invalid package name. */
	FString JoinPackageName(const FString& Root, const FString& Path)
	{
		const FString Name = Root + TEXT("/") + Path;
		return FPackageName::IsValidLongPackageName(Name) ? Name : FString();
	}

	/** A full folder path with '/' separators and a trailing '/'. */
	FString NormalizeDirectory(const FString& Directory)
	{
		FString Full = FPaths::ConvertRelativePathToFull(Directory);
		FPaths::NormalizeFilename(Full);
		FPaths::CollapseRelativeDirectories(Full);
		if (!Full.EndsWith(TEXT("/")))
		{
			Full += TEXT("/");
		}
		return Full;
	}

} // namespace

FString FLegacyAssetKeys::GetPrefixForExtension(const FString& Extension)
{
	FString Ext = Extension;
	Ext.RemoveFromStart(TEXT("."));
	if (Ext == TEXT("lmat"))
	{
		return TEXT("M_");
	}
	if (Ext == TEXT("lmesh"))
	{
		return TEXT("SM_");
	}
	if (Ext == TEXT("png") || Ext == TEXT("jpg") || Ext == TEXT("jpeg") || Ext == TEXT("tga") || Ext == TEXT("bmp"))
	{
		return TEXT("T_");
	}
	if (Ext == TEXT("wav"))
	{
		return TEXT("S_");
	}
	return FString();
}

FString FLegacyAssetKeys::NormalizeKey(const FString& Key)
{
	FString Path = Key;
	Path.ReplaceCharInline('\\', '/');
	while (Path.StartsWith(TEXT("./")))
	{
		Path.RemoveFromStart(TEXT("./"));
	}
	Path.RemoveFromStart(TEXT("assets/"), ESearchCase::CaseSensitive);
	Path.RemoveFromStart(TEXT("/"));
	const FString Folder = FPaths::GetPath(Path);
	const FString Leaf = FPaths::GetBaseFilename(Path);
	return Folder.IsEmpty() ? Leaf : Folder + TEXT("/") + Leaf;
}

FString FLegacyAssetKeys::GetMigratedPackageName(const FString& ContentRootPath, const FString& Key)
{
	FString Path = NormalizeKey(Key);
	FString Folder = FPaths::GetPath(Path);
	FString Leaf = FPaths::GetCleanFilename(Path);
	const FString Prefix = GetPrefixForExtension(FPaths::GetExtension(Key));
	if (!Prefix.IsEmpty() && !Leaf.StartsWith(Prefix, ESearchCase::CaseSensitive))
	{
		Leaf = Prefix + Leaf;
	}
	// The engine's legacy content folders became UE's.
	if (ContentRootPath == TEXT("/Engine"))
	{
		FString First = Folder;
		FString Rest;
		int32 Slash = INDEX_NONE;
		if (Folder.FindChar('/', Slash))
		{
			First = Folder.Left(Slash);
			Rest = Folder.Mid(Slash);
		}
		if (First == TEXT("Materials") || First == TEXT("Textures"))
		{
			Folder = FString(TEXT("EngineMaterials")) + Rest;
		}
	}
	return JoinPackageName(ContentRootPath, SanitizePath(Folder.IsEmpty() ? Leaf : Folder + TEXT("/") + Leaf));
}

void FLegacyAssetKeys::GetCandidatePackageNames(
	const FString& ContentRootPath, const FString& Key, TArray<FString>& OutPackageNames)
{
	FString Root = ContentRootPath;
	FString RelativeKey = Key;
	// An absolute key names a file: its folder is the content root.
	if (!FPaths::IsRelative(Key))
	{
		Root = MountContentDirectory(FPaths::GetPath(Key));
		RelativeKey = FPaths::GetCleanFilename(Key);
	}
	TArray<FString> Roots;
	for (const FString& Candidate : {Root, FString(TEXT("/Game")), FString(TEXT("/Engine"))})
	{
		if (!Candidate.IsEmpty() && !Roots.Contains(Candidate))
		{
			Roots.Add(Candidate);
		}
	}
	for (const FString& CandidateRoot : Roots)
	{
		for (const FString& Name : {GetMigratedPackageName(CandidateRoot, RelativeKey),
				 JoinPackageName(CandidateRoot, NormalizeKey(RelativeKey))})
		{
			if (!Name.IsEmpty() && !OutPackageNames.Contains(Name))
			{
				OutPackageNames.Add(Name);
			}
		}
	}
}

FString FLegacyAssetKeys::ResolveKey(const FString& ContentRootPath, const FString& Key)
{
	if (Key.IsEmpty())
	{
		return FString();
	}
	TArray<FString> Candidates;
	GetCandidatePackageNames(ContentRootPath, Key, Candidates);
	for (const FString& PackageName : Candidates)
	{
		const FString ObjectPath = PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
		if (FindObject<UObject>(nullptr, *ObjectPath) != nullptr || FPackageName::DoesPackageExist(PackageName))
		{
			return ObjectPath;
		}
	}
	return FString();
}

FString FLegacyAssetKeys::MountContentDirectory(const FString& ContentDir)
{
	const FString Directory = NormalizeDirectory(ContentDir);
	// A folder under a mount point: the package path of a file in it, without the file.
	FString PackageName;
	if (FPackageName::TryConvertFilenameToLongPackageName(Directory + TEXT("X"), PackageName))
	{
		return FPackageName::GetLongPackagePath(PackageName);
	}
	FString Leaf = SanitizePath(FPaths::GetPathLeaf(Directory.LeftChop(1)));
	Leaf.ReplaceCharInline('/', '_');
	if (Leaf.IsEmpty() || !FChar::IsAlpha(Leaf[0]))
	{
		Leaf = TEXT("Content_") + Leaf;
	}
	FString Root = TEXT("/") + Leaf;
	for (int32 Suffix = 2; FPackageName::MountPointExists(Root + TEXT("/")); ++Suffix)
	{
		Root = FString::Printf(TEXT("/%s_%d"), *Leaf, Suffix);
	}
	FPackageName::RegisterMountPoint(Root + TEXT("/"), Directory);
	return Root;
}
