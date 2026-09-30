#include "Framework/Application/NavigationConfig.h"

EUINavigation FNavigationConfig::GetNavigationDirectionFromKey(const FKey& Key)
{
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
	{
		return EUINavigation::Up;
	}
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
	{
		return EUINavigation::Down;
	}
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
	{
		return EUINavigation::Left;
	}
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
	{
		return EUINavigation::Right;
	}
	if (Key == EKeys::Tab)
	{
		return EUINavigation::Next;
	}
	return EUINavigation::Invalid;
}

EUINavigationAction FNavigationConfig::GetNavigationActionFromKey(const FKey& Key)
{
	if (Key == EKeys::Enter || Key == EKeys::NumPadEnter || Key == EKeys::SpaceBar ||
		Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		return EUINavigationAction::Accept;
	}
	if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
	{
		return EUINavigationAction::Back;
	}
	return EUINavigationAction::Invalid;
}
