#include "DynamicRHI.h"
#include "HAL/PlatformMath.h"
#include "Math/UnrealMathUtility.h"
#include "PS2GSContext.h"
#include "PS2RHI.h"
#include "PS2SceneState.h"

#include <graph.h>
#include <math3d.h>

namespace
{

	FPS2Draw3DStats GDraw3DDebug{};

	constexpr float TwoPi = 6.28318530718f;
	constexpr int FaceCount = 6;
	constexpr int VertsPerFace = 4;
	constexpr int CubeVertexCount = FaceCount * VertsPerFace;

	constexpr float FaceRgb[FaceCount][3] = {
		{0.92f, 0.22f, 0.18f},
		{0.55f, 0.12f, 0.10f},
		{0.28f, 0.90f, 0.32f},
		{0.14f, 0.38f, 0.16f},
		{0.28f, 0.48f, 0.95f},
		{0.14f, 0.22f, 0.48f},
	};

	VECTOR FaceNormals[FaceCount] __attribute__((aligned(16))) = {
		{1.0f, 0.0f, 0.0f, 1.0f},
		{-1.0f, 0.0f, 0.0f, 1.0f},
		{0.0f, 1.0f, 0.0f, 1.0f},
		{0.0f, -1.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, -1.0f, 1.0f},
	};

	/// Local axis each face normal points along (0 = X, 1 = Y, 2 = Z) — picks the half-extent.
	constexpr int FaceAxis[FaceCount] = {0, 0, 1, 1, 2, 2};

	VECTOR FaceCorners[CubeVertexCount] __attribute__((aligned(16))) = {
		{1.0f, -1.0f, -1.0f, 1.0f},
		{1.0f, -1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, -1.0f, 1.0f},
		{-1.0f, -1.0f, 1.0f, 1.0f},
		{-1.0f, -1.0f, -1.0f, 1.0f},
		{-1.0f, 1.0f, -1.0f, 1.0f},
		{-1.0f, 1.0f, 1.0f, 1.0f},
		{-1.0f, 1.0f, -1.0f, 1.0f},
		{1.0f, 1.0f, -1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 1.0f},
		{-1.0f, 1.0f, 1.0f, 1.0f},
		{-1.0f, -1.0f, 1.0f, 1.0f},
		{1.0f, -1.0f, 1.0f, 1.0f},
		{1.0f, -1.0f, -1.0f, 1.0f},
		{-1.0f, -1.0f, -1.0f, 1.0f},
		{-1.0f, -1.0f, 1.0f, 1.0f},
		{-1.0f, 1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 1.0f},
		{1.0f, -1.0f, 1.0f, 1.0f},
		{1.0f, -1.0f, -1.0f, 1.0f},
		{1.0f, 1.0f, -1.0f, 1.0f},
		{-1.0f, 1.0f, -1.0f, 1.0f},
		{-1.0f, -1.0f, -1.0f, 1.0f},
	};

	// Per-face UV corner pattern (same for all 6 faces).
	constexpr float FaceUv[VertsPerFace][2] = {
		{0.0f, 1.0f},
		{1.0f, 1.0f},
		{1.0f, 0.0f},
		{0.0f, 0.0f},
	};

	// Two triangles per face, indices into the face's 4 corners.
	constexpr int FaceTris[2][3] = {{0, 1, 2}, {0, 2, 3}};

	struct ViewMatrices
	{
		MATRIX WorldView{};
		MATRIX ViewScreen{};
		/// Camera world position used to shift objects into camera-relative space.
		float CamX = 0.0f;
		float CamY = 0.0f;
		float CamZ = 0.0f;
		bool bReady = false;
	};

	ViewMatrices& CachedView()
	{
		static ViewMatrices Cache{};
		return Cache;
	}

	[[nodiscard]] float TurnsToRadians(unsigned Angle256)
	{
		return (static_cast<float>(Angle256 & 255u) * TwoPi) / 256.0f;
	}

