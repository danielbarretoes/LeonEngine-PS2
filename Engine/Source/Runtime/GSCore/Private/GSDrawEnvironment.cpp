#include "GSDrawEnvironment.h"

void FGSDrawEnvironment::Append(FGSCommandList& List) const
{
	List.SetPrimModeFromPrim();
	List.SetFrame(0, Frame);
	List.SetZBuf(0, ZBuf);
	FGSXYOffset Offset;
	Offset.OFX = GSToFixed4(PrimitiveX(0.0f), 16);
	Offset.OFY = GSToFixed4(PrimitiveY(0.0f), 16);
	List.SetXYOffset(0, Offset);
	FGSScissor Scissor;
	Scissor.SCAX1 = uint16(Width - 1);
	Scissor.SCAY1 = uint16(Height - 1);
	List.SetScissor(0, Scissor);
	List.SetAlpha(0, FGSAlpha::Translucent());
	List.SetFba(0, false);
	List.SetColorClamp(true);
	List.SetPixelAlphaBlend(false);
	List.SetTexA(FGSTexA());
	List.SetDimx(FGSDimx::Default());
	List.SetDither(Frame.PSM == EGSPixelFormat::PSMCT16 || Frame.PSM == EGSPixelFormat::PSMCT16S);
	List.SetTest(0, DepthTest(true));
}

FGSTest FGSDrawEnvironment::DepthTest(bool bDepthTest)
{
	FGSTest Test;
	Test.ZTST = bDepthTest ? EGSDepthTest::GreaterEqual : EGSDepthTest::Always;
	return Test;
}

FGSXYZ FGSDrawEnvironment::PixelVertex(float X, float Y, uint32 Z) const
{
	FGSXYZ Vertex;
	Vertex.X = GSToFixed4(PrimitiveX(X), 16);
	Vertex.Y = GSToFixed4(PrimitiveY(Y), 16);
	Vertex.Z = Z;
	return Vertex;
}
