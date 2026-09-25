#pragma once

#include "Tokenizer.h"
#include "TypeModel.h"

#include <string>
#include <vector>

class FDiagnostics;

/** One entry of a UCLASS / USTRUCT / UENUM / UPROPERTY / UFUNCTION / UMETA specifier list. */
struct FSpecifier
{
	std::string Name;
	std::string Value;
	int Line = 0;
};

/**
 * Parses the UE subset LeonHeaderTool understands (UHT: FHeaderParser): UCLASS, USTRUCT, UENUM (+UMETA), UPROPERTY,
 * UFUNCTION, GENERATED_BODY / GENERATED_UCLASS_BODY / GENERATED_USTRUCT_BODY and #if WITH_EDITORONLY_DATA blocks.
 * Everything else in the header is skipped. Types are only named here; FTypeTable resolves them afterwards.
 * The first structural error stops the file, as UHT does.
 */
class FHeaderParser
{
public:
	FHeaderParser(FUnrealSourceFile& InFile, FDiagnostics& InDiagnostics);

	/** Fills the file's types from Source. */
	void Parse(const std::string& Source);

	/** Line of an '#include "...generated.h"' in Source, or 0 (a quick scan for headers not yet reflected). */
	static int FindGeneratedInclude(const std::string& Source);

private:
	struct FParseAbort
	{
	};

	[[noreturn]] void Fail(int Line, const std::string& Message);
	void Warn(int Line, const std::string& Message);

	void Preprocess(const std::vector<FToken>& RawTokens);
	void ParseFile();

	const FToken& Peek(size_t Ahead = 0) const;
	const FToken& Next();
	bool MatchSymbol(const char* Symbol);
	bool MatchIdentifier(const char* Name);

	int ParseSpecifiers(const FToken& Macro, std::vector<FSpecifier>& OutSpecifiers);
	std::string ParseBaseList();
	void ParseClass(const FToken& Macro);
	void ParseStruct(const FToken& Macro);
	void ParseEnum(const FToken& Macro);
	/** Parses a class or struct body up to and including its '}'; returns the GENERATED_BODY line. */
	int ParseBody(FClassDef* Class, FStructDef* Struct, const std::string& OwnerName, int DeclarationLine);
	void ParseProperty(const FToken& Macro, EAccessSpecifier Access, std::vector<FPropertyDef>& OutProperties);
	void ParseFunction(const FToken& Macro, EAccessSpecifier Access, FClassDef& Class);
	void SkipDeclaration();
	void CheckConstructor(FClassDef& Class);

	/** Parses Tokens[Begin, End) as a property type; bOutReference reports a trailing '&'. */
	void ParseType(const std::vector<FToken>& Tokens, size_t Begin, size_t End, int Line, FPropertyType& OutType,
		bool& bOutReference);

	FUnrealSourceFile& File;
	FDiagnostics& Diagnostics;
	std::vector<FToken> Tokens;
	size_t Pos = 0;
	int GeneratedIncludeLine = 0;
	int FirstTypeLine = 0;
};
