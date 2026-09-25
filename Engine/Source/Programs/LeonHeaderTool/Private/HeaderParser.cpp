#include "HeaderParser.h"

#include "Diagnostics.h"

#include <algorithm>
#include <cctype>

namespace
{
	std::string ToLower(std::string Text)
	{
		for (char& C : Text)
		{
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		}
		return Text;
	}

	bool IsOneOf(const std::string& Value, std::initializer_list<const char*> Names)
	{
		for (const char* Name : Names)
		{
			if (Value == Name)
			{
				return true;
			}
		}
		return false;
	}

	bool IsTypeMacro(const FToken& Token)
	{
		return Token.Type == ETokenType::Identifier &&
			IsOneOf(Token.Text, {"UCLASS", "USTRUCT", "UENUM", "UINTERFACE", "UDELEGATE"});
	}

	bool IsBodyMacro(const FToken& Token)
	{
		return Token.Type == ETokenType::Identifier &&
			IsOneOf(Token.Text,
				{"UPROPERTY", "UFUNCTION", "GENERATED_BODY", "GENERATED_UCLASS_BODY", "GENERATED_USTRUCT_BODY",
					"GENERATED_UINTERFACE_BODY", "GENERATED_IINTERFACE_BODY"});
	}

	bool IsAccessSpecifier(const FToken& Token)
	{
		return Token.Type == ETokenType::Identifier && IsOneOf(Token.Text, {"public", "protected", "private"});
	}

	const FSpecifier* FindSpecifier(const std::vector<FSpecifier>& Specifiers, const char* LowerName)
	{
		for (const FSpecifier& Specifier : Specifiers)
		{
			if (ToLower(Specifier.Name) == LowerName)
			{
				return &Specifier;
			}
		}
		return nullptr;
	}

	/** Template names LeonHeaderTool maps to properties, with their argument count. */
	int GetTemplateArity(const std::string& Name)
	{
		if (IsOneOf(Name,
				{"TArray", "TSet", "TSubclassOf", "TSoftObjectPtr", "TSoftClassPtr", "TWeakObjectPtr", "TEnumAsByte"}))
		{
			return 1;
		}
		return Name == "TMap" ? 2 : 0;
	}

	bool FindBuiltinKind(const std::string& Name, EPropertyKind& OutKind)
	{
		static const std::pair<const char*, EPropertyKind> Builtins[] = {{"bool", EPropertyKind::Bool},
			{"int8", EPropertyKind::Int8}, {"int16", EPropertyKind::Int16}, {"int32", EPropertyKind::Int},
			{"int64", EPropertyKind::Int64}, {"uint8", EPropertyKind::Byte}, {"uint16", EPropertyKind::UInt16},
			{"uint32", EPropertyKind::UInt32}, {"uint64", EPropertyKind::UInt64}, {"int", EPropertyKind::UnsizedInt},
			{"float", EPropertyKind::Float}, {"double", EPropertyKind::Double}, {"FString", EPropertyKind::Str},
			{"FName", EPropertyKind::Name}, {"FText", EPropertyKind::Text}};
		for (const auto& Builtin : Builtins)
		{
			if (Name == Builtin.first)
			{
				OutKind = Builtin.second;
				return true;
			}
		}
		return false;
	}

	/** Joins token texts the way they are written in source ("EFoo::Max", "(A=1, B=\"x\")"). */
	std::string JoinTokens(const std::vector<FToken>& Tokens, size_t Begin, size_t End)
	{
		std::string Text;
		for (size_t Index = Begin; Index < End; ++Index)
		{
			const FToken& Token = Tokens[Index];
			const bool bWord = Token.Type != ETokenType::Symbol;
			if (!Text.empty() && bWord && Index > Begin && Tokens[Index - 1].Type != ETokenType::Symbol)
			{
				Text += ' ';
			}
			if (Token.Type == ETokenType::Symbol && Token.Text == ",")
			{
				Text += ", ";
				continue;
			}
			Text += Token.Text;
		}
		return Text;
	}

	std::string Unquote(const std::string& Text)
	{
		if (Text.size() >= 2 && Text.front() == '"' && Text.back() == '"')
		{
			return Text.substr(1, Text.size() - 2);
		}
		return Text;
	}
} // namespace

FHeaderParser::FHeaderParser(FUnrealSourceFile& InFile, FDiagnostics& InDiagnostics)
	: File(InFile)
	, Diagnostics(InDiagnostics)
{
}

void FHeaderParser::Parse(const std::string& Source)
{
	std::vector<FToken> RawTokens;
	std::string TokenError;
	int TokenErrorLine = 0;
	if (!FTokenizer::Tokenize(Source, RawTokens, TokenError, TokenErrorLine))
	{
		Diagnostics.Error(File.DisplayPath, TokenErrorLine, TokenError);
		return;
	}
	try
	{
		Preprocess(RawTokens);
		ParseFile();
		if (FirstTypeLine > 0 && GeneratedIncludeLine == 0)
		{
			Fail(FirstTypeLine,
				"Expected an include at the top of the header: '#include \"" + File.BaseName + ".generated.h\"'");
		}
		if (FirstTypeLine > 0 && GeneratedIncludeLine > FirstTypeLine)
		{
			Fail(GeneratedIncludeLine,
				"'" + File.BaseName + ".generated.h' must be included before the first UCLASS, USTRUCT or UENUM");
		}
	}
	catch (const FParseAbort&)
	{
	}
}

int FHeaderParser::FindGeneratedInclude(const std::string& Source)
{
	std::vector<FToken> RawTokens;
	std::string Error;
	int ErrorLine = 0;
	FTokenizer::Tokenize(Source, RawTokens, Error, ErrorLine);
	for (const FToken& Token : RawTokens)
	{
		if (Token.Type == ETokenType::Directive && Token.Text.compare(0, 7, "include") == 0 &&
			Token.Text.find(".generated.h") != std::string::npos)
		{
			return Token.Line;
		}
	}
	return 0;
}

void FHeaderParser::Fail(int Line, const std::string& Message)
{
	Diagnostics.Error(File.DisplayPath, Line, Message);
	throw FParseAbort();
}

void FHeaderParser::Warn(int Line, const std::string& Message)
{
	Diagnostics.Warning(File.DisplayPath, Line, Message);
}

