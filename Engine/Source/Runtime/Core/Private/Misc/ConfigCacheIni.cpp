#include "Misc/ConfigCacheIni.h"

#include "HAL/PlatformProperties.h"
#include "Logging/LogMacros.h"
#include "Math/Color.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Misc/CString.h"
#include "Misc/Char.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogConfig, Log, All);

FConfigCacheIni* GConfig = nullptr;
FString GEngineIni;
FString GGameIni;
FString GInputIni;
FString GEditorIni;

namespace
{
	/** A value as it must be written so that reading it back gives the same text (quotes when needed). */
	FString QuoteIfNeeded(const FString& Value)
	{
		const bool bNeedsQuotes = !Value.IsEmpty() &&
			(FChar::IsWhitespace(Value[0]) || FChar::IsWhitespace(Value[Value.Len() - 1]) || Value[0] == '"' ||
				Value.Contains("\n", ESearchCase::CaseSensitive) || Value.Contains("\r", ESearchCase::CaseSensitive));
		if (!bNeedsQuotes)
		{
			return Value;
		}

		FString Result("\"");
		for (const TCHAR C : Value)
		{
			switch (C)
			{
				case '"':
					Result += "\\\"";
					break;
				case '\\':
					Result += "\\\\";
					break;
				case '\n':
					Result += "\\n";
					break;
				case '\r':
					Result += "\\r";
					break;
				default:
					Result += C;
			}
		}
		Result += "\"";
		return Result;
	}

	/** A section's keys with values, then its array keys without any (sorted, so a file is written the same way). */
	TArray<FName> GetSectionKeys(const FConfigSection& Section)
	{
		TArray<FName> Keys;
		Section.GetKeys(Keys);
		TArray<FName> EmptyArrays;
		for (const FName& Key : Section.ArrayKeys)
		{
			if (!Keys.Contains(Key))
			{
				EmptyArrays.Add(Key);
			}
		}
		EmptyArrays.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
		Keys.Append(EmptyArrays);
		return Keys;
	}

	/** Applies one "Key=Value" line (with its command character) to a section (UE: CombineFromBuffer). */
	void ApplyLine(FConfigSection& Section, const FString& Line)
	{
		TCHAR Cmd = Line[0];
		int32 Start = 1;
		if (Cmd != '+' && Cmd != '-' && Cmd != '.' && Cmd != '!')
		{
			Cmd = ' ';
			Start = 0;
		}

		FString Key;
		FString Value;
		const int32 Equals = Line.Find("=", ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		if (Equals == INDEX_NONE)
		{
			if (Cmd != '!')
			{
				return;
			}
			Key = Line.Mid(Start).TrimStartAndEnd();
		}
		else
		{
			Key = Line.Mid(Start, Equals - Start).TrimStartAndEnd();
			Value = Line.Mid(Equals + 1).TrimStartAndEnd();
		}
		if (Key.IsEmpty())
		{
			return;
		}

		// A value in quotes may hold escapes and outer spaces.
		FString ProcessedValue;
		if (!Value.IsEmpty() && Value[0] == '"' && FParse::QuotedString(*Value, ProcessedValue))
		{
			Value = MoveTemp(ProcessedValue);
		}

		const FName KeyName(*Key);
		if (Cmd == ' ')
		{
			Section.ArrayKeys.Remove(KeyName);
		}
		else
		{
			Section.ArrayKeys.Add(KeyName);
		}
		switch (Cmd)
		{
			case '+':
				// Add if not already present.
				Section.HandleAddCommand(KeyName, MoveTemp(Value), false);
				break;
			case '.':
				// Add even when present.
				Section.HandleAddCommand(KeyName, MoveTemp(Value), true);
				break;
			case '-':
				// Remove if present.
				Section.RemoveSingle(KeyName, FConfigValue(Value));
				break;
			case '!':
				// Remove every value.
				Section.Remove(KeyName);
				break;
			default:
			{
				// Add if not present and replace if present.
				FConfigValue* ConfigValue = Section.Find(KeyName);
				if (ConfigValue == nullptr)
				{
					Section.Add(KeyName, FConfigValue(MoveTemp(Value)));
				}
				else
				{
					*ConfigValue = FConfigValue(MoveTemp(Value));
				}
				break;
			}
		}
	}
} // namespace

// FConfigSection ------------------------------------------------------------------------------------------------------

bool FConfigSection::operator==(const FConfigSection& Other) const
{
	if (Num() != Other.Num())
	{
		return false;
	}

	for (const auto& Pair : *this)
	{
		TArray<FConfigValue> MyValues;
		TArray<FConfigValue> OtherValues;
		MultiFind(Pair.Key, MyValues, true);
		Other.MultiFind(Pair.Key, OtherValues, true);
		if (MyValues != OtherValues)
		{
			return false;
		}
	}
	return true;
}

void FConfigSection::HandleAddCommand(FName Key, FString&& Value, bool bAppendValueIfNotArrayOfStructsKeyUsed)
{
	if (bAppendValueIfNotArrayOfStructsKeyUsed)
	{
		Add(Key, FConfigValue(MoveTemp(Value)));
	}
	else
	{
		AddUnique(Key, FConfigValue(MoveTemp(Value)));
	}
}

// FConfigFile ---------------------------------------------------------------------------------------------------------

void FConfigFile::Read(const FString& Filename)
{
	Empty();
	Combine(Filename);
}

bool FConfigFile::Combine(const FString& Filename)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Filename, FILEREAD_Silent))
	{
		return false;
	}
	CombineFromBuffer(Text);
	return true;
}

