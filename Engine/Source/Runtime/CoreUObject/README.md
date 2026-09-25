# CoreUObject

`UObject` and its reflection, as in UE 4.27's `Runtime/CoreUObject` (plan phases P9 and P10): the object model and
reflection (P9), then garbage collection, weak / strong / soft references, `UPROPERTY(Config)` and `UFUNCTION(Exec)`
(P10). The module depends on Core only, builds for every platform (PS2 included) in C++17 without RTTI or exceptions,
and is the runtime side of LeonHeaderTool's generated code
([LeonHeaderTool/README.md](../../Programs/LeonHeaderTool/README.md) is the contract).

No engine module is reflected yet: `LeonAutomationTests` and `TestPAL` link CoreUObject for its tests; the gameplay
classes become `UCLASS` types in P12.

## Headers

| Header | Contents |
| --- | --- |
| `UObject/ObjectMacros.h` | the flag enums (`EObjectFlags`, `EInternalObjectFlags`, `EClassFlags`, `EClassCastFlags`, `EPropertyFlags`, `EStructFlags`, `EPackageFlags`, …), `UCLASS` / `USTRUCT` / `UPROPERTY` / `GENERATED_BODY`, `DECLARE_CLASS`, `IMPLEMENT_CLASS`, the constructor and vtable-helper macros, `StaticClass<T>` / `StaticStruct<T>` / `StaticEnum<T>` |
| `UObject/UObjectBase.h`, `UObjectBaseUtility.h`, `Object.h` | the object model: `UObjectBase` (flags, index, class, name, outer), `UObjectBaseUtility` (names, paths, outer chain, `IsA`, flags, `AddToRoot`, `MarkPendingKill`), `UObject` (`PostInitProperties`, `ProcessEvent`, `CreateDefaultSubobject`, `BeginDestroy` / `FinishDestroy`, `AddReferencedObjects`, `LoadConfig` / `SaveConfig` / `ReloadConfig`, `CallFunctionByNameWithArguments`, `ProcessConsoleExec`; the P11 stubs), `IsValid` |
| `UObject/Class.h` | `UField`, `UStruct`, `UScriptStruct` (`ICppStructOps`), `UClass`, `UEnum` (`_MAX` appended), `UFunction` |
| `UObject/Field.h`, `UObject/UnrealType.h` | `FField` / `FFieldClass` / `FProperty` and every property type: numeric, `FBoolProperty` (native bools and bitfields), `FByteProperty` / `FEnumProperty`, `FStrProperty` / `FNameProperty` / `FTextProperty`, object / class / weak / soft references, `FStructProperty`, `FArrayProperty` / `FSetProperty` / `FMapProperty` with their script helpers; `TFieldIterator`, `TFieldRange` |
| `UObject/UObjectGlobals.h` | `NewObject`, `FObjectInitializer`, `StaticConstructObject_Internal`, `StaticFindObject` / `FindObject`, `MakeUniqueObjectName`, `CreatePackage`, `GetTransientPackage`, `GetDefault`, `CollectGarbage` / `TryCollectGarbage` / `IncrementalPurgeGarbage`, `FReferenceCollector`, `GARBAGE_COLLECTION_KEEPFLAGS`, the `UE4CodeGen_Private` params and `Construct*` functions |
| `UObject/GarbageCollection.h`, `GCObject.h`, `StrongObjectPtr.h` | `LogGarbage`, `FGarbageCollectionStats`, `FGarbageCollectionSettings` / `FGarbageCollectionTimer`; `FGCObject`; `TStrongObjectPtr` |
| `UObject/UObjectArray.h`, `UObjectHash.h`, `UObjectIterator.h` | `GUObjectArray` (`FUObjectArray` / `FUObjectItem`), the name hash, `GetObjectsWithOuter` / `GetObjectsOfClass`, `TObjectIterator` / `TObjectRange` |
| `UObject/Package.h` | `UPackage` (`/Script/<Module>` packages; P11 loads `.lasset` packages) |
| `UObject/Stack.h`, `Script.h`, `ScriptMacros.h` | `FFrame`, `EFunctionFlags`, `DECLARE_FUNCTION` / `DEFINE_FUNCTION`, the `P_GET_*` family |
| `UObject/NoExportTypes.h` | `USTRUCT(noexport)` declarations of the Core structs (`FVector`, `FVector2D`, `FVector4`, `FPlane`, `FRotator`, `FQuat`, `FTransform`, `FColor`, `FLinearColor`, `FGuid`, `FIntPoint`, `FIntVector`, `FBox`) and of `FSoftObjectPath` / `FSoftClassPath` |
| `UObject/WeakObjectPtr*.h`, `PersistentObjectPtr.h`, `SoftObjectPath.h`, `SoftObjectPtr.h` | `FWeakObjectPtr` / `TWeakObjectPtr` (index + serial number; Core's `UObject/WeakObjectPtrTemplatesFwd.h` names it for the delegates), `TPersistentObjectPtr`, `FSoftObjectPath` / `FSoftClassPath` (4.27 layout: `AssetPathName` + `SubPathString`), `FSoftObjectPtr`, `TSoftObjectPtr` / `TSoftClassPtr` (resolve objects already in memory until P11 loads packages) |
| `Templates/Casts.h`, `Templates/SubclassOf.h` | `Cast` (class cast flags as the fast path, else the super chain), `CastChecked`, `ExactCast`, `TSubclassOf` |
| `CoreUObject.h` | everything above |

## Startup and registration

For each statically linked module, `FModuleManager::StartupStaticallyLinkedModules` does:

1. `InitializeModule_<Module>()`;
2. the module's `RegisterReflection` (LeonHeaderTool's `RegisterReflection_<Module>`), which calls
   `RegisterCompiledInInfo`: it only records the classes, structs and enums;
