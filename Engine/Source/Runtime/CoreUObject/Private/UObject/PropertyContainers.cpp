// FArrayProperty, FSetProperty, FMapProperty and their script helpers (UE: PropertyArray.cpp, PropertySet.cpp,
// PropertyMap.cpp). The values are Core's TArray / TSet / TMap, handled through the layout-compatible FScriptArray /
// FScriptSet / FScriptMap.

#include "Serialization/Archive.h"
#include "UObject/PropertyHelpers.h"
#include "UObject/PropertyTag.h"
#include "UObject/UnrealType.h"

#include <new>

// CheckConstraints compares member offsets of the containers (offsetof), as UE does.
#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

using namespace UE::CoreUObject::Private;

DEFINE_LOG_CATEGORY_STATIC(LogProperty, Log, All);

namespace
{
	/** Checks at compile time that the script containers are drop-in views of the Core containers. */
	[[maybe_unused]] void CheckScriptContainerLayouts()
	{
		FScriptArray::CheckConstraints();
		TScriptBitArray<FDefaultBitArrayAllocator>::CheckConstraints();
		FScriptSparseArray::CheckConstraints();
		FScriptSet::CheckConstraints();
		FScriptMap::CheckConstraints();
	}

	/** Element size of a container element property (its whole value: containers of C arrays are not allowed). */
	FORCEINLINE int32 GetElementSize(const FProperty* Property)
	{
		return Property->ElementSize;
	}

	/** Skips '(' at the start of a container value; reports a missing one. */
	const TCHAR* OpenParenthesis(const FProperty* Property, const TCHAR* Buffer, FOutputDevice* ErrorText)
	{
		Buffer = SkipWhitespace(Buffer);
		if (*Buffer != '(')
		{
			ReportImportError(ErrorText,
				FString::Printf(
					TEXT("%s: expected '(' to start a %s value"), *Property->GetName(), *Property->GetCPPType()));
			return nullptr;
		}
		return SkipWhitespace(Buffer + 1);
	}

	/** After an element: skips ',' or stops at ')'; reports anything else. */
	const TCHAR* NextElement(const FProperty* Property, const TCHAR* Buffer, FOutputDevice* ErrorText)
	{
		Buffer = SkipWhitespace(Buffer);
		if (*Buffer == ',')
		{
			return SkipWhitespace(Buffer + 1);
		}
		if (*Buffer != ')')
		{
			ReportImportError(ErrorText, FString::Printf(TEXT("%s: expected ',' or ')'"), *Property->GetName()));
			return nullptr;
		}
		return Buffer;
	}

	/** A value of Property in scratch memory, for parsing set elements and map pairs before adding them. */
	class FScratchValue
	{
	public:
		explicit FScratchValue(const FProperty* InProperty)
			: Property(InProperty)
			, Memory((uint8*)FMemory::Malloc(SIZE_T(InProperty->ElementSize), uint32(InProperty->GetMinAlignment())))
		{
			Property->InitializeValue(Memory);
		}

		~FScratchValue()
		{
			Property->DestroyValue(Memory);
			FMemory::Free(Memory);
		}

		FScratchValue(const FScratchValue&) = delete;
		FScratchValue& operator=(const FScratchValue&) = delete;

		uint8* Get() const
		{
			return Memory;
		}

	private:
		const FProperty* Property;
		uint8* Memory;
	};

	/**
	 * Checks a loaded element count: every element takes at least one byte, so a count above what is left of the
	 * archive is corrupt data (a critical error) rather than something to allocate.
	 */
	bool IsLoadedCountValid(const FProperty* Property, FArchive& Ar, int32 Num)
	{
		const int64 Remaining = Ar.TotalSize() != INDEX_NONE ? Ar.TotalSize() - Ar.Tell() : int64(Num);
		if (Num < 0 || Num > Remaining)
		{
			UE_LOG(LogProperty, Error, TEXT("%s: %s has an invalid element count %d"), *Ar.GetArchiveName(),
				*Property->GetName(), Num);
			Ar.SetCriticalError();
			return false;
		}
		return true;
	}
} // namespace

// FArrayProperty

IMPLEMENT_FIELD(FArrayProperty)

FArrayProperty::FArrayProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FProperty(InOwner, InName, InObjectFlags)
	, Inner(nullptr)
	, ArrayFlags(EArrayPropertyFlags::None)
{
	SetFieldClass(StaticClass());
	ElementSize = sizeof(FScriptArray);
}

FArrayProperty::FArrayProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
	EPropertyFlags InFlags, EArrayPropertyFlags InArrayPropertyFlags)
	: FProperty(InOwner, InName, InObjectFlags, InOffset, InFlags)
	, Inner(nullptr)
	, ArrayFlags(InArrayPropertyFlags)
{
	SetFieldClass(StaticClass());
	ElementSize = sizeof(FScriptArray);
}

FArrayProperty::~FArrayProperty()
{
	delete Inner;
}

void FArrayProperty::AddCppProperty(FProperty* Property)
{
	checkf(!Inner, "Array property %s already has an inner property", *GetName());
	Inner = Property;
}

FString FArrayProperty::GetCPPType() const
{
	return FString::Printf(TEXT("TArray<%s>"), Inner ? *Inner->GetCPPType() : TEXT("void"));
}

int32 FArrayProperty::GetMinAlignment() const
{
	return alignof(FScriptArray);
}

