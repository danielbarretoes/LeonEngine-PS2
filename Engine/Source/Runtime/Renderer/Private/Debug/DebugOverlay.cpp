#include "Debug/DebugOverlay.h"

#include "LegacyGLMath.h"
#include "Misc/Paths.h"
#include "OpenGLVertexAttrib.h"
#include "RendererLog.h"

#include <glad/glad.h>

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
	constexpr int32 MaxOnScreenMessages = 12;

	struct FRgba8
	{
		uint8 V[4] = {};
	};

	struct FPackedVert
	{
		float X, Y, Z;
		uint8 Rgba[4];
	};

	struct FDrawVert
	{
		float X, Y, Z;
		float R, G, B, A;
	};

	/** stb_easy_font takes mutable, null-terminated text: copy [Begin, Begin + Count) into Scratch. */
	ANSICHAR* TerminatedCopy(TArray<ANSICHAR>& Scratch, const ANSICHAR* Begin, int32 Count)
	{
		Scratch.SetNumUninitialized(Count + 1);
		FMemory::Memcpy(Scratch.GetData(), Begin, Count);
		Scratch[Count] = '\0';
		return Scratch.GetData();
	}

	/** Calls Visit(Begin, Count) for every '\n'-separated line of Text (including empty ones). */
	template <typename VisitorType>
	void ForEachLine(const FString& Text, const VisitorType& Visit)
	{
		const ANSICHAR* Cursor = *Text;
		for (;;)
		{
			const ANSICHAR* End = Cursor;
			while (*End != '\0' && *End != '\n')
			{
				++End;
			}
			Visit(Cursor, static_cast<int32>(End - Cursor));
			if (*End == '\0')
			{
				break;
			}
			Cursor = End + 1;
		}
	}

	float RawTextWidth(const ANSICHAR* Begin, int32 Count)
	{
		TArray<ANSICHAR> Scratch;
		return static_cast<float>(stb_easy_font_width(TerminatedCopy(Scratch, Begin, Count)));
	}

	void AppendTextMesh(TArray<FDrawVert>& Tris, const ANSICHAR* InText, int32 Count, float OriginX, float OriginY,
		float InPixelScale, const FRgba8& InColor)
	{
		if (Count <= 0)
		{
			return;
		}

		TArray<ANSICHAR> MutableText;
		TArray<ANSICHAR> FontBuf;
		FontBuf.SetNumZeroed(Count * 300 + 64);
		FRgba8 ColorCopy = InColor;
		const int32 Quads = stb_easy_font_print(
			0.0f, 0.0f, TerminatedCopy(MutableText, InText, Count), ColorCopy.V, FontBuf.GetData(), FontBuf.Num());
		if (Quads <= 0)
		{
			return;
		}

		const auto* Packed = reinterpret_cast<const FPackedVert*>(FontBuf.GetData());
		auto Push = [&](const FPackedVert& V)
		{
			FDrawVert Out{};
			Out.X = (V.X * InPixelScale) + OriginX;
			Out.Y = (V.Y * InPixelScale) + OriginY;
			Out.Z = 0.0f;
			Out.R = InColor.V[0] / 255.0f;
			Out.G = InColor.V[1] / 255.0f;
			Out.B = InColor.V[2] / 255.0f;
			Out.A = InColor.V[3] / 255.0f;
			Tris.Add(Out);
		};

		for (int32 Q = 0; Q < Quads; ++Q)
		{
			Push(Packed[(Q * 4) + 0]);
			Push(Packed[(Q * 4) + 1]);
			Push(Packed[(Q * 4) + 2]);
			Push(Packed[(Q * 4) + 0]);
			Push(Packed[(Q * 4) + 2]);
			Push(Packed[(Q * 4) + 3]);
		}
	}

	void AppendTextMesh(TArray<FDrawVert>& Tris, const FString& InText, float OriginX, float OriginY,
		float InPixelScale, const FRgba8& InColor)
	{
		AppendTextMesh(Tris, *InText, InText.Len(), OriginX, OriginY, InPixelScale, InColor);
	}

	/** Draws multiline text. AnchorX is the left / center / right of each line per InJustify. */
	void AppendJustifiedLines(TArray<FDrawVert>& Tris, const FString& InText, float AnchorX, float OriginY,
		float InPixelScale, ETextJustify InJustify, const FRgba8& InColor)
	{
		if (InText.IsEmpty())
		{
			return;
		}

		float LocalY = OriginY;
		ForEachLine(InText,
			[&](const ANSICHAR* Line, int32 Count)
			{
				if (Count > 0)
				{
					const float LineW = RawTextWidth(Line, Count) * InPixelScale;
					float OriginX = AnchorX;
					if (InJustify == ETextJustify::Center)
					{
						OriginX = AnchorX - (LineW * 0.5f);
					}
					else if (InJustify == ETextJustify::Right)
					{
						OriginX = AnchorX - LineW;
					}
					AppendTextMesh(Tris, Line, Count, OriginX, LocalY, InPixelScale, InColor);
				}
				LocalY += 14.0f * InPixelScale;
			});
	}

	void AppendRightAlignedLines(TArray<FDrawVert>& Tris, const FString& InText, int32 FramebufferWidth, float OriginY,
		float InPixelScale, const FRgba8& InColor)
	{
		AppendJustifiedLines(Tris, InText, static_cast<float>(FramebufferWidth) - MarginX, OriginY, InPixelScale,
			ETextJustify::Right, InColor);
	}

	void AppendCenterAlignedLines(TArray<FDrawVert>& Tris, const FString& InText, int32 FramebufferWidth, float OriginY,
		float InPixelScale, const FRgba8& InColor)
	{
		AppendJustifiedLines(Tris, InText, static_cast<float>(FramebufferWidth) * 0.5f, OriginY, InPixelScale,
			ETextJustify::Center, InColor);
	}

	/** Measures multiline HUD text: max line width (raw font units) + line count. */
	void MeasureMultilineText(const FString& InText, float& OutMaxRawWidth, int32& OutLineCount)
	{
		OutMaxRawWidth = 0.0f;
		OutLineCount = 0;
		ForEachLine(InText,
			[&](const ANSICHAR* Line, int32 Count)
			{
				if (Count > 0)
				{
					OutMaxRawWidth = FMath::Max(OutMaxRawWidth, RawTextWidth(Line, Count));
				}
				++OutLineCount;
			});
	}

	[[nodiscard]] FRgba8 ColorWithAlpha(const FLinearColor& Rgb, float Alpha)
	{
		FRgba8 Out;
		Out.V[0] = static_cast<uint8>(FMath::Clamp(Rgb.R, 0.0f, 1.0f) * 255.0f);
		Out.V[1] = static_cast<uint8>(FMath::Clamp(Rgb.G, 0.0f, 1.0f) * 255.0f);
		Out.V[2] = static_cast<uint8>(FMath::Clamp(Rgb.B, 0.0f, 1.0f) * 255.0f);
		Out.V[3] = static_cast<uint8>(FMath::Clamp(Alpha, 0.0f, 1.0f) * 255.0f);
		return Out;
	}

	[[nodiscard]] constexpr FRgba8 Rgba8(uint8 R, uint8 G, uint8 B)
	{
		return FRgba8{{R, G, B, 255}};
	}

	void AppendScreenQuad(TArray<FDrawVert>& Tris, float InX0, float InY0, float InX1, float InY1, float X2, float Y2,
		float X3, float Y3, const FLinearColor& InColor)
	{
		auto Push = [&](float InX, float InY)
		{
			FDrawVert Out{};
			Out.X = InX;
			Out.Y = InY;
			Out.Z = 0.0f;
			Out.R = InColor.R;
			Out.G = InColor.G;
			Out.B = InColor.B;
			Out.A = 1.0f;
			Tris.Add(Out);
		};
		Push(InX0, InY0);
		Push(InX1, InY1);
		Push(X2, Y2);
		Push(InX0, InY0);
		Push(X2, Y2);
		Push(X3, Y3);
	}

	void AppendThickScreenLine(TArray<FDrawVert>& Tris, float InX0, float InY0, float InX1, float InY1,
		float InThickness, const FLinearColor& InColor)
	{
		const float Dx = InX1 - InX0;
		const float Dy = InY1 - InY0;
		const float Len = FMath::Sqrt((Dx * Dx) + (Dy * Dy));
		if (Len < 1.0e-4f)
		{
			return;
		}
		const float Hx = (-Dy / Len) * (InThickness * 0.5f);
		const float Hy = (Dx / Len) * (InThickness * 0.5f);
		AppendScreenQuad(
			Tris, InX0 - Hx, InY0 - Hy, InX0 + Hx, InY0 + Hy, InX1 + Hx, InY1 + Hy, InX1 - Hx, InY1 - Hy, InColor);
	}

	/** Replaces Target when it differs (case-sensitive); returns true on change. */
	bool AssignIfChanged(FString& Target, const FString& Value)
	{
		if (Target.Equals(Value, ESearchCase::CaseSensitive))
		{
			return false;
		}
		Target = Value;
		return true;
	}

} // namespace