void FConfigFile::CombineFromBuffer(const FString& Buffer)
{
	FConfigSection* CurrentSection = nullptr;
	const TCHAR* Ptr = *Buffer;
	FString TheLine;
	while (FParse::Line(&Ptr, TheLine, true))
	{
		TheLine.TrimStartAndEndInline();
		if (TheLine.IsEmpty() || TheLine[0] == ';' || TheLine[0] == '#')
		{
			continue;
		}

		if (TheLine[0] == '[' && TheLine[TheLine.Len() - 1] == ']')
		{
			const FString SectionName = TheLine.Mid(1, TheLine.Len() - 2);
			CurrentSection = Find(SectionName);
			if (CurrentSection == nullptr)
			{
				CurrentSection = &Add(SectionName);
			}
			continue;
		}

		if (CurrentSection != nullptr)
		{
			ApplyLine(*CurrentSection, TheLine);
		}
	}
}

void FConfigFile::ProcessInputFileContents(const FString& Contents)
{
	Empty();
	CombineFromBuffer(Contents);
}

FString FConfigFile::ToIniString() const
{
	FString Text;
	for (const auto& SectionPair : *this)
	{
		const FConfigSection& Section = SectionPair.Value;
		Text += FString::Printf("[%s]" LINE_TERMINATOR, *SectionPair.Key);

		for (const FName& Key : GetSectionKeys(Section))
		{
			TArray<FConfigValue> Values;
			Section.MultiFind(Key, Values, true);
			const FString KeyString = Key.ToString();
			if (Values.Num() == 1 && !Section.ArrayKeys.Contains(Key))
			{
				Text += FString::Printf("%s=%s" LINE_TERMINATOR, *KeyString, *QuoteIfNeeded(Values[0].GetSavedValue()));
			}
			else
			{
				// Arrays replace whatever the lower layers had, duplicates and all ('.' adds even when present).
				Text += FString::Printf("!%s=ClearArray" LINE_TERMINATOR, *KeyString);
				for (const FConfigValue& Value : Values)
				{
					Text +=
						FString::Printf(".%s=%s" LINE_TERMINATOR, *KeyString, *QuoteIfNeeded(Value.GetSavedValue()));
				}
			}
		}
		Text += LINE_TERMINATOR;
	}
	return Text;
}

bool FConfigFile::Write(const FString& Filename) const
{
	return FFileHelper::SaveStringToFile(ToIniString(), *Filename);
}

void FConfigFile::Dump(FOutputDevice& Ar) const
{
	TArray<FString> Lines;
	ToIniString().ParseIntoArrayLines(Lines, false);
	for (const FString& Line : Lines)
	{
		Ar.Log(Line);
	}
}

bool FConfigFile::GetString(const TCHAR* Section, const TCHAR* Key, FString& Value) const
{
	const FConfigSection* Sec = Find(Section);
	if (Sec == nullptr)
	{
		return false;
	}
	const FConfigValue* PairString = Sec->Find(Key);
	if (PairString == nullptr)
	{
		return false;
	}
	Value = PairString->GetValue();
	return true;
}

bool FConfigFile::GetInt(const TCHAR* Section, const TCHAR* Key, int32& Value) const
{
	FString Text;
	if (GetString(Section, Key, Text))
	{
		Value = FCString::Atoi(*Text);
		return true;
	}
	return false;
}

