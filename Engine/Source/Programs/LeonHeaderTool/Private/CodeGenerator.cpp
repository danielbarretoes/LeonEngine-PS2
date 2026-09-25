#include "CodeGenerator.h"

#include <cctype>
#include <set>

namespace
{
	const char* const Banner = "/*===========================================================================\n"
							   "\tGenerated code exported from LeonHeaderTool.\n"
							   "\tDO NOT modify this manually! Edit the corresponding .h files instead!\n"
							   "===========================================================================*/\n\n";
	const char* const ObjectFlags = "RF_Public|RF_Transient|RF_MarkAsNative";
	const char* const ParamsBasePointer = "(const UE4CodeGen_Private::FPropertyParamsBase*)";

	std::string ToUpper(std::string Text)
	{
		for (char& C : Text)
		{
			C = static_cast<char>(std::toupper(static_cast<unsigned char>(C)));
		}
		return Text;
	}

	/** "#define Name \" + one continued line per entry; the last line has no backslash. */
	std::string DefineMacro(const std::string& Name, const std::vector<std::string>& Lines)
	{
		if (Lines.empty())
		{
			return "#define " + Name + "\n";
		}
		std::string Text = "#define " + Name + " \\\n";
		for (size_t Index = 0; Index < Lines.size(); ++Index)
		{
			Text += Lines[Index] + (Index + 1 < Lines.size() ? " \\\n" : "\n");
		}
		return Text;
	}

	/** Declarations of the Z_Construct_* functions a .gen.cpp calls, in first-use order (UE: Cross Module References).
	 */
	class FCrossReferences
	{
	public:
		void Add(const std::string& Declaration)
		{
			if (Seen.insert(Declaration).second)
			{
				Lines.push_back(Declaration);
			}
		}

		std::string Write() const
		{
			std::string Text = "// Cross Module References\n";
			for (const std::string& Line : Lines)
			{
				Text += "\t" + Line + "\n";
			}
			return Text + "// End Cross Module References\n";
		}

	private:
		std::vector<std::string> Lines;
		std::set<std::string> Seen;
	};

	/** Resolves Z_Construct function names and records their declarations. */
	struct FSymbols
	{
		const FManifest& Manifest;
		FCrossReferences& References;

		std::string Api(const std::string& Module) const
		{
			return Module == Manifest.Module ? Manifest.Api : ToUpper(Module) + "_API";
		}

		std::string ClassNoRegister(const std::string& Name, const std::string& Module) const
		{
			const std::string Function = "Z_Construct_UClass_" + Name + "_NoRegister";
			References.Add(Api(Module) + " UClass* " + Function + "();");
			return Function;
		}

		std::string Class(const std::string& Name, const std::string& Module) const
		{
			const std::string Function = "Z_Construct_UClass_" + Name;
			References.Add(Api(Module) + " UClass* " + Function + "();");
			return Function;
		}

		std::string Struct(const std::string& Name, const std::string& Module) const
		{
			const std::string Function = "Z_Construct_UScriptStruct_" + Name;
			References.Add(Api(Module) + " UScriptStruct* " + Function + "();");
			return Function;
		}

		std::string Enum(const std::string& Name, const std::string& Module) const
		{
			const std::string Function = "Z_Construct_UEnum_" + Module + "_" + Name;
			References.Add(Api(Module) + " UEnum* " + Function + "();");
			return Function;
		}
	};

	/** Generated code for a run of properties that share the same WITH_EDITORONLY_DATA state. */
	struct FPropertyBlock
	{
		bool bEditorOnly = false;
		std::string Declarations;
		std::string Definitions;
		std::string Pointers;
	};

	FPropertyType MakeUnderlyingType(const std::string& Underlying)
	{
		FPropertyType Type;
		if (Underlying == "uint8")
		{
			Type.Kind = EPropertyKind::Byte;
		}
		else if (Underlying == "uint16")
		{
			Type.Kind = EPropertyKind::UInt16;
		}
		else if (Underlying == "uint32")
		{
			Type.Kind = EPropertyKind::UInt32;
		}
		else if (Underlying == "uint64")
		{
			Type.Kind = EPropertyKind::UInt64;
		}
		else if (Underlying == "int8")
		{
			Type.Kind = EPropertyKind::Int8;
		}
		else if (Underlying == "int16")
		{
			Type.Kind = EPropertyKind::Int16;
		}
		else if (Underlying == "int64")
		{
			Type.Kind = EPropertyKind::Int64;
		}
		else
		{
			Type.Kind = EPropertyKind::Int;
		}
		return Type;
	}

	/** F<Type>PropertyParams and EPropertyGenFlags name of a property kind. */
	void GetParamsNames(EPropertyKind Kind, std::string& OutParams, std::string& OutGenFlag)
	{
		switch (Kind)
		{
			case EPropertyKind::Bool:
				OutParams = "FBoolPropertyParams", OutGenFlag = "Bool";
				return;
			case EPropertyKind::Int8:
				OutParams = "FInt8PropertyParams", OutGenFlag = "Int8";
				return;
			case EPropertyKind::Int16:
				OutParams = "FInt16PropertyParams", OutGenFlag = "Int16";
				return;
			case EPropertyKind::Int:
				OutParams = "FIntPropertyParams", OutGenFlag = "Int";
				return;
			case EPropertyKind::Int64:
				OutParams = "FInt64PropertyParams", OutGenFlag = "Int64";
				return;
			case EPropertyKind::Byte:
				OutParams = "FBytePropertyParams", OutGenFlag = "Byte";
				return;
			case EPropertyKind::UInt16:
				OutParams = "FUInt16PropertyParams", OutGenFlag = "UInt16";
				return;
			case EPropertyKind::UInt32:
				OutParams = "FUInt32PropertyParams", OutGenFlag = "UInt32";
				return;
			case EPropertyKind::UInt64:
				OutParams = "FUInt64PropertyParams", OutGenFlag = "UInt64";
				return;
			case EPropertyKind::UnsizedInt:
				OutParams = "FUnsizedIntPropertyParams", OutGenFlag = "UnsizedInt";
				return;
			case EPropertyKind::UnsizedUInt:
				OutParams = "FUnsizedUIntPropertyParams", OutGenFlag = "UnsizedUInt";
				return;
			case EPropertyKind::Float:
				OutParams = "FFloatPropertyParams", OutGenFlag = "Float";
				return;
			case EPropertyKind::Double:
				OutParams = "FDoublePropertyParams", OutGenFlag = "Double";
				return;
			case EPropertyKind::Str:
				OutParams = "FStrPropertyParams", OutGenFlag = "Str";
				return;
			case EPropertyKind::Name:
				OutParams = "FNamePropertyParams", OutGenFlag = "Name";
				return;
			case EPropertyKind::Text:
				OutParams = "FTextPropertyParams", OutGenFlag = "Text";
				return;
			case EPropertyKind::Enum:
				OutParams = "FEnumPropertyParams", OutGenFlag = "Enum";
				return;
			case EPropertyKind::Object:
				OutParams = "FObjectPropertyParams", OutGenFlag = "Object";
				return;
			case EPropertyKind::Class:
				OutParams = "FClassPropertyParams", OutGenFlag = "Class";
				return;
			case EPropertyKind::SoftObject:
				OutParams = "FSoftObjectPropertyParams", OutGenFlag = "SoftObject";
				return;
			case EPropertyKind::SoftClass:
				OutParams = "FSoftClassPropertyParams", OutGenFlag = "SoftClass";
				return;
			case EPropertyKind::WeakObject:
				OutParams = "FWeakObjectPropertyParams", OutGenFlag = "WeakObject";
				return;
			case EPropertyKind::Struct:
				OutParams = "FStructPropertyParams", OutGenFlag = "Struct";
				return;
			case EPropertyKind::Array:
				OutParams = "FArrayPropertyParams", OutGenFlag = "Array";
				return;
			case EPropertyKind::Map:
				OutParams = "FMapPropertyParams", OutGenFlag = "Map";
				return;
			case EPropertyKind::Set:
				OutParams = "FSetPropertyParams", OutGenFlag = "Set";
				return;
			case EPropertyKind::Unresolved:
				break;
		}
		OutParams = "FGenericPropertyParams", OutGenFlag = "None";
	}

