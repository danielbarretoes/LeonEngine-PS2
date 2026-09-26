#include "PS2ErrorScreen.h"

#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CString.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "PS2RHI.h"

#include <cstdio>
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
		static constexpr int32 MaxLineChars = 2 * CharsPerLine;

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

	/** Draws Text from Y on, wrapped at CharsPerLine; returns the Y below it. */
	float DrawWrapped(float Y, const char* Text, float R, float G, float B)
	{
		const float Left = -float(ScreenWidth) * 0.5f + Margin;
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
			FPS2RHI::DrawDebugText(Left, Y, Line, R, G, B, BodyScale);
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
	std::printf("FPS2ErrorScreen: the game stopped (exit code %d)\n", int(ExitCode));
	if (!FPS2RHI::InitDisplay(ScreenWidth, ScreenHeight))
	{
		std::printf("FPS2ErrorScreen: the display did not initialize; EE halted\n");
		for (;;)
		{
			SleepThread();
		}
	}

	char Title[64];
	std::snprintf(Title, sizeof(Title), "THE GAME STOPPED (EXIT CODE %d)", int(ExitCode));
	const char* Project = FApp::HasProjectName() ? FApp::GetProjectName() : "<Project>";
	const char* BaseDir = FPlatformProcess::BaseDir();
	char Hint[512];
	if (FCString::Strnicmp(BaseDir, "host", 4) == 0)
	{
		std::snprintf(Hint, sizeof(Hint),
			"The game reads its config and content from '%s' (the ELF's folder).\n"
			"PCSX2: Settings > Advanced > Enable Host Filesystem, then boot the staged ELF, not Binaries/PS2's:\n"
			"Game/%s/Packages/PS2/%s.elf (Package.bat) or Game/%s/Saved/StagedBuilds/PS2/%s.elf (BuildCookRun\n"
			"-stage -pak), beside its %s/Content/Paks/%s-PS2.lpak.\n"
			"The EE log has the whole log.",
			BaseDir, Project, Project, Project, Project, Project, Project);
	}
	else
	{
		std::snprintf(Hint, sizeof(Hint),
			"The game reads its config and content from '%s' (the ELF's folder).\nThe EE log has the whole log.",
			BaseDir);
	}

	const float Left = -float(ScreenWidth) * 0.5f + Margin;
	const float Top = -float(ScreenHeight) * 0.5f + Margin;
	for (;;)
	{
		FPS2RHI::ClearColor(0.25f, 0.02f, 0.02f);
		FPS2RHI::DrawDebugText(Left, Top, Title, 1.0f, 0.9f, 0.6f, 1.0f);
		float Y = Top + 28.0f;
		GErrorLines.ForEach([&Y](const char* Line) { Y = DrawWrapped(Y, Line, 1.0f, 0.75f, 0.75f) + 4.0f; });
		DrawWrapped(Y + 16.0f, Hint, 0.85f, 0.85f, 0.95f);
		FPS2RHI::WaitVSync();
	}
}