	void RefreshViewMatrices(const FPS2ViewTarget& Vt)
	{
		auto& LocalCache = CachedView();
		// Camera-relative rendering: bake translation into object positions, keep view at origin.
		// Avoids EE float error when character/camera drift far from world origin.
		LocalCache.CamX = Vt.LocationX;
		LocalCache.CamY = Vt.LocationY;
		LocalCache.CamZ = Vt.LocationZ;
		VECTOR CameraPosition __attribute__((aligned(16))) = {0.0f, 0.0f, 0.0f, 1.0f};
		VECTOR CameraRotation __attribute__((aligned(16))) = {Vt.Pitch, Vt.Yaw, 0.0f, 1.0f};
		create_world_view(LocalCache.WorldView, CameraPosition, CameraRotation);
		// Match ps2sdk ee/draw/samples/cube: glFrustum-like perspective (aspect applied inside).
		create_view_screen(LocalCache.ViewScreen, graph_aspect_ratio(), -3.0f, 3.0f, -3.0f, 3.0f, 1.0f, 2000.0f);
		LocalCache.bReady = true;
	}

	// math3d: intensity = max(0, -N·L); L is ray travel direction. w must be 1.
	void BuildDirectionalRay(VECTOR OutDirection, unsigned Yaw256, unsigned Pitch256)
	{
		const float Cp = FPlatformMath::Cos256(Pitch256);
		VECTOR Raw __attribute__((aligned(16))) = {FPlatformMath::Sin256(Yaw256) * Cp, -FPlatformMath::Sin256(Pitch256),
			FPlatformMath::Cos256(Yaw256) * Cp, 1.0f};
		vector_normalize(OutDirection, Raw);
		OutDirection[3] = 1.0f;
	}

	// --- Homogeneous clipping -------------------------------------------------------------------
	// The GS has no clipper and math3d gives W = -Z_eye. NDC ±1 maps onto the whole 0..4096 GS
	// coordinate range (TriangleWriter), while the 640×448 screen only spans ~±0.16 × ±0.11 NDC.
	// So: clip against the near plane and a ±Guard band (keeps XYZ2 in range), trivially reject
	// against the visible frustum, and let the GS scissor trim the rest. No triangle is dropped
	// just because one vertex is off-screen or behind the camera.
	constexpr float NearW = 1.0f; // = create_view_screen near
	constexpr float Guard = 0.95f;
	constexpr float VisibleSlack = 1.05f;
	// NDC z in [-1, 1] (near at 1) onto the Z24 buffer's range; the depth test is GEQUAL.
	constexpr float MaxDepth = 16777215.0f;
	constexpr int MaxPolyVerts = 3 + 5; // each of the 5 clip planes adds at most one vertex

	enum : unsigned
	{
		OutNear = 1u << 0,
		OutVisXPos = 1u << 1,
		OutVisXNeg = 1u << 2,
		OutVisYPos = 1u << 3,
		OutVisYNeg = 1u << 4,
		OutGuardXPos = 1u << 5,
		OutGuardXNeg = 1u << 6,
		OutGuardYPos = 1u << 7,
		OutGuardYNeg = 1u << 8,
		OutRejectMask = OutNear | OutVisXPos | OutVisXNeg | OutVisYPos | OutVisYNeg,
		OutClipMask = OutNear | OutGuardXPos | OutGuardXNeg | OutGuardYPos | OutGuardYNeg,
	};

