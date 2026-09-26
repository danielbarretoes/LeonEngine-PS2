#include "UObject/Class.h"

#include "Containers/StringConv.h"
#include "Serialization/Archive.h"
#include "Templates/Casts.h"
#include "UObject/Package.h"
#include "UObject/PropertyTag.h"
#include "UObject/Stack.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogClass, Log, All);

COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
COREUOBJECT_API UClass* Z_Construct_UClass_UField();
COREUOBJECT_API UClass* Z_Construct_UClass_UStruct();
COREUOBJECT_API UClass* Z_Construct_UClass_UScriptStruct();
COREUOBJECT_API UClass* Z_Construct_UClass_UClass();
COREUOBJECT_API UClass* Z_Construct_UClass_UEnum();
COREUOBJECT_API UClass* Z_Construct_UClass_UFunction();
COREUOBJECT_API UClass* Z_Construct_UClass_UPackage();

// UField

UField::UField(EStaticConstructor, EObjectFlags InFlags)
	: UObject(EC_StaticConstructor, InFlags)
	, Next(nullptr)
{
}

UField::UField(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer)
	, Next(nullptr)
{
}

void UField::AddCppProperty(FProperty* Property)
{
	UE_LOG(LogClass, Fatal, TEXT("%s cannot own the property %s"), *GetFullName(), *Property->GetName());
}

void UField::Bind()
{
}

UClass* UField::GetOwnerClass() const
{
	for (const UObject* Obj = this; Obj; Obj = Obj->GetOuter())
	{
		if (UClass* Class = const_cast<UClass*>(Cast<UClass>(Obj)))
		{
			return Class;
		}
	}
	return nullptr;
}

UStruct* UField::GetOwnerStruct() const
{
	for (const UObject* Obj = this; Obj; Obj = Obj->GetOuter())
	{
		if (UStruct* Struct = const_cast<UStruct*>(Cast<UStruct>(Obj)))
		{
			return Struct;
		}
	}
	return nullptr;
}

// UStruct

UStruct::UStruct(EStaticConstructor, int32 InSize, int32 InAlignment, EObjectFlags InFlags)
	: UField(EC_StaticConstructor, InFlags)
	, Children(nullptr)
	, ChildProperties(nullptr)
	, PropertiesSize(InSize)
	, MinAlignment(InAlignment)
	, PropertyLink(nullptr)
	, RefLink(nullptr)
	, DestructorLink(nullptr)
	, PostConstructLink(nullptr)
	, SuperStruct(nullptr)
{
}

UStruct::UStruct(
	const FObjectInitializer& ObjectInitializer, UStruct* InSuperStruct, SIZE_T ParamsSize, SIZE_T Alignment)
	: UField(ObjectInitializer)
	, Children(nullptr)
	, ChildProperties(nullptr)
	, PropertiesSize(int32(ParamsSize))
	, MinAlignment(Alignment ? int32(Alignment) : 1)
	, PropertyLink(nullptr)
	, RefLink(nullptr)
	, DestructorLink(nullptr)
	, PostConstructLink(nullptr)
	, SuperStruct(InSuperStruct)
{
}

UStruct::~UStruct()
{
	FField* Field = ChildProperties;
	while (Field)
	{
		FField* NextField = Field->Next;
		delete Field;
		Field = NextField;
	}
	ChildProperties = nullptr;
}

void UStruct::SetSuperStruct(UStruct* NewSuperStruct)
{
	SuperStruct = NewSuperStruct;
}

void UStruct::AddCppProperty(FProperty* Property)
{
	// The generated tables are built from their end, so prepending keeps the declaration order (UE).
	Property->Next = ChildProperties;
	ChildProperties = Property;
}

bool UStruct::IsChildOf(const UStruct* SomeBase) const
{
	if (!SomeBase)
	{
		return false;
	}
	for (const UStruct* Struct = this; Struct; Struct = Struct->GetSuperStruct())
	{
		if (Struct == SomeBase)
		{
			return true;
		}
	}
	return false;
}

FProperty* UStruct::FindPropertyByName(FName InName) const
{
	for (FProperty* Property = PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (Property->GetFName() == InName)
		{
			return Property;
		}
	}
	// Not linked yet: search the declared properties.
	for (const UStruct* Struct = this; Struct; Struct = Struct->GetSuperStruct())
	{
		for (FField* Field = Struct->ChildProperties; Field; Field = Field->Next)
		{
			if (Field->GetFName() == InName)
			{
				return CastField<FProperty>(Field);
			}
		}
	}
	return nullptr;
}

void UStruct::StaticLink(bool bRelinkExistingProperties)
{
	FArchive ArDummy;
	Link(ArDummy, bRelinkExistingProperties);
}