bool FArrayProperty::ContainsObjectReference(
	TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType) const
{
	return Inner && Inner->ContainsObjectReference(EncounteredStructProps, InReferenceType);
}

bool FArrayProperty::SameType(const FProperty* Other) const
{
	return FProperty::SameType(Other) && Inner && Inner->SameType(((const FArrayProperty*)Other)->Inner);
}

void FArrayProperty::LinkInternal(FArchive& Ar)
{
	checkf(Inner, "Array property %s has no inner property", *GetName());
	Inner->LinkWithoutChangingOffset(Ar);
	checkf(Inner->GetMinAlignment() <= 16, "Array property %s: elements aligned above 16 bytes are not supported",
		*GetName());
	ElementSize = sizeof(FScriptArray);
	// An empty TArray is all zeros; it needs its destructor (UE).
	PropertyFlags |= CPF_ZeroConstructor;
	PropertyFlags &= ~(CPF_IsPlainOldData | CPF_NoDestructor);
}

bool FArrayProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	FScriptArrayHelper ArrayHelperA(this, A);
	const int32 ArrayNum = ArrayHelperA.Num();
	if (!B)
	{
		return ArrayNum == 0;
	}
	FScriptArrayHelper ArrayHelperB(this, B);
	if (ArrayNum != ArrayHelperB.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < ArrayNum; ++Index)
	{
		if (!Inner->Identical(ArrayHelperA.GetRawPtr(Index), ArrayHelperB.GetRawPtr(Index), PortFlags))
		{
			return false;
		}
	}
	return true;
}

void FArrayProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	FScriptArrayHelper ArrayHelper(this, PropertyValue);
	ValueStr += TEXT("(");
	for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
	{
		if (Index > 0)
		{
			ValueStr += TEXT(",");
		}
		Inner->ExportTextItem(
			ValueStr, ArrayHelper.GetRawPtr(Index), nullptr, Parent, PortFlags | PPF_Delimited, ExportRootScope);
	}
	ValueStr += TEXT(")");
}

const TCHAR* FArrayProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	// (Element,Element,...)
	Buffer = OpenParenthesis(this, Buffer, ErrorText);
	if (!Buffer)
	{
		return nullptr;
	}
	FScriptArrayHelper ArrayHelper(this, Data);
	ArrayHelper.EmptyValues();
	while (*Buffer != ')')
	{
		const int32 Index = ArrayHelper.AddValue();
		Buffer =
			Inner->ImportText(Buffer, ArrayHelper.GetRawPtr(Index), PortFlags | PPF_Delimited, OwnerObject, ErrorText);
		Buffer = Buffer ? NextElement(this, Buffer, ErrorText) : nullptr;
		if (!Buffer)
		{
			return nullptr;
		}
	}
	return Buffer + 1;
}

void FArrayProperty::CopyValuesInternal(void* Dest, void const* Src, int32 Count) const
{
	for (int32 ValueIndex = 0; ValueIndex < Count; ++ValueIndex)
	{
		void* DestArray = (uint8*)Dest + ValueIndex * ElementSize;
		const void* SrcArray = (const uint8*)Src + ValueIndex * ElementSize;
		FScriptArrayHelper SrcArrayHelper(this, SrcArray);
		FScriptArrayHelper DestArrayHelper(this, DestArray);
		const int32 Num = SrcArrayHelper.Num();
		if (Inner->HasAnyPropertyFlags(CPF_IsPlainOldData))
		{
			DestArrayHelper.EmptyValues(Num);
			if (Num)
			{
				DestArrayHelper.AddValues(Num);
				FMemory::Memcpy(DestArrayHelper.GetRawPtr(), SrcArrayHelper.GetRawPtr(),
					SIZE_T(Num) * SIZE_T(GetElementSize(Inner)));
			}
		}
		else
		{
			DestArrayHelper.Resize(Num);
			for (int32 Index = 0; Index < Num; ++Index)
			{
				Inner->CopyCompleteValue(DestArrayHelper.GetRawPtr(Index), SrcArrayHelper.GetRawPtr(Index));
			}
		}
	}
}

void FArrayProperty::ClearValueInternal(void* Data) const
{
	FScriptArrayHelper ArrayHelper(this, Data);
	ArrayHelper.EmptyValues();
}

void FArrayProperty::DestroyValueInternal(void* Dest) const
{
	for (int32 Index = 0; Index < ArrayDim; ++Index)
	{
		void* Array = (uint8*)Dest + Index * ElementSize;
		FScriptArrayHelper ArrayHelper(this, Array);
		ArrayHelper.EmptyValues();
		((FScriptArray*)Array)->~FScriptArray();
	}
}

void FArrayProperty::InitializeValueInternal(void* Dest) const
{
	for (int32 Index = 0; Index < ArrayDim; ++Index)
	{
		::new ((uint8*)Dest + Index * ElementSize) FScriptArray();
	}
}

// FScriptArrayHelper

FScriptArrayHelper::FScriptArrayHelper(const FArrayProperty* InProperty, const void* InArray)
	: FScriptArrayHelper(InProperty->Inner, InArray, GetElementSize(InProperty->Inner))
{
}

FScriptArrayHelper::FScriptArrayHelper(const FProperty* InInnerProperty, const void* InArray, int32 InElementSize)
	: InnerProperty(InInnerProperty)
	, HeapArray((FScriptArray*)InArray)
	, ElementSize(InElementSize)
{
	checkf(ElementSize > 0, "Array element property %s has no size", *InInnerProperty->GetName());
}

