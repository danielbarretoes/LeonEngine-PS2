#pragma once

// The asynchronous loader's side the synchronous one uses (Docs/PLANS/ps2-shipping.md N24).

#include "CoreMinimal.h"

/**
 * The bytes of the package file Filename that an asynchronous load read, waiting for its read when it is still on its
 * way; false when no asynchronous load has them (or its read failed). The caller owns OutBytes (FMemory::Free).
 */
bool TakeAsyncPackageBytes(const TCHAR* Filename, uint8*& OutBytes, int64& OutSize);
