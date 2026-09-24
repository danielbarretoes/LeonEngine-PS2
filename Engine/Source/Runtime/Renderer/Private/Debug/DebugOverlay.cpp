#include "Debug/DebugOverlay.h"

#include "Misc/Paths.h"
#include "OpenGLVertexAttrib.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4505) // unused statics in stb_easy_font.h
#endif
#include <stb_easy_font.h>
#ifdef _MSC_VER
	#pragma warning(pop)
#endif

namespace
{

	constexpr float HudPixelScale = 2.0f;
	constexpr float MessagePixelScale = 1.15f;
	constexpr float MarginX = 10.0f;
	constexpr float MarginY = 10.0f;
	constexpr float MessageLineStepY = 14.0f * MessagePixelScale;
	constexpr float FadeTailSeconds = 0.5f;
	constexpr std::size_t MaxOnScreenMessages = 12;

	struct FPackedVert
	{
		float X, Y, Z;
		std::array<unsigned char, 4> Rgba{};
	};

	struct FDrawVert
	{
		float X, Y, Z;
		float R, G, B, A;
	};

	void AppendTextMesh(std::vector<FDrawVert>& Tris, const std::string& InText, float OriginX, float OriginY,
		float InPixelScale, const std::array<unsigned char, 4>& InColor)
	{
		if (InText.empty())
		{
			return;
		}

		std::vector<char> FontBuf(InText.size() * 300 + 64);
		std::vector<char> MutableText(InText.begin(), InText.end());
		MutableText.push_back('\0');

		std::array<unsigned char, 4> ColorCopy = InColor;
		const int Quads = stb_easy_font_print(
			0.0f, 0.0f, MutableText.data(), ColorCopy.data(), FontBuf.data(), static_cast<int>(FontBuf.size()));
		if (Quads <= 0)
		{
			return;
		}

		const auto* Packed = reinterpret_cast<const FPackedVert*>(FontBuf.data());
		auto Push = [&](const FPackedVert& V)
		{
			FDrawVert Out{};
			Out.X = (V.X * InPixelScale) + OriginX;
			Out.Y = (V.Y * InPixelScale) + OriginY;
			Out.Z = 0.0f;
			Out.R = InColor[0] / 255.0f;
			Out.G = InColor[1] / 255.0f;
			Out.B = InColor[2] / 255.0f;
			Out.A = InColor[3] / 255.0f;
			Tris.push_back(Out);
		};

		for (int Q = 0; Q < Quads; ++Q)
		{
			Push(Packed[(Q * 4) + 0]);
			Push(Packed[(Q * 4) + 1]);
			Push(Packed[(Q * 4) + 2]);
			Push(Packed[(Q * 4) + 0]);
			Push(Packed[(Q * 4) + 2]);
			Push(Packed[(Q * 4) + 3]);
		}
	}

	void AppendRightAlignedLines(std::vector<FDrawVert>& Tris, const std::string& InText, int FramebufferWidth,
		float OriginY, float InPixelScale, const std::array<unsigned char, 4>& InColor)
	{
		if (InText.empty())
		{
			return;
		}

		float LocalY = OriginY;
		std::size_t Start = 0;
		while (Start <= InText.size())
		{
			const std::size_t End = InText.find('\n', Start);
			const std::size_t Count = (End == std::string::npos) ? (InText.size() - Start) : (End - Start);
			const std::string Line = InText.substr(Start, Count);

			if (!Line.empty())
			{
				std::vector<char> MutableLine(Line.begin(), Line.end());
				MutableLine.push_back('\0');
				const auto RawWidth = static_cast<float>(stb_easy_font_width(MutableLine.data()));
				const float OriginX = static_cast<float>(FramebufferWidth) - (RawWidth * InPixelScale) - MarginX;
				AppendTextMesh(Tris, Line, OriginX, LocalY, InPixelScale, InColor);
			}

			LocalY += 14.0f * InPixelScale;
			if (End == std::string::npos)
			{
				break;
			}
			Start = End + 1;
		}
	}

