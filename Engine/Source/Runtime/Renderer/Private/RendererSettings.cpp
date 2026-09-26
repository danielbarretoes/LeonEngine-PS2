#include "RendererSettings.h"

#include "Misc/ConfigCacheIni.h"

FRendererSettings FRendererSettings::Load()
{
	FRendererSettings Settings;
	if (GConfig != nullptr)
	{
		const TCHAR* const Section = TEXT("/Script/Engine.RendererSettings");
		GConfig->GetFloat(Section, TEXT("DisplayAspectRatio"), Settings.DisplayAspectRatio, GEngineIni);
		GConfig->GetInt(Section, TEXT("SyncInterval"), Settings.SyncInterval, GEngineIni);
	}
	Settings.DisplayAspectRatio = FMath::Max(Settings.DisplayAspectRatio, 0.0f);
	Settings.SyncInterval = FMath::Clamp(Settings.SyncInterval, 0, 4);
	return Settings;
}

float FRendererSettings::GetDisplayAspectRatio(const FIntPoint& TargetSize) const
{
	if (DisplayAspectRatio > 0.0f)
	{
		return DisplayAspectRatio;
	}
	return TargetSize.Y > 0 ? float(TargetSize.X) / float(TargetSize.Y) : 1.0f;
}
