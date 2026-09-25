#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformAtomics.h"
#include "Misc/AssertionMacros.h"
#include "Templates/MemoryOps.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"

#include <cstddef>
#include <new>
#include <type_traits>

// Reference-counted smart pointers (UE: Templates/SharedPointer.h):
//   TSharedRef<T>  never null
//   TSharedPtr<T>  may be null
//   TWeakPtr<T>    does not keep the object alive; Pin() for a TSharedPtr
// ESPMode::ThreadSafe counts with atomics; NotThreadSafe (the default, like UE's ESPMode::Fast) does not.

enum class ESPMode : uint8
{
	NotThreadSafe = 0,
	Fast = NotThreadSafe,
	ThreadSafe = 1
};

template <class ObjectType, ESPMode Mode = ESPMode::Fast>
class TSharedRef;
template <class ObjectType, ESPMode Mode = ESPMode::Fast>
class TSharedPtr;
template <class ObjectType, ESPMode Mode = ESPMode::Fast>
class TWeakPtr;
template <class ObjectType, ESPMode Mode = ESPMode::Fast>
class TSharedFromThis;

template <class CastToType, class CastFromType, ESPMode Mode>
TSharedRef<CastToType, Mode> StaticCastSharedRef(TSharedRef<CastFromType, Mode> const& InSharedRef);

namespace SharedPointerInternals
{
	struct FStaticCastTag
	{
	};
	struct FConstCastTag
	{
	};
	struct FNullTag
	{
	};

	/** Shared + weak counts; the weak count holds one extra reference while shared references exist (UE). */
	class FReferenceControllerBase
	{
	public:
		FReferenceControllerBase() = default;
		FReferenceControllerBase(const FReferenceControllerBase&) = delete;
		FReferenceControllerBase& operator=(const FReferenceControllerBase&) = delete;
		virtual ~FReferenceControllerBase() = default;

		/** Destroys the object; the controller stays until the last weak reference. */
		virtual void DestroyObject() = 0;

		int32 SharedReferenceCount = 1;
		int32 WeakReferenceCount = 1;
	};

	/** Controller of an object allocated elsewhere, destroyed with a deleter (UE: TReferenceControllerWithDeleter). */
	template <typename ObjectType, typename DeleterType>
	class TReferenceControllerWithDeleter
		: private DeleterType
		, public FReferenceControllerBase
	{
	public:
		explicit TReferenceControllerWithDeleter(ObjectType* InObject, DeleterType&& Deleter)
			: DeleterType(MoveTemp(Deleter))
			, Object(InObject)
		{
		}

		virtual void DestroyObject() override
		{
			(*static_cast<DeleterType*>(this))(Object);
		}

	private:
		ObjectType* Object;
	};

	/** Controller that holds the object itself: MakeShared makes one allocation (UE: TIntrusiveReferenceController). */
	template <typename ObjectType>
	class TIntrusiveReferenceController : public FReferenceControllerBase
	{
	public:
		template <typename... ArgTypes>
		explicit TIntrusiveReferenceController(ArgTypes&&... Args)
		{
			new ((void*)&ObjectStorage) ObjectType(Forward<ArgTypes>(Args)...);
		}

		ObjectType* GetObjectPtr() const
		{
			return (ObjectType*)&ObjectStorage;
		}

		virtual void DestroyObject() override
		{
			DestructItem((ObjectType*)&ObjectStorage);
		}

	private:
		mutable TTypeCompatibleBytes<ObjectType> ObjectStorage;
	};

	template <typename Type>
	struct DefaultDeleter
	{
		FORCEINLINE void operator()(Type* Object) const
		{
			delete Object;
		}
	};

	template <typename ObjectType, typename DeleterType>
	FORCEINLINE FReferenceControllerBase* NewCustomReferenceController(ObjectType* Object, DeleterType&& Deleter)
	{
		return new TReferenceControllerWithDeleter<ObjectType, std::remove_reference_t<DeleterType>>(
			Object, Forward<DeleterType>(Deleter));
	}

	template <typename ObjectType>
	FORCEINLINE FReferenceControllerBase* NewDefaultReferenceController(ObjectType* Object)
	{
		return new TReferenceControllerWithDeleter<ObjectType, DefaultDeleter<ObjectType>>(
			Object, DefaultDeleter<ObjectType>());
	}

	template <typename ObjectType, typename... ArgTypes>
	FORCEINLINE TIntrusiveReferenceController<ObjectType>* NewIntrusiveReferenceController(ArgTypes&&... Args)
	{
		return new TIntrusiveReferenceController<ObjectType>(Forward<ArgTypes>(Args)...);
	}

	/** Raw pointer + optional deleter handed to a TSharedPtr / TSharedRef (UE: FRawPtrProxy, MakeShareable). */
	template <class ObjectType>
	struct FRawPtrProxy
	{
		ObjectType* Object;
		FReferenceControllerBase* ReferenceController;

		FORCEINLINE FRawPtrProxy(std::nullptr_t)
			: Object(nullptr)
			, ReferenceController(nullptr)
		{
		}

		FORCEINLINE explicit FRawPtrProxy(ObjectType* InObject)
			: Object(InObject)
			, ReferenceController(NewDefaultReferenceController(InObject))
		{
		}

		template <class Deleter>
		FORCEINLINE explicit FRawPtrProxy(ObjectType* InObject, Deleter&& InDeleter)
			: Object(InObject)
			, ReferenceController(NewCustomReferenceController(InObject, Forward<Deleter>(InDeleter)))
		{
		}
	};

	/** Count operations for a thread-safety mode (UE: FReferenceControllerOps). */
	template <ESPMode Mode>
	struct FReferenceControllerOps;

	template <>
	struct FReferenceControllerOps<ESPMode::NotThreadSafe>
	{
		static FORCEINLINE int32 GetSharedReferenceCount(const FReferenceControllerBase* ReferenceController)
		{
			return ReferenceController->SharedReferenceCount;
		}

		static FORCEINLINE void AddSharedReference(FReferenceControllerBase* ReferenceController)
		{
			++ReferenceController->SharedReferenceCount;
		}