	void AppendCenterAlignedLines(std::vector<FDrawVert>& Tris, const std::string& InText, int FramebufferWidth,
		float OriginY, float InPixelScale, const std::array<unsigned char, 4>& InColor)
	{
		if (InText.empty())
		{
			return;
		}

		float LocalY = OriginY;
		std::size_t Start = 0;
		while (Start <= InText.size())
		{
			const std::size_t End = InText.find('\n', Start);
			const std::size_t Count = (End == std::string::npos) ? (InText.size() - Start) : (End - Start);
			const std::string Line = InText.substr(Start, Count);

			if (!Line.empty())
			{
				std::vector<char> MutableLine(Line.begin(), Line.end());
				MutableLine.push_back('\0');
				const auto RawWidth = static_cast<float>(stb_easy_font_width(MutableLine.data()));
				const float OriginX = (static_cast<float>(FramebufferWidth) - (RawWidth * InPixelScale)) * 0.5f;
				AppendTextMesh(Tris, Line, OriginX, LocalY, InPixelScale, InColor);
			}

			LocalY += 14.0f * InPixelScale;
			if (End == std::string::npos)
			{
				break;
			}
			Start = End + 1;
		}
	}

	/// Draw multiline text. `anchorX` is left / center / right of each line per `justify`.
	void AppendJustifiedLines(std::vector<FDrawVert>& Tris, const std::string& InText, float AnchorX, float OriginY,
		float InPixelScale, ETextJustify InJustify, const std::array<unsigned char, 4>& InColor)
	{
		if (InText.empty())
		{
			return;
		}

		float LocalY = OriginY;
		std::size_t Start = 0;
		while (Start <= InText.size())
		{
			const std::size_t End = InText.find('\n', Start);
			const std::size_t Count = (End == std::string::npos) ? (InText.size() - Start) : (End - Start);
			const std::string Line = InText.substr(Start, Count);

			if (!Line.empty())
			{
				std::vector<char> MutableLine(Line.begin(), Line.end());
				MutableLine.push_back('\0');
				const auto RawWidth = static_cast<float>(stb_easy_font_width(MutableLine.data()));
				const float LineW = RawWidth * InPixelScale;
				float OriginX = AnchorX;
				if (InJustify == ETextJustify::Center)
				{
					OriginX = AnchorX - (LineW * 0.5f);
				}
				else if (InJustify == ETextJustify::Right)
				{
					OriginX = AnchorX - LineW;
				}
				AppendTextMesh(Tris, Line, OriginX, LocalY, InPixelScale, InColor);
			}

			LocalY += 14.0f * InPixelScale;
			if (End == std::string::npos)
			{
				break;
			}
			Start = End + 1;
		}
	}

	/// Measure multiline HUD text: max line width (raw font units) + line count.
	void MeasureMultilineText(const std::string& InText, float& OutMaxRawWidth, int& OutLineCount)
	{
		OutMaxRawWidth = 0.0f;
		OutLineCount = 1;
		std::size_t Start = 0;
		while (Start <= InText.size())
		{
			const std::size_t End = InText.find('\n', Start);
			const std::size_t Count = (End == std::string::npos) ? (InText.size() - Start) : (End - Start);
			if (Count > 0)
			{
				std::vector<char> Line(InText.begin() + static_cast<std::ptrdiff_t>(Start),
					InText.begin() + static_cast<std::ptrdiff_t>(Start + Count));
				Line.push_back('\0');
				OutMaxRawWidth = std::max(OutMaxRawWidth, static_cast<float>(stb_easy_font_width(Line.data())));
			}
			if (End == std::string::npos)
			{
				break;
			}
			++OutLineCount;
			Start = End + 1;
		}
	}

	[[nodiscard]] std::array<unsigned char, 4> ColorWithAlpha(const glm::vec3& Rgb, float Alpha)
	{
		const float LocalA = std::clamp(Alpha, 0.0f, 1.0f);
		return {static_cast<unsigned char>(std::clamp(Rgb.r, 0.0f, 1.0f) * 255.0f),
			static_cast<unsigned char>(std::clamp(Rgb.g, 0.0f, 1.0f) * 255.0f),
			static_cast<unsigned char>(std::clamp(Rgb.b, 0.0f, 1.0f) * 255.0f),
			static_cast<unsigned char>(LocalA * 255.0f)};
	}

