#include "SaveGameSystem.h"

// The desktop keeps its saves as files (UE: FGenericSaveGameSystem): <Project>/Saved/SaveGames/<Slot>.sav.
ISaveGameSystem& GetPlatformSaveGameSystem()
{
	static FGenericSaveGameSystem SaveGameSystem;
	return SaveGameSystem;
}
