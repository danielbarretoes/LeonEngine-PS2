#include "Async/AsyncIOSystem.h"

// Win64 has no IO thread (Docs/PLANS/ps2-shipping.md N24): the game thread reads the queue in its order, at
// FAsyncIOSystem::Tick and in a wait, so a run is the same whatever the disk's timing (the bot match's determinism).
IAsyncIOWorker* CreatePlatformAsyncIOWorker(FAsyncIOSystem& /*System*/)
{
	return nullptr;
}
