#include "Components/VerticalBox.h"

#include "GenericPlatform/GenericWindow.h"
#include "InputCoreTypes.h"

UVerticalBox::UVerticalBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVerticalBox::ClearChildren()
{
	Buttons.Reset();
	Selected = 0;
}

bool UVerticalBox::IsSelectable(const UButton& Button)
{
	return !Button.GetId().IsNone() && Button.IsEnabled();
}

UButton* UVerticalBox::AddButton(FName Id, const FText& Label)
{
	UButton* Button = NewObject<UButton>(this);
	Button->SetId(Id);
	Button->SetLabel(Label);
	Buttons.Add(Button);
	SnapSelectionToSelectable();
	ApplySelectionVisuals();
	return Button;
}

UButton* UVerticalBox::GetButton(int32 Index)
{
	return Buttons.IsValidIndex(Index) ? Buttons[Index] : nullptr;
}

const UButton* UVerticalBox::GetButton(int32 Index) const
{
	return Buttons.IsValidIndex(Index) ? Buttons[Index] : nullptr;
}

void UVerticalBox::SetSelectedIndex(int32 Index)
{
	if (Buttons.Num() == 0)
	{
		Selected = 0;
		ApplySelectionVisuals();
		return;
	}
	Selected = FMath::Clamp(Index, 0, Buttons.Num() - 1);
	ApplySelectionVisuals();
}

void UVerticalBox::ResetEdges()
{
	bUpWasDown = bDownWasDown = bEnterWasDown = bMouseWasDown = true;
	// Keyboard only: the mouse must stay usable on the first click after travel.
	IgnoreActivateSeconds = 0.35f;
}

void UVerticalBox::SnapSelectionToSelectable()
{
	if (Buttons.Num() == 0)
	{
		Selected = 0;
		return;
	}
	Selected = FMath::Clamp(Selected, 0, Buttons.Num() - 1);
	if (IsSelectable(*Buttons[Selected]))
	{
		return;
	}
	for (int32 I = 0; I < Buttons.Num(); ++I)
	{
		if (IsSelectable(*Buttons[I]))
		{
			Selected = I;
			return;
		}
	}
}

void UVerticalBox::StepSelectable(int32 Delta)
{
	const int32 N = Buttons.Num();
	if (N <= 0)
	{
		return;
	}
	int32 Idx = Selected;
	for (int32 Guard = 0; Guard < N; ++Guard)
	{
		Idx = (Idx + Delta + N) % N;
		if (IsSelectable(*Buttons[Idx]))
		{
			Selected = Idx;
			ApplySelectionVisuals();
			return;
		}
	}
}

void UVerticalBox::ApplySelectionVisuals()
{
	for (int32 I = 0; I < Buttons.Num(); ++I)
	{
		Buttons[I]->SetSelected(I == Selected);
	}
}

void UVerticalBox::CacheLayout(int32 ViewportW, int32 ViewportH)
{
	if (ViewportW <= 0 || ViewportH <= 0)
	{
		return;
	}

	float MaxButtonW = MinButtonW;
	float ButtonsH = 0.0f;
	for (int32 I = 0; I < Buttons.Num(); ++I)
	{
		float Dw = 0.0f;
		float Dh = 0.0f;
		Buttons[I]->MeasureDesiredSize(Dw, Dh);
		MaxButtonW = FMath::Max(MaxButtonW, Dw);
		ButtonsH += Dh;
		if (I + 1 < Buttons.Num())
		{
			ButtonsH += ButtonGap;
		}
	}

	TitleH = 0.0f;
	if (!Title.IsEmpty())
	{
		float Tw = 0.0f;
		FPaintContext::MeasureTextOnly(Title.ToString(), HudFontScale, Tw, TitleH);
		TitleH += HudLineHeight; // blank separator under the title
	}

	HintH = 0.0f;
	if (!Hint.IsEmpty())
	{
		float Hw = 0.0f;
		FPaintContext::MeasureTextOnly(Hint.ToString(), HudFontScale, Hw, HintH);
		HintH += 12.0f; // gap above the hint
	}

	BoxW = MaxButtonW;
	const float TotalH = TitleH + ButtonsH + HintH;
	BoxX = (static_cast<float>(ViewportW) - BoxW) * 0.5f;
	BoxY = FMath::Clamp((static_cast<float>(ViewportH) - TotalH) * 0.5f, 10.0f,
		FMath::Max(10.0f, static_cast<float>(ViewportH) - TotalH - 10.0f));

	float Y = BoxY + TitleH;
	for (UButton* Button : Buttons)
	{
		float Dw = 0.0f;
		float Dh = 0.0f;
		Button->MeasureDesiredSize(Dw, Dh);
		Button->SetSize(BoxW, Dh);
		Button->SetPosition(BoxX, Y);
		Y += Dh + ButtonGap;
	}
}

