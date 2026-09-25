#include "UObject/UObjectGlobals.h"

#include "Containers/StringConv.h"
#include "Misc/CString.h"
#include "Misc/PackageName.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectGlobals, Log, All);

FUObjectThreadContext& FUObjectThreadContext::Get()
{
	static FUObjectThreadContext Context;
	return Context;
}

FVTableHelper::FVTableHelper()
{
}

// FObjectInitializer

FObjectInitializer::FObjectInitializer()
{
}

FObjectInitializer::FObjectInitializer(
	UObject* InObj, UObject* InObjectArchetype, bool bInCopyTransientsFromClassDefaults, bool bInShouldInitializeProps)
	: Obj(InObj)
	, ObjectArchetype(InObjectArchetype)
	, bCopyTransientsFromClassDefaults(bInCopyTransientsFromClassDefaults)
	, bShouldInitializePropsFromArchetype(bInShouldInitializeProps)
	, bIsConstructing(true)
{
	FUObjectThreadContext::Get().InitializerStack.Add(this);
}

FObjectInitializer::~FObjectInitializer()
{
	if (!bIsConstructing)
	{
		return;
	}
	FUObjectThreadContext& Context = FUObjectThreadContext::Get();
	check(Context.TopInitializer() == this);
	Context.InitializerStack.Pop(false);
	PostConstructInit();
}

UClass* FObjectInitializer::GetClass() const
{
	return Obj ? Obj->GetClass() : nullptr;
}

FObjectInitializer& FObjectInitializer::Get()
{
	FObjectInitializer* Top = FUObjectThreadContext::Get().TopInitializer();
	if (!Top)
	{
		UE_LOG(LogUObjectGlobals, Fatal,
			TEXT("FObjectInitializer::Get() called outside of a UObject constructor: create objects with NewObject"));
	}
	return *Top;
}

bool FObjectInitializer::IsInConstructor(const UObject* Outer)
{
	const FObjectInitializer* Top = FUObjectThreadContext::Get().TopInitializer();
	return Top && Outer && Top->Obj == Outer;
}

const FObjectInitializer& FObjectInitializer::SetDefaultSubobjectClass(FName SubobjectName, UClass* Class) const
{
	for (FOverride& Override : Overrides)
	{
		if (Override.Name == SubobjectName)
		{
			Override.Class = Class;
			Override.bDoNotCreate = false;
			return *this;
		}
	}
	FOverride Override;
	Override.Name = SubobjectName;
	Override.Class = Class;
	Overrides.Add(Override);
	return *this;
}

const FObjectInitializer& FObjectInitializer::DoNotCreateDefaultSubobject(FName SubobjectName) const
{
	for (FOverride& Override : Overrides)
	{
		if (Override.Name == SubobjectName)
		{
			Override.Class = nullptr;
			Override.bDoNotCreate = true;
			return *this;
		}
	}
	FOverride Override;
	Override.Name = SubobjectName;
	Override.bDoNotCreate = true;
	Overrides.Add(Override);
	return *this;
}

UObject* FObjectInitializer::CreateDefaultSubobject(UObject* Outer, FName SubobjectFName, UClass* ReturnType,
	UClass* ClassToCreateByDefault, bool bIsRequired, bool bIsTransient) const
{
	if (!Outer || !Outer->HasAnyFlags(RF_NeedInitialization))
	{
		UE_LOG(LogUObjectGlobals, Fatal,
			TEXT("CreateDefaultSubobject(%s) can only be used inside the constructor of the subobject's outer"),
			*SubobjectFName.ToString());
	}
	checkf(!SubobjectFName.IsNone(), "Default subobjects need a name");

	UClass* ClassToCreate = ClassToCreateByDefault;
	for (const FOverride& Override : Overrides)
	{
		if (Override.Name == SubobjectFName)
		{
			ClassToCreate = Override.bDoNotCreate ? nullptr : Override.Class;
			break;
		}
	}
	if (!ClassToCreate)
	{
		if (bIsRequired)
		{
			UE_LOG(LogUObjectGlobals, Fatal, TEXT("Default subobject %s of %s is required and cannot be skipped"),
				*SubobjectFName.ToString(), *Outer->GetName());
		}
		return nullptr;
	}
	if (!ClassToCreate->IsChildOf(ReturnType))
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Default subobject %s of %s: class %s is not a %s"),
			*SubobjectFName.ToString(), *Outer->GetName(), *ClassToCreate->GetName(), *ReturnType->GetName());
	}
	if (ClassToCreate->HasAnyClassFlags(CLASS_Abstract))
	{
		// UE skips abstract classes instead of failing: a derived class sets the concrete one.
		return nullptr;
	}

	// Each instance builds its own subobjects; they start from their class defaults, not from the owner archetype's
	// subobject (no archetype instancing; plan decision D12).
	EObjectFlags SubobjectFlags = Outer->GetMaskedFlags(RF_PropagateToSubObjects) | RF_DefaultSubObject;
	if (bIsTransient)
	{
		SubobjectFlags |= RF_Transient;
	}
	FStaticConstructObjectParameters Params(ClassToCreate);
	Params.Outer = Outer;
	Params.Name = SubobjectFName;
	Params.SetFlags = SubobjectFlags;
	return StaticConstructObject_Internal(Params);
}