	void AppendScreenQuad(std::vector<FDrawVert>& Tris, float InX0, float InY0, float InX1, float InY1, float X2,
		float Y2, float X3, float Y3, const glm::vec3& InColor)
	{
		auto Push = [&](float InX, float InY)
		{
			FDrawVert Out{};
			Out.X = InX;
			Out.Y = InY;
			Out.Z = 0.0f;
			Out.R = InColor.r;
			Out.G = InColor.g;
			Out.B = InColor.b;
			Out.A = 1.0f;
			Tris.push_back(Out);
		};
		Push(InX0, InY0);
		Push(InX1, InY1);
		Push(X2, Y2);
		Push(InX0, InY0);
		Push(X2, Y2);
		Push(X3, Y3);
	}

	void AppendThickScreenLine(std::vector<FDrawVert>& Tris, float InX0, float InY0, float InX1, float InY1,
		float InThickness, const glm::vec3& InColor)
	{
		const float Dx = InX1 - InX0;
		const float Dy = InY1 - InY0;
		const float Len = std::sqrt((Dx * Dx) + (Dy * Dy));
		if (Len < 1.0e-4f)
		{
			return;
		}
		const float Hx = (-Dy / Len) * (InThickness * 0.5f);
		const float Hy = (Dx / Len) * (InThickness * 0.5f);
		AppendScreenQuad(
			Tris, InX0 - Hx, InY0 - Hy, InX0 + Hx, InY0 + Hy, InX1 + Hx, InY1 + Hy, InX1 - Hx, InY1 - Hy, InColor);
	}

} // namespace

bool FDebugOverlay::Initialize(const std::string& /*shaderDirectory*/)
{
	const std::string Vert = FPaths::ResolveAssetPath("assets/Shaders/debug_overlay.vert");
	const std::string Frag = FPaths::ResolveAssetPath("assets/Shaders/debug_overlay.frag");
	if (!Shader.LoadFromFiles(Vert, Frag))
	{
		std::cerr << "Failed to load debug overlay shaders\n";
		return false;
	}

	glGenVertexArrays(1, &Vao);
	glGenBuffers(1, &Vbo);
	glBindVertexArray(Vao);
	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FDrawVert), GlAttribOffset(&FDrawVert::X));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(FDrawVert), GlAttribOffset(&FDrawVert::R));
	glBindVertexArray(0);
	return true;
}

void FDebugOverlay::Shutdown()
{
	if (Vbo != 0)
	{
		glDeleteBuffers(1, &Vbo);
		Vbo = 0;
	}
	if (Vao != 0)
	{
		glDeleteVertexArrays(1, &Vao);
		Vao = 0;
	}
	Shader.Destroy();
	Text.clear();
	BottomLeftText.clear();
	CenterText.clear();
	RightText.clear();
	RightTextOriginY = MarginY;
	OnScreenMessages.clear();
	ScreenLines.clear();
	ScreenRects.clear();
	VertexCount = 0;
	bDirty = true;
	BuiltForWidth = 0;
	BuiltForHeight = 0;
}

void FDebugOverlay::SetText(const std::string& InText)
{
	if (Text == InText)
	{
		return;
	}
	Text = InText;
	bDirty = true;
}

void FDebugOverlay::SetBottomLeftText(const std::string& InText)
{
	if (BottomLeftText == InText)
	{
		return;
	}
	BottomLeftText = InText;
	bDirty = true;
}

void FDebugOverlay::SetCenterText(const std::string& InText)
{
	if (CenterText == InText)
	{
		return;
	}
	CenterText = InText;
	bDirty = true;
}

void FDebugOverlay::SetRightText(const std::string& InText)
{
	if (RightText == InText)
	{
		return;
	}
	RightText = InText;
	bDirty = true;
}

void FDebugOverlay::SetRightTextOriginY(float OriginY)
{
	if (RightTextOriginY == OriginY)
	{
		return;
	}
	RightTextOriginY = OriginY;
	bDirty = true;
}

