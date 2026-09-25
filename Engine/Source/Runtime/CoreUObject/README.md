# CoreUObject

`UObject` and its reflection, as in UE 4.27's `Runtime/CoreUObject` (plan phases P9 to P11): the object model and
reflection (P9), garbage collection, weak / strong / soft references, `UPROPERTY(Config)` and `UFUNCTION(Exec)` (P10),
then packages: saving and loading objects in `.lasset` / `.lmap` files (P11). The module depends on Core only, builds
for every platform (PS2 included) in C++17 without RTTI or exceptions, and is the runtime side of LeonHeaderTool's
generated code ([LeonHeaderTool/README.md](../../Programs/LeonHeaderTool/README.md) is the contract).

Since P12 the gameplay framework is built on it: `Engine` (actors, components, world, level, game instance, game mode
and state, controllers, HUD, player input, the anim instances; since P14 the asset classes), `AIModule`
(`AAIController`) and `UMG` (`UUserWidget` and the widgets) are reflected modules ([ARCHITECTURE.md §10](../../../../Docs/ARCHITECTURE.md#10-gameplay-framework-engine-desktop)
has the ownership, spawn and destroy flows); since P13 also the engine object (`UEngine` / `UGameEngine`), the viewport
client, the players, `EngineSettings` (`UGameMapsSettings`) and the input (`UInputSettings`, `UPlayerInput`, and
InputCore's reflected `FKey`). `LeonAutomationTests` and `TestPAL` link CoreUObject for its tests; the PS2 game links it
through InputCore and starts the object system, but has no UObject of its own yet.

## Headers

| Header | Contents |
| --- | --- |
| `UObject/ObjectMacros.h` | the flag enums (`EObjectFlags` with `RF_Load`, `EInternalObjectFlags`, `EClassFlags`, `EClassCastFlags`, `EPropertyFlags`, `EStructFlags`, `EPackageFlags`, `ELoadFlags`, `ESaveFlags`, …), `UCLASS` / `USTRUCT` / `UPROPERTY` / `GENERATED_BODY`, `DECLARE_CLASS`, `DECLARE_SERIALIZER`, `IMPLEMENT_CLASS`, the constructor and vtable-helper macros, `StaticClass<T>` / `StaticStruct<T>` / `StaticEnum<T>` |
| `UObject/UObjectBase.h`, `UObjectBaseUtility.h`, `Object.h` | the object model: `UObjectBase` (flags, index, class, name, outer), `UObjectBaseUtility` (names, paths, outer chain, `IsA`, flags, `AddToRoot`, `MarkPendingKill`), `UObject` (`PostInitProperties`, `ProcessEvent`, `CreateDefaultSubobject`, `BeginDestroy` / `FinishDestroy`, `AddReferencedObjects`, `LoadConfig` / `SaveConfig` / `ReloadConfig`, `CallFunctionByNameWithArguments`, `ProcessConsoleExec`, `Serialize` / `SerializeScriptProperties`, `PostLoad` / `ConditionalPostLoad`, `GetArchetype`, `IsAsset`), `IsValid` |
| `UObject/Class.h` | `UField`, `UStruct`, `UScriptStruct` (`ICppStructOps`), `UClass`, `UEnum` (`_MAX` appended), `UFunction` |
| `UObject/Field.h`, `UObject/UnrealType.h` | `FField` / `FFieldClass` / `FProperty` and every property type: numeric, `FBoolProperty` (native bools and bitfields), `FByteProperty` / `FEnumProperty`, `FStrProperty` / `FNameProperty` / `FTextProperty`, object / class / weak / soft references, `FStructProperty`, `FArrayProperty` / `FSetProperty` / `FMapProperty` with their script helpers; `TFieldIterator`, `TFieldRange` |
| `UObject/UObjectGlobals.h` | `NewObject`, `FObjectInitializer`, `StaticConstructObject_Internal`, `StaticFindObject` / `FindObject`, `MakeUniqueObjectName`, `CreatePackage`, `FindPackage`, `LoadPackage`, `StaticLoadObject` / `LoadObject`, `StaticLoadClass` / `LoadClass`, `GetTransientPackage`, `GetDefault`, `CollectGarbage` / `TryCollectGarbage` / `IncrementalPurgeGarbage`, `FReferenceCollector`, `GARBAGE_COLLECTION_KEEPFLAGS`, the `UE4CodeGen_Private` params and `Construct*` functions |
| `UObject/GarbageCollection.h`, `GCObject.h`, `StrongObjectPtr.h` | `LogGarbage`, `FGarbageCollectionStats`, `FGarbageCollectionSettings` / `FGarbageCollectionTimer`; `FGCObject`; `TStrongObjectPtr` |
| `UObject/UObjectArray.h`, `UObjectHash.h`, `UObjectIterator.h` | `GUObjectArray` (`FUObjectArray` / `FUObjectItem`), the name hash, `GetObjectsWithOuter` / `GetObjectsOfClass`, `TObjectIterator` / `TObjectRange` |
| `UObject/Package.h`, `UObject/SavePackage.h` | `UPackage` (`/Script/<Module>` packages, loaded `.lasset` / `.lmap` packages: flags, GUID, `FileName`, `LinkerLoad`, `IsFullyLoaded`) and its `Save` / `SavePackage` / `SaveToMemory`, `FSavePackageResultStruct` |
| `UObject/PackageFileSummary.h`, `ObjectResource.h`, `Linker.h`, `LinkerLoad.h`, `LinkerSave.h`, `PropertyTag.h` | the package format: `FPackageFileSummary` (`'LEON'`), `FPackageIndex`, `FObjectImport` / `FObjectExport`, `FLinker`, `FLinkerLoad` (with `BeginLoad` / `EndLoad` and the in-memory packages), `FLinkerSave`, `FPropertyTag`, `ResetLoaders` |
| `Serialization/BulkData.h` | `FByteBulkData` |
| `Misc/PackageName.h` | `FPackageName`: long package names, mount points, `.lasset` / `.lmap` files |
| `UObject/Stack.h`, `Script.h`, `ScriptMacros.h` | `FFrame`, `EFunctionFlags`, `DECLARE_FUNCTION` / `DEFINE_FUNCTION`, the `P_GET_*` family |
| `UObject/NoExportTypes.h` | `USTRUCT(noexport)` declarations of the Core structs (`FVector`, `FVector2D`, `FVector4`, `FPlane`, `FRotator`, `FQuat`, `FTransform`, `FColor`, `FLinearColor`, `FGuid`, `FIntPoint`, `FIntVector`, `FBox`) and of `FSoftObjectPath` / `FSoftClassPath` |
| `UObject/WeakObjectPtr*.h`, `PersistentObjectPtr.h`, `SoftObjectPath.h`, `SoftObjectPtr.h` | `FWeakObjectPtr` / `TWeakObjectPtr` (index + serial number; Core's `UObject/WeakObjectPtrTemplatesFwd.h` names it for the delegates), `TPersistentObjectPtr`, `FSoftObjectPath` / `FSoftClassPath` (4.27 layout: `AssetPathName` + `SubPathString`), `FSoftObjectPtr`, `TSoftObjectPtr` / `TSoftClassPtr` (`TryLoad` / `LoadSynchronous` load the package) |
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
rule: a `UObject*` member is a `UPROPERTY`; a non-UObject holder is an `FGCObject` or uses `TStrongObjectPtr`). The
engine calls it where UE does: after the world is destroyed (`UEngine::LoadMap` before the new map,
`UGameEngine::PreExit`, a replaced game instance, a test's `FScopedTestWorld`), after a level (re)load
(`ApplyLevelDocument`), on `obj gc`, and every frame's `UEngine::ConditionalCollectGarbage` through
`FGarbageCollectionTimer::Tick` after the world tick; the interval comes from
`[/Script/Engine.GarbageCollectionSettings] gc.TimeBetweenPurgingPendingKillObjects` in the engine config
(`FGarbageCollectionSettings::LoadFromConfig`, 61.1 s by default). `AActor::Destroy` marks the actor and its components
pending kill and the next collection frees them.

**References.**

- `TWeakObjectPtr` (index + serial number): `Get` / `IsValid` / `IsStale` / `IsExplicitlyNull` / `Reset`, comparison
  and `GetTypeHash`; a pending-kill or unreachable object reads as null (`Get(true)` still returns a pending-kill one).
- `TStrongObjectPtr` keeps its object alive through a private `FGCObject` (copyable, movable).
- `FGCObject`: construct to register, destroy to unregister; `AddReferencedObjects(FReferenceCollector&)` reports.
- `FSoftObjectPath` (`"/Game/Maps/Arena.Arena"` plus a subobject path after `:`), `FSoftClassPath`,
  `TSoftObjectPtr` / `TSoftClassPtr` over `FSoftObjectPtr` (a `TPersistentObjectPtr`: a path and a cached weak
  pointer, re-resolved when objects were created since). `ResolveObject` finds objects in memory; `TryLoad` /
  `LoadSynchronous` load the package when the object is not there (`StaticLoadObject`). Both paths are reflected
  noexport structs whose text form is the path itself (`TStructOpsTypeTraits::WithExportTextItem` /
  `WithImportTextItem`) and which serialize themselves (`WithSerializer`: the path name and the subobject string).

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
calls it; Core's `FExec` / `FSelfRegisteringExec` (`Misc/CoreMisc.h`) let non-UObjects publish commands. Since P13 the
engine's console chain (`ULocalPlayer::Exec` → `UGameViewportClient` → `UEngine::Exec` → `UPlayer::Exec`) reaches the
Exec functions of the player input, the player controller (`FOV`), the pawn, the game mode, the game state and the
world settings through `ProcessConsoleExec`.

## Packages

Objects are saved to and loaded from `.lasset` / `.lmap` packages (plan decision D13; the byte layout is in
[ASSET_FORMATS.md](../../../../Docs/ASSET_FORMATS.md#packages--lasset--lmap)).

**Names.** `FPackageName` maps long package names to files through mount points: `/Engine/` →
`FPaths::EngineContentDir()`, `/Game/` → `FPaths::ProjectContentDir()`, plus `RegisterMountPoint` roots (plugins,
tests). `/Script/<Module>` is a module's compiled-in package: valid (with `bIncludeReadOnlyRoots`) but without a file.
`TryConvertLongPackageNameToFilename`, `TryConvertFilenameToLongPackageName`, `DoesPackageExist` (a `.lasset`, a
`.lmap` or registered bytes), `ObjectPathToPackageName`, `GetShortName`, `SplitLongPackageName`, ...

**Saving.** `UPackage::SavePackage(Package, Base, TopLevelFlags, Filename)` (or `Save`, `SaveToMemory`):

1. **Exports**: `Base`, the package's objects with any of `TopLevelFlags`, and recursively their outers inside the
   package, their inner objects (default subobjects included) and every object of the package they reference
   (`FArchiveSaveTagExports` serializes each one to find them). Transient objects (`RF_Transient`, pending kill, in a
   transient outer, of a `CLASS_Transient` class) are left out.
2. **Imports and names**: `FArchiveSaveTagImports` serializes each export again and records every `FName` and every
   object outside the package (with its outers and its class); an export's class is a `/Script` class import. A
   transient object, or an object of the package that is not exported, is saved as null. Soft object paths add their
   package to the soft package references.
3. **Write** through `FLinkerSave`, into memory: the summary, the sorted name table, the imports and exports sorted by
   path name, the soft package references, each export's data (`UObject::Serialize`), the bulk data payloads, the tag
   again; then the summary and the export table are written once more with the final offsets. `Save` writes the
   bytes with `FFileHelper::SaveArrayToFile` (through `IFileManager`); a `.lmap` file name sets `PKG_ContainsMap`.
   The package GUID is `FGuid::NewDeterministicGuid(PackageName)`: the same objects give the same bytes (D13).

**Serialize.** `UObject::Serialize(Ar)` calls `SerializeScriptProperties`: `UStruct::SerializeTaggedProperties` writes
an `FPropertyTag` and the value of each property that differs from the archetype (`GetArchetype`: the class default
object, or for a default subobject the subobject of the same name in its outer's archetype), then `NAME_None`. A class
with native data overrides `Serialize`, calls `Super::Serialize(Ar)` first and serializes its members after (the
"native tail"), for example an `FByteBulkData`. `FProperty::SerializeItem` does one value of each property type:
structs use their own `Serialize` (`WithSerializer`), binary members for immutable structs (`FVector`, `FTransform`,
...) or nested tagged properties; enums save the enumerator name. Loading reads the tags back: unknown names are
skipped by size, and `FProperty::ConvertFromType` converts a changed type when it can (warning otherwise). Transient
and `CPF_SkipSerialization` properties are never serialized; editor-only ones not in an archive that filters them
(`PKG_FilterEditorOnly`).

**Loading.** `LoadPackage(nullptr, TEXT("/Game/Maps/Arena"), LOAD_None)`, `LoadObject<T>(nullptr, Path)`,
`StaticLoadObject`, `LoadClass<T>`, `FSoftObjectPath::TryLoad`, `TSoftObjectPtr::LoadSynchronous`. Synchronous:

1. `BeginLoad`; `FLinkerLoad::CreateLinker` reads the whole file (or the bytes registered with
   `FLinkerLoad::RegisterInMemoryPackage`), checks the tag, the end tag and the version, reads the tables and
   attaches to the package (`UPackage::LinkerLoad`; the flags and the GUID come from the summary).
2. `LoadAllObjects`: each export is created (`CreateExport`: its class import, its outer, then `StaticConstructObject`
   with `RF_NeedLoad | RF_NeedPostLoad | RF_WasLoaded`, or the object of that name its outer's constructor already
   built, D12) and serialized from its offset (`Preload`: class defaults first, then the saved deltas); a read that
   does not end at `SerialSize` is an error. `UObject*` values resolve to exports or imports: a package import loads
   its package first (recursively; a package already loading is used as it is), another import is found in its
   outer. A missing import is a warning and a null reference, as in UE.
3. `EndLoad` (the outermost one): `ConditionalPostLoad` on every loaded object, package by package in the order their
   loads finished (imports first), in export order, so a `PostLoad` sees every object of the load serialized; the
   packages are marked fully loaded and their linkers deleted (Leon keeps no linker: everything, bulk data included,
   was read eagerly).

A package that is already loaded, or was created in memory and has no file, is returned as it is; a missing package
is an error (`LOAD_NoWarn`: a log line, `LOAD_Quiet`: nothing). `FLinkerLoad::CreateLinker(nullptr, ...)` reads the
tables of a package without loading it (imports, soft package references: the cook's dependency walk).

**Versions.** The summary's `FileVersionUE` is an `ELeonPackageVersion` (Core `UObject/ObjectVersion.h`); the linkers
set `FArchive::UEVer()` from it, so native `Serialize` code checks `Ar.UEVer() >= VER_LEON_<Change>` for data added
later. Older than `VER_LEON_OLDEST_LOADABLE_PACKAGE` or newer than `VER_LEON_LATEST` fails with an error.

**Editor-only data (D14).** A build with `WITH_EDITORONLY_DATA` saves editor-only properties unless the package has
`PKG_FilterEditorOnly`; a build without it (PS2, Shipping) marks every package it saves `PKG_FilterEditorOnly`, and
when it loads a package without that flag (uncooked) it logs it once and skips the editor-only tags as unknown
names (their properties do not exist there). A filtered package also leaves out the editor-only objects (P16, UE's
`IsEditorOnlyObject`): an object whose `UObject::IsEditorOnly()` is true (`UAssetImportData`), or one inside it, is not
exported, and references to it are saved as null. A `PKG_Cooked` package records the cook's target platform, the
`CookedPlatformName` argument of `Save` / `SavePackage` / `SaveToMemory`, in the summary.

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
  layer), no `UpdateDefaultConfigFile` for `DefaultConfig` classes (`Default<Name>.ini` is edited by hand: the
  editor module, LeonEd, has no settings UI), no console variables (`gc.*` keys are read directly). An instance reloaded through
  `ReloadConfig` reads its class's sections parents first, as its class default object did.
- Exec: no `CPP_Default_` metadata, so a missing trailing argument keeps its zero / default value with a warning on
  `Ar` (UE uses the C++ default, or fails when there is none). No `BindUFunction` and no dynamic delegates.
- Packages (P11): one file per package (no `.uexp` / `.ubulk`), a trimmed summary (no custom versions, generations,
  thumbnails, asset registry data or preload dependencies), no struct or property GUIDs in the tags, sets and maps
  saved whole, no `TemplateIndex` in the exports, a GUID derived from the package name, synchronous loading only (no
  async loader, no lazy exports; linkers are released when the load ends), eager bulk data without compression or
  mapping, no redirectors, soft object paths serialized by a free `operator<<`, `SavePackage` without the conform /
  diff / platform parameters, and in-memory packages for the tests. A soft pointer re-resolves after any object is
  created (UE: after a package loads).
- No script VM: `FFrame::Code` is always null and `ProcessEvent` calls native thunks only.
- Default subobjects are rebuilt per instance instead of instanced from the archetype (D12).
- `MakeUniqueObjectName` numbers per class (UE 4.27 per outer); `StaticAllocateObject` does not replace an existing
  object.
- The name hash is a `TMultiMap` keyed by the name's hash; `GetObjectsOfClass` walks `GUObjectArray` (no per-class
  hash).
- `FUObjectItem` has no cluster index; the object array does not grow.
- No metadata, no hot reload (`ClassVTableHelperCtorCaller` is never called).

## Tests

`Private/Tests`: 62 `System.CoreUObject.<Area>.<Name>` automation tests (objects and names, classes and casts, CDOs
and subobjects, registration order, properties, bools, enums, struct ops, containers, functions, NoExport structs,
`WITH_EDITORONLY_DATA`; garbage collection, weak / strong / soft references, `TSubclassOf`, UObject delegates, config
load / defaults / per-object / save, Exec; packages: round trip of every property kind, hard and soft references,
schema evolution, missing imports, deterministic saves with a golden hash, bulk data, editor-only data, package
names, `PostLoad` order, default subobjects, damaged data, files, the round-trip budget) with reflected fixtures
(`ReflectionTestTypes.h`, `HierarchyTestTypes.h`, `OrderTestParent.h`, `OrderTestChild.h`,
`GarbageCollectionTestTypes.h`, `ConfigExecTestTypes.h`, `PackageTestTypes.h`). They run in `LeonAutomationTests` and
in `TestPAL` on every platform, PS2 included (`System.CoreUObject.Config.SaveConfig` and
`System.CoreUObject.Package.Files` are desktop-only: they write under `<Project>/Intermediate/Tests/`; the other
package tests save to memory). TestPAL also logs the reflection budget, the GC cost
(`System.CoreUObject.GarbageCollection.Budget`), a package round trip (`System.CoreUObject.Package.Budget`) and a final
collection (see [Budgets.md](../../../Platforms/PS2/Documentation/Budgets.md)).
