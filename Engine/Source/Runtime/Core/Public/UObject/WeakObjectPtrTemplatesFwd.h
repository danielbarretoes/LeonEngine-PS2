#pragma once

// Forward declaration of TWeakObjectPtr for Core code that names it without depending on CoreUObject: the UObject
// delegate bindings (UE: UObject/WeakObjectPtrTemplatesFwd.h). CoreUObject's UObject/WeakObjectPtrTemplates.h defines
// the template; code that instantiates a binding includes it.

class UObject;
struct FWeakObjectPtr;

template <class T = UObject, class TWeakObjectPtrBase = FWeakObjectPtr>
struct TWeakObjectPtr;
