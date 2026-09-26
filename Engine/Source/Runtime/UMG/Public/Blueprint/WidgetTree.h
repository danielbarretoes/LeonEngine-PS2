#pragma once

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "WidgetTree.generated.h"

/** A user widget's widgets: their root, and the factory that makes them inside the tree (UE: UWidgetTree). */
UCLASS()
class UMG_API UWidgetTree : public UObject
{
	GENERATED_BODY()

public:
	UWidgetTree(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: RootWidget. */
	UPROPERTY()
	UWidget* RootWidget = nullptr;

	/** A new widget of WidgetClass inside the tree (UE: ConstructWidget). */
	template <typename WidgetT>
	WidgetT* ConstructWidget(TSubclassOf<UWidget> WidgetClass = WidgetT::StaticClass(), FName WidgetName = NAME_None)
	{
		return Cast<WidgetT>(NewObject<UWidget>(this, WidgetClass, WidgetName));
	}
};
