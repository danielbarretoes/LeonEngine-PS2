#include "Components/MenuListWidget.h"

#include "GenericPlatform/GenericWindow.h"
#include "InputCoreTypes.h"

UMenuListWidget::UMenuListWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

namespace
{

	/** Lines before the first menu item in BuildPaintText (title + blank separator). */
	[[nodiscard]] int32 ItemStartLine(const FString& InTitle)
	{
		if (InTitle.IsEmpty())
		{
			return 1; // the leading "\n\n" still inserts one blank row before the items
		}
		int32 TitleLines = 1;
		for (const TCHAR C : InTitle)
		{
			if (C == '\n')
			{
				++TitleLines;
			}
		}
		return TitleLines + 1;
	}

} // namespace

void UMenuListWidget::SetItems(TArray<FItem> InItems)
{
	Items = MoveTemp(InItems);
	if (Items.Num() == 0)
	{
		Selected = 0;
		return;
	}
	Selected = FMath::Clamp(Selected, 0, Items.Num() - 1);
	// Prefer a selectable row (an id): status labels are not activatable.
	if (Items[Selected].Id.IsNone())
	{
		for (int32 I = 0; I < Items.Num(); ++I)
		{
			if (!Items[I].Id.IsNone())
			{
				Selected = I;
				break;
			}
		}
	}
}

void UMenuListWidget::SetSelectedIndex(int32 Index)
{
	if (Items.Num() == 0)
	{
		Selected = 0;
		return;
	}
	Selected = FMath::Clamp(Index, 0, Items.Num() - 1);
}

void UMenuListWidget::ResetEdges()
{
	bUpWasDown = bDownWasDown = bEnterWasDown = bMouseWasDown = true;
	// Keyboard only: the mouse must stay usable on the first click after travel.
	IgnoreActivateSeconds = 0.35f;
}

int32 UMenuListWidget::CountLines(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return 0;
	}
	int32 N = 1;
	for (const TCHAR C : Text)
	{
		if (C == '\n')
		{
			++N;
		}
	}
	return N;
}

FString UMenuListWidget::BuildPaintText() const
{
	FString Text = Title.ToString();
	Text += "\n\n";
	for (int32 I = 0; I < Items.Num(); ++I)
	{
		Text += (I == Selected) ? "> " : "  ";
		Text += Items[I].Label.ToString();
		Text += "\n";
	}
	if (!Hint.IsEmpty())
	{
		Text += "\n";
		Text += Hint.ToString();
	}
	return Text;
}

void UMenuListWidget::CacheLayout(int32 /*ViewportW*/, int32 InViewportH)
{
	ViewportH = InViewportH;
	LineH = HudLineHeight;
	if (Items.Num() == 0 || InViewportH <= 0)
	{
		ItemsTopPx = 0.0f;
		return;
	}
	// Same vertical center as NativePaint / FDebugOverlay::MeasureText (14 * scale per line).
	const int32 Lines = CountLines(BuildPaintText());
	const float H = LineH * static_cast<float>(FMath::Max(Lines, 1));
	float Top = (static_cast<float>(InViewportH) - H) * 0.5f;
	Top = FMath::Clamp(Top, 10.0f, FMath::Max(10.0f, static_cast<float>(InViewportH) - H - 10.0f));
	ItemsTopPx = Top + LineH * static_cast<float>(ItemStartLine(Title.ToString()));
}

void UMenuListWidget::NativePaint(FPaintContext& Ctx)
{
	if (Items.Num() == 0)
	{
		return;
	}
	CacheLayout(Ctx.GetWidth(), Ctx.GetHeight());
	const FString Text = BuildPaintText();
	float W = 0.0f;
	float H = 0.0f;
	Ctx.MeasureText(Text, HudFontScale, W, H);
	float Top = (static_cast<float>(Ctx.GetHeight()) - H) * 0.5f;
	Top = FMath::Clamp(Top, 10.0f, FMath::Max(10.0f, static_cast<float>(Ctx.GetHeight()) - H - 10.0f));
	Ctx.DrawText(Text, static_cast<float>(Ctx.GetWidth()) * 0.5f, Top, Color, HudFontScale, ETextJustify::Center);
}

FName UMenuListWidget::TickInput(FGenericWindow& Window, bool bCursorCaptured, float DeltaTime)
{
	if (Items.Num() == 0)
	{
		return NAME_None;
	}
	if (IgnoreActivateSeconds > 0.0f)
	{
		IgnoreActivateSeconds = FMath::Max(0.0f, IgnoreActivateSeconds - DeltaTime);
	}

	const bool bUp = Window.IsKeyPressed(EKeys::Up) || Window.IsKeyPressed(EKeys::W);
	const bool bDown = Window.IsKeyPressed(EKeys::Down) || Window.IsKeyPressed(EKeys::S);
	const bool bEnter = Window.IsKeyPressed(EKeys::Enter) || Window.IsKeyPressed(EKeys::NumPadEnter) ||
		Window.IsKeyPressed(EKeys::SpaceBar);
	const bool bMouse = Window.IsMouseButtonDown(EMouseButtons::Left);

	auto StepSelectable = [this](int32 Delta)
	{
		const int32 N = Items.Num();
		if (N <= 0)
		{
			return;
		}
		int32 Idx = Selected;
		for (int32 Guard = 0; Guard < N; ++Guard)
		{
			Idx = (Idx + Delta + N) % N;
			if (!Items[Idx].Id.IsNone())
			{
				Selected = Idx;
				return;
			}
		}
	};

	if (bUp && !bUpWasDown)
	{
		StepSelectable(-1);
	}
	if (bDown && !bDownWasDown)
	{
		StepSelectable(1);
	}

	const bool bAllowKeyboardActivate = IgnoreActivateSeconds <= 0.0f;
	FName Activated = NAME_None;
	if (bAllowKeyboardActivate && bEnter && !bEnterWasDown)
	{
		Activated = Items[Selected].Id;
	}

	// Mouse activate is never gated by the travel lockout (only edges / capture).
	if (bMouse && !bMouseWasDown && !bCursorCaptured)
	{
		const float My = Window.GetCursorPos().Y;
		int32 WinW = 0;
		int32 WinH = 0;
		Window.GetWindowSize(WinW, WinH);
		int32 FbW = 0;
		int32 FbH = 0;
		Window.GetFramebufferSize(FbW, FbH);
		WinW = FMath::Max(WinW, 1);
		WinH = FMath::Max(WinH, 1);
		if (FbW > 0 && FbH > 0)
		{
			CacheLayout(FbW, FbH);
			const float FbY = My * static_cast<float>(FbH) / static_cast<float>(WinH);
			// Inclusive band with a small pad so the first row is not a dead zone.
			const float Rel = FbY - ItemsTopPx + (LineH * 0.15f);
			const int32 Hit = FMath::FloorToInt(Rel / LineH);
			if (Hit >= 0 && Hit < Items.Num())
			{
				Selected = Hit;
				if (!Items[Selected].Id.IsNone())
				{
					Activated = Items[Selected].Id;
				}
			}
		}
	}

	bUpWasDown = bUp;
	bDownWasDown = bDown;
	bEnterWasDown = bEnter;
	bMouseWasDown = bMouse;
	return Activated;
}
