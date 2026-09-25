#include "UnrealEngine.h"

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineLogs.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Materials/Material.h"
#include "Misc/App.h"
#include "Misc/CoreMisc.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RendererInterface.h"
#include "Sound/SoundWave.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

UEngine* GEngine = nullptr;

namespace
{

	/**
	 * Mounts the content folder of a map file that no mount point contains, and gives the map's long package name
	 * (Leon; UE only opens maps under its mount points). The content folder is the one above the map's `Maps/` folder
	 * (`<Root>/Maps/X.lmap`, UE's content layout), else the map's own folder; its mount point is named after it
	 * (`.../RenderTest/Maps/RenderTest.lmap` mounts `.../RenderTest` as `/RenderTest/`), so the packages the map
	 * references by that root load from there. False when that name already names another folder.
	 */
	[[nodiscard]] bool MountMapContentRoot(const FString& MapFile, FString& OutPackageName)
	{
		FString Root = FPaths::GetPath(MapFile);
		for (FString Folder = Root; !Folder.IsEmpty(); Folder = FPaths::GetPath(Folder))
		{
			if (FPaths::GetCleanFilename(Folder) == TEXT("Maps"))
			{
				Root = FPaths::GetPath(Folder);
				break;
			}
			if (FPaths::GetPath(Folder) == Folder)
			{
				break;
			}
		}
		const FString MountPoint = TEXT("/") + FPaths::GetCleanFilename(Root) + TEXT("/");
		if (FPackageName::MountPointExists(MountPoint))
		{
			UE_LOG(LogLoad, Error, TEXT("Cannot mount '%s' as %s: that mount point names another folder"), *Root,
				*MountPoint);
			return false;
		}
		FPackageName::RegisterMountPoint(MountPoint, Root + TEXT("/"));
		UE_LOG(LogLoad, Log, TEXT("Mounted '%s' as %s"), *Root, *MountPoint);
		return FPackageName::TryConvertFilenameToLongPackageName(MapFile, OutPackageName);
	}

	/**
	 * The long package name of a map's `.lmap` package: a long package name (`/Engine/Maps/Entry`, `/Game/Maps/X`)
	 * whose `.lmap` exists (or whose bytes are registered in memory), or a `.lmap` file path (absolute or relative to
	 * the working directory; MountMapContentRoot mounts it when no mount point contains it).
	 */
	[[nodiscard]] bool FindMapPackage(const FString& Map, FString& OutPackageName)
	{
		FString PackageName;
		if (FPackageName::IsValidLongPackageName(Map))
		{
			PackageName = Map;
		}
		else if (FPaths::GetExtension(Map, true) == FPackageName::GetMapPackageExtension() && FPaths::FileExists(Map))
		{
			const FString MapFile = FPaths::ConvertRelativePathToFull(Map);
			if (!FPackageName::TryConvertFilenameToLongPackageName(MapFile, PackageName) &&
				!MountMapContentRoot(MapFile, PackageName))
			{
				return false;
			}
		}
		else
		{
			return false;
		}
		FString Filename;
		if (!FPackageName::DoesPackageExist(PackageName, nullptr, &Filename))
		{
			return false;
		}
		OutPackageName = PackageName;
		// A map package: a `.lmap` file, or bytes registered in memory.
		return Filename.IsEmpty() || FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension();
	}

} // namespace

