#pragma once

#include "CoreTypes.h"

/** Uninitialised storage with the size and alignment of a type (UE: TTypeCompatibleBytes). */
template <typename ElementType>
struct TTypeCompatibleBytes
{
	alignas(ElementType) uint8 Pad[sizeof(ElementType)];

	ElementType* GetTypedPtr()
	{
		return reinterpret_cast<ElementType*>(this);
	}
	const ElementType* GetTypedPtr() const
	{
		return reinterpret_cast<const ElementType*>(this);
	}
};

/** Uninitialised storage of a given size and alignment (UE: TAlignedBytes). */
template <int32 Size, uint32 Alignment>
struct TAlignedBytes
{
	alignas(Alignment) uint8 Pad[Size];
};