void UVerticalBox::NativePaint(FPaintContext& Ctx)
{
	if (!IsVisible())
	{
		return;
	}
	CacheLayout(Ctx.GetWidth(), Ctx.GetHeight());

	if (!Title.IsEmpty())
	{
		Ctx.DrawText(Title.ToString(), static_cast<float>(Ctx.GetWidth()) * 0.5f, BoxY,
			FLinearColor(1.0f, 0.82f, 0.35f), HudFontScale, ETextJustify::Center);
	}

	for (UButton* Button : Buttons)
	{
		Button->NativePaint(Ctx);
	}

	if (!Hint.IsEmpty())
	{
		float ButtonsBottom = BoxY + TitleH;
		for (const UButton* Button : Buttons)
		{
			ButtonsBottom = Button->GetY() + Button->GetHeight();
		}
		const float HintY = ButtonsBottom + 12.0f;
		Ctx.DrawText(Hint.ToString(), static_cast<float>(Ctx.GetWidth()) * 0.5f, HintY,
			FLinearColor(0.65f, 0.65f, 0.60f), HudFontScale, ETextJustify::Center);
	}
}

FName UVerticalBox::TickInput(FGenericWindow& Window, bool bCursorCaptured, float DeltaTime)
{
	if (Buttons.Num() == 0)
	{
		return NAME_None;
	}
	if (IgnoreActivateSeconds > 0.0f)
	{
		IgnoreActivateSeconds = FMath::Max(0.0f, IgnoreActivateSeconds - DeltaTime);
	}

	int32 FbW = 0;
	int32 FbH = 0;
	Window.GetFramebufferSize(FbW, FbH);
	if (FbW > 0 && FbH > 0)
	{
		CacheLayout(FbW, FbH);
	}

	const bool bUp = Window.IsKeyPressed(EKeys::Up) || Window.IsKeyPressed(EKeys::W);
	const bool bDown = Window.IsKeyPressed(EKeys::Down) || Window.IsKeyPressed(EKeys::S);
	const bool bEnter = Window.IsKeyPressed(EKeys::Enter) || Window.IsKeyPressed(EKeys::NumPadEnter) ||
		Window.IsKeyPressed(EKeys::SpaceBar);
	const bool bMouse = Window.IsMouseButtonDown(EMouseButtons::Left);

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
		UButton* Button = GetButton(Selected);
		if (Button != nullptr && IsSelectable(*Button))
		{
			Activated = Button->GetId();
		}
	}

	// Hover + click (never gated by the travel lockout).
	if (!bCursorCaptured && FbW > 0 && FbH > 0)
	{
		const FVector2D Cursor = Window.GetCursorPos();
		int32 WinW = 0;
		int32 WinH = 0;
		Window.GetWindowSize(WinW, WinH);
		WinW = FMath::Max(WinW, 1);
		WinH = FMath::Max(WinH, 1);
		const float FbX = Cursor.X * static_cast<float>(FbW) / static_cast<float>(WinW);
		const float FbY = Cursor.Y * static_cast<float>(FbH) / static_cast<float>(WinH);

		for (UButton* Button : Buttons)
		{
			Button->SetHovered(Button->Contains(FbX, FbY));
		}

		if (bMouse && !bMouseWasDown)
		{
			for (int32 I = 0; I < Buttons.Num(); ++I)
			{
				UButton* Button = Buttons[I];
				if (!Button->Contains(FbX, FbY))
				{
					continue;
				}
				Selected = I;
				ApplySelectionVisuals();
				if (IsSelectable(*Button))
				{
					Activated = Button->GetId();
				}
				break;
			}
		}
	}
	else
	{
		for (UButton* Button : Buttons)
		{
			Button->SetHovered(false);
		}
	}

	bUpWasDown = bUp;
	bDownWasDown = bDown;
	bEnterWasDown = bEnter;
	bMouseWasDown = bMouse;
	return Activated;
}