	bool IsObjectWrapper(const FPropertyType& Type)
	{
		return Type.Kind == EPropertyKind::Class || Type.Kind == EPropertyKind::SoftObject ||
			Type.Kind == EPropertyKind::SoftClass || Type.Kind == EPropertyKind::WeakObject;
	}

	/** Parameters passed as const T& that UHT marks as by-reference (structs and containers). */
	bool IsPassedByReference(const FPropertyDef& Param)
	{
		return Param.bConstRef && (Param.Type.Kind == EPropertyKind::Struct || Param.Type.IsContainer());
	}

	/** Emits the F<Type>PropertyParams of properties, children (inner, key, value, underlying) first. */
	struct FPropertyEmitter
	{
		const FSymbols& Symbols;
		std::string Statics;
		std::string Outer;

		void Emit(const FPropertyType& Type, const std::string& Var, const std::string& NameUtf8, uint64_t Flags,
			const std::string& ArrayDim, const std::string& Offset, const std::string& BoolMember,
			const std::string& BoolStorage, FPropertyBlock& Block) const
		{
			switch (Type.Kind)
			{
				case EPropertyKind::Enum:
					Emit(MakeUnderlyingType(Type.EnumUnderlying), Var + "_Underlying", "UnderlyingType", 0, "1", "0",
						"", "", Block);
					break;
				case EPropertyKind::Array:
					Emit(Type.Inner[0], Var + "_Inner", NameUtf8, InnerFlags(Type.Inner[0]), "1", "0", "", "", Block);
					break;
				case EPropertyKind::Set:
					Emit(Type.Inner[0], Var + "_ElementProp", NameUtf8, InnerFlags(Type.Inner[0]), "1", "0", "", "",
						Block);
					break;
				case EPropertyKind::Map:
					Emit(Type.Inner[1], Var + "_ValueProp", NameUtf8, InnerFlags(Type.Inner[1]), "1", "1", "", "",
						Block);
					Emit(Type.Inner[0], Var + "_Key_KeyProp", NameUtf8 + "_Key", InnerFlags(Type.Inner[0]), "1", "0",
						"", "", Block);
					break;
				default:
					break;
			}

			std::string Params;
			std::string GenFlag;
			GetParamsNames(Type.Kind, Params, GenFlag);
			std::string Initializer = "{ \"" + NameUtf8 + "\", nullptr, (EPropertyFlags)" + FormatHex64(Flags) +
				", UE4CodeGen_Private::EPropertyGenFlags::" + GenFlag;
			if (Type.Kind == EPropertyKind::Bool)
			{
				if (BoolStorage.empty())
				{
					Initializer += " | UE4CodeGen_Private::EPropertyGenFlags::NativeBool";
				}
				Initializer += std::string(", ") + ObjectFlags + ", " + ArrayDim + ", sizeof(" +
					(BoolStorage.empty() ? std::string("bool") : BoolStorage) + "), ";
				if (BoolMember.empty())
				{
					Initializer += "0, nullptr";
				}
				else
				{
					Initializer += "sizeof(" + Outer + "), &" + Statics + "::" + Var + "_SetBit";
					Block.Declarations += "\t\tstatic void " + Var + "_SetBit(void* Obj);\n";
					Block.Definitions += "\tvoid " + Statics + "::" + Var + "_SetBit(void* Obj)\n\t{\n\t\t((" + Outer +
						"*)Obj)->" + BoolMember + " = 1;\n\t}\n";
				}
			}
			else
			{
				Initializer += std::string(", ") + ObjectFlags + ", " + ArrayDim + ", " + Offset + GetExtra(Type);
			}
			Initializer += " }";
			Block.Declarations += "\t\tstatic const UE4CodeGen_Private::" + Params + " " + Var + ";\n";
			Block.Definitions +=
				"\tconst UE4CodeGen_Private::" + Params + " " + Statics + "::" + Var + " = " + Initializer + ";\n";
			Block.Pointers += std::string("\t\t") + ParamsBasePointer + "&" + Statics + "::" + Var + ",\n";
		}

		static uint64_t InnerFlags(const FPropertyType& Type)
		{
			return IsObjectWrapper(Type) ? EPropertyFlagBits::UObjectWrapper : 0;
		}

		std::string GetExtra(const FPropertyType& Type) const
		{
			switch (Type.Kind)
			{
				case EPropertyKind::Byte:
					return Type.bEnumAsByte ? ", " + Symbols.Enum(Type.TypeName, Type.TypeModule) : ", nullptr";
				case EPropertyKind::Enum:
					return ", " + Symbols.Enum(Type.TypeName, Type.TypeModule);
				case EPropertyKind::Object:
				case EPropertyKind::SoftObject:
				case EPropertyKind::WeakObject:
				case EPropertyKind::SoftClass:
					return ", " + Symbols.ClassNoRegister(Type.TypeName, Type.TypeModule);
				case EPropertyKind::Class:
					return ", " + Symbols.ClassNoRegister(Type.TypeName, Type.TypeModule) + ", " +
						Symbols.Class("UClass", "CoreUObject");
				case EPropertyKind::Struct:
					return ", " + Symbols.Struct(Type.TypeName, Type.TypeModule);
				case EPropertyKind::Array:
					return ", EArrayPropertyFlags::None";
				case EPropertyKind::Map:
					return ", EMapPropertyFlags::None";
				default:
					return "";
			}
		}
	};

