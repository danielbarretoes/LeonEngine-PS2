#include "PS2GSContext.h"

#include <graph.h>

namespace Leon::PS2
{

	float FPS2GSContext::OriginX() const
	{
		return 2048.0f - (static_cast<float>(Frame.width) * 0.5f);
	}

	float FPS2GSContext::OriginY() const
	{
		return 2048.0f - (static_cast<float>(Frame.height) * 0.5f);
	}

	FPS2GSContext& GetGSContext()
	{
		static FPS2GSContext Context{};
		return Context;
	}

	int AllocateVram(int Width, int Height, int Psm, int Alignment)
	{
		const int Address = graph_vram_allocate(Width, Height, Psm, Alignment);
		if (Address >= 0)
		{
			const int End = Address + graph_vram_size(Width, Height, Psm, Alignment);
			FPS2GSContext& Gs = GetGSContext();
			if (End > Gs.VramEndWords)
			{
				Gs.VramEndWords = End;
			}
		}
		return Address;
	}

} // namespace Leon::PS2