FScriptArrayHelper FScriptArrayHelper::CreateHelperFormInnerProperty(
	const FProperty* InInnerProperty, const void* InArray)
{
	return FScriptArrayHelper(InInnerProperty, InArray, GetElementSize(InInnerProperty));
}

void FScriptArrayHelper::EmptyValues(int32 Slack)
{
	check(Slack >= 0);
	const int32 OldNum = Num();
	if (OldNum)
	{
		DestructItems(0, OldNum);
	}
	if (OldNum || Slack != HeapArray->Max())
	{
		HeapArray->Empty(Slack, ElementSize);
	}
}

void FScriptArrayHelper::EmptyAndAddValues(int32 Count)
{
	check(Count >= 0);
	DestructItems(0, Num());
	HeapArray->Empty(Count, ElementSize);
	if (Count)
	{
		AddValues(Count);
	}
}

int32 FScriptArrayHelper::AddValue()
{
	return AddValues(1);
}

int32 FScriptArrayHelper::AddValues(int32 Count)
{
	const int32 OldNum = HeapArray->Add(Count, ElementSize);
	ConstructItems(OldNum, Count);
	return OldNum;
}

void FScriptArrayHelper::InsertValues(int32 Index, int32 Count)
{
	check(Index >= 0 && Index <= Num());
	HeapArray->Insert(Index, Count, ElementSize);
	ConstructItems(Index, Count);
}

void FScriptArrayHelper::Resize(int32 NewNum)
{
	check(NewNum >= 0);
	const int32 OldNum = Num();
	if (NewNum > OldNum)
	{
		AddValues(NewNum - OldNum);
	}
	else if (NewNum < OldNum)
	{
		RemoveValues(NewNum, OldNum - NewNum);
	}
}

void FScriptArrayHelper::RemoveValues(int32 Index, int32 Count)
{
	check(Index >= 0 && Index + Count <= Num());
	DestructItems(Index, Count);
	HeapArray->Remove(Index, Count, ElementSize);
}

void FScriptArrayHelper::SwapValues(int32 A, int32 B)
{
	HeapArray->SwapMemory(A, B, ElementSize);
}

void FScriptArrayHelper::ConstructItems(int32 Index, int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	uint8* Dest = (uint8*)HeapArray->GetData() + Index * ElementSize;
	if (InnerProperty->HasAnyPropertyFlags(CPF_ZeroConstructor))
	{
		FMemory::Memzero(Dest, SIZE_T(Count) * SIZE_T(ElementSize));
	}
	else
	{
		for (int32 LoopIndex = 0; LoopIndex < Count; ++LoopIndex, Dest += ElementSize)
		{
			InnerProperty->InitializeValue(Dest);
		}
	}
}

void FScriptArrayHelper::DestructItems(int32 Index, int32 Count)
{
	if (Count <= 0 || InnerProperty->HasAnyPropertyFlags(CPF_IsPlainOldData | CPF_NoDestructor))
	{
		return;
	}
	uint8* Dest = (uint8*)HeapArray->GetData() + Index * ElementSize;
	for (int32 LoopIndex = 0; LoopIndex < Count; ++LoopIndex, Dest += ElementSize)
	{
		InnerProperty->DestroyValue(Dest);
	}
}

// FSetProperty

IMPLEMENT_FIELD(FSetProperty)

FSetProperty::FSetProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FProperty(InOwner, InName, InObjectFlags)
	, ElementProp(nullptr)
	, SetLayout()
{
	SetFieldClass(StaticClass());
	ElementSize = sizeof(FScriptSet);
}

FSetProperty::FSetProperty(
	FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset, EPropertyFlags InFlags)
	: FProperty(InOwner, InName, InObjectFlags, InOffset, InFlags)
	, ElementProp(nullptr)
	, SetLayout()
{
	SetFieldClass(StaticClass());
	ElementSize = sizeof(FScriptSet);
}

FSetProperty::~FSetProperty()
{
	delete ElementProp;
}

void FSetProperty::AddCppProperty(FProperty* Property)
{
	checkf(!ElementProp, "Set property %s already has an element property", *GetName());
	ElementProp = Property;
}

FString FSetProperty::GetCPPType() const
{
	return FString::Printf(TEXT("TSet<%s>"), ElementProp ? *ElementProp->GetCPPType() : TEXT("void"));
}

int32 FSetProperty::GetMinAlignment() const
{
	return alignof(FScriptSet);
}

bool FSetProperty::ContainsObjectReference(
	TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType) const
{
	return ElementProp && ElementProp->ContainsObjectReference(EncounteredStructProps, InReferenceType);
}

bool FSetProperty::SameType(const FProperty* Other) const
{
	return FProperty::SameType(Other) && ElementProp &&
		ElementProp->SameType(((const FSetProperty*)Other)->ElementProp);
}

void FSetProperty::LinkInternal(FArchive& Ar)
{
	checkf(ElementProp, "Set property %s has no element property", *GetName());
	ElementProp->LinkWithoutChangingOffset(Ar);
	checkf(ElementProp->HasAnyPropertyFlags(CPF_HasGetValueTypeHash),
		"Set property %s: its element type %s has no GetTypeHash", *GetName(), *ElementProp->GetCPPType());
	SetLayout = FScriptSet::GetScriptLayout(GetElementSize(ElementProp), ElementProp->GetMinAlignment());
	ElementSize = sizeof(FScriptSet);
	PropertyFlags &= ~(CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor);
}