bool FDebugOverlay::Initialize(const FString& /*ShaderDirectory*/)
{
	const FString Vert = FPaths::ResolveLegacyContentPath("assets/Shaders/debug_overlay.vert");
	const FString Frag = FPaths::ResolveLegacyContentPath("assets/Shaders/debug_overlay.frag");
	if (!Shader.LoadFromFiles(Vert, Frag))
	{
		UE_LOG(LogRenderer, Error, "Failed to load debug overlay shaders");
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
	Text.Empty();
	BottomLeftText.Empty();
	CenterText.Empty();
	RightText.Empty();
	RightTextOriginY = MarginY;
	OnScreenMessages.Empty();
	ScreenLines.Empty();
	ScreenRects.Empty();
	VertexCount = 0;
	bDirty = true;
	BuiltForWidth = 0;
	BuiltForHeight = 0;
}

void FDebugOverlay::SetText(const FString& InText)
{
	bDirty |= AssignIfChanged(Text, InText);
}

void FDebugOverlay::SetBottomLeftText(const FString& InText)
{
	bDirty |= AssignIfChanged(BottomLeftText, InText);
}

void FDebugOverlay::SetCenterText(const FString& InText)
{
	bDirty |= AssignIfChanged(CenterText, InText);
}

void FDebugOverlay::SetRightText(const FString& InText)
{
	bDirty |= AssignIfChanged(RightText, InText);
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

void FDebugOverlay::AddOnScreenDebugMessage(const FString& Message, float DisplaySeconds, const FLinearColor& InColor)
{
	if (Message.IsEmpty())
	{
		return;
	}
	const float LocalDuration = DisplaySeconds > 0.0f ? DisplaySeconds : 0.01f;
	OnScreenMessages.Add(FOnScreenMessage{Message, LocalDuration, LocalDuration, InColor});
	if (OnScreenMessages.Num() > MaxOnScreenMessages)
	{
		OnScreenMessages.RemoveAt(0, OnScreenMessages.Num() - MaxOnScreenMessages);
	}
	bDirty = true;
}

void FDebugOverlay::TickOnScreenMessages(float DeltaTime)
{
	if (OnScreenMessages.Num() == 0)
	{
		return;
	}
	for (FOnScreenMessage& Msg : OnScreenMessages)
	{
		Msg.TimeRemaining -= DeltaTime;
	}
	OnScreenMessages.RemoveAll([](const FOnScreenMessage& Msg) { return Msg.TimeRemaining <= 0.0f; });
	bDirty = true;
}

void FDebugOverlay::ClearScreenGeometry()
{
	if (ScreenLines.Num() == 0 && ScreenRects.Num() == 0 && ScreenTexts.Num() == 0)
	{
		return;
	}
	ScreenLines.Reset();
	ScreenRects.Reset();
	ScreenTexts.Reset();
	bDirty = true;
}

void FDebugOverlay::AddScreenLine(
	float InX0, float InY0, float InX1, float InY1, const FLinearColor& InColor, float InThickness)
{
	ScreenLines.Add(FScreenLine{InX0, InY0, InX1, InY1, InThickness, InColor});
	bDirty = true;
}

void FDebugOverlay::AddScreenRect(float InX, float InY, float InW, float InH, const FLinearColor& InColor)
{
	ScreenRects.Add(FScreenRect{InX, InY, InW, InH, InColor});
	bDirty = true;
}

void FDebugOverlay::AddScreenText(const FString& InText, float InX, float InY, const FLinearColor& InColor,
	float InPixelScale, ETextJustify InJustify)
{
	if (InText.IsEmpty())
	{
		return;
	}
	ScreenTexts.Add(FScreenText{InText, InX, InY, InPixelScale, InJustify, InColor});
	bDirty = true;
}

void FDebugOverlay::MeasureText(const FString& InText, float InPixelScale, float& OutWidth, float& OutHeight)
{
	float MaxRaw = 0.0f;
	int32 Lines = 1;
	MeasureMultilineText(InText, MaxRaw, Lines);
	OutWidth = MaxRaw * InPixelScale;
	OutHeight = 14.0f * InPixelScale * static_cast<float>(Lines);
}

EShaderReloadResult FDebugOverlay::ReloadShader(bool bForce)
{
	return bForce ? Shader.ForceReloadFromDisk() : Shader.ReloadFromDiskIfChanged();
}

void FDebugOverlay::RebuildMesh(int32 FramebufferWidth, int32 FramebufferHeight)
{
	bDirty = false;
	BuiltForWidth = FramebufferWidth;
	BuiltForHeight = FramebufferHeight;
	VertexCount = 0;
	if (Vao == 0 ||
		(Text.IsEmpty() && BottomLeftText.IsEmpty() && CenterText.IsEmpty() && RightText.IsEmpty() &&
			OnScreenMessages.Num() == 0 && ScreenLines.Num() == 0 && ScreenRects.Num() == 0 && ScreenTexts.Num() == 0))
	{
		return;
	}

	TArray<FDrawVert> Tris;
	constexpr FRgba8 LeftColor = Rgba8(230, 235, 240);
	constexpr FRgba8 BottomLeftColor = Rgba8(200, 210, 220);
	constexpr FRgba8 CenterColor = Rgba8(255, 210, 90);
	constexpr FRgba8 RightColor = Rgba8(240, 240, 245);
	constexpr float LineStepY = 14.0f * HudPixelScale;

	// Top-left HUD block (FPS / tools).
	AppendTextMesh(Tris, Text, MarginX, MarginY, HudPixelScale, LeftColor);

	if (!BottomLeftText.IsEmpty())
	{
		int32 LineCount = 1;
		for (const ANSICHAR* C = *BottomLeftText; *C != '\0'; ++C)
		{
			if (*C == '\n')
			{
				++LineCount;
			}
		}
		const float OriginY =
			static_cast<float>(FramebufferHeight) - MarginY - (LineStepY * static_cast<float>(LineCount));
		AppendTextMesh(Tris, BottomLeftText, MarginX, OriginY, HudPixelScale, BottomLeftColor);
	}

	if (!CenterText.IsEmpty())
	{
		float MaxRawWidth = 0.0f;
		int32 LineCount = 1;
		MeasureMultilineText(CenterText, MaxRawWidth, LineCount);
		const float BlockH = 14.0f * HudPixelScale * static_cast<float>(LineCount);
		// Vertically center; clamp so short windows still keep the block on-screen.
		float OriginY = (static_cast<float>(FramebufferHeight) - BlockH) * 0.5f;
		OriginY = FMath::Clamp(
			OriginY, MarginY, FMath::Max(MarginY, static_cast<float>(FramebufferHeight) - BlockH - MarginY));
		// Each line centered — long Main Menu hints must not left-bias short rows.
		AppendCenterAlignedLines(Tris, CenterText, FramebufferWidth, OriginY, HudPixelScale, CenterColor);
	}

	// Right-aligned block (stats top-right / level chrome bottom-right).
	AppendRightAlignedLines(Tris, RightText, FramebufferWidth, RightTextOriginY, HudPixelScale, RightColor);

	// Top-left debug console: newest at the fixed top slot; older lines shift down (+Y).
	float LocalY = MarginY;
	for (int32 Index = OnScreenMessages.Num() - 1; Index >= 0; --Index)
	{
		const FOnScreenMessage& Msg = OnScreenMessages[Index];
		float Alpha = 1.0f;
		if (Msg.TimeRemaining < FadeTailSeconds)
		{
			Alpha = FMath::Clamp(Msg.TimeRemaining / FadeTailSeconds, 0.0f, 1.0f);
		}
		AppendTextMesh(Tris, Msg.Text, MarginX, LocalY, MessagePixelScale, ColorWithAlpha(Msg.Color, Alpha));
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
		GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(Tris.Num() * sizeof(FDrawVert)), Tris.GetData(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	VertexCount = Tris.Num();
}

void FDebugOverlay::Draw(int32 FramebufferWidth, int32 FramebufferHeight)
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

	const FMatrix Projection = LegacyGL::Ortho(
		0.0f, static_cast<float>(FramebufferWidth), static_cast<float>(FramebufferHeight), 0.0f, -1.0f, 1.0f);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Shader.Bind();
	Shader.SetMat4("uProjection", LegacyGL::ValuePtr(Projection));
	glBindVertexArray(Vao);
	glDrawArrays(GL_TRIANGLES, 0, VertexCount);
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}
