# LeonHeaderTool

The UnrealHeaderTool counterpart: a C++17 host program that uses only the standard library (no Core). It reads the
UE macros in a module's headers and writes UE 4.27-shaped reflection code. CoreUObject
(`Engine/Source/Runtime/CoreUObject`, since P9) implements the runtime side. This page is the contract between the two.

```
LeonHeaderTool <Module>.lhtmanifest      generate one module (unit); exit code 1 on any error
LeonHeaderTool -Test [<dir>] [-Update]   golden tests: <dir>/Inputs/<Case> against <dir>/Expected/<Case>
```

- Diagnostics are printed as `<file>(<line>): error: <message>` (or `warning:`).
- An output is only rewritten when its content changes. The stamp is always rewritten.

## Build integration (LeonBuildTool)

- **Host tools tree.** `System/HostTools.cmake` builds this folder before any target configure. It uses the host
  compiler (MSVC on Win64, g++ in the ps2dev Docker image) and Ninja, in Release, into
  `Engine/Intermediate/Build/HostTools/<Win64|Linux|LinuxMusl>/`. The target configure receives
  `-DLEON_HEADER_TOOL=<exe>`.
- **Detection.** `Configuration/ReflectionRules.cmake` treats a module as reflected when a header under its `Public/`,
  `Classes/` or `Private/` folders (or the whole folder of a flat module) contains `#include "<Name>.generated.h"`.
  Targets with `COLLECT_AUTOMATION_TESTS` also scan `Private/Tests/**.h`; those headers form the `<Module>.Tests` unit.
- **Manifest.** Each reflected unit gets `<tree>/Inc/<Module>/<Unit>.lhtmanifest`. It is a line-based `Key=Value`
  file (keys are documented in `Private/Manifest.h`) listing the headers, the module, its API macro, the output folder
  and the type indexes of reflected dependencies.
- **Custom command.** The output is `<Unit>.lhtstamp`; the generated files are byproducts, so an unchanged run
  recompiles nothing. It depends on the tool, the manifest, every header of the unit and the dependencies'
  `<Module>.lhttypes`.
- **Compilation.** The `.gen.cpp` files and `<Unit>.init.gen.cpp` compile into the module library. For a launch module
  or the Tests unit they compile into the executable. `<tree>/Inc/<Module>` is a PUBLIC include path.
- **Module table.** `FStaticallyLinkedModuleInfo::RegisterReflection` (in `Modules/ModuleManager.h`) points at
  `RegisterReflection_<Module>`, or at a wrapper that also calls `RegisterReflection_<Module>_Tests` in test targets.
  It is `nullptr` for modules without reflected types. A reflected module must have a table entry (Runtime or
  Developer, with `IMPLEMENT_MODULE`), otherwise configure fails. `FModuleManager` calls it right after creating the
  module, then `OnProcessLoadedObjectsCallback` (bound by CoreUObject), then `StartupModule`.
- **New headers.** Header lists are `CONFIGURE_DEPENDS` globs, so a new header reconfigures the tree.
- **First include in an existing header.** If a header of a reflected unit gains its first `.generated.h` include,
  LeonHeaderTool reports it and touches `<Unit>.lhtreconfigure`, and the next build reconfigures. In a module with no
  reflected header yet, touch its `.Build.cmake` after adding the first include.
- **Circular dependencies.** `CIRCULAR_DEPENDENCIES` are not followed for type indexes.

## Supported subset

- **Macros:**
  - `UCLASS`, `USTRUCT` and `UENUM` (with `UMETA` on enumerators), at file scope.
  - `UPROPERTY` and `UFUNCTION` (UFUNCTION in classes only).
  - `GENERATED_BODY`, `GENERATED_UCLASS_BODY` (legacy constructors) and `GENERATED_USTRUCT_BODY`.
- **Preprocessor:**
  - `#if WITH_EDITORONLY_DATA` around properties marks them `CPF_EditorOnly`, and their generated tables are wrapped
    in the same `#if`.
  - `#if CPP` / `#if 0` branches are skipped; `#if !CPP` / `#if 1` branches are parsed.
  - A property or function inside any other `#if` block is an error.