bool FSetProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	FScriptSetHelper SetHelperA(this, A);
	const int32 NumA = SetHelperA.Num();
	if (!B)
	{
		return NumA == 0;
	}
	FScriptSetHelper SetHelperB(this, B);
	if (NumA != SetHelperB.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < SetHelperA.GetMaxIndex(); ++Index)
	{
		if (SetHelperA.IsValidIndex(Index))
		{
			const int32 IndexB = SetHelperB.FindElementIndex(SetHelperA.GetElementPtr(Index));
			if (IndexB == INDEX_NONE ||
				!ElementProp->Identical(SetHelperA.GetElementPtr(Index), SetHelperB.GetElementPtr(IndexB), PortFlags))
			{
				return false;
			}
		}
	}
	return true;
}

void FSetProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	FScriptSetHelper SetHelper(this, PropertyValue);
	ValueStr += TEXT("(");
	bool bFirst = true;
	for (int32 Index = 0; Index < SetHelper.GetMaxIndex(); ++Index)
	{
		if (!SetHelper.IsValidIndex(Index))
		{
			continue;
		}
		if (!bFirst)
		{
			ValueStr += TEXT(",");
		}
		bFirst = false;
		ElementProp->ExportTextItem(
			ValueStr, SetHelper.GetElementPtr(Index), nullptr, Parent, PortFlags | PPF_Delimited, ExportRootScope);
	}
	ValueStr += TEXT(")");
}

const TCHAR* FSetProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	// (Element,Element,...)
	Buffer = OpenParenthesis(this, Buffer, ErrorText);
	if (!Buffer)
	{
		return nullptr;
	}
	FScriptSetHelper SetHelper(this, Data);
	SetHelper.EmptyElements();
	FScratchValue Element(ElementProp);
	while (*Buffer != ')')
	{
		Buffer = ElementProp->ImportText(Buffer, Element.Get(), PortFlags | PPF_Delimited, OwnerObject, ErrorText);
		Buffer = Buffer ? NextElement(this, Buffer, ErrorText) : nullptr;
		if (!Buffer)
		{
			return nullptr;
		}
		SetHelper.AddElement(Element.Get());
	}
	return Buffer + 1;
}

void FSetProperty::CopyValuesInternal(void* Dest, void const* Src, int32 Count) const
{
	for (int32 ValueIndex = 0; ValueIndex < Count; ++ValueIndex)
	{
		void* DestSet = (uint8*)Dest + ValueIndex * ElementSize;
		const void* SrcSet = (const uint8*)Src + ValueIndex * ElementSize;
		if (DestSet == SrcSet)
		{
			continue;
		}
		FScriptSetHelper SrcSetHelper(this, SrcSet);
		FScriptSetHelper DestSetHelper(this, DestSet);
		DestSetHelper.EmptyElements(SrcSetHelper.Num());
		for (int32 SrcIndex = 0; SrcIndex < SrcSetHelper.GetMaxIndex(); ++SrcIndex)
		{
			if (SrcSetHelper.IsValidIndex(SrcIndex))
			{
				const int32 DestIndex = DestSetHelper.AddDefaultValue_Invalid_NeedsRehash();
				ElementProp->CopyCompleteValue(
					DestSetHelper.GetElementPtr(DestIndex), SrcSetHelper.GetElementPtr(SrcIndex));
			}
		}
		DestSetHelper.Rehash();
	}
}

void FSetProperty::ClearValueInternal(void* Data) const
{
	FScriptSetHelper SetHelper(this, Data);
	SetHelper.EmptyElements();
}

void FSetProperty::DestroyValueInternal(void* Dest) const
{
	for (int32 Index = 0; Index < ArrayDim; ++Index)
	{
		void* Set = (uint8*)Dest + Index * ElementSize;
		FScriptSetHelper SetHelper(this, Set);
		SetHelper.EmptyElements();
		((FScriptSet*)Set)->~FScriptSet();
	}
}

void FSetProperty::InitializeValueInternal(void* Dest) const
{
	for (int32 Index = 0; Index < ArrayDim; ++Index)
	{
		::new ((uint8*)Dest + Index * ElementSize) FScriptSet();
	}
}

// FScriptSetHelper

FScriptSetHelper::FScriptSetHelper(const FSetProperty* InProperty, const void* InSet)
	: ElementProp(InProperty->ElementProp)
	, Set((FScriptSet*)InSet)
	, SetLayout(InProperty->SetLayout)
{
	checkf(SetLayout.Size > 0, "Set property %s is not linked", *InProperty->GetName());
}

void FScriptSetHelper::EmptyElements(int32 Slack)
{
	check(Slack >= 0);
	const int32 OldNum = Num();
	if (OldNum && !ElementProp->HasAnyPropertyFlags(CPF_IsPlainOldData | CPF_NoDestructor))
	{
		for (int32 Index = 0; Index < GetMaxIndex(); ++Index)
		{
			if (IsValidIndex(Index))
			{
				ElementProp->DestroyValue(GetElementPtr(Index));
			}
		}
	}
	if (OldNum || Slack)
	{
		Set->Empty(Slack, SetLayout);
	}
}

