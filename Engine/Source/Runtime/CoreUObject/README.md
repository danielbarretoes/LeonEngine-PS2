# CoreUObject

`UObject` and its reflection, as in UE 4.27's `Runtime/CoreUObject` (plan phase P9). The module depends on Core only,
builds for every platform (PS2 included) in C++17 without RTTI or exceptions, and is the runtime side of
LeonHeaderTool's generated code ([LeonHeaderTool/README.md](../../Programs/LeonHeaderTool/README.md) is the contract).

No engine module is reflected yet: `LeonAutomationTests` and `TestPAL` link CoreUObject for its tests; the gameplay
classes become `UCLASS` types in P12.

## Headers

| Header | Contents |
| --- | --- |
| `UObject/ObjectMacros.h` | the flag enums (`EObjectFlags`, `EInternalObjectFlags`, `EClassFlags`, `EClassCastFlags`, `EPropertyFlags`, `EStructFlags`, `EPackageFlags`, …), `UCLASS` / `USTRUCT` / `UPROPERTY` / `GENERATED_BODY`, `DECLARE_CLASS`, `IMPLEMENT_CLASS`, the constructor and vtable-helper macros, `StaticClass<T>` / `StaticStruct<T>` / `StaticEnum<T>` |
| `UObject/UObjectBase.h`, `UObjectBaseUtility.h`, `Object.h` | the object model: `UObjectBase` (flags, index, class, name, outer), `UObjectBaseUtility` (names, paths, outer chain, `IsA`, flags), `UObject` (`PostInitProperties`, `ProcessEvent`, `CreateDefaultSubobject`, the P10 / P11 stubs) |
| `UObject/Class.h` | `UField`, `UStruct`, `UScriptStruct` (`ICppStructOps`), `UClass`, `UEnum` (`_MAX` appended), `UFunction` |
| `UObject/Field.h`, `UObject/UnrealType.h` | `FField` / `FFieldClass` / `FProperty` and every property type: numeric, `FBoolProperty` (native bools and bitfields), `FByteProperty` / `FEnumProperty`, `FStrProperty` / `FNameProperty` / `FTextProperty`, object / class / weak / soft references, `FStructProperty`, `FArrayProperty` / `FSetProperty` / `FMapProperty` with their script helpers; `TFieldIterator`, `TFieldRange` |
| `UObject/UObjectGlobals.h` | `NewObject`, `FObjectInitializer`, `StaticConstructObject_Internal`, `StaticFindObject` / `FindObject`, `MakeUniqueObjectName`, `CreatePackage`, `GetTransientPackage`, `GetDefault`, the `UE4CodeGen_Private` params and `Construct*` functions |
| `UObject/UObjectArray.h`, `UObjectHash.h`, `UObjectIterator.h` | `GUObjectArray` (`FUObjectArray` / `FUObjectItem`), the name hash, `GetObjectsWithOuter` / `GetObjectsOfClass`, `TObjectIterator` / `TObjectRange` |
| `UObject/Package.h` | `UPackage` (`/Script/<Module>` packages; P11 loads `.lasset` packages) |
| `UObject/Stack.h`, `Script.h`, `ScriptMacros.h` | `FFrame`, `EFunctionFlags`, `DECLARE_FUNCTION` / `DEFINE_FUNCTION`, the `P_GET_*` family |
| `UObject/NoExportTypes.h` | `USTRUCT(noexport)` declarations of the Core structs: `FVector`, `FVector2D`, `FVector4`, `FPlane`, `FRotator`, `FQuat`, `FTransform`, `FColor`, `FLinearColor`, `FGuid`, `FIntPoint`, `FIntVector`, `FBox` |
| `UObject/WeakObjectPtr*.h`, `SoftObjectPath.h`, `SoftObjectPtr.h` | `FWeakObjectPtr` / `TWeakObjectPtr` (index + serial number), minimal `FSoftObjectPath` / `TSoftObjectPtr` / `TSoftClassPtr` (resolve objects already in memory) |
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
  96 KB) and 131072 on desktop (16-byte slots, 2 MB). Running out is a fatal error that logs the capacity.
- Objects are never destroyed yet: garbage collection is P10.

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

- No garbage collection: objects live until the process exits, `AddToRoot` only sets its flag and `BeginDestroy` /
  `FinishDestroy` are stubs (P10).
- No `UPROPERTY(Config)` loading, `Exec` functions or `UObject` delegates (P10); no packages on disk, `Serialize` is a
  stub, soft references only resolve objects already in memory (P11).
- No script VM: `FFrame::Code` is always null and `ProcessEvent` calls native thunks only.
- Default subobjects are rebuilt per instance instead of instanced from the archetype (D12).
- `MakeUniqueObjectName` numbers per class (UE 4.27 per outer); `StaticAllocateObject` does not replace an existing
  object.
- The name hash is a `TMultiMap` keyed by the name's hash; `GetObjectsOfClass` walks `GUObjectArray` (no per-class
  hash).
- `FUObjectItem` has no cluster index; the object array does not grow.
- No metadata, no hot reload (`ClassVTableHelperCtorCaller` is never called).

## Tests

`Private/Tests`: `System.CoreUObject.<Area>.<Name>` automation tests (objects and names, classes and casts, CDOs and
subobjects, registration order, properties, bools, enums, struct ops, containers, functions, NoExport structs,
`WITH_EDITORONLY_DATA`) with reflected fixtures (`ReflectionTestTypes.h`, `HierarchyTestTypes.h`, `OrderTestParent.h`,
`OrderTestChild.h`). They run in `LeonAutomationTests` and in `TestPAL` on every platform, PS2 included; TestPAL also
logs the reflection budget (see [Budgets.md](../../../Platforms/PS2/Documentation/Budgets.md)).
