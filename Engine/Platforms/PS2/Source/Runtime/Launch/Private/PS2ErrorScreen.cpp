#include "PS2ErrorScreen.h"

#include "CoreGlobals.h"
#include "GSCommandList.h"
#include "GSDebugDraw.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CString.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "PS2RHI.h"

#include <kernel.h>

namespace
{

	constexpr int32 ScreenWidth = 640;
	constexpr int32 ScreenHeight = 448;
	constexpr float Margin = 24.0f;
	/** Body text at half scale: 1-pixel cells, 6 pixels a character, 7 tall. */
	constexpr float BodyScale = 0.5f;
	constexpr float BodyAdvance = 6.0f;
	constexpr float BodyLineHeight = 11.0f;
	constexpr int32 CharsPerLine = int32((float(ScreenWidth) - 2.0f * Margin) / BodyAdvance);

	/**
	 * The last error lines of the log, in fixed storage: the screen may be shown after the heap ran out, so nothing
	 * here allocates.
	 */
	class FErrorLines final : public FOutputDevice
	{
	public:
		static constexpr int32 MaxLines = 8;
		// Three screen lines each: a boot error names the folder it read and what to boot instead.
		static constexpr int32 MaxLineChars = 3 * CharsPerLine;

		void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& /*Category*/) override
		{
			if (V == nullptr || Verbosity > ELogVerbosity::Error)
			{
				return;
			}
			char* Line = Lines[Next % MaxLines];
			FCString::Strncpy(Line, V, SIZE_T(MaxLineChars));
			++Next;
		}

		bool CanBeUsedOnAnyThread() const override
		{
			return true;
		}

		/** The kept lines, oldest first. */
		template <typename FunctionType>
		void ForEach(FunctionType Function) const
		{
			const int32 First = Next > MaxLines ? Next - MaxLines : 0;
			for (int32 Index = First; Index < Next; ++Index)
			{
				Function(Lines[Index % MaxLines]);
			}
		}

	private:
		char Lines[MaxLines][MaxLineChars] = {};
		int32 Next = 0;
	};

	FErrorLines GErrorLines;

	/** Records Text from Y on (pixels from the top), wrapped at CharsPerLine; returns the Y below it. */
	float DrawWrapped(
		FGSCommandList& List, const FGSDrawEnvironment& Environment, float Y, const char* Text, const FGSRGBAQ& Color)
	{
		char Line[CharsPerLine + 1];
		const char* Cursor = Text;
		while (*Cursor != '\0')
		{
			int32 Length = 0;
			while (Cursor[Length] != '\0' && Cursor[Length] != '\n' && Length < CharsPerLine)
			{
				Line[Length] = Cursor[Length];
				++Length;
			}
			Line[Length] = '\0';
			FGSDebugDraw::DrawString(List, Environment, Margin, Y, Line, Color, BodyScale);
			Y += BodyLineHeight;
			Cursor += Length;
			if (*Cursor == '\n')
			{
				++Cursor;
			}
		}
		return Y;
	}

	void ShowForFatalExit(uint8 ReturnCode)
	{
		FPS2ErrorScreen::Show(int32(ReturnCode));
	}

} // namespace

void FPS2ErrorScreen::Install()
{
	GLog->AddOutputDevice(&GErrorLines);
	FPS2PlatformMisc::SetFatalExitHandler(&ShowForFatalExit);
}

void FPS2ErrorScreen::Show(int32 ExitCode)
{
	FPlatformMisc::LowLevelOutputDebugStringf("FPS2ErrorScreen: the game stopped (exit code %d)\n", int(ExitCode));
	if (!FPS2RHI::InitDisplay(ScreenWidth, ScreenHeight))
	{
		FPlatformMisc::LowLevelOutputDebugString("FPS2ErrorScreen: the display did not initialize; EE halted\n");
		for (;;)
		{
			SleepThread();
		}
	}

	char Title[64];
	FCString::Snprintf(Title, int32(sizeof(Title)), "THE GAME STOPPED (EXIT CODE %d)", int(ExitCode));
	const char* Project = FApp::HasProjectName() ? FApp::GetProjectName() : "<Project>";
	const char* BaseDir = FPlatformProcess::BaseDir();
	char Hint[512];
	if (FCString::Strnicmp(BaseDir, "host", 4) == 0)
	{
		FCString::Snprintf(Hint, int32(sizeof(Hint)),
			"The game reads its config and content from '%s' (the ELF's folder).\n"
			"PCSX2: Settings > Advanced > Enable Host Filesystem, then boot the staged ELF, not Binaries/PS2's:\n"
			"Game/%s/Packages/PS2/%s.elf (Package.bat) or Game/%s/Saved/StagedBuilds/PS2/%s.elf (BuildCookRun\n"
			"-stage -pak), beside its %s/Content/Paks/%s-PS2.lpak.\n"
			"The EE log has the whole log.",
			BaseDir, Project, Project, Project, Project, Project, Project);
	}
	else
	{
		FCString::Snprintf(Hint, int32(sizeof(Hint)),
			"The game reads its config and content from '%s' (the ELF's folder).\nThe EE log has the whole log.",
			BaseDir);
	}

	// One list recorded once (the screen does not change) and submitted every frame.
	const FGSDrawEnvironment Environment = FPS2RHI::GetDrawEnvironment();
	FGSCommandList List;
	FGSDebugDraw::DrawString(List, Environment, Margin, Margin, Title, FGSDebugDraw::UnitColor(1.0f, 0.9f, 0.6f));
	float Y = Margin + 28.0f;
	const FGSRGBAQ ErrorColor = FGSDebugDraw::UnitColor(1.0f, 0.75f, 0.75f);
	GErrorLines.ForEach([&List, &Environment, &Y, &ErrorColor](const char* Line)
		{ Y = DrawWrapped(List, Environment, Y, Line, ErrorColor) + 4.0f; });
	DrawWrapped(List, Environment, Y + 16.0f, Hint, FGSDebugDraw::UnitColor(0.85f, 0.85f, 0.95f));
	for (;;)
	{
		FPS2RHI::ClearColor(0.25f, 0.02f, 0.02f);
		FPS2RHI::Submit(List);
		FPS2RHI::WaitVSync();
	}
}