void UStruct::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	checkf(!bRelinkExistingProperties, "%s: Leon only links compiled-in layouts (no relinking)", *GetName());
	UClass* OwnerClass = Cast<UClass>(this);
	const bool bOwnedByNativeClass = OwnerClass && OwnerClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic);

	// Each property sets up its computed flags; offsets and sizes come from the generated code (UE:
	// LinkWithoutChangingOffset).
	FProperty** PropertyLinkPtr = &PropertyLink;
	FProperty** RefLinkPtr = &RefLink;
	FProperty** DestructorLinkPtr = &DestructorLink;
	FProperty** PostConstructLinkPtr = &PostConstructLink;
	for (FField* Field = ChildProperties; Field; Field = Field->Next)
	{
		FProperty* Property = CastField<FProperty>(Field);
		if (!Property)
		{
			continue;
		}
		Property->LinkWithoutChangingOffset(Ar);
		MinAlignment = FMath::Max(MinAlignment, Property->GetMinAlignment());

		*PropertyLinkPtr = Property;
		PropertyLinkPtr = &Property->PropertyLinkNext;
		// Weak and soft references too (UE): RefLink lists every property that names an object.
		if (Property->ContainsObjectReference(EPropertyObjectReferenceType::Any))
		{
			*RefLinkPtr = Property;
			RefLinkPtr = &Property->NextRef;
		}
		if (!Property->HasAnyPropertyFlags(CPF_IsPlainOldData | CPF_NoDestructor))
		{
			*DestructorLinkPtr = Property;
			DestructorLinkPtr = &Property->DestructorLinkNext;
		}
		// Native class properties are set by the constructor; only config properties are copied from the class
		// defaults afterwards (UE).
		if (OwnerClass && (!bOwnedByNativeClass || Property->HasAnyPropertyFlags(CPF_Config)))
		{
			*PostConstructLinkPtr = Property;
			PostConstructLinkPtr = &Property->PostConstructLinkNext;
		}
	}

	// The supers' chains follow this struct's own properties (UE).
	UStruct* InheritanceSuper = GetInheritanceSuper();
	*PropertyLinkPtr = InheritanceSuper ? InheritanceSuper->PropertyLink : nullptr;
	*RefLinkPtr = InheritanceSuper ? InheritanceSuper->RefLink : nullptr;
	*DestructorLinkPtr = InheritanceSuper ? InheritanceSuper->DestructorLink : nullptr;
	*PostConstructLinkPtr = InheritanceSuper ? InheritanceSuper->PostConstructLink : nullptr;
}

void UStruct::InitializeStruct(void* InDest, int32 ArrayDim) const
{
	uint8* Dest = (uint8*)InDest;
	const int32 Stride = GetStructureSize();
	FMemory::Memzero(Dest, SIZE_T(ArrayDim) * SIZE_T(Stride));
	for (int32 ArrayIndex = 0; ArrayIndex < ArrayDim; ++ArrayIndex)
	{
		for (FProperty* Property = PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			if (!Property->HasAnyPropertyFlags(CPF_ZeroConstructor))
			{
				Property->InitializeValue_InContainer(Dest + ArrayIndex * Stride);
			}
		}
	}
}

void UStruct::DestroyStruct(void* InDest, int32 ArrayDim) const
{
	uint8* Dest = (uint8*)InDest;
	const int32 Stride = GetStructureSize();
	for (int32 ArrayIndex = 0; ArrayIndex < ArrayDim; ++ArrayIndex)
	{
		for (FProperty* Property = DestructorLink; Property; Property = Property->DestructorLinkNext)
		{
			Property->DestroyValue_InContainer(Dest + ArrayIndex * Stride);
		}
	}
}

