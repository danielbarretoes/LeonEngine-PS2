#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class FDiagnostics;

/** EPropertyFlags values used by the generated code (UE 4.27 ObjectMacros.h). */
namespace EPropertyFlagBits
{
	constexpr uint64_t Edit = 0x0000000000000001ull;
	constexpr uint64_t ConstParm = 0x0000000000000002ull;
	constexpr uint64_t BlueprintVisible = 0x0000000000000004ull;
	constexpr uint64_t ExportObject = 0x0000000000000008ull;
	constexpr uint64_t BlueprintReadOnly = 0x0000000000000010ull;
	constexpr uint64_t EditFixedSize = 0x0000000000000040ull;
	constexpr uint64_t Parm = 0x0000000000000080ull;
	constexpr uint64_t OutParm = 0x0000000000000100ull;
	constexpr uint64_t ReturnParm = 0x0000000000000400ull;
	constexpr uint64_t DisableEditOnTemplate = 0x0000000000000800ull;
	constexpr uint64_t Transient = 0x0000000000002000ull;
	constexpr uint64_t Config = 0x0000000000004000ull;
	constexpr uint64_t DisableEditOnInstance = 0x0000000000010000ull;
	constexpr uint64_t EditConst = 0x0000000000020000ull;
	constexpr uint64_t GlobalConfig = 0x0000000000040000ull;
	constexpr uint64_t InstancedReference = 0x0000000000080000ull;
	constexpr uint64_t DuplicateTransient = 0x0000000000200000ull;
	constexpr uint64_t SaveGame = 0x0000000001000000ull;
	constexpr uint64_t NoClear = 0x0000000002000000ull;
	constexpr uint64_t ReferenceParm = 0x0000000008000000ull;
	constexpr uint64_t BlueprintAssignable = 0x0000000010000000ull;
	constexpr uint64_t Interp = 0x0000000200000000ull;
	constexpr uint64_t NonTransactional = 0x0000000400000000ull;
	constexpr uint64_t EditorOnly = 0x0000000800000000ull;
	constexpr uint64_t AssetRegistrySearchable = 0x0000010000000000ull;
	constexpr uint64_t SimpleDisplay = 0x0000020000000000ull;
	constexpr uint64_t AdvancedDisplay = 0x0000040000000000ull;
	constexpr uint64_t Protected = 0x0000080000000000ull;
	constexpr uint64_t TextExportTransient = 0x0000400000000000ull;
	constexpr uint64_t NonPIEDuplicateTransient = 0x0000800000000000ull;
	constexpr uint64_t PersistentInstance = 0x0002000000000000ull;
	constexpr uint64_t UObjectWrapper = 0x0004000000000000ull;
	constexpr uint64_t NativeAccessSpecifierPublic = 0x0010000000000000ull;
	constexpr uint64_t NativeAccessSpecifierProtected = 0x0020000000000000ull;
	constexpr uint64_t NativeAccessSpecifierPrivate = 0x0040000000000000ull;
	constexpr uint64_t SkipSerialization = 0x0080000000000000ull;
} // namespace EPropertyFlagBits

/** EClassFlags values (UE 4.27). */
namespace EClassFlagBits
{
	constexpr uint32_t Abstract = 0x00000001u;
	constexpr uint32_t DefaultConfig = 0x00000002u;
	constexpr uint32_t Config = 0x00000004u;
	constexpr uint32_t Transient = 0x00000008u;
	constexpr uint32_t MatchedSerializers = 0x00000020u;
	constexpr uint32_t Native = 0x00000080u;
	constexpr uint32_t NotPlaceable = 0x00000200u;
	constexpr uint32_t EditInlineNew = 0x00001000u;
	constexpr uint32_t MinimalAPI = 0x00080000u;
	constexpr uint32_t RequiredAPI = 0x00100000u;
} // namespace EClassFlagBits