3. `OnProcessLoadedObjectsCallback` (UE's hook in `FModuleManager`; Core never depends on CoreUObject);
4. `StartupModule`.

CoreUObject's own `StartupModule` runs `UObjectBaseInit` (allocates `GUObjectArray`, registers the intrinsic classes
`UObject`, `UField`, `UStruct`, `UScriptStruct`, `UClass`, `UEnum`, `UFunction`, `UPackage`, creates the transient
package), processes what was recorded so far and binds the callback to `ProcessNewlyLoadedUObjects`. From then on every
module's types are constructed before its `StartupModule`: the `/Script/<Module>` packages, then enums, structs and
classes (a class may be recorded before its super; `DependentSingletons` construct supers first), then the properties
are linked and the class default objects created, parents first. Modules started before CoreUObject cannot be
reflected (they would depend on it).

## Objects

- `NewObject<T>(Outer, Class, Name, Flags, Template)` → `StaticConstructObject_Internal` → `StaticAllocateObject`
  (memory, unique name) → the class constructor with an `FObjectInitializer` → `PostConstructInit` (properties from the
  CDO or the template, `PostInitProperties`). An existing object with the same name and outer is a fatal error (UE
  would replace it); an abstract class is a fatal error.
- The object's class, name, outer and flags reach `UObjectBase()` through the construction context
  (`FUObjectThreadContext`), which registers it in `GUObjectArray` and the name hash.
- `CreateDefaultSubobject` works inside constructors, with `FObjectInitializer::SetDefaultSubobjectClass` /
  `DoNotCreateDefaultSubobject` overrides. **Every instance builds its own subobjects** (plan decision D12). When an
  object is created from a template, a copied reference to one of the template's default subobjects is redirected to
  the new object's subobject of the same name.
- `GUObjectArray` has a fixed capacity, `FPlatformProperties::MaxObjectsInGame`: 8192 on the PS2 (12-byte slots,
  96 KB) and 131072 on desktop (16-byte slots, 2 MB). Running out is a fatal error that logs the capacity. The garbage
  collector frees slots, and freed slots are reused (last freed first).

## Garbage collection

