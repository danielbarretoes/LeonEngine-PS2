// Unknown specifiers are warnings; known ones without an effect (meta, Category...) are silent.
#pragma once

#include "CoreMinimal.h"
#include "Specifiers.generated.h"

UENUM(BlueprintType, Shiny)
enum class ESpecifierTest : uint8
{
	One UMETA(DisplayName = "One", ToolTip = "The first")
};

USTRUCT(BlueprintType, Glittery)
struct FSpecifierStruct
{
	GENERATED_BODY()
};

UCLASS(Blueprintable, ClassGroup = (Custom), HideCategories = (Rendering, Physics), Sparkly)
class USpecifierObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "A|B", meta = (ClampMin = "0.0", UIMin = 0), FancyFlag)
	float Tuned = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Tests", Mystery = Value)
	void Run();
};
