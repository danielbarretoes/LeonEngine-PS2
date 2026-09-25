#include "LegacyGLMath.h"

// The formulas follow glm 1.0.1 term by term (same products, same summation order) so the matrices keep the values
// glm produced. M[C][R] is column C, row R, as in glm.

namespace LegacyGL
{
	namespace
	{
		FMatrix Zero()
		{
			FMatrix Result;
			FMemory::Memzero(&Result, sizeof(Result));
			return Result;
		}
	} // namespace

	FMatrix Perspective(float FovYRadians, float Aspect, float ZNear, float ZFar)
	{
		const float TanHalfFovy = FMath::Tan(FovYRadians / 2.0f);
		FMatrix Result = Zero();
		Result.M[0][0] = 1.0f / (Aspect * TanHalfFovy);
		Result.M[1][1] = 1.0f / TanHalfFovy;
		Result.M[2][2] = -(ZFar + ZNear) / (ZFar - ZNear);
		Result.M[2][3] = -1.0f;
		Result.M[3][2] = -(2.0f * ZFar * ZNear) / (ZFar - ZNear);
		return Result;
	}

	FMatrix Ortho(float Left, float Right, float Bottom, float Top, float ZNear, float ZFar)
	{
		FMatrix Result = FMatrix::Identity;
		Result.M[0][0] = 2.0f / (Right - Left);
		Result.M[1][1] = 2.0f / (Top - Bottom);
		Result.M[2][2] = -2.0f / (ZFar - ZNear);
		Result.M[3][0] = -(Right + Left) / (Right - Left);
		Result.M[3][1] = -(Top + Bottom) / (Top - Bottom);
		Result.M[3][2] = -(ZFar + ZNear) / (ZFar - ZNear);
		return Result;
	}

	FMatrix LookAt(const FVector& Eye, const FVector& Center, const FVector& Up)
	{
		const FVector F = (Center - Eye).GetUnsafeNormal();
		const FVector S = (F ^ Up).GetUnsafeNormal();
		const FVector U = S ^ F;

		FMatrix Result = FMatrix::Identity;
		Result.M[0][0] = S.X;
		Result.M[1][0] = S.Y;
		Result.M[2][0] = S.Z;
		Result.M[0][1] = U.X;
		Result.M[1][1] = U.Y;
		Result.M[2][1] = U.Z;
		Result.M[0][2] = -F.X;
		Result.M[1][2] = -F.Y;
		Result.M[2][2] = -F.Z;
		Result.M[3][0] = -(S | Eye);
		Result.M[3][1] = -(U | Eye);
		Result.M[3][2] = F | Eye;
		return Result;
	}

	FMatrix Translate(const FMatrix& M, const FVector& V)
	{
		FMatrix Result = M;
		for (int32 R = 0; R < 4; ++R)
		{
			Result.M[3][R] = M.M[0][R] * V.X + M.M[1][R] * V.Y + M.M[2][R] * V.Z + M.M[3][R];
		}
		return Result;
	}

	FMatrix Rotate(const FMatrix& M, float Radians, const FVector& Axis)
	{
		const float C = FMath::Cos(Radians);
		const float S = FMath::Sin(Radians);
		const FVector A = Axis.GetUnsafeNormal();
		const FVector Temp = A * (1.0f - C);

		float Rot[3][3];
		Rot[0][0] = C + Temp.X * A.X;
		Rot[0][1] = Temp.X * A.Y + S * A.Z;
		Rot[0][2] = Temp.X * A.Z - S * A.Y;
		Rot[1][0] = Temp.Y * A.X - S * A.Z;
		Rot[1][1] = C + Temp.Y * A.Y;
		Rot[1][2] = Temp.Y * A.Z + S * A.X;
		Rot[2][0] = Temp.Z * A.X + S * A.Y;
		Rot[2][1] = Temp.Z * A.Y - S * A.X;
		Rot[2][2] = C + Temp.Z * A.Z;

		FMatrix Result = M;
		for (int32 Col = 0; Col < 3; ++Col)
		{
			for (int32 R = 0; R < 4; ++R)
			{
				Result.M[Col][R] = M.M[0][R] * Rot[Col][0] + M.M[1][R] * Rot[Col][1] + M.M[2][R] * Rot[Col][2];
			}
		}
		return Result;
	}

