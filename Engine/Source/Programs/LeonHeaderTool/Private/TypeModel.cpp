#include "TypeModel.h"

#include "Diagnostics.h"
#include "Manifest.h"

#include <cstdio>
#include <sstream>

namespace
{
	const char* const IntrinsicLocation = "<intrinsic>";

	/** Classes CoreUObject defines by hand (UE: IMPLEMENT_CORE_INTRINSIC_CLASS and the NoExportTypes). */
	const char* const IntrinsicClasses[] = {
		"UObject", "UField", "UStruct", "UClass", "UScriptStruct", "UFunction", "UEnum", "UPackage", "UInterface"};

	std::string Unrecognized(const std::string& Name)
	{
		return "Unrecognized type '" + Name + "' - type must be a UCLASS, USTRUCT or UENUM";
	}

	const char* KindName(ETypeKind Kind)
	{
		switch (Kind)
		{
			case ETypeKind::Class:
				return "UCLASS";
			case ETypeKind::Struct:
				return "USTRUCT";
			case ETypeKind::Enum:
				return "UENUM";
		}
		return "type";
	}

	struct FResolveContext
	{
		const FTypeTable& Table;
		FDiagnostics& Diagnostics;
		const std::string& File;
		int Line;
	};

	void ResolveType(FPropertyType& Type, const FResolveContext& Context)
	{
		const std::string& File = Context.File;
		switch (Type.Kind)
		{
			case EPropertyKind::Unresolved:
			{
				const FTypeInfo* Info = Context.Table.Find(Type.TypeName);
				if (!Info)
				{
					Context.Diagnostics.Error(File, Context.Line, Unrecognized(Type.TypeName));
					return;
				}
				Type.TypeModule = Info->Module;
				if (Info->Kind == ETypeKind::Class)
				{
					if (!Type.bPointer)
					{
						Context.Diagnostics.Error(File, Context.Line,
							"UCLASS '" + Type.TypeName + "' can only be referenced through a pointer ('" +
								Type.TypeName + "*')");
						return;
					}
					Type.Kind = EPropertyKind::Object;
					return;
				}
				if (Type.bPointer)
				{
					Context.Diagnostics.Error(File, Context.Line,
						"Inappropriate '*' on variable of type '" + Type.TypeName +
							"', cannot have an exposed pointer to this type.");
					return;
				}
				if (Info->Kind == ETypeKind::Struct)
				{
					Type.Kind = EPropertyKind::Struct;
					return;
				}
				if (!Info->bEnumClass || Info->EnumUnderlying.empty())
				{
					Context.Diagnostics.Error(File, Context.Line,
						"You cannot use the raw enum name as a type for member variables, instead use TEnumAsByte or a "
						"C++11 enum class with an explicit underlying type.");
					return;
				}
				Type.Kind = EPropertyKind::Enum;
				Type.EnumUnderlying = Info->EnumUnderlying;
				return;
			}
			case EPropertyKind::Object:
			case EPropertyKind::Class:
			case EPropertyKind::SoftObject:
			case EPropertyKind::SoftClass:
			case EPropertyKind::WeakObject:
			{
				const FTypeInfo* Info = Context.Table.Find(Type.TypeName);
				if (!Info)
				{
					Context.Diagnostics.Error(File, Context.Line, Unrecognized(Type.TypeName));
					return;
				}
				if (Info->Kind != ETypeKind::Class)
				{
					Context.Diagnostics.Error(File, Context.Line,
						"'" + Type.TypeName + "' is a " + KindName(Info->Kind) + "; " + Type.GetCppType() +
							" needs a UCLASS");
					return;
				}
				Type.TypeModule = Info->Module;
				return;
			}
			case EPropertyKind::Byte:
			{
				if (!Type.bEnumAsByte)
				{
					return;
				}
				const FTypeInfo* Info = Context.Table.Find(Type.TypeName);
				if (!Info)
				{
					Context.Diagnostics.Error(File, Context.Line, Unrecognized(Type.TypeName));
					return;
				}
				if (Info->Kind != ETypeKind::Enum)
				{
					Context.Diagnostics.Error(File, Context.Line,
						"'" + Type.TypeName + "' is a " + KindName(Info->Kind) + "; TEnumAsByte needs a UENUM");
					return;
				}
				Type.TypeModule = Info->Module;
				return;
			}
			case EPropertyKind::Array:
			case EPropertyKind::Map:
			case EPropertyKind::Set:
				for (FPropertyType& Inner : Type.Inner)
				{
					ResolveType(Inner, Context);
				}
				return;
			default:
				return;
		}
	}

	void ResolveProperty(
		FPropertyDef& Property, const FTypeTable& Table, FDiagnostics& Diagnostics, const std::string& File)
	{
		ResolveType(Property.Type, FResolveContext{Table, Diagnostics, File, Property.Line});
	}
} // namespace