	/** Joins blocks, wrapping editor-only runs in #if WITH_EDITORONLY_DATA. */
	std::string JoinBlocks(
		const std::vector<FPropertyBlock>& Blocks, std::string FPropertyBlock::* Member, bool bMarkers = true)
	{
		std::string Text;
		bool bOpen = false;
		for (const FPropertyBlock& Block : Blocks)
		{
			if (bMarkers && Block.bEditorOnly != bOpen)
			{
				Text += Block.bEditorOnly ? "#if WITH_EDITORONLY_DATA\n" : "#endif // WITH_EDITORONLY_DATA\n";
				bOpen = Block.bEditorOnly;
			}
			Text += Block.*Member;
		}
		if (bOpen)
		{
			Text += "#endif // WITH_EDITORONLY_DATA\n";
		}
		return Text;
	}

	/** Property tables of a class, struct or function: declarations, definitions and the PropPointers array. */
	struct FPropertyTables
	{
		std::vector<FPropertyBlock> Blocks;
		int NumAlways = 0;
		int NumEditorOnly = 0;
		std::string Statics;

		void Add(const FPropertyEmitter& Emitter, const FPropertyDef& Property, uint64_t Flags)
		{
			FPropertyBlock Block;
			Block.bEditorOnly = Property.bEditorOnly;
			const std::string ArrayDim =
				Property.bFixedArray ? "CPP_ARRAY_DIM(" + Property.Name + ", " + Emitter.Outer + ")" : "1";
			Emitter.Emit(Property.Type, "NewProp_" + Property.Name, Property.Name, Flags, ArrayDim,
				"STRUCT_OFFSET(" + Emitter.Outer + ", " + Property.Name + ")", Property.Name, Property.BitfieldStorage,
				Block);
			(Property.bEditorOnly ? NumEditorOnly : NumAlways)++;
			Blocks.push_back(std::move(Block));
		}

		bool IsEmpty() const
		{
			return NumAlways + NumEditorOnly == 0;
		}

		/** PropPointers only exists with WITH_EDITORONLY_DATA when every property is editor-only. */
		bool IsEditorOnly() const
		{
			return NumAlways == 0 && NumEditorOnly > 0;
		}

		std::string Declarations() const
		{
			std::string Text = JoinBlocks(Blocks, &FPropertyBlock::Declarations);
			if (!IsEmpty())
			{
				Text += Wrap("\t\tstatic const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];\n");
			}
			return Text;
		}

		std::string Definitions() const
		{
			std::string Text = JoinBlocks(Blocks, &FPropertyBlock::Definitions);
			if (!IsEmpty())
			{
				Text += Wrap("\tconst UE4CodeGen_Private::FPropertyParamsBase* const " + Statics +
					"::PropPointers[] = {\n" + JoinBlocks(Blocks, &FPropertyBlock::Pointers, !IsEditorOnly()) +
					"\t};\n");
			}
			return Text;
		}

		/** The "PropPointers, count" pair of a *Params initializer (two lines at the given indentation). */
		std::string ParamsArguments(const std::string& Indent) const
		{
			const std::string Present =
				Indent + Statics + "::PropPointers,\n" + Indent + "UE_ARRAY_COUNT(" + Statics + "::PropPointers),\n";
			const std::string Absent = Indent + "nullptr,\n" + Indent + "0,\n";
			if (IsEmpty())
			{
				return Absent;
			}
			if (IsEditorOnly())
			{
				return "#if WITH_EDITORONLY_DATA\n" + Present + "#else\n" + Absent + "#endif\n";
			}
			return Present;
		}

		std::string Wrap(const std::string& Text) const
		{
			return IsEditorOnly() ? "#if WITH_EDITORONLY_DATA\n" + Text + "#endif\n" : Text;
		}
	};

	std::string ClassFlagNames(uint32_t Flags)
	{
		static const std::pair<uint32_t, const char*> Names[] = {{EClassFlagBits::Abstract, "CLASS_Abstract"},
			{EClassFlagBits::DefaultConfig, "CLASS_DefaultConfig"}, {EClassFlagBits::Config, "CLASS_Config"},
			{EClassFlagBits::Transient, "CLASS_Transient"}, {EClassFlagBits::NotPlaceable, "CLASS_NotPlaceable"},
			{EClassFlagBits::EditInlineNew, "CLASS_EditInlineNew"}, {EClassFlagBits::MinimalAPI, "CLASS_MinimalAPI"}};
		std::string Text = "0";
		for (const auto& Name : Names)
		{
			if (Flags & Name.first)
			{
				Text += std::string(" | ") + Name.second;
			}
		}
		return Text;
	}

	/** The P_GET_* line of an exec thunk and the argument passed to the native call. */
	std::string GetParamMacro(const FPropertyDef& Param, std::string& OutArgument)
	{
		const FPropertyType& Type = Param.Type;
		const std::string Var = "Z_Param_" + Param.Name;
		const std::string OutVar = "Z_Param_Out_" + Param.Name;
		OutArgument = Var;
		const auto Property = [&Var](const char* PropertyClass)
		{ return std::string("P_GET_PROPERTY(") + PropertyClass + "," + Var + ");"; };
		const bool bByReference = IsPassedByReference(Param);
		if (bByReference)
		{
			OutArgument = OutVar;
		}
		switch (Type.Kind)
		{
			case EPropertyKind::Bool:
				return "P_GET_UBOOL(" + Var + ");";
			case EPropertyKind::Int8:
				return Property("FInt8Property");
			case EPropertyKind::Int16:
				return Property("FInt16Property");
			case EPropertyKind::Int:
				return Property("FIntProperty");
			case EPropertyKind::Int64:
				return Property("FInt64Property");
			case EPropertyKind::Byte:
				if (Type.bEnumAsByte)
				{
					OutArgument = "TEnumAsByte<" + Type.TypeName + ">(" + Var + ")";
				}
				return Property("FByteProperty");
			case EPropertyKind::UInt16:
				return Property("FUInt16Property");
			case EPropertyKind::UInt32:
				return Property("FUInt32Property");
			case EPropertyKind::UInt64:
				return Property("FUInt64Property");
			case EPropertyKind::UnsizedInt:
				return Property("FUnsizedIntProperty");
			case EPropertyKind::UnsizedUInt:
				return Property("FUnsizedUIntProperty");
			case EPropertyKind::Float:
				return Property("FFloatProperty");
			case EPropertyKind::Double:
				return Property("FDoubleProperty");
			case EPropertyKind::Str:
				return Property("FStrProperty");
			case EPropertyKind::Name:
				return Property("FNameProperty");
			case EPropertyKind::Text:
				return Property("FTextProperty");
			case EPropertyKind::Enum:
				OutArgument = Type.TypeName + "(" + Var + ")";
				return "P_GET_ENUM(" + Type.TypeName + "," + Var + ");";
			case EPropertyKind::Object:
				return "P_GET_OBJECT(" + Type.TypeName + "," + Var + ");";
			case EPropertyKind::Class:
				return "P_GET_OBJECT(UClass," + Var + ");";
			case EPropertyKind::SoftObject:
				return "P_GET_SOFTOBJECT(" + Type.GetCppType() + "," + Var + ");";
			case EPropertyKind::SoftClass:
				return "P_GET_SOFTCLASS(" + Type.GetCppType() + "," + Var + ");";
			case EPropertyKind::WeakObject:
				return "P_GET_WEAKOBJECT(" + Type.GetCppType() + "," + Var + ");";
			case EPropertyKind::Struct:
				return bByReference ? "P_GET_STRUCT_REF(" + Type.TypeName + "," + OutVar + ");"
									: "P_GET_STRUCT(" + Type.TypeName + "," + Var + ");";
			case EPropertyKind::Array:
				return (bByReference ? "P_GET_TARRAY_REF(" : "P_GET_TARRAY(") + Type.Inner[0].GetCppType() + "," +
					(bByReference ? OutVar : Var) + ");";
			case EPropertyKind::Set:
				return (bByReference ? "P_GET_TSET_REF(" : "P_GET_TSET(") + Type.Inner[0].GetCppType() + "," +
					(bByReference ? OutVar : Var) + ");";
			case EPropertyKind::Map:
				return (bByReference ? "P_GET_TMAP_REF(" : "P_GET_TMAP(") + Type.Inner[0].GetCppType() + "," +
					Type.Inner[1].GetCppType() + "," + (bByReference ? OutVar : Var) + ");";
			case EPropertyKind::Unresolved:
				break;
		}
		return "";
	}