int32 FScriptSetHelper::AddDefaultValue_Invalid_NeedsRehash()
{
	const int32 Result = Set->AddUninitialized(SetLayout);
	ElementProp->InitializeValue(GetElementPtr(Result));
	return Result;
}

void FScriptSetHelper::Rehash()
{
	const FProperty* LocalElementProp = ElementProp;
	Set->Rehash(SetLayout, [LocalElementProp](const void* Src) { return LocalElementProp->GetValueTypeHash(Src); });
}

void FScriptSetHelper::RemoveAt(int32 Index, int32 Count)
{
	check(IsValidIndex(Index));
	for (; Count; ++Index)
	{
		if (IsValidIndex(Index))
		{
			ElementProp->DestroyValue(GetElementPtr(Index));
			Set->RemoveAt(Index, SetLayout);
			--Count;
		}
	}
}

int32 FScriptSetHelper::FindElementIndex(const void* ElementToFind) const
{
	const FProperty* LocalElementProp = ElementProp;
	return Set->FindIndex(
		ElementToFind, SetLayout,
		[LocalElementProp](const void* Element) { return LocalElementProp->GetValueTypeHash(Element); },
		[LocalElementProp](const void* A, const void* B) { return LocalElementProp->Identical(A, B); });
}

void FScriptSetHelper::AddElement(const void* ElementToAdd)
{
	const FProperty* LocalElementProp = ElementProp;
	Set->Add(
		ElementToAdd, SetLayout,
		[LocalElementProp](const void* Element) { return LocalElementProp->GetValueTypeHash(Element); },
		[LocalElementProp](const void* A, const void* B) { return LocalElementProp->Identical(A, B); },
		[LocalElementProp, ElementToAdd](void* NewElement)
		{
			LocalElementProp->InitializeValue(NewElement);
			LocalElementProp->CopySingleValue(NewElement, ElementToAdd);
		},
		[LocalElementProp](void* Element) { LocalElementProp->DestroyValue(Element); });
}

bool FScriptSetHelper::RemoveElement(const void* ElementToRemove)
{
	const int32 Index = FindElementIndex(ElementToRemove);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	RemoveAt(Index);
	return true;
}

// FMapProperty

IMPLEMENT_FIELD(FMapProperty)

FMapProperty::FMapProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags)
	: FProperty(InOwner, InName, InObjectFlags)
	, KeyProp(nullptr)
	, ValueProp(nullptr)
	, MapLayout()
	, MapFlags(EMapPropertyFlags::None)
{
	SetFieldClass(StaticClass());
	ElementSize = sizeof(FScriptMap);
}

FMapProperty::FMapProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags, int32 InOffset,
	EPropertyFlags InFlags, EMapPropertyFlags InMapFlags)
	: FProperty(InOwner, InName, InObjectFlags, InOffset, InFlags)
	, KeyProp(nullptr)
	, ValueProp(nullptr)
	, MapLayout()
	, MapFlags(InMapFlags)
{
	SetFieldClass(StaticClass());
	ElementSize = sizeof(FScriptMap);
}

FMapProperty::~FMapProperty()
{
	delete KeyProp;
	delete ValueProp;
}

void FMapProperty::AddCppProperty(FProperty* Property)
{
	if (!KeyProp)
	{
		KeyProp = Property;
	}
	else
	{
		checkf(!ValueProp, "Map property %s already has a key and a value property", *GetName());
		ValueProp = Property;
	}
}

FString FMapProperty::GetCPPType() const
{
	return FString::Printf(TEXT("TMap<%s, %s>"), KeyProp ? *KeyProp->GetCPPType() : TEXT("void"),
		ValueProp ? *ValueProp->GetCPPType() : TEXT("void"));
}

int32 FMapProperty::GetMinAlignment() const
{
	return alignof(FScriptMap);
}

bool FMapProperty::ContainsObjectReference(
	TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType) const
{
	return (KeyProp && KeyProp->ContainsObjectReference(EncounteredStructProps, InReferenceType)) ||
		(ValueProp && ValueProp->ContainsObjectReference(EncounteredStructProps, InReferenceType));
}

bool FMapProperty::SameType(const FProperty* Other) const
{
	const FMapProperty* OtherMap = (const FMapProperty*)Other;
	return FProperty::SameType(Other) && KeyProp && ValueProp && KeyProp->SameType(OtherMap->KeyProp) &&
		ValueProp->SameType(OtherMap->ValueProp);
}

void FMapProperty::LinkInternal(FArchive& Ar)
{
	checkf(KeyProp && ValueProp, "Map property %s needs a key and a value property", *GetName());
	KeyProp->LinkWithoutChangingOffset(Ar);
	ValueProp->LinkWithoutChangingOffset(Ar);
	checkf(KeyProp->HasAnyPropertyFlags(CPF_HasGetValueTypeHash), "Map property %s: its key type %s has no GetTypeHash",
		*GetName(), *KeyProp->GetCPPType());
	MapLayout = FScriptMap::GetScriptLayout(
		GetElementSize(KeyProp), KeyProp->GetMinAlignment(), GetElementSize(ValueProp), ValueProp->GetMinAlignment());
	ElementSize = sizeof(FScriptMap);
	PropertyFlags &= ~(CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor);
}