bool FConfigFile::GetInt64(const TCHAR* Section, const TCHAR* Key, int64& Value) const
{
	FString Text;
	if (GetString(Section, Key, Text))
	{
		Value = FCString::Atoi64(*Text);
		return true;
	}
	return false;
}

bool FConfigFile::GetFloat(const TCHAR* Section, const TCHAR* Key, float& Value) const
{
	FString Text;
	if (GetString(Section, Key, Text))
	{
		Value = FCString::Atof(*Text);
		return true;
	}
	return false;
}

bool FConfigFile::GetBool(const TCHAR* Section, const TCHAR* Key, bool& Value) const
{
	FString Text;
	if (GetString(Section, Key, Text))
	{
		Value = FCString::ToBool(*Text);
		return true;
	}
	return false;
}

int32 FConfigFile::GetArray(const TCHAR* Section, const TCHAR* Key, TArray<FString>& Value) const
{
	Value.Empty();
	const FConfigSection* Sec = Find(Section);
	if (Sec != nullptr)
	{
		TArray<FConfigValue> RemapArray;
		Sec->MultiFind(Key, RemapArray, true);
		for (const FConfigValue& Entry : RemapArray)
		{
			Value.Add(Entry.GetValue());
		}
	}
	return Value.Num();
}

void FConfigFile::SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value)
{
	FConfigSection* Sec = Find(Section);
	if (Sec == nullptr)
	{
		Sec = &Add(Section);
	}

	Sec->ArrayKeys.Remove(Key);
	FConfigValue* ConfigValue = Sec->Find(Key);
	if (ConfigValue == nullptr)
	{
		Sec->Add(Key, FConfigValue(Value));
		Dirty = true;
	}
	else if (!ConfigValue->GetSavedValue().Equals(Value, ESearchCase::CaseSensitive))
	{
		*ConfigValue = FConfigValue(Value);
		Dirty = true;
	}
}

void FConfigFile::SetInt64(const TCHAR* Section, const TCHAR* Key, const int64 Value)
{
	SetString(Section, Key, *FString::Printf("%lld", (long long)Value));
}

void FConfigFile::SetArray(const TCHAR* Section, const TCHAR* Key, const TArray<FString>& Value)
{
	FConfigSection* Sec = Find(Section);
	if (Sec == nullptr)
	{
		Sec = &Add(Section);
	}

	Sec->Remove(Key);
	Sec->ArrayKeys.Add(Key);
	for (const FString& Entry : Value)
	{
		Sec->Add(Key, FConfigValue(Entry));
	}
	Dirty = true;
}

void FConfigFile::OverrideFromCommandline(const TCHAR* CommandLine)
{
	// -ini:<Name>:[Section]:Key=Value
	const FString Prefix = FString("-ini:") + Name.ToString() + ":[";
	const TCHAR* Stream = CommandLine;
	FString Token;
	while (FParse::Token(Stream, Token, false))
	{
		if (!Token.StartsWith(Prefix))
		{
			continue;
		}
		const FString Rest = Token.Mid(Prefix.Len());
		const int32 SectionEnd = Rest.Find("]:", ESearchCase::CaseSensitive);
		if (SectionEnd == INDEX_NONE)
		{
			continue;
		}
		const FString SectionName = Rest.Mid(0, SectionEnd);
		FString Assignment = Rest.Mid(SectionEnd + 2);
		FString Key;
		FString Value;
		if (!Assignment.Split("=", &Key, &Value))
		{
			continue;
		}
		Value.TrimQuotesInline();
		SetString(*SectionName, *Key, *Value);
		UE_LOG(LogConfig, Log, "Override from the command line: [%s] %s=%s in %s", *SectionName, *Key, *Value,
			*Name.ToString());
	}
}

// FConfigCacheIni -----------------------------------------------------------------------------------------------------

FConfigFile* FConfigCacheIni::FindConfigFile(const FString& Filename)
{
	return TMap<FString, FConfigFile>::Find(Filename);
}

const FConfigFile* FConfigCacheIni::FindConfigFile(const FString& Filename) const
{
	return TMap<FString, FConfigFile>::Find(Filename);
}

FConfigFile* FConfigCacheIni::Find(const FString& InFilename, bool bCreateIfNotFound)
{
	if (InFilename.IsEmpty())
	{
		return nullptr;
	}

	FConfigFile* Result = FindConfigFile(InFilename);
	if (Result == nullptr && bCreateIfNotFound)
	{
		Result = &Add(InFilename);
		Result->Read(InFilename);
	}
	return Result;
}

