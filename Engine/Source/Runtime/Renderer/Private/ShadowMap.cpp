#include "ShadowMap.h"

#include "GLClipSpace.h"
#include "RendererLog.h"
#include "ViewMatrices.h"

#include <glad/glad.h>

FShadowMap::~FShadowMap()
{
	Destroy();
}

bool FShadowMap::Create(int InSize)
{
	Destroy();
	InSize = FMath::Max(InSize, 64);
	Size = InSize;

	glGenFramebuffers(1, &Fbo);
	glGenTextures(1, &DepthTexture);

	glBindTexture(GL_TEXTURE_2D, DepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, Size, Size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float Border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, Border);

	glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	const GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (Status != GL_FRAMEBUFFER_COMPLETE)
	{
		UE_LOG(LogRenderer, Error, "ShadowMap framebuffer incomplete");
		Destroy();
		return false;
	}
	return true;
}

void FShadowMap::Destroy()
{
	if (DepthTexture != 0)
	{
		glDeleteTextures(1, &DepthTexture);
		DepthTexture = 0;
	}
	if (Fbo != 0)
	{
		glDeleteFramebuffers(1, &Fbo);
		Fbo = 0;
	}
	Size = 0;
}

void FShadowMap::Begin() const
{
	glViewport(0, 0, Size, Size);
	glBindFramebuffer(GL_FRAMEBUFFER, Fbo);
	glClear(GL_DEPTH_BUFFER_BIT);
	// No face cull so one-sided casters (planes, cards) still write depth.
	glDisable(GL_CULL_FACE);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 2.0f);
}

void FShadowMap::End(int FramebufferWidth, int FramebufferHeight, unsigned int RestoreFbo) const
{
	glDisable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glBindFramebuffer(GL_FRAMEBUFFER, RestoreFbo);
	glViewport(0, 0, FramebufferWidth, FramebufferHeight);
}

void FShadowMap::BindDepthTexture(unsigned int Unit) const
{
	glActiveTexture(GL_TEXTURE0 + Unit);
	glBindTexture(GL_TEXTURE_2D, DepthTexture);
}

FMatrix FShadowMap::FitLightSpaceMatrix(
	const FVector& LightDirection, const FVector& WorldMin, const FVector& WorldMax, float Padding)
{
	FVector Dir = LightDirection;
	if ((Dir | Dir) < 1e-8f)
	{
		Dir = FVector(0.35f, -1.0f, -0.45f);
	}
	Dir = Dir.GetUnsafeNormal();

	const FVector Center = (WorldMin + WorldMax) * 0.5f;
	const FVector Extents = (WorldMax - WorldMin) * 0.5f + FVector(Padding);
	const float Radius = Extents.Size();

	FVector Up(0.0f, 1.0f, 0.0f);
	if (FMath::Abs(Dir | Up) > 0.95f)
	{
		Up = FVector(0.0f, 0.0f, 1.0f);
	}

	/** cm between the bounding sphere and the light eye. */
	constexpr float EyeMargin = 100.0f;
	const FVector Eye = Center - (Dir * (Radius + EyeMargin));
	// UE view space of the light: x right, y up, z along the light (left-handed).
	const FMatrix LightView = MakeLookAtView(Eye, Center, Up);

	FVector MinLs(TNumericLimits<float>::Max());
	FVector MaxLs(TNumericLimits<float>::Lowest());

	const FVector Corners[8] = {
		FVector(WorldMin.X, WorldMin.Y, WorldMin.Z),
		FVector(WorldMax.X, WorldMin.Y, WorldMin.Z),
		FVector(WorldMin.X, WorldMax.Y, WorldMin.Z),
		FVector(WorldMax.X, WorldMax.Y, WorldMin.Z),
		FVector(WorldMin.X, WorldMin.Y, WorldMax.Z),
		FVector(WorldMax.X, WorldMin.Y, WorldMax.Z),
		FVector(WorldMin.X, WorldMax.Y, WorldMax.Z),
		FVector(WorldMax.X, WorldMax.Y, WorldMax.Z),
	};

	for (const FVector& Corner : Corners)
	{
		const FVector Ls = FVector(LightView.TransformPosition(Corner));
		MinLs = MinLs.ComponentMin(Ls);
		MaxLs = MaxLs.ComponentMax(Ls);
	}

	// View-space depth grows along +Z in front of the light (cm).
	const float ZNear = FMath::Max(5.0f, MinLs.Z - Padding);
	const float ZFar = FMath::Max(ZNear + 10.0f, MaxLs.Z + Padding);

	// Off-centre box: centre it in x / y, then a UE ortho with half sizes (depth [0, 1] from ZNear to ZFar), then GL
	// clip space.
	const float Left = MinLs.X - Padding;
	const float Right = MaxLs.X + Padding;
	const float Bottom = MinLs.Y - Padding;
	const float Top = MaxLs.Y + Padding;
	const FMatrix Centre = FTranslationMatrix(FVector(-(Left + Right) * 0.5f, -(Bottom + Top) * 0.5f, 0.0f));
	const FMatrix LightProj =
		Centre * FOrthoMatrix((Right - Left) * 0.5f, (Top - Bottom) * 0.5f, 1.0f / (ZFar - ZNear), -ZNear);
	return LightView * ToGLClipSpace(LightProj);
}