		/** Adds a shared reference unless the object is already destroyed (used by TWeakPtr::Pin). */
		static bool ConditionallyAddSharedReference(FReferenceControllerBase* ReferenceController)
		{
			if (ReferenceController->SharedReferenceCount == 0)
			{
				return false;
			}
			++ReferenceController->SharedReferenceCount;
			return true;
		}

		static FORCEINLINE void ReleaseSharedReference(FReferenceControllerBase* ReferenceController)
		{
			checkSlow(ReferenceController->SharedReferenceCount > 0);
			if (--ReferenceController->SharedReferenceCount == 0)
			{
				// Last shared reference was released! Destroy the referenced object.
				ReferenceController->DestroyObject();

				// No more shared referencers, so decrement the weak reference count by one. When the weak reference
				// count reaches zero, this object will be deleted.
				ReleaseWeakReference(ReferenceController);
			}
		}

		static FORCEINLINE void AddWeakReference(FReferenceControllerBase* ReferenceController)
		{
			++ReferenceController->WeakReferenceCount;
		}

		static void ReleaseWeakReference(FReferenceControllerBase* ReferenceController)
		{
			checkSlow(ReferenceController->WeakReferenceCount > 0);
			if (--ReferenceController->WeakReferenceCount == 0)
			{
				// No more references to this reference count. Destroy it!
				delete ReferenceController;
			}
		}
	};

	template <>
	struct FReferenceControllerOps<ESPMode::ThreadSafe>
	{
		static FORCEINLINE int32 GetSharedReferenceCount(const FReferenceControllerBase* ReferenceController)
		{
			return FPlatformAtomics::AtomicRead(&ReferenceController->SharedReferenceCount);
		}

		static FORCEINLINE void AddSharedReference(FReferenceControllerBase* ReferenceController)
		{
			FPlatformAtomics::InterlockedIncrement(&ReferenceController->SharedReferenceCount);
		}

		static bool ConditionallyAddSharedReference(FReferenceControllerBase* ReferenceController)
		{
			for (;;)
			{
				// Peek at the current shared reference count. Remember, this value may be updated by multiple threads.
				const int32 OriginalCount = FPlatformAtomics::AtomicRead(&ReferenceController->SharedReferenceCount);
				if (OriginalCount == 0)
				{
					// Never add a shared reference if the pointer has already expired.
					return false;
				}

				// Attempt to increment the reference count.
				const int32 ActualOriginalCount = FPlatformAtomics::InterlockedCompareExchange(
					&ReferenceController->SharedReferenceCount, OriginalCount + 1, OriginalCount);

				// We need to make sure that we never revive a counter that has already expired, so if the actual value
				// what we expected (because it was touched by another thread), then we'll try again.
				if (ActualOriginalCount == OriginalCount)
				{
					return true;
				}
			}
		}

		static FORCEINLINE void ReleaseSharedReference(FReferenceControllerBase* ReferenceController)
		{
			if (FPlatformAtomics::InterlockedDecrement(&ReferenceController->SharedReferenceCount) == 1)
			{
				ReferenceController->DestroyObject();
				ReleaseWeakReference(ReferenceController);
			}
		}

		static FORCEINLINE void AddWeakReference(FReferenceControllerBase* ReferenceController)
		{
			FPlatformAtomics::InterlockedIncrement(&ReferenceController->WeakReferenceCount);
		}

		static void ReleaseWeakReference(FReferenceControllerBase* ReferenceController)
		{
			if (FPlatformAtomics::InterlockedDecrement(&ReferenceController->WeakReferenceCount) == 1)
			{
				delete ReferenceController;
			}
		}
	};

	/** Holds one shared reference (UE: FSharedReferencer). */
	template <ESPMode Mode>
	class FSharedReferencer
	{
		typedef FReferenceControllerOps<Mode> TOps;

	public:
		FORCEINLINE FSharedReferencer()
			: ReferenceController(nullptr)
		{
		}

		explicit FORCEINLINE FSharedReferencer(FReferenceControllerBase* InReferenceController)
			: ReferenceController(InReferenceController)
		{
		}

		FORCEINLINE FSharedReferencer(const FSharedReferencer& InSharedReference)
			: ReferenceController(InSharedReference.ReferenceController)
		{
			if (ReferenceController != nullptr)
			{
				TOps::AddSharedReference(ReferenceController);
			}
		}

		FORCEINLINE FSharedReferencer(FSharedReferencer&& InSharedReference)
			: ReferenceController(InSharedReference.ReferenceController)
		{
			InSharedReference.ReferenceController = nullptr;
		}

		FORCEINLINE ~FSharedReferencer()
		{
			if (ReferenceController != nullptr)
			{
				TOps::ReleaseSharedReference(ReferenceController);
			}
		}

		inline FSharedReferencer& operator=(const FSharedReferencer& InSharedReference)
		{
			// Make sure we're not be reassigned to ourself!
			FReferenceControllerBase* NewReferenceController = InSharedReference.ReferenceController;
			if (NewReferenceController != ReferenceController)
			{
				// First, add a shared reference to the new object.
				if (NewReferenceController != nullptr)
				{
					TOps::AddSharedReference(NewReferenceController);
				}

				// Release shared reference to the old object.
				if (ReferenceController != nullptr)
				{
					TOps::ReleaseSharedReference(ReferenceController);
				}

				// Assume ownership of the assigned reference counter.
				ReferenceController = NewReferenceController;
			}
			return *this;
		}

		inline FSharedReferencer& operator=(FSharedReferencer&& InSharedReference)
		{
			FReferenceControllerBase* NewReferenceController = InSharedReference.ReferenceController;
			FReferenceControllerBase* OldReferenceController = ReferenceController;
			if (NewReferenceController != OldReferenceController)
			{
				// Assume ownership of the assigned reference counter.
				InSharedReference.ReferenceController = nullptr;
				ReferenceController = NewReferenceController;

				// Release shared reference to the old object.
				if (OldReferenceController != nullptr)
				{
					TOps::ReleaseSharedReference(OldReferenceController);
				}
			}
			return *this;
		}

