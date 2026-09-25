#include "Tokenizer.h"

#include <cctype>

namespace
{
	bool IsIdentifierStart(char C)
	{
		return std::isalpha(static_cast<unsigned char>(C)) != 0 || C == '_';
	}

	bool IsIdentifierChar(char C)
	{
		return std::isalnum(static_cast<unsigned char>(C)) != 0 || C == '_';
	}

	bool IsHorizontalSpace(char C)
	{
		return C == ' ' || C == '\t' || C == '\r' || C == '\f' || C == '\v';
	}

	/** Collapses whitespace runs to one space and trims both ends. */
	std::string NormalizeSpaces(const std::string& Text)
	{
		std::string Result;
		bool bPendingSpace = false;
		for (char C : Text)
		{
			if (IsHorizontalSpace(C) || C == '\n')
			{
				bPendingSpace = !Result.empty();
				continue;
			}
			if (bPendingSpace)
			{
				Result += ' ';
				bPendingSpace = false;
			}
			Result += C;
		}
		return Result;
	}

	struct FCursor
	{
		const std::string& Source;
		size_t Pos = 0;
		int Line = 1;

		char At(size_t Offset = 0) const
		{
			return Pos + Offset < Source.size() ? Source[Pos + Offset] : '\0';
		}

		bool IsEnd() const
		{
			return Pos >= Source.size();
		}
	};

	/** Skips a quoted literal starting at the opening quote; false if a newline or the end comes first. */
	bool SkipQuoted(FCursor& Cursor, char Quote)
	{
		++Cursor.Pos;
		while (!Cursor.IsEnd())
		{
			const char C = Cursor.At();
			if (C == '\\')
			{
				if (Cursor.At(1) == '\n')
				{
					++Cursor.Line;
				}
				Cursor.Pos += 2;
				continue;
			}
			if (C == '\n')
			{
				return false;
			}
			++Cursor.Pos;
			if (C == Quote)
			{
				return true;
			}
		}
		return false;
	}

	/** Skips R"delim( ... )delim" starting at the opening quote. */
	bool SkipRawString(FCursor& Cursor)
	{
		const size_t Open = Cursor.Source.find('(', Cursor.Pos);
		if (Open == std::string::npos)
		{
			return false;
		}
		const std::string Terminator = ")" + Cursor.Source.substr(Cursor.Pos + 1, Open - Cursor.Pos - 1) + "\"";
		const size_t Close = Cursor.Source.find(Terminator, Open);
		if (Close == std::string::npos)
		{
			return false;
		}
		for (size_t Index = Cursor.Pos; Index < Close; ++Index)
		{
			if (Cursor.Source[Index] == '\n')
			{
				++Cursor.Line;
			}
		}
		Cursor.Pos = Close + Terminator.size();
		return true;
	}

	/** Reads a preprocessor line after its '#': joins continuations and drops comments. */
	bool ReadDirective(FCursor& Cursor, std::string& OutText)
	{
		std::string Text;
		++Cursor.Pos;
		while (!Cursor.IsEnd())
		{
			const char C = Cursor.At();
			if (C == '\n')
			{
				break;
			}
			if (C == '\\' && (Cursor.At(1) == '\n' || (Cursor.At(1) == '\r' && Cursor.At(2) == '\n')))
			{
				Cursor.Pos += Cursor.At(1) == '\n' ? 2 : 3;
				++Cursor.Line;
				Text += ' ';
				continue;
			}
			if (C == '/' && Cursor.At(1) == '/')
			{
				while (!Cursor.IsEnd() && Cursor.At() != '\n')
				{
					++Cursor.Pos;
				}
				break;
			}
			if (C == '/' && Cursor.At(1) == '*')
			{
				const size_t End = Cursor.Source.find("*/", Cursor.Pos + 2);
				if (End == std::string::npos)
				{
					return false;
				}
				for (size_t Index = Cursor.Pos; Index < End; ++Index)
				{
					if (Cursor.Source[Index] == '\n')
					{
						++Cursor.Line;
					}
				}
				Cursor.Pos = End + 2;
				Text += ' ';
				continue;
			}
			if (C == '"' || C == '\'')
			{
				const size_t Start = Cursor.Pos;
				if (!SkipQuoted(Cursor, C))
				{
					return false;
				}
				Text += Cursor.Source.substr(Start, Cursor.Pos - Start);
				continue;
			}
			Text += C;
			++Cursor.Pos;
		}
		OutText = NormalizeSpaces(Text);
		return true;
	}
} // namespace