void FObjectInitializer::PostConstructInit()
{
	UClass* Class = Obj->GetClass();
	const bool bIsCDO = Obj->HasAnyFlags(RF_ClassDefaultObject);
	if (bShouldInitializePropsFromArchetype)
	{
		// A class default object starts from its super class's defaults, an instance from its archetype.
		UClass* BaseClass = bIsCDO ? Class->GetSuperClass() : Class;
		if (!BaseClass)
		{
			BaseClass = Class;
		}
		UObject* Defaults = ObjectArchetype ? ObjectArchetype : BaseClass->GetDefaultObject(false);
		if (Defaults && Defaults != Obj)
		{
			InitProperties(Obj, BaseClass, Defaults, bCopyTransientsFromClassDefaults);
		}
	}
	// A class default object reads its config (its parents' sections first); instances copied the config members
	// from it above. A PerObjectConfig object reads its own section (UE).
	if (bIsCDO || Class->HasAnyClassFlags(CLASS_PerObjectConfig))
	{
		Obj->LoadConfig(nullptr, nullptr, bIsCDO ? UE4::LCPF_ReadParentSections : UE4::LCPF_None);
	}
	Obj->PostInitProperties();
	Obj->ClearFlags(RF_NeedInitialization);
}

void FObjectInitializer::InitProperties(
	UObject* Obj, UClass* DefaultsClass, UObject* DefaultData, bool bCopyTransientsFromClassDefaults)
{
	check(DefaultsClass && DefaultData);
	if (DefaultData == DefaultsClass->GetDefaultObject(false) && !bCopyTransientsFromClassDefaults)
	{
		// From the class defaults: the constructor already set every native property; copy the ones the defaults
		// may hold differently (the config properties) (UE: PostConstructLink).
		for (FProperty* Property = DefaultsClass->PostConstructLink; Property;
			Property = Property->PostConstructLinkNext)
		{
			Property->CopyCompleteValue_InContainer(Obj, DefaultData);
		}
		return;
	}

	// From a template: every property. A reference to one of the template's default subobjects becomes a reference to
	// the new object's subobject of the same name (D12: subobjects are rebuilt per instance, not instanced).
	for (FProperty* Property = DefaultsClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		Property->CopyCompleteValue_InContainer(Obj, DefaultData);
		if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			for (int32 Index = 0; Index < ObjectProperty->ArrayDim; ++Index)
			{
				UObject* Value = ObjectProperty->GetObjectPropertyValue_InContainer(Obj, Index);
				if (Value && Value->GetOuter() == DefaultData && Value->IsDefaultSubobject())
				{
					UObject* OwnSubobject =
						StaticFindObjectFastInternal(Value->GetClass(), Obj, Value->GetFName(), /*bExactClass =*/true);
					ObjectProperty->SetObjectPropertyValue_InContainer(Obj, OwnSubobject, Index);
				}
			}
		}
	}
}

// Construction

FStaticConstructObjectParameters::FStaticConstructObjectParameters(const UClass* InClass)
	: Class(InClass)
	, Name(NAME_None)
{
}

void CheckIsClassChildOf_Internal(const UClass* Parent, const UClass* Child)
{
	checkf(Child, "NewObject called with a null class");
	if (!Child->IsChildOf(Parent))
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("NewObject called with class %s, which is not a %s"), *Child->GetName(),
			*Parent->GetName());
	}
}

