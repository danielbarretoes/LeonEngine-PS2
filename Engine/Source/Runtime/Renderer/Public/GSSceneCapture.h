#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

class FGSCommandList;
class FGSSceneRenderer;
class UWorld;
struct FGSDrawEnvironment;

/**
 * A view of a world's static geometry as the GS scene renderer draws it, recorded for a tool that rasterizes the list
 * itself (Leon, Docs/PLANS/ps2-polish.md P7: LeonEd's map overview runs it on GSReference's software GS, so the
 * picture depends on no GPU and is the same on every run). It is the frame FGSSceneRenderer records, of a scene of its
 * own that holds the world's Static, visible mesh components (with their baked lighting): no pawns, effects, lights
 * computed per frame or debug lines, and neither the map's sky nor its fog. The world needs no scene of its own (a map
 * import's or a commandlet's world has none); a component that is in one gets its proxy back afterwards. Textures
 * that are not paletted yet are converted as the PS2 cook converts them, into a texture arena the caller's
 * environment leaves free, without an upload budget. The list may hold the converted textures' texels in place
 * (FGSCommandList::UploadImageInPlace): keep the capture until the list is executed. Desktop only (the desktop's
 * texture converter).
 */
class RENDERER_API FGSSceneCapture
{
public:
	FGSSceneCapture();
	~FGSSceneCapture();
	FGSSceneCapture(const FGSSceneCapture&) = delete;
	FGSSceneCapture& operator=(const FGSSceneCapture&) = delete;

	/**
	 * Records into List (after Environment's registers) the view from ViewOrigin with UE's ViewMatrix and
	 * ProjectionMatrix (depth 0 to 1, not reversed: a perspective or an FOrthoMatrix), textures in the GS blocks
	 * [TextureArenaFirstBlock, + TextureArenaBlocks). Returns the static mesh components drawn from (0: nothing).
	 * Keep ViewOrigin outside the map's visibility cells: an eye inside one sees through its portals only.
	 */
	int32 RecordStaticWorld(UWorld& World, const FVector& ViewOrigin, const FMatrix& ViewMatrix,
		const FMatrix& ProjectionMatrix, const FGSDrawEnvironment& Environment, uint32 TextureArenaFirstBlock,
		uint32 TextureArenaBlocks, FGSCommandList& List);

private:
	/** The renderer whose texture cache keeps the converted textures the lists point into. */
	TUniquePtr<FGSSceneRenderer> Renderer;
};
