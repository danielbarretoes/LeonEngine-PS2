#pragma once

#include "IfBlock.generated.h"

UCLASS()
class UIfBlock : public UObject
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	UPROPERTY()
	int32 EditorValue;
#endif
};
