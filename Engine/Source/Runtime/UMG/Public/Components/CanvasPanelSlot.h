#pragma once

#include "Components/PanelSlot.h"
#include "CoreMinimal.h"
#include "CanvasPanelSlot.generated.h"

/**
 * A child's rectangle in a canvas panel, from the panel's top-left corner (UE: UCanvasPanelSlot; its anchors are
 * always the top-left corner in Leon).
 */
UCLASS()
class UMG_API UCanvasPanelSlot : public UPanelSlot
{
	GENERATED_BODY()

public:
	UCanvasPanelSlot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: SetPosition / GetPosition. */
	void SetPosition(const FVector2D& InPosition)
	{
		Position = InPosition;
	}
	[[nodiscard]] const FVector2D& GetPosition() const
	{
		return Position;
	}
	/** The child's size when it does not size itself (UE: SetSize / GetSize). */
	void SetSize(const FVector2D& InSize)
	{
		Size = InSize;
	}
	[[nodiscard]] const FVector2D& GetSize() const
	{
		return Size;
	}
	/** The child takes its desired size (UE: SetAutoSize / GetAutoSize). */
	void SetAutoSize(bool bInAutoSize)
	{
		bAutoSize = bInAutoSize;
	}
	[[nodiscard]] bool GetAutoSize() const
	{
		return bAutoSize;
	}

private:
	FVector2D Position = FVector2D::ZeroVector;
	/** UE's default offsets: 100 by 30. */
	FVector2D Size = FVector2D(100.0f, 30.0f);
	bool bAutoSize = false;
};
