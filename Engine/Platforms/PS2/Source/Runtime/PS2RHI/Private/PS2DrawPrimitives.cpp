#include "HAL/PlatformMath.h"
#include "PS2GSContext.h"
#include "PS2RHI.h"

#include <cstdint>
#include <cstring>
#include <dma.h>
#include <draw2d.h>
#include <draw_blending.h>
#include <draw_tests.h>

namespace
{

#pragma pack(push, 1)
	struct Ps2MeshHeader
	{
		char Magic[4]; // LPS2
		std::uint32_t Version;
		std::uint32_t VertexCount;
		std::uint32_t IndexCount;
	};
#pragma pack(pop)

	void FillVertex(vertex_t& Out, float X, float Y)
	{
		Out.x = X;
		Out.y = Y;
		Out.z = 0;
	}

	void FillColor(color_t& Out, float R, float G, float B)
	{
		Out.r = static_cast<unsigned char>(static_cast<int>(R * 255.0f) & 0xFF);
		Out.g = static_cast<unsigned char>(static_cast<int>(G * 255.0f) & 0xFF);
		Out.b = static_cast<unsigned char>(static_cast<int>(B * 255.0f) & 0xFF);
		Out.a = 0x80;
		Out.q = 1.0f;
	}

	[[nodiscard]] bool SubmitPacket(Leon::PS2::FPS2GSContext& Gs, qword_t* End)
	{
		if (Gs.Packet == nullptr || End <= Gs.Packet->data)
		{
			return false;
		}
		dma_channel_send_normal(DMA_CHANNEL_GIF, Gs.Packet->data, End - Gs.Packet->data, 0, 0);
		dma_wait_fast();
		draw_wait_finish();
		return true;
	}

	[[nodiscard]] bool IsDisplayReady(const Leon::PS2::FPS2GSContext& Gs)
	{
		return Gs.bReady && Gs.Packet != nullptr && Gs.Frame.width > 0;
	}

	void RotatePoint(float CenterX, float CenterY, float Lx, float Ly, float CosA, float SinA, float& OutX, float& OutY)
	{
		OutX = CenterX + Lx * CosA - Ly * SinA;
		OutY = CenterY + Lx * SinA + Ly * CosA;
	}

} // namespace

bool FPS2RHI::DrawUnlitTriangleAt(
	float CenterX, float CenterY, float Size, unsigned Angle256, float R, float G, float B)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!IsDisplayReady(Gs) || Size <= 0.0f)
	{
		return false;
	}

	// draw_triangle_filled adds +2048; with XYOFFSET at (2048-w/2, 2048-h/2),
	// drawable space is centered on (0,0).
	const float CosA = FPlatformMath::Cos256(Angle256);
	const float SinA = FPlatformMath::Sin256(Angle256);

	float X0 = 0.0f;
	float Y0 = 0.0f;
	float X1 = 0.0f;
	float Y1 = 0.0f;
	float X2 = 0.0f;
	float Y2 = 0.0f;
	RotatePoint(CenterX, CenterY, 0.0f, -Size, CosA, SinA, X0, Y0);
	RotatePoint(CenterX, CenterY, -Size * 0.9f, Size * 0.75f, CosA, SinA, X1, Y1);
	RotatePoint(CenterX, CenterY, Size * 0.9f, Size * 0.75f, CosA, SinA, X2, Y2);

	triangle_t Tri{};
	FillColor(Tri.color, R, G, B);
	FillVertex(Tri.v0, X0, Y0);
	FillVertex(Tri.v1, X1, Y1);
	FillVertex(Tri.v2, X2, Y2);

	qword_t* Q = Gs.Packet->data;
	Q = draw_triangle_filled(Q, 0, &Tri);
	Q = draw_finish(Q);
	return SubmitPacket(Gs, Q);
}