		FORCEINLINE bool IsValid() const
		{
			return ReferenceController != nullptr;
		}

		FORCEINLINE int32 GetSharedReferenceCount() const
		{
			return ReferenceController != nullptr ? TOps::GetSharedReferenceCount(ReferenceController) : 0;
		}

		FORCEINLINE bool IsUnique() const
		{
			return GetSharedReferenceCount() == 1;
		}

	private:
		template <ESPMode OtherMode>
		friend class FWeakReferencer;

		FReferenceControllerBase* ReferenceController;
	};

	/** Holds one weak reference (UE: FWeakReferencer). */
	template <ESPMode Mode>
	class FWeakReferencer
	{
		typedef FReferenceControllerOps<Mode> TOps;

	public:
		FORCEINLINE FWeakReferencer()
			: ReferenceController(nullptr)
		{
		}

		FORCEINLINE FWeakReferencer(const FWeakReferencer& InWeakRefCountPointer)
			: ReferenceController(InWeakRefCountPointer.ReferenceController)
		{
			if (ReferenceController != nullptr)
			{
				TOps::AddWeakReference(ReferenceController);
			}
		}

		FORCEINLINE FWeakReferencer(FWeakReferencer&& InWeakRefCountPointer)
			: ReferenceController(InWeakRefCountPointer.ReferenceController)
		{
			InWeakRefCountPointer.ReferenceController = nullptr;
		}

		FORCEINLINE FWeakReferencer(const FSharedReferencer<Mode>& InSharedRefCountPointer)
			: ReferenceController(InSharedRefCountPointer.ReferenceController)
		{
			if (ReferenceController != nullptr)
			{
				TOps::AddWeakReference(ReferenceController);
			}
		}

		FORCEINLINE ~FWeakReferencer()
		{
			if (ReferenceController != nullptr)
			{
				TOps::ReleaseWeakReference(ReferenceController);
			}
		}

		FORCEINLINE FWeakReferencer& operator=(const FWeakReferencer& InWeakReference)
		{
			AssignReferenceController(InWeakReference.ReferenceController);
			return *this;
		}

		FORCEINLINE FWeakReferencer& operator=(FWeakReferencer&& InWeakReference)
		{
			FReferenceControllerBase* OldReferenceController = ReferenceController;
			ReferenceController = InWeakReference.ReferenceController;
			InWeakReference.ReferenceController = nullptr;
			if (OldReferenceController != nullptr)
			{
				TOps::ReleaseWeakReference(OldReferenceController);
			}
			return *this;
		}

		FORCEINLINE FWeakReferencer& operator=(const FSharedReferencer<Mode>& InSharedReference)
		{
			AssignReferenceController(InSharedReference.ReferenceController);
			return *this;
		}

		/** True while the object is alive. */
		FORCEINLINE bool IsValid() const
		{
			return ReferenceController != nullptr && TOps::GetSharedReferenceCount(ReferenceController) > 0;
		}

	private:
		inline void AssignReferenceController(FReferenceControllerBase* NewReferenceController)
		{
			// Only proceed if the new reference counter is different than our current.
			if (NewReferenceController != ReferenceController)
			{
				// First, add a weak reference to the new object.
				if (NewReferenceController != nullptr)
				{
					TOps::AddWeakReference(NewReferenceController);
				}

				// Release a weak reference to the old object.
				if (ReferenceController != nullptr)
				{
					TOps::ReleaseWeakReference(ReferenceController);
				}

				// Assume ownership of the assigned reference counter.
				ReferenceController = NewReferenceController;
			}
		}

		template <ESPMode OtherMode>
		friend class FSharedReferencer;

	public:
		/** Pins: returns a controller with one more shared reference, or nullptr when expired. */
		FReferenceControllerBase* PinController() const
		{
			if (ReferenceController != nullptr && TOps::ConditionallyAddSharedReference(ReferenceController))
			{
				return ReferenceController;
			}
			return nullptr;
		}

	private:
		FReferenceControllerBase* ReferenceController;
	};

	/** Checks that a pointer converts (UE: TIsDerivedFrom-based static checks). */
	template <typename From, typename To>
	constexpr bool TPointerIsConvertibleFromTo = std::is_convertible_v<From*, To*>;
} // namespace SharedPointerInternals

/** Wraps a raw pointer for TSharedPtr / TSharedRef construction (UE: MakeShareable). */
template <class ObjectType>
FORCEINLINE SharedPointerInternals::FRawPtrProxy<ObjectType> MakeShareable(ObjectType* InObject)
{
	return SharedPointerInternals::FRawPtrProxy<ObjectType>(InObject);
}

template <class ObjectType, class DeleterType>
FORCEINLINE SharedPointerInternals::FRawPtrProxy<ObjectType> MakeShareable(
	ObjectType* InObject, DeleterType&& InDeleter)
{
	return SharedPointerInternals::FRawPtrProxy<ObjectType>(InObject, Forward<DeleterType>(InDeleter));
}

namespace SharedPointerInternals
{
	// TSharedFromThis hook: a new shared pointer tells the object about itself.
	template <class SharedPtrType, class ObjectType, class OtherType, ESPMode Mode>
	FORCEINLINE void EnableSharedFromThis(TSharedPtr<SharedPtrType, Mode> const* InSharedPtr,
		ObjectType const* InObject, TSharedFromThis<OtherType, Mode> const* InShareable);
	template <class SharedRefType, class ObjectType, class OtherType, ESPMode Mode>
	FORCEINLINE void EnableSharedFromThis(TSharedRef<SharedRefType, Mode> const* InSharedRef,
		ObjectType const* InObject, TSharedFromThis<OtherType, Mode> const* InShareable);
	inline void EnableSharedFromThis(...)
	{
	}
} // namespace SharedPointerInternals

/** Non-null shared reference (UE: TSharedRef). */
template <class ObjectType, ESPMode Mode>
class TSharedRef
{
public:
	using ElementType = ObjectType;