void FConfigCacheIni::Flush(bool bRead, const FString& Filename)
{
	TArray<FString> Filenames;
	if (Filename.IsEmpty())
	{
		PendingUserChanges.GetKeys(Filenames);
	}
	else if (PendingUserChanges.Contains(Filename))
	{
		Filenames.Add(Filename);
	}

	for (const FString& File : Filenames)
	{
		FConfigFile* Combined = FindConfigFile(File);
		const FConfigFile& Changes = PendingUserChanges.FindChecked(File);
		if (Combined == nullptr || Combined->NoSave || !Combined->Dirty)
		{
			continue;
		}

		// Merge into what the user layer already holds on disk.
		FConfigFile UserLayer;
		UserLayer.Read(File);
		for (const auto& SectionPair : Changes)
		{
			FConfigSection* Target = UserLayer.Find(SectionPair.Key);
			if (Target == nullptr)
			{
				Target = &UserLayer.Add(SectionPair.Key);
			}
			for (const FName& Key : GetSectionKeys(SectionPair.Value))
			{
				TArray<FConfigValue> Values;
				SectionPair.Value.MultiFind(Key, Values, true);
				Target->Remove(Key);
				for (const FConfigValue& Value : Values)
				{
					Target->Add(Key, Value);
				}
				if (SectionPair.Value.ArrayKeys.Contains(Key))
				{
					Target->ArrayKeys.Add(Key);
				}
				else
				{
					Target->ArrayKeys.Remove(Key);
				}
			}
		}

		if (UserLayer.Write(File))
		{
			Combined->Dirty = false;
		}
		else
		{
			UE_LOG(LogConfig, Warning, "Could not write %s", *File);
		}
	}

	if (bRead)
	{
		for (const FString& File : Filenames)
		{
			// A global file is loaded again from its layers (the user layer just written on top); another one goes.
			const FConfigFile* Combined = FindConfigFile(File);
			const FString BaseName = Combined != nullptr ? Combined->Name.ToString() : FString();
			Remove(File);
			PendingUserChanges.Remove(File);
			if (!BaseName.IsEmpty() && GetDestIniFilename(*BaseName, nullptr) == File)
			{
				FString Reloaded;
				(void)LoadGlobalIniFile(Reloaded, *BaseName, nullptr, true, this);
			}
		}
	}
}

bool FConfigCacheIni::GetString(const TCHAR* Section, const TCHAR* Key, FString& Value, const FString& Filename)
{
	const FConfigFile* File = FindConfigFile(Filename);
	return File != nullptr && File->GetString(Section, Key, Value);
}

bool FConfigCacheIni::GetInt(const TCHAR* Section, const TCHAR* Key, int32& Value, const FString& Filename)
{
	const FConfigFile* File = FindConfigFile(Filename);
	return File != nullptr && File->GetInt(Section, Key, Value);
}

bool FConfigCacheIni::GetFloat(const TCHAR* Section, const TCHAR* Key, float& Value, const FString& Filename)
{
	const FConfigFile* File = FindConfigFile(Filename);
	return File != nullptr && File->GetFloat(Section, Key, Value);
}

bool FConfigCacheIni::GetBool(const TCHAR* Section, const TCHAR* Key, bool& Value, const FString& Filename)
{
	const FConfigFile* File = FindConfigFile(Filename);
	return File != nullptr && File->GetBool(Section, Key, Value);
}

int32 FConfigCacheIni::GetArray(const TCHAR* Section, const TCHAR* Key, TArray<FString>& Value, const FString& Filename)
{
	const FConfigFile* File = FindConfigFile(Filename);
	if (File == nullptr)
	{
		Value.Empty();
		return 0;
	}
	return File->GetArray(Section, Key, Value);
}

bool FConfigCacheIni::GetVector(const TCHAR* Section, const TCHAR* Key, FVector& Value, const FString& Filename)
{
	FString Text;
	return GetString(Section, Key, Text, Filename) && Value.InitFromString(Text);
}

bool FConfigCacheIni::GetRotator(const TCHAR* Section, const TCHAR* Key, FRotator& Value, const FString& Filename)
{
	FString Text;
	return GetString(Section, Key, Text, Filename) && Value.InitFromString(Text);
}

bool FConfigCacheIni::GetColor(const TCHAR* Section, const TCHAR* Key, FColor& Value, const FString& Filename)
{
	FString Text;
	return GetString(Section, Key, Text, Filename) && Value.InitFromString(Text);
}