bool FMapProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	FScriptMapHelper MapHelperA(this, A);
	const int32 NumA = MapHelperA.Num();
	if (!B)
	{
		return NumA == 0;
	}
	FScriptMapHelper MapHelperB(this, B);
	if (NumA != MapHelperB.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < MapHelperA.GetMaxIndex(); ++Index)
	{
		if (MapHelperA.IsValidIndex(Index))
		{
			const int32 IndexB = MapHelperB.FindMapIndexWithKey(MapHelperA.GetKeyPtr(Index));
			if (IndexB == INDEX_NONE ||
				!ValueProp->Identical(MapHelperA.GetValuePtr(Index), MapHelperB.GetValuePtr(IndexB), PortFlags))
			{
				return false;
			}
		}
	}
	return true;
}

void FMapProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue,
	UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	(void)DefaultValue;
	FScriptMapHelper MapHelper(this, PropertyValue);
	ValueStr += TEXT("(");
	bool bFirst = true;
	for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); ++Index)
	{
		if (!MapHelper.IsValidIndex(Index))
		{
			continue;
		}
		if (!bFirst)
		{
			ValueStr += TEXT(",");
		}
		bFirst = false;
		ValueStr += TEXT("(");
		KeyProp->ExportTextItem(
			ValueStr, MapHelper.GetKeyPtr(Index), nullptr, Parent, PortFlags | PPF_Delimited, ExportRootScope);
		ValueStr += TEXT(",");
		ValueProp->ExportTextItem(
			ValueStr, MapHelper.GetValuePtr(Index), nullptr, Parent, PortFlags | PPF_Delimited, ExportRootScope);
		ValueStr += TEXT(")");
	}
	ValueStr += TEXT(")");
}