void UStruct::SerializeTaggedProperties(
	FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults, const UObject* BreakRecursionIfFullyLoad) const
{
	(void)BreakRecursionIfFullyLoad;
	if (Ar.IsLoading())
	{
		// Tags until NAME_None; each says how many bytes its value takes, so what cannot be loaded is skipped.
		while (true)
		{
			FPropertyTag Tag;
			Ar << Tag;
			if (Ar.IsError() || Tag.Name.IsNone())
			{
				break;
			}
			const int64 ValueEnd = Ar.Tell() + Tag.Size;
			bool bLoaded = false;
			FProperty* Property = FindPropertyByName(Tag.Name);
			if (!Property)
			{
				// Renamed or removed, or editor-only data on a build without it: schema evolution.
				UE_LOG(LogClass, Verbose, TEXT("%s: %s.%s no longer exists; its saved value is skipped"),
					*Ar.GetArchiveName(), *GetName(), *Tag.Name.ToString());
			}
			else if (Tag.ArrayIndex >= Property->ArrayDim)
			{
				UE_LOG(LogClass, Warning, TEXT("%s: %s.%s[%d] is past the property's %d elements; skipped"),
					*Ar.GetArchiveName(), *GetName(), *Tag.Name.ToString(), Tag.ArrayIndex, Property->ArrayDim);
			}
			else if (!Property->ShouldSerializeValue(Ar))
			{
				UE_LOG(LogClass, Verbose,
					TEXT("%s: %s.%s is not loaded (transient, deprecated or editor-only); skipped"),
					*Ar.GetArchiveName(), *GetName(), *Tag.Name.ToString());
			}
			else
			{
				uint8* Value = Property->ContainerPtrToValuePtr<uint8>(Data, Tag.ArrayIndex);
				const uint8* DefaultValue =
					Property->ContainerPtrToValuePtrForDefaults<uint8>(DefaultsStruct, Defaults, Tag.ArrayIndex);
				switch (Property->ConvertFromType(Tag, Ar, Value))
				{
					case EConvertFromTypeResult::Serialized:
					case EConvertFromTypeResult::Converted:
						bLoaded = true;
						break;
					case EConvertFromTypeResult::UseSerializeItem:
						if (Tag.Type == Property->GetID())
						{
							Tag.SerializeTaggedProperty(Ar, Property, Value, DefaultValue);
							bLoaded = true;
						}
						break;
					case EConvertFromTypeResult::CannotConvert:
						break;
				}
				if (!bLoaded)
				{
					UE_LOG(LogClass, Warning,
						TEXT("%s: %s.%s was saved as a %s and cannot be loaded into a %s; skipped"),
						*Ar.GetArchiveName(), *GetName(), *Tag.Name.ToString(), *Tag.Type.ToString(),
						*Property->GetID().ToString());
				}
			}
			if (Ar.IsError())
			{
				break;
			}
			if (bLoaded && Ar.Tell() != ValueEnd)
			{
				UE_LOG(LogClass, Warning, TEXT("%s: %s.%s read %lld bytes, its tag says %d"), *Ar.GetArchiveName(),
					*GetName(), *Tag.Name.ToString(), (long long)(Ar.Tell() - (ValueEnd - Tag.Size)), Tag.Size);
			}
			Ar.Seek(ValueEnd);
		}
		return;
	}

	// Saving: the properties that differ from the defaults (all of them without defaults), then NAME_None.
	for (FProperty* Property = PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->ShouldSerializeValue(Ar))
		{
			continue;
		}
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			uint8* Value = Property->ContainerPtrToValuePtr<uint8>(Data, Index);
			const uint8* DefaultValue =
				Property->ContainerPtrToValuePtrForDefaults<uint8>(DefaultsStruct, Defaults, Index);
			if (DefaultValue && Property->Identical(Value, DefaultValue))
			{
				continue;
			}
			FPropertyTag Tag(Property, Index, Value);
			Ar << Tag;
			const int64 ValueOffset = Ar.Tell();
			Tag.SerializeTaggedProperty(Ar, Property, Value, DefaultValue);
			const int64 ValueEnd = Ar.Tell();
			Tag.Size = int32(ValueEnd - ValueOffset);
			if (Tag.Size != 0)
			{
				// The size is only known now: patch it into the tag.
				Ar.Seek(Tag.SizeOffset);
				Ar << Tag.Size;
				Ar.Seek(ValueEnd);
			}
		}
	}
	FName Terminator(NAME_None);
	Ar << Terminator;
}

void UStruct::SerializeBin(FArchive& Ar, void* Data) const
{
	for (FProperty* Property = PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (Property->ShouldSerializeValue(Ar))
		{
			for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
			{
				Property->SerializeItem(Ar, Property->ContainerPtrToValuePtr<void>(Data, Index), nullptr);
			}
		}
	}
}

// UScriptStruct

UScriptStruct::UScriptStruct(EStaticConstructor, int32 InSize, int32 InAlignment, EObjectFlags InFlags)
	: UStruct(EC_StaticConstructor, InSize, InAlignment, InFlags)
	, StructFlags(STRUCT_NoFlags)
	, CppStructOps(nullptr)
{
}

UScriptStruct::UScriptStruct(const FObjectInitializer& ObjectInitializer, UScriptStruct* InSuperStruct,
	ICppStructOps* InCppStructOps, EStructFlags InStructFlags, SIZE_T ExplicitSize, SIZE_T ExplicitAlignment)
	: UStruct(ObjectInitializer, InSuperStruct, ExplicitSize, ExplicitAlignment)
	, StructFlags(InStructFlags)
	, CppStructOps(nullptr)
{
	if (InCppStructOps)
	{
		SetCppStructOps(InCppStructOps);
	}
}

UScriptStruct::~UScriptStruct()
{
	delete CppStructOps;
}

void UScriptStruct::SetCppStructOps(ICppStructOps* InCppStructOps)
{
	delete CppStructOps;
	CppStructOps = InCppStructOps;
	StructFlags &= ~STRUCT_ComputedFlags;
	if (!CppStructOps)
	{
		return;
	}
	checkf(CppStructOps->GetSize() == PropertiesSize, "Struct %s: C++ size %d, reflected size %d", *GetName(),
		CppStructOps->GetSize(), PropertiesSize);
	if (CppStructOps->HasZeroConstructor())
	{
		StructFlags |= STRUCT_ZeroConstructor;
	}
	if (CppStructOps->IsPlainOldData())
	{
		StructFlags |= STRUCT_IsPlainOldData;
	}
	if (!CppStructOps->HasDestructor())
	{
		StructFlags |= STRUCT_NoDestructor;
	}
	if (CppStructOps->HasCopy())
	{
		StructFlags |= STRUCT_CopyNative;
	}
	if (CppStructOps->HasIdentical())
	{
		StructFlags |= STRUCT_IdenticalNative;
	}
	if (CppStructOps->HasExportTextItem())
	{
		StructFlags |= STRUCT_ExportTextItemNative;
	}
	if (CppStructOps->HasImportTextItem())
	{
		StructFlags |= STRUCT_ImportTextItemNative;
	}
	if (CppStructOps->HasSerializer())
	{
		StructFlags |= STRUCT_SerializeNative;
	}
}