	struct ClipVertex
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;
		float W = 0.0f;
		float R = 0.0f;
		float G = 0.0f;
		float B = 0.0f;
		float S = 0.0f;
		float T = 0.0f;
	};

	/// Visible frustum half-extents in NDC (screen px / 2048, see TriangleWriter).
	struct VisibleExtents
	{
		float X = 0.16f;
		float Y = 0.11f;
	};

	[[nodiscard]] unsigned Outcode(const ClipVertex& V, const VisibleExtents& Vis)
	{
		unsigned Code = 0;
		if (V.W < NearW)
		{
			Code |= OutNear;
		}
		const float Vx = Vis.X * V.W;
		const float Vy = Vis.Y * V.W;
		const float G = Guard * V.W;
		if (V.X > Vx)
		{
			Code |= OutVisXPos;
		}
		if (V.X < -Vx)
		{
			Code |= OutVisXNeg;
		}
		if (V.Y > Vy)
		{
			Code |= OutVisYPos;
		}
		if (V.Y < -Vy)
		{
			Code |= OutVisYNeg;
		}
		if (V.X > G)
		{
			Code |= OutGuardXPos;
		}
		if (V.X < -G)
		{
			Code |= OutGuardXNeg;
		}
		if (V.Y > G)
		{
			Code |= OutGuardYPos;
		}
		if (V.Y < -G)
		{
			Code |= OutGuardYNeg;
		}
		return Code;
	}

	/// Signed distance to clip plane p (inside when >= 0). Order matches OutClipMask bits.
	[[nodiscard]] float PlaneDistance(const ClipVertex& V, int Plane)
	{
		switch (Plane)
		{
			case 0:
				return V.W - NearW;
			case 1:
				return Guard * V.W - V.X;
			case 2:
				return Guard * V.W + V.X;
			case 3:
				return Guard * V.W - V.Y;
			default:
				return Guard * V.W + V.Y;
		}
	}

	constexpr unsigned PlaneBit[5] = {OutNear, OutGuardXPos, OutGuardXNeg, OutGuardYPos, OutGuardYNeg};

	[[nodiscard]] ClipVertex Lerp(const ClipVertex& A, const ClipVertex& B, float T)
	{
		ClipVertex O{};
		O.X = A.X + (B.X - A.X) * T;
		O.Y = A.Y + (B.Y - A.Y) * T;
		O.Z = A.Z + (B.Z - A.Z) * T;
		O.W = A.W + (B.W - A.W) * T;
		O.R = A.R + (B.R - A.R) * T;
		O.G = A.G + (B.G - A.G) * T;
		O.B = A.B + (B.B - A.B) * T;
		O.S = A.S + (B.S - A.S) * T;
		O.T = A.T + (B.T - A.T) * T;
		return O;
	}

	/// Sutherland–Hodgman against the planes in `planes` (OutClipMask bits). Returns vertex count.
	[[nodiscard]] int ClipPolygon(ClipVertex* Poly, int Count, unsigned Planes)
	{
		ClipVertex Scratch[MaxPolyVerts];
		ClipVertex* In = Poly;
		ClipVertex* Out = Scratch;
		for (int P = 0; P < 5 && Count > 0; ++P)
		{
			if ((Planes & PlaneBit[P]) == 0)
			{
				continue;
			}
			int OutCount = 0;
			for (int I = 0; I < Count; ++I)
			{
				const ClipVertex& A = In[I];
				const ClipVertex& B = In[(I + 1) % Count];
				const float Da = PlaneDistance(A, P);
				const float Db = PlaneDistance(B, P);
				if (Da >= 0.0f)
				{
					Out[OutCount++] = A;
				}
				if ((Da >= 0.0f) != (Db >= 0.0f) && OutCount < MaxPolyVerts)
				{
					Out[OutCount++] = Lerp(A, B, Da / (Da - Db));
				}
			}
			Count = OutCount;
			ClipVertex* T = In;
			In = Out;
			Out = T;
		}
		if (In != Poly)
		{
			for (int I = 0; I < Count; ++I)
			{
				Poly[I] = In[I];
			}
		}
		return Count;
	}

	/// Appends one vertex (RGBAQ [+ ST] + XYZ2) after the perspective divide.
	struct TriangleWriter
	{
		FGSCommandList* List = nullptr;
		bool bTextured = false;
		int Count = 0;

		void Vertex(const ClipVertex& V)
		{
			const float InvW = 1.0f / V.W;
			// Modulate treats 0x80 as 1.0: the lit color's 1.0 is 0x80, which leaves headroom for over-bright light.
			FGSRGBAQ Color;
			Color.R = uint8(FMath::Clamp(int32(V.R * 128.0f), 0, 255));
			Color.G = uint8(FMath::Clamp(int32(V.G * 128.0f), 0, 255));
			Color.B = uint8(FMath::Clamp(int32(V.B * 128.0f), 0, 255));
			Color.A = 0x80;
			Color.Q = InvW;
			List->SetRGBAQ(Color);
			if (bTextured)
			{
				FGSST St;
				St.S = V.S * InvW;
				St.T = V.T * InvW;
				List->SetST(St);
			}
			// NDC ±1 onto the primitive coordinates 0..4096 around the window's 2048 center, Y down.
			FGSXYZ Xyz;
			Xyz.X = GSToFixed4(2048.0f + (V.X * InvW * 2048.0f), 16);
			Xyz.Y = GSToFixed4(2048.0f - (V.Y * InvW * 2048.0f), 16);
			Xyz.Z = uint32(FMath::Clamp(((V.Z * InvW) + 1.0f) * 0.5f * MaxDepth, 0.0f, MaxDepth));
			List->AddVertex(Xyz);
		}

		void Triangle(const ClipVertex& A, const ClipVertex& B, const ClipVertex& C)
		{
			Vertex(A);
			Vertex(B);
			Vertex(C);
			++Count;
		}
	};

	void SubmitClippedTriangle(TriangleWriter& Writer, const ClipVertex& A, const ClipVertex& B, const ClipVertex& C,
		const VisibleExtents& Vis)
	{
		++GDraw3DDebug.InTris;
		const unsigned Oa = Outcode(A, Vis);
		const unsigned Ob = Outcode(B, Vis);
		const unsigned Oc = Outcode(C, Vis);
		if ((Oa & Ob & Oc & OutRejectMask) != 0)
		{
			++GDraw3DDebug.Drop0;
			return;
		}
		const unsigned NeedClip = (Oa | Ob | Oc) & OutClipMask;
		if (NeedClip == 0)
		{
			++GDraw3DDebug.Keep3;
			Writer.Triangle(A, B, C);
			return;
		}
		ClipVertex Poly[MaxPolyVerts] = {A, B, C};
		const int N = ClipPolygon(Poly, 3, NeedClip);
		if (N < 3)
		{
			++GDraw3DDebug.Drop0;
			return;
		}
		++GDraw3DDebug.Clipped;
		for (int I = 1; I + 1 < N; ++I)
		{
			Writer.Triangle(Poly[0], Poly[I], Poly[I + 1]);
		}
	}

} // namespace