`CollectGarbage(KeepFlags, bPerformFullPurge)` is UE4's stop-the-world mark and sweep (plan decision D11), on one
thread and without clusters:

1. **Roots.** Every object starts unreachable except: the root set (`AddToRoot`; the transient package and every
   class are rooted), native objects (`EInternalObjectFlags::Native`: what the registration made, the
   `RF_MarkAsNative` objects — classes, functions, structs, enums), class default objects and everything inside them,
   the compiled-in `/Script/<Module>` packages, and objects with one of `KeepFlags` (`GARBAGE_COLLECTION_KEEPFLAGS` is
   `RF_NoFlags`: UE passes `RF_Standalone` only in the editor). A pending-kill object is never a root through its
   flags. Then every `FGCObject` reports its references.
2. **Mark.** Each reachable object marks its outer and its class, the strong references its class lists
   (`UClass::ReferenceTokenStream`: `UObject*` and `TSubclassOf` members, C arrays of them, and `TArray` / `TSet` /
   `TMap` / `USTRUCT` members holding them, assembled on first use from `RefLink`), and whatever its class's static
   `AddReferencedObjects` reports. `TWeakObjectPtr` and soft references do not keep objects alive.
3. **Pending kill.** A strong reference to an object marked with `MarkPendingKill` is cleared during the mark, so the
   object is collected even while referenced (UE 4.27); a set element or map key that is such a pointer is removed
   (clearing it would break the hash). The outer and class references are never cleared: an object whose inner
   objects are still reachable survives, pending kill or not.
4. **Sweep.** Every object still unreachable gets `ConditionalBeginDestroy` (`UObject::BeginDestroy` renames it to
   `NAME_None`, out of the name hash), then, once every one is `IsReadyForFinishDestroy`, `ConditionalFinishDestroy`,
   then its destructor (which frees its `GUObjectArray` slot, so weak pointers to it go stale) and `FMemory::Free`. An
   override that does not call `Super::BeginDestroy` / `Super::FinishDestroy` is a fatal error. Without
   `bPerformFullPurge` the last two steps wait for `IncrementalPurgeGarbage`, which the next collection runs first.

Each collection logs `LogGarbage: Collected N of M objects in T ms (mark, purge), R references cleared` and keeps the
figures in `GetLastGarbageCollectionStats()`.

**When it runs.** Only at safe points, never on its own: nothing may hold an unreported `UObject*` across it (the
rule: a `UObject*` member is a `UPROPERTY`; a non-UObject holder is an `FGCObject` or uses `TStrongObjectPtr`). In
this phase only the tests and TestPAL call it. P12 / P13 call it where UE does: `UEngine::LoadMap` after the old world
is released (`CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS)`), the round restart of the game mode, and every frame's
`UEngine::ConditionalCollectGarbage` through `FGarbageCollectionTimer::Tick` after the world tick; the interval comes
from `[/Script/Engine.GarbageCollectionSettings] gc.TimeBetweenPurgingPendingKillObjects` in the engine config
(`FGarbageCollectionSettings::LoadFromConfig`, 61.1 s by default). `AActor::Destroy` will mark the actor pending kill
and the next collection frees it.

**References.**

- `TWeakObjectPtr` (index + serial number): `Get` / `IsValid` / `IsStale` / `IsExplicitlyNull` / `Reset`, comparison
  and `GetTypeHash`; a pending-kill or unreachable object reads as null (`Get(true)` still returns a pending-kill one).
- `TStrongObjectPtr` keeps its object alive through a private `FGCObject` (copyable, movable).
- `FGCObject`: construct to register, destroy to unregister; `AddReferencedObjects(FReferenceCollector&)` reports.
- `FSoftObjectPath` (`"/Game/Maps/Arena.Arena"` plus a subobject path after `:`), `FSoftClassPath`,
  `TSoftObjectPtr` / `TSoftClassPtr` over `FSoftObjectPtr` (a `TPersistentObjectPtr`: a path and a cached weak
  pointer, re-resolved when objects were created since). `ResolveObject` finds objects in memory; `TryLoad` /
  `LoadSynchronous` do the same until P11 loads packages. Both paths are reflected noexport structs whose text form is
  the path itself (`TStructOpsTypeTraits::WithExportTextItem` / `WithImportTextItem`).

