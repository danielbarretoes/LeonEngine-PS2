#pragma once

#include "NoExportArray.generated.h"

#if !CPP
USTRUCT(noexport)
struct FNoExportArray
{
	UPROPERTY()
	float M[16];
};
#endif