bool FTokenizer::Tokenize(
	const std::string& Source, std::vector<FToken>& OutTokens, std::string& OutError, int& OutErrorLine)
{
	static const char* const MultiCharSymbols[] = {"...", "::", "->", "&&", "||", "==", "!="};

	OutTokens.clear();
	FCursor Cursor{Source};
	bool bLineStart = true;
	while (!Cursor.IsEnd())
	{
		const char C = Cursor.At();
		if (C == '\n')
		{
			++Cursor.Line;
			++Cursor.Pos;
			bLineStart = true;
			continue;
		}
		if (IsHorizontalSpace(C))
		{
			++Cursor.Pos;
			continue;
		}
		if (C == '\\' && (Cursor.At(1) == '\n' || (Cursor.At(1) == '\r' && Cursor.At(2) == '\n')))
		{
			Cursor.Pos += Cursor.At(1) == '\n' ? 2 : 3;
			++Cursor.Line;
			continue;
		}
		if (C == '/' && Cursor.At(1) == '/')
		{
			while (!Cursor.IsEnd() && Cursor.At() != '\n')
			{
				++Cursor.Pos;
			}
			continue;
		}
		if (C == '/' && Cursor.At(1) == '*')
		{
			const int StartLine = Cursor.Line;
			const size_t End = Source.find("*/", Cursor.Pos + 2);
			if (End == std::string::npos)
			{
				OutError = "Unterminated comment";
				OutErrorLine = StartLine;
				return false;
			}
			for (size_t Index = Cursor.Pos; Index < End; ++Index)
			{
				if (Source[Index] == '\n')
				{
					++Cursor.Line;
				}
			}
			Cursor.Pos = End + 2;
			continue;
		}

		FToken Token;
		Token.Line = Cursor.Line;
		if (C == '#' && bLineStart)
		{
			Token.Type = ETokenType::Directive;
			if (!ReadDirective(Cursor, Token.Text))
			{
				OutError = "Unterminated comment or literal in a preprocessor line";
				OutErrorLine = Token.Line;
				return false;
			}
			OutTokens.push_back(Token);
			continue;
		}
		bLineStart = false;

		const size_t Start = Cursor.Pos;
		if (IsIdentifierStart(C))
		{
			while (IsIdentifierChar(Cursor.At()))
			{
				++Cursor.Pos;
			}
			const std::string Word = Source.substr(Start, Cursor.Pos - Start);
			const char Next = Cursor.At();
			const bool bRawPrefix = Word == "R" || Word == "LR" || Word == "uR" || Word == "UR" || Word == "u8R";
			const bool bCharPrefix = Word == "L" || Word == "u" || Word == "U" || Word == "u8";
			if (Next == '"' && (bRawPrefix || bCharPrefix))
			{
				const int LiteralLine = Cursor.Line;
				if (!(bRawPrefix ? SkipRawString(Cursor) : SkipQuoted(Cursor, '"')))
				{
					OutError = "Unterminated string literal";
					OutErrorLine = LiteralLine;
					return false;
				}
				Token.Type = ETokenType::String;
			}
			else if (Next == '\'' && bCharPrefix)
			{
				if (!SkipQuoted(Cursor, '\''))
				{
					OutError = "Unterminated character literal";
					OutErrorLine = Token.Line;
					return false;
				}
				Token.Type = ETokenType::Char;
			}
			else
			{
				Token.Type = ETokenType::Identifier;
			}
		}
		else if (std::isdigit(static_cast<unsigned char>(C)) != 0 ||
			(C == '.' && std::isdigit(static_cast<unsigned char>(Cursor.At(1))) != 0))
		{
			Token.Type = ETokenType::Number;
			while (!Cursor.IsEnd())
			{
				const char D = Cursor.At();
				const char Previous = Cursor.Source[Cursor.Pos - 1];
				const bool bExponentSign = (D == '+' || D == '-') && Cursor.Pos > Start &&
					(Previous == 'e' || Previous == 'E' || Previous == 'p' || Previous == 'P');
				if (IsIdentifierChar(D) || D == '.' || D == '\'' || bExponentSign)
				{
					++Cursor.Pos;
				}
				else
				{
					break;
				}
			}
		}
		else if (C == '"' || C == '\'')
		{
			if (!SkipQuoted(Cursor, C))
			{
				OutError = C == '"' ? "Unterminated string literal" : "Unterminated character literal";
				OutErrorLine = Token.Line;
				return false;
			}
			Token.Type = C == '"' ? ETokenType::String : ETokenType::Char;
		}
		else
		{
			Token.Type = ETokenType::Symbol;
			size_t SymbolLength = 1;
			for (const char* Symbol : MultiCharSymbols)
			{
				const std::string Candidate(Symbol);
				if (Source.compare(Cursor.Pos, Candidate.size(), Candidate) == 0)
				{
					SymbolLength = Candidate.size();
					break;
				}
			}
			Cursor.Pos += SymbolLength;
		}
		Token.Text = Source.substr(Start, Cursor.Pos - Start);
		OutTokens.push_back(Token);
	}

	FToken EndToken;
	EndToken.Type = ETokenType::EndOfFile;
	EndToken.Line = Cursor.Line;
	OutTokens.push_back(EndToken);
	return true;
}