UObject* StaticAllocateObject(const UClass* InClass, UObject* InOuter, FName InName, EObjectFlags InFlags)
{
	checkf(UObjectInitialized(), "Objects cannot be created before CoreUObject starts");
	checkf(InClass, "StaticAllocateObject without a class");
	UClass* Class = const_cast<UClass*>(InClass);
	if (Class->HasAnyClassFlags(CLASS_Abstract) && !(InFlags & RF_ClassDefaultObject))
	{
		UE_LOG(LogUObjectGlobals, Fatal, TEXT("Class %s is abstract and cannot be instantiated"), *Class->GetName());
	}
	checkf(InOuter || Class->IsChildOf(UPackage::StaticClass()),
		"Object of class %s has no outer; only packages are outermost", *Class->GetName());

	if (InName.IsNone())
	{
		InName = MakeUniqueObjectName(InOuter, Class);
	}
	else if (UObject* Existing = StaticFindObjectFastInternal(nullptr, InOuter, InName))
	{
		UE_LOG(LogUObjectGlobals, Fatal,
			TEXT("Cannot create %s %s: an object with that name already exists (%s); Leon does not replace objects"),
			*Class->GetName(), *InName.ToString(), *Existing->GetFullName());
	}

	const int32 Size = Class->GetPropertiesSize();
	const int32 Alignment = FMath::Max(Class->GetMinAlignment(), int32(alignof(UObject)));
	checkf(Size >= int32(sizeof(UObject)), "Class %s has an invalid size %d", *Class->GetName(), Size);
	void* Memory = FMemory::Malloc(SIZE_T(Size), uint32(Alignment));
	FMemory::Memzero(Memory, SIZE_T(Size));

	FUObjectThreadContext::FPendingConstruction Pending;
	Pending.Memory = (UObjectBase*)Memory;
	Pending.Class = Class;
	Pending.Outer = InOuter;
	Pending.Name = InName;
	Pending.Flags = InFlags | RF_NeedInitialization;
	FUObjectThreadContext::Get().PendingConstructions.Add(Pending);
	// Soft pointers that did not find their object look again (UE bumps the tag when packages load).
	FSoftObjectPath::InvalidateTag();
	return (UObject*)Memory;
}

UObject* StaticConstructObject_Internal(const FStaticConstructObjectParameters& Params)
{
	UClass* Class = const_cast<UClass*>(Params.Class);
	checkf(Class, "StaticConstructObject_Internal without a class");
	UObject* Outer = Params.Outer;
	if (!Outer && !Class->IsChildOf(UPackage::StaticClass()))
	{
		Outer = GetTransientPackage();
	}
	checkf(!Params.Template || Params.Template->IsA(Class) || (Params.SetFlags & RF_ClassDefaultObject),
		"The template %s of a new %s is not one", Params.Template ? *Params.Template->GetName() : "",
		*Class->GetName());

	// Instances start from the class defaults unless a template is given. The class defaults exist once the class is
	// constructed; the objects made while the classes themselves are built (packages, reflection objects) have none.
	UObject* Template = Params.Template;
	if (!Template && !(Params.SetFlags & RF_ClassDefaultObject))
	{
		Template = Class->GetDefaultObject(Class->HasAnyClassFlags(CLASS_Constructed));
	}

	UObject* Result = StaticAllocateObject(Class, Outer, Params.Name, Params.SetFlags);
	{
		FObjectInitializer ObjectInitializer(Result, Template, Params.bCopyTransientsFromClassDefaults, true);
		(*Class->ClassConstructor)(ObjectInitializer);
	}
	return Result;
}

// Lookup

namespace
{
	/** Splits a path at its '.' and ':' separators; the first part is the package name. */
	void SplitObjectPath(const TCHAR* Path, TArray<FString>& OutParts)
	{
		FString Current;
		for (const TCHAR* Char = Path; *Char; ++Char)
		{
			if (*Char == '.' || *Char == ':')
			{
				OutParts.Add(Current);
				Current.Empty();
			}
			else
			{
				Current += *Char;
			}
		}
		OutParts.Add(Current);
	}
} // namespace

