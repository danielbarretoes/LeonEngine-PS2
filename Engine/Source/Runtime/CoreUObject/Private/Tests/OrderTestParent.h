// Registration-order fixture: the parent class of UOrderTestChild. Headers are registered in path order, so
// OrderTestChild.h (and its class) comes before this one.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OrderTestParent.generated.h"

/** Counts the class default objects of the order fixtures, to observe their construction order. */
struct FOrderTestSequence
{
	static int32 Next()
	{
		static int32 Counter = 0;
		return ++Counter;
	}
};

UCLASS()
class UOrderTestParent : public UObject
{
	GENERATED_BODY()

public:
	UOrderTestParent();

	UPROPERTY()
	int32 ParentValue = 11;

	/** Order in which this class's default object was constructed (0 on instances). */
	int32 DefaultObjectSequence = 0;
};
