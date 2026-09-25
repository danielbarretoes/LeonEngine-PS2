#pragma once

#include "CoreMinimal.h"

class FCanvas;

/**
 * The engine's on-screen debug text (UE: UEngine's on-screen debug messages and the stat text the viewport client
 * draws): the top-left block and timed console (AddOnScreenDebugMessage), the bottom-left toggle hints, the centred
 * status and the right-aligned stats. It keeps the text between frames and draws it into each frame's canvas, behind
 * the HUD's widgets (depth sort key 1).
 */
class ENGINE_API FDebugOverlay
{
public:
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

	/** Forgets every text and message. */
	void Clear();

	/** Draws the text blocks and the messages into the canvas (depth sort key 1, behind the HUD). */
	void Draw(FCanvas& Canvas) const;

private:
	struct FOnScreenMessage
	{
		FString Text;
		float TimeRemaining = 0.0f;
		float Duration = 0.0f;
		FLinearColor Color = FLinearColor(1.0f, 0.0f, 0.0f);
	};

	FString Text;
	FString BottomLeftText;
	FString CenterText;
	FString RightText;
	float RightTextOriginY = 10.0f;
	TArray<FOnScreenMessage> OnScreenMessages;
};
