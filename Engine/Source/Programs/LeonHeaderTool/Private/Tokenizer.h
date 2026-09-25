#pragma once

#include <string>
#include <vector>

enum class ETokenType
{
	Identifier,
	Number,
	String,
	Char,
	Symbol,
	/** A whole preprocessor line (continuations joined, comments removed) without the '#'. */
	Directive,
	EndOfFile
};

/** One C++ token with the line it starts on (UHT: FToken). */
struct FToken
{
	ETokenType Type = ETokenType::EndOfFile;
	std::string Text;
	int Line = 0;
	/** Set by the preprocessor pass: inside #if WITH_EDITORONLY_DATA. */
	bool bEditorOnlyData = false;
	/** Set by the preprocessor pass: inside any other #if / #ifdef block. */
	bool bInOtherConditional = false;

	bool IsIdentifier(const char* Name) const
	{
		return Type == ETokenType::Identifier && Text == Name;
	}

	bool IsSymbol(const char* Symbol) const
	{
		return Type == ETokenType::Symbol && Text == Symbol;
	}
};

/**
 * Splits a header into tokens (UHT: FBaseParser). Comments are dropped, preprocessor lines become Directive tokens,
 * string literals keep their quotes. '>>' is always two '>' tokens so nested template arguments close one by one.
 */
class FTokenizer
{
public:
	/** Returns false (and fills OutError / OutErrorLine) on an unterminated comment, string or character literal. */
	static bool Tokenize(
		const std::string& Source, std::vector<FToken>& OutTokens, std::string& OutError, int& OutErrorLine);
};
