#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "UObject/NameTypes.h"

class FOutputDevice;
struct FColor;
struct FLinearColor;
struct FRotator;
struct FVector;

/** One value of a config key (UE: FConfigValue). Leon does not expand %GAME% style variables. */
struct FConfigValue
{
	FConfigValue() = default;

	FConfigValue(const TCHAR* InValue)
		: SavedValue(InValue)
	{
	}

	FConfigValue(const FString& InValue)
		: SavedValue(InValue)
	{
	}

	FConfigValue(FString&& InValue)
		: SavedValue(MoveTemp(InValue))
	{
	}

	const FString& GetValue() const
	{
		return SavedValue;
	}

	const FString& GetSavedValue() const
	{
		return SavedValue;
	}

	bool operator==(const FConfigValue& Other) const
	{
		return SavedValue.Equals(Other.SavedValue, ESearchCase::CaseSensitive);
	}

	bool operator!=(const FConfigValue& Other) const
	{
		return !(*this == Other);
	}

private:
	FString SavedValue;
};

typedef TMultiMap<FName, FConfigValue> FConfigSectionMap;

/** Keys of one [Section]; a key may hold several values (arrays written as +Key=Value) (UE: FConfigSection). */
class CORE_API FConfigSection : public FConfigSectionMap
{
public:
	bool operator==(const FConfigSection& Other) const;
	bool operator!=(const FConfigSection& Other) const
	{
		return !(*this == Other);
	}

	/** +Key=Value (unique) or .Key=Value (bAppendValueIfNotArrayOfStructsKeyUsed: always added) (UE: HandleAddCommand).
	 */
	void HandleAddCommand(FName Key, FString&& Value, bool bAppendValueIfNotArrayOfStructsKeyUsed);
};

/**
 * The sections of one config file, or of a whole hierarchy combined (UE: FConfigFile). Section names ignore case.
 *
 * Syntax (UE): "[Section]", "Key=Value" (sets the first value), "+Key=Value" (adds when missing), ".Key=Value" (adds
 * even when present), "-Key=Value" (removes that value), "!Key" (removes every value). Lines starting with ';' or '#'
 * are comments; a value in double quotes may hold escapes (\n, \", \\).
 */
class CORE_API FConfigFile : public TMap<FString, FConfigSection>
{
public:
	/** Unsaved changes. */
	bool Dirty = false;

	/** Never written back (engine and project files are read-only; only the user layer is saved). */
	bool NoSave = false;

	/** Short name: "Engine", "Game", ... */
	FName Name;

	/** Replaces the contents with the file's (UE: Read). */
	void Read(const FString& Filename);

	/** Applies the file on top of the contents (UE: Combine). False when the file does not exist. */
	bool Combine(const FString& Filename);

	/** Applies ini text on top of the contents (UE: CombineFromBuffer). */
	void CombineFromBuffer(const FString& Buffer);

	/** Replaces the contents with ini text (UE: ProcessInputFileContents). */
	void ProcessInputFileContents(const FString& Contents);

	/** Writes the sections as ini text (UE: Write). */
	bool Write(const FString& Filename) const;

	/** The file as ini text. */
	FString ToIniString() const;

	void Dump(FOutputDevice& Ar) const;

	bool GetString(const TCHAR* Section, const TCHAR* Key, FString& Value) const;
	bool GetInt(const TCHAR* Section, const TCHAR* Key, int32& Value) const;
	bool GetInt64(const TCHAR* Section, const TCHAR* Key, int64& Value) const;
	bool GetFloat(const TCHAR* Section, const TCHAR* Key, float& Value) const;
	bool GetBool(const TCHAR* Section, const TCHAR* Key, bool& Value) const;

	/** Every value of the key in file order (UE: GetArray). */
	int32 GetArray(const TCHAR* Section, const TCHAR* Key, TArray<FString>& Value) const;

	void SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value);
	void SetInt64(const TCHAR* Section, const TCHAR* Key, const int64 Value);

	/** Replaces every value of the key (UE: SetArray). */
	void SetArray(const TCHAR* Section, const TCHAR* Key, const TArray<FString>& Value);

	/** Sets values from "-ini:<Name>:[Section]:Key=Value" command line switches (UE: OverrideFromCommandline). */
	void OverrideFromCommandline(const TCHAR* CommandLine);
};

/**
 * All loaded config files, keyed by their file name (GEngineIni and friends) (UE: FConfigCacheIni). Each global file
 * is a hierarchy combined at load time:
 *   Engine/Config/Base.ini, Engine/Config/Base<Name>.ini,
 *   Engine/Config/<Platform>/<Platform><Name>.ini and Engine/Platforms/<Platform>/Config/<Platform><Name>.ini,
 *   <Project>/Config/Default<Name>.ini,
 *   <Project>/Config/<Platform>/<Platform><Name>.ini and <Project>/Platforms/<Platform>/Config/<Platform><Name>.ini,
 *   <Project>/Saved/Config/<Platform>/<Name>.ini (the user layer, desktop only),
 * then "-ini:" command line overrides.
 */
