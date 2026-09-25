#include "RenderMatrices.h"

void GetNormalMatrix3x3(const FMatrix& Model, float Out[9])
{
	// GLSL's N * n must equal the row-vector n * Inverse^T, so N's column Col, row Row is Inverse.M[Row][Col].
	const FMatrix Inverse = Model.Inverse();
	for (int32 Col = 0; Col < 3; ++Col)
	{
		for (int32 Row = 0; Row < 3; ++Row)
		{
			Out[Col * 3 + Row] = Inverse.M[Row][Col];
		}
	}
}

FMatrix MakePixelSpaceProjection(float Width, float Height)
{
	// Row vectors: x_ndc = 2 x / Width - 1, y_ndc = 1 - 2 y / Height.
	return FMatrix(FPlane(2.0f / Width, 0.0f, 0.0f, 0.0f), FPlane(0.0f, -2.0f / Height, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 1.0f, 0.0f), FPlane(-1.0f, 1.0f, 0.0f, 1.0f));
}