std::string FPropertyType::GetCppType() const
{
	switch (Kind)
	{
		case EPropertyKind::Unresolved:
			return TypeName + (bPointer ? "*" : "");
		case EPropertyKind::Bool:
			return "bool";
		case EPropertyKind::Int8:
			return "int8";
		case EPropertyKind::Int16:
			return "int16";
		case EPropertyKind::Int:
			return "int32";
		case EPropertyKind::Int64:
			return "int64";
		case EPropertyKind::Byte:
			return bEnumAsByte ? "TEnumAsByte<" + TypeName + ">" : "uint8";
		case EPropertyKind::UInt16:
			return "uint16";
		case EPropertyKind::UInt32:
			return "uint32";
		case EPropertyKind::UInt64:
			return "uint64";
		case EPropertyKind::UnsizedInt:
			return "int";
		case EPropertyKind::UnsizedUInt:
			return "unsigned int";
		case EPropertyKind::Float:
			return "float";
		case EPropertyKind::Double:
			return "double";
		case EPropertyKind::Str:
			return "FString";
		case EPropertyKind::Name:
			return "FName";
		case EPropertyKind::Text:
			return "FText";
		case EPropertyKind::Enum:
		case EPropertyKind::Struct:
			return TypeName;
		case EPropertyKind::Object:
			return TypeName + "*";
		case EPropertyKind::Class:
			return "TSubclassOf<" + TypeName + ">";
		case EPropertyKind::SoftObject:
			return "TSoftObjectPtr<" + TypeName + ">";
		case EPropertyKind::SoftClass:
			return "TSoftClassPtr<" + TypeName + ">";
		case EPropertyKind::WeakObject:
			return "TWeakObjectPtr<" + TypeName + ">";
		case EPropertyKind::Array:
			return "TArray<" + Inner[0].GetCppType() + ">";
		case EPropertyKind::Map:
			return "TMap<" + Inner[0].GetCppType() + "," + Inner[1].GetCppType() + ">";
		case EPropertyKind::Set:
			return "TSet<" + Inner[0].GetCppType() + ">";
	}
	return TypeName;
}

FTypeTable::FTypeTable()
{
	for (const char* Name : IntrinsicClasses)
	{
		FTypeInfo Info;
		Info.Kind = ETypeKind::Class;
		Info.Module = "CoreUObject";
		Info.Location = IntrinsicLocation;
		Types[Name] = Info;
	}
}

bool FTypeTable::LoadIndex(const std::string& Path, FDiagnostics& Diagnostics)
{
	std::string Content;
	if (!ReadTextFile(Path, Content))
	{
		Diagnostics.Error(Path, 0, "Cannot read the type index of a dependency (was its LeonHeaderTool step run?)");
		return false;
	}
	std::string Module;
	std::istringstream Stream(Content);
	std::string Line;
	int LineNumber = 0;
	while (std::getline(Stream, Line))
	{
		++LineNumber;
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.pop_back();
		}
		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}
		const size_t Equals = Line.find('=');
		if (Equals == std::string::npos)
		{
			Diagnostics.Error(Path, LineNumber, "Expected <Key>=<Value>");
			return false;
		}
		const std::string Key = Line.substr(0, Equals);
		const std::vector<std::string> Fields = SplitFields(Line.substr(Equals + 1));
		if (Key == "Module")
		{
			Module = Fields[0];
			continue;
		}
		FTypeInfo Info;
		Info.Module = Module;
		Info.Location = "module " + Module;
		if (Key == "Class")
		{
			Info.Kind = ETypeKind::Class;
		}
		else if (Key == "Struct")
		{
			Info.Kind = ETypeKind::Struct;
		}
		else if (Key == "Enum")
		{
			Info.Kind = ETypeKind::Enum;
			Info.bEnumClass = Fields.size() > 1 && Fields[1] == "EnumClass";
			Info.EnumUnderlying = Fields.size() > 2 ? Fields[2] : "";
		}
		else
		{
			Diagnostics.Error(Path, LineNumber, "Unknown key '" + Key + "'");
			return false;
		}
		if (Module.empty())
		{
			Diagnostics.Error(Path, LineNumber, "Module= must come first");
			return false;
		}
		Types[Fields[0]] = Info;
	}
	return true;
}

