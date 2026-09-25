#pragma once

#include "CoreTypes.h"

// Forward declarations and default template arguments of the containers (UE: Containers/ContainersFwd.h).

template <int IndexSize>
class TSizedDefaultAllocator;
using FDefaultAllocator = TSizedDefaultAllocator<32>;
class FDefaultSetAllocator;
class FDefaultBitArrayAllocator;
class FDefaultSparseArrayAllocator;

class FString;
class FName;
class FText;

template <typename T, typename Allocator = FDefaultAllocator>
class TArray;

template <typename T = void>
struct TLess;

template <typename InElementType>
class TArrayView;
template <typename InElementType>
using TConstArrayView = TArrayView<const InElementType>;

template <typename Allocator = FDefaultBitArrayAllocator>
class TBitArray;
template <typename Allocator = FDefaultBitArrayAllocator>
class TConstSetBitIterator;

template <typename InElementType, typename Allocator = FDefaultSparseArrayAllocator>
class TSparseArray;

template <typename ElementType, bool bInAllowDuplicateKeys = false>
struct DefaultKeyFuncs;

template <typename ElementType, typename KeyFuncs = DefaultKeyFuncs<ElementType>,
	typename Allocator = FDefaultSetAllocator>
class TSet;

template <typename KeyType, typename ValueType, bool bInAllowDuplicateKeys>
struct TDefaultMapKeyFuncs;
template <typename KeyType, typename ValueType, bool bInAllowDuplicateKeys = false>
struct TDefaultMapHashableKeyFuncs;

template <typename InKeyType, typename InValueType, typename SetAllocator = FDefaultSetAllocator,
	typename KeyFuncs = TDefaultMapHashableKeyFuncs<InKeyType, InValueType, false>>
class TMap;

template <typename KeyType, typename ValueType, typename SetAllocator = FDefaultSetAllocator,
	typename KeyFuncs = TDefaultMapHashableKeyFuncs<KeyType, ValueType, true>>
class TMultiMap;
