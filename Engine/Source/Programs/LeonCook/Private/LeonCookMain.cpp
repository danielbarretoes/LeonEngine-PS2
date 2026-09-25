#include "CommandletHelpers.h"
#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IProjectManager.h"
#include "Logging/LogSuppressionInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/Package.h"

// LeonCook: the engine as a command-line editor (UE: UE4Editor-Cmd.exe <Project>.uproject -run=<Commandlet>).
//
//   LeonCook [<Project>.lproj | -project=<Project>.lproj] -run=<Commandlet> [commandlet arguments]
//
// Without a project it runs engine-only (/Game is the program's own folder). The commandlet class is found by name
// through reflection: -run=ImportAssets makes a UImportAssetsCommandlet (UE: FEngineLoop::PreInit's -run=). LeonCook
// links the editor module (LeonEd) and no RHI: it never opens a window. Its exit code is the commandlet's.

DEFINE_LOG_CATEGORY_STATIC(LogLeonCook, Log, All);

namespace
{

	/** Counts the errors and warnings logged while the commandlet runs (UE: the commandlet's error summary). */
	class FCommandletErrorCounter : public FOutputDevice
	{
	public:
		void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			(void)V;
			(void)Category;
			const ELogVerbosity::Type Level =
				static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
			if (Level == ELogVerbosity::Error || Level == ELogVerbosity::Fatal)
			{
				++Errors;
			}
			else if (Level == ELogVerbosity::Warning)
			{
				++Warnings;
			}
		}

		int32 Errors = 0;
		int32 Warnings = 0;
	};

	void PrintUsage()
	{
		UE_LOG(LogLeonCook, Display, "Usage: LeonCook [<Project>.lproj] -run=<Commandlet> [arguments]");
		UE_LOG(LogLeonCook, Display, "Commandlets:");
		TArray<UClass*> Classes;
		CommandletHelpers::GetCommandletClasses(Classes);
		for (UClass* Class : Classes)
		{
			const UCommandlet* Default = Class->GetDefaultObject<UCommandlet>();
			FString Name = Class->GetName();
			Name.RemoveFromEnd(TEXT("Commandlet"));
			UE_LOG(LogLeonCook, Display, "  %s: %s", *Name, *Default->HelpDescription);
			UE_LOG(LogLeonCook, Display, "    %s", *Default->HelpUsage);
		}
	}

	/** The command line without the program's own arguments (the project and -run=), for the commandlet. */
	FString GetCommandletParams(const TCHAR* CmdLine, const FString& ProjectToken)
	{
		FString Params;
		FString Token;
		const TCHAR* Stream = CmdLine;
		while (FParse::Token(Stream, Token, false))
		{
			if (Token.StartsWith(TEXT("-run="), ESearchCase::IgnoreCase) ||
				Token.StartsWith(TEXT("-project="), ESearchCase::IgnoreCase) ||
				(!ProjectToken.IsEmpty() && Token == ProjectToken))
			{
				continue;
			}
			if (!Params.IsEmpty())
			{
				Params += TEXT(" ");
			}
			// Keep a value with spaces one token for the commandlet's ParseCommandLine.
			int32 Equals = INDEX_NONE;
			if (Token.Contains(TEXT(" ")) && Token.FindChar('=', Equals))
			{
				Params += Token.Left(Equals + 1) + TEXT("\"") + Token.Mid(Equals + 1) + TEXT("\"");
			}
			else if (Token.Contains(TEXT(" ")))
			{
				Params += TEXT("\"") + Token + TEXT("\"");
			}
			else
			{
				Params += Token;
			}
		}
		return Params;
	}

} // namespace

int main(int ArgC, char* ArgV[])
{
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));
	const TCHAR* CmdLine = FCommandLine::Get();

	// The project: -project=<path>.lproj or a first argument ending in .lproj; none runs engine-only.
	FString ProjectFile;
	FString ProjectToken;
	if (!FParse::Value(CmdLine, "project=", ProjectFile))
	{
		const TCHAR* Stream = CmdLine;
		const FString FirstToken = FParse::Token(Stream, false);
		if (FirstToken.EndsWith(".lproj"))
		{
			ProjectFile = FirstToken;
			ProjectToken = FirstToken;
		}
	}
	if (!ProjectFile.IsEmpty())
	{
		FPaths::SetProjectFilePath(ProjectFile);
		FApp::SetProjectName(*FPaths::GetBaseFilename(ProjectFile));
	}

	FConfigCacheIni::InitializeConfigSystem();
	TUniquePtr<FOutputDeviceFile> LogFile = MakeUnique<FOutputDeviceFile>();
	GLog->AddOutputDevice(LogFile.Get());
	FLogSuppressionInterface::Get().ProcessConfigAndCommandLine();
	if (FPaths::IsProjectFilePathSet())
	{
		IProjectManager::Get().LoadProjectFile(FPaths::GetProjectFilePath());
	}
	FModuleManager::Get().StartupStaticallyLinkedModules();

	int32 Result = 1;
	FString CommandletName;
	if (!FParse::Value(CmdLine, "run=", CommandletName) || CommandletName.IsEmpty())
	{
		const bool bHelp = FParse::Param(CmdLine, "help") || FParse::Param(CmdLine, "h");
		PrintUsage();
		Result = bHelp ? 0 : 1;
	}
	else if (UClass* CommandletClass = CommandletHelpers::FindCommandletClass(CommandletName))
	{
		UE_LOG(LogLeonCook, Display, "Running %s (project: %s)", *CommandletClass->GetName(),
			FApp::HasProjectName() ? FApp::GetProjectName() : "none, engine only");
		UCommandlet* Commandlet = NewObject<UCommandlet>(GetTransientPackage(), CommandletClass);
		Commandlet->AddToRoot();
		FCommandletErrorCounter Counter;
		GLog->AddOutputDevice(&Counter);
		Result = Commandlet->Main(GetCommandletParams(CmdLine, ProjectToken));
		GLog->RemoveOutputDevice(&Counter);
		Commandlet->RemoveFromRoot();
		if (Commandlet->ShowErrorCount)
		{
			UE_LOG(LogLeonCook, Display, "%s - %d error(s), %d warning(s)", Result == 0 ? "Success" : "Failure",
				Counter.Errors, Counter.Warnings);
		}
	}
	else
	{
		UE_LOG(LogLeonCook, Error, "No commandlet named '%s' (U%s or U%sCommandlet)", *CommandletName, *CommandletName,
			*CommandletName);
		PrintUsage();
	}

	FModuleManager::Get().ShutdownModules();
	GLog->Flush();
	GLog->RemoveOutputDevice(LogFile.Get());
	LogFile.Reset();
	return Result;
}
