#include "Diagnostics.h"

#include <cstdio>

FDiagnostics::FDiagnostics(bool bInCapture)
	: bCapture(bInCapture)
{
}

void FDiagnostics::Error(const std::string& File, int Line, const std::string& Message)
{
	++ErrorCount;
	Emit(File, Line, "error", Message);
}

void FDiagnostics::Warning(const std::string& File, int Line, const std::string& Message)
{
	Emit(File, Line, "warning", Message);
}

int FDiagnostics::GetErrorCount() const
{
	return ErrorCount;
}

const std::vector<std::string>& FDiagnostics::GetMessages() const
{
	return Messages;
}

void FDiagnostics::Emit(const std::string& File, int Line, const char* Severity, const std::string& Message)
{
	std::string Text = File;
	if (Line > 0)
	{
		Text += "(" + std::to_string(Line) + ")";
	}
	Text += std::string(": ") + Severity + ": " + Message;
	if (bCapture)
	{
		Messages.push_back(Text);
	}
	else
	{
		std::fprintf(stderr, "%s\n", Text.c_str());
	}
}