## Config

`UCLASS(Config=Game)` (any name: `Engine`, `Game`, `Input`, `Editor` map to `GEngineIni` & co., another name loads its
own hierarchy) and `UPROPERTY(Config)` / `UPROPERTY(GlobalConfig)`, as in UE 4.27:

- **Section**: the class path, `/Script/<Module>.<Class without prefix>`. A `GlobalConfig` member always uses the
  section (and file) of the class that declares it. A `PerObjectConfig` class uses `"<ObjectName> <Class>"`
  (`OverridePerObjectConfigSection` may change it).
- **Values** are parsed with `FProperty::ImportText`: numbers, `True` / `False`, strings (the whole value), names,
  text, enum names, structs `(X=1,Y=2)`, sets `(A,B)`, maps `((Key,Value),...)`, objects and classes by path
  (`/Engine/Transient`, `Class'/Script/Engine.Actor'`), soft paths. A `TArray` takes the values the layers left for the
  key (`+Key=`, `.Key=`, after `-Key=` / `!Key` edits), or `Key[N]=` entries; a C array member reads `Key[N]=`.
- **When**: a class default object loads its config when it is created, after its parent's defaults were copied and
  before `PostInitProperties`, reading its parents' sections first (`LCPF_ReadParentSections`); an instance copies
  the config members from its class default object (`PostConstructLink`); a `PerObjectConfig` object reads its own
  section when it is created. `ReloadConfig` reloads and reaches the instances (`PostReloadConfig`).
- **Save**: `SaveConfig(Flags, Filename, Config, bAllowCopyToDefaultObject)` writes the members (arrays as values,
  C arrays as `Key[N]`, strings unquoted with `PPF_ConfigOnly`) and flushes the file's user layer
  (`<Project>/Saved/Config/<Platform>/<File>.ini`, desktop only, D8); on the PS2 it only logs. An instance also copies
  its values to its class default object.
- `GetConfigFilename(Object)`, `UClass::GetConfigName()`, `UObject::GetDefaultConfigFilename()`
  (`<Project>/Config/Default<Name>.ini`).

## Console commands

`UFUNCTION(Exec)` sets `FUNC_Exec`. `UObject::CallFunctionByNameWithArguments(Cmd, Ar, Executor)` finds the function
named by the first word, parses one argument per parameter with `FProperty::ImportText` (a quoted argument may hold
spaces; a last `FString` parameter takes the rest of the line; a first object parameter receives `Executor` when it
fits) and calls it through `ProcessEvent`. Unknown and non-Exec functions return false (`bForceCallWithNonExec` calls
the latter anyway); a bad argument is reported on `Ar` and the function is not called. `UObject::ProcessConsoleExec`
calls it; Core's `FExec` / `FSelfRegisteringExec` (`Misc/CoreMisc.h`) let non-UObjects publish commands. The console
itself arrives with P13 (`UGameViewportClient`).

## UObject delegates

`TDelegate::BindUObject` / `CreateUObject` and `TMulticastDelegate::AddUObject` hold the object through a
`TWeakObjectPtr` (Core names the template through `UObject/WeakObjectPtrTemplatesFwd.h`): the binding does not keep
the object alive, `IsBound()` is false once it is collected or pending kill, `Broadcast` skips it, and `Add` drops dead
bindings. `RemoveAll(Object)` removes the bindings of a live object.

## Script containers