void FTypeTable::AddLocalTypes(
	const std::string& Module, std::vector<FUnrealSourceFile>& Files, FDiagnostics& Diagnostics)
{
	for (const FUnrealSourceFile& File : Files)
	{
		for (const std::pair<ETypeKind, int>& Entry : File.Order)
		{
			std::string Name;
			int Line = 0;
			FTypeInfo Info;
			Info.Kind = Entry.first;
			Info.Module = Module;
			switch (Entry.first)
			{
				case ETypeKind::Class:
					Name = File.Classes[Entry.second].Name;
					Line = File.Classes[Entry.second].DeclarationLine;
					break;
				case ETypeKind::Struct:
					Name = File.Structs[Entry.second].Name;
					Line = File.Structs[Entry.second].DeclarationLine;
					break;
				case ETypeKind::Enum:
				{
					const FEnumDef& Enum = File.Enums[Entry.second];
					Name = Enum.Name;
					Line = Enum.DeclarationLine;
					Info.bEnumClass = Enum.bEnumClass;
					Info.EnumUnderlying = Enum.Underlying;
					break;
				}
			}
			Info.Location = File.DisplayPath + "(" + std::to_string(Line) + ")";
			const auto Existing = Types.find(Name);
			if (Existing != Types.end() && Existing->second.Location != IntrinsicLocation)
			{
				Diagnostics.Error(
					File.DisplayPath, Line, "Type '" + Name + "' is already defined in " + Existing->second.Location);
				continue;
			}
			Types[Name] = Info;
		}
	}
}

const FTypeInfo* FTypeTable::Find(const std::string& Name) const
{
	const auto Found = Types.find(Name);
	return Found != Types.end() ? &Found->second : nullptr;
}

void FTypeTable::Resolve(std::vector<FUnrealSourceFile>& Files, FDiagnostics& Diagnostics) const
{
	for (FUnrealSourceFile& File : Files)
	{
		const std::string& Path = File.DisplayPath;
		for (FClassDef& Class : File.Classes)
		{
			if (Class.SuperName.empty())
			{
				if (Class.Name != "UObject")
				{
					Diagnostics.Error(Path, Class.DeclarationLine,
						"Class '" + Class.Name + "' must derive from UObject or another UCLASS");
				}
			}
			else
			{
				const FTypeInfo* Super = Find(Class.SuperName);
				if (!Super || Super->Kind != ETypeKind::Class)
				{
					Diagnostics.Error(Path, Class.DeclarationLine,
						"Couldn't find parent type for '" + Class.Name + "' named '" + Class.SuperName +
							"' in current module or any module it depends on.");
				}
				else
				{
					Class.SuperModule = Super->Module;
				}
			}
			for (FPropertyDef& Property : Class.Properties)
			{
				ResolveProperty(Property, *this, Diagnostics, Path);
			}
			for (FFunctionDef& Function : Class.Functions)
			{
				for (FPropertyDef& Param : Function.Params)
				{
					ResolveProperty(Param, *this, Diagnostics, Path);
				}
				if (Function.bHasReturnValue)
				{
					ResolveProperty(Function.ReturnValue, *this, Diagnostics, Path);
				}
			}
		}
		for (FStructDef& Struct : File.Structs)
		{
			if (!Struct.SuperName.empty())
			{
				const FTypeInfo* Super = Find(Struct.SuperName);
				if (!Super || Super->Kind != ETypeKind::Struct)
				{
					Diagnostics.Error(Path, Struct.DeclarationLine,
						"Couldn't find parent struct '" + Struct.SuperName + "' of '" + Struct.Name +
							"'; the base of a USTRUCT must be a USTRUCT");
				}
				else
				{
					Struct.SuperModule = Super->Module;
				}
			}
			for (FPropertyDef& Property : Struct.Properties)
			{
				ResolveProperty(Property, *this, Diagnostics, Path);
			}
		}
	}
}

std::string FTypeTable::WriteIndex(const std::string& Module, const std::vector<FUnrealSourceFile>& Files)
{
	std::string Text = "# LeonHeaderTool type index: the reflected types of module " + Module +
		" (generated; do not edit).\nModule=" + Module + "\n";
	for (const FUnrealSourceFile& File : Files)
	{
		for (const std::pair<ETypeKind, int>& Entry : File.Order)
		{
			switch (Entry.first)
			{
				case ETypeKind::Class:
					Text += "Class=" + File.Classes[Entry.second].Name + "\n";
					break;
				case ETypeKind::Struct:
					Text += "Struct=" + File.Structs[Entry.second].Name + "\n";
					break;
				case ETypeKind::Enum:
				{
					const FEnumDef& Enum = File.Enums[Entry.second];
					Text += "Enum=" + Enum.Name + "|" + (Enum.bEnumClass ? "EnumClass" : "Regular") + "|" +
						Enum.Underlying + "\n";
					break;
				}
			}
		}
	}
	return Text;
}

std::string FormatHex64(uint64_t Value)
{
	char Buffer[32];
	std::snprintf(Buffer, sizeof(Buffer), "0x%016llx", static_cast<unsigned long long>(Value));
	return Buffer;
}

std::string FormatHex32(uint32_t Value)
{
	char Buffer[16];
	std::snprintf(Buffer, sizeof(Buffer), "0x%08X", static_cast<unsigned>(Value));
	return Buffer;
}