void UScriptStruct::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	Super::Link(Ar, bRelinkExistingProperties);
}

void UScriptStruct::InitializeStruct(void* InDest, int32 ArrayDim) const
{
	uint8* Dest = (uint8*)InDest;
	const int32 Stride = GetStructureSize();
	if (!CppStructOps)
	{
		Super::InitializeStruct(InDest, ArrayDim);
		return;
	}
	// Zeroed first, then value-initialized by the C++ type (UE).
	FMemory::Memzero(Dest, SIZE_T(ArrayDim) * SIZE_T(Stride));
	if (!CppStructOps->HasZeroConstructor())
	{
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayDim; ++ArrayIndex)
		{
			CppStructOps->Construct(Dest + ArrayIndex * Stride);
		}
	}
}

void UScriptStruct::DestroyStruct(void* InDest, int32 ArrayDim) const
{
	uint8* Dest = (uint8*)InDest;
	const int32 Stride = GetStructureSize();
	if (!CppStructOps)
	{
		Super::DestroyStruct(InDest, ArrayDim);
		return;
	}
	if (CppStructOps->HasDestructor())
	{
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayDim; ++ArrayIndex)
		{
			CppStructOps->Destruct(Dest + ArrayIndex * Stride);
		}
	}
}

void UScriptStruct::CopyScriptStruct(void* InDest, void const* InSrc, int32 ArrayDim) const
{
	uint8* Dest = (uint8*)InDest;
	const uint8* Src = (const uint8*)InSrc;
	const int32 Stride = GetStructureSize();
	if (StructFlags & STRUCT_IsPlainOldData)
	{
		FMemory::Memcpy(Dest, Src, SIZE_T(ArrayDim) * SIZE_T(Stride));
		return;
	}
	if (CppStructOps && CppStructOps->HasCopy() && CppStructOps->Copy(Dest, Src, ArrayDim))
	{
		return;
	}
	for (int32 ArrayIndex = 0; ArrayIndex < ArrayDim; ++ArrayIndex)
	{
		for (FProperty* Property = PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			Property->CopyCompleteValue_InContainer(Dest + ArrayIndex * Stride, Src + ArrayIndex * Stride);
		}
	}
}

void UScriptStruct::ClearScriptStruct(void* Dest, int32 ArrayDim) const
{
	DestroyStruct(Dest, ArrayDim);
	InitializeStruct(Dest, ArrayDim);
}

bool UScriptStruct::CompareScriptStruct(const void* A, const void* B, uint32 PortFlags) const
{
	check(A);
	if (!B)
	{
		// Against the default value (UE): build a default instance to compare with.
		uint8* Default = (uint8*)FMemory::Malloc(SIZE_T(GetStructureSize()), uint32(GetMinAlignment()));
		InitializeStruct(Default);
		const bool bResult = CompareScriptStruct(A, Default, PortFlags);
		DestroyStruct(Default);
		FMemory::Free(Default);
		return bResult;
	}
	if (CppStructOps && CppStructOps->HasIdentical())
	{
		bool bResult = false;
		if (CppStructOps->Identical(A, B, PortFlags, bResult))
		{
			return bResult;
		}
	}
	for (FProperty* Property = PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
		{
			if (!Property->Identical_InContainer(A, B, ArrayIndex, PortFlags))
			{
				return false;
			}
		}
	}
	return true;
}

uint32 UScriptStruct::GetStructTypeHash(const void* Src) const
{
	checkf(CppStructOps && CppStructOps->HasGetTypeHash(), "Struct %s has no GetTypeHash", *GetName());
	return CppStructOps->GetStructTypeHash(Src);
}

FString UScriptStruct::GetStructCPPName() const
{
	return FString(TEXT("F")) + GetName();
}

bool UScriptStruct::UseBinarySerialization(const FArchive& Ar) const
{
	return !(Ar.IsLoading() || Ar.IsSaving()) || (StructFlags & STRUCT_Immutable) != 0;
}

void UScriptStruct::SerializeItem(FArchive& Ar, void* Value, void const* Defaults) const
{
	if ((StructFlags & STRUCT_SerializeNative) && CppStructOps->Serialize(Ar, Value))
	{
		return;
	}
	if (UseBinarySerialization(Ar))
	{
		SerializeBin(Ar, Value);
	}
	else
	{
		SerializeTaggedProperties(Ar, (uint8*)Value, const_cast<UScriptStruct*>(this), (uint8*)Defaults);
	}
}

// UClass

UClass::UClass(EStaticConstructor, FName InName, uint32 InSize, uint32 InAlignment, EClassFlags InClassFlags,
	EClassCastFlags InClassCastFlags, const TCHAR* InClassConfigName, EObjectFlags InFlags,
	ClassConstructorType InClassConstructor, ClassVTableHelperCtorCallerType InClassVTableHelperCtorCaller,
	ClassAddReferencedObjectsType InClassAddReferencedObjects)
	: UStruct(EC_StaticConstructor, int32(InSize), int32(InAlignment), InFlags)
	, ClassConstructor(InClassConstructor)
	, ClassVTableHelperCtorCaller(InClassVTableHelperCtorCaller)
	, ClassAddReferencedObjects(InClassAddReferencedObjects)
	, ClassUnique(0)
	, ClassFlags(InClassFlags | CLASS_Native)
	, ClassCastFlags(InClassCastFlags)
	, ClassConfigName(InClassConfigName)
	, ClassDefaultObject(nullptr)
	, CppTypeInfoStatic(nullptr)
{
	(void)InName;
}

