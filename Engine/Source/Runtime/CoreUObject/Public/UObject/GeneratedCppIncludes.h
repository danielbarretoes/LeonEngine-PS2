#pragma once

// Everything a LeonHeaderTool .gen.cpp / .init.gen.cpp needs (UE: UObject/GeneratedCppIncludes.h).

#include "CoreMinimal.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/ScriptMacros.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/Stack.h"
#include "UObject/UObjectBase.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <cstddef>

// The generated code takes STRUCT_OFFSET of members of UObject classes, which are not standard-layout; GCC and Clang
// warn about offsetof on them (-Winvalid-offsetof). The offsets are what the compiler lays out, as UE relies on, so
// the warning is off for the rest of the generated translation unit (UE does the same through its toolchains).
#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

/** The offset of a member (UE: STRUCT_OFFSET). */
#ifndef STRUCT_OFFSET
	#define STRUCT_OFFSET(struc, member) offsetof(struc, member)
#endif

/** The element count of a C array member (UE: CPP_ARRAY_DIM). */
#define CPP_ARRAY_DIM(ArrayName, ClassName) (sizeof(((ClassName*)0)->ArrayName) / sizeof(((ClassName*)0)->ArrayName[0]))