void FHeaderParser::Preprocess(const std::vector<FToken>& RawTokens)
{
	enum class EConditional
	{
		EditorOnlyData,
		Other,
		/** #if CPP / #if 0 branch: skipped, as UHT does (CPP is 0 while parsing). */
		Skip,
		/** #if !CPP / #if 1 branch: parsed. */
		Active,
		/** Any #if nested inside a skipped branch. */
		Nested
	};
	struct FEntry
	{
		EConditional Kind;
		int Line;
		/** The branch is the '#if !CPP' one (NoExport declarations). */
		bool bNotCpp = false;
	};
	std::vector<FEntry> Stack;
	const auto IsSkipping = [&Stack]()
	{
		return std::any_of(
			Stack.begin(), Stack.end(), [](const FEntry& Entry) { return Entry.Kind == EConditional::Skip; });
	};

	Tokens.clear();
	for (const FToken& Raw : RawTokens)
	{
		if (Raw.Type != ETokenType::Directive)
		{
			if (Raw.Type != ETokenType::EndOfFile && IsSkipping())
			{
				continue;
			}
			FToken Token = Raw;
			for (const FEntry& Entry : Stack)
			{
				Token.bEditorOnlyData |= Entry.Kind == EConditional::EditorOnlyData;
				Token.bInOtherConditional |= Entry.Kind == EConditional::Other;
				Token.bInNotCppBlock |= Entry.Kind == EConditional::Active && Entry.bNotCpp;
			}
			Tokens.push_back(Token);
			continue;
		}

		const size_t Space = Raw.Text.find(' ');
		const std::string Keyword = Raw.Text.substr(0, Space);
		const std::string Argument = Space == std::string::npos ? std::string() : Raw.Text.substr(Space + 1);
		if (Keyword == "if" || Keyword == "ifdef" || Keyword == "ifndef")
		{
			EConditional Kind = EConditional::Other;
			if (IsSkipping())
			{
				Kind = EConditional::Nested;
			}
			else if (Keyword == "if" && Argument == "WITH_EDITORONLY_DATA")
			{
				Kind = EConditional::EditorOnlyData;
			}
			else if (Keyword == "if" && (Argument == "CPP" || Argument == "0"))
			{
				Kind = EConditional::Skip;
			}
			else if (Keyword == "if" && (Argument == "!CPP" || Argument == "1"))
			{
				Kind = EConditional::Active;
			}
			Stack.push_back({Kind, Raw.Line, Keyword == "if" && Argument == "!CPP"});
		}
		else if (Keyword == "else" || Keyword == "elif")
		{
			if (Stack.empty())
			{
				Fail(Raw.Line, "#" + Keyword + " without #if");
			}
			EConditional& Kind = Stack.back().Kind;
			if (Kind == EConditional::Skip)
			{
				Kind = Keyword == "else" ? EConditional::Active : EConditional::Other;
				Stack.back().bNotCpp = false;
			}
			else if (Kind == EConditional::Active)
			{
				Kind = EConditional::Skip;
			}
			else if (Kind == EConditional::EditorOnlyData)
			{
				Kind = EConditional::Other;
			}
		}
		else if (Keyword == "endif")
		{
			if (Stack.empty())
			{
				Fail(Raw.Line, "#endif without #if");
			}
			Stack.pop_back();
		}
		else if (Keyword == "include" && !IsSkipping())
		{
			std::string Name = Argument;
			if (Name.size() >= 2 && (Name.front() == '"' || Name.front() == '<'))
			{
				Name = Name.substr(1, Name.size() - 2);
			}
			const std::string Suffix = ".generated.h";
			const bool bGenerated =
				Name.size() > Suffix.size() && Name.compare(Name.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
			if (GeneratedIncludeLine > 0)
			{
				Fail(Raw.Line,
					"#include found after .generated.h file - the .generated.h file should always be the "
					"last #include in a header");
			}
			if (bGenerated)
			{
				const size_t Slash = Name.find_last_of('/');
				const std::string FileName = Slash == std::string::npos ? Name : Name.substr(Slash + 1);
				if (FileName != File.BaseName + Suffix)
				{
					Fail(Raw.Line, "Expected '#include \"" + File.BaseName + Suffix + "\"', found '" + Name + "'");
				}
				GeneratedIncludeLine = Raw.Line;
			}
		}
	}
	if (!Stack.empty())
	{
		Fail(Stack.back().Line, "#if without #endif");
	}
}

const FToken& FHeaderParser::Peek(size_t Ahead) const
{
	const size_t Index = std::min(Pos + Ahead, Tokens.size() - 1);
	return Tokens[Index];
}

const FToken& FHeaderParser::Next()
{
	const FToken& Token = Peek();
	if (Pos < Tokens.size() - 1)
	{
		++Pos;
	}
	return Token;
}

bool FHeaderParser::MatchSymbol(const char* Symbol)
{
	if (Peek().IsSymbol(Symbol))
	{
		Next();
		return true;
	}
	return false;
}

bool FHeaderParser::MatchIdentifier(const char* Name)
{
	if (Peek().IsIdentifier(Name))
	{
		Next();
		return true;
	}
	return false;
}

void FHeaderParser::ParseFile()
{
	Pos = 0;
	int BraceDepth = 0;
	while (Peek().Type != ETokenType::EndOfFile)
	{
		const FToken Token = Next();
		if (Token.Type == ETokenType::Symbol)
		{
			BraceDepth += Token.Text == "{" ? 1 : (Token.Text == "}" ? -1 : 0);
			continue;
		}
		if (Token.Type != ETokenType::Identifier)
		{
			continue;
		}
		if (IsTypeMacro(Token))
		{
			if (Token.Text == "UINTERFACE" || Token.Text == "UDELEGATE")
			{
				Fail(Token.Line, Token.Text + " is not supported by LeonHeaderTool");
			}
			if (BraceDepth > 0)
			{
				Fail(Token.Line, Token.Text + " inside a namespace or another scope is not supported");
			}
			if (FirstTypeLine == 0)
			{
				FirstTypeLine = Token.Line;
			}
			if (Token.Text == "UCLASS")
			{
				ParseClass(Token);
			}
			else if (Token.Text == "USTRUCT")
			{
				ParseStruct(Token);
			}
			else
			{
				ParseEnum(Token);
			}
		}
		else if (IsBodyMacro(Token) || Token.Text == "UMETA")
		{
			Fail(Token.Line,
				Token.Text + " is only valid inside a UCLASS or USTRUCT" +
					(Token.Text == "UMETA" ? std::string(" (UMETA: inside a UENUM)") : std::string()));
		}
		else if (Token.Text.compare(0, 16, "DECLARE_DYNAMIC_") == 0)
		{
			Fail(Token.Line, "Dynamic delegates (" + Token.Text + ") are not supported by LeonHeaderTool");
		}
	}
}

int FHeaderParser::ParseSpecifiers(const FToken& Macro, std::vector<FSpecifier>& OutSpecifiers)
{
	if (!MatchSymbol("("))
	{
		Fail(Macro.Line, "Expected '(' after " + Macro.Text);
	}
	while (true)
	{
		const FToken Token = Next();
		if (Token.Type == ETokenType::EndOfFile)
		{
			Fail(Macro.Line, "Missing ')' to close " + Macro.Text + "(");
		}
		if (Token.IsSymbol(")"))
		{
			return Token.Line;
		}
		if (Token.Type != ETokenType::Identifier)
		{
			Fail(Token.Line, "Unexpected '" + Token.Text + "' in " + Macro.Text + "(...); expected a specifier name");
		}
		FSpecifier Specifier;
		Specifier.Name = Token.Text;
		Specifier.Line = Token.Line;
		if (MatchSymbol("="))
		{
			const size_t Begin = Pos;
			int Depth = 0;
			while (true)
			{
				const FToken& Value = Peek();
				if (Value.Type == ETokenType::EndOfFile)
				{
					Fail(Macro.Line, "Missing ')' to close " + Macro.Text + "(");
				}
				if (Depth == 0 && (Value.IsSymbol(",") || Value.IsSymbol(")")))
				{
					break;
				}
				Depth += Value.IsSymbol("(") ? 1 : (Value.IsSymbol(")") ? -1 : 0);
				Next();
			}
			if (Pos == Begin)
			{
				Fail(Token.Line, "Missing value after '" + Token.Text + "=' in " + Macro.Text + "(...)");
			}
			Specifier.Value = Pos - Begin == 1 ? Unquote(Tokens[Begin].Text) : JoinTokens(Tokens, Begin, Pos);
		}
		OutSpecifiers.push_back(Specifier);
		if (MatchSymbol(","))
		{
			continue;
		}
		if (Peek().IsSymbol(")") || Peek().Type == ETokenType::EndOfFile)
		{
			continue;
		}
		Fail(Peek().Line, "Unexpected '" + Peek().Text + "' in " + Macro.Text + "(...); expected ',' or ')'");
	}
}

std::string FHeaderParser::ParseBaseList()
{
	std::string First;
	bool bFirst = true;
	while (true)
	{
		while (Peek().IsIdentifier("public") || Peek().IsIdentifier("protected") || Peek().IsIdentifier("private") ||
			Peek().IsIdentifier("virtual"))
		{
			Next();
		}
		std::string Name;
		if (Peek().Type != ETokenType::Identifier)
		{
			Fail(Peek().Line, "Expected a base class name, found '" + Peek().Text + "'");
		}
		Name = Next().Text;
		while (Peek().IsSymbol("::") && Peek(1).Type == ETokenType::Identifier)
		{
			Next();
			Name += "::" + Next().Text;
		}
		if (Peek().IsSymbol("<"))
		{
			int Depth = 0;
			do
			{
				const FToken& Token = Next();
				if (Token.Type == ETokenType::EndOfFile)
				{
					Fail(Token.Line, "Unterminated template argument list in a base class");
				}
				Depth += Token.IsSymbol("<") ? 1 : (Token.IsSymbol(">") ? -1 : 0);
			} while (Depth > 0);
		}
		if (bFirst)
		{
			First = Name;
			bFirst = false;
		}
		if (!MatchSymbol(","))
		{
			return First;
		}
	}
}

void FHeaderParser::ParseClass(const FToken& Macro)
{
	FClassDef Class;
	Class.PrologFirstLine = Macro.Line;
	std::vector<FSpecifier> Specifiers;
	Class.PrologLastLine = ParseSpecifiers(Macro, Specifiers);
	if (!MatchIdentifier("class"))
	{
		Fail(Peek().Line, "Expected 'class' after UCLASS(...)");
	}
	const std::string ApiSuffix = "_API";
	if (Peek().Type == ETokenType::Identifier && Peek(1).Type == ETokenType::Identifier &&
		Peek().Text.size() > ApiSuffix.size() &&
		Peek().Text.compare(Peek().Text.size() - ApiSuffix.size(), ApiSuffix.size(), ApiSuffix) == 0)
	{
		Class.Api = Next().Text;
	}
	if (Peek().Type != ETokenType::Identifier)
	{
		Fail(Peek().Line, "Expected a class name after 'class'");
	}
	const FToken Name = Next();
	Class.Name = Name.Text;
	Class.DeclarationLine = Name.Line;
	if (Class.Name.size() < 2 || (Class.Name[0] != 'U' && Class.Name[0] != 'A'))
	{
		Fail(Name.Line, "Class '" + Class.Name + "' has an invalid Unreal prefix, expecting 'U' or 'A'");
	}
	MatchIdentifier("final");
	if (Peek().IsSymbol(";"))
	{
		Fail(Name.Line, "UCLASS must be followed by a class definition, not a forward declaration");
	}
	if (MatchSymbol(":"))
	{
		Class.SuperName = ParseBaseList();
	}
	if (!MatchSymbol("{"))
	{
		Fail(Peek().Line, "Expected '{' to start the body of class '" + Class.Name + "'");
	}

	Class.ClassFlags = 0;
	for (const FSpecifier& Specifier : Specifiers)
	{
		const std::string Key = ToLower(Specifier.Name);
		if (Key == "abstract")
		{
			Class.ClassFlags |= EClassFlagBits::Abstract;
		}
		else if (Key == "config")
		{
			if (Specifier.Value.empty())
			{
				Fail(Specifier.Line, "UCLASS specifier 'Config' needs a value (Config=Game)");
			}
			Class.ClassFlags |= EClassFlagBits::Config;
			Class.ConfigName = Specifier.Value;
		}
		else if (Key == "defaultconfig")
		{
			Class.ClassFlags |= EClassFlagBits::DefaultConfig;
		}
		else if (Key == "transient")
		{
			Class.ClassFlags |= EClassFlagBits::Transient;
		}
		else if (Key == "notplaceable")
		{
			Class.ClassFlags |= EClassFlagBits::NotPlaceable;
		}
		else if (Key == "minimalapi")
		{
			Class.ClassFlags |= EClassFlagBits::MinimalAPI;
		}
		else if (Key == "editinlinenew")
		{
			Class.ClassFlags |= EClassFlagBits::EditInlineNew;
		}
		else if (IsOneOf(Key,
					 {"interface", "perobjectconfig", "globaluserconfig", "projectuserconfig", "within", "noexport",
						 "intrinsic", "customconstructor", "deprecated"}))
		{
			Fail(Specifier.Line, "Class specifier '" + Specifier.Name + "' is not supported by LeonHeaderTool");
		}
		else if (!IsOneOf(Key,
					 {"blueprinttype", "notblueprinttype", "blueprintable", "notblueprintable", "meta", "classgroup",
						 "hidecategories", "showcategories", "autoexpandcategories", "autocollapsecategories",
						 "dontautocollapsecategories", "collapsecategories", "dontcollapsecategories", "hidefunctions",
						 "showfunctions", "placeable", "hidedropdown", "conversionroot", "componentwrapperclass",
						 "advancedclassdisplay", "notedittimesettable", "defaulttoinstanced"}))
		{
			Warn(Specifier.Line, "Unknown class specifier '" + Specifier.Name + "' (ignored)");
		}
	}
	if (!Class.Api.empty())
	{
		Class.ClassFlags |= EClassFlagBits::RequiredAPI;
	}

	Class.BodyLine = ParseBody(&Class, nullptr, Class.Name, Class.DeclarationLine);
	if (!MatchSymbol(";"))
	{
		Fail(Peek().Line, "Expected ';' after the body of class '" + Class.Name + "'");
	}
	File.Order.emplace_back(ETypeKind::Class, static_cast<int>(File.Classes.size()));
	File.Classes.push_back(std::move(Class));
}

void FHeaderParser::ParseStruct(const FToken& Macro)
{
	FStructDef Struct;
	std::vector<FSpecifier> Specifiers;
	ParseSpecifiers(Macro, Specifiers);
	if (!MatchIdentifier("struct"))
	{
		Fail(Peek().Line, "Expected 'struct' after USTRUCT(...)");
	}
	if (Peek().Type == ETokenType::Identifier && Peek(1).Type == ETokenType::Identifier && Peek().Text.size() > 4 &&
		Peek().Text.compare(Peek().Text.size() - 4, 4, "_API") == 0)
	{
		Struct.Api = Next().Text;
	}
	if (Peek().Type != ETokenType::Identifier)
	{
		Fail(Peek().Line, "Expected a struct name after 'struct'");
	}
	const FToken Name = Next();
	Struct.Name = Name.Text;
	Struct.DeclarationLine = Name.Line;
	if (Struct.Name.size() < 2 || Struct.Name[0] != 'F')
	{
		Fail(Name.Line, "Struct '" + Struct.Name + "' has an invalid Unreal prefix, expecting 'F'");
	}
	MatchIdentifier("final");
	if (Peek().IsSymbol(";"))
	{
		Fail(Name.Line, "USTRUCT must be followed by a struct definition, not a forward declaration");
	}
	if (MatchSymbol(":"))
	{
		Struct.SuperName = ParseBaseList();
	}
	if (!MatchSymbol("{"))
	{
		Fail(Peek().Line, "Expected '{' to start the body of struct '" + Struct.Name + "'");
	}

	Struct.StructFlags = EStructFlagBits::Native;
	for (const FSpecifier& Specifier : Specifiers)
	{
		const std::string Key = ToLower(Specifier.Name);
		if (Key == "atomic")
		{
			Struct.StructFlags |= EStructFlagBits::Atomic;
		}
		else if (Key == "immutable")
		{
			Struct.StructFlags |= EStructFlagBits::Immutable | EStructFlagBits::Atomic;
		}
		else if (Key == "noexport")
		{
			if (!Macro.bInNotCppBlock)
			{
				Fail(Specifier.Line,
					"USTRUCT(NoExport) '" + Struct.Name +
						"' must be declared inside an '#if !CPP' block: the C++ type is defined elsewhere");
			}
			Struct.bNoExport = true;
			Struct.StructFlags |= EStructFlagBits::NoExport;
		}
		else if (!IsOneOf(Key, {"blueprinttype", "notblueprinttype", "meta"}))
		{
			Warn(Specifier.Line, "Unknown struct specifier '" + Specifier.Name + "' (ignored)");
		}
	}
	if (!Struct.Api.empty())
	{
		Struct.StructFlags |= EStructFlagBits::RequiredAPI;
	}

	Struct.BodyLine = ParseBody(nullptr, &Struct, Struct.Name, Struct.DeclarationLine);
	if (!MatchSymbol(";"))
	{
		Fail(Peek().Line, "Expected ';' after the body of struct '" + Struct.Name + "'");
	}
	if (Struct.bNoExport)
	{
		// The generated code checks each member against the real type; these forms cannot be checked.
		for (const FPropertyDef& Property : Struct.Properties)
		{
			if (Property.bBitfield || Property.bFixedArray || Property.bEditorOnly)
			{
				Fail(Property.Line,
					"NoExport struct '" + Struct.Name + "' cannot reflect '" + Property.Name +
						"': bitfields, C arrays and editor-only members are not supported in NoExport structs");
			}
		}
	}
	File.Order.emplace_back(ETypeKind::Struct, static_cast<int>(File.Structs.size()));
	File.Structs.push_back(std::move(Struct));
}

void FHeaderParser::ParseEnum(const FToken& Macro)
{
	FEnumDef Enum;
	std::vector<FSpecifier> Specifiers;
	ParseSpecifiers(Macro, Specifiers);
	for (const FSpecifier& Specifier : Specifiers)
	{
		const std::string Key = ToLower(Specifier.Name);
		if (Key == "flags")
		{
			Enum.bFlags = true;
		}
		else if (!IsOneOf(Key, {"blueprinttype", "meta"}))
		{
			Warn(Specifier.Line, "Unknown enum specifier '" + Specifier.Name + "' (ignored)");
		}
	}
	if (Peek().IsIdentifier("namespace"))
	{
		Fail(Peek().Line,
			"Namespaced enums (UENUM() namespace E { enum Type ... }) are not supported; use an enum "
			"class with an underlying type");
	}
	if (!MatchIdentifier("enum"))
	{
		Fail(Peek().Line, "Expected 'enum' after UENUM(...)");
	}
	Enum.bEnumClass = MatchIdentifier("class") || MatchIdentifier("struct");
	if (Peek().Type != ETokenType::Identifier)
	{
		Fail(Peek().Line, "Expected an enum name");
	}
	const FToken Name = Next();
	Enum.Name = Name.Text;
	Enum.DeclarationLine = Name.Line;
	if (MatchSymbol(":"))
	{
		if (Peek().Type != ETokenType::Identifier)
		{
			Fail(Peek().Line, "Expected the underlying type of enum '" + Enum.Name + "'");
		}
		const FToken Underlying = Next();
		if (!IsOneOf(Underlying.Text, {"uint8", "uint16", "uint32", "uint64", "int8", "int16", "int32", "int64"}))
		{
			Fail(Underlying.Line,
				"Unsupported underlying type '" + Underlying.Text + "' for enum '" + Enum.Name +
					"' (use uint8, uint16, uint32, uint64, int8, int16, int32 or int64)");
		}
		Enum.Underlying = Underlying.Text;
	}
	if (Peek().IsSymbol(";"))
	{
		Fail(Name.Line, "UENUM must be followed by an enum definition, not a forward declaration");
	}
	if (!MatchSymbol("{"))
	{
		Fail(Peek().Line, "Expected '{' to start the body of enum '" + Enum.Name + "'");
	}
	while (!MatchSymbol("}"))
	{
		if (Peek().Type != ETokenType::Identifier)
		{
			Fail(Peek().Line, "Expected an enumerator name in enum '" + Enum.Name + "', found '" + Peek().Text + "'");
		}
		Enum.Enumerators.push_back(Next().Text);
		if (MatchSymbol("="))
		{
			int Depth = 0;
			while (true)
			{
				const FToken& Token = Peek();
				if (Token.Type == ETokenType::EndOfFile)
				{
					Fail(Name.Line, "Missing '}' at the end of enum '" + Enum.Name + "'");
				}
				if (Depth == 0 && (Token.IsSymbol(",") || Token.IsSymbol("}") || Token.IsIdentifier("UMETA")))
				{
					break;
				}
				Depth += Token.IsSymbol("(") ? 1 : (Token.IsSymbol(")") ? -1 : 0);
				Next();
			}
		}
		if (Peek().IsIdentifier("UMETA"))
		{
			const FToken Meta = Next();
			std::vector<FSpecifier> Ignored;
			ParseSpecifiers(Meta, Ignored);
		}
		if (!MatchSymbol(",") && !Peek().IsSymbol("}"))
		{
			Fail(Peek().Line, "Expected ',' or '}' in enum '" + Enum.Name + "', found '" + Peek().Text + "'");
		}
	}
	if (!MatchSymbol(";"))
	{
		Fail(Peek().Line, "Expected ';' after the body of enum '" + Enum.Name + "'");
	}
	if (Enum.Enumerators.empty())
	{
		Fail(Enum.DeclarationLine, "Enum '" + Enum.Name + "' has no enumerators");
	}
	File.Order.emplace_back(ETypeKind::Enum, static_cast<int>(File.Enums.size()));
	File.Enums.push_back(std::move(Enum));
}

int FHeaderParser::ParseBody(FClassDef* Class, FStructDef* Struct, const std::string& OwnerName, int DeclarationLine)
{
	EAccessSpecifier Access = Class ? EAccessSpecifier::Private : EAccessSpecifier::Public;
	const char* const OwnerKind = Class ? "class" : "struct";
	int BodyLine = 0;
	while (true)
	{
		const FToken& Token = Peek();
		if (Token.Type == ETokenType::EndOfFile)
		{
			Fail(DeclarationLine, "Missing '}' at the end of " + std::string(OwnerKind) + " '" + OwnerName + "'");
		}
		if (MatchSymbol("}"))
		{
			break;
		}
		if (IsAccessSpecifier(Token) && Peek(1).IsSymbol(":"))
		{
			Access = Token.Text == "public"
				? EAccessSpecifier::Public
				: (Token.Text == "protected" ? EAccessSpecifier::Protected : EAccessSpecifier::Private);
			Next();
			Next();
			continue;
		}
		if (Token.Type == ETokenType::Identifier && Token.Text.compare(0, 10, "GENERATED_") == 0 && IsBodyMacro(Token))
		{
			const FToken Macro = Next();
			const bool bLegacy = Macro.Text == "GENERATED_UCLASS_BODY";
			if (Macro.Text == "GENERATED_UINTERFACE_BODY" || Macro.Text == "GENERATED_IINTERFACE_BODY")
			{
				Fail(Macro.Line, Macro.Text + " is not supported by LeonHeaderTool");
			}
			if ((Struct && bLegacy) || (Class && Macro.Text == "GENERATED_USTRUCT_BODY"))
			{
				Fail(Macro.Line, Macro.Text + " cannot be used in " + OwnerKind + " '" + OwnerName + "'");
			}
			if (BodyLine > 0)
			{
				Fail(Macro.Line, "Duplicate " + Macro.Text + " in " + OwnerKind + " '" + OwnerName + "'");
			}
			if (!MatchSymbol("("))
			{
				Fail(Macro.Line, "Expected '(' after " + Macro.Text);
			}
			int Depth = 1;
			while (Depth > 0)
			{
				const FToken& Inner = Next();
				if (Inner.Type == ETokenType::EndOfFile)
				{
					Fail(Macro.Line, "Missing ')' to close " + Macro.Text + "(");
				}
				Depth += Inner.IsSymbol("(") ? 1 : (Inner.IsSymbol(")") ? -1 : 0);
			}
			MatchSymbol(";");
			BodyLine = Macro.Line;
			if (Class)
			{
				Class->bLegacyBody = bLegacy;
				Access = bLegacy ? EAccessSpecifier::Public : EAccessSpecifier::Private;
			}
			continue;
		}
		if (Token.IsIdentifier("UPROPERTY"))
		{
			const FToken Macro = Next();
			ParseProperty(Macro, Access, Class ? Class->Properties : Struct->Properties);
			continue;
		}
		if (Token.IsIdentifier("UFUNCTION"))
		{
			const FToken Macro = Next();
			if (!Class)
			{
				Fail(Macro.Line, "USTRUCTs cannot contain UFUNCTIONs");
			}
			ParseFunction(Macro, Access, *Class);
			continue;
		}
		if (IsTypeMacro(Token))
		{
			Fail(Token.Line, Token.Text + " cannot be nested inside " + OwnerKind + " '" + OwnerName + "'");
		}
		if (Class)
		{
			CheckConstructor(*Class);
		}
		SkipDeclaration();
	}
	if (BodyLine == 0 && !(Struct && Struct->bNoExport))
	{
		Fail(DeclarationLine,
			"Expected a GENERATED_BODY() at the start of " + std::string(OwnerKind) + " '" + OwnerName + "'");
	}
	if (BodyLine != 0 && Struct && Struct->bNoExport)
	{
		Fail(BodyLine, "NoExport struct '" + OwnerName + "' must not have a GENERATED_BODY (it is never compiled)");
	}
	return BodyLine;
}

void FHeaderParser::CheckConstructor(FClassDef& Class)
{
	size_t Offset = 0;
	while (Peek(Offset).IsIdentifier("explicit") || Peek(Offset).IsIdentifier("inline") ||
		Peek(Offset).IsIdentifier("FORCEINLINE"))
	{
		++Offset;
	}
	if (!Peek(Offset).IsIdentifier(Class.Name.c_str()) || !Peek(Offset + 1).IsSymbol("("))
	{
		return;
	}
	size_t Index = Offset + 2;
	int Depth = 1;
	bool bEmpty = true;
	bool bVoid = false;
	bool bObjectInitializer = false;
	bool bVTableHelper = false;
	while (Depth > 0 && Peek(Index).Type != ETokenType::EndOfFile)
	{
		const FToken& Token = Peek(Index);
		Depth += Token.IsSymbol("(") ? 1 : (Token.IsSymbol(")") ? -1 : 0);
		if (Depth > 0)
		{
			bVoid = bEmpty && Token.IsIdentifier("void");
			bEmpty = false;
			bObjectInitializer |= Token.IsIdentifier("FObjectInitializer");
			bVTableHelper |= Token.IsIdentifier("FVTableHelper");
		}
		++Index;
	}
	if (bEmpty || bVoid)
	{
		Class.bHasDefaultConstructor = true;
	}
	else if (bObjectInitializer)
	{
		Class.bHasObjectInitializerConstructor = true;
	}
	else if (bVTableHelper)
	{
		Class.bHasVTableHelperConstructor = true;
	}
}

void FHeaderParser::SkipDeclaration()
{
	int Depth = 0;
	while (true)
	{
		const FToken& Token = Peek();
		if (Token.Type == ETokenType::EndOfFile)
		{
			return;
		}
		if (Depth == 0 &&
			(Token.IsSymbol("}") || IsBodyMacro(Token) || IsTypeMacro(Token) ||
				(IsAccessSpecifier(Token) && Peek(1).IsSymbol(":"))))
		{
			return;
		}
		Next();
		if (Depth > 0 && (IsBodyMacro(Token) || IsTypeMacro(Token)))
		{
			Fail(Token.Line, Token.Text + " inside a nested type or a function body is not supported");
		}
		if (Token.IsSymbol("(") || Token.IsSymbol("[") || Token.IsSymbol("{"))
		{
			++Depth;
		}
		else if (Token.IsSymbol(")") || Token.IsSymbol("]"))
		{
			--Depth;
		}
		else if (Token.IsSymbol("}"))
		{
			if (--Depth == 0)
			{
				MatchSymbol(";");
				return;
			}
		}
		else if (Depth == 0 && Token.IsSymbol(";"))
		{
			return;
		}
	}
}

void FHeaderParser::ParseProperty(
	const FToken& Macro, EAccessSpecifier Access, std::vector<FPropertyDef>& OutProperties)
{
	std::vector<FSpecifier> Specifiers;
	ParseSpecifiers(Macro, Specifiers);
	if (Macro.bInOtherConditional)
	{
		Fail(Macro.Line, "UPROPERTY must not be inside preprocessor blocks, except for WITH_EDITORONLY_DATA");
	}

	// The declaration up to ';', without its initializer.
	std::vector<FToken> Declaration;
	bool bInitializer = false;
	int Depth = 0;
	int AngleDepth = 0;
	while (true)
	{
		const FToken& Token = Peek();
		if (Token.Type == ETokenType::EndOfFile || (Depth == 0 && Token.IsSymbol("}")) ||
			(Depth == 0 && (IsBodyMacro(Token) || IsTypeMacro(Token))))
		{
			Fail(Macro.Line, "Expected ';' after the UPROPERTY declaration");
		}
		Next();
		if (Depth == 0 && Token.IsSymbol(";"))
		{
			break;
		}
		if (!bInitializer && Depth == 0 && AngleDepth == 0 && (Token.IsSymbol("=") || Token.IsSymbol("{")))
		{
			bInitializer = true;
		}
		if (!bInitializer && Depth == 0 && Token.IsSymbol("("))
		{
			Fail(Token.Line, "UPROPERTY cannot be used on a function; use UFUNCTION");
		}
		if (!bInitializer && Depth == 0 && AngleDepth == 0 && Token.IsSymbol(","))
		{
			Fail(Token.Line, "Only one variable can be declared per UPROPERTY");
		}
		if (Token.IsSymbol("(") || Token.IsSymbol("[") || Token.IsSymbol("{"))
		{
			++Depth;
		}
		else if (Token.IsSymbol(")") || Token.IsSymbol("]") || Token.IsSymbol("}"))
		{
			--Depth;
		}
		else if (!bInitializer && Token.IsSymbol("<"))
		{
			++AngleDepth;
		}
		else if (!bInitializer && Token.IsSymbol(">"))
		{
			--AngleDepth;
		}
		if (!bInitializer)
		{
			Declaration.push_back(Token);
		}
	}
	if (Declaration.empty())
	{
		Fail(Macro.Line, "Expected a variable declaration after UPROPERTY(...)");
	}
	const int Line = Declaration.front().Line;

	FPropertyDef Property;
	Property.Line = Line;
	size_t End = Declaration.size();
	if (End >= 2 && Declaration[End - 2].IsSymbol(":"))
	{
		if (Declaration[End - 1].Text != "1")
		{
			Fail(Line, "Bitfield UPROPERTYs must be one bit wide (uint8 bFlag : 1)");
		}
		Property.bBitfield = true;
		End -= 2;
	}
	if (End > 0 && Declaration[End - 1].IsSymbol("]"))
	{
		size_t Open = End - 1;
		while (Open > 0 && !Declaration[Open].IsSymbol("["))
		{
			--Open;
		}
		if (Open > 0 && Declaration[Open - 1].IsSymbol("]"))
		{
			Fail(Line, "Multi-dimensional arrays are not supported");
		}
		Property.bFixedArray = true;
		End = Open;
	}
	if (End == 0 || Declaration[End - 1].Type != ETokenType::Identifier)
	{
		Fail(Line, "Expected a variable name in the UPROPERTY declaration");
	}
	Property.Name = Declaration[End - 1].Text;
	--End;

	size_t Begin = 0;
	bool bConst = false;
	while (Begin < End && Declaration[Begin].Type == ETokenType::Identifier &&
		IsOneOf(Declaration[Begin].Text, {"static", "mutable", "const"}))
	{
		if (Declaration[Begin].Text == "static")
		{
			Fail(Line, "UPROPERTY cannot be used on static members");
		}
		bConst |= Declaration[Begin].Text == "const";
		++Begin;
	}
	bool bReference = false;
	ParseType(Declaration, Begin, End, Line, Property.Type, bReference);
	if (bReference)
	{
		Fail(Line, "Reference member variables cannot be UPROPERTYs");
	}
	if (bConst && !(Property.Type.Kind == EPropertyKind::Unresolved && Property.Type.bPointer))
	{
		Fail(Line, "Const properties are not supported: '" + Property.Name + "'");
	}
	if (Property.bBitfield)
	{
		const EPropertyKind Kind = Property.Type.Kind;
		const bool bUnsigned = (Kind == EPropertyKind::Byte && !Property.Type.bEnumAsByte) ||
			Kind == EPropertyKind::UInt16 || Kind == EPropertyKind::UInt32 || Kind == EPropertyKind::UInt64;
		if (!bUnsigned && Kind != EPropertyKind::Bool)
		{
			Fail(Line, "Bitfield UPROPERTYs must use an unsigned integer type (uint8 bFlag : 1)");
		}
		Property.BitfieldStorage = Property.Type.GetCppType();
		Property.Type = FPropertyType();
		Property.Type.Kind = EPropertyKind::Bool;
	}
	if (Property.bFixedArray && Property.Type.IsContainer())
	{
		Fail(Line, "Static arrays of containers are not allowed");
	}
	if (Property.bFixedArray && Property.Type.Kind == EPropertyKind::Bool)
	{
		Fail(Line, "Static arrays of bool are not supported");
	}

	using namespace EPropertyFlagBits;
	uint64_t Flags = 0;
	for (const FSpecifier& Specifier : Specifiers)
	{
		const std::string Key = ToLower(Specifier.Name);
		static const std::pair<const char*, uint64_t> FlagSpecifiers[] = {{"config", Config},
			{"globalconfig", GlobalConfig | Config}, {"transient", Transient},
			{"duplicatetransient", DuplicateTransient}, {"nonpieduplicatetransient", NonPIEDuplicateTransient},
			{"textexporttransient", TextExportTransient}, {"savegame", SaveGame},
			{"skipserialization", SkipSerialization}, {"editanywhere", Edit},
			{"editdefaultsonly", Edit | DisableEditOnInstance}, {"editinstanceonly", Edit | DisableEditOnTemplate},
			{"visibleanywhere", Edit | EditConst}, {"visibledefaultsonly", Edit | EditConst | DisableEditOnInstance},
			{"visibleinstanceonly", Edit | EditConst | DisableEditOnTemplate}, {"blueprintreadwrite", BlueprintVisible},
			{"blueprintreadonly", BlueprintVisible | BlueprintReadOnly},
			{"instanced", PersistentInstance | ExportObject | InstancedReference}, {"export", ExportObject},
			{"noclear", NoClear}, {"editfixedsize", EditFixedSize}, {"advanceddisplay", AdvancedDisplay},
			{"simpledisplay", SimpleDisplay}, {"assetregistrysearchable", AssetRegistrySearchable}, {"interp", Interp},
			{"nontransactional", NonTransactional}, {"blueprintassignable", BlueprintAssignable}};
		bool bKnown = false;
		for (const auto& FlagSpecifier : FlagSpecifiers)
		{
			if (Key == FlagSpecifier.first)
			{
				Flags |= FlagSpecifier.second;
				bKnown = true;
			}
		}
		if (bKnown)
		{
			continue;
		}
		if (IsOneOf(Key, {"replicated", "replicatedusing", "notreplicated", "repretry"}))
		{
			Fail(Specifier.Line,
				"Property specifier '" + Specifier.Name + "' is not supported by LeonHeaderTool (no replication)");
		}
		if (!IsOneOf(Key,
				{"category", "meta", "blueprintsetter", "blueprintgetter", "blueprintcallable",
					"blueprintauthorityonly"}))
		{
			Warn(Specifier.Line, "Unknown property specifier '" + Specifier.Name + "' (ignored)");
		}
	}
	if (Access == EAccessSpecifier::Private)
	{
		const FSpecifier* Meta = FindSpecifier(Specifiers, "meta");
		const bool bAllowPrivateAccess = Meta && ToLower(Meta->Value).find("allowprivateaccess") != std::string::npos;
		if (!bAllowPrivateAccess && FindSpecifier(Specifiers, "blueprintreadwrite"))
		{
			Fail(Line, "BlueprintReadWrite should not be used on private members");
		}
		if (!bAllowPrivateAccess && FindSpecifier(Specifiers, "blueprintreadonly"))
		{
			Fail(Line, "BlueprintReadOnly should not be used on private members");
		}
	}
	switch (Access)
	{
		case EAccessSpecifier::Public:
			Flags |= NativeAccessSpecifierPublic;
			break;
		case EAccessSpecifier::Protected:
			Flags |= NativeAccessSpecifierProtected | Protected;
			break;
		case EAccessSpecifier::Private:
			Flags |= NativeAccessSpecifierPrivate;
			break;
	}
	if (Macro.bEditorOnlyData)
	{
		Flags |= EditorOnly;
		Property.bEditorOnly = true;
	}
	Property.Flags = Flags;
	Property.Access = Access;
	for (const FPropertyDef& Existing : OutProperties)
	{
		if (Existing.Name == Property.Name)
		{
			Fail(Line, "Property '" + Property.Name + "' is declared twice");
		}
	}
	OutProperties.push_back(std::move(Property));
}

void FHeaderParser::ParseFunction(const FToken& Macro, EAccessSpecifier Access, FClassDef& Class)
{
	std::vector<FSpecifier> Specifiers;
	ParseSpecifiers(Macro, Specifiers);
	if (Macro.bInOtherConditional || Macro.bEditorOnlyData)
	{
		Fail(Macro.Line, "UFUNCTION must not be inside preprocessor blocks");
	}

	std::vector<FToken> Declaration;
	int Depth = 0;
	while (true)
	{
		const FToken& Token = Peek();
		if (Token.Type == ETokenType::EndOfFile || (Depth == 0 && Token.IsSymbol("}")))
		{
			Fail(Macro.Line, "Expected a function declaration after UFUNCTION(...)");
		}
		if (Depth == 0 && Token.IsSymbol("{"))
		{
			SkipDeclaration();
			break;
		}
		Next();
		if (Depth == 0 && Token.IsSymbol(";"))
		{
			break;
		}
		Depth += (Token.IsSymbol("(") || Token.IsSymbol("[")) ? 1 : 0;
		Depth -= (Token.IsSymbol(")") || Token.IsSymbol("]")) ? 1 : 0;
		Declaration.push_back(Token);
	}

	FFunctionDef Function;
	bool bVirtual = false;
	size_t Index = 0;
	while (Index < Declaration.size() && Declaration[Index].Type == ETokenType::Identifier &&
		IsOneOf(Declaration[Index].Text,
			{"virtual", "static", "inline", "FORCEINLINE", "FORCENOINLINE", "constexpr", "explicit"}))
	{
		bVirtual |= Declaration[Index].Text == "virtual";
		Function.bStatic |= Declaration[Index].Text == "static";
		++Index;
	}
	size_t Open = Index;
	int AngleDepth = 0;
	while (Open < Declaration.size() && !(AngleDepth == 0 && Declaration[Open].IsSymbol("(")))
	{
		AngleDepth += Declaration[Open].IsSymbol("<") ? 1 : (Declaration[Open].IsSymbol(">") ? -1 : 0);
		++Open;
	}
	if (Open >= Declaration.size() || Open == Index || Declaration[Open - 1].Type != ETokenType::Identifier)
	{
		Fail(Macro.Line, "Expected a function declaration after UFUNCTION(...)");
	}
	const FToken& Name = Declaration[Open - 1];
	Function.Name = Name.Text;
	Function.Line = Name.Line;
	if (Function.Name == Class.Name || (Open >= 2 && Declaration[Open - 2].IsSymbol("~")))
	{
		Fail(Name.Line, "UFUNCTION cannot be used on constructors or destructors");
	}
	for (const FFunctionDef& Existing : Class.Functions)
	{
		if (Existing.Name == Function.Name)
		{
			Fail(Name.Line, "Overloaded UFUNCTIONs are not supported: '" + Function.Name + "' is declared twice");
		}
	}

	size_t Close = Open + 1;
	Depth = 1;
	while (Close < Declaration.size())
	{
		Depth += Declaration[Close].IsSymbol("(") ? 1 : (Declaration[Close].IsSymbol(")") ? -1 : 0);
		if (Depth == 0)
		{
			break;
		}
		++Close;
	}
	if (Close >= Declaration.size())
	{
		Fail(Name.Line, "Missing ')' in the declaration of '" + Function.Name + "'");
	}

	using namespace EFunctionFlagBits;
	uint32_t Flags = Native;
	for (size_t After = Close + 1; After < Declaration.size(); ++After)
	{
		const FToken& Token = Declaration[After];
		if (Token.IsIdentifier("const"))
		{
			Flags |= Const;
		}
		else if (Token.IsSymbol("="))
		{
			Fail(Token.Line, "UFUNCTION '" + Function.Name + "' cannot be pure virtual, defaulted or deleted");
		}
		else if (!Token.IsIdentifier("override") && !Token.IsIdentifier("final") && !Token.IsIdentifier("noexcept"))
		{
			Fail(Token.Line, "Unexpected '" + Token.Text + "' after the parameters of '" + Function.Name + "'");
		}
	}

	// Return value.
	size_t ReturnBegin = Index;
	while (ReturnBegin < Open - 1 && Declaration[ReturnBegin].IsIdentifier("const"))
	{
		++ReturnBegin;
	}
	if (ReturnBegin == Open - 1)
	{
		Fail(Name.Line, "Missing return type for '" + Function.Name + "'");
	}
	if (!(Open - 1 - ReturnBegin == 1 && Declaration[ReturnBegin].IsIdentifier("void")))
	{
		bool bReference = false;
		ParseType(Declaration, ReturnBegin, Open - 1, Name.Line, Function.ReturnValue.Type, bReference);
		if (bReference)
		{
			Fail(Name.Line, "UFUNCTION return values cannot be references: '" + Function.Name + "'");
		}
		Function.bHasReturnValue = true;
		Function.ReturnValue.Name = "ReturnValue";
		Function.ReturnValue.Line = Name.Line;
		Function.ReturnValue.Flags = EPropertyFlagBits::Parm | EPropertyFlagBits::OutParm |
			EPropertyFlagBits::ReturnParm | EPropertyFlagBits::NativeAccessSpecifierPublic;
	}

	// Parameters, split at top-level commas.
	std::vector<std::pair<size_t, size_t>> Ranges;
	{
		size_t Start = Open + 1;
		int ParamDepth = 0;
		for (size_t Cursor = Open + 1; Cursor <= Close; ++Cursor)
		{
			const FToken& Token = Declaration[Cursor];
			if (Cursor == Close || (ParamDepth == 0 && Token.IsSymbol(",")))
			{
				Ranges.emplace_back(Start, Cursor);
				Start = Cursor + 1;
				continue;
			}
			if (Token.IsSymbol("(") || Token.IsSymbol("<") || Token.IsSymbol("[") || Token.IsSymbol("{"))
			{
				++ParamDepth;
			}
			else if (Token.IsSymbol(")") || Token.IsSymbol(">") || Token.IsSymbol("]") || Token.IsSymbol("}"))
			{
				--ParamDepth;
			}
		}
	}
	if (Ranges.size() == 1 &&
		(Ranges[0].first == Ranges[0].second ||
			(Ranges[0].second - Ranges[0].first == 1 && Declaration[Ranges[0].first].IsIdentifier("void"))))
	{
		Ranges.clear();
	}
	for (const std::pair<size_t, size_t>& Range : Ranges)
	{
		size_t Begin = Range.first;
		size_t End = Range.second;
		for (size_t Cursor = Begin; Cursor < End; ++Cursor)
		{
			if (Declaration[Cursor].IsSymbol("="))
			{
				End = Cursor;
				break;
			}
		}
		if (Begin == End)
		{
			Fail(Name.Line, "Empty parameter in the declaration of '" + Function.Name + "'");
		}
		const int ParamLine = Declaration[Begin].Line;
		if (Declaration[End - 1].IsSymbol("]"))
		{
			Fail(ParamLine, "Array parameters are not supported in UFUNCTION '" + Function.Name + "'");
		}
		bool bConst = false;
		while (Begin < End && Declaration[Begin].IsIdentifier("const"))
		{
			bConst = true;
			++Begin;
		}
		if (End - Begin < 2 || Declaration[End - 1].Type != ETokenType::Identifier)
		{
			Fail(ParamLine, "UFUNCTION parameters must be named ('" + Function.Name + "')");
		}
		FPropertyDef Param;
		Param.Name = Declaration[End - 1].Text;
		Param.Line = ParamLine;
		bool bReference = false;
		ParseType(Declaration, Begin, End - 1, ParamLine, Param.Type, bReference);
		if (bReference && !bConst)
		{
			Fail(ParamLine, "Non-const reference parameters (out parameters) are not supported: '" + Param.Name + "'");
		}
		Param.bConstRef = bConst && bReference;
		Param.Flags = EPropertyFlagBits::Parm | EPropertyFlagBits::NativeAccessSpecifierPublic;
		if (bConst && !bReference)
		{
			Param.Flags |= EPropertyFlagBits::ConstParm;
		}
		Function.Params.push_back(std::move(Param));
	}

	for (const FSpecifier& Specifier : Specifiers)
	{
		const std::string Key = ToLower(Specifier.Name);
		if (Key == "exec")
		{
			Flags |= Exec;
		}
		else if (Key == "blueprintcallable")
		{
			Flags |= BlueprintCallable;
		}
		else if (Key == "blueprintpure")
		{
			Flags |= BlueprintCallable | BlueprintPure;
		}
		else if (Key == "blueprintauthorityonly")
		{
			Flags |= BlueprintAuthorityOnly;
		}
		else if (Key == "blueprintcosmetic")
		{
			Flags |= BlueprintCosmetic;
		}
		else if (IsOneOf(Key,
					 {"blueprintimplementableevent", "blueprintnativeevent", "server", "client", "netmulticast",
						 "reliable", "unreliable", "withvalidation", "customthunk", "sealedevent", "serviceresponse",
						 "servicerequest"}))
		{
			Fail(Specifier.Line, "Function specifier '" + Specifier.Name + "' is not supported by LeonHeaderTool");
		}
		else if (!IsOneOf(Key, {"category", "meta", "callineditor", "displayname"}))
		{
			Warn(Specifier.Line, "Unknown function specifier '" + Specifier.Name + "' (ignored)");
		}
	}
	switch (Access)
	{
		case EAccessSpecifier::Public:
			Flags |= Public;
			break;
		case EAccessSpecifier::Protected:
			Flags |= Protected;
			break;
		case EAccessSpecifier::Private:
			Flags |= Private;
			break;
	}
	if (!bVirtual)
	{
		Flags |= Final;
	}
	if (Function.bStatic)
	{
		Flags |= Static;
	}
	Function.FunctionFlags = Flags;
	Function.Access = Access;
	Class.Functions.push_back(std::move(Function));
}

void FHeaderParser::ParseType(const std::vector<FToken>& TypeTokens, size_t Begin, size_t End, int Line,
	FPropertyType& OutType, bool& bOutReference)
{
	bOutReference = false;
	OutType = FPropertyType();
	size_t Index = Begin;
	if (Index < End && TypeTokens[Index].Type == ETokenType::Identifier &&
		IsOneOf(TypeTokens[Index].Text, {"class", "struct", "enum"}))
	{
		++Index;
	}
	if (Index >= End)
	{
		Fail(Line, "Missing type in declaration");
	}
	const FToken& Word = TypeTokens[Index];
	if (Word.Type != ETokenType::Identifier)
	{
		Fail(Line, "Unexpected '" + Word.Text + "' in a type");
	}
	++Index;
	if (Index < End && TypeTokens[Index].IsSymbol("::"))
	{
		Fail(Line,
			"Qualified type names ('" + Word.Text +
				"::...') are not supported; namespaced enums must become enum classes");
	}

	const int Arity = GetTemplateArity(Word.Text);
	EPropertyKind BuiltinKind = EPropertyKind::Unresolved;
	if (Arity > 0)
	{
		if (Index >= End || !TypeTokens[Index].IsSymbol("<"))
		{
			Fail(Line, "Expected '<' after " + Word.Text);
		}
		++Index;
		std::vector<std::pair<size_t, size_t>> Arguments;
		size_t ArgumentBegin = Index;
		int Depth = 0;
		while (true)
		{
			if (Index >= End)
			{
				Fail(Line, "Missing '>' to close " + Word.Text + "<");
			}
			const FToken& Token = TypeTokens[Index];
			if (Depth == 0 && (Token.IsSymbol(",") || Token.IsSymbol(">")))
			{
				Arguments.emplace_back(ArgumentBegin, Index);
				ArgumentBegin = Index + 1;
				if (Token.IsSymbol(">"))
				{
					++Index;
					break;
				}
			}
			else if (Token.IsSymbol("<") || Token.IsSymbol("("))
			{
				++Depth;
			}
			else if (Token.IsSymbol(">") || Token.IsSymbol(")"))
			{
				--Depth;
			}
			++Index;
		}
		if (static_cast<int>(Arguments.size()) != Arity)
		{
			Fail(Line,
				Word.Text + " takes " + std::to_string(Arity) + " type argument" + (Arity > 1 ? "s" : "") +
					" here (custom allocators and key funcs are not supported)");
		}
		if (Word.Text == "TArray" || Word.Text == "TSet" || Word.Text == "TMap")
		{
			OutType.Kind = Word.Text == "TArray" ? EPropertyKind::Array
												 : (Word.Text == "TSet" ? EPropertyKind::Set : EPropertyKind::Map);
			for (const std::pair<size_t, size_t>& Argument : Arguments)
			{
				FPropertyType Inner;
				bool bInnerReference = false;
				ParseType(TypeTokens, Argument.first, Argument.second, Line, Inner, bInnerReference);
				if (bInnerReference)
				{
					Fail(Line, "Containers of references are not supported");
				}
				if (Inner.IsContainer())
				{
					Fail(Line, "Nested containers are not supported.");
				}
				OutType.Inner.push_back(std::move(Inner));
			}
		}
		else
		{
			size_t ArgumentIndex = Arguments[0].first;
			const size_t ArgumentEnd = Arguments[0].second;
			if (ArgumentIndex < ArgumentEnd && TypeTokens[ArgumentIndex].Type == ETokenType::Identifier &&
				IsOneOf(TypeTokens[ArgumentIndex].Text, {"class", "struct", "enum"}))
			{
				++ArgumentIndex;
			}
			if (ArgumentEnd - ArgumentIndex != 1 || TypeTokens[ArgumentIndex].Type != ETokenType::Identifier)
			{
				Fail(Line, "Expected a type name in " + Word.Text + "<...>");
			}
			OutType.TypeName = TypeTokens[ArgumentIndex].Text;
			if (Word.Text == "TSubclassOf")
			{
				OutType.Kind = EPropertyKind::Class;
			}
			else if (Word.Text == "TSoftObjectPtr")
			{
				OutType.Kind = EPropertyKind::SoftObject;
			}
			else if (Word.Text == "TSoftClassPtr")
			{
				OutType.Kind = EPropertyKind::SoftClass;
			}
			else if (Word.Text == "TWeakObjectPtr")
			{
				OutType.Kind = EPropertyKind::WeakObject;
			}
			else
			{
				OutType.Kind = EPropertyKind::Byte;
				OutType.bEnumAsByte = true;
			}
		}
	}
	else if (Word.Text == "unsigned")
	{
		if (Index < End && TypeTokens[Index].IsIdentifier("int"))
		{
			++Index;
		}
		else if (Index < End && TypeTokens[Index].Type == ETokenType::Identifier)
		{
			Fail(Line,
				"Unrecognized type 'unsigned " + TypeTokens[Index].Text + "' - use uint8, uint16, uint32 or uint64");
		}
		OutType.Kind = EPropertyKind::UnsizedUInt;
	}
	else if (FindBuiltinKind(Word.Text, BuiltinKind))
	{
		OutType.Kind = BuiltinKind;
	}
	else
	{
		OutType.Kind = EPropertyKind::Unresolved;
		OutType.TypeName = Word.Text;
	}

	for (; Index < End; ++Index)
	{
		const FToken& Token = TypeTokens[Index];
		if (Token.IsSymbol("*"))
		{
			if (OutType.Kind == EPropertyKind::Unresolved && !OutType.bPointer)
			{
				OutType.bPointer = true;
				continue;
			}
			if (OutType.Kind == EPropertyKind::Unresolved)
			{
				Fail(Line, "Pointers to pointers are not supported");
			}
			Fail(Line,
				"Inappropriate '*' on variable of type '" + OutType.GetCppType() +
					"', cannot have an exposed pointer to this type.");
		}
		if (Token.IsSymbol("&"))
		{
			bOutReference = true;
			continue;
		}
		if (Token.IsSymbol("&&"))
		{
			Fail(Line, "Rvalue references are not supported");
		}
		if (Token.IsIdentifier("const"))
		{
			continue;
		}
		Fail(Line, "Unexpected '" + Token.Text + "' after type '" + OutType.GetCppType() + "'");
	}
}