- **Class specifiers with an effect:** `Abstract`, `Config=<Name>`, `DefaultConfig`, `PerObjectConfig`, `Transient`,
  `NotPlaceable`, `MinimalAPI` and `EditInlineNew`. `Interface`, `Within`, `GlobalUserConfig`, `ProjectUserConfig`,
  `NoExport` and similar are errors.
- **Struct specifiers with an effect:** `Atomic`, `Immutable` and `NoExport`.
  - `USTRUCT(NoExport)` declares the reflection of a C++ type defined elsewhere (UE: CoreUObject's
    `NoExportTypes.h`). It must sit inside an `#if !CPP` block, so the compiler never sees it, and must not have a
    `GENERATED_BODY`. Bitfields, C arrays and `WITH_EDITORONLY_DATA` members are errors in it, since the generated
    layout check cannot express them.
- **Property specifiers:** `Config` (`CPF_Config`), `GlobalConfig` (`CPF_GlobalConfig | CPF_Config`), `Transient`,
  `DuplicateTransient`, `SaveGame`, the `Edit*` /
  `Visible*` / `BlueprintRead*` family, `Instanced` and others each map to their `CPF_` flags. `Replicated*` is an
  error.
- **Function specifiers:** `Exec` (`FUNC_Exec`, for `UObject::CallFunctionByNameWithArguments`), `BlueprintCallable`,
  `BlueprintPure`, `BlueprintAuthorityOnly` and `BlueprintCosmetic`. Events and RPC specifiers are errors.
- **Ignored silently:** `meta=(...)`, `Category`, `BlueprintType` and the like. Unknown specifiers produce a warning.
- **Property types:**
  - Scalars: `bool` (including `uint8 bX : 1` bitfields), `int8/16/32/64`, `uint8/16/32/64`, `int`,
    `unsigned int`, `float` and `double`.
  - Strings: `FString`, `FName` and `FText`.
  - Enums: an `enum class` with an underlying type, or `TEnumAsByte<E>`.
  - Object references: `UObject`-derived `T*` (`class T*` and `const T*` are accepted), `TSubclassOf`,
    `TSoftObjectPtr`, `TSoftClassPtr` and `TWeakObjectPtr`.
  - Containers: `TArray`, `TMap` and `TSet` of the types above. Nested containers are errors.
  - `USTRUCT`s by value, and C arrays `T X[N]` (not of containers or bool).
- **UFUNCTION parameters:** the same types, by value or `const&`. Structs and containers passed as `const&` are
  by-reference parameters (`CPF_OutParm|CPF_ReferenceParm|CPF_ConstParm`). Non-const references, C arrays and
  overloads are errors.
- **Type lookup:** the module's own types in any of its headers, the `.lhttypes` of reflected dependencies, and the
  intrinsic CoreUObject classes `UObject`, `UField`, `UStruct`, `UClass`, `UScriptStruct`, `UFunction`, `UEnum`,
  `UPackage` and `UInterface`.
- **Header rules:**
  - `#include "<Header>.generated.h"` must be the header's last include and come before the first U-type.
  - The first base class is the reflected super.
  - Private and protected members are fine: `STRUCT_OFFSET` is used inside the friend `*_Statics` struct.
  - `BlueprintReadWrite` / `BlueprintReadOnly` on a private member without `meta=(AllowPrivateAccess)` is an error,
    as in UHT.

## Generated files (in `<tree>/Inc/<Module>/`)

- `FileId` is the header path relative to the repository (for a project outside it, relative to the project's
  parent folder), with every non-alphanumeric character replaced by `_`. Example:
  `Engine_Source_Runtime_Engine_Classes_GameFramework_Actor_h`.
- `L` is the line of `GENERATED_BODY`.
- `API` is the class's `<MODULE>_API` if it was declared with one, otherwise `NO_API`.
- `MODULE_API` is the module's `<MODULE>_API` (LeonBuildTool defines it as empty).