const TCHAR* FMapProperty::ImportText_Internal(
	const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const
{
	// ((Key,Value),(Key,Value),...)
	Buffer = OpenParenthesis(this, Buffer, ErrorText);
	if (!Buffer)
	{
		return nullptr;
	}
	FScriptMapHelper MapHelper(this, Data);
	MapHelper.EmptyValues();
	FScratchValue Key(KeyProp);
	FScratchValue Value(ValueProp);
	while (*Buffer != ')')
	{
		Buffer = OpenParenthesis(this, Buffer, ErrorText);
		Buffer = Buffer ? KeyProp->ImportText(Buffer, Key.Get(), PortFlags | PPF_Delimited, OwnerObject, ErrorText)
						: nullptr;
		Buffer = Buffer ? SkipWhitespace(Buffer) : nullptr;
		if (!Buffer || *Buffer != ',')
		{
			ReportImportError(ErrorText, FString::Printf(TEXT("%s: expected (Key, Value)"), *GetName()));
			return nullptr;
		}
		Buffer = ValueProp->ImportText(Buffer + 1, Value.Get(), PortFlags | PPF_Delimited, OwnerObject, ErrorText);
		Buffer = Buffer ? SkipWhitespace(Buffer) : nullptr;
		if (!Buffer || *Buffer != ')')
		{
			ReportImportError(ErrorText, FString::Printf(TEXT("%s: expected ')' after a map value"), *GetName()));
			return nullptr;
		}
		MapHelper.AddPair(Key.Get(), Value.Get());
		Buffer = NextElement(this, Buffer + 1, ErrorText);
		if (!Buffer)
		{
			return nullptr;
		}
	}
	return Buffer + 1;
}

void FMapProperty::CopyValuesInternal(void* Dest, void const* Src, int32 Count) const
{
	for (int32 ValueIndex = 0; ValueIndex < Count; ++ValueIndex)
	{
		void* DestMap = (uint8*)Dest + ValueIndex * ElementSize;
		const void* SrcMap = (const uint8*)Src + ValueIndex * ElementSize;
		if (DestMap == SrcMap)
		{
			continue;
		}
		FScriptMapHelper SrcMapHelper(this, SrcMap);
		FScriptMapHelper DestMapHelper(this, DestMap);
		DestMapHelper.EmptyValues(SrcMapHelper.Num());
		for (int32 SrcIndex = 0; SrcIndex < SrcMapHelper.GetMaxIndex(); ++SrcIndex)
		{
			if (SrcMapHelper.IsValidIndex(SrcIndex))
			{
				const int32 DestIndex = DestMapHelper.AddDefaultValue_Invalid_NeedsRehash();
				KeyProp->CopyCompleteValue(DestMapHelper.GetKeyPtr(DestIndex), SrcMapHelper.GetKeyPtr(SrcIndex));
				ValueProp->CopyCompleteValue(DestMapHelper.GetValuePtr(DestIndex), SrcMapHelper.GetValuePtr(SrcIndex));
			}
		}
		DestMapHelper.Rehash();
	}
}

void FMapProperty::ClearValueInternal(void* Data) const
{
	FScriptMapHelper MapHelper(this, Data);
	MapHelper.EmptyValues();
}

void FMapProperty::DestroyValueInternal(void* Dest) const
{
	for (int32 Index = 0; Index < ArrayDim; ++Index)
	{
		void* Map = (uint8*)Dest + Index * ElementSize;
		FScriptMapHelper MapHelper(this, Map);
		MapHelper.EmptyValues();
		((FScriptMap*)Map)->~FScriptMap();
	}
}

void FMapProperty::InitializeValueInternal(void* Dest) const
{
	for (int32 Index = 0; Index < ArrayDim; ++Index)
	{
		::new ((uint8*)Dest + Index * ElementSize) FScriptMap();
	}
}

// FScriptMapHelper

FScriptMapHelper::FScriptMapHelper(const FMapProperty* InProperty, const void* InMap)
	: KeyProp(InProperty->KeyProp)
	, ValueProp(InProperty->ValueProp)
	, Map((FScriptMap*)InMap)
	, MapLayout(InProperty->MapLayout)
{
	checkf(MapLayout.SetLayout.Size > 0, "Map property %s is not linked", *InProperty->GetName());
}

void FScriptMapHelper::EmptyValues(int32 Slack)
{
	check(Slack >= 0);
	const int32 OldNum = Num();
	if (OldNum)
	{
		for (int32 Index = 0; Index < GetMaxIndex(); ++Index)
		{
			if (IsValidIndex(Index))
			{
				KeyProp->DestroyValue(GetKeyPtr(Index));
				ValueProp->DestroyValue(GetValuePtr(Index));
			}
		}
	}
	if (OldNum || Slack)
	{
		Map->Empty(Slack, MapLayout);
	}
}

int32 FScriptMapHelper::AddDefaultValue_Invalid_NeedsRehash()
{
	const int32 Result = Map->AddUninitialized(MapLayout);
	KeyProp->InitializeValue(GetKeyPtr(Result));
	ValueProp->InitializeValue(GetValuePtr(Result));
	return Result;
}

void FScriptMapHelper::Rehash()
{
	const FProperty* LocalKeyProp = KeyProp;
	Map->Rehash(MapLayout, [LocalKeyProp](const void* Src) { return LocalKeyProp->GetValueTypeHash(Src); });
}

void FScriptMapHelper::RemoveAt(int32 Index, int32 Count)
{
	check(IsValidIndex(Index));
	for (; Count; ++Index)
	{
		if (IsValidIndex(Index))
		{
			KeyProp->DestroyValue(GetKeyPtr(Index));
			ValueProp->DestroyValue(GetValuePtr(Index));
			Map->RemoveAt(Index, MapLayout);
			--Count;
		}
	}
}

int32 FScriptMapHelper::FindMapIndexWithKey(const void* KeyPtr) const
{
	const FProperty* LocalKeyProp = KeyProp;
	return Map->FindPairIndex(
		KeyPtr, MapLayout, [LocalKeyProp](const void* Key) { return LocalKeyProp->GetValueTypeHash(Key); },
		[LocalKeyProp](const void* A, const void* B) { return LocalKeyProp->Identical(A, B); });
}

uint8* FScriptMapHelper::FindValueFromHash(const void* KeyPtr)
{
	const int32 Index = FindMapIndexWithKey(KeyPtr);
	return Index != INDEX_NONE ? GetValuePtr(Index) : nullptr;
}

void* FScriptMapHelper::FindOrAdd(const void* KeyPtr)
{
	const FProperty* LocalKeyProp = KeyProp;
	const FProperty* LocalValueProp = ValueProp;
	const int32 LocalValueOffset = MapLayout.ValueOffset;
	const int32 Index = Map->FindOrAdd(
		KeyPtr, MapLayout, [LocalKeyProp](const void* Key) { return LocalKeyProp->GetValueTypeHash(Key); },
		[LocalKeyProp](const void* A, const void* B) { return LocalKeyProp->Identical(A, B); },
		[LocalKeyProp, LocalValueProp, LocalValueOffset, KeyPtr](void* NewPair)
		{
			LocalKeyProp->InitializeValue(NewPair);
			LocalKeyProp->CopySingleValue(NewPair, KeyPtr);
			LocalValueProp->InitializeValue((uint8*)NewPair + LocalValueOffset);
		});
	return GetValuePtr(Index);
}

void FScriptMapHelper::AddPair(const void* KeyPtr, const void* ValuePtr)
{
	void* Value = FindOrAdd(KeyPtr);
	ValueProp->CopySingleValue(Value, ValuePtr);
}

bool FScriptMapHelper::RemovePair(const void* KeyPtr)
{
	const int32 Index = FindMapIndexWithKey(KeyPtr);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	RemoveAt(Index);
	return true;
}

// Serialization (UE: PropertyArray.cpp, PropertySet.cpp, PropertyMap.cpp)

void FArrayProperty::SerializeItem(FArchive& Ar, void* Value, void const* Defaults) const
{
	(void)Defaults;
	FScriptArrayHelper ArrayHelper(this, Value);
	int32 Num = ArrayHelper.Num();
	Ar << Num;
	if (Ar.IsLoading())
	{
		if (!IsLoadedCountValid(this, Ar, Num))
		{
			ArrayHelper.EmptyValues();
			return;
		}
		ArrayHelper.EmptyAndAddValues(Num);
	}

	// Struct elements are preceded by a tag of the element type, so a load sees when the struct changed (UE 4.27).
	FStructProperty* InnerStruct = CastField<FStructProperty>(Inner);
	FPropertyTag InnerTag;
	int64 ElementsOffset = INDEX_NONE;
	if (InnerStruct)
	{
		if (Ar.IsSaving())
		{
			InnerTag = FPropertyTag(Inner, 0, nullptr);
		}
		Ar << InnerTag;
		ElementsOffset = Ar.Tell();
		if (Ar.IsLoading() &&
			(InnerTag.Type != NAME_StructProperty || InnerTag.StructName != InnerStruct->Struct->GetFName()))
		{
			UE_LOG(LogProperty, Warning, TEXT("%s: %s holds %s elements, it was saved with %s ones; skipped"),
				*Ar.GetArchiveName(), *GetName(), *InnerStruct->Struct->GetName(), *InnerTag.StructName.ToString());
			ArrayHelper.EmptyValues();
			Ar.Seek(ElementsOffset + InnerTag.Size);
			return;
		}
	}
	for (int32 Index = 0; Index < Num && !Ar.IsError(); ++Index)
	{
		Inner->SerializeItem(Ar, ArrayHelper.GetRawPtr(Index), nullptr);
	}
	if (InnerStruct && Ar.IsSaving())
	{
		const int64 ElementsEnd = Ar.Tell();
		InnerTag.Size = int32(ElementsEnd - ElementsOffset);
		Ar.Seek(InnerTag.SizeOffset);
		Ar << InnerTag.Size;
		Ar.Seek(ElementsEnd);
	}
}

EConvertFromTypeResult FArrayProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, void* Value) const
{
	(void)Ar;
	(void)Value;
	return Tag.Type == GetID() && Tag.InnerType == Inner->GetID() ? EConvertFromTypeResult::UseSerializeItem
																  : EConvertFromTypeResult::CannotConvert;
}

