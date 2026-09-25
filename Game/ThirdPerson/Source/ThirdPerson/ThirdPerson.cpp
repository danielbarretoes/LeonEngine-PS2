#include "ThirdPerson.h"

#include "CoreGlobals.h"
#include "GenericPlatform/GenericApplication.h"
#include "LaunchEngineLoop.h"
#include "Modules/ModuleManager.h"
#include "ThirdPersonGameMode.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FThirdPersonModule, ThirdPerson, "ThirdPerson")

DEFINE_LOG_CATEGORY(LogThirdPerson);

void FThirdPersonModule::StartupModule()
{
	FGenericWindow* Window = GEngineLoop.GetMainWindow();
	GenericApplication* Application = GEngineLoop.GetApplication();
	if (Window == nullptr || Application == nullptr)
	{
		UE_LOG(LogThirdPerson, Error, TEXT("No main window"));
		RequestEngineExit("ThirdPerson: no main window");
		return;
	}

	GameMode = std::make_unique<FThirdPersonGameMode>(*Window, Application->GetInputInterface());
	GameMode->StartPlay();
	TickHandle = FTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float DeltaTime) { return GameMode->Tick(DeltaTime); }));
}

void FThirdPersonModule::ShutdownModule()
{
	if (TickHandle.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle = {};
	}
	GameMode.reset();
}
