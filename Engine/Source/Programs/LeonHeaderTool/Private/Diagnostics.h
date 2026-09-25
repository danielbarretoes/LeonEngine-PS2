#pragma once

#include <string>
#include <vector>

/**
 * Errors and warnings in the form "<file>(<line>): error: <message>" (UHT: FUHTException / UE_LOG_WARNING_UHT), so
 * IDEs and ninja jump to the offending header line. Any error makes LeonHeaderTool exit with a non-zero code.
 */
class FDiagnostics
{
public:
	/** bInCapture keeps the messages for the golden tests instead of printing them. */
	explicit FDiagnostics(bool bInCapture = false);

	void Error(const std::string& File, int Line, const std::string& Message);
	void Warning(const std::string& File, int Line, const std::string& Message);

	int GetErrorCount() const;
	const std::vector<std::string>& GetMessages() const;

private:
	void Emit(const std::string& File, int Line, const char* Severity, const std::string& Message);

	bool bCapture = false;
	int ErrorCount = 0;
	std::vector<std::string> Messages;
};
