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
		GConfig->GetInt(Section, TEXT("TextureUploadBudgetKB"), Settings.TextureUploadBudgetKB, GEngineIni);
		GConfig->GetFloat(Section, TEXT("StaticMeshLODDistanceScale"), Settings.StaticMeshLODDistanceScale, GEngineIni);
	}
	Settings.DisplayAspectRatio = FMath::Max(Settings.DisplayAspectRatio, 0.0f);
	Settings.SyncInterval = FMath::Clamp(Settings.SyncInterval, 0, 4);
	Settings.TextureUploadBudgetKB = FMath::Clamp(Settings.TextureUploadBudgetKB, 0, 4096);
	Settings.StaticMeshLODDistanceScale = FMath::Clamp(Settings.StaticMeshLODDistanceScale, 0.01f, 100.0f);
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