bool FPS2RHI::DrawBox(float LocationX, float LocationY, float LocationZ, unsigned Yaw256, unsigned Pitch256,
	float ScaleX, float ScaleY, float ScaleZ)
{
	auto& Gs = Leon::PS2::GetGSContext();
	if (!Gs.bReady || ScaleX <= 0.0f || ScaleY <= 0.0f || ScaleZ <= 0.0f)
	{
		return false;
	}

	auto& Scene = Leon::PS2::GetSceneState();
	if (Scene.bViewDirty || !CachedView().bReady)
	{
		RefreshViewMatrices(Scene.ViewTarget);
		Scene.bViewDirty = false;
	}
	++GDraw3DDebug.Boxes;

	auto& View = CachedView();
	VECTOR ObjectPosition
		__attribute__((aligned(16))) = {LocationX - View.CamX, LocationY - View.CamY, LocationZ - View.CamZ, 1.0f};
	VECTOR ObjectRotation __attribute__((aligned(16))) = {TurnsToRadians(Pitch256), TurnsToRadians(Yaw256), 0.0f, 1.0f};
	VECTOR ObjectScale __attribute__((aligned(16))) = {ScaleX, ScaleY, ScaleZ, 1.0f};
	const float HalfExtent[3] = {ScaleX, ScaleY, ScaleZ};

	MATRIX LocalWorld;
	MATRIX LocalLight;
	MATRIX LocalScreen;
	// Local SRT: scale → rotate → translate (same multiply order as create_local_world,
	// with scale first). Scaling AFTER create_local_world shears non-uniform boxes on yaw.
	matrix_unit(LocalWorld);
	matrix_scale(LocalWorld, LocalWorld, ObjectScale);
	matrix_rotate(LocalWorld, LocalWorld, ObjectRotation);
	matrix_translate(LocalWorld, LocalWorld, ObjectPosition);
	create_local_light(LocalLight, ObjectRotation);
	create_local_screen(LocalScreen, LocalWorld, View.WorldView, View.ViewScreen);

	VisibleExtents Vis{};
	Vis.X = static_cast<float>(Gs.Width) * 0.5f / 2048.0f * VisibleSlack;
	Vis.Y = static_cast<float>(Gs.Height) * 0.5f / 2048.0f * VisibleSlack;

	// Clip-space corners; reject the whole box when every corner is outside one plane.
	VECTOR ClipVerts[CubeVertexCount] __attribute__((aligned(16)));
	for (int I = 0; I < CubeVertexCount; ++I)
	{
		vector_apply(ClipVerts[I], FaceCorners[I], LocalScreen);
	}
	unsigned BoxOut = ~0u;
	for (int I = 0; I < CubeVertexCount; ++I)
	{
		ClipVertex C{};
		C.X = ClipVerts[I][0];
		C.Y = ClipVerts[I][1];
		C.W = ClipVerts[I][3];
		BoxOut &= Outcode(C, Vis);
	}
	if ((BoxOut & OutRejectMask) != 0)
	{
		++GDraw3DDebug.CulledBoxes;
		return true;
	}

	// Face normals in camera-relative world space (camera at the origin).
	VECTOR LocalFaceNormals[FaceCount] __attribute__((aligned(16)));
	calculate_normals(LocalFaceNormals, FaceCount, FaceNormals, LocalLight);

	// Backface cull: visible when the face centre → camera vector points along the normal.
	bool FaceVisible[FaceCount];
	int VisibleFaces = 0;
	for (int F = 0; F < FaceCount; ++F)
	{
		const float H = HalfExtent[FaceAxis[F]];
		const float Cx = ObjectPosition[0] + LocalFaceNormals[F][0] * H;
		const float Cy = ObjectPosition[1] + LocalFaceNormals[F][1] * H;
		const float Cz = ObjectPosition[2] + LocalFaceNormals[F][2] * H;
		FaceVisible[F] = LocalFaceNormals[F][0] * Cx + LocalFaceNormals[F][1] * Cy + LocalFaceNormals[F][2] * Cz < 0.0f;
		if (FaceVisible[F])
		{
			++VisibleFaces;
		}
		else
		{
			++GDraw3DDebug.BackFaces;
		}
	}
	if (VisibleFaces == 0)
	{
		return true;
	}

	const FPS2Material& Mat = Scene.BoundMaterial;
	const bool bTextured = Mat.BaseColorMap != nullptr && Mat.BaseColorMap->Valid();
	const bool bLit = Mat.ShadingModel == EMaterialShadingModel::DefaultLit;
	if (bTextured)
	{
		Mat.BaseColorMap->Bind();
	}

	// Flat faces under ambient + directional light: one colour per face (6, not 24 verts).
	VECTOR Albedos[FaceCount] __attribute__((aligned(16)));
	VECTOR Shaded[FaceCount] __attribute__((aligned(16)));
	const bool bFaceTint = Mat.bUseFaceAlbedo && !bTextured;
	for (int F = 0; F < FaceCount; ++F)
	{
		Albedos[F][0] = bFaceTint ? Mat.BaseColorR * FaceRgb[F][0] : Mat.BaseColorR;
		Albedos[F][1] = bFaceTint ? Mat.BaseColorG * FaceRgb[F][1] : Mat.BaseColorG;
		Albedos[F][2] = bFaceTint ? Mat.BaseColorB * FaceRgb[F][2] : Mat.BaseColorB;
		Albedos[F][3] = 1.0f;
	}
	if (bLit)
	{
		VECTOR LightDirections[2] __attribute__((aligned(16)));
		VECTOR LightColours[2] __attribute__((aligned(16)));
		const int LightTypes[2] = {LIGHT_AMBIENT, LIGHT_DIRECTIONAL};
		LightDirections[0][0] = 0.0f;
		LightDirections[0][1] = 0.0f;
		LightDirections[0][2] = 0.0f;
		LightDirections[0][3] = 1.0f;
		LightColours[0][0] = Scene.AmbientR;
		LightColours[0][1] = Scene.AmbientG;
		LightColours[0][2] = Scene.AmbientB;
		LightColours[0][3] = 1.0f;
		BuildDirectionalRay(LightDirections[1], Scene.Sun.Yaw256, Scene.Sun.Pitch256);
		LightColours[1][0] = Scene.Sun.LightColorR * Scene.Sun.Intensity;
		LightColours[1][1] = Scene.Sun.LightColorG * Scene.Sun.Intensity;
		LightColours[1][2] = Scene.Sun.LightColorB * Scene.Sun.Intensity;
		LightColours[1][3] = 1.0f;

		VECTOR Lights[FaceCount] __attribute__((aligned(16)));
		calculate_lights(Lights, FaceCount, LocalFaceNormals, LightDirections, LightColours, LightTypes, 2);
		calculate_colours(Shaded, FaceCount, Albedos, Lights);
	}
	else
	{
		for (int F = 0; F < FaceCount; ++F)
		{
			vector_copy(Shaded[F], Albedos[F]);
		}
	}
	// Textured faces: TriangleWriter sends 1.0 as 0x80 (MODULATE's 1.0); halved, the texture shows at half brightness.
	if (bTextured)
	{
		for (int F = 0; F < FaceCount; ++F)
		{
			Shaded[F][0] *= 0.5f;
			Shaded[F][1] *= 0.5f;
			Shaded[F][2] *= 0.5f;
		}
	}

	// Gouraud so Q interpolates per vertex (flat would take one Q and make the texture swim).
	FGSPrim Prim;
	Prim.Type = EGSPrimitive::Triangle;
	Prim.bGouraud = true;
	Prim.bTextured = bTextured;
	Leon::PS2::AppendDepthTest(Gs, true);
	Gs.FrameList.SetPrim(Prim);

	TriangleWriter Writer{};
	Writer.List = &Gs.FrameList;
	Writer.bTextured = bTextured;

	for (int F = 0; F < FaceCount; ++F)
	{
		if (!FaceVisible[F])
		{
			continue;
		}
		ClipVertex Corners[VertsPerFace];
		for (int V = 0; V < VertsPerFace; ++V)
		{
			const int Idx = F * VertsPerFace + V;
			ClipVertex& C = Corners[V];
			C.X = ClipVerts[Idx][0];
			C.Y = ClipVerts[Idx][1];
			C.Z = ClipVerts[Idx][2];
			C.W = ClipVerts[Idx][3];
			C.R = Shaded[F][0];
			C.G = Shaded[F][1];
			C.B = Shaded[F][2];
			C.S = FaceUv[V][0];
			C.T = FaceUv[V][1];
		}
		for (const auto& Tri : FaceTris)
		{
			SubmitClippedTriangle(Writer, Corners[Tri[0]], Corners[Tri[1]], Corners[Tri[2]], Vis);
		}
	}

	GDraw3DDebug.Emitted += static_cast<unsigned>(Writer.Count);
	return true;
}

void FPS2RHI::BeginDraw3DStatsFrame()
{
	GDraw3DDebug = FPS2Draw3DStats{};
}

void FPS2RHI::GetDraw3DStats(FPS2Draw3DStats& Out)
{
	Out = GDraw3DDebug;
	Out.PacketQwordsPeak = Leon::PS2::GetGSContext().PacketQuadwordsPeak;
}

void FPS2RHI::PrintDraw3DStats(const FPS2Draw3DStats& S)
{
	UE_LOG(LogRHI, Log, "[Draw3D] boxes=%u culled=%u backfaces=%u tris=%u keep=%u drop=%u clip=%u emit=%u qwPeak=%u",
		S.Boxes, S.CulledBoxes, S.BackFaces, S.InTris, S.Keep3, S.Drop0, S.Clipped, S.Emitted, S.PacketQwordsPeak);
}