UClass::UClass(const FObjectInitializer& ObjectInitializer)
	: UStruct(ObjectInitializer)
	, ClassConstructor(nullptr)
	, ClassVTableHelperCtorCaller(nullptr)
	, ClassAddReferencedObjects(&UObject::AddReferencedObjects)
	, ClassUnique(0)
	, ClassFlags(CLASS_None)
	, ClassCastFlags(CASTCLASS_None)
	, ClassDefaultObject(nullptr)
	, CppTypeInfoStatic(nullptr)
{
}

FName UClass::GetDefaultObjectName() const
{
	return FName(*(FString(TEXT("Default__")) + GetName()));
}

void UClass::AssembleReferenceTokenStream(bool bForce)
{
	if (HasAnyClassFlags(CLASS_TokenStreamAssembled) && !bForce)
	{
		return;
	}
	// UE compiles a token stream of offsets and opcodes; Leon keeps the properties themselves and walks them. RefLink
	// already skips every property without a reference; weak and soft references do not keep objects alive.
	ReferenceTokenStream.Reset();
	for (FProperty* Property = RefLink; Property; Property = Property->NextRef)
	{
		if (Property->ContainsObjectReference(EPropertyObjectReferenceType::Strong))
		{
			ReferenceTokenStream.Add(Property);
		}
	}
	ReferenceTokenStream.Shrink();
	ClassFlags |= CLASS_TokenStreamAssembled;
}

FString UClass::GetConfigName() const
{
	if (!GConfig)
	{
		return FString();
	}
	const FString ConfigName = ClassConfigName.ToString();
	// The global files, when InitializeConfigSystem loaded them (UE: GEngineIni and friends).
	const auto GlobalFile = [&ConfigName](const TCHAR* BaseName, const FString& Key) -> const FString*
	{ return ConfigName.Equals(BaseName) && !Key.IsEmpty() ? &Key : nullptr; };
	if (const FString* Key = GlobalFile(TEXT("Engine"), GEngineIni))
	{
		return *Key;
	}
	if (const FString* Key = GlobalFile(TEXT("Game"), GGameIni))
	{
		return *Key;
	}
	if (const FString* Key = GlobalFile(TEXT("Input"), GInputIni))
	{
		return *Key;
	}
	if (const FString* Key = GlobalFile(TEXT("Editor"), GEditorIni))
	{
		return *Key;
	}
	if (ClassConfigName.IsNone())
	{
		UE_LOG(LogClass, Fatal, TEXT("UClass::GetConfigName() called on class %s with config name 'None'"), *GetName());
	}
	// Any other name: its own hierarchy (Base<Name>.ini, Default<Name>.ini, ...), loaded on first use (UE).
	FString ConfigGameName;
	FConfigCacheIni::LoadGlobalIniFile(ConfigGameName, *ConfigName);
	return ConfigGameName;
}

UObject* UClass::CreateDefaultObject()
{
	if (ClassDefaultObject)
	{
		return ClassDefaultObject;
	}
	checkf(ClassConstructor, "Class %s has no constructor", *GetName());
	UClass* ParentClass = GetSuperClass();
	UObject* ParentDefaultObject = ParentClass ? ParentClass->GetDefaultObject() : nullptr;
	if (!ClassDefaultObject)
	{
		// Set before the constructor runs, so code in it that asks for the defaults gets this object (UE).
		ClassDefaultObject = StaticAllocateObject(
			this, GetOuter(), GetDefaultObjectName(), RF_Public | RF_ClassDefaultObject | RF_ArchetypeObject);
		// A native class's constructor chain sets every property, its own config defaults included, so its CDO does not
		// start from the parent's (UE: bShouldInitializeProperties is false for CLASS_Native | CLASS_Intrinsic);
		// copying the parent's config members would undo a subclass constructor's values. LoadConfig then reads the
		// parents' sections and the class's own.
		const bool bShouldInitializeProperties = !HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic);
		FObjectInitializer ObjectInitializer(
			ClassDefaultObject, ParentDefaultObject, false, bShouldInitializeProperties);
		(*ClassConstructor)(ObjectInitializer);
	}
	return ClassDefaultObject;
}

UFunction* UClass::FindFunctionByName(FName InName, EIncludeSuperFlag::Type IncludeSuper) const
{
	for (const UClass* Class = this; Class; Class = Class->GetSuperClass())
	{
		if (UFunction* const* Found = Class->FuncMap.Find(InName))
		{
			return *Found;
		}
		if (IncludeSuper == EIncludeSuperFlag::ExcludeSuper)
		{
			break;
		}
	}
	return nullptr;
}

void UClass::AddFunctionToFunctionMap(UFunction* Function, FName FuncName)
{
	FuncMap.Add(FuncName, Function);
}

