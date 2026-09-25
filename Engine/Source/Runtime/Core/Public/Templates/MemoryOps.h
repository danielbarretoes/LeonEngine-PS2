#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

#include <new>
#include <type_traits>

// Element construction / destruction over raw memory (UE: Templates/MemoryOps.h). Containers call these so trivial
// types use memset / memcpy / memmove.

/** Default-constructs Count elements at Address. */
template <typename ElementType, typename SizeType>
FORCEINLINE void DefaultConstructItems(void* Address, SizeType Count)
{
	if constexpr (TIsZeroConstructType<ElementType>::Value)
	{
		FMemory::Memset(Address, 0, sizeof(ElementType) * Count);
	}
	else
	{
		ElementType* Element = static_cast<ElementType*>(Address);
		while (Count)
		{
			new (Element) ElementType;
			++Element;
			--Count;
		}
	}
}

template <typename ElementType>
FORCEINLINE void DestructItem(ElementType* Element)
{
	if constexpr (!std::is_trivially_destructible_v<ElementType>)
	{
		// The typedef keeps the destructor call working for types named through a typedef.
		typedef ElementType DestructItemsElementTypeTypedef;
		Element->DestructItemsElementTypeTypedef::~DestructItemsElementTypeTypedef();
	}
}

template <typename ElementType, typename SizeType>
FORCEINLINE void DestructItems(ElementType* Element, SizeType Count)
{
	if constexpr (!std::is_trivially_destructible_v<ElementType>)
	{
		while (Count)
		{
			typedef ElementType DestructItemsElementTypeTypedef;
			Element->DestructItemsElementTypeTypedef::~DestructItemsElementTypeTypedef();
			++Element;
			--Count;
		}
	}
}

/** Copy-constructs Count elements at Dest from Source (memcpy when bitwise constructible). */
template <typename DestinationElementType, typename SourceElementType, typename SizeType>
FORCEINLINE void ConstructItems(void* Dest, const SourceElementType* Source, SizeType Count)
{
	if constexpr (TIsBitwiseConstructible<DestinationElementType, SourceElementType>::Value)
	{
		if (Count)
		{
			FMemory::Memcpy(Dest, Source, sizeof(SourceElementType) * Count);
		}
	}
	else
	{
		while (Count)
		{
			new (Dest) DestinationElementType(*Source);
			++(DestinationElementType*&)Dest;
			++Source;
			--Count;
		}
	}
}

/** Copy-assigns Count elements from Source to Dest. */
template <typename ElementType, typename SizeType>
FORCEINLINE void CopyAssignItems(ElementType* Dest, const ElementType* Source, SizeType Count)
{
	if constexpr (std::is_trivially_copy_assignable_v<ElementType>)
	{
		if (Count)
		{
			FMemory::Memcpy(Dest, Source, sizeof(ElementType) * Count);
		}
	}
	else
	{
		while (Count)
		{
			*Dest = *Source;
			++Dest;
			++Source;
			--Count;
		}
	}
}

/**
 * Moves Count elements from Source to Dest and ends the source objects' lifetime. Like UE, Leon containers treat
 * element types as trivially relocatable: a memmove. Do not store types that point into themselves.
 */
template <typename DestinationElementType, typename SourceElementType, typename SizeType>
FORCEINLINE void RelocateConstructItems(void* Dest, const SourceElementType* Source, SizeType Count)
{
	static_assert(std::is_same_v<std::remove_cv_t<DestinationElementType>, std::remove_cv_t<SourceElementType>>,
		"Relocation between different types needs a converting container");
	if (Count)
	{
		FMemory::Memmove(Dest, Source, sizeof(SourceElementType) * Count);
	}
}

/** Move-constructs Count elements at Dest from Source (the sources stay valid, moved-from). */
template <typename ElementType, typename SizeType>
FORCEINLINE void MoveConstructItems(void* Dest, const ElementType* Source, SizeType Count)
{
	if constexpr (std::is_trivially_copy_constructible_v<ElementType>)
	{
		if (Count)
		{
			FMemory::Memmove(Dest, Source, sizeof(ElementType) * Count);
		}
	}
	else
	{
		while (Count)
		{
			new (Dest) ElementType((ElementType&&)*Source);
			++(ElementType*&)Dest;
			++Source;
			--Count;
		}
	}
}

/** Move-assigns Count elements from Source to Dest. */
template <typename ElementType, typename SizeType>
FORCEINLINE void MoveAssignItems(ElementType* Dest, const ElementType* Source, SizeType Count)
{
	if constexpr (std::is_trivially_copy_assignable_v<ElementType>)
	{
		if (Count)
		{
			FMemory::Memmove(Dest, Source, sizeof(ElementType) * Count);
		}
	}
	else
	{
		while (Count)
		{
			*Dest = (ElementType&&)*Source;
			++Dest;
			++Source;
			--Count;
		}
	}
}

/** Element-wise equality (memcmp for integers, enums and pointers). */
template <typename ElementType, typename SizeType>
FORCEINLINE bool CompareItems(const ElementType* A, const ElementType* B, SizeType Count)
{
	if constexpr (TIsBytewiseComparable<ElementType>::Value)
	{
		return Count == 0 || !FMemory::Memcmp(A, B, sizeof(ElementType) * Count);
	}
	else
	{
		while (Count)
		{
			if (!(*A == *B))
			{
				return false;
			}
			++A;
			++B;
			--Count;
		}
		return true;
	}
}
