#include "CookPaths.h"

#include "Misc/Paths.h"

FString FCookPaths::ResolveBeside(const FString& BaseDir, const FString& Relative)
{
	FString Path = FPaths::IsRelative(Relative) ? FPaths::Combine(BaseDir, Relative) : Relative;
	FPaths::NormalizeFilename(Path);
	FPaths::CollapseRelativeDirectories(Path);
	return Path;
}