	FMatrix Scale(const FMatrix& M, const FVector& V)
	{
		FMatrix Result = M;
		for (int32 R = 0; R < 4; ++R)
		{
			Result.M[0][R] = M.M[0][R] * V.X;
			Result.M[1][R] = M.M[1][R] * V.Y;
			Result.M[2][R] = M.M[2][R] * V.Z;
		}
		return Result;
	}

	FMatrix QuatToMatrix(float W, float X, float Y, float Z)
	{
		const float Qxx = X * X;
		const float Qyy = Y * Y;
		const float Qzz = Z * Z;
		const float Qxz = X * Z;
		const float Qxy = X * Y;
		const float Qyz = Y * Z;
		const float Qwx = W * X;
		const float Qwy = W * Y;
		const float Qwz = W * Z;

		FMatrix Result = FMatrix::Identity;
		Result.M[0][0] = 1.0f - 2.0f * (Qyy + Qzz);
		Result.M[0][1] = 2.0f * (Qxy + Qwz);
		Result.M[0][2] = 2.0f * (Qxz - Qwy);

		Result.M[1][0] = 2.0f * (Qxy - Qwz);
		Result.M[1][1] = 1.0f - 2.0f * (Qxx + Qzz);
		Result.M[1][2] = 2.0f * (Qyz + Qwx);

		Result.M[2][0] = 2.0f * (Qxz + Qwy);
		Result.M[2][1] = 2.0f * (Qyz - Qwx);
		Result.M[2][2] = 1.0f - 2.0f * (Qxx + Qyy);
		return Result;
	}

	void NormalMatrix3x3(const FMatrix& Model, float Out[9])
	{
		const float (&M)[4][4] = Model.M;
		const float Det = M[0][0] * (M[1][1] * M[2][2] - M[2][1] * M[1][2]) -
			M[1][0] * (M[0][1] * M[2][2] - M[2][1] * M[0][2]) + M[2][0] * (M[0][1] * M[1][2] - M[1][1] * M[0][2]);
		if (FMath::Abs(Det) < 1e-12f)
		{
			const float Identity[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
			FMemory::Memcpy(Out, Identity, sizeof(Identity));
			return;
		}

		const float OneOverDeterminant = 1.0f / Det;
		float Inverse[3][3];
		Inverse[0][0] = +(M[1][1] * M[2][2] - M[2][1] * M[1][2]) * OneOverDeterminant;
		Inverse[1][0] = -(M[1][0] * M[2][2] - M[2][0] * M[1][2]) * OneOverDeterminant;
		Inverse[2][0] = +(M[1][0] * M[2][1] - M[2][0] * M[1][1]) * OneOverDeterminant;
		Inverse[0][1] = -(M[0][1] * M[2][2] - M[2][1] * M[0][2]) * OneOverDeterminant;
		Inverse[1][1] = +(M[0][0] * M[2][2] - M[2][0] * M[0][2]) * OneOverDeterminant;
		Inverse[2][1] = -(M[0][0] * M[2][1] - M[2][0] * M[0][1]) * OneOverDeterminant;
		Inverse[0][2] = +(M[0][1] * M[1][2] - M[1][1] * M[0][2]) * OneOverDeterminant;
		Inverse[1][2] = -(M[0][0] * M[1][2] - M[1][0] * M[0][2]) * OneOverDeterminant;
		Inverse[2][2] = +(M[0][0] * M[1][1] - M[1][0] * M[0][1]) * OneOverDeterminant;

		// transpose(Inverse), written in mat3 memory order (column-major).
		for (int32 Col = 0; Col < 3; ++Col)
		{
			for (int32 R = 0; R < 3; ++R)
			{
				Out[Col * 3 + R] = Inverse[R][Col];
			}
		}
	}
} // namespace LegacyGL