void FDebugOverlay::AddOnScreenDebugMessage(std::string Message, float DisplaySeconds, const glm::vec3& InColor)
{
	if (Message.empty())
	{
		return;
	}
	const float LocalDuration = DisplaySeconds > 0.0f ? DisplaySeconds : 0.01f;
	OnScreenMessages.push_back(FOnScreenMessage{std::move(Message), LocalDuration, LocalDuration, InColor});
	while (OnScreenMessages.size() > MaxOnScreenMessages)
	{
		OnScreenMessages.erase(OnScreenMessages.begin());
	}
	bDirty = true;
}

void FDebugOverlay::TickOnScreenMessages(float DeltaTime)
{
	if (OnScreenMessages.empty())
	{
		return;
	}
	bool bChanged = false;
	for (FOnScreenMessage& Msg : OnScreenMessages)
	{
		Msg.TimeRemaining -= DeltaTime;
		bChanged = true;
	}
	const auto EraseIt = std::remove_if(OnScreenMessages.begin(), OnScreenMessages.end(),
		[](const FOnScreenMessage& Msg) { return Msg.TimeRemaining <= 0.0f; });
	if (EraseIt != OnScreenMessages.end())
	{
		OnScreenMessages.erase(EraseIt, OnScreenMessages.end());
		bChanged = true;
	}
	if (bChanged)
	{
		bDirty = true;
	}
}

void FDebugOverlay::ClearScreenGeometry()
{
	if (ScreenLines.empty() && ScreenRects.empty() && ScreenTexts.empty())
	{
		return;
	}
	ScreenLines.clear();
	ScreenRects.clear();
	ScreenTexts.clear();
	bDirty = true;
}

void FDebugOverlay::AddScreenLine(
	float InX0, float InY0, float InX1, float InY1, const glm::vec3& InColor, float InThickness)
{
	ScreenLines.push_back(FScreenLine{InX0, InY0, InX1, InY1, InThickness, InColor});
	bDirty = true;
}

void FDebugOverlay::AddScreenRect(float InX, float InY, float InW, float InH, const glm::vec3& InColor)
{
	ScreenRects.push_back(FScreenRect{InX, InY, InW, InH, InColor});
	bDirty = true;
}

void FDebugOverlay::AddScreenText(
	std::string InText, float InX, float InY, const glm::vec3& InColor, float InPixelScale, ETextJustify InJustify)
{
	if (InText.empty())
	{
		return;
	}
	ScreenTexts.push_back(FScreenText{std::move(InText), InX, InY, InPixelScale, InJustify, InColor});
	bDirty = true;
}

void FDebugOverlay::MeasureText(const std::string& InText, float InPixelScale, float& OutWidth, float& OutHeight)
{
	float MaxRaw = 0.0f;
	int Lines = 1;
	MeasureMultilineText(InText, MaxRaw, Lines);
	OutWidth = MaxRaw * InPixelScale;
	OutHeight = 14.0f * InPixelScale * static_cast<float>(Lines);
}

EShaderReloadResult FDebugOverlay::ReloadShader(bool bForce)
{
	return bForce ? Shader.ForceReloadFromDisk() : Shader.ReloadFromDiskIfChanged();
}