	std::string StripPrefix(const std::string& Name)
	{
		return Name.size() > 1 ? Name.substr(1) : Name;
	}

	/**
	 * Checks that a NoExport declaration describes its C++ type: a layout struct with the declared members (derived
	 * from the declared base) must have the C++ type's size, and each member the C++ member's type and offset. The
	 * reflection data itself uses the C++ type's offsets (STRUCT_OFFSET), so a mismatch is a compile error, never wrong
	 * data. Non-public members need the C++ type to befriend the _Statics struct (FTransform does, as in UE).
	 */
	std::string NoExportLayoutChecks(const FStructDef& Struct)
	{
		const std::string& Name = Struct.Name;
		std::string Text = "\t\t/** The NoExport declaration of " + Name + ", checked against the C++ type. */\n";
		Text += "\t\tstruct FNoExportLayout" + (Struct.SuperName.empty() ? std::string() : " : " + Struct.SuperName) +
			"\n\t\t{\n";
		for (const FPropertyDef& Property : Struct.Properties)
		{
			Text += "\t\t\t" + Property.Type.GetCppType() + " " + Property.Name + ";\n";
		}
		Text += "\t\t};\n";
		Text += "\t\tstatic_assert(sizeof(FNoExportLayout) == sizeof(" + Name + "), \"NoExport " + Name +
			" does not match the size of the C++ type\");\n";
		for (const FPropertyDef& Property : Struct.Properties)
		{
			const std::string Member = Name + "::" + Property.Name;
			Text += "\t\tstatic_assert(TIsSame<decltype(" + Member + "), " + Property.Type.GetCppType() +
				">::Value, \"NoExport " + Member + " does not match the type of the C++ member\");\n";
			Text += "\t\tstatic_assert(STRUCT_OFFSET(FNoExportLayout, " + Property.Name + ") == STRUCT_OFFSET(" + Name +
				", " + Property.Name + "), \"NoExport " + Member + " does not match the offset of the C++ member\");\n";
		}
		return Text;
	}
} // namespace

FCodeGenerator::FCodeGenerator(const FManifest& InManifest)
	: Manifest(InManifest)
{
}

std::string FCodeGenerator::GetPackageFunction() const
{
	std::string Name = Manifest.Package;
	for (char& C : Name)
	{
		if (std::isalnum(static_cast<unsigned char>(C)) == 0)
		{
			C = '_';
		}
	}
	return "Z_Construct_UPackage_" + Name;
}

void FCodeGenerator::GenerateHeader(const FUnrealSourceFile& File, std::vector<FGeneratedFile>& OutFiles) const
{
	OutFiles.push_back({File.BaseName + ".generated.h", GenerateGeneratedHeader(File)});
	OutFiles.push_back({File.BaseName + ".gen.cpp", GenerateGenCpp(File)});
}

