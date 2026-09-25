#include "Components/MenuListWidget.h"

#include "GenericPlatform/GenericWindow.h"
#include "InputCoreTypes.h"

#include <algorithm>
#include <cmath>

namespace
{

	/// Lines before the first menu item in BuildPaintText (title + blank separator).
	[[nodiscard]] int ItemStartLine(const std::string& InTitle)
	{
		if (InTitle.empty())
		{
			return 1; // leading "\n\n" still inserts one blank row before items
		}
		int TitleLines = 1;
		for (char C : InTitle)
		{
			if (C == '\n')
			{
				++TitleLines;
			}
		}
		return TitleLines + 1;
	}

} // namespace

void UMenuListWidget::SetItems(std::vector<FItem> InItems)
{
	Items = std::move(InItems);
	if (Items.empty())
	{
		Selected = 0;
		return;
	}
	Selected = std::clamp(Selected, 0, static_cast<int>(Items.size()) - 1);
	// Prefer a selectable row (non-empty id) — status labels are not activatable.
	if (Items[static_cast<std::size_t>(Selected)].Id.empty())
	{
		for (int I = 0; I < static_cast<int>(Items.size()); ++I)
		{
			if (!Items[static_cast<std::size_t>(I)].Id.empty())
			{
				Selected = I;
				break;
			}
		}
	}
}

void UMenuListWidget::SetSelectedIndex(int Index)
{
	if (Items.empty())
	{
		Selected = 0;
		return;
	}
	Selected = std::clamp(Index, 0, static_cast<int>(Items.size()) - 1);
}

void UMenuListWidget::ResetEdges()
{
	bUpWasDown = bDownWasDown = bEnterWasDown = bMouseWasDown = true;
	// Keyboard only — mouse must stay usable on the first click after travel.
	IgnoreActivateSeconds = 0.35f;
}

int UMenuListWidget::CountLines(const std::string& Text) const
{
	if (Text.empty())
	{
		return 0;
	}
	int N = 1;
	for (char C : Text)
	{
		if (C == '\n')
		{
			++N;
		}
	}
	return N;
}

std::string UMenuListWidget::BuildPaintText() const
{
	std::string Text = Title;
	Text += "\n\n";
	for (int I = 0; I < static_cast<int>(Items.size()); ++I)
	{
		Text += (I == Selected) ? "> " : "  ";
		Text += Items[static_cast<std::size_t>(I)].Label;
		Text += '\n';
	}
	if (!Hint.empty())
	{
		Text += "\n";
		Text += Hint;
	}
	return Text;
}

void UMenuListWidget::CacheLayout(int /*viewportW*/, int InViewportH)
{
	ViewportH = InViewportH;
	LineH = HudLineHeight;
	if (Items.empty() || InViewportH <= 0)
	{
		ItemsTopPx = 0.0f;
		return;
	}
	// Same vertical center as NativePaint / FDebugOverlay::MeasureText (14 * scale per line).
	const int Lines = CountLines(BuildPaintText());
	const float H = LineH * static_cast<float>(std::max(Lines, 1));
	float Top = (static_cast<float>(InViewportH) - H) * 0.5f;
	Top = std::clamp(Top, 10.0f, std::max(10.0f, static_cast<float>(InViewportH) - H - 10.0f));
	ItemsTopPx = Top + LineH * static_cast<float>(ItemStartLine(Title));
}

void UMenuListWidget::NativePaint(FPaintContext& Ctx)
{
	if (Items.empty())
	{
		return;
	}
	CacheLayout(Ctx.GetWidth(), Ctx.GetHeight());
	const std::string Text = BuildPaintText();
	float W = 0.0f;
	float H = 0.0f;
	Ctx.MeasureText(Text, HudFontScale, W, H);
	float Top = (static_cast<float>(Ctx.GetHeight()) - H) * 0.5f;
	Top = std::clamp(Top, 10.0f, std::max(10.0f, static_cast<float>(Ctx.GetHeight()) - H - 10.0f));
	Ctx.DrawText(Text, static_cast<float>(Ctx.GetWidth()) * 0.5f, Top, Color, HudFontScale, ETextJustify::Center);
}

std::string UMenuListWidget::TickInput(FGenericWindow& Window, bool bCursorCaptured, float DeltaTime)
{
	if (Items.empty())
	{
		return {};
	}
	if (IgnoreActivateSeconds > 0.0f)
	{
		IgnoreActivateSeconds = std::max(0.0f, IgnoreActivateSeconds - DeltaTime);
	}

	const bool bUp = Window.IsKeyPressed(EKeys::Up) || Window.IsKeyPressed(EKeys::W);
	const bool bDown = Window.IsKeyPressed(EKeys::Down) || Window.IsKeyPressed(EKeys::S);
	const bool bEnter = Window.IsKeyPressed(EKeys::Enter) || Window.IsKeyPressed(EKeys::NumPadEnter) ||
		Window.IsKeyPressed(EKeys::SpaceBar);
	const bool bMouse = Window.IsMouseButtonDown(EMouseButtons::Left);

	auto StepSelectable = [this](int Delta)
	{
		const int N = static_cast<int>(Items.size());
		if (N <= 0)
		{
			return;
		}
		int Idx = Selected;
		for (int Guard = 0; Guard < N; ++Guard)
		{
			Idx = (Idx + Delta + N) % N;
			if (!Items[static_cast<std::size_t>(Idx)].Id.empty())
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
	std::string Activated;
	if (bAllowKeyboardActivate && bEnter && !bEnterWasDown)
	{
		const std::string& LocalId = Items[static_cast<std::size_t>(Selected)].Id;
		if (!LocalId.empty())
		{
			Activated = LocalId;
		}
	}

	// Mouse activate is never gated by travel lockout (only edges / capture).
	if (bMouse && !bMouseWasDown && !bCursorCaptured)
	{
		const double My = Window.GetCursorPos().Y;
		int WinW = 0;
		int WinH = 0;
		Window.GetWindowSize(WinW, WinH);
		int FbW = 0;
		int FbH = 0;
		Window.GetFramebufferSize(FbW, FbH);
		WinW = std::max(WinW, 1);
		WinH = std::max(WinH, 1);
		if (FbW > 0 && FbH > 0)
		{
			CacheLayout(FbW, FbH);
			const float FbY = static_cast<float>(My) * static_cast<float>(FbH) / static_cast<float>(WinH);
			// Inclusive band with small pad so the first row is not a dead zone.
			const float Rel = FbY - ItemsTopPx + (LineH * 0.15f);
			const int Hit = static_cast<int>(std::floor(Rel / LineH));
			if (Hit >= 0 && Hit < static_cast<int>(Items.size()))
			{
				Selected = Hit;
				const std::string& LocalId = Items[static_cast<std::size_t>(Selected)].Id;
				if (!LocalId.empty())
				{
					Activated = LocalId;
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