UEngine::UEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UEngine::Init(IEngineLoop* InEngineLoop)
{
	// Without a main window (-nullrhi, tests) nothing renders and the audio is silent.
	bHeadless = InEngineLoop == nullptr || InEngineLoop->GetMainWindow() == nullptr;

	LocalPlayerClass = LocalPlayerClassName.IsValid() ? LocalPlayerClassName.TryLoadClass<ULocalPlayer>() : nullptr;
	if (LocalPlayerClass == nullptr)
	{
		LocalPlayerClass = ULocalPlayer::StaticClass();
	}

	GarbageCollectionTimer = FGarbageCollectionTimer(FGarbageCollectionSettings::LoadFromConfig());
	InitializeObjectReferences();
	(void)AudioDevice.Initialize(/*silent=*/bHeadless);
	// The UI cues with a sound wave in the config play it; the others keep their procedural tone.
	for (int32 Cue = 0; Cue < UISounds.Num() && Cue < NumUISounds; ++Cue)
	{
		if (UISounds[Cue] != nullptr)
		{
			TArray<int16> Samples;
			AudioDevice.SetUiSound(static_cast<EUISound>(Cue), UISounds[Cue]->GetPCMView(Samples));
		}
	}
	bIsInitialized = true;
}

void UEngine::InitializeObjectReferences()
{
	// UE: LoadEngineTexture. A missing package is a warning (LoadObject) and a null texture.
	DefaultTexture = LoadObject<UTexture2D>(nullptr, *DefaultTextureName.ToString());
	DefaultBumpNormalTexture = LoadObject<UTexture2D>(nullptr, *DefaultBumpNormalTextureName.ToString());
	(void)UMaterial::GetDefaultMaterial(MD_Surface);
	UISounds.Reset();
	for (const FSoftObjectPath* Name : {&UIClickSoundName, &UIConfirmSoundName, &UIBackSoundName, &UIErrorSoundName})
	{
		UISounds.Add(Name->IsNull() ? nullptr : LoadObject<USoundWave>(nullptr, *Name->ToString()));
	}
}

void UEngine::Start()
{
}

void UEngine::PreExit()
{
	AudioDevice.Shutdown();
	Overlay.Clear();
	bIsInitialized = false;
}

void UEngine::Tick(float /*DeltaSeconds*/, bool /*bIdleMode*/)
{
}

void UEngine::TickDeferredCommands()
{
	// Commands queued while these run wait for the next frame (UE).
	const int32 NumCommands = DeferredCommands.Num();
	for (int32 Index = 0; Index < NumCommands; ++Index)
	{
		const FString Command = DeferredCommands[Index];
		ULocalPlayer* LocalPlayer = nullptr;
		UWorld* World = nullptr;
		for (FWorldContext* Context : GetWorldContexts())
		{
			if (Context != nullptr && Context->OwningGameInstance != nullptr)
			{
				World = Context->World();
				LocalPlayer = Context->OwningGameInstance->GetFirstGamePlayer();
				break;
			}
		}
		UE_LOG(LogEngine, Log, TEXT("Cmd: %s"), *Command);
		const bool bHandled =
			LocalPlayer != nullptr ? LocalPlayer->Exec(World, *Command, *GLog) : Exec(World, *Command, *GLog);
		if (!bHandled)
		{
			UE_LOG(LogEngine, Warning, TEXT("Command not recognized: %s"), *Command);
		}
	}
	DeferredCommands.RemoveAt(0, NumCommands);
}