void FDebugOverlay::RebuildMesh(int FramebufferWidth, int FramebufferHeight)
{
	bDirty = false;
	BuiltForWidth = FramebufferWidth;
	BuiltForHeight = FramebufferHeight;
	VertexCount = 0;
	if (Vao == 0 ||
		(Text.empty() && BottomLeftText.empty() && CenterText.empty() && RightText.empty() &&
			OnScreenMessages.empty() && ScreenLines.empty() && ScreenRects.empty() && ScreenTexts.empty()))
	{
		return;
	}

	std::vector<FDrawVert> Tris;
	constexpr std::array<unsigned char, 4> LeftColor = {230, 235, 240, 255};
	constexpr std::array<unsigned char, 4> BottomLeftColor = {200, 210, 220, 255};
	constexpr std::array<unsigned char, 4> CenterColor = {255, 210, 90, 255};
	constexpr std::array<unsigned char, 4> RightColor = {240, 240, 245, 255};
	constexpr float LineStepY = 14.0f * HudPixelScale;

	// Top-left HUD block (FPS / tools).
	AppendTextMesh(Tris, Text, MarginX, MarginY, HudPixelScale, LeftColor);

	if (!BottomLeftText.empty())
	{
		int LineCount = 1;
		for (char C : BottomLeftText)
		{
			if (C == '\n')
			{
				++LineCount;
			}
		}
		const float OriginY =
			static_cast<float>(FramebufferHeight) - MarginY - (LineStepY * static_cast<float>(LineCount));
		AppendTextMesh(Tris, BottomLeftText, MarginX, OriginY, HudPixelScale, BottomLeftColor);
	}

	if (!CenterText.empty())
	{
		float MaxRawWidth = 0.0f;
		int LineCount = 1;
		MeasureMultilineText(CenterText, MaxRawWidth, LineCount);
		const float BlockH = 14.0f * HudPixelScale * static_cast<float>(LineCount);
		// Vertically center; clamp so short windows still keep the block on-screen.
		float OriginY = (static_cast<float>(FramebufferHeight) - BlockH) * 0.5f;
		OriginY =
			std::clamp(OriginY, MarginY, std::max(MarginY, static_cast<float>(FramebufferHeight) - BlockH - MarginY));
		// Each line centered — long Main Menu hints must not left-bias short rows.
		AppendCenterAlignedLines(Tris, CenterText, FramebufferWidth, OriginY, HudPixelScale, CenterColor);
	}

	// Right-aligned block (stats top-right / level chrome bottom-right).
	AppendRightAlignedLines(Tris, RightText, FramebufferWidth, RightTextOriginY, HudPixelScale, RightColor);

	// Top-left debug console: newest at the fixed top slot; older lines shift down (+Y).
	float LocalY = MarginY;
	for (auto It = OnScreenMessages.rbegin(); It != OnScreenMessages.rend(); ++It)
	{
		float Alpha = 1.0f;
		if (It->TimeRemaining < FadeTailSeconds)
		{
			Alpha = std::clamp(It->TimeRemaining / FadeTailSeconds, 0.0f, 1.0f);
		}
		AppendTextMesh(Tris, It->Text, MarginX, LocalY, MessagePixelScale, ColorWithAlpha(It->Color, Alpha));
		LocalY += MessageLineStepY;
		if (LocalY > static_cast<float>(FramebufferHeight) - MarginY)
		{
			break;
		}
	}

	// Screen widgets: panels/buttons first, then lines, then labels on top.
	// (Texts before rects hid VerticalBox labels under Button fills and under UImage.)
	for (const FScreenRect& Rect : ScreenRects)
	{
		AppendScreenQuad(Tris, Rect.X, Rect.Y, Rect.X + Rect.W, Rect.Y, Rect.X + Rect.W, Rect.Y + Rect.H, Rect.X,
			Rect.Y + Rect.H, Rect.Color);
	}
	for (const FScreenLine& Line : ScreenLines)
	{
		AppendThickScreenLine(Tris, Line.X0, Line.Y0, Line.X1, Line.Y1, Line.Thickness, Line.Color);
	}
	for (const FScreenText& Entry : ScreenTexts)
	{
		AppendJustifiedLines(
			Tris, Entry.Text, Entry.X, Entry.Y, Entry.PixelScale, Entry.Justify, ColorWithAlpha(Entry.Color, 1.0f));
	}

	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(
		GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(Tris.size() * sizeof(FDrawVert)), Tris.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	VertexCount = static_cast<int>(Tris.size());
}

void FDebugOverlay::Draw(int FramebufferWidth, int FramebufferHeight)
{
	if (!IsValid() || FramebufferWidth <= 0 || FramebufferHeight <= 0)
	{
		return;
	}

	if (bDirty || BuiltForWidth != FramebufferWidth || BuiltForHeight != FramebufferHeight)
	{
		RebuildMesh(FramebufferWidth, FramebufferHeight);
	}
	if (VertexCount <= 0)
	{
		return;
	}

	const glm::mat4 Projection = glm::ortho(
		0.0f, static_cast<float>(FramebufferWidth), static_cast<float>(FramebufferHeight), 0.0f, -1.0f, 1.0f);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Shader.Bind();
	Shader.SetMat4("uProjection", glm::value_ptr(Projection));
	glBindVertexArray(Vao);
	glDrawArrays(GL_TRIANGLES, 0, VertexCount);
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}