### `<Header>.generated.h`

The file includes `UObject/ObjectMacros.h` and `UObject/ScriptMacros.h`. It is wrapped in
`PRAGMA_DISABLE/ENABLE_DEPRECATION_WARNINGS`, and a guard `<MODULE>_<Header>_generated_h` raises `#error` on a double
include. It ends with `#undef CURRENT_FILE_ID` / `#define CURRENT_FILE_ID <FileId>`.

**Class.**

- `<FileId>_<L>_RPC_WRAPPERS` and `_RPC_WRAPPERS_NO_PURE_DECLS`: one `DECLARE_FUNCTION(exec<Func>);` per UFUNCTION.
- `_INCLASS` and `_INCLASS_NO_PURE_DECLS`:
  - `private: static void StaticRegisterNatives<C>(); friend struct Z_Construct_UClass_<C>_Statics;`
  - `public: DECLARE_CLASS(C, Super, COMPILED_IN_FLAGS(0 | CLASS_...), CASTCLASS_None, TEXT("/Script/<Module>"), API)`
  - `DECLARE_SERIALIZER(C)`
  - `static const TCHAR* StaticConfigName()` with `Config=` (a class without it inherits its super's config name).
  - The class's static `AddReferencedObjects` needs no generated code: `IMPLEMENT_CLASS` passes
    `&C::AddReferencedObjects` (UObject's unless the class declares its own) to the class.
- `_STANDARD_CONSTRUCTORS` (legacy):
  - `API C(const FObjectInitializer& = FObjectInitializer::Get());`
  - `DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(C)`
  - the vtable helper
  - private declared-only `C(C&&)` and `C(const C&)`.
- `_ENHANCED_CONSTRUCTORS`:
  - If the class declares no default constructor and no `FObjectInitializer` constructor, an inline
    `API C(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { }`.
    The parent therefore needs an `FObjectInitializer` constructor.
  - The private move and copy constructors.
  - `DECLARE_VTABLE_PTR_HELPER_CTOR(API, C);`, unless the class declares `C(FVTableHelper&)`.
  - `DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(C);`
  - `DEFINE_DEFAULT_CONSTRUCTOR_CALL(C)` when only `C()` is declared, otherwise
    `DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(C)`.
- `<FileId>_<Line>_PROLOG`: an empty macro for every line that `UCLASS(...)` spans.
- `_GENERATED_BODY_LEGACY`: `public:` RPC_WRAPPERS, INCLASS, STANDARD_CONSTRUCTORS, then `public:`.
- `_GENERATED_BODY`: `public:` RPC_WRAPPERS_NO_PURE_DECLS, INCLASS_NO_PURE_DECLS, ENHANCED_CONSTRUCTORS, then
  `private:`.
- `template<> MODULE_API UClass* StaticClass<class C>();`

**Struct.**

- `<FileId>_<L>_GENERATED_BODY`: `friend struct Z_Construct_UScriptStruct_<S>_Statics;`,
  `static class UScriptStruct* StaticStruct();` and, when the struct has a reflected base, `typedef <Base> Super;`.
  A NoExport struct gets no `GENERATED_BODY` macro.
- `template<> MODULE_API UScriptStruct* StaticStruct<struct S>();`

**Enum.**

- `FOREACH_ENUM_<UPPERNAME>(op)` with `op(E::A)` (enum class) or `op(A)` (regular enum).
- For an `enum class` with an underlying type only: `enum class E : T;` and
  `template<> MODULE_API UEnum* StaticEnum<E>();`. Other enums cannot be forward-declared, so `StaticEnum<E>` is only
  defined, in the `.gen.cpp`.

### `<Header>.gen.cpp`

- **Includes:** `UObject/GeneratedCppIncludes.h`, then the header by its module-relative path.
- **Cross module references:** a declaration `<OWNER>_API <Type>* Z_Construct_*();` for each function it calls,
  including those of other modules and CoreUObject's (`Z_Construct_UClass_UObject`, `_UObject_NoRegister`,
  `Z_Construct_UClass_UClass`), then `UPackage* Z_Construct_UPackage__Script_<Module>();`.
- **Per enum:**
  - `static UEnum* E_StaticEnum()` using `GetStaticEnum(Z_Construct_UEnum_<Module>_E, <Package>(), TEXT("E"))`.
  - `StaticEnum<E>()`.
  - `Z_Construct_UEnum_<Module>_E_Statics { Enumerators[]; EnumParams; }`, with enumerator names `"E::A"`
    (`"A"` for a regular enum) and values `(int64)E::A`.
  - `Z_Construct_UEnum_<Module>_E()` calling `ConstructUEnum`.
- **Per struct:**
  - `S::StaticStruct()` using `GetStaticStruct(Z_Construct_UScriptStruct_S, <Package>(), TEXT("<S without F>"), sizeof(S), 0)`.
  - `StaticStruct<S>()`.
  - `Z_Construct_UScriptStruct_S_Statics { NewStructOps(); NewProp_*; PropPointers[]; ReturnStructParams; }`.
    `NewStructOps` returns `new UScriptStruct::TCppStructOps<S>()`.
  - `Z_Construct_UScriptStruct_S()` calling `ConstructUScriptStruct`.
  - **NoExport:** `S::StaticStruct()` becomes a file-local `static UScriptStruct* S_StaticStruct()`. The offsets are
    still `STRUCT_OFFSET(S, Member)` on the real C++ type, and the `_Statics` struct also holds a check of the
    declaration against it: a `struct FNoExportLayout [: <Base>] { <declared members> };` with `static_assert`s on
    `sizeof(FNoExportLayout) == sizeof(S)`, on each member's type (`TIsSame<decltype(S::M), T>`) and on each member's
    offset. A declaration that drifts from the C++ type is a compile error, never wrong data. Non-public members need
    the C++ type to befriend `Z_Construct_UScriptStruct_S_Statics` (Core's `FTransform` does).
- **Per class:**
  - `DEFINE_FUNCTION(C::exec<F>) { P_GET_*; P_FINISH; P_NATIVE_BEGIN; [*(R*)Z_Param__Result=]P_THIS->F(args); P_NATIVE_END; }`
    (`C::F(args)` for static functions).
  - `C::StaticRegisterNatives<C>()`, which calls `FNativeFunctionRegistrar::RegisterFunctions(Class, FNameNativePtrPair[], N)`.
  - Per function: `Z_Construct_UFunction_<C>_<F>_Statics`. It holds a nested `<C without prefix>_event<F>_Parms`
    struct (parameters, then `ReturnValue`), their `NewProp_*`, `PropPointers[]` and `FuncParams`.
    `Z_Construct_UFunction_<C>_<F>()` calls `ConstructUFunction`.
  - `Z_Construct_UClass_<C>_NoRegister()`, which returns `C::StaticClass()`.
  - `Z_Construct_UClass_<C>_Statics { DependentSingletons[] (super, package); FuncInfo[]; NewProp_*; PropPointers[];
    StaticCppClassTypeInfo; ClassParams; }`.
  - `Z_Construct_UClass_<C>()` calling `ConstructUClass`.
  - `IMPLEMENT_CLASS(C, 0);`, `StaticClass<C>()` and `DEFINE_VTABLE_PTR_HELPER_CTOR(C);`.
- **Property params:**
  - `NewProp_<Name>` initializers are `{ "Name", nullptr, (EPropertyFlags)0x..., EPropertyGenFlags::<Kind>,
    RF_Public|RF_Transient|RF_MarkAsNative, ArrayDim, Offset, <extra> }`.
  - `ArrayDim` is `1` or `CPP_ARRAY_DIM(Name, Outer)`; `Offset` is `STRUCT_OFFSET(Outer, Name)`.
  - Children come first in `PropPointers`:
    - enum: `_Underlying` (name `"UnderlyingType"`);
    - `TArray`: `_Inner`;
    - `TSet`: `_ElementProp`;
    - `TMap`: `_ValueProp` (offset field `1`), then `_Key_KeyProp` (name `"<Name>_Key"`, offset `0`).
  - A bool gets `static void NewProp_<Name>_SetBit(void* Obj) { ((Outer*)Obj)-><Name> = 1; }`. Its params are
    `ArrayDim, sizeof(bool | bitfield type), sizeof(Outer), &SetBit` (`0, nullptr` for container elements); its gen
    flags are `Bool | NativeBool` (`Bool` only for bitfields).
  - Property flags exclude the computed ones (`CPF_ZeroConstructor`, `CPF_IsPlainOldData`, `CPF_NoDestructor`,
    `CPF_HasGetValueTypeHash`); FProperty sets those at runtime, as in UE.
  - Class flags = specifier flags `| CLASS_Native | CLASS_MatchedSerializers`, plus `| CLASS_RequiredAPI` with an API
    macro.

### `<Module>.init.gen.cpp` (`<Module>.Tests.init.gen.cpp`)

- Includes every reflected header of the unit.
- Defines `Z_Construct_UPackage__Script_<Module>()`: `FPackageParams { "/Script/<Module>", nullptr, 0,
  PKG_CompiledIn | 0, 0, 0 }` and `ConstructUPackage`. The Tests unit only declares it when the module defines it.
- Defines the registration function, which `FModuleManager` calls through the module table before `StartupModule`:

```cpp
void RegisterReflection_<Module>()   // or RegisterReflection_<Module>_Tests
{
	static const FClassRegisterCompiledInInfo ClassInfo[] = { { Z_Construct_UClass_C, C::StaticClass, TEXT("C"), sizeof(C) }, ... };
	static const FStructRegisterCompiledInInfo StructInfo[] = { { Z_Construct_UScriptStruct_S, TEXT("S-without-F"), sizeof(S) }, ... };
	static const FEnumRegisterCompiledInInfo EnumInfo[] = { { Z_Construct_UEnum_<Module>_E, TEXT("E") }, ... };
	RegisterCompiledInInfo(TEXT("/Script/<Module>"), ClassInfo, UE_ARRAY_COUNT(ClassInfo), StructInfo, ..., EnumInfo, ...);
}
```

An empty table is passed as `nullptr, 0`. Order is declaration order: headers in manifest (path) order, then file
order. A class can come before its super. Construction resolves that through `DependentSingletons` (each `Z_Construct_*`
is idempotent): `RegisterCompiledInInfo` only records, and `ProcessNewlyLoadedUObjects` constructs afterwards.

## What CoreUObject provides

P8 wrote this list as the requirements for P9; CoreUObject implements all of it (`Public/UObject/ObjectMacros.h`,
`ScriptMacros.h`, `GeneratedCppIncludes.h`, `UObjectGlobals.h`, `UObjectBase.h`, `Class.h`; see
[CoreUObject/README.md](../../Runtime/CoreUObject/README.md)). The CoreUObject `NoExportTypes.h` declares the
NoExport Core structs (`FVector`, `FRotator`, `FTransform`, …), so a `UPROPERTY() FVector X;` in any module resolves to
`Z_Construct_UScriptStruct_FVector`.

- **`UObject/ObjectMacros.h`:**
  - `UCLASS(...)` → `BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_PROLOG)`.
  - `USTRUCT`, `UENUM`, `UPROPERTY`, `UFUNCTION` and `UMETA` → nothing.
  - `GENERATED_BODY()` → `BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_GENERATED_BODY)`,
    `GENERATED_BODY_LEGACY()` → `..._GENERATED_BODY_LEGACY`, `GENERATED_UCLASS_BODY()` → `GENERATED_BODY_LEGACY()`,
    `GENERATED_USTRUCT_BODY()` → `GENERATED_BODY()`.
  - `BODY_MACRO_COMBINE` (with an inner macro so its arguments expand).
  - `DECLARE_CLASS(TClass, TSuperClass, TStaticFlags, TStaticCastFlags, TPackage, TRequiredAPI)`, which also
    provides the `Super` and `ThisClass` typedefs and `static UClass* StaticClass()`. Also `COMPILED_IN_FLAGS`,
    `EClassFlags` (`CLASS_*`), `CASTCLASS_None`, `DECLARE_SERIALIZER` and `NO_API`.
  - `DECLARE_VTABLE_PTR_HELPER_CTOR(API, T)` = `API T(FVTableHelper& Helper)`, `DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(T)`
    and `DEFINE_VTABLE_PTR_HELPER_CTOR(T)` = `T::T(FVTableHelper& Helper) : Super(Helper) {}`.
  - `DEFINE_DEFAULT_CONSTRUCTOR_CALL(T)` and `DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(T)`.
  - `FObjectInitializer::Get()` and `FVTableHelper`.
  - `PRAGMA_DISABLE/ENABLE_DEPRECATION_WARNINGS`, used inside macros, so `_Pragma` / `__pragma` or empty.
  - The primary templates `StaticClass<T>()`, `StaticStruct<T>()` and `StaticEnum<T>()`.
- **`UObject/ScriptMacros.h`:**
  - `DECLARE_FUNCTION` / `DEFINE_FUNCTION`: `(UObject* Context, FFrame& Stack, RESULT_DECL)` with `Z_Param__Result`.
  - `P_THIS`, `P_FINISH`, `P_NATIVE_BEGIN` and `P_NATIVE_END`.
  - `P_GET_PROPERTY(<F…Property>, V)` for `FInt8`, `FInt16`, `FInt`, `FInt64`, `FByte`, `FUInt16`, `FUInt32`,
    `FUInt64`, `FUnsizedInt`, `FUnsizedUInt`, `FFloat`, `FDouble`, `FStr`, `FName` and `FText`.
  - `P_GET_UBOOL`, `P_GET_ENUM`, `P_GET_OBJECT`, `P_GET_SOFTOBJECT`, `P_GET_SOFTCLASS`, `P_GET_WEAKOBJECT`,
    `P_GET_STRUCT(_REF)`, `P_GET_TARRAY(_REF)`, `P_GET_TSET(_REF)` and `P_GET_TMAP(_REF)`.
  - `P_FINISH` should also consume `Context` and `Z_Param__Result` (for example `(void)Context;`). Static and void
    thunks do not use them, and Leon builds use `/W4` and `-Wextra` without UE's `/wd4100`.
- **`UObject/GeneratedCppIncludes.h`:**
  - `namespace UE4CodeGen_Private`:
    - `EPropertyGenFlags : uint8`, UE 4.27 values, with `operator|`;
    - the params structs below;
    - `ConstructUClass`, `ConstructUScriptStruct`, `ConstructUEnum`, `ConstructUFunction` and `ConstructUPackage`,
      each `(T*& Out, const F<T>Params&)`.
  - `FClassFunctionLinkInfo { UFunction* (*CreateFuncPtr)(); const char* FuncNameUTF8; }` and
    `FCppClassTypeInfoStatic { bool bIsAbstract; }`.
  - `TCppClassTypeTraits<T>::IsAbstract`, `FNameNativePtrPair { const char*; FNativeFuncPtr; }` and
    `FNativeFunctionRegistrar::RegisterFunctions`.
  - `GetStaticStruct(UScriptStruct*(*)(), UObject*, const TCHAR*, SIZE_T, uint32)` and
    `GetStaticEnum(UEnum*(*)(), UObject*, const TCHAR*)`.
  - `UScriptStruct::ICppStructOps` / `TCppStructOps<T>` and `UEnum::ECppForm { Regular, Namespaced, EnumClass }`.
  - `EObjectFlags` (`RF_Public`, `RF_Transient`, `RF_MarkAsNative`, with `operator|`), `EPropertyFlags : uint64`,
    `EFunctionFlags`, `EStructFlags`, `EEnumFlags::{None, Flags}`, `EArrayPropertyFlags::None`,
    `EMapPropertyFlags::None` and `PKG_CompiledIn`.
  - `STRUCT_OFFSET` (`offsetof`), `CPP_ARRAY_DIM`, `IMPLEMENT_CLASS(T, Crc)` (with no static registrar object) and
    the registration API below.
  - GCC warns about `offsetof` on UObject classes (`-Winvalid-offsetof`, on by default). Disable it the way UE does:
    `#pragma GCC diagnostic ignored "-Winvalid-offsetof"` in this header, or `-Wno-invalid-offsetof`.
- **Intrinsic classes:** `Z_Construct_UClass_UObject`, `Z_Construct_UClass_UObject_NoRegister` and
  `Z_Construct_UClass_UClass` (and the same for any other intrinsic class that is referenced), written by hand or
  generated once CoreUObject reflects `UObject`.
- **Registration API:**
  ```cpp
  struct FClassRegisterCompiledInInfo  { UClass* (*OuterRegister)(); UClass* (*InnerRegister)(); const TCHAR* Name; SIZE_T Size; };
  struct FStructRegisterCompiledInInfo { UScriptStruct* (*OuterRegister)(); const TCHAR* Name; SIZE_T Size; };
  struct FEnumRegisterCompiledInInfo   { UEnum* (*OuterRegister)(); const TCHAR* Name; };
  void RegisterCompiledInInfo(const TCHAR* PackageName, const FClassRegisterCompiledInInfo* ClassInfo, SIZE_T NumClassInfo,
      const FStructRegisterCompiledInInfo* StructInfo, SIZE_T NumStructInfo, const FEnumRegisterCompiledInInfo* EnumInfo, SIZE_T NumEnumInfo);
  ```
- **Module startup:** before `StartupModule`, call each `FStaticallyLinkedModuleInfo::RegisterReflection` that is not
  null. `FModuleManager::StartupStaticallyLinkedModules` does it, then calls `OnProcessLoadedObjectsCallback`, which
  CoreUObject binds to `ProcessNewlyLoadedUObjects`.

### `UE4CodeGen_Private` params (field order = initializer order; no metadata fields)

| Struct | Fields |
| --- | --- |
| `FPropertyParamsBase` | `NameUTF8, RepNotifyFuncUTF8, PropertyFlags (EPropertyFlags), Flags (EPropertyGenFlags), ObjectFlags (EObjectFlags), ArrayDim (int32)` |
| `FGenericPropertyParams` = Int8/Int16/Int/Int64/UInt16/UInt32/UInt64/UnsizedInt/UnsizedUInt/Float/Double/Name/Str/Text/Set | base + `Offset (int32)` |
| `FBytePropertyParams`, `FEnumPropertyParams` | base + `Offset, UEnum* (*EnumFunc)()` |
| `FBoolPropertyParams` | base + `uint32 ElementSize, SIZE_T SizeOfOuter, void (*SetBitFunc)(void*)` |
| `FObjectPropertyParams` = Weak/SoftObject | base + `Offset, UClass* (*ClassFunc)()` |
| `FClassPropertyParams` | base + `Offset, UClass* (*MetaClassFunc)(), UClass* (*ClassFunc)()` |
| `FSoftClassPropertyParams` | base + `Offset, UClass* (*MetaClassFunc)()` |
| `FStructPropertyParams` | base + `Offset, UScriptStruct* (*ScriptStructFunc)()` |
| `FArrayPropertyParams` / `FMapPropertyParams` | base + `Offset, EArrayPropertyFlags ArrayFlags` / `EMapPropertyFlags MapFlags` |
| `FEnumeratorParam` | `NameUTF8, int64 Value` |
| `FEnumParams` | `UObject* (*OuterFunc)(), FText (*DisplayNameFunc)(int32), NameUTF8, CppTypeUTF8, const FEnumeratorParam* EnumeratorParams, int32 NumEnumerators, EObjectFlags ObjectFlags, EEnumFlags EnumFlags, uint8 CppForm` |
| `FStructParams` | `UObject* (*OuterFunc)(), UScriptStruct* (*SuperFunc)(), void* (*StructOpsFunc)(), NameUTF8, SIZE_T SizeOf, SIZE_T AlignOf, const FPropertyParamsBase* const* PropertyArray, int32 NumProperties, EObjectFlags ObjectFlags, uint32 StructFlags` |
| `FFunctionParams` | `UObject* (*OuterFunc)(), UFunction* (*SuperFunc)(), NameUTF8, OwningClassName, DelegateName, SIZE_T StructureSize, const FPropertyParamsBase* const* PropertyArray, int32 NumProperties, EObjectFlags ObjectFlags, EFunctionFlags FunctionFlags, uint16 RPCId, uint16 RPCResponseId` |
| `FPackageParams` | `NameUTF8, UObject* (*const* SingletonFuncArray)(), int32 NumSingletons, uint32 PackageFlags, uint32 BodyCRC, uint32 DeclarationsCRC` |
| `FClassParams` | `UClass* (*ClassNoRegisterFunc)(), ClassConfigNameUTF8 (nullptr = inherit), const FCppClassTypeInfoStatic* CppClassInfo, UObject* (*const* DependencySingletonFuncArray)(), const FClassFunctionLinkInfo* FunctionLinkArray, const FPropertyParamsBase* const* PropertyArray, const FImplementedInterfaceParams* ImplementedInterfaceArray (always nullptr), int32 NumDependencySingletons, int32 NumFunctions, int32 NumProperties, int32 NumImplementedInterfaces, uint32 ClassFlags` |

## Deviations from UE 4.27

- **No metadata.** There are no `WITH_METADATA` tables or `METADATA_PARAMS` fields; `meta`, `Category`, `UMETA` and
  tooltips are dropped.
- **No hot-reload CRCs.** `IMPLEMENT_CLASS(C, 0)`, `GetStaticStruct(..., 0)`, and the package `BodyCRC` /
  `DeclarationsCRC` are 0. There are no `Get_Z_Construct_*_Hash()` functions.
- **No static registration objects** (`FCompiledInDefer`, `FCompiledInDeferStruct`, `TClassCompiledInDefer`).
  Registration is explicit: `<Module>.init.gen.cpp` defines `RegisterReflection_<Module>`, and the generated module
  table references it (the shape of UE5's `RegisterCompiledInInfo`). As a result, static linking cannot drop it.
- **Enum `_Statics`.** Enums use a `_Statics` struct like classes (UE5 shape; 4.27 used function-local statics).
- **No `SPARSE_DATA` or `PRIVATE_PROPERTY_OFFSET`** macros, no `EmptyLinkFunctionForGeneratedCode*`, no MSVC
  `#pragma warning` block.
- **Declaration order.** Functions keep declaration order; UHT sorts them by name.
- **Not supported:** interfaces, delegates (`DECLARE_DYNAMIC_*`), `BlueprintImplementableEvent` /
  `BlueprintNativeEvent`, RPCs, `NoExport` classes, `Within`, namespaced enums, out parameters, and `WITH_EDITOR`
  functions.
- **NoExport structs.** UHT trusts a NoExport declaration to match the C++ type. LeonHeaderTool takes the offsets from
  the C++ type and emits `static_assert`s on the size, member types and member offsets, so a mismatch fails the build.
- **Per-module runs.** Each unit runs separately and sees other modules through their `.lhttypes` index, whereas UHT
  runs once for all modules.

## Golden tests

- 35 cases: 14 feature cases and 21 error cases (`Error*`). `ConfigAndExec` covers `Config=`, `DefaultConfig`,
  `PerObjectConfig`, an inherited config class, `Config` / `GlobalConfig` members and `Exec` functions.
- `Tests/Inputs/<Case>/*.h` is one module (`LhtTest`, or the case's `Test.lhtmanifest`).
- `Tests/Expected/<Case>/` holds every output plus `Diagnostics.txt` when there are messages. Error cases expect only
  `Diagnostics.txt`, with the exact `file(line): error:` text.
- `RunTests.bat` runs `LeonHeaderTool -Test` after the automation tests.
- After an intended output change, run `LeonHeaderTool -Test -Update` and review the diff.
- `Tests/.clang-format` disables formatting for this data.