bool UEngine::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	const TCHAR* Str = Cmd;
	if (FParse::Command(&Str, TEXT("EXIT")) || FParse::Command(&Str, TEXT("QUIT")))
	{
		Ar.Log(TEXT("Closing by request"));
		RequestEngineExit("Exit command");
		return true;
	}
	if (FParse::Command(&Str, TEXT("OBJ")))
	{
		if (FParse::Command(&Str, TEXT("GC")) || FParse::Command(&Str, TEXT("GARBAGE")))
		{
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			return true;
		}
		return false;
	}
	if (FParse::Command(&Str, TEXT("STAT")))
	{
		// Leon's stats overlay stands for both (frame time, memory, triangles, GPU passes).
		if (FParse::Command(&Str, TEXT("UNIT")) || FParse::Command(&Str, TEXT("FPS")))
		{
			SetHudStatsVisible(!bShowHudStats);
			UE_LOG(LogEngine, Log, TEXT("HUD stats: %s"), bShowHudStats ? TEXT("on") : TEXT("off"));
			return true;
		}
		return false;
	}
	if (FParse::Command(&Str, TEXT("RECOMPILESHADERS")))
	{
		// `RecompileShaders changed` reloads the shaders whose files changed, anything else all of them (UE).
		const bool bForce = !FParse::Command(&Str, TEXT("CHANGED"));
		IRendererModule* RendererModule = GetRendererModulePtr();
		const EShaderReloadResult Result = RendererModule != nullptr && RendererModule->IsRendererInitialized()
			? RendererModule->ReloadShaders(bForce)
			: EShaderReloadResult::Unchanged;
		if (Result == EShaderReloadResult::Failed)
		{
			UE_LOG(LogEngine, Warning, TEXT("Shader reload failed; previous programs kept where possible"));
		}
		else if (Result == EShaderReloadResult::Reloaded)
		{
			UE_LOG(LogEngine, Log, TEXT("Shaders reloaded"));
		}
		else
		{
			UE_LOG(LogEngine, Log, TEXT("Shaders unchanged"));
		}
		return true;
	}
	if (FParse::Command(&Str, TEXT("OPEN")))
	{
		// The map changes at the start of the next frame, never inside the world tick (UE).
		FString URLString(Str);
		URLString.TrimStartAndEndInline();
		if (URLString.IsEmpty())
		{
			Ar.Log(TEXT("open: a map is required"));
			return true;
		}
		SetClientTravel(InWorld, *URLString, TRAVEL_Absolute);
		return true;
	}
	return FSelfRegisteringExec::StaticExec(InWorld, Cmd, Ar);
}

EBrowseReturnVal::Type UEngine::Browse(FWorldContext& WorldContext, FURL URL, FString& Error)
{
	Error.Empty();
	WorldContext.TravelURL.Empty();
	UE_LOG(LogLoad, Log, TEXT("Browse: %s"), *URL.ToString());
	if (URL.Valid == 0)
	{
		Error = FString::Printf(TEXT("Invalid URL: %s"), *URL.ToString());
		return EBrowseReturnVal::Failure;
	}
	// A local map (Leon has no network URLs).
	return LoadMap(WorldContext, URL, nullptr, Error) ? EBrowseReturnVal::Success : EBrowseReturnVal::Failure;
}

