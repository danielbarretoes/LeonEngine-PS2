#include "IPlatformFilePak.h"
#include "Modules/ModuleManager.h"

// The pak platform file is created by FEngineLoop::PreInit before the modules start (UE: the PakFile module's
// IPlatformFileModule), so the module itself has no startup work.
IMPLEMENT_MODULE(FDefaultModuleImpl, PakFile)
