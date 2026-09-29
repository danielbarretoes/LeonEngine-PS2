#include "GSFieldPacer.h"

float FGSFieldPacer::GetFieldsPerSecond(EGSVideoMode Mode)
{
	// NTSC's field rate is 60 / 1.001.
	return Mode == EGSVideoMode::Pal ? 50.0f : 59.94006f;
}

uint32 FGSFieldPacer::GetFieldMicroseconds(EGSVideoMode Mode)
{
	return Mode == EGSVideoMode::Pal ? 20000u : 16683u;
}

int32 FGSFieldPacer::GetVisibleLines(EGSVideoMode Mode)
{
	return Mode == EGSVideoMode::Pal ? 512 : 448;
}

uint32 FGSFieldPacer::GetFlipField(uint32 LastFlipField, uint32 CurrentField, uint32 SyncInterval)
{
	const uint32 Target = LastFlipField + FMath::Max(SyncInterval, 1u);
	return HasBegun(Target, CurrentField) ? CurrentField + 1 : Target;
}