/** EFunctionFlags values (UE 4.27). */
namespace EFunctionFlagBits
{
	constexpr uint32_t Final = 0x00000001u;
	constexpr uint32_t BlueprintAuthorityOnly = 0x00000004u;
	constexpr uint32_t BlueprintCosmetic = 0x00000008u;
	constexpr uint32_t Exec = 0x00000200u;
	constexpr uint32_t Native = 0x00000400u;
	constexpr uint32_t Static = 0x00002000u;
	constexpr uint32_t Public = 0x00020000u;
	constexpr uint32_t Private = 0x00040000u;
	constexpr uint32_t Protected = 0x00080000u;
	constexpr uint32_t HasOutParms = 0x00400000u;
	constexpr uint32_t BlueprintCallable = 0x04000000u;
	constexpr uint32_t BlueprintPure = 0x10000000u;
	constexpr uint32_t Const = 0x40000000u;
} // namespace EFunctionFlagBits

/** EStructFlags values (UE 4.27). */
namespace EStructFlagBits
{
	constexpr uint32_t Native = 0x00000001u;
	constexpr uint32_t NoExport = 0x00000008u;
	constexpr uint32_t Atomic = 0x00000010u;
	constexpr uint32_t Immutable = 0x00000020u;
	constexpr uint32_t RequiredAPI = 0x00000200u;
} // namespace EStructFlagBits

enum class EAccessSpecifier
{
	Public,
	Protected,
	Private
};

/** The FProperty class a declaration maps to (UE: EPropertyGenFlags). */
enum class EPropertyKind
{
	/** A user type name still to be looked up (class, struct or enum). */
	Unresolved,
	Bool,
	Int8,
	Int16,
	Int,
	Int64,
	Byte,
	UInt16,
	UInt32,
	UInt64,
	UnsizedInt,
	UnsizedUInt,
	Float,
	Double,
	Str,
	Name,
	Text,
	Enum,
	Object,
	Class,
	SoftObject,
	SoftClass,
	WeakObject,
	Struct,
	Array,
	Map,
	Set
};

struct FPropertyType
{
	EPropertyKind Kind = EPropertyKind::Unresolved;
	/** Unresolved: the identifier. Object / Class / SoftObject / SoftClass / WeakObject: the class. Struct: the struct.
	 * Enum, and Byte from TEnumAsByte: the enum. */
	std::string TypeName;
	/** Unresolved only: the declaration had a '*'. */
	bool bPointer = false;
	/** Byte: declared as TEnumAsByte<TypeName>. */
	bool bEnumAsByte = false;
	/** Enum: the underlying C++ type (uint8, int32, ...). */
	std::string EnumUnderlying;
	/** Owning module of TypeName, filled by the resolver (empty for built-in types). */
	std::string TypeModule;
	/** Array / Set: the element. Map: key, value. */
	std::vector<FPropertyType> Inner;

	/** The C++ type as UHT spells it in generated code ("TArray<int32>", "TMap<FName,UObject*>"). */
	std::string GetCppType() const;
	bool IsContainer() const
	{
		return Kind == EPropertyKind::Array || Kind == EPropertyKind::Map || Kind == EPropertyKind::Set;
	}
};

struct FPropertyDef
{
	std::string Name;
	FPropertyType Type;
	int Line = 0;
	/** Flags from specifiers and the access level (not the computed ones FProperty sets at runtime). */
	uint64_t Flags = 0;
	EAccessSpecifier Access = EAccessSpecifier::Public;
	/** Declared inside #if WITH_EDITORONLY_DATA. */
	bool bEditorOnly = false;
	/** C array member (T Name[N]): ArrayDim is CPP_ARRAY_DIM(Name, Outer). */
	bool bFixedArray = false;
	/** uint8 bFlag : 1 style bool; BitfieldStorage is the declared integer type. */
	bool bBitfield = false;
	std::string BitfieldStorage;
	/** Function parameter declared as const T& (structs and containers are passed by reference). */
	bool bConstRef = false;
};

struct FFunctionDef
{
	std::string Name;
	int Line = 0;
	std::vector<FPropertyDef> Params;
	bool bHasReturnValue = false;
	FPropertyDef ReturnValue;
	uint32_t FunctionFlags = 0;
	bool bStatic = false;
	EAccessSpecifier Access = EAccessSpecifier::Public;
};

