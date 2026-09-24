#pragma once

#include "Debug/DebugOverlay.h"
#include "Level/LevelAnimation.h"
#include "Level/LevelCatalog.h"

#include <string>
#include <string_view>

class UGameEngine;

/// Scans game levels, loads them into an Engine, and draws a bottom-right browser
/// (`< name (i/n) >`). Switch with `[` / `]`, digit keys `1`–`9`, or mouse on the arrows.
class ENGINE_API FLevelDirector
{
public:
	bool Initialize(const std::string& ShaderDirectory);
	void Shutdown();
	[[nodiscard]] EShaderReloadResult ReloadShaders(bool bForce = false);

	/// Discover `.llev` levels under `projectsRoot/<pack>/Content/Levels/` and load the first.
	bool ScanAndLoad(UGameEngine& Engine, const std::string& ProjectsDirectory);

	/// Discover levels for a single project folder (`Projects/<pack>/`) and load
	/// `preferredLevelKey` when set (FLevelEntry.name / stem); otherwise the first entry.
	bool ScanPackAndLoad(
		UGameEngine& Engine, const std::string& PackDirectory, std::string_view PreferredLevelKey = {});

	/// Load by catalog index. On failure the previous Level contents may be cleared;
	/// CurrentIndex is only updated after a successful load.
	bool LoadIndex(UGameEngine& Engine, std::size_t Index);
	/// Load by FLevelEntry.name / path stem (net travel key). Returns false if unknown.
	bool LoadByKey(UGameEngine& Engine, std::string_view LevelKey);
	bool Next(UGameEngine& Engine);
	bool Previous(UGameEngine& Engine);

	/// Apply spin / bob / point-light orbit from the loaded Level.
	void Update(UGameEngine& Engine, float DeltaTime);

	/// Draw the bottom-right chrome after the 3D + stats pass.
	void DrawUi(int FramebufferWidth, int FramebufferHeight);

	/// Handle `[` `]`, digits `1`–`9`, and clicks on `<` `>`.
	/// Returns true while camera drag should be blocked.
	bool HandleUiInput(UGameEngine& Engine);

	[[nodiscard]] bool IsEmpty() const
	{
		return Catalog.IsEmpty();
	}
	[[nodiscard]] std::size_t GetCurrentIndex() const
	{
		return CurrentIndex;
	}
	[[nodiscard]] const FLevelCatalog& GetCatalog() const
	{
		return Catalog;
	}

	/// When false, `[`/`]` chrome and input are disabled (menus / shipping UI).
	void SetBrowserVisible(bool bVisible)
	{
		bBrowserVisible = bVisible;
	}
	[[nodiscard]] bool IsBrowserVisible() const
	{
		return bBrowserVisible;
	}

private:
	void RefreshChrome(int FramebufferWidth, int FramebufferHeight);
	void LayoutChrome(int FramebufferWidth, int FramebufferHeight);
	[[nodiscard]] bool HitPrev(float X, float Y) const;
	[[nodiscard]] bool HitNext(float X, float Y) const;
	void CursorFramebuffer(UGameEngine& Engine, float& OutX, float& OutY) const;

	FLevelCatalog Catalog;
	FLevelAnimation Animation;
	FDebugOverlay Chrome;

	std::size_t CurrentIndex = 0;
	float Elapsed = 0.0f;

	float PrevMinX = 0.0f;
	float PrevMaxX = 0.0f;
	float NextMinX = 0.0f;
	float NextMaxX = 0.0f;
	float ChromeMinY = 0.0f;
	float ChromeMaxY = 0.0f;
	int LayoutFbWidth = 0;
	int LayoutFbHeight = 0;

	bool bMouseWasDown = false;
	bool bIgnoreDrag = false;
	bool bKeyPrevDown = false;
	bool bKeyNextDown = false;
	bool DigitWasDown[9] = {};
	bool bBrowserVisible = true;
};