class CORE_API FConfigCacheIni : public TMap<FString, FConfigFile>
{
public:
	/** The combined file, or nullptr when not loaded (UE: FindConfigFile). */
	FConfigFile* FindConfigFile(const FString& Filename);
	const FConfigFile* FindConfigFile(const FString& Filename) const;

	/** The file, loaded (or created empty with CreateIfNotFound) when missing (UE: Find). */
	FConfigFile* Find(const FString& InFilename, bool bCreateIfNotFound = false);

	/** Writes the user layer of dirty files (UE: Flush). bRead drops them from the cache afterwards. */
	void Flush(bool bRead, const FString& Filename = FString());

	bool GetString(const TCHAR* Section, const TCHAR* Key, FString& Value, const FString& Filename);
	bool GetInt(const TCHAR* Section, const TCHAR* Key, int32& Value, const FString& Filename);
	bool GetFloat(const TCHAR* Section, const TCHAR* Key, float& Value, const FString& Filename);
	bool GetBool(const TCHAR* Section, const TCHAR* Key, bool& Value, const FString& Filename);
	int32 GetArray(const TCHAR* Section, const TCHAR* Key, TArray<FString>& Value, const FString& Filename);

	/** "X=... Y=... Z=..." values (UE: GetVector / GetRotator / GetColor). */
	bool GetVector(const TCHAR* Section, const TCHAR* Key, FVector& Value, const FString& Filename);
	bool GetRotator(const TCHAR* Section, const TCHAR* Key, FRotator& Value, const FString& Filename);
	bool GetColor(const TCHAR* Section, const TCHAR* Key, FColor& Value, const FString& Filename);

	/** "Key=Value" lines of a section (UE: GetSection). */
	bool GetSection(const TCHAR* Section, TArray<FString>& Result, const FString& Filename);
	bool DoesSectionExist(const TCHAR* Section, const FString& Filename);

	/** Sets a value; on a writable file it also goes to the user layer that Flush saves (UE: SetString). */
	void SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const FString& Filename);
	void SetInt(const TCHAR* Section, const TCHAR* Key, int32 Value, const FString& Filename);
	void SetFloat(const TCHAR* Section, const TCHAR* Key, float Value, const FString& Filename);
	void SetBool(const TCHAR* Section, const TCHAR* Key, bool Value, const FString& Filename);
	void SetArray(const TCHAR* Section, const TCHAR* Key, const TArray<FString>& Value, const FString& Filename);

	bool RemoveKey(const TCHAR* Section, const TCHAR* Key, const FString& Filename);
	bool EmptySection(const TCHAR* Section, const FString& Filename);

	void GetConfigFilenames(TArray<FString>& ConfigFilenames) const;

	/** Logs every section of one file (or all files) (UE: Dump). */
	void Dump(FOutputDevice& Ar, const TCHAR* IniName = nullptr);

	/** Creates GConfig and loads Engine, Game, Input and Editor (UE: InitializeConfigSystem). */
	static void InitializeConfigSystem();

	/**
	 * Loads a global file into ConfigSystem under GetDestIniFilename (UE: LoadGlobalIniFile). OutFinalIniFilename
	 * receives the key to pass to GetString and friends.
	 */
	static bool LoadGlobalIniFile(FString& OutFinalIniFilename, const TCHAR* BaseIniName,
		const TCHAR* Platform = nullptr, bool bForceReload = false, FConfigCacheIni* ConfigSystem = nullptr);

	/** Loads a hierarchy into ConfigFile without caching it (UE: LoadLocalIniFile). */
	static bool LoadLocalIniFile(
		FConfigFile& ConfigFile, const TCHAR* IniName, bool bIsBaseIniName, const TCHAR* Platform = nullptr);

	/** Files making up a hierarchy, lowest first; the last one is the user layer (Leon helper, used by tests). */
	static TArray<FString> GetHierarchy(const TCHAR* BaseIniName, const TCHAR* Platform);

	/** <Project>/Saved/Config/<Platform>/<BaseIniName>.ini (UE: GetDestIniFilename). */
	static FString GetDestIniFilename(const TCHAR* BaseIniName, const TCHAR* PlatformName);

private:
	FConfigSection* GetSectionPrivate(const TCHAR* Section, bool bForce, const FString& Filename);

	/** Values set on a writable file since load, per file: the user layer that Flush merges into the saved file. */
	TMap<FString, FConfigFile> PendingUserChanges;
};

/** The global config (UE: GConfig). Null until FEngineLoop::PreInit (or a program) calls InitializeConfigSystem. */
extern CORE_API FConfigCacheIni* GConfig;

/** Keys of the global files in GConfig (UE: GEngineIni and friends). */
extern CORE_API FString GEngineIni;
extern CORE_API FString GGameIni;
extern CORE_API FString GInputIni;
extern CORE_API FString GEditorIni;