struct FClassDef
{
	std::string Name;
	std::string SuperName;
	/** "<MODULE>_API" when the class is declared with it, empty otherwise (the generated code then uses NO_API). */
	std::string Api;
	int DeclarationLine = 0;
	/** Lines spanned by UCLASS(...): each gets a <FileId>_<Line>_PROLOG macro. */
	int PrologFirstLine = 0;
	int PrologLastLine = 0;
	int BodyLine = 0;
	bool bLegacyBody = false;
	uint32_t ClassFlags = 0;
	std::string ConfigName;
	bool bHasObjectInitializerConstructor = false;
	bool bHasDefaultConstructor = false;
	bool bHasVTableHelperConstructor = false;
	std::vector<FPropertyDef> Properties;
	std::vector<FFunctionDef> Functions;
	/** Filled by the resolver: owning module of SuperName. */
	std::string SuperModule;
};

struct FStructDef
{
	std::string Name;
	/** Reflected base struct, or empty. */
	std::string SuperName;
	std::string Api;
	int DeclarationLine = 0;
	int BodyLine = 0;
	uint32_t StructFlags = 0;
	/**
	 * USTRUCT(NoExport) inside #if !CPP: the C++ type is defined elsewhere (a Core type such as FVector). The generated
	 * code reads the real type's offsets and checks that this declaration matches it; there is no GENERATED_BODY.
	 */
	bool bNoExport = false;
	std::vector<FPropertyDef> Properties;
	std::string SuperModule;
};

struct FEnumDef
{
	std::string Name;
	int DeclarationLine = 0;
	bool bEnumClass = false;
	/** Explicit underlying type, or empty. */
	std::string Underlying;
	std::vector<std::string> Enumerators;
	bool bFlags = false;
};

enum class ETypeKind
{
	Class,
	Struct,
	Enum
};

/** One parsed header (UHT: FUnrealSourceFile). */
struct FUnrealSourceFile
{
	/** Path as read from disk. */
	std::string Path;
	/** Path printed in diagnostics. */
	std::string DisplayPath;
	/** Path the generated .gen.cpp includes ("GameFramework/Actor.h"). */
	std::string IncludePath;
	/** File name without extension ("Actor"). */
	std::string BaseName;
	/** Path relative to the root with non-identifier characters replaced by '_' (UE: CURRENT_FILE_ID). */
	std::string FileId;

	std::vector<FClassDef> Classes;
	std::vector<FStructDef> Structs;
	std::vector<FEnumDef> Enums;
	/** Declaration order: (kind, index into the matching vector). */
	std::vector<std::pair<ETypeKind, int>> Order;
};

/** What the resolver knows about a reflected type name. */
struct FTypeInfo
{
	ETypeKind Kind = ETypeKind::Class;
	std::string Module;
	bool bEnumClass = false;
	std::string EnumUnderlying;
	/** "<file>(<line>)" of a local declaration, for duplicate-type errors. */
	std::string Location;
};

/**
 * Every reflected type visible to a module: CoreUObject's intrinsic classes, the type indexes of the reflected
 * modules it depends on and its own types (UHT: the global type map built while parsing all modules).
 */
class FTypeTable
{
public:
	FTypeTable();

	/** Adds the types of a <Module>.lhttypes index. */
	bool LoadIndex(const std::string& Path, FDiagnostics& Diagnostics);
	/** Adds the types declared by the parsed headers; reports duplicates. */
	void AddLocalTypes(const std::string& Module, std::vector<FUnrealSourceFile>& Files, FDiagnostics& Diagnostics);
	const FTypeInfo* Find(const std::string& Name) const;

	/** Resolves every property, parameter and super type of the files; reports unknown or misused types. */
	void Resolve(std::vector<FUnrealSourceFile>& Files, FDiagnostics& Diagnostics) const;

	/** The <Module>.lhttypes content for the given files. */
	static std::string WriteIndex(const std::string& Module, const std::vector<FUnrealSourceFile>& Files);

private:
	std::map<std::string, FTypeInfo> Types;
};

/** "0x0010000000000001" style hex used for flags in the generated code. */
std::string FormatHex64(uint64_t Value);
std::string FormatHex32(uint32_t Value);