	/** Takes ownership of a new object. */
	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE explicit TSharedRef(OtherType* InObject)
		: Object(InObject)
		, SharedReferenceCount(SharedPointerInternals::NewDefaultReferenceController(InObject))
	{
		Init(InObject);
	}

	template <typename OtherType, typename DeleterType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TSharedRef(OtherType* InObject, DeleterType&& InDeleter)
		: Object(InObject)
		, SharedReferenceCount(
			  SharedPointerInternals::NewCustomReferenceController(InObject, Forward<DeleterType>(InDeleter)))
	{
		Init(InObject);
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TSharedRef(SharedPointerInternals::FRawPtrProxy<OtherType> const& InRawPtrProxy)
		: Object(InRawPtrProxy.Object)
		, SharedReferenceCount(InRawPtrProxy.ReferenceController)
	{
		// If the following assert goes off, it means a TSharedRef was initialized from a nullptr object pointer.
		// Shared references must never be nullptr, so either pass a valid object or consider using TSharedPtr instead.
		check(InRawPtrProxy.Object != nullptr);

		// If the object happens to be derived from TSharedFromThis, the following method
		// will prime the object with a weak pointer to itself.
		SharedPointerInternals::EnableSharedFromThis(this, InRawPtrProxy.Object, InRawPtrProxy.Object);
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TSharedRef(TSharedRef<OtherType, Mode> const& InSharedRef)
		: Object(InSharedRef.Object)
		, SharedReferenceCount(InSharedRef.SharedReferenceCount)
	{
	}

	template <typename OtherType>
	FORCEINLINE TSharedRef(TSharedRef<OtherType, Mode> const& InSharedRef, SharedPointerInternals::FStaticCastTag)
		: Object(static_cast<ObjectType*>(InSharedRef.Object))
		, SharedReferenceCount(InSharedRef.SharedReferenceCount)
	{
	}

	template <typename OtherType>
	FORCEINLINE TSharedRef(TSharedRef<OtherType, Mode> const& InSharedRef, SharedPointerInternals::FConstCastTag)
		: Object(const_cast<ObjectType*>(InSharedRef.Object))
		, SharedReferenceCount(InSharedRef.SharedReferenceCount)
	{
	}

	/** Aliasing constructor: shares ownership with OtherSharedRef but points at InObject. */
	template <typename OtherType>
	FORCEINLINE TSharedRef(TSharedRef<OtherType, Mode> const& OtherSharedRef, ObjectType* InObject)
		: Object(InObject)
		, SharedReferenceCount(OtherSharedRef.SharedReferenceCount)
	{
		check(InObject != nullptr);
	}

	FORCEINLINE TSharedRef(TSharedRef const& InSharedRef)
		: Object(InSharedRef.Object)
		, SharedReferenceCount(InSharedRef.SharedReferenceCount)
	{
	}

	FORCEINLINE TSharedRef(TSharedRef&& InSharedRef)
		: Object(InSharedRef.Object)
		, SharedReferenceCount(InSharedRef.SharedReferenceCount)
	{
		// We're intentionally not moving here, because we don't want to leave InSharedRef in a null state, because
		// that breaks the class invariant. But we provide a move constructor anyway in case the compiler complains
		// that we have a move assign but no move construct.
	}

	FORCEINLINE TSharedRef& operator=(TSharedRef const& InSharedRef)
	{
		TSharedRef Temp = InSharedRef;
		::Swap(Temp, *this);
		return *this;
	}

	FORCEINLINE TSharedRef& operator=(TSharedRef&& InSharedRef)
	{
		SwapMembers(InSharedRef);
		return *this;
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TSharedRef& operator=(SharedPointerInternals::FRawPtrProxy<OtherType> const& InRawPtrProxy)
	{
		// If the following assert goes off, it means a TSharedRef was initialized from a nullptr object pointer.
		check(InRawPtrProxy.Object != nullptr);
		*this = TSharedRef<ObjectType, Mode>(InRawPtrProxy);
		return *this;
	}

	FORCEINLINE TSharedPtr<ObjectType, Mode> ToSharedPtr() const
	{
		return TSharedPtr<ObjectType, Mode>(*this);
	}

	FORCEINLINE ObjectType& Get() const
	{
		return *Object;
	}

	FORCEINLINE ObjectType& operator*() const
	{
		return *Object;
	}

	FORCEINLINE ObjectType* operator->() const
	{
		return Object;
	}

	FORCEINLINE int32 GetSharedReferenceCount() const
	{
		return SharedReferenceCount.GetSharedReferenceCount();
	}

	FORCEINLINE bool IsUnique() const
	{
		return SharedReferenceCount.IsUnique();
	}

private:
	template <class OtherType>
	void Init(OtherType* InObject)
	{
		// If the following assert goes off, it means a TSharedRef was initialized from a nullptr object pointer.
		check(InObject != nullptr);
		SharedPointerInternals::EnableSharedFromThis(this, InObject, InObject);
	}

	template <class OtherType, ESPMode OtherMode>
	friend class TSharedRef;
	template <class OtherType, ESPMode OtherMode>
	friend class TSharedPtr;
	template <class OtherType, ESPMode OtherMode>
	friend class TWeakPtr;

	template <class OtherType, typename... ArgTypes>
	friend TSharedRef<OtherType, ESPMode::Fast> MakeShared(ArgTypes&&... Args);
	template <class OtherType, ESPMode OtherMode, typename... ArgTypes>
	friend TSharedRef<OtherType, OtherMode> MakeSharedWithMode(ArgTypes&&... Args);

	/** Used by MakeShared: the controller holds the object. */
	FORCEINLINE explicit TSharedRef(
		ObjectType* InObject, SharedPointerInternals::FReferenceControllerBase* InSharedReferenceCount)
		: Object(InObject)
		, SharedReferenceCount(InSharedReferenceCount)
	{
		Init(InObject);
	}

	/** Converts from a valid TSharedPtr (used by TSharedPtr::ToSharedRef). */
	template <typename OtherType>
	FORCEINLINE explicit TSharedRef(TSharedPtr<OtherType, Mode> const& InSharedPtr)
		: Object(InSharedPtr.Object)
		, SharedReferenceCount(InSharedPtr.SharedReferenceCount)
	{
		// If this expression goes off, it means a shared reference was created from a shared pointer that was
		// nullptr. Shared references are never allowed to be null. Consider using TSharedPtr instead.
		check(IsValid());
	}

	template <typename OtherType>
	FORCEINLINE explicit TSharedRef(TSharedPtr<OtherType, Mode>&& InSharedPtr)
		: Object(InSharedPtr.Object)
		, SharedReferenceCount(MoveTemp(InSharedPtr.SharedReferenceCount))
	{
		InSharedPtr.Object = nullptr;
		check(IsValid());
	}

	FORCEINLINE bool IsValid() const
	{
		return Object != nullptr;
	}

	FORCEINLINE void SwapMembers(TSharedRef& Other)
	{
		ObjectType* TempObject = Object;
		Object = Other.Object;
		Other.Object = TempObject;
		SharedPointerInternals::FSharedReferencer<Mode> TempCount = MoveTemp(SharedReferenceCount);
		SharedReferenceCount = MoveTemp(Other.SharedReferenceCount);
		Other.SharedReferenceCount = MoveTemp(TempCount);
	}

	ObjectType* Object;
	SharedPointerInternals::FSharedReferencer<Mode> SharedReferenceCount;
};

/** Nullable shared pointer (UE: TSharedPtr). */
template <class ObjectType, ESPMode Mode>
class TSharedPtr
{
public:
	using ElementType = ObjectType;

	FORCEINLINE TSharedPtr(SharedPointerInternals::FNullTag* = nullptr)
		: Object(nullptr)
		, SharedReferenceCount()
	{
	}

	FORCEINLINE TSharedPtr(std::nullptr_t)
		: Object(nullptr)
		, SharedReferenceCount()
	{
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE explicit TSharedPtr(OtherType* InObject)
		: Object(InObject)
		, SharedReferenceCount(InObject ? SharedPointerInternals::NewDefaultReferenceController(InObject) : nullptr)
	{
		SharedPointerInternals::EnableSharedFromThis(this, InObject, InObject);
	}

	template <typename OtherType, typename DeleterType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TSharedPtr(OtherType* InObject, DeleterType&& InDeleter)
		: Object(InObject)
		, SharedReferenceCount(InObject
				  ? SharedPointerInternals::NewCustomReferenceController(InObject, Forward<DeleterType>(InDeleter))
				  : nullptr)
	{
		SharedPointerInternals::EnableSharedFromThis(this, InObject, InObject);
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TSharedPtr(SharedPointerInternals::FRawPtrProxy<OtherType> const& InRawPtrProxy)
		: Object(InRawPtrProxy.Object)
		, SharedReferenceCount(InRawPtrProxy.ReferenceController)
	{
		SharedPointerInternals::EnableSharedFromThis(this, InRawPtrProxy.Object, InRawPtrProxy.Object);
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TSharedPtr(TSharedPtr<OtherType, Mode> const& InSharedPtr)
		: Object(InSharedPtr.Object)
		, SharedReferenceCount(InSharedPtr.SharedReferenceCount)
	{
	}

	FORCEINLINE TSharedPtr(TSharedPtr const& InSharedPtr)
		: Object(InSharedPtr.Object)
		, SharedReferenceCount(InSharedPtr.SharedReferenceCount)
	{
	}

	FORCEINLINE TSharedPtr(TSharedPtr&& InSharedPtr)
		: Object(InSharedPtr.Object)
		, SharedReferenceCount(MoveTemp(InSharedPtr.SharedReferenceCount))
	{
		InSharedPtr.Object = nullptr;
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TSharedPtr(TSharedRef<OtherType, Mode> const& InSharedRef)
		: Object(InSharedRef.Object)
		, SharedReferenceCount(InSharedRef.SharedReferenceCount)
	{
		// There is no rvalue overload of this constructor, because 'stealing' the pointer from a TSharedRef would
		// leave it as null, which would invalidate its invariant.
	}

	template <typename OtherType>
	FORCEINLINE TSharedPtr(TSharedPtr<OtherType, Mode> const& InSharedPtr, SharedPointerInternals::FStaticCastTag)
		: Object(static_cast<ObjectType*>(InSharedPtr.Object))
		, SharedReferenceCount(InSharedPtr.SharedReferenceCount)
	{
	}

	template <typename OtherType>
	FORCEINLINE TSharedPtr(TSharedPtr<OtherType, Mode> const& InSharedPtr, SharedPointerInternals::FConstCastTag)
		: Object(const_cast<ObjectType*>(InSharedPtr.Object))
		, SharedReferenceCount(InSharedPtr.SharedReferenceCount)
	{
	}

	/** Aliasing constructor. */
	template <typename OtherType>
	FORCEINLINE TSharedPtr(TSharedPtr<OtherType, Mode> const& OtherSharedPtr, ObjectType* InObject)
		: Object(InObject)
		, SharedReferenceCount(OtherSharedPtr.SharedReferenceCount)
	{
	}

	template <typename OtherType>
	FORCEINLINE TSharedPtr(TSharedRef<OtherType, Mode> const& OtherSharedRef, ObjectType* InObject)
		: Object(InObject)
		, SharedReferenceCount(OtherSharedRef.SharedReferenceCount)
	{
	}

	FORCEINLINE TSharedPtr& operator=(std::nullptr_t)
	{
		Reset();
		return *this;
	}

	FORCEINLINE TSharedPtr& operator=(TSharedPtr const& InSharedPtr)
	{
		TSharedPtr Temp = InSharedPtr;
		::Swap(Temp, *this);
		return *this;
	}

	FORCEINLINE TSharedPtr& operator=(TSharedPtr&& InSharedPtr)
	{
		if (this != &InSharedPtr)
		{
			Object = InSharedPtr.Object;
			InSharedPtr.Object = nullptr;
			SharedReferenceCount = MoveTemp(InSharedPtr.SharedReferenceCount);
		}
		return *this;
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TSharedPtr& operator=(SharedPointerInternals::FRawPtrProxy<OtherType> const& InRawPtrProxy)
	{
		*this = TSharedPtr<ObjectType, Mode>(InRawPtrProxy);
		return *this;
	}

	/** The object as a TSharedRef; the pointer must be valid. */
	FORCEINLINE TSharedRef<ObjectType, Mode> ToSharedRef() const&
	{
		// If this assert goes off, it means a shared reference was created from a shared pointer that was nullptr.
		// Shared references are never allowed to be null. Consider using TSharedPtr instead.
		check(IsValid());
		return TSharedRef<ObjectType, Mode>(*this);
	}

	FORCEINLINE TSharedRef<ObjectType, Mode> ToSharedRef() &&
	{
		check(IsValid());
		return TSharedRef<ObjectType, Mode>(MoveTemp(*this));
	}

	FORCEINLINE ObjectType* Get() const
	{
		return Object;
	}

	FORCEINLINE explicit operator bool() const
	{
		return Object != nullptr;
	}

	FORCEINLINE bool IsValid() const
	{
		return Object != nullptr;
	}

	FORCEINLINE ObjectType& operator*() const
	{
		check(IsValid());
		return *Object;
	}

	FORCEINLINE ObjectType* operator->() const
	{
		check(IsValid());
		return Object;
	}

	/** Releases the reference and becomes null. */
	FORCEINLINE void Reset()
	{
		*this = TSharedPtr<ObjectType, Mode>();
	}

	FORCEINLINE int32 GetSharedReferenceCount() const
	{
		return SharedReferenceCount.GetSharedReferenceCount();
	}

	FORCEINLINE bool IsUnique() const
	{
		return SharedReferenceCount.IsUnique();
	}

private:
	/** Pins a weak pointer (used by TWeakPtr::Pin). */
	template <typename OtherType>
	FORCEINLINE explicit TSharedPtr(TWeakPtr<OtherType, Mode> const& InWeakPtr)
		: Object(nullptr)
		, SharedReferenceCount(InWeakPtr.WeakReferenceCount.PinController())
	{
		// Check that the shared reference was created from the weak reference successfully. We'll only cache a
		// pointer to the object if we have a valid shared reference.
		if (SharedReferenceCount.IsValid())
		{
			Object = InWeakPtr.Object;
		}
	}

	template <class OtherType, ESPMode OtherMode>
	friend class TSharedPtr;
	template <class OtherType, ESPMode OtherMode>
	friend class TSharedRef;
	template <class OtherType, ESPMode OtherMode>
	friend class TWeakPtr;
	template <class OtherType, ESPMode OtherMode>
	friend class TSharedFromThis;

	ObjectType* Object;
	SharedPointerInternals::FSharedReferencer<Mode> SharedReferenceCount;
};

/** Weak reference: does not keep the object alive (UE: TWeakPtr). */
template <class ObjectType, ESPMode Mode>
class TWeakPtr
{
public:
	using ElementType = ObjectType;

	FORCEINLINE TWeakPtr(SharedPointerInternals::FNullTag* = nullptr)
		: Object(nullptr)
		, WeakReferenceCount()
	{
	}

	FORCEINLINE TWeakPtr(std::nullptr_t)
		: Object(nullptr)
		, WeakReferenceCount()
	{
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TWeakPtr(TSharedRef<OtherType, Mode> const& InSharedRef)
		: Object(InSharedRef.Object)
		, WeakReferenceCount(InSharedRef.SharedReferenceCount)
	{
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TWeakPtr(TSharedPtr<OtherType, Mode> const& InSharedPtr)
		: Object(InSharedPtr.Object)
		, WeakReferenceCount(InSharedPtr.SharedReferenceCount)
	{
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TWeakPtr(TWeakPtr<OtherType, Mode> const& InWeakPtr)
		: Object(InWeakPtr.Object)
		, WeakReferenceCount(InWeakPtr.WeakReferenceCount)
	{
	}

	FORCEINLINE TWeakPtr(TWeakPtr const& InWeakPtr)
		: Object(InWeakPtr.Object)
		, WeakReferenceCount(InWeakPtr.WeakReferenceCount)
	{
	}

	FORCEINLINE TWeakPtr(TWeakPtr&& InWeakPtr)
		: Object(InWeakPtr.Object)
		, WeakReferenceCount(MoveTemp(InWeakPtr.WeakReferenceCount))
	{
		InWeakPtr.Object = nullptr;
	}

	FORCEINLINE TWeakPtr& operator=(std::nullptr_t)
	{
		Reset();
		return *this;
	}

	FORCEINLINE TWeakPtr& operator=(TWeakPtr const& InWeakPtr)
	{
		TWeakPtr Temp = InWeakPtr;
		::Swap(Temp, *this);
		return *this;
	}

	FORCEINLINE TWeakPtr& operator=(TWeakPtr&& InWeakPtr)
	{
		if (this != &InWeakPtr)
		{
			Object = InWeakPtr.Object;
			InWeakPtr.Object = nullptr;
			WeakReferenceCount = MoveTemp(InWeakPtr.WeakReferenceCount);
		}
		return *this;
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TWeakPtr& operator=(TSharedRef<OtherType, Mode> const& InSharedRef)
	{
		Object = InSharedRef.Object;
		WeakReferenceCount = InSharedRef.SharedReferenceCount;
		return *this;
	}

	template <typename OtherType,
		std::enable_if_t<SharedPointerInternals::TPointerIsConvertibleFromTo<OtherType, ObjectType>, int> = 0>
	FORCEINLINE TWeakPtr& operator=(TSharedPtr<OtherType, Mode> const& InSharedPtr)
	{
		Object = InSharedPtr.Object;
		WeakReferenceCount = InSharedPtr.SharedReferenceCount;
		return *this;
	}

	/** A shared pointer to the object, or an empty one when it is gone. */
	FORCEINLINE TSharedPtr<ObjectType, Mode> Pin() const
	{
		return TSharedPtr<ObjectType, Mode>(*this);
	}

	FORCEINLINE bool IsValid() const
	{
		return Object != nullptr && WeakReferenceCount.IsValid();
	}

	FORCEINLINE void Reset()
	{
		*this = TWeakPtr<ObjectType, Mode>();
	}

	/** True when this weak pointer refers to InObject (even if the object is gone). */
	FORCEINLINE bool HasSameObject(const void* InOtherPtr) const
	{
		return Pin().Get() == InOtherPtr;
	}

private:
	template <class OtherType, ESPMode OtherMode>
	friend class TWeakPtr;
	template <class OtherType, ESPMode OtherMode>
	friend class TSharedPtr;

	ObjectType* Object;
	SharedPointerInternals::FWeakReferencer<Mode> WeakReferenceCount;
};

/** Lets an object hand out shared references to itself (UE: TSharedFromThis). */
template <class ObjectType, ESPMode Mode>
class TSharedFromThis
{
public:
	/** A shared reference to this object; it must be owned by a shared pointer / reference already. */
	TSharedRef<ObjectType, Mode> AsShared()
	{
		TSharedPtr<ObjectType, Mode> SharedThis(WeakThis.Pin());
		// If the following assert goes off, it means one of the following:
		// - You tried to request a shared pointer before the object was ever assigned to one.
		// - You tried to request a shared pointer while the object is being destroyed.
		check(SharedThis.Get() == this);
		return MoveTemp(SharedThis).ToSharedRef();
	}

	TSharedRef<ObjectType const, Mode> AsShared() const
	{
		TSharedPtr<ObjectType const, Mode> SharedThis(WeakThis);
		check(SharedThis.Get() == this);
		return MoveTemp(SharedThis).ToSharedRef();
	}

	/** Called by the shared pointers; primes the weak self-reference once. */
	template <class SharedPtrType, class OtherType>
	FORCEINLINE void UpdateWeakReferenceInternal(
		TSharedPtr<SharedPtrType, Mode> const* InSharedPtr, OtherType* InObject) const
	{
		if (!WeakThis.IsValid())
		{
			WeakThis = TSharedPtr<ObjectType, Mode>(*InSharedPtr, InObject);
		}
	}

	template <class SharedRefType, class OtherType>
	FORCEINLINE void UpdateWeakReferenceInternal(
		TSharedRef<SharedRefType, Mode> const* InSharedRef, OtherType* InObject) const
	{
		if (!WeakThis.IsValid())
		{
			WeakThis = TSharedRef<ObjectType, Mode>(*InSharedRef, InObject);
		}
	}

	/** True once the object is owned by a shared pointer and still alive. */
	FORCEINLINE bool DoesSharedInstanceExist() const
	{
		return WeakThis.IsValid();
	}

protected:
	/** Shared reference to a subclass of this object. */
	template <class OtherType>
	FORCEINLINE static TSharedRef<OtherType, Mode> SharedThis(OtherType* ThisPtr)
	{
		return StaticCastSharedRef<OtherType>(ThisPtr->AsShared());
	}

	template <class OtherType>
	FORCEINLINE static TSharedRef<OtherType const, Mode> SharedThis(const OtherType* ThisPtr)
	{
		return StaticCastSharedRef<OtherType const>(ThisPtr->AsShared());
	}

	TSharedFromThis() = default;
	TSharedFromThis(TSharedFromThis const&)
	{
	}
	FORCEINLINE TSharedFromThis& operator=(TSharedFromThis const&)
	{
		return *this;
	}
	~TSharedFromThis() = default;

private:
	mutable TWeakPtr<ObjectType, Mode> WeakThis;
};

namespace SharedPointerInternals
{
	template <class SharedPtrType, class ObjectType, class OtherType, ESPMode Mode>
	FORCEINLINE void EnableSharedFromThis(TSharedPtr<SharedPtrType, Mode> const* InSharedPtr,
		ObjectType const* InObject, TSharedFromThis<OtherType, Mode> const* InShareable)
	{
		if (InShareable != nullptr)
		{
			InShareable->UpdateWeakReferenceInternal(InSharedPtr, const_cast<ObjectType*>(InObject));
		}
	}

	template <class SharedRefType, class ObjectType, class OtherType, ESPMode Mode>
	FORCEINLINE void EnableSharedFromThis(TSharedRef<SharedRefType, Mode> const* InSharedRef,
		ObjectType const* InObject, TSharedFromThis<OtherType, Mode> const* InShareable)
	{
		if (InShareable != nullptr)
		{
			InShareable->UpdateWeakReferenceInternal(InSharedRef, const_cast<ObjectType*>(InObject));
		}
	}
} // namespace SharedPointerInternals

template <class ObjectTypeA, class ObjectTypeB, ESPMode Mode>
FORCEINLINE bool operator==(
	TSharedRef<ObjectTypeA, Mode> const& InSharedRefA, TSharedRef<ObjectTypeB, Mode> const& InSharedRefB)
{
	return &(InSharedRefA.Get()) == &(InSharedRefB.Get());
}
template <class ObjectTypeA, class ObjectTypeB, ESPMode Mode>
FORCEINLINE bool operator!=(
	TSharedRef<ObjectTypeA, Mode> const& InSharedRefA, TSharedRef<ObjectTypeB, Mode> const& InSharedRefB)
{
	return &(InSharedRefA.Get()) != &(InSharedRefB.Get());
}
template <class ObjectTypeA, class ObjectTypeB, ESPMode Mode>
FORCEINLINE bool operator==(
	TSharedPtr<ObjectTypeA, Mode> const& InSharedPtrA, TSharedPtr<ObjectTypeB, Mode> const& InSharedPtrB)
{
	return InSharedPtrA.Get() == InSharedPtrB.Get();
}
template <class ObjectTypeA, class ObjectTypeB, ESPMode Mode>
FORCEINLINE bool operator!=(
	TSharedPtr<ObjectTypeA, Mode> const& InSharedPtrA, TSharedPtr<ObjectTypeB, Mode> const& InSharedPtrB)
{
	return InSharedPtrA.Get() != InSharedPtrB.Get();
}
template <class ObjectTypeA, class ObjectTypeB, ESPMode Mode>
FORCEINLINE bool operator==(
	TSharedRef<ObjectTypeA, Mode> const& InSharedRef, TSharedPtr<ObjectTypeB, Mode> const& InSharedPtr)
{
	return InSharedPtr.IsValid() && InSharedPtr.Get() == &(InSharedRef.Get());
}
template <class ObjectTypeA, class ObjectTypeB, ESPMode Mode>
FORCEINLINE bool operator==(
	TSharedPtr<ObjectTypeB, Mode> const& InSharedPtr, TSharedRef<ObjectTypeA, Mode> const& InSharedRef)
{
	return InSharedRef == InSharedPtr;
}
template <class ObjectType, ESPMode Mode>
FORCEINLINE bool operator==(TSharedPtr<ObjectType, Mode> const& InSharedPtr, std::nullptr_t)
{
	return !InSharedPtr.IsValid();
}
template <class ObjectType, ESPMode Mode>
FORCEINLINE bool operator==(std::nullptr_t, TSharedPtr<ObjectType, Mode> const& InSharedPtr)
{
	return !InSharedPtr.IsValid();
}
template <class ObjectType, ESPMode Mode>
FORCEINLINE bool operator!=(TSharedPtr<ObjectType, Mode> const& InSharedPtr, std::nullptr_t)
{
	return InSharedPtr.IsValid();
}
template <class ObjectType, ESPMode Mode>
FORCEINLINE bool operator!=(std::nullptr_t, TSharedPtr<ObjectType, Mode> const& InSharedPtr)
{
	return InSharedPtr.IsValid();
}
template <class ObjectTypeA, class ObjectTypeB, ESPMode Mode>
FORCEINLINE bool operator==(
	TWeakPtr<ObjectTypeA, Mode> const& InWeakPtrA, TWeakPtr<ObjectTypeB, Mode> const& InWeakPtrB)
{
	return InWeakPtrA.Pin().Get() == InWeakPtrB.Pin().Get();
}
template <class ObjectTypeA, class ObjectTypeB, ESPMode Mode>
FORCEINLINE bool operator!=(
	TWeakPtr<ObjectTypeA, Mode> const& InWeakPtrA, TWeakPtr<ObjectTypeB, Mode> const& InWeakPtrB)
{
	return InWeakPtrA.Pin().Get() != InWeakPtrB.Pin().Get();
}

template <class ObjectType, ESPMode Mode>
FORCEINLINE uint32 GetTypeHash(const TSharedRef<ObjectType, Mode>& InSharedRef)
{
	return ::PointerHash(&InSharedRef.Get());
}
template <class ObjectType, ESPMode Mode>
FORCEINLINE uint32 GetTypeHash(const TSharedPtr<ObjectType, Mode>& InSharedPtr)
{
	return ::PointerHash(InSharedPtr.Get());
}
template <class ObjectType, ESPMode Mode>
FORCEINLINE uint32 GetTypeHash(const TWeakPtr<ObjectType, Mode>& InWeakPtr)
{
	return ::PointerHash(InWeakPtr.Pin().Get());
}

template <class CastToType, class CastFromType, ESPMode Mode>
FORCEINLINE TSharedRef<CastToType, Mode> StaticCastSharedRef(TSharedRef<CastFromType, Mode> const& InSharedRef)
{
	return TSharedRef<CastToType, Mode>(InSharedRef, SharedPointerInternals::FStaticCastTag());
}
template <class CastToType, class CastFromType, ESPMode Mode>
FORCEINLINE TSharedPtr<CastToType, Mode> StaticCastSharedPtr(TSharedPtr<CastFromType, Mode> const& InSharedPtr)
{
	return TSharedPtr<CastToType, Mode>(InSharedPtr, SharedPointerInternals::FStaticCastTag());
}
template <class CastToType, class CastFromType, ESPMode Mode>
FORCEINLINE TSharedRef<CastToType, Mode> ConstCastSharedRef(TSharedRef<CastFromType, Mode> const& InSharedRef)
{
	return TSharedRef<CastToType, Mode>(InSharedRef, SharedPointerInternals::FConstCastTag());
}
template <class CastToType, class CastFromType, ESPMode Mode>
FORCEINLINE TSharedPtr<CastToType, Mode> ConstCastSharedPtr(TSharedPtr<CastFromType, Mode> const& InSharedPtr)
{
	return TSharedPtr<CastToType, Mode>(InSharedPtr, SharedPointerInternals::FConstCastTag());
}

/** Allocates the object and its reference counts in one block (UE: MakeShared). */
template <typename InObjectType, typename... InArgTypes>
FORCEINLINE TSharedRef<InObjectType, ESPMode::Fast> MakeShared(InArgTypes&&... Args)
{
	SharedPointerInternals::TIntrusiveReferenceController<InObjectType>* Controller =
		SharedPointerInternals::NewIntrusiveReferenceController<InObjectType>(Forward<InArgTypes>(Args)...);
	return TSharedRef<InObjectType, ESPMode::Fast>(
		Controller->GetObjectPtr(), (SharedPointerInternals::FReferenceControllerBase*)Controller);
}

/** MakeShared for an explicit thread-safety mode (UE: MakeShared<T, Mode>). */
template <typename InObjectType, ESPMode InMode, typename... InArgTypes>
FORCEINLINE TSharedRef<InObjectType, InMode> MakeSharedWithMode(InArgTypes&&... Args)
{
	SharedPointerInternals::TIntrusiveReferenceController<InObjectType>* Controller =
		SharedPointerInternals::NewIntrusiveReferenceController<InObjectType>(Forward<InArgTypes>(Args)...);
	return TSharedRef<InObjectType, InMode>(
		Controller->GetObjectPtr(), (SharedPointerInternals::FReferenceControllerBase*)Controller);
}
