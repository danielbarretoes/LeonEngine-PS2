#include "CoreMinimal.h"
#include "Math/VectorMath.h"

// VU0 in macro mode (Docs/PLANS/ps2-shipping.md N15): the EE issues VU0's instructions itself (COP2), as ps2sdk's
// libvux and math3d do. VU0 runs no microprogram in the engine, so its registers are free between the calls; each
// call loads what it needs (LQC2, 16-byte aligned: FMatrix is, the vectors are copied to aligned locals) and stores its
// result (SQC2). Every block clobbers "memory": its inputs are the locals it reads through their addresses, which the
// compiler must have stored first. VU0's multiply-adds accumulate in ACC and truncate as the EE's FPU does:
// FVectorMathFPU is the reference (TestPAL, System.Core.Math.VectorMathVU0).

namespace
{

	/** A vector in a quadword (LQC2 / SQC2 ignore an address's low four bits). */
	struct alignas(16) FQuadword
	{
		float V[4];
	};

	[[nodiscard]] float BitsToFloat(uint32 Bits)
	{
		float Value = 0.0f;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

} // namespace

void FVectorMath::MatrixMultiply(FMatrix& Result, const FMatrix& A, const FMatrix& B)
{
	// Row R of the product is A[R].x B[0] + A[R].y B[1] + A[R].z B[2] + A[R].w B[3]; every load before the first store,
	// so Result may be A or B.
	checkSlow((reinterpret_cast<UPTRINT>(&Result) & 15) == 0 && (reinterpret_cast<UPTRINT>(&A) & 15) == 0 &&
		(reinterpret_cast<UPTRINT>(&B) & 15) == 0);
	asm volatile("lqc2 $vf1, 0x00(%2)\n\t"
				 "lqc2 $vf2, 0x10(%2)\n\t"
				 "lqc2 $vf3, 0x20(%2)\n\t"
				 "lqc2 $vf4, 0x30(%2)\n\t"
				 "lqc2 $vf5, 0x00(%1)\n\t"
				 "lqc2 $vf6, 0x10(%1)\n\t"
				 "lqc2 $vf7, 0x20(%1)\n\t"
				 "lqc2 $vf8, 0x30(%1)\n\t"
				 "vmulax.xyzw $ACC, $vf1, $vf5x\n\t"
				 "vmadday.xyzw $ACC, $vf2, $vf5y\n\t"
				 "vmaddaz.xyzw $ACC, $vf3, $vf5z\n\t"
				 "vmaddw.xyzw $vf9, $vf4, $vf5w\n\t"
				 "vmulax.xyzw $ACC, $vf1, $vf6x\n\t"
				 "vmadday.xyzw $ACC, $vf2, $vf6y\n\t"
				 "vmaddaz.xyzw $ACC, $vf3, $vf6z\n\t"
				 "vmaddw.xyzw $vf10, $vf4, $vf6w\n\t"
				 "vmulax.xyzw $ACC, $vf1, $vf7x\n\t"
				 "vmadday.xyzw $ACC, $vf2, $vf7y\n\t"
				 "vmaddaz.xyzw $ACC, $vf3, $vf7z\n\t"
				 "vmaddw.xyzw $vf11, $vf4, $vf7w\n\t"
				 "vmulax.xyzw $ACC, $vf1, $vf8x\n\t"
				 "vmadday.xyzw $ACC, $vf2, $vf8y\n\t"
				 "vmaddaz.xyzw $ACC, $vf3, $vf8z\n\t"
				 "vmaddw.xyzw $vf12, $vf4, $vf8w\n\t"
				 "sqc2 $vf9, 0x00(%0)\n\t"
				 "sqc2 $vf10, 0x10(%0)\n\t"
				 "sqc2 $vf11, 0x20(%0)\n\t"
				 "sqc2 $vf12, 0x30(%0)\n\t"
		:
		: "r"(&Result), "r"(&A), "r"(&B)
		: "memory");
}

void FVectorMath::TransformVector4(FVector4& Result, const FMatrix& M, const FVector4& P)
{
	// P.x M[0] + P.y M[1] + P.z M[2] + P.w M[3] (FVector4 is not aligned: through a quadword each way).
	FQuadword In = {{P.X, P.Y, P.Z, P.W}};
	FQuadword Out;
	asm volatile("lqc2 $vf1, 0x00(%2)\n\t"
				 "lqc2 $vf2, 0x10(%2)\n\t"
				 "lqc2 $vf3, 0x20(%2)\n\t"
				 "lqc2 $vf4, 0x30(%2)\n\t"
				 "lqc2 $vf5, 0x00(%1)\n\t"
				 "vmulax.xyzw $ACC, $vf1, $vf5x\n\t"
				 "vmadday.xyzw $ACC, $vf2, $vf5y\n\t"
				 "vmaddaz.xyzw $ACC, $vf3, $vf5z\n\t"
				 "vmaddw.xyzw $vf6, $vf4, $vf5w\n\t"
				 "sqc2 $vf6, 0x00(%0)\n\t"
		:
		: "r"(&Out), "r"(&In), "r"(&M)
		: "memory");
	Result = FVector4(Out.V[0], Out.V[1], Out.V[2], Out.V[3]);
}

bool FVectorMath::IsBoxOutside(const FVectorPlaneSet& Planes, const FVector& Center, const FVector& Extent)
{
	// Four planes a group: X cx + Y cy + Z cz + |X| ex + |Y| ey + |Z| ez + W, the two groups' least lane last.
	const FQuadword C = {{Center.X, Center.Y, Center.Z, 0.0f}};
	const FQuadword E = {{Extent.X, Extent.Y, Extent.Z, 0.0f}};
	uint64 Least = 0;
	asm volatile("lqc2 $vf1, 0x00(%1)\n\t"
				 "lqc2 $vf2, 0x00(%2)\n\t"
				 "lqc2 $vf3, 0x00(%3)\n\t"
				 "lqc2 $vf4, 0x20(%3)\n\t"
				 "lqc2 $vf5, 0x40(%3)\n\t"
				 "lqc2 $vf6, 0x60(%3)\n\t"
				 "lqc2 $vf7, 0x80(%3)\n\t"
				 "lqc2 $vf8, 0xa0(%3)\n\t"
				 "lqc2 $vf9, 0xc0(%3)\n\t"
				 "vmulax.xyzw $ACC, $vf3, $vf1x\n\t"
				 "vmadday.xyzw $ACC, $vf4, $vf1y\n\t"
				 "vmaddaz.xyzw $ACC, $vf5, $vf1z\n\t"
				 "vmaddax.xyzw $ACC, $vf7, $vf2x\n\t"
				 "vmadday.xyzw $ACC, $vf8, $vf2y\n\t"
				 "vmaddaz.xyzw $ACC, $vf9, $vf2z\n\t"
				 "vmaddw.xyzw $vf10, $vf6, $vf0w\n\t"
				 "lqc2 $vf3, 0x10(%3)\n\t"
				 "lqc2 $vf4, 0x30(%3)\n\t"
				 "lqc2 $vf5, 0x50(%3)\n\t"
				 "lqc2 $vf6, 0x70(%3)\n\t"
				 "lqc2 $vf7, 0x90(%3)\n\t"
				 "lqc2 $vf8, 0xb0(%3)\n\t"
				 "lqc2 $vf9, 0xd0(%3)\n\t"
				 "vmulax.xyzw $ACC, $vf3, $vf1x\n\t"
				 "vmadday.xyzw $ACC, $vf4, $vf1y\n\t"
				 "vmaddaz.xyzw $ACC, $vf5, $vf1z\n\t"
				 "vmaddax.xyzw $ACC, $vf7, $vf2x\n\t"
				 "vmadday.xyzw $ACC, $vf8, $vf2y\n\t"
				 "vmaddaz.xyzw $ACC, $vf9, $vf2z\n\t"
				 "vmaddw.xyzw $vf11, $vf6, $vf0w\n\t"
				 "vmini.xyzw $vf10, $vf10, $vf11\n\t"
				 "vminiy.x $vf10, $vf10, $vf10y\n\t"
				 "vminiz.x $vf10, $vf10, $vf10z\n\t"
				 "vminiw.x $vf10, $vf10, $vf10w\n\t"
				 "qmfc2 %0, $vf10\n\t"
		: "=r"(Least)
		: "r"(&C), "r"(&E), "r"(&Planes)
		: "memory");
	return BitsToFloat(uint32(Least)) < 0.0f;
}

bool FVectorMath::IsSphereOutside(const FVectorPlaneSet& Planes, const FVector& Center, float Radius)
{
	// Four planes a group: X cx + Y cy + Z cz + W, then + the radius.
	const FQuadword C = {{Center.X, Center.Y, Center.Z, Radius}};
	uint64 Least = 0;
	asm volatile("lqc2 $vf1, 0x00(%1)\n\t"
				 "lqc2 $vf3, 0x00(%2)\n\t"
				 "lqc2 $vf4, 0x20(%2)\n\t"
				 "lqc2 $vf5, 0x40(%2)\n\t"
				 "lqc2 $vf6, 0x60(%2)\n\t"
				 "vmulax.xyzw $ACC, $vf3, $vf1x\n\t"
				 "vmadday.xyzw $ACC, $vf4, $vf1y\n\t"
				 "vmaddaz.xyzw $ACC, $vf5, $vf1z\n\t"
				 "vmaddw.xyzw $vf10, $vf6, $vf0w\n\t"
				 "vaddw.xyzw $vf10, $vf10, $vf1w\n\t"
				 "lqc2 $vf3, 0x10(%2)\n\t"
				 "lqc2 $vf4, 0x30(%2)\n\t"
				 "lqc2 $vf5, 0x50(%2)\n\t"
				 "lqc2 $vf6, 0x70(%2)\n\t"
				 "vmulax.xyzw $ACC, $vf3, $vf1x\n\t"
				 "vmadday.xyzw $ACC, $vf4, $vf1y\n\t"
				 "vmaddaz.xyzw $ACC, $vf5, $vf1z\n\t"
				 "vmaddw.xyzw $vf11, $vf6, $vf0w\n\t"
				 "vaddw.xyzw $vf11, $vf11, $vf1w\n\t"
				 "vmini.xyzw $vf10, $vf10, $vf11\n\t"
				 "vminiy.x $vf10, $vf10, $vf10y\n\t"
				 "vminiz.x $vf10, $vf10, $vf10z\n\t"
				 "vminiw.x $vf10, $vf10, $vf10w\n\t"
				 "qmfc2 %0, $vf10\n\t"
		: "=r"(Least)
		: "r"(&C), "r"(&Planes)
		: "memory");
	return BitsToFloat(uint32(Least)) < 0.0f;
}

bool FVectorMath::IsVectorUnit()
{
	return true;
}