void UClass::AddNativeFunction(const ANSICHAR* InName, FNativeFuncPtr InPointer)
{
	const FName InFName(UTF8_TO_TCHAR(InName));
	for (FNativeFunctionLookup& Lookup : NativeFunctionLookupTable)
	{
		if (Lookup.Name == InFName)
		{
			Lookup.Pointer = InPointer;
			return;
		}
	}
	NativeFunctionLookupTable.Emplace(InFName, InPointer);
}

void UClass::CreateLinkAndAddChildFunctionsToMap(const FClassFunctionLinkInfo* Functions, uint32 NumFunctions)
{
	// From the last declared to the first, so Children lists them in declaration order (UE lists them reversed).
	for (uint32 Index = NumFunctions; Index > 0; --Index)
	{
		const FClassFunctionLinkInfo& Info = Functions[Index - 1];
		UFunction* Function = Info.CreateFuncPtr();
		Function->Next = Children;
		Children = Function;
		AddFunctionToFunctionMap(Function, FName(UTF8_TO_TCHAR(Info.FuncNameUTF8)));
	}
}

void UClass::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	Super::Link(Ar, bRelinkExistingProperties);
}

void UClass::SetSuperStruct(UStruct* NewSuperStruct)
{
	Super::SetSuperStruct(NewSuperStruct);
}

// UEnum

UEnum::UEnum(const FObjectInitializer& ObjectInitializer)
	: UField(ObjectInitializer)
	, EnumDisplayNameFn(nullptr)
	, CppForm(ECppForm::Regular)
	, EnumFlags(EEnumFlags::None)
{
}

FString UEnum::GenerateFullEnumName(const TCHAR* InEnumName) const
{
	if (CppForm == ECppForm::Regular)
	{
		return InEnumName;
	}
	return GetName() + TEXT("::") + InEnumName;
}

FString UEnum::GenerateEnumPrefix() const
{
	FString Prefix;
	if (Names.Num() > 0)
	{
		Prefix = Names[0].Key.ToString();
		for (int32 NameIndex = 1; NameIndex < Names.Num(); ++NameIndex)
		{
			const FString EnumItemName = Names[NameIndex].Key.ToString();
			int32 PrefixIndex = 0;
			while (PrefixIndex < Prefix.Len() && PrefixIndex < EnumItemName.Len() &&
				Prefix[PrefixIndex] == EnumItemName[PrefixIndex])
			{
				++PrefixIndex;
			}
			Prefix.LeftInline(PrefixIndex, false);
		}
		// Up to the last underscore; without one the names do not share a UE-style prefix.
		int32 UnderscoreIndex = INDEX_NONE;
		if (Prefix.FindLastChar('_', UnderscoreIndex) && UnderscoreIndex > 0)
		{
			Prefix.LeftInline(UnderscoreIndex, false);
		}
		else
		{
			Prefix.Empty();
		}
	}
	if (Prefix.Len() == 0)
	{
		Prefix = GetName();
	}
	return Prefix;
}

bool UEnum::SetEnums(
	TArray<TPair<FName, int64>>& InNames, ECppForm InCppForm, EEnumFlags InFlags, bool bAddMaxKeyIfMissing)
{
	Names = InNames;
	CppForm = InCppForm;
	EnumFlags = InFlags;
	if (bAddMaxKeyIfMissing)
	{
		bool bHasMax = false;
		for (const TPair<FName, int64>& Name : Names)
		{
			const FString NameString = Name.Key.ToString();
			if (NameString.Len() >= 4 && NameString.Right(4) == TEXT("_MAX"))
			{
				bHasMax = true;
				break;
			}
		}
		if (!bHasMax)
		{
			const FName MaxEnumItem(*GenerateFullEnumName(*(GenerateEnumPrefix() + TEXT("_MAX"))));
			Names.Emplace(MaxEnumItem, GetMaxEnumValue() + 1);
		}
	}
	return true;
}

FName UEnum::GetNameByIndex(int32 Index) const
{
	return Names.IsValidIndex(Index) ? Names[Index].Key : FName(NAME_None);
}

int64 UEnum::GetValueByIndex(int32 Index) const
{
	checkf(Names.IsValidIndex(Index), "Enum %s has no index %d", *GetName(), Index);
	return Names[Index].Value;
}

FName UEnum::GetNameByValue(int64 InValue) const
{
	const int32 Index = GetIndexByValue(InValue);
	return Index != INDEX_NONE ? Names[Index].Key : FName(NAME_None);
}

int32 UEnum::GetIndexByName(FName InName, EGetByNameFlags Flags) const
{
	if (InName.IsNone())
	{
		return INDEX_NONE;
	}
	// A short name ("Value") also matches the scoped name ("EMyEnum::Value") of an enum class (UE).
	FName FullName = InName;
	if (CppForm != ECppForm::Regular)
	{
		const FString NameString = InName.ToString();
		if (!NameString.Contains(TEXT("::")))
		{
			FullName = FName(*GenerateFullEnumName(*NameString), FNAME_Find);
		}
	}
	// An FName keeps one casing per name (EGetByNameFlags::CaseSensitive): the names compare as FNames.
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		if (Names[Index].Key == FullName)
		{
			return Index;
		}
	}
	if (EnumHasAnyFlags(Flags, EGetByNameFlags::ErrorIfNotFound))
	{
		UE_LOG(LogClass, Warning, TEXT("Enum %s has no entry %s"), *GetName(), *InName.ToString());
	}
	return INDEX_NONE;
}