bool FConfigCacheIni::GetSection(const TCHAR* Section, TArray<FString>& Result, const FString& Filename)
{
	Result.Empty();
	const FConfigFile* File = FindConfigFile(Filename);
	const FConfigSection* Sec = File != nullptr ? File->Find(Section) : nullptr;
	if (Sec == nullptr)
	{
		return false;
	}
	for (const auto& Pair : *Sec)
	{
		Result.Add(FString::Printf("%s=%s", *Pair.Key.ToString(), *Pair.Value.GetValue()));
	}
	return true;
}

bool FConfigCacheIni::DoesSectionExist(const TCHAR* Section, const FString& Filename)
{
	const FConfigFile* File = FindConfigFile(Filename);
	return File != nullptr && File->Find(Section) != nullptr;
}

FConfigSection* FConfigCacheIni::GetSectionPrivate(const TCHAR* Section, bool bForce, const FString& Filename)
{
	FConfigFile* File = Find(Filename, bForce);
	if (File == nullptr)
	{
		return nullptr;
	}
	FConfigSection* Sec = File->Find(Section);
	if (Sec == nullptr && bForce)
	{
		Sec = &File->Add(Section);
	}
	return Sec;
}

void FConfigCacheIni::SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const FString& Filename)
{
	FConfigFile* File = Find(Filename, true);
	if (File == nullptr)
	{
		return;
	}
	File->SetString(Section, Key, Value);
	if (!File->NoSave)
	{
		PendingUserChanges.FindOrAdd(Filename).SetString(Section, Key, Value);
	}
}

void FConfigCacheIni::SetInt(const TCHAR* Section, const TCHAR* Key, int32 Value, const FString& Filename)
{
	SetString(Section, Key, *FString::Printf("%d", Value), Filename);
}

void FConfigCacheIni::SetFloat(const TCHAR* Section, const TCHAR* Key, float Value, const FString& Filename)
{
	SetString(Section, Key, *FString::Printf("%f", double(Value)), Filename);
}

void FConfigCacheIni::SetBool(const TCHAR* Section, const TCHAR* Key, bool Value, const FString& Filename)
{
	SetString(Section, Key, Value ? "True" : "False", Filename);
}

void FConfigCacheIni::SetArray(
	const TCHAR* Section, const TCHAR* Key, const TArray<FString>& Value, const FString& Filename)
{
	FConfigFile* File = Find(Filename, true);
	if (File == nullptr)
	{
		return;
	}
	File->SetArray(Section, Key, Value);
	if (!File->NoSave)
	{
		PendingUserChanges.FindOrAdd(Filename).SetArray(Section, Key, Value);
	}
}

bool FConfigCacheIni::RemoveKey(const TCHAR* Section, const TCHAR* Key, const FString& Filename)
{
	FConfigSection* Sec = GetSectionPrivate(Section, false, Filename);
	if (Sec == nullptr || Sec->Remove(Key) == 0)
	{
		return false;
	}
	Sec->ArrayKeys.Remove(Key);
	RecordRemoval(Section, Key, Filename);
	return true;
}

bool FConfigCacheIni::EmptySection(const TCHAR* Section, const FString& Filename)
{
	FConfigSection* Sec = GetSectionPrivate(Section, false, Filename);
	if (Sec == nullptr)
	{
		return false;
	}
	TArray<FName> Keys;
	Sec->GetKeys(Keys);
	Sec->Empty();
	Sec->ArrayKeys.Empty();
	for (const FName& Key : Keys)
	{
		RecordRemoval(Section, *Key.ToString(), Filename);
	}
	return true;
}

void FConfigCacheIni::RecordRemoval(const TCHAR* Section, const TCHAR* Key, const FString& Filename)
{
	FConfigFile* File = FindConfigFile(Filename);
	if (File == nullptr)
	{
		return;
	}
	File->Dirty = true;
	if (!File->NoSave)
	{
		// An empty array in the user layer ("!Key=ClearArray") takes the key away from the lower layers too.
		PendingUserChanges.FindOrAdd(Filename).SetArray(Section, Key, TArray<FString>());
	}
}

void FConfigCacheIni::GetConfigFilenames(TArray<FString>& ConfigFilenames) const
{
	GetKeys(ConfigFilenames);
}

void FConfigCacheIni::Dump(FOutputDevice& Ar, const TCHAR* IniName)
{
	for (const auto& FilePair : *this)
	{
		if (IniName == nullptr || FilePair.Value.Name == FName(IniName))
		{
			Ar.Logf("FConfigCacheIni: %s", *FilePair.Key);
			FilePair.Value.Dump(Ar);
		}
	}
}