UObject* StaticFindObjectFast(UClass* ObjectClass, UObject* InOuter, FName InName, bool bExactClass, bool bAnyPackage,
	EObjectFlags ExclusiveFlags, EInternalObjectFlags ExclusiveInternalFlags)
{
	return StaticFindObjectFastInternal(
		ObjectClass, InOuter, InName, bExactClass, bAnyPackage, ExclusiveFlags, ExclusiveInternalFlags);
}

UObject* StaticFindObject(UClass* ObjectClass, UObject* InOuter, const TCHAR* Name, bool bExactClass)
{
	if (!Name || !*Name || FCString::Stricmp(Name, TEXT("None")) == 0)
	{
		return nullptr;
	}
	const bool bAnyPackage = InOuter == (UObject*)ANY_PACKAGE;
	const bool bIsPath = FCString::Strchr(Name, '.') != nullptr || FCString::Strchr(Name, ':') != nullptr;
	if (!bIsPath)
	{
		return StaticFindObjectFastInternal(
			ObjectClass, bAnyPackage ? nullptr : InOuter, FName(Name, FNAME_Find), bExactClass, bAnyPackage);
	}

	// A path: resolve its outers from the package (or from InOuter for a relative one) down to the object.
	TArray<FString> Parts;
	SplitObjectPath(Name, Parts);
	UObject* Outer = bAnyPackage ? nullptr : InOuter;
	for (int32 Index = 0; Index < Parts.Num() - 1; ++Index)
	{
		const FName PartName(*Parts[Index], FNAME_Find);
		if (PartName.IsNone())
		{
			return nullptr;
		}
		Outer = StaticFindObjectFastInternal(nullptr, Outer, PartName);
		if (!Outer)
		{
			return nullptr;
		}
	}
	const FName ObjectName(*Parts.Last(), FNAME_Find);
	return StaticFindObjectFastInternal(ObjectClass, Outer, ObjectName, bExactClass);
}

FName MakeUniqueObjectName(UObject* Outer, const UClass* Class, FName InBaseName)
{
	checkf(Class, "MakeUniqueObjectName without a class");
	const FName BaseName = InBaseName.IsNone() ? Class->GetFName() : InBaseName;
	FName TestName;
	do
	{
		TestName = FName(BaseName, ++Class->ClassUnique);
	} while (StaticFindObjectFastInternal(nullptr, Outer, TestName));
	return TestName;
}

UPackage* CreatePackage(const TCHAR* PackageName)
{
	checkf(PackageName && *PackageName, "CreatePackage without a name");
	const FName Name(PackageName);
	UPackage* Package = (UPackage*)StaticFindObjectFastInternal(UPackage::StaticClass(), nullptr, Name);
	if (!Package)
	{
		FStaticConstructObjectParameters Params(UPackage::StaticClass());
		Params.Name = Name;
		Params.SetFlags = RF_Public;
		Package = (UPackage*)StaticConstructObject_Internal(Params);
	}
	return Package;
}

UPackage* FindPackage(UObject* InOuter, const TCHAR* PackageName)
{
	if (!PackageName || !*PackageName)
	{
		return nullptr;
	}
	const FName Name(PackageName, FNAME_Find);
	if (Name.IsNone())
	{
		return nullptr;
	}
	return (UPackage*)StaticFindObjectFastInternal(UPackage::StaticClass(), InOuter, Name);
}

