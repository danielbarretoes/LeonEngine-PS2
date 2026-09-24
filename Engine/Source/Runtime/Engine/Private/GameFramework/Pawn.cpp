#include "GameFramework/Pawn.h"

#include "GameFramework/Controller.h"

void APawn::DetachController()
{
	if (Controller == nullptr)
	{
		return;
	}
	// Controller::UnPossess clears Pawn and calls bindController(nullptr).
	Controller->UnPossess();
}

void APawn::Destroy()
{
	if (IsPendingKillPending())
	{
		return;
	}
	DetachController();
	AActor::Destroy();
}

void APawn::EndPlay()
{
	DetachController();
}
