// UHT parses with CPP = 0: #if CPP and #if 0 branches are skipped, #if !CPP and #else branches are parsed.
#pragma once

#include "CoreMinimal.h"
#include "CppBlocks.generated.h"

#if 0
this is not C++ at all UCLASS( {
#endif

UCLASS()
class UCppBlocksObject : public UObject
{
	GENERATED_BODY()

public:
#if CPP
	int32 OnlySeenByTheCompiler;
#else
	UPROPERTY()
	int32 SeenByTheHeaderTool;
#endif

#if !CPP
	UPROPERTY()
	float NotCpp;
#endif
};
