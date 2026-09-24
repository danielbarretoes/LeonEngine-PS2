#pragma once

#include "Blueprint/UserWidget.h"
#include "Fonts/TextLayout.h"

#include <glm/vec3.hpp>

#include <string>

class FGenericWindow;

/// Unreal-like UButton (lite): filled rect + label; hover / selected / press.
/// Usually owned by UVerticalBox; can also be a root HUD widget with SetPosition.
class UMG_API UButton : public UUserWidget
{
public:
	void SetId(std::string InId)
	{
		Id = std::move(InId);
	}
	[[nodiscard]] const std::string& GetId() const
	{
		return Id;
	}

	void SetLabel(std::string InLabel)
	{
		Label = std::move(InLabel);
	}
	[[nodiscard]] const std::string& GetLabel() const
	{
		return Label;
	}

	void SetPosition(float InX, float InY)
	{
		X = InX;
		Y = InY;
	}
	void SetSize(float InW, float InH)
	{
		W = InW;
		H = InH;
	}

	[[nodiscard]] float GetX() const
	{
		return X;
	}
	[[nodiscard]] float GetY() const
	{
		return Y;
	}
	[[nodiscard]] float GetWidth() const
	{
		return W;
	}
	[[nodiscard]] float GetHeight() const
	{
		return H;
	}

	void SetSelected(bool bInSelected)
	{
		bSelected = bInSelected;
	}
	[[nodiscard]] bool IsSelected() const
	{
		return bSelected;
	}

	void SetEnabled(bool bInEnabled)
	{
		bEnabled = bInEnabled;
	}
	[[nodiscard]] bool IsEnabled() const
	{
		return bEnabled;
	}

	void SetTextColor(const glm::vec3& Color)
	{
		TextColor = Color;
	}
	void SetBackgroundColor(const glm::vec3& Color)
	{
		BackgroundColor = Color;
	}
	void SetSelectedBackgroundColor(const glm::vec3& Color)
	{
		SelectedBackgroundColor = Color;
	}

	/// Preferred size for the current label (padding included).
	void MeasureDesiredSize(float& OutW, float& OutH) const;

	/// Hit-test in framebuffer pixels (top-left origin).
	[[nodiscard]] bool Contains(float FbX, float FbY) const;

	void NativePaint(FPaintContext& Ctx) override;

private:
	std::string Id;
	std::string Label = "Button";
	float X = 0.0f;
	float Y = 0.0f;
	float W = 160.0f;
	float H = HudLineHeight + 16.0f;
	bool bSelected = false;
	bool bEnabled = true;
	bool bHovered = false;

	glm::vec3 TextColor{1.0f, 0.92f, 0.75f};
	glm::vec3 BackgroundColor{0.12f, 0.12f, 0.14f};
	glm::vec3 SelectedBackgroundColor{0.28f, 0.22f, 0.10f};
	glm::vec3 HoverBackgroundColor{0.18f, 0.16f, 0.12f};
	glm::vec3 DisabledTextColor{0.45f, 0.45f, 0.45f};

	friend class UVerticalBox;
	void SetHovered(bool bInHovered)
	{
		bHovered = bInHovered;
	}
};