bool FPS2RHI::DrawUnlitRect(float X0, float Y0, float X1, float Y1, float R, float G, float B)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!IsDisplayReady(Gs))
	{
		return false;
	}
	if (X1 < X0)
	{
		const float T = X0;
		X0 = X1;
		X1 = T;
	}
	if (Y1 < Y0)
	{
		const float T = Y0;
		Y0 = Y1;
		Y1 = T;
	}

	rect_t Rect{};
	FillColor(Rect.color, R, G, B);
	FillVertex(Rect.v0, X0, Y0);
	FillVertex(Rect.v1, X1, Y1);

	qword_t* Q = Gs.Packet->data;
	// Overlay: ignore z so terrain cannot cover HUD / 2D chrome.
	Q = draw_disable_tests(Q, 0, &Gs.Z);
	Q = draw_rect_filled(Q, 0, &Rect);
	Q = draw_enable_tests(Q, 0, &Gs.Z);
	Q = draw_finish(Q);
	return SubmitPacket(Gs, Q);
}

bool FPS2RHI::DrawUnlitRectAlpha(float X0, float Y0, float X1, float Y1, float R, float G, float B, float Alpha)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!IsDisplayReady(Gs))
	{
		return false;
	}
	if (X1 < X0)
	{
		const float T = X0;
		X0 = X1;
		X1 = T;
	}
	if (Y1 < Y0)
	{
		const float T = Y0;
		Y0 = Y1;
		Y1 = T;
	}
	Alpha = Alpha < 0.0f ? 0.0f : (Alpha > 1.0f ? 1.0f : Alpha);

	rect_t Rect{};
	FillColor(Rect.color, R, G, B);
	// GS alpha: 0x80 = 1.0. Keep >= 1 so ATEST (A != 0) never discards the sprite.
	const int A = static_cast<int>(Alpha * 128.0f + 0.5f);
	Rect.color.a = static_cast<unsigned char>(A < 1 ? 1 : A);
	FillVertex(Rect.v0, X0, Y0);
	FillVertex(Rect.v1, X1, Y1);

	// (Cs - Cd) * As + Cd. libdraw bakes PRIM.ABE from a global flag inside draw_rect_filled,
	// so enable it only around this sprite (everything else stays opaque).
	blend_t Blend{};
	Blend.color1 = BLEND_COLOR_SOURCE;
	Blend.color2 = BLEND_COLOR_DEST;
	Blend.alpha = BLEND_ALPHA_SOURCE;
	Blend.color3 = BLEND_COLOR_DEST;
	Blend.fixed_alpha = 0x80;

	qword_t* Q = Gs.Packet->data;
	Q = draw_disable_tests(Q, 0, &Gs.Z);
	Q = draw_alpha_blending(Q, 0, &Blend);
	draw_enable_blending();
	Q = draw_rect_filled(Q, 0, &Rect);
	draw_disable_blending();
	Q = draw_enable_tests(Q, 0, &Gs.Z);
	Q = draw_finish(Q);
	return SubmitPacket(Gs, Q);
}

bool FPS2RHI::DrawUnlitTriangle()
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!IsDisplayReady(Gs))
	{
		return false;
	}
	const float Size = static_cast<float>(Gs.Frame.height) * 0.28f;
	return FPS2RHI::DrawUnlitTriangleAt(0.0f, 0.0f, Size, 0, 1.0f, 0.784f, 0.125f);
}

bool FPS2RHI::DrawCookedMesh(const void* Data, unsigned Size)
{
	if (Data == nullptr || Size < sizeof(Ps2MeshHeader))
	{
		return false;
	}
	Ps2MeshHeader Header{};
	std::memcpy(&Header, Data, sizeof(Header));
	if (std::memcmp(Header.Magic, "LPS2", 4) != 0 || Header.Version != 1)
	{
		return false;
	}
	if (Header.VertexCount == 0)
	{
		return false;
	}
	// Full LPS2 vertex upload is not wired yet; keep a visible GS result for cook smoke.
	return FPS2RHI::DrawUnlitTriangle();
}