TArray<FString> FConfigCacheIni::GetHierarchy(const TCHAR* BaseIniName, const TCHAR* Platform)
{
	const FString PlatformName =
		Platform != nullptr ? FString(Platform) : FString(FPlatformProperties::IniPlatformName());
	const FString Name(BaseIniName);

	TArray<FString> Files;
	Files.Add(FPaths::EngineConfigDir() + "Base.ini");
	Files.Add(FPaths::EngineConfigDir() + "Base" + Name + ".ini");
	Files.Add(FPaths::EngineConfigDir() + PlatformName + "/" + PlatformName + Name + ".ini");
	Files.Add(FPaths::EnginePlatformExtensionsDir() + PlatformName + "/Config/" + PlatformName + Name + ".ini");
	Files.Add(FPaths::ProjectConfigDir() + "Default" + Name + ".ini");
	Files.Add(FPaths::ProjectConfigDir() + PlatformName + "/" + PlatformName + Name + ".ini");
	Files.Add(FPaths::ProjectPlatformExtensionsDir() + PlatformName + "/Config/" + PlatformName + Name + ".ini");
#if PLATFORM_DESKTOP
	// The user layer: what SetX + Flush saved on this machine.
	Files.Add(GetDestIniFilename(BaseIniName, *PlatformName));
#endif
	return Files;
}

FString FConfigCacheIni::GetDestIniFilename(const TCHAR* BaseIniName, const TCHAR* PlatformName)
{
	const FString Platform =
		PlatformName != nullptr ? FString(PlatformName) : FString(FPlatformProperties::IniPlatformName());
	return FPaths::GeneratedConfigDir() + Platform + "/" + BaseIniName + ".ini";
}

bool FConfigCacheIni::LoadLocalIniFile(
	FConfigFile& ConfigFile, const TCHAR* IniName, bool bIsBaseIniName, const TCHAR* Platform)
{
	ConfigFile.Empty();
	ConfigFile.Name = FName(IniName);

	if (!bIsBaseIniName)
	{
		return ConfigFile.Combine(IniName);
	}

	int32 NumFound = 0;
	for (const FString& Layer : GetHierarchy(IniName, Platform))
	{
		if (ConfigFile.Combine(Layer))
		{
			++NumFound;
			UE_LOG(LogConfig, Verbose, "%s: %s", IniName, *Layer);
		}
	}
	return NumFound > 0;
}

bool FConfigCacheIni::LoadGlobalIniFile(FString& OutFinalIniFilename, const TCHAR* BaseIniName, const TCHAR* Platform,
	bool bForceReload, FConfigCacheIni* ConfigSystem)
{
	if (ConfigSystem == nullptr)
	{
		ConfigSystem = GConfig;
	}
	check(ConfigSystem != nullptr);

	OutFinalIniFilename = GetDestIniFilename(BaseIniName, Platform);
	if (!bForceReload && ConfigSystem->FindConfigFile(OutFinalIniFilename) != nullptr)
	{
		return true;
	}

	FConfigFile NewConfigFile;
	const bool bFound = LoadLocalIniFile(NewConfigFile, BaseIniName, true, Platform);
	NewConfigFile.NoSave = PLATFORM_DESKTOP == 0;
	NewConfigFile.OverrideFromCommandline(FCommandLine::Get());
	NewConfigFile.Dirty = false;
	ConfigSystem->Add(OutFinalIniFilename, MoveTemp(NewConfigFile));
	return bFound;
}

void FConfigCacheIni::InitializeConfigSystem()
{
	if (GConfig == nullptr)
	{
		GConfig = new FConfigCacheIni();
	}

	FString Loaded;
	auto Load = [&Loaded](FString& OutFilename, const TCHAR* BaseIniName)
	{
		if (LoadGlobalIniFile(OutFilename, BaseIniName))
		{
			Loaded += Loaded.IsEmpty() ? "" : ", ";
			Loaded += BaseIniName;
		}
	};
	Load(GEngineIni, "Engine");
	Load(GGameIni, "Game");
	Load(GInputIni, "Input");
#if PLATFORM_DESKTOP
	Load(GEditorIni, "Editor");
#endif

	UE_LOG(LogConfig, Log, "Config files found for: %s (engine config in %s)", Loaded.IsEmpty() ? "none" : *Loaded,
		*FPaths::EngineConfigDir());
}
