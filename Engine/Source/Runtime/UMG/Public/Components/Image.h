#pragma once

#include "Blueprint/UserWidget.h"

#include <glm/vec3.hpp>

/// Unreal-like UImage (lite): solid tinted rect (no texture brush yet — HUD DrawRect only).
/// Useful as panel chrome, health backdrop, letterbox bars.
class UMG_API UImage : public UUserWidget
{
public:
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

	void SetColor(const glm::vec3& InColor)
	{
		Color = InColor;
	}
	[[nodiscard]] const glm::vec3& GetColor() const
	{
		return Color;
	}

	void SetBorderColor(const glm::vec3& InColor)
	{
		BorderColor = InColor;
	}
	void SetDrawBorder(bool bEnabled)
	{
		bDrawBorder = bEnabled;
	}

	/// Stretch to full framebuffer each paint (dim overlay / letterbox).
	void SetFillScreen(bool bEnabled)
	{
		bFillScreen = bEnabled;
	}

	void NativePaint(FPaintContext& Ctx) override;

private:
	float X = 0.0f;
	float Y = 0.0f;
	float W = 64.0f;
	float H = 64.0f;
	bool bDrawBorder = false;
	bool bFillScreen = false;
	glm::vec3 Color{0.08f, 0.08f, 0.10f};
	glm::vec3 BorderColor{0.45f, 0.38f, 0.22f};
};
