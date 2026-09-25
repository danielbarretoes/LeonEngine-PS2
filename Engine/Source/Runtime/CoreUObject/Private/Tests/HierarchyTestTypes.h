// Reflected fixtures of the CoreUObject automation tests: a class hierarchy with default subobjects.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HierarchyTestTypes.generated.h"

/** A default subobject class. */
UCLASS()
class UHierarchyTestComponent : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Power = 5;

	virtual int32 GetKind() const
	{
		return 10;
	}
};

/** A subclass a derived owner can substitute with SetDefaultSubobjectClass. */
UCLASS()
class UHierarchyTestSpecialComponent : public UHierarchyTestComponent
{
	GENERATED_BODY()

public:
	UHierarchyTestSpecialComponent();

	virtual int32 GetKind() const override
	{
		return 11;
	}
};

/** An abstract base: it has a default object but no instances. */
UCLASS(Abstract)
class UHierarchyTestBase : public UObject
{
	GENERATED_BODY()

public:
	UHierarchyTestBase(const FObjectInitializer& ObjectInitializer);

	UPROPERTY()
	int32 BaseValue = 1;

	virtual int32 GetKind() const
	{
		return 0;
	}
};

/** A class with a default subobject created in its constructor. */
UCLASS()
class UHierarchyTestChild : public UHierarchyTestBase
{
	GENERATED_BODY()

public:
	UHierarchyTestChild(const FObjectInitializer& ObjectInitializer);

	UPROPERTY()
	int32 ChildValue = 2;

	UPROPERTY()
	UHierarchyTestComponent* Component = nullptr;

	virtual int32 GetKind() const override
	{
		return 1;
	}
};

/** Overrides the class of its parent's default subobject. */
UCLASS()
class UHierarchyTestGrandChild : public UHierarchyTestChild
{
	GENERATED_BODY()

public:
	UHierarchyTestGrandChild(const FObjectInitializer& ObjectInitializer);

	UPROPERTY()
	float GrandChildValue = 3.0f;

	virtual int32 GetKind() const override
	{
		return 2;
	}
};

/** Owns an optional default subobject. */
UCLASS()
class UHierarchyTestOptionalOwner : public UObject
{
	GENERATED_BODY()

public:
	UHierarchyTestOptionalOwner(const FObjectInitializer& ObjectInitializer);

	UPROPERTY()
	UHierarchyTestComponent* Optional = nullptr;
};

/** Skips its parent's optional subobject with DoNotCreateDefaultSubobject. */
UCLASS()
class UHierarchyTestNoOptional : public UHierarchyTestOptionalOwner
{
	GENERATED_BODY()

public:
	UHierarchyTestNoOptional(const FObjectInitializer& ObjectInitializer);
};
