#include "ThirdPerson.h"

#include "CoreGlobals.h"
#include "GenericPlatform/GenericApplication.h"
#include "LaunchEngineLoop.h"
#include "Modules/ModuleManager.h"
#include "ThirdPersonGameMode.h"

#include <cstdio>

IMPLEMENT_PRIMARY_GAME_MODULE(FThirdPersonModule, ThirdPerson, "ThirdPerson")

void FThirdPersonModule::StartupModule()
{
	FGenericWindow* Window = GEngineLoop.GetMainWindow();
	GenericApplication* Application = GEngineLoop.GetApplication();
	if (Window == nullptr || Application == nullptr)
	{
		std::printf("ThirdPerson: no main window\n");
		RequestEngineExit("ThirdPerson: no main window");
		return;
	}

	GameMode = std::make_unique<FThirdPersonGameMode>(*Window, Application->GetInputInterface());
	GameMode->StartPlay();
	TickHandle = FTicker::GetCoreTicker().AddTicker([this](float DeltaTime)
	{
		return GameMode->Tick(DeltaTime);
	});
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