void FSetProperty::SerializeItem(FArchive& Ar, void* Value, void const* Defaults) const
{
	(void)Defaults;
	FScriptSetHelper SetHelper(this, Value);
	// UE writes the default elements a set removed; Leon writes the whole set, so there are none.
	int32 NumElementsToRemove = 0;
	Ar << NumElementsToRemove;
	if (Ar.IsLoading())
	{
		SetHelper.EmptyElements();
		if (!IsLoadedCountValid(this, Ar, NumElementsToRemove))
		{
			return;
		}
		if (NumElementsToRemove > 0)
		{
			// A package saved by an engine that writes them: the set is replaced anyway.
			FScratchValue Removed(ElementProp);
			for (int32 Index = 0; Index < NumElementsToRemove && !Ar.IsError(); ++Index)
			{
				ElementProp->SerializeItem(Ar, Removed.Get(), nullptr);
			}
		}
		int32 Num = 0;
		Ar << Num;
		if (!IsLoadedCountValid(this, Ar, Num))
		{
			return;
		}
		for (int32 Index = 0; Index < Num && !Ar.IsError(); ++Index)
		{
			const int32 ElementIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
			ElementProp->SerializeItem(Ar, SetHelper.GetElementPtr(ElementIndex), nullptr);
		}
		SetHelper.Rehash();
		return;
	}
	int32 Num = SetHelper.Num();
	Ar << Num;
	for (int32 Index = 0; Index < SetHelper.GetMaxIndex(); ++Index)
	{
		if (SetHelper.IsValidIndex(Index))
		{
			ElementProp->SerializeItem(Ar, SetHelper.GetElementPtr(Index), nullptr);
		}
	}
}

EConvertFromTypeResult FSetProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, void* Value) const
{
	(void)Ar;
	(void)Value;
	return Tag.Type == GetID() && Tag.InnerType == ElementProp->GetID() ? EConvertFromTypeResult::UseSerializeItem
																		: EConvertFromTypeResult::CannotConvert;
}

void FMapProperty::SerializeItem(FArchive& Ar, void* Value, void const* Defaults) const
{
	(void)Defaults;
	FScriptMapHelper MapHelper(this, Value);
	// UE writes the default keys a map removed; Leon writes the whole map, so there are none.
	int32 NumKeysToRemove = 0;
	Ar << NumKeysToRemove;
	if (Ar.IsLoading())
	{
		MapHelper.EmptyValues();
		if (!IsLoadedCountValid(this, Ar, NumKeysToRemove))
		{
			return;
		}
		if (NumKeysToRemove > 0)
		{
			FScratchValue Removed(KeyProp);
			for (int32 Index = 0; Index < NumKeysToRemove && !Ar.IsError(); ++Index)
			{
				KeyProp->SerializeItem(Ar, Removed.Get(), nullptr);
			}
		}
		int32 Num = 0;
		Ar << Num;
		if (!IsLoadedCountValid(this, Ar, Num))
		{
			return;
		}
		for (int32 Index = 0; Index < Num && !Ar.IsError(); ++Index)
		{
			const int32 PairIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			KeyProp->SerializeItem(Ar, MapHelper.GetKeyPtr(PairIndex), nullptr);
			ValueProp->SerializeItem(Ar, MapHelper.GetValuePtr(PairIndex), nullptr);
		}
		MapHelper.Rehash();
		return;
	}
	int32 Num = MapHelper.Num();
	Ar << Num;
	for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); ++Index)
	{
		if (MapHelper.IsValidIndex(Index))
		{
			KeyProp->SerializeItem(Ar, MapHelper.GetKeyPtr(Index), nullptr);
			ValueProp->SerializeItem(Ar, MapHelper.GetValuePtr(Index), nullptr);
		}
	}
}

EConvertFromTypeResult FMapProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, void* Value) const
{
	(void)Ar;
	(void)Value;
	return Tag.Type == GetID() && Tag.InnerType == KeyProp->GetID() && Tag.ValueType == ValueProp->GetID()
		? EConvertFromTypeResult::UseSerializeItem
		: EConvertFromTypeResult::CannotConvert;
}