int64 UEnum::GetValueByName(FName InName, EGetByNameFlags Flags) const
{
	const int32 Index = GetIndexByName(InName, Flags);
	return Index != INDEX_NONE ? Names[Index].Value : int64(INDEX_NONE);
}

int32 UEnum::GetIndexByValue(int64 InValue) const
{
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		if (Names[Index].Value == InValue)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FString UEnum::GetNameStringByIndex(int32 InIndex) const
{
	if (!Names.IsValidIndex(InIndex))
	{
		return FString();
	}
	const FString EnumName = Names[InIndex].Key.ToString();
	if (CppForm == ECppForm::Regular)
	{
		return EnumName;
	}
	const int32 ScopeIndex = EnumName.Find(TEXT("::"), ESearchCase::CaseSensitive);
	return ScopeIndex != INDEX_NONE ? EnumName.RightChop(ScopeIndex + 2) : EnumName;
}

FString UEnum::GetNameStringByValue(int64 InValue) const
{
	return GetNameStringByIndex(GetIndexByValue(InValue));
}

int64 UEnum::GetValueByNameString(const FString& SearchString, EGetByNameFlags Flags) const
{
	// Compared as strings: the short name ("Value") of an enum class may not exist as an FName.
	const ESearchCase::Type SearchCase =
		EnumHasAnyFlags(Flags, EGetByNameFlags::CaseSensitive) ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		if (Names[Index].Key.ToString().Equals(SearchString, SearchCase) ||
			GetNameStringByIndex(Index).Equals(SearchString, SearchCase))
		{
			return Names[Index].Value;
		}
	}
	if (EnumHasAnyFlags(Flags, EGetByNameFlags::ErrorIfNotFound))
	{
		UE_LOG(LogClass, Warning, TEXT("Enum %s has no entry %s"), *GetName(), *SearchString);
	}
	return INDEX_NONE;
}

FText UEnum::GetDisplayNameTextByIndex(int32 InIndex) const
{
	if (EnumDisplayNameFn)
	{
		return EnumDisplayNameFn(InIndex);
	}
	return FText::FromString(GetNameStringByIndex(InIndex));
}

int64 UEnum::GetMaxEnumValue() const
{
	if (Names.Num() == 0)
	{
		return 0;
	}
	int64 MaxValue = Names[0].Value;
	for (const TPair<FName, int64>& Name : Names)
	{
		MaxValue = FMath::Max(MaxValue, Name.Value);
	}
	return MaxValue;
}

bool UEnum::IsValidEnumValue(int64 InValue) const
{
	return GetIndexByValue(InValue) != INDEX_NONE;
}

bool UEnum::IsValidEnumName(FName InName) const
{
	return GetIndexByName(InName) != INDEX_NONE;
}

// UFunction

UFunction::UFunction(const FObjectInitializer& ObjectInitializer, UFunction* InSuperFunction,
	EFunctionFlags InFunctionFlags, SIZE_T ParamsSize)
	: UStruct(ObjectInitializer, InSuperFunction, ParamsSize)
	, FunctionFlags(InFunctionFlags)
	, NumParms(0)
	, ParmsSize(0)
	, ReturnValueOffset(MAX_uint16)
	, Func(nullptr)
{
}

void UFunction::Invoke(UObject* Obj, FFrame& Stack, RESULT_DECL)
{
	checkf(Func, "Function %s has no native implementation", *GetName());
	(*Func)(Obj, Stack, RESULT_PARAM);
}

FProperty* UFunction::GetReturnProperty() const
{
	for (FField* Field = ChildProperties; Field; Field = Field->Next)
	{
		FProperty* Property = CastField<FProperty>(Field);
		if (Property && Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			return Property;
		}
	}
	return nullptr;
}

void UFunction::Bind()
{
	UClass* OwnerClass = GetOwnerClass();
	Func = nullptr;
	if (!OwnerClass)
	{
		return;
	}
	const FName Name = GetFName();
	for (const FNativeFunctionLookup& Lookup : OwnerClass->NativeFunctionLookupTable)
	{
		if (Lookup.Name == Name)
		{
			Func = Lookup.Pointer;
			return;
		}
	}
	UE_LOG(LogClass, Warning, TEXT("Function %s has no registered exec thunk"), *GetFullName());
}

void UFunction::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	Super::Link(Ar, bRelinkExistingProperties);
	NumParms = 0;
	ParmsSize = 0;
	ReturnValueOffset = MAX_uint16;
	for (FField* Field = ChildProperties; Field; Field = Field->Next)
	{
		FProperty* Property = CastField<FProperty>(Field);
		if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}
		++NumParms;
		ParmsSize = uint16(Property->GetOffset_ForUFunction() + Property->GetSize());
		if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			ReturnValueOffset = uint16(Property->GetOffset_ForUFunction());
		}
	}
	// The parameter block is the generated _Parms struct.
	ParmsSize = uint16(FMath::Max(int32(ParmsSize), GetPropertiesSize()));
}

// Native functions

void FNativeFunctionRegistrar::RegisterFunction(UClass* Class, const ANSICHAR* InName, FNativeFuncPtr InPointer)
{
	Class->AddNativeFunction(InName, InPointer);
}