std::string FCodeGenerator::GenerateGeneratedHeader(const FUnrealSourceFile& File) const
{
	const std::string& FileId = File.FileId;
	std::string Text = Banner;
	Text += "#include \"UObject/ObjectMacros.h\"\n#include \"UObject/ScriptMacros.h\"\n\n";
	Text += "PRAGMA_DISABLE_DEPRECATION_WARNINGS\n";
	const std::string Guard = ToUpper(Manifest.Module) + "_" + File.BaseName + "_generated_h";
	Text += "#ifdef " + Guard + "\n#error \"" + File.BaseName +
		".generated.h already included, missing '#pragma once' in " + File.BaseName + ".h\"\n#endif\n#define " + Guard +
		"\n\n";

	for (const std::pair<ETypeKind, int>& Entry : File.Order)
	{
		if (Entry.first == ETypeKind::Class)
		{
			const FClassDef& Class = File.Classes[Entry.second];
			const std::string& Name = Class.Name;
			const std::string Api = Class.Api.empty() ? "NO_API" : Class.Api;
			const std::string Prefix = FileId + "_" + std::to_string(Class.BodyLine);
			const std::string Super = Class.SuperName.empty() ? Name : Class.SuperName;

			std::vector<std::string> Wrappers;
			for (const FFunctionDef& Function : Class.Functions)
			{
				Wrappers.push_back("\tDECLARE_FUNCTION(exec" + Function.Name + ");");
			}
			Text += DefineMacro(Prefix + "_RPC_WRAPPERS", Wrappers) + "\n";
			Text += DefineMacro(Prefix + "_RPC_WRAPPERS_NO_PURE_DECLS", Wrappers) + "\n";

			std::vector<std::string> InClass = {"private:", "\tstatic void StaticRegisterNatives" + Name + "();",
				"\tfriend struct Z_Construct_UClass_" + Name + "_Statics;", "public:",
				"\tDECLARE_CLASS(" + Name + ", " + Super + ", COMPILED_IN_FLAGS(" + ClassFlagNames(Class.ClassFlags) +
					"), CASTCLASS_None, TEXT(\"" + Manifest.Package + "\"), " + Api + ")",
				"\tDECLARE_SERIALIZER(" + Name + ")"};
			if (!Class.ConfigName.empty())
			{
				InClass.push_back(
					"\tstatic const TCHAR* StaticConfigName() {return TEXT(\"" + Class.ConfigName + "\");}");
			}
			Text += DefineMacro(Prefix + "_INCLASS_NO_PURE_DECLS", InClass) + "\n";
			Text += DefineMacro(Prefix + "_INCLASS", InClass) + "\n";

			const std::vector<std::string> PrivateCopy = {
				"private:", "\t/** Private move- and copy-constructors, should never be used */",
				"\t" + Api + " " + Name + "(" + Name + "&&);", "\t" + Api + " " + Name + "(const " + Name + "&);",
				"public:"};
			std::vector<std::string> VTable;
			if (!Class.bHasVTableHelperConstructor)
			{
				VTable.push_back("\tDECLARE_VTABLE_PTR_HELPER_CTOR(" + Api + ", " + Name + ");");
			}
			VTable.push_back("\tDEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(" + Name + ");");

			std::vector<std::string> Standard = {
				"\t/** Standard constructor, called after all reflected properties have been initialized */",
				"\t" + Api + " " + Name + "(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());",
				"\tDEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(" + Name + ")"};
			Standard.insert(Standard.end(), VTable.begin(), VTable.end());
			Standard.insert(Standard.end(), PrivateCopy.begin(), PrivateCopy.end());
			Text += DefineMacro(Prefix + "_STANDARD_CONSTRUCTORS", Standard) + "\n";

			std::vector<std::string> Enhanced;
			if (!Class.bHasObjectInitializerConstructor && !Class.bHasDefaultConstructor)
			{
				Enhanced.push_back(
					"\t/** Standard constructor, called after all reflected properties have been initialized */");
				Enhanced.push_back("\t" + Api + " " + Name +
					"(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : "
					"Super(ObjectInitializer) { }");
			}
			Enhanced.insert(Enhanced.end(), PrivateCopy.begin(), PrivateCopy.end());
			Enhanced.insert(Enhanced.end(), VTable.begin(), VTable.end());
			Enhanced.push_back(Class.bHasDefaultConstructor && !Class.bHasObjectInitializerConstructor
					? "\tDEFINE_DEFAULT_CONSTRUCTOR_CALL(" + Name + ")"
					: "\tDEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(" + Name + ")");
			Text += DefineMacro(Prefix + "_ENHANCED_CONSTRUCTORS", Enhanced) + "\n";

			for (int Line = Class.PrologFirstLine; Line <= Class.PrologLastLine; ++Line)
			{
				Text += DefineMacro(FileId + "_" + std::to_string(Line) + "_PROLOG", {});
			}
			Text += "\n";
			Text += DefineMacro(Prefix + "_GENERATED_BODY_LEGACY",
						{"PRAGMA_DISABLE_DEPRECATION_WARNINGS", "public:", "\t" + Prefix + "_RPC_WRAPPERS",
							"\t" + Prefix + "_INCLASS", "\t" + Prefix + "_STANDARD_CONSTRUCTORS",
							"public:", "PRAGMA_ENABLE_DEPRECATION_WARNINGS"}) +
				"\n";
			Text +=
				DefineMacro(Prefix + "_GENERATED_BODY",
					{"PRAGMA_DISABLE_DEPRECATION_WARNINGS", "public:", "\t" + Prefix + "_RPC_WRAPPERS_NO_PURE_DECLS",
						"\t" + Prefix + "_INCLASS_NO_PURE_DECLS", "\t" + Prefix + "_ENHANCED_CONSTRUCTORS",
						"private:", "PRAGMA_ENABLE_DEPRECATION_WARNINGS"}) +
				"\n";
			Text += "template<> " + Manifest.Api + " UClass* StaticClass<class " + Name + ">();\n\n";
		}
		else if (Entry.first == ETypeKind::Struct)
		{
			const FStructDef& Struct = File.Structs[Entry.second];
			// A NoExport struct has no GENERATED_BODY: its C++ type is defined elsewhere.
			if (!Struct.bNoExport)
			{
				std::vector<std::string> Body = {
					"\tfriend struct Z_Construct_UScriptStruct_" + Struct.Name + "_Statics;",
					"\tstatic class UScriptStruct* StaticStruct();"};
				if (!Struct.SuperName.empty())
				{
					Body.push_back("\ttypedef " + Struct.SuperName + " Super;");
				}
				Text += DefineMacro(FileId + "_" + std::to_string(Struct.BodyLine) + "_GENERATED_BODY", Body) + "\n";
			}
			Text += "template<> " + Manifest.Api + " UScriptStruct* StaticStruct<struct " + Struct.Name + ">();\n\n";
		}
		else
		{
			const FEnumDef& Enum = File.Enums[Entry.second];
			std::vector<std::string> Values;
			for (const std::string& Enumerator : Enum.Enumerators)
			{
				Values.push_back("\top(" + (Enum.bEnumClass ? Enum.Name + "::" : std::string()) + Enumerator + ")");
			}
			Text += DefineMacro("FOREACH_ENUM_" + ToUpper(Enum.Name) + "(op)", Values) + "\n";
			// Only an enum class with an underlying type can be declared before its header defines it (as in UHT).
			if (Enum.bEnumClass && !Enum.Underlying.empty())
			{
				Text += "enum class " + Enum.Name + " : " + Enum.Underlying + ";\n";
				Text += "template<> " + Manifest.Api + " UEnum* StaticEnum<" + Enum.Name + ">();\n";
			}
			Text += "\n";
		}
	}

	Text += "#undef CURRENT_FILE_ID\n#define CURRENT_FILE_ID " + FileId + "\n\n";
	Text += "PRAGMA_ENABLE_DEPRECATION_WARNINGS\n";
	return Text;
}

