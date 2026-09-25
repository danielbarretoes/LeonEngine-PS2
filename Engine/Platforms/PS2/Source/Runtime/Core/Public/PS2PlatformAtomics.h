#pragma once

#include "GenericPlatform/GenericPlatformAtomics.h"

/** Leon runs a single EE thread; the generic (non-atomic) operations apply. */
struct FPS2PlatformAtomics : public FGenericPlatformAtomics
{
};

typedef FPS2PlatformAtomics FPlatformAtomics;