`FScriptArray`, `FScriptSparseArray`, `FScriptSet` and `FScriptMap` (in Core, next to the containers they view) are
type-erased views with exactly the layout of `TArray`, `TSparseArray`, `TSet` and `TMap`. `CheckConstraints` in each
`static_assert`s the member sizes and offsets against the real container (Core's containers befriend them), and
CoreUObject instantiates the checks (`CheckScriptContainerLayouts` in `PropertyContainers.cpp`). `FScriptArrayHelper`, `FScriptSetHelper` and
`FScriptMapHelper` add, remove, rehash, copy and destroy elements through the inner properties, so a reflected
`TArray` / `TSet` / `TMap` can be initialized, copied, compared, exported and imported as text without its C++ type.

## NoExport structs

The Core math structs are defined in Core, which knows nothing about reflection. `NoExportTypes.h` redeclares them
inside `#if !CPP` with `USTRUCT(noexport)`; LeonHeaderTool generates `Z_Construct_UScriptStruct_FVector` & co. with the
offsets of the real Core types and `static_assert`s that the declaration matches them (size, member types, member
offsets). `FTransform`'s members are private, so it befriends `Z_Construct_UScriptStruct_FTransform_Statics`. `FMatrix`
is not reflected (its layout is `float M[4][4]`, a C array of C arrays).

## Deviations from UE 4.27

- Garbage collection: no disregard-for-GC pool (class default objects, native objects and compiled-in packages are
  roots by their flags instead), no clusters, no token stream (the collector walks the class's list of strong
  reference properties), single-threaded, no `UGCObjectReferencer` object (the `FGCObject` list lives in the
  collector). Sets and maps of pointers lose the element / pair of a pending-kill object instead of keeping a null
  key. `GARBAGE_COLLECTION_KEEPFLAGS` is `RF_NoFlags` (no editor).
- Config: no `GlobalUserConfig` / `ProjectUserConfig` classes (LeonHeaderTool rejects them: D8 has no per-user global
  layer), no `UpdateDefaultConfigFile` for `DefaultConfig` classes (the editor module, P14, will write
  `Default<Name>.ini`), no console variables (`gc.*` keys are read directly). An instance reloaded through
  `ReloadConfig` reads its class's sections parents first, as its class default object did.
- Exec: no `CPP_Default_` metadata, so a missing trailing argument keeps its zero / default value with a warning on
  `Ar` (UE uses the C++ default, or fails when there is none). No `BindUFunction` and no dynamic delegates.
- No packages on disk, `Serialize` is a stub, soft references only resolve objects already in memory (P11); a soft
  pointer re-resolves after any object is created (UE: after a package loads).
- No script VM: `FFrame::Code` is always null and `ProcessEvent` calls native thunks only.
- Default subobjects are rebuilt per instance instead of instanced from the archetype (D12).
- `MakeUniqueObjectName` numbers per class (UE 4.27 per outer); `StaticAllocateObject` does not replace an existing
  object.
- The name hash is a `TMultiMap` keyed by the name's hash; `GetObjectsOfClass` walks `GUObjectArray` (no per-class
  hash).
- `FUObjectItem` has no cluster index; the object array does not grow.
- No metadata, no hot reload (`ClassVTableHelperCtorCaller` is never called).

## Tests

`Private/Tests`: 48 `System.CoreUObject.<Area>.<Name>` automation tests (objects and names, classes and casts, CDOs
and subobjects, registration order, properties, bools, enums, struct ops, containers, functions, NoExport structs,
`WITH_EDITORONLY_DATA`; garbage collection, weak / strong / soft references, `TSubclassOf`, UObject delegates, config
load / defaults / per-object / save, Exec) with reflected fixtures (`ReflectionTestTypes.h`, `HierarchyTestTypes.h`,
`OrderTestParent.h`, `OrderTestChild.h`, `GarbageCollectionTestTypes.h`, `ConfigExecTestTypes.h`). They run in
`LeonAutomationTests` and in `TestPAL` on every platform, PS2 included (`System.CoreUObject.Config.SaveConfig` is
desktop-only: it writes a user layer under `<Project>/Intermediate/Tests/`). TestPAL also logs the reflection budget,
the GC cost (`System.CoreUObject.GarbageCollection.Budget`) and a final collection (see
[Budgets.md](../../../Platforms/PS2/Documentation/Budgets.md)).