UPackage* LoadPackage(UPackage* InOuter, const TCHAR* InLongPackageName, uint32 LoadFlags)
{
	const FString Requested = (InLongPackageName && *InLongPackageName) ? FString(InLongPackageName)
																		: (InOuter ? InOuter->GetName() : FString());
	FString PackageName;
	if (FLinkerLoad::FindInMemoryPackage(Requested) || FPackageName::IsValidLongPackageName(Requested, true))
	{
		PackageName = Requested;
	}
	else if (!FPackageName::TryConvertFilenameToLongPackageName(Requested, PackageName))
	{
		if (!(LoadFlags & LOAD_Quiet))
		{
			UE_LOG(LogUObjectGlobals, Error,
				TEXT("LoadPackage: '%s' is neither a long package name nor a package file"), *Requested);
		}
		return nullptr;
	}
	if (FPackageName::IsScriptPackage(PackageName))
	{
		// Compiled in: nothing to load.
		return FindPackage(nullptr, *PackageName);
	}

	UPackage* Package = InOuter ? InOuter : FindPackage(nullptr, *PackageName);
	if (Package && (Package->LinkerLoad || (!InOuter && Package->IsFullyLoaded())))
	{
		// Loaded, created in memory without a file, or being loaded (a circular reference: its load finishes it).
		return Package;
	}

	const TArray<uint8>* PackageData = FLinkerLoad::FindInMemoryPackage(PackageName);
	FString Filename;
	if (!PackageData && !FPackageName::DoesPackageExist(PackageName, nullptr, &Filename))
	{
		if (!(LoadFlags & LOAD_Quiet))
		{
			FString Expected;
			FPackageName::TryConvertLongPackageNameToFilename(PackageName, Expected);
			if (LoadFlags & LOAD_NoWarn)
			{
				UE_LOG(LogUObjectGlobals, Log, TEXT("LoadPackage: %s does not exist (no %s.lasset / .lmap)"),
					*PackageName, *Expected);
			}
			else
			{
				UE_LOG(LogUObjectGlobals, Error, TEXT("LoadPackage: %s does not exist (no %s.lasset / .lmap)"),
					*PackageName, *Expected);
			}
		}
		return nullptr;
	}

	BeginLoad();
	if (!Package)
	{
		Package = CreatePackage(*PackageName);
	}
	FLinkerLoad* Linker = PackageData
		? FLinkerLoad::CreateLinkerFromMemory(Package, *PackageName, LoadFlags, *PackageData)
		: FLinkerLoad::CreateLinker(Package, *Filename, LoadFlags);
	if (Linker)
	{
		Linker->LoadAllObjects();
	}
	EndLoad();
	return Linker ? Package : nullptr;
}

UObject* StaticLoadObject(UClass* Class, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename, uint32 LoadFlags,
	bool bAllowObjectReconciliation)
{
	if (!Name || !*Name || FCString::Stricmp(Name, TEXT("None")) == 0)
	{
		return nullptr;
	}
	// "Class'/Game/Path.Asset'" names its object between the quotes.
	FString ObjectPath = Name;
	int32 QuoteIndex = INDEX_NONE;
	if (ObjectPath.Len() > 1 && ObjectPath[ObjectPath.Len() - 1] == '\'' && ObjectPath.FindChar('\'', QuoteIndex) &&
		QuoteIndex < ObjectPath.Len() - 1)
	{
		ObjectPath = ObjectPath.Mid(QuoteIndex + 1, ObjectPath.Len() - QuoteIndex - 2);
	}

	UObject* Result = bAllowObjectReconciliation ? StaticFindObject(Class, InOuter, *ObjectPath) : nullptr;
	if (!Result)
	{
		const FString PackageName =
			InOuter ? InOuter->GetOutermost()->GetName() : FPackageName::ObjectPathToPackageName(ObjectPath);
		LoadPackage(nullptr, Filename ? Filename : *PackageName, LoadFlags | LOAD_NoWarn);
		Result = StaticFindObject(Class, InOuter, *ObjectPath);
	}
	if (!Result && !(LoadFlags & (LOAD_NoWarn | LOAD_Quiet)))
	{
		UE_LOG(LogUObjectGlobals, Warning, TEXT("Failed to find object '%s %s'"),
			Class ? *Class->GetName() : TEXT("Object"), *ObjectPath);
	}
	return Result;
}

UClass* StaticLoadClass(UClass* BaseClass, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename, uint32 LoadFlags)
{
	UClass* Class = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), InOuter, Name, Filename, LoadFlags));
	if (Class && BaseClass && !Class->IsChildOf(BaseClass))
	{
		if (!(LoadFlags & (LOAD_NoWarn | LOAD_Quiet)))
		{
			UE_LOG(LogUObjectGlobals, Warning, TEXT("LoadClass: %s is not a child class of %s"), *Class->GetPathName(),
				*BaseClass->GetName());
		}
		return nullptr;
	}
	return Class;
}

UPackage* GetTransientPackage()
{
	static UPackage* TransientPackage = nullptr;
	if (!TransientPackage)
	{
		TransientPackage = CreatePackage(TEXT("/Engine/Transient"));
		TransientPackage->SetFlags(RF_Transient);
		TransientPackage->AddToRoot();
	}
	return TransientPackage;
}

// Reflection data from the generated tables (UE: UObjectGlobals.cpp, UE4CodeGen_Private).