void FNativeFunctionRegistrar::RegisterFunctions(UClass* Class, const FNameNativePtrPair* InArray, int32 NumFunctions)
{
	for (int32 Index = 0; Index < NumFunctions; ++Index)
	{
		Class->AddNativeFunction(InArray[Index].NameUTF8, InArray[Index].Pointer);
	}
}

UScriptStruct* GetStaticStruct(
	UScriptStruct* (*InRegister)(), UObject* StructOuter, const TCHAR* StructName, SIZE_T Size, uint32 Crc)
{
	(void)StructOuter;
	(void)StructName;
	(void)Size;
	(void)Crc;
	UScriptStruct* Struct = InRegister();
	checkf(Struct->GetPropertiesSize() == int32(Size), "Struct %s: size mismatch", StructName);
	return Struct;
}

UEnum* GetStaticEnum(UEnum* (*InRegister)(), UObject* EnumOuter, const TCHAR* EnumName)
{
	(void)EnumOuter;
	(void)EnumName;
	return InRegister();
}

// The intrinsic classes: their UClass and (empty) reflection data are written by hand, as UE does
// (IMPLEMENT_CORE_INTRINSIC_CLASS). UObject's are in Obj.cpp, UPackage's in Package.cpp.

/** Defines TClass::StaticClass() and Z_Construct_UClass_<TClass>[_NoRegister] of a CoreUObject intrinsic class. */
#define IMPLEMENT_CORE_INTRINSIC_CLASS(TClass, TSuperClass)                                                            \
	IMPLEMENT_CLASS(TClass, 0)                                                                                         \
	COREUOBJECT_API UClass* Z_Construct_UClass_##TClass##_NoRegister()                                                 \
	{                                                                                                                  \
		return TClass::StaticClass();                                                                                  \
	}                                                                                                                  \
	COREUOBJECT_API UClass* Z_Construct_UClass_##TClass()                                                              \
	{                                                                                                                  \
		static UClass* Class = nullptr;                                                                                \
		if (!Class)                                                                                                    \
		{                                                                                                              \
			UClass* SuperClass = Z_Construct_UClass_##TSuperClass();                                                   \
			Class = TClass::StaticClass();                                                                             \
			UObjectForceRegistration(Class);                                                                           \
			if (Class->GetSuperClass() != SuperClass)                                                                  \
			{                                                                                                          \
				UE_LOG(LogClass, Fatal, TEXT("Intrinsic class %s has the wrong super class"), *Class->GetName());      \
			}                                                                                                          \
			Class->ClassFlags |= CLASS_Constructed;                                                                    \
			Class->StaticLink();                                                                                       \
		}                                                                                                              \
		return Class;                                                                                                  \
	}

COREUOBJECT_API UClass* Z_Construct_UClass_UField_NoRegister();
COREUOBJECT_API UClass* Z_Construct_UClass_UStruct_NoRegister();
COREUOBJECT_API UClass* Z_Construct_UClass_UScriptStruct_NoRegister();
COREUOBJECT_API UClass* Z_Construct_UClass_UClass_NoRegister();
COREUOBJECT_API UClass* Z_Construct_UClass_UEnum_NoRegister();
COREUOBJECT_API UClass* Z_Construct_UClass_UFunction_NoRegister();

IMPLEMENT_CORE_INTRINSIC_CLASS(UField, UObject)
IMPLEMENT_CORE_INTRINSIC_CLASS(UStruct, UField)
IMPLEMENT_CORE_INTRINSIC_CLASS(UScriptStruct, UStruct)
IMPLEMENT_CORE_INTRINSIC_CLASS(UClass, UStruct)
IMPLEMENT_CORE_INTRINSIC_CLASS(UEnum, UField)
IMPLEMENT_CORE_INTRINSIC_CLASS(UFunction, UStruct)

void UObjectRegisterIntrinsicClasses()
{
	static const FClassRegisterCompiledInInfo IntrinsicClasses[] = {
		{Z_Construct_UClass_UObject, UObject::StaticClass, TEXT("UObject"), sizeof(UObject)},
		{Z_Construct_UClass_UField, UField::StaticClass, TEXT("UField"), sizeof(UField)},
		{Z_Construct_UClass_UStruct, UStruct::StaticClass, TEXT("UStruct"), sizeof(UStruct)},
		{Z_Construct_UClass_UScriptStruct, UScriptStruct::StaticClass, TEXT("UScriptStruct"), sizeof(UScriptStruct)},
		{Z_Construct_UClass_UClass, UClass::StaticClass, TEXT("UClass"), sizeof(UClass)},
		{Z_Construct_UClass_UEnum, UEnum::StaticClass, TEXT("UEnum"), sizeof(UEnum)},
		{Z_Construct_UClass_UFunction, UFunction::StaticClass, TEXT("UFunction"), sizeof(UFunction)},
		{Z_Construct_UClass_UPackage, UPackage::StaticClass, TEXT("UPackage"), sizeof(UPackage)},
	};
	RegisterCompiledInInfo(
		TEXT("/Script/CoreUObject"), IntrinsicClasses, UE_ARRAY_COUNT(IntrinsicClasses), nullptr, 0, nullptr, 0);
}
