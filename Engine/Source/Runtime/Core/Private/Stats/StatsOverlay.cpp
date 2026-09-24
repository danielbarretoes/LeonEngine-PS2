#include "Stats/StatsOverlay.h"

#include <cstdio>

namespace
{
	constexpr int32 MessageChars = 32;

	bool bStatsVisible = true;
	bool bGamepadWidgetVisible = true;
	char Messages[FStatsOverlay::MaxOnScreenMessages][MessageChars] = {};
}

void FStatsOverlay::SetStatsVisible(bool bVisible)
{
	bStatsVisible = bVisible;
}

bool FStatsOverlay::IsStatsVisible()
{
	return bStatsVisible;
}

void FStatsOverlay::SetGamepadWidgetVisible(bool bVisible)
{
	bGamepadWidgetVisible = bVisible;
}

bool FStatsOverlay::IsGamepadWidgetVisible()
{
	return bGamepadWidgetVisible;
}

void FStatsOverlay::CycleVisibility()
{
	if (bStatsVisible && bGamepadWidgetVisible)
	{
		bGamepadWidgetVisible = false;
	}
	else if (bStatsVisible)
	{
		bStatsVisible = false;
		bGamepadWidgetVisible = true;
	}
	else if (bGamepadWidgetVisible)
	{
		bGamepadWidgetVisible = false;
	}
	else
	{
		bStatsVisible = true;
		bGamepadWidgetVisible = true;
	}
	std::printf("StatsOverlay: stats %s, gamepad %s\n", bStatsVisible ? "on" : "off",
		bGamepadWidgetVisible ? "on" : "off");
}

void FStatsOverlay::AddOnScreenDebugMessage(int32 Key, const char* Message)
{
	if (Key < 0 || Key >= MaxOnScreenMessages)
	{
		return;
	}
	if (Message == nullptr)
	{
		Messages[Key][0] = '\0';
		return;
	}
	std::snprintf(Messages[Key], MessageChars, "%s", Message);
}

void FStatsOverlay::ClearOnScreenDebugMessage(int32 Key)
{
	AddOnScreenDebugMessage(Key, nullptr);
}

const char* FStatsOverlay::GetOnScreenDebugMessage(int32 Key)
{
	return (Key >= 0 && Key < MaxOnScreenMessages) ? Messages[Key] : "";
}