namespace UE4CodeGen_Private
{
	namespace
	{
		FName NameFromUTF8(const char* NameUTF8)
		{
			return FName(UTF8_TO_TCHAR(NameUTF8));
		}

		/** Builds the property at the end of the array, then its children (they precede it), and steps back (UE). */
		void ConstructFProperty(
			FFieldVariant Outer, const FPropertyParamsBase* const*& PropertyArray, int32& NumProperties)
		{
			const FPropertyParamsBase* PropBase = *--PropertyArray;
			const FName Name = NameFromUTF8(PropBase->NameUTF8);
			const EObjectFlags ObjectFlags = PropBase->ObjectFlags;
			const EPropertyFlags PropertyFlags = PropBase->PropertyFlags;
			const int32 Offset = ((const FPropertyParamsBaseWithOffset*)PropBase)->Offset;
			uint32 ReadMore = 0;
			FProperty* NewProp = nullptr;
			switch (PropBase->Flags & PropertyTypeMask)
			{
				case EPropertyGenFlags::Byte:
				{
					const FBytePropertyParams* Prop = (const FBytePropertyParams*)PropBase;
					NewProp = new FByteProperty(
						Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->EnumFunc ? Prop->EnumFunc() : nullptr);
					break;
				}
				case EPropertyGenFlags::Int8:
					NewProp = new FInt8Property(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::Int16:
					NewProp = new FInt16Property(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::Int:
				case EPropertyGenFlags::UnsizedInt:
					NewProp = new FIntProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::Int64:
					NewProp = new FInt64Property(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::UInt16:
					NewProp = new FUInt16Property(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::UInt32:
				case EPropertyGenFlags::UnsizedUInt:
					NewProp = new FUInt32Property(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::UInt64:
					NewProp = new FUInt64Property(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::Float:
					NewProp = new FFloatProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::Double:
					NewProp = new FDoubleProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::Bool:
				{
					const FBoolPropertyParams* Prop = (const FBoolPropertyParams*)PropBase;
					// The generated SetBit sets the bool in a zeroed owner; the byte that changed locates it (UE).
					uint32 BoolOffset = 0;
					uint32 BitMask = 0;
					if (Prop->SetBitFunc)
					{
						uint8* Buffer = (uint8*)FMemory::Malloc(Prop->SizeOfOuter ? Prop->SizeOfOuter : 1);
						FMemory::Memzero(Buffer, Prop->SizeOfOuter);
						Prop->SetBitFunc(Buffer);
						for (uint32 TestOffset = 0; TestOffset < uint32(Prop->SizeOfOuter); ++TestOffset)
						{
							if (const uint8 Mask = Buffer[TestOffset])
							{
								BoolOffset = TestOffset;
								BitMask = Mask;
								checkf(FMath::IsPowerOfTwo(BitMask), "Bool property %s sets more than one bit",
									UTF8_TO_TCHAR(Prop->NameUTF8));
								break;
							}
						}
						FMemory::Free(Buffer);
					}
					const bool bNativeBool = !!(Prop->Flags & EPropertyGenFlags::NativeBool);
					NewProp = new FBoolProperty(Outer, Name, ObjectFlags, int32(BoolOffset), PropertyFlags, BitMask,
						Prop->ElementSize, bNativeBool);
					break;
				}
				case EPropertyGenFlags::Object:
				{
					const FObjectPropertyParams* Prop = (const FObjectPropertyParams*)PropBase;
					NewProp = new FObjectProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->ClassFunc());
					break;
				}
				case EPropertyGenFlags::Class:
				{
					const FClassPropertyParams* Prop = (const FClassPropertyParams*)PropBase;
					NewProp = new FClassProperty(
						Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->MetaClassFunc(), Prop->ClassFunc());
					break;
				}
				case EPropertyGenFlags::WeakObject:
				{
					const FWeakObjectPropertyParams* Prop = (const FWeakObjectPropertyParams*)PropBase;
					NewProp =
						new FWeakObjectProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->ClassFunc());
					break;
				}
				case EPropertyGenFlags::SoftObject:
				{
					const FSoftObjectPropertyParams* Prop = (const FSoftObjectPropertyParams*)PropBase;
					NewProp =
						new FSoftObjectProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->ClassFunc());
					break;
				}
				case EPropertyGenFlags::SoftClass:
				{
					const FSoftClassPropertyParams* Prop = (const FSoftClassPropertyParams*)PropBase;
					NewProp =
						new FSoftClassProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->MetaClassFunc());
					break;
				}
				case EPropertyGenFlags::Name:
					NewProp = new FNameProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::Str:
					NewProp = new FStrProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::Text:
					NewProp = new FTextProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					break;
				case EPropertyGenFlags::Struct:
				{
					const FStructPropertyParams* Prop = (const FStructPropertyParams*)PropBase;
					NewProp =
						new FStructProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->ScriptStructFunc());
					break;
				}
				case EPropertyGenFlags::Enum:
				{
					const FEnumPropertyParams* Prop = (const FEnumPropertyParams*)PropBase;
					NewProp = new FEnumProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->EnumFunc());
					ReadMore = 1;
					break;
				}
				case EPropertyGenFlags::Array:
				{
					const FArrayPropertyParams* Prop = (const FArrayPropertyParams*)PropBase;
					NewProp = new FArrayProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->ArrayFlags);
					ReadMore = 1;
					break;
				}
				case EPropertyGenFlags::Set:
					NewProp = new FSetProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags);
					ReadMore = 1;
					break;
				case EPropertyGenFlags::Map:
				{
					const FMapPropertyParams* Prop = (const FMapPropertyParams*)PropBase;
					NewProp = new FMapProperty(Outer, Name, ObjectFlags, Offset, PropertyFlags, Prop->MapFlags);
					ReadMore = 2;
					break;
				}
				default:
					UE_LOG(LogUObjectGlobals, Fatal, TEXT("Unsupported property type %d for %s"),
						int32(PropBase->Flags & PropertyTypeMask), *Name.ToString());
					break;
			}
			NewProp->ArrayDim = PropBase->ArrayDim;
			--NumProperties;
			for (; ReadMore; --ReadMore)
			{
				ConstructFProperty(NewProp, PropertyArray, NumProperties);
			}
		}

		/** Builds every property of the params array (children come before their parents) (UE). */
		void ConstructFProperties(
			FFieldVariant Outer, const FPropertyParamsBase* const* PropertyArray, int32 NumProperties)
		{
			PropertyArray += NumProperties;
			while (NumProperties)
			{
				ConstructFProperty(Outer, PropertyArray, NumProperties);
			}
		}
	} // namespace

	void ConstructUFunction(UFunction*& OutFunction, const FFunctionParams& Params)
	{
		UObject* Outer = Params.OuterFunc ? Params.OuterFunc() : nullptr;
		UFunction* Super = Params.SuperFunc ? Params.SuperFunc() : nullptr;
		if (OutFunction)
		{
			return;
		}
		FStaticConstructObjectParameters ConstructParams(UFunction::StaticClass());
		ConstructParams.Outer = Outer;
		ConstructParams.Name = NameFromUTF8(Params.NameUTF8);
		ConstructParams.SetFlags = Params.ObjectFlags;
		UFunction* NewFunction = (UFunction*)StaticConstructObject_Internal(ConstructParams);
		NewFunction->SetSuperStruct(Super);
		NewFunction->FunctionFlags = Params.FunctionFlags;
		NewFunction->SetPropertiesSize(int32(Params.StructureSize));
		NewFunction->MinAlignment = 1;
		OutFunction = NewFunction;
		ConstructFProperties(NewFunction, Params.PropertyArray, Params.NumProperties);
		NewFunction->Bind();
		NewFunction->StaticLink();
	}

	void ConstructUEnum(UEnum*& OutEnum, const FEnumParams& Params)
	{
		UObject* Outer = Params.OuterFunc ? Params.OuterFunc() : nullptr;
		if (OutEnum)
		{
			return;
		}
		FStaticConstructObjectParameters ConstructParams(UEnum::StaticClass());
		ConstructParams.Outer = Outer;
		ConstructParams.Name = NameFromUTF8(Params.NameUTF8);
		ConstructParams.SetFlags = Params.ObjectFlags;
		UEnum* NewEnum = (UEnum*)StaticConstructObject_Internal(ConstructParams);
		OutEnum = NewEnum;

		TArray<TPair<FName, int64>> EnumNames;
		EnumNames.Reserve(Params.NumEnumerators);
		for (int32 Index = 0; Index < Params.NumEnumerators; ++Index)
		{
			const FEnumeratorParam& Enumerator = Params.EnumeratorParams[Index];
			EnumNames.Emplace(NameFromUTF8(Enumerator.NameUTF8), Enumerator.Value);
		}
		NewEnum->SetEnums(EnumNames, (UEnum::ECppForm)Params.CppForm, Params.EnumFlags);
		NewEnum->CppType = UTF8_TO_TCHAR(Params.CppTypeUTF8);
		NewEnum->EnumDisplayNameFn = Params.DisplayNameFunc;
	}

	void ConstructUScriptStruct(UScriptStruct*& OutStruct, const FStructParams& Params)
	{
		UObject* Outer = Params.OuterFunc ? Params.OuterFunc() : nullptr;
		UScriptStruct* Super = Params.SuperFunc ? Params.SuperFunc() : nullptr;
		if (OutStruct)
		{
			return;
		}
		FStaticConstructObjectParameters ConstructParams(UScriptStruct::StaticClass());
		ConstructParams.Outer = Outer;
		ConstructParams.Name = NameFromUTF8(Params.NameUTF8);
		ConstructParams.SetFlags = Params.ObjectFlags;
		UScriptStruct* NewStruct = (UScriptStruct*)StaticConstructObject_Internal(ConstructParams);
		NewStruct->SetSuperStruct(Super);
		NewStruct->StructFlags =
			EStructFlags(Params.StructFlags | (Super ? uint32(Super->StructFlags & STRUCT_Inherit) : 0u));
		NewStruct->SetPropertiesSize(int32(Params.SizeOf));
		NewStruct->MinAlignment = int32(Params.AlignOf);
		OutStruct = NewStruct;
		if (Params.StructOpsFunc)
		{
			NewStruct->SetCppStructOps((UScriptStruct::ICppStructOps*)Params.StructOpsFunc());
		}
		ConstructFProperties(NewStruct, Params.PropertyArray, Params.NumProperties);
		NewStruct->StaticLink();
	}

	void ConstructUPackage(UPackage*& OutPackage, const FPackageParams& Params)
	{
		UPackage* NewPackage = CreatePackage(UTF8_TO_TCHAR(Params.NameUTF8));
		NewPackage->SetPackageFlags(Params.PackageFlags);
		NewPackage->SetGuid(FGuid(Params.BodyCRC, Params.DeclarationsCRC, 0u, 0u));
		OutPackage = NewPackage;
		for (int32 Index = 0; Index < Params.NumSingletons; ++Index)
		{
			Params.SingletonFuncArray[Index]();
		}
	}

	void ConstructUClass(UClass*& OutClass, const FClassParams& Params)
	{
		if (OutClass && OutClass->HasAnyClassFlags(CLASS_Constructed))
		{
			return;
		}
		// The super class and the package first (UE: DependentSingletons).
		for (int32 Index = 0; Index < Params.NumDependencySingletons; ++Index)
		{
			Params.DependencySingletonFuncArray[Index]();
		}
		UClass* NewClass = Params.ClassNoRegisterFunc();
		OutClass = NewClass;
		if (NewClass->HasAnyClassFlags(CLASS_Constructed))
		{
			return;
		}
		UObjectForceRegistration(NewClass);

		UClass* SuperClass = NewClass->GetSuperClass();
		if (SuperClass)
		{
			NewClass->ClassFlags |= (SuperClass->ClassFlags & CLASS_Inherit);
		}
		// A generated class is not intrinsic (COMPILED_IN_FLAGS marks every class declaration intrinsic, as UE does).
		NewClass->ClassFlags |= EClassFlags(Params.ClassFlags) | CLASS_Constructed;
		NewClass->ClassFlags &= ~CLASS_Intrinsic;
		if (Params.ClassConfigNameUTF8)
		{
			NewClass->ClassConfigName = NameFromUTF8(Params.ClassConfigNameUTF8);
		}
		NewClass->SetCppTypeInfoStatic(Params.CppClassInfo);
		NewClass->CreateLinkAndAddChildFunctionsToMap(Params.FunctionLinkArray, uint32(Params.NumFunctions));
		ConstructFProperties(NewClass, Params.PropertyArray, Params.NumProperties);
		NewClass->StaticLink();
	}
} // namespace UE4CodeGen_Private
