// Registration-order fixture: a class whose header sorts before its parent's, so RegisterReflection records it first
// (ClassInfo is in header path order) and ProcessNewlyLoadedUObjects must build the parent through the child's
// DependentSingletons.
#pragma once

#include "CoreMinimal.h"
#include "Tests/OrderTestParent.h"
#include "OrderTestChild.generated.h"

UCLASS()
class UOrderTestChild : public UOrderTestParent
{
	GENERATED_BODY()

public:
	UOrderTestChild();

	UPROPERTY()
	int32 ChildValue = 22;
};
