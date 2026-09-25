#pragma once

#include "CoreMinimal.h"
#include "Fonts/TextLayout.h"
#include "Shader.h"

/**
 * Immediate-mode screen text: top-right stats, bottom-left hints, bottom-right chrome,
 * and top-left timed debug console (Unreal-like AddOnScreenDebugMessage).
 */
class RENDERER_API FDebugOverlay
{
public:
	bool Initialize(const FString& ShaderDirectory);
	void Shutdown();

	/** Optional top-left block (unused by default Engine HUD — messages live there). */
	void SetText(const FString& InText);
	/** Bottom-left block (e.g. F1/F2 debug toggles), left-aligned. */
	void SetBottomLeftText(const FString& InText);
	/** Horizontally + vertically centered block (menus / status). */
	void SetCenterText(const FString& InText);
	/** Right-aligned block (Engine stats top-right / level chrome bottom-right). */
	void SetRightText(const FString& InText);
	/** Vertical origin for SetRightText (default top margin). */
	void SetRightTextOriginY(float OriginY);

	/**
	 * Queues a temporary message (top-left console). Newest stays at the top; older
	 * lines shift down. Default color is red; duration and color are configurable.
	 */
	void AddOnScreenDebugMessage(const FString& Message, float DisplaySeconds = 2.0f,
		const FLinearColor& InColor = FLinearColor(1.0f, 0.0f, 0.0f));
	void TickOnScreenMessages(float DeltaTime);

	/** Screen-space geometry for UUserWidget / HUD (cleared each Paint). Pixel coords, top-left. */
	void ClearScreenGeometry();
	void AddScreenLine(
		float InX0, float InY0, float InX1, float InY1, const FLinearColor& InColor, float InThickness = 2.0f);
	void AddScreenRect(float InX, float InY, float InW, float InH, const FLinearColor& InColor);
	/** Widget text. InX / InY = left / center / right of the first line per InJustify. */
	void AddScreenText(const FString& InText, float InX, float InY, const FLinearColor& InColor,
		float InPixelScale = HudFontScale, ETextJustify InJustify = ETextJustify::Left);

	/** Measures multiline bitmap text at InPixelScale (width = longest line). */
	static void MeasureText(const FString& InText, float InPixelScale, float& OutWidth, float& OutHeight);

	void Draw(int32 FramebufferWidth, int32 FramebufferHeight);
	[[nodiscard]] EShaderReloadResult ReloadShader(bool bForce = false);

	[[nodiscard]] bool IsValid() const
	{
		return Shader.Valid() && Vao != 0;
	}

private:
	struct FOnScreenMessage
	{
		FString Text;
		float TimeRemaining = 0.0f;
		float Duration = 0.0f;
		FLinearColor Color = FLinearColor(1.0f, 0.0f, 0.0f);
	};

	struct FScreenLine
	{
		float X0 = 0.0f;
		float Y0 = 0.0f;
		float X1 = 0.0f;
		float Y1 = 0.0f;
		float Thickness = 2.0f;
		FLinearColor Color = FLinearColor::White;
	};

	struct FScreenRect
	{
		float X = 0.0f;
		float Y = 0.0f;
		float W = 0.0f;
		float H = 0.0f;
		FLinearColor Color = FLinearColor::White;
	};

	struct FScreenText
	{
		FString Text;
		float X = 0.0f;
		float Y = 0.0f;
		float PixelScale = HudFontScale;
		ETextJustify Justify = ETextJustify::Left;
		FLinearColor Color = FLinearColor::White;
	};

	void RebuildMesh(int32 FramebufferWidth, int32 FramebufferHeight);

	FShader Shader;
	FString Text;
	FString BottomLeftText;
	FString CenterText;
	FString RightText;
	float RightTextOriginY = 10.0f;
	TArray<FOnScreenMessage> OnScreenMessages;
	TArray<FScreenLine> ScreenLines;
	TArray<FScreenRect> ScreenRects;
	TArray<FScreenText> ScreenTexts;
	uint32 Vao = 0;
	uint32 Vbo = 0;
	int32 VertexCount = 0;
	bool bDirty = true;
	int32 BuiltForWidth = 0;
	int32 BuiltForHeight = 0;
};
