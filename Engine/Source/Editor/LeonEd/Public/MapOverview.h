#pragma once

#include "CoreMinimal.h"

class UTexture2D;
class UWorld;

/** How a map's overview is made (UMapImportSettings' Overview* settings, UBuildOverviewCommandlet's switches). */
struct FMapOverviewSettings
{
	/** The texture's side, texels (a power of two, 8 to the GS's 256). */
	int32 Resolution = 128;
	/** The render's pixels per texel on each side, averaged down (Resolution x Supersampling: 64 to 512 pixels). */
	int32 Supersampling = 4;
	/**
	 * The height the view cuts the map at, world Z, cm: what is above it (roofs, a tunnel's ceiling, the walls' tops)
	 * is not drawn, so the floors under it show (CS's overviews).
	 */
	float ClipHeight = 250.0f;
	/**
	 * The height a point is tested at for standing inside something solid, world Z, cm: a wall, a house, a container or
	 * a crate there is drawn as an obstacle, not as the floor under it.
	 */
	float SolidHeight = 100.0f;
	/** The space around the static geometry's bounds, cm on each side. */
	float Margin = 200.0f;
};

/**
 * A map's overview (Leon, Docs/PLANS/ps2-polish.md P7; CS 1.6's overviews): the map seen from above in orthographic,
 * north (+X) up and east (+Y) right, for a radar to draw under its dots. It is rendered by the GS scene renderer's own
 * frame (FGSSceneCapture: the Static mesh components with their baked lighting, no sky or fog) on GSReference's
 * software GS, so it depends on no GPU and the same map gives the same bytes (gate G5). Three views of the square:
 *
 * - from above, cut at ClipHeight: the floors' colours and heights;
 * - from below, looking up from SolidHeight, once as the renderer draws (the first face above that looks down: a
 *   ceiling) and once mirrored, its culling turned (the first face above that looks up: the top of what the point is
 *   inside). A point whose first face above looks up is inside something solid: an obstacle.
 *
 * Then it is made readable at a radar's size in CS's style: the floors' colours greyer and their light flatter, the
 * obstacles a dark grey, the outside darker, a dark line along every obstacle's and the outside's edge and the higher
 * side of every step of more than StepHeight; averaged down to Resolution and paletted to PF_P8, one level
 * (FPalettedTextureBuilder: what the cook keeps as it is). The world's settings keep it and the square it shows
 * (FWorldOverviewSettings).
 */
class LEONED_API FMapOverview
{
public:
	/** A step between two neighbouring pixels higher than this draws an edge line, cm. */
	static constexpr float StepHeight = 40.0f;

	/**
	 * The square the overview shows: the centre of the XY bounds of the map's visibility cells (where the players go),
	 * or without cells of the world's Static, visible mesh components, the larger side plus Margin on each side; and
	 * the meshes' lowest and highest Z. False without any mesh.
	 */
	static bool GetWorldSquare(
		const UWorld& World, float Margin, FVector2D& OutCenter, float& OutSize, float& OutMinZ, float& OutMaxZ);

	/**
	 * Renders the overview of the square Center / Size (the geometry between MinZ and MaxZ): Resolution x Resolution
	 * colours, the top row (north) first, styled and averaged down. False when nothing was drawn.
	 */
	static bool Render(UWorld& World, const FMapOverviewSettings& Settings, const FVector2D& Center, float Size,
		float MinZ, float MaxZ, TArray<FColor>& OutPixels);

	/** The overview texture's long package name: `<Map's package>/T_<Map>_Overview`. */
	[[nodiscard]] static FString GetOverviewPackageName(const UWorld& World);

	/**
	 * Renders the world's overview into Texture (PF_P8, Resolution square, one level) and stores it and its square in
	 * the world's settings. False (logged) when the map has nothing to show; the world's settings are left as they
	 * were.
	 */
	static bool Build(UWorld& World, const FMapOverviewSettings& Settings, UTexture2D& Texture);
};