bool UEngine::LoadMap(FWorldContext& WorldContext, FURL URL, UPendingNetGame* /*Pending*/, FString& Error)
{
	const double StartTime = FPlatformTime::Seconds();
	Error.Empty();
	UE_LOG(LogLoad, Log, TEXT("LoadMap: %s"), *URL.ToString());

	// The map is found before the current world goes: a map that is not there leaves it playing.
	FString MapPackageName;
	if (!FindMapPackage(URL.Map, MapPackageName))
	{
		Error = FString::Printf(TEXT("Failed to load package '%s'"), *URL.Map);
		return false;
	}

	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	if (UWorld* OldWorld = WorldContext.World())
	{
		// The players leave their controllers, then the old world ends play and goes (UE).
		if (GameInstance != nullptr)
		{
			for (ULocalPlayer* Player : GameInstance->GetLocalPlayers())
			{
				if (Player != nullptr && Player->PlayerController != nullptr)
				{
					Player->PlayerController->Player = nullptr;
					Player->PlayerController = nullptr;
				}
			}
		}
		if (OldWorld->PersistentLevel != nullptr)
		{
			const TArray<AActor*> Actors = OldWorld->PersistentLevel->Actors;
			for (AActor* Actor : Actors)
			{
				if (Actor != nullptr && !Actor->IsPendingKill())
				{
					Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}
		}
		WorldContext.SetCurrentWorld(nullptr);
		OldWorld->DestroyWorld(true);
		// A map change is a garbage collection safe point (plan decision D11).
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	// The world of the map package, with its level and actors (UE).
	UPackage* const WorldPackage = LoadPackage(nullptr, *MapPackageName, LOAD_None);
	UWorld* const NewWorld = UWorld::FindWorldInPackage(WorldPackage);
	if (NewWorld == nullptr)
	{
		Error = FString::Printf(TEXT("Failed to load map '%s': no world in the package"), *MapPackageName);
		return false;
	}
	NewWorld->SetWorldType(WorldContext.WorldType != EWorldType::None ? WorldContext.WorldType : EWorldType::Game);
	NewWorld->AddToRoot();
	NewWorld->InitWorld();
	WorldContext.SetCurrentWorld(NewWorld);

	// The game mode (plan decision D18), then the actors get ready for play (a loaded map's components register).
	NewWorld->SetGameMode(URL);
	NewWorld->InitializeActorsForPlay(URL);
	WorldContext.LastURL = URL;

	// Every local player logs in (UE: SpawnPlayActor).
	if (GameInstance != nullptr)
	{
		for (ULocalPlayer* Player : GameInstance->GetLocalPlayers())
		{
			FString PlayerError;
			if (Player != nullptr && !Player->SpawnPlayActor(URL.ToString(true), PlayerError, NewWorld))
			{
				UE_LOG(LogEngine, Error, TEXT("Couldn't spawn player: %s"), *PlayerError);
			}
		}
	}

	NewWorld->BeginPlay();

	const double StopTime = FPlatformTime::Seconds();
	UE_LOG(LogLoad, Log, TEXT("Took %f seconds to LoadMap(%s)"), StopTime - StartTime, *URL.Map);
	if (GameInstance != nullptr)
	{
		GameInstance->LoadComplete(static_cast<float>(StopTime - StartTime), URL.Map);
	}
	return true;
}

void UEngine::SetClientTravel(UWorld* InWorld, const TCHAR* NextURL, ETravelType InTravelType)
{
	FWorldContext* Context = GetWorldContextFromWorld(InWorld);
	if (Context == nullptr)
	{
		TArray<FWorldContext*> Contexts = GetWorldContexts();
		Context = Contexts.Num() > 0 ? Contexts[0] : nullptr;
	}
	if (Context != nullptr)
	{
		Context->TravelURL = NextURL;
		Context->TravelType = static_cast<uint8>(InTravelType);
	}
}

void UEngine::TickWorldTravel(FWorldContext& WorldContext, float /*DeltaSeconds*/)
{
	if (WorldContext.TravelURL.IsEmpty())
	{
		return;
	}
	const FString TravelURL = WorldContext.TravelURL;
	const FURL URL(&WorldContext.LastURL, *TravelURL, static_cast<ETravelType>(WorldContext.TravelType));
	FString Error;
	if (Browse(WorldContext, URL, Error) != EBrowseReturnVal::Success)
	{
		UE_LOG(LogLoad, Error, TEXT("Travel to %s failed: %s"), *TravelURL, *Error);
	}
}

bool UEngine::ConditionalCollectGarbage(float DeltaSeconds)
{
	return GarbageCollectionTimer.Tick(DeltaSeconds, GARBAGE_COLLECTION_KEEPFLAGS);
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* InWorld)
{
	for (FWorldContext* Context : GetWorldContexts())
	{
		if (Context != nullptr && Context->World() == InWorld)
		{
			return Context;
		}
	}
	return nullptr;
}

TArray<FWorldContext*> UEngine::GetWorldContexts()
{
	return {};
}

void UEngine::AddOnScreenDebugMessage(
	int32 /*Key*/, float TimeToDisplay, const FLinearColor& DisplayColor, const FString& DebugMessage)
{
	if (bHeadless)
	{
		UE_LOG(LogEngine, Log, "[headless] %s", *DebugMessage);
		return;
	}
	Overlay.AddOnScreenDebugMessage(DebugMessage, TimeToDisplay, DisplayColor);
}

void UEngine::SetHudStatsVisible(bool bVisible)
{
	bShowHudStats = bVisible;
	if (!bShowHudStats)
	{
		Overlay.SetRightText(FString());
		Overlay.SetBottomLeftText(FString());
	}
}