std::string FCodeGenerator::GenerateGenCpp(const FUnrealSourceFile& File) const
{
	FCrossReferences References;
	const FSymbols Symbols{Manifest, References};
	const std::string Module = Manifest.Module;
	const std::string PackageFunction = GetPackageFunction();

	// The file's own types come first in the cross references, as in UHT.
	for (const std::pair<ETypeKind, int>& Entry : File.Order)
	{
		if (Entry.first == ETypeKind::Class)
		{
			Symbols.ClassNoRegister(File.Classes[Entry.second].Name, Module);
			Symbols.Class(File.Classes[Entry.second].Name, Module);
		}
		else if (Entry.first == ETypeKind::Struct)
		{
			Symbols.Struct(File.Structs[Entry.second].Name, Module);
		}
		else
		{
			Symbols.Enum(File.Enums[Entry.second].Name, Module);
		}
	}

	std::string Body;
	for (const std::pair<ETypeKind, int>& Entry : File.Order)
	{
		if (Entry.first == ETypeKind::Enum)
		{
			const FEnumDef& Enum = File.Enums[Entry.second];
			const std::string Construct = Symbols.Enum(Enum.Name, Module);
			const std::string Statics = Construct + "_Statics";
			Body += "\tstatic UEnum* " + Enum.Name + "_StaticEnum()\n\t{\n\t\tstatic UEnum* Singleton = nullptr;\n";
			Body += "\t\tif (!Singleton)\n\t\t{\n\t\t\tSingleton = GetStaticEnum(" + Construct + ", " +
				PackageFunction + "(), TEXT(\"" + Enum.Name + "\"));\n\t\t}\n\t\treturn Singleton;\n\t}\n";
			Body += "\ttemplate<> " + Manifest.Api + " UEnum* StaticEnum<" + Enum.Name + ">()\n\t{\n\t\treturn " +
				Enum.Name + "_StaticEnum();\n\t}\n";
			Body += "\tstruct " + Statics + "\n\t{\n";
			Body += "\t\tstatic const UE4CodeGen_Private::FEnumeratorParam Enumerators[];\n";
			Body += "\t\tstatic const UE4CodeGen_Private::FEnumParams EnumParams;\n\t};\n";
			Body += "\tconst UE4CodeGen_Private::FEnumeratorParam " + Statics + "::Enumerators[] = {\n";
			for (const std::string& Enumerator : Enum.Enumerators)
			{
				const std::string Qualified = Enum.bEnumClass ? Enum.Name + "::" + Enumerator : Enumerator;
				Body += "\t\t{ \"" + Qualified + "\", (int64)" + Qualified + " },\n";
			}
			Body += "\t};\n";
			Body += "\tconst UE4CodeGen_Private::FEnumParams " + Statics + "::EnumParams = {\n";
			Body += "\t\t(UObject*(*)())" + PackageFunction + ",\n\t\tnullptr,\n";
			Body += "\t\t\"" + Enum.Name + "\",\n\t\t\"" + Enum.Name + "\",\n";
			Body += "\t\t" + Statics + "::Enumerators,\n\t\tUE_ARRAY_COUNT(" + Statics + "::Enumerators),\n";
			Body += std::string("\t\t") + ObjectFlags + ",\n";
			Body += std::string("\t\t") + (Enum.bFlags ? "EEnumFlags::Flags" : "EEnumFlags::None") + ",\n";
			Body += std::string("\t\t(uint8)UEnum::ECppForm::") + (Enum.bEnumClass ? "EnumClass" : "Regular") + ",\n";
			Body += "\t};\n";
			Body += "\tUEnum* " + Construct +
				"()\n\t{\n\t\tstatic UEnum* ReturnEnum = nullptr;\n\t\tif (!ReturnEnum)\n\t\t{\n";
			Body += "\t\t\tUE4CodeGen_Private::ConstructUEnum(ReturnEnum, " + Statics +
				"::EnumParams);\n\t\t}\n\t\treturn ReturnEnum;\n\t}\n";
		}
		else if (Entry.first == ETypeKind::Struct)
		{
			const FStructDef& Struct = File.Structs[Entry.second];
			const std::string& Name = Struct.Name;
			const std::string Construct = Symbols.Struct(Name, Module);
			const std::string Statics = Construct + "_Statics";
			// A NoExport struct cannot get a StaticStruct() member: it gets a file-local function, as enums do.
			const std::string StaticStructFunction =
				Struct.bNoExport ? Name + "_StaticStruct" : Name + "::StaticStruct";
			Body += std::string(Struct.bNoExport ? "\tstatic class UScriptStruct* " : "\tclass UScriptStruct* ") +
				StaticStructFunction + "()\n\t{\n";
			Body += "\t\tstatic class UScriptStruct* Singleton = nullptr;\n\t\tif (!Singleton)\n\t\t{\n";
			Body += "\t\t\tSingleton = GetStaticStruct(" + Construct + ", " + PackageFunction + "(), TEXT(\"" +
				StripPrefix(Name) + "\"), sizeof(" + Name + "), 0);\n\t\t}\n\t\treturn Singleton;\n\t}\n";
			Body += "\ttemplate<> " + Manifest.Api + " UScriptStruct* StaticStruct<" + Name + ">()\n\t{\n\t\treturn " +
				StaticStructFunction + "();\n\t}\n";

			FPropertyTables Tables;
			Tables.Statics = Statics;
			const FPropertyEmitter Emitter{Symbols, Statics, Name};
			for (const FPropertyDef& Property : Struct.Properties)
			{
				Tables.Add(Emitter, Property, Property.Flags);
			}
			const std::string SuperFunction =
				Struct.SuperName.empty() ? "nullptr" : Symbols.Struct(Struct.SuperName, Struct.SuperModule);

			Body += "\tstruct " + Statics + "\n\t{\n\t\tstatic void* NewStructOps();\n";
			if (Struct.bNoExport)
			{
				Body += NoExportLayoutChecks(Struct);
			}
			Body += Tables.Declarations();
			Body += "\t\tstatic const UE4CodeGen_Private::FStructParams ReturnStructParams;\n\t};\n";
			Body += "\tvoid* " + Statics +
				"::NewStructOps()\n\t{\n\t\treturn (UScriptStruct::ICppStructOps*)new "
				"UScriptStruct::TCppStructOps<" +
				Name + ">();\n\t}\n";
			Body += Tables.Definitions();
			Body += "\tconst UE4CodeGen_Private::FStructParams " + Statics + "::ReturnStructParams = {\n";
			Body += "\t\t(UObject* (*)())" + PackageFunction + ",\n";
			Body += "\t\t" + SuperFunction + ",\n\t\t&NewStructOps,\n";
			Body += "\t\t\"" + StripPrefix(Name) + "\",\n\t\tsizeof(" + Name + "),\n\t\talignof(" + Name + "),\n";
			Body += Tables.ParamsArguments("\t\t");
			Body += std::string("\t\t") + ObjectFlags + ",\n";
			Body += "\t\tEStructFlags(" + FormatHex32(Struct.StructFlags) + "),\n\t};\n";
			Body += "\tUScriptStruct* " + Construct + "()\n\t{\n\t\tstatic UScriptStruct* ReturnStruct = nullptr;\n";
			Body += "\t\tif (!ReturnStruct)\n\t\t{\n\t\t\tUE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, " +
				Statics + "::ReturnStructParams);\n\t\t}\n\t\treturn ReturnStruct;\n\t}\n";
		}
		else
		{
			const FClassDef& Class = File.Classes[Entry.second];
			const std::string& Name = Class.Name;
			const std::string Construct = Symbols.Class(Name, Module);
			const std::string Statics = Construct + "_Statics";

			// Exec thunks and native function registration.
			for (const FFunctionDef& Function : Class.Functions)
			{
				Body += "\tDEFINE_FUNCTION(" + Name + "::exec" + Function.Name + ")\n\t{\n";
				std::string Arguments;
				for (const FPropertyDef& Param : Function.Params)
				{
					std::string Argument;
					Body += "\t\t" + GetParamMacro(Param, Argument) + "\n";
					Arguments += (Arguments.empty() ? "" : ",") + Argument;
				}
				Body += "\t\tP_FINISH;\n\t\tP_NATIVE_BEGIN;\n\t\t";
				if (Function.bHasReturnValue)
				{
					Body += "*(" + Function.ReturnValue.Type.GetCppType() + "*)Z_Param__Result=";
				}
				Body += (Function.bStatic ? Name + "::" : std::string("P_THIS->")) + Function.Name + "(" + Arguments +
					");\n";
				Body += "\t\tP_NATIVE_END;\n\t}\n";
			}
			Body += "\tvoid " + Name + "::StaticRegisterNatives" + Name + "()\n\t{\n";
			if (!Class.Functions.empty())
			{
				Body += "\t\tUClass* Class = " + Name + "::StaticClass();\n";
				Body += "\t\tstatic const FNameNativePtrPair Funcs[] = {\n";
				for (const FFunctionDef& Function : Class.Functions)
				{
					Body += "\t\t\t{ \"" + Function.Name + "\", &" + Name + "::exec" + Function.Name + " },\n";
				}
				Body +=
					"\t\t};\n\t\tFNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));\n";
			}
			Body += "\t}\n";

			// One Z_Construct_UFunction per UFUNCTION.
			for (const FFunctionDef& Function : Class.Functions)
			{
				const std::string FunctionConstruct = "Z_Construct_UFunction_" + Name + "_" + Function.Name;
				const std::string FunctionStatics = FunctionConstruct + "_Statics";
				const std::string Parms = StripPrefix(Name) + "_event" + Function.Name + "_Parms";
				FPropertyTables Tables;
				Tables.Statics = FunctionStatics;
				const FPropertyEmitter Emitter{Symbols, FunctionStatics, Parms};
				uint32_t FunctionFlags = Function.FunctionFlags;
				std::string ParmsMembers;
				for (const FPropertyDef& Param : Function.Params)
				{
					uint64_t Flags = Param.Flags;
					if (IsPassedByReference(Param))
					{
						Flags |= EPropertyFlagBits::ConstParm | EPropertyFlagBits::OutParm |
							EPropertyFlagBits::ReferenceParm;
						FunctionFlags |= EFunctionFlagBits::HasOutParms;
					}
					Tables.Add(Emitter, Param, Flags);
					ParmsMembers += "\t\t\t" + Param.Type.GetCppType() + " " + Param.Name + ";\n";
				}
				if (Function.bHasReturnValue)
				{
					Tables.Add(Emitter, Function.ReturnValue, Function.ReturnValue.Flags);
					ParmsMembers += "\t\t\t" + Function.ReturnValue.Type.GetCppType() + " ReturnValue;\n";
				}
				const bool bHasParms = !Tables.IsEmpty();
				Body += "\tstruct " + FunctionStatics + "\n\t{\n";
				if (bHasParms)
				{
					Body += "\t\tstruct " + Parms + "\n\t\t{\n" + ParmsMembers + "\t\t};\n";
				}
				Body += Tables.Declarations();
				Body += "\t\tstatic const UE4CodeGen_Private::FFunctionParams FuncParams;\n\t};\n";
				Body += Tables.Definitions();
				Body += "\tconst UE4CodeGen_Private::FFunctionParams " + FunctionStatics +
					"::FuncParams = { (UObject*(*)())" + Construct + ", nullptr, \"" + Function.Name +
					"\", nullptr, nullptr, ";
				if (bHasParms)
				{
					Body += "sizeof(" + FunctionStatics + "::" + Parms + "), " + FunctionStatics + "::PropPointers, " +
						"UE_ARRAY_COUNT(" + FunctionStatics + "::PropPointers), ";
				}
				else
				{
					Body += "0, nullptr, 0, ";
				}
				Body += std::string(ObjectFlags) + ", (EFunctionFlags)" + FormatHex32(FunctionFlags) + ", 0, 0 };\n";
				Body +=
					"\tUFunction* " + FunctionConstruct + "()\n\t{\n\t\tstatic UFunction* ReturnFunction = nullptr;\n";
				Body +=
					"\t\tif (!ReturnFunction)\n\t\t{\n\t\t\tUE4CodeGen_Private::ConstructUFunction(ReturnFunction, " +
					FunctionStatics + "::FuncParams);\n\t\t}\n\t\treturn ReturnFunction;\n\t}\n";
			}

			// The class itself.
			Body += "\tUClass* " + Symbols.ClassNoRegister(Name, Module) + "()\n\t{\n\t\treturn " + Name +
				"::StaticClass();\n\t}\n";
			FPropertyTables Tables;
			Tables.Statics = Statics;
			const FPropertyEmitter Emitter{Symbols, Statics, Name};
			for (const FPropertyDef& Property : Class.Properties)
			{
				Tables.Add(Emitter, Property, Property.Flags);
			}
			std::vector<std::string> Singletons;
			if (!Class.SuperName.empty())
			{
				Singletons.push_back(Symbols.Class(Class.SuperName, Class.SuperModule));
			}
			Symbols.References.Add("UPackage* " + PackageFunction + "();");
			Singletons.push_back(PackageFunction);

			Body += "\tstruct " + Statics + "\n\t{\n\t\tstatic UObject* (*const DependentSingletons[])();\n";
			if (!Class.Functions.empty())
			{
				Body += "\t\tstatic const FClassFunctionLinkInfo FuncInfo[];\n";
			}
			Body += Tables.Declarations();
			Body += "\t\tstatic const FCppClassTypeInfoStatic StaticCppClassTypeInfo;\n";
			Body += "\t\tstatic const UE4CodeGen_Private::FClassParams ClassParams;\n\t};\n";
			Body += "\tUObject* (*const " + Statics + "::DependentSingletons[])() = {\n";
			for (const std::string& Singleton : Singletons)
			{
				Body += "\t\t(UObject* (*)())" + Singleton + ",\n";
			}
			Body += "\t};\n";
			if (!Class.Functions.empty())
			{
				Body += "\tconst FClassFunctionLinkInfo " + Statics + "::FuncInfo[] = {\n";
				for (const FFunctionDef& Function : Class.Functions)
				{
					Body += "\t\t{ &Z_Construct_UFunction_" + Name + "_" + Function.Name + ", \"" + Function.Name +
						"\" },\n";
				}
				Body += "\t};\n";
			}
			Body += Tables.Definitions();
			Body += "\tconst FCppClassTypeInfoStatic " + Statics +
				"::StaticCppClassTypeInfo = {\n\t\tTCppClassTypeTraits<" + Name + ">::IsAbstract,\n\t};\n";
			Body += "\tconst UE4CodeGen_Private::FClassParams " + Statics + "::ClassParams = {\n";
			Body += "\t\t&" + Name + "::StaticClass,\n";
			Body +=
				"\t\t" + (Class.ConfigName.empty() ? std::string("nullptr") : "\"" + Class.ConfigName + "\"") + ",\n";
			Body += "\t\t&StaticCppClassTypeInfo,\n\t\tDependentSingletons,\n";
			Body += Class.Functions.empty() ? "\t\tnullptr,\n" : "\t\tFuncInfo,\n";
			// FClassParams puts the property count after the dependency and function counts: split the pair.
			const std::string Pointers = Tables.IsEmpty() ? "\t\tnullptr,\n"
				: Tables.IsEditorOnly()
				? "#if WITH_EDITORONLY_DATA\n\t\t" + Statics + "::PropPointers,\n#else\n\t\tnullptr,\n#endif\n"
				: "\t\t" + Statics + "::PropPointers,\n";
			const std::string Count = Tables.IsEmpty() ? "\t\t0,\n"
				: Tables.IsEditorOnly()                ? "#if WITH_EDITORONLY_DATA\n\t\tUE_ARRAY_COUNT(" + Statics +
					"::PropPointers),\n#else\n\t\t0,\n#endif\n"
										: "\t\tUE_ARRAY_COUNT(" + Statics + "::PropPointers),\n";
			Body += Pointers;
			Body += "\t\tnullptr,\n\t\tUE_ARRAY_COUNT(DependentSingletons),\n";
			Body += Class.Functions.empty() ? "\t\t0,\n" : "\t\tUE_ARRAY_COUNT(FuncInfo),\n";
			Body += Count;
			Body += "\t\t0,\n";
			Body += "\t\t" +
				FormatHex32(Class.ClassFlags | EClassFlagBits::Native | EClassFlagBits::MatchedSerializers) + "u,\n";
			Body += "\t};\n";
			Body += "\tUClass* " + Construct +
				"()\n\t{\n\t\tstatic UClass* OuterClass = nullptr;\n\t\tif (!OuterClass)\n\t\t{\n";
			Body += "\t\t\tUE4CodeGen_Private::ConstructUClass(OuterClass, " + Statics +
				"::ClassParams);\n\t\t}\n\t\treturn OuterClass;\n\t}\n";
			Body += "\tIMPLEMENT_CLASS(" + Name + ", 0);\n";
			Body += "\ttemplate<> " + Manifest.Api + " UClass* StaticClass<" + Name + ">()\n\t{\n\t\treturn " + Name +
				"::StaticClass();\n\t}\n";
			if (!Class.bHasVTableHelperConstructor)
			{
				Body += "\tDEFINE_VTABLE_PTR_HELPER_CTOR(" + Name + ");\n";
			}
		}
	}
	References.Add("UPackage* " + PackageFunction + "();");

	std::string Text = Banner;
	Text += "#include \"UObject/GeneratedCppIncludes.h\"\n#include \"" + File.IncludePath + "\"\n";
	Text += "PRAGMA_DISABLE_DEPRECATION_WARNINGS\n";
	Text += References.Write();
	Text += Body;
	Text += "PRAGMA_ENABLE_DEPRECATION_WARNINGS\n";
	return Text;
}

