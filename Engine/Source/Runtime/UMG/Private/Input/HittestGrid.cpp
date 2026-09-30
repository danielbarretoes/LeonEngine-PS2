#include "Input/HittestGrid.h"

#include "Components/Widget.h"

void FHittestGrid::AddWidget(UWidget* Widget, const FGeometry& Geometry)
{
	FWidgetEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Widget = Widget;
	Entry.Geometry = Geometry;
}

bool FHittestGrid::IsFocusable(const FWidgetEntry& Entry)
{
	const UWidget* Widget = Entry.Widget.Get();
	return Widget != nullptr && Widget->GetIsEnabled() && Widget->SupportsKeyboardFocus();
}

UWidget* FHittestGrid::FindTopmostWidgetAt(const FVector2D& Location) const
{
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		UWidget* Widget = Entries[Index].Widget.Get();
		if (Widget != nullptr && Entries[Index].Geometry.IsUnderLocation(Location))
		{
			return Widget;
		}
	}
	return nullptr;
}

UWidget* FHittestGrid::FindFirstFocusableWidget() const
{
	for (const FWidgetEntry& Entry : Entries)
	{
		if (IsFocusable(Entry))
		{
			return Entry.Widget.Get();
		}
	}
	return nullptr;
}

const FGeometry* FHittestGrid::FindGeometry(const UWidget* Widget) const
{
	for (const FWidgetEntry& Entry : Entries)
	{
		if (Widget != nullptr && Entry.Widget.Get() == Widget)
		{
			return &Entry.Geometry;
		}
	}
	return nullptr;
}

UWidget* FHittestGrid::FindNextFocusableWidget(const UWidget* From, EUINavigation Direction) const
{
	if (From == nullptr)
	{
		return nullptr;
	}
	// An explicit rule wins (UE: EUINavigationRule::Explicit), when its widget was painted and takes focus.
	if (UWidget* Explicit = From->GetExplicitNavigation(Direction))
	{
		for (const FWidgetEntry& Entry : Entries)
		{
			if (Entry.Widget.Get() == Explicit && IsFocusable(Entry))
			{
				return Explicit;
			}
		}
	}
	int32 FromIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index].Widget.Get() == From)
		{
			FromIndex = Index;
			break;
		}
	}
	if (FromIndex == INDEX_NONE)
	{
		return nullptr;
	}
	if (Direction == EUINavigation::Next || Direction == EUINavigation::Previous)
	{
		const int32 Step = Direction == EUINavigation::Next ? 1 : -1;
		for (int32 Index = FromIndex + Step; Index >= 0 && Index < Entries.Num(); Index += Step)
		{
			if (IsFocusable(Entries[Index]))
			{
				return Entries[Index].Widget.Get();
			}
		}
		return nullptr;
	}
	const FVector2D Origin = Entries[FromIndex].Geometry.GetAbsoluteCenter();
	UWidget* Best = nullptr;
	float BestScore = 0.0f;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Index == FromIndex || !IsFocusable(Entries[Index]))
		{
			continue;
		}
		const FVector2D Delta = Entries[Index].Geometry.GetAbsoluteCenter() - Origin;
		float Along = 0.0f;
		float Across = 0.0f;
		switch (Direction)
		{
			case EUINavigation::Up:
				Along = -Delta.Y;
				Across = FMath::Abs(Delta.X);
				break;
			case EUINavigation::Down:
				Along = Delta.Y;
				Across = FMath::Abs(Delta.X);
				break;
			case EUINavigation::Left:
				Along = -Delta.X;
				Across = FMath::Abs(Delta.Y);
				break;
			case EUINavigation::Right:
				Along = Delta.X;
				Across = FMath::Abs(Delta.Y);
				break;
			default:
				return nullptr;
		}
		// Only what lies that way (half a pixel of slack for a row's rounding).
		if (Along < 0.5f)
		{
			continue;
		}
		const float Score = Along + (2.0f * Across);
		if (Best == nullptr || Score < BestScore)
		{
			Best = Entries[Index].Widget.Get();
			BestScore = Score;
		}
	}
	return Best;
}