std::string FCodeGenerator::GenerateInit(const std::vector<FUnrealSourceFile>& Files) const
{
	const std::string PackageFunction = GetPackageFunction();
	std::string Includes;
	std::string Declarations;
	std::string ClassInfo;
	std::string StructInfo;
	std::string EnumInfo;
	for (const FUnrealSourceFile& File : Files)
	{
		if (File.Order.empty())
		{
			continue;
		}
		Includes += "#include \"" + File.IncludePath + "\"\n";
		for (const std::pair<ETypeKind, int>& Entry : File.Order)
		{
			if (Entry.first == ETypeKind::Class)
			{
				const std::string& Name = File.Classes[Entry.second].Name;
				Declarations += "\t" + Manifest.Api + " UClass* Z_Construct_UClass_" + Name + "();\n";
				ClassInfo += "\t\t\t{ Z_Construct_UClass_" + Name + ", " + Name + "::StaticClass, TEXT(\"" + Name +
					"\"), sizeof(" + Name + ") },\n";
			}
			else if (Entry.first == ETypeKind::Struct)
			{
				const std::string& Name = File.Structs[Entry.second].Name;
				Declarations += "\t" + Manifest.Api + " UScriptStruct* Z_Construct_UScriptStruct_" + Name + "();\n";
				StructInfo += "\t\t\t{ Z_Construct_UScriptStruct_" + Name + ", TEXT(\"" + StripPrefix(Name) +
					"\"), sizeof(" + Name + ") },\n";
			}
			else
			{
				const std::string& Name = File.Enums[Entry.second].Name;
				const std::string Construct = "Z_Construct_UEnum_" + Manifest.Module + "_" + Name;
				Declarations += "\t" + Manifest.Api + " UEnum* " + Construct + "();\n";
				EnumInfo += "\t\t\t{ " + Construct + ", TEXT(\"" + Name + "\") },\n";
			}
		}
	}

	std::string Text = Banner;
	Text += "#include \"UObject/GeneratedCppIncludes.h\"\n" + Includes;
	Text += "PRAGMA_DISABLE_DEPRECATION_WARNINGS\n";
	Text += "// Cross Module References\n" + Declarations;
	if (!Manifest.bDefinePackage)
	{
		Text += "\tUPackage* " + PackageFunction + "();\n";
	}
	Text += "// End Cross Module References\n";
	if (Manifest.bDefinePackage)
	{
		Text += "\tUPackage* " + PackageFunction + "()\n\t{\n\t\tstatic UPackage* ReturnPackage = nullptr;\n";
		Text += "\t\tif (!ReturnPackage)\n\t\t{\n";
		Text += "\t\t\tstatic const UE4CodeGen_Private::FPackageParams PackageParams = {\n";
		Text += "\t\t\t\t\"" + Manifest.Package +
			"\",\n\t\t\t\tnullptr,\n\t\t\t\t0,\n\t\t\t\tPKG_CompiledIn | 0x00000000,\n";
		Text += "\t\t\t\t0x00000000,\n\t\t\t\t0x00000000,\n\t\t\t};\n";
		Text += "\t\t\tUE4CodeGen_Private::ConstructUPackage(ReturnPackage, PackageParams);\n\t\t}\n";
		Text += "\t\treturn ReturnPackage;\n\t}\n";
	}
	Text += "\tvoid " + Manifest.RegisterFunction + "()\n\t{\n";
	const auto Table = [&Text](const char* Type, const char* Name, const std::string& Entries)
	{
		if (!Entries.empty())
		{
			Text += std::string("\t\tstatic const ") + Type + " " + Name + "[] = {\n" + Entries + "\t\t};\n";
		}
	};
	Table("FClassRegisterCompiledInInfo", "ClassInfo", ClassInfo);
	Table("FStructRegisterCompiledInInfo", "StructInfo", StructInfo);
	Table("FEnumRegisterCompiledInInfo", "EnumInfo", EnumInfo);
	const auto Arguments = [](const char* Name, const std::string& Entries)
	{
		return Entries.empty() ? std::string("nullptr, 0")
							   : std::string(Name) + ", UE_ARRAY_COUNT(" + std::string(Name) + ")";
	};
	Text += "\t\tRegisterCompiledInInfo(TEXT(\"" + Manifest.Package + "\"), " + Arguments("ClassInfo", ClassInfo) +
		", " + Arguments("StructInfo", StructInfo) + ", " + Arguments("EnumInfo", EnumInfo) + ");\n";
	Text += "\t}\n";
	Text += "PRAGMA_ENABLE_DEPRECATION_WARNINGS\n";
	return Text;
}
