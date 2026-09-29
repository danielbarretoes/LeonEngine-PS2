#include "Factories/BlendSpaceFactoryNew.h"

#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "Factories/AimOffsetBlendSpaceFactory1D.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/BlendSpaceFactory1D.h"
#include "LeonEdLog.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace
{

	/**
	 * The asset Reference names: a long object path (`/Game/A/A_Idle.A_Idle`), a long package name (`/Game/A/A_Idle`)
	 * or a name in the folder of InParent's package (`A_Idle`). Null (logged) when it does not load as a T.
	 */
	template <typename T>
	T* LoadReferencedAsset(const FString& Reference, const UObject* InParent, const TCHAR* FactoryName)
	{
		FString Path = Reference.TrimStartAndEnd();
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		if (!Path.StartsWith(TEXT("/")))
		{
			Path = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName()) + TEXT("/") + Path;
		}
		if (!Path.Contains(TEXT(".")))
		{
			Path += TEXT(".") + FPackageName::GetShortName(Path);
		}
		T* Asset = LoadObject<T>(nullptr, *Path);
		if (Asset == nullptr)
		{
			UE_LOG(LogLeonEd, Error, "%s: '%s' is not a %s", FactoryName, *Path, *T::StaticClass()->GetName());
		}
		return Asset;
	}

	/** The comma-separated fields of an entry, trimmed. */
	TArray<FString> SplitFields(const FString& Entry)
	{
		TArray<FString> Fields;
		Entry.ParseIntoArray(Fields, TEXT(","), false);
		for (FString& Field : Fields)
		{
			Field.TrimStartAndEndInline();
		}
		return Fields;
	}

	/** The `;`-separated entries of a setting. */
	TArray<FString> SplitEntries(const FString& Setting)
	{
		TArray<FString> Entries;
		Setting.ParseIntoArray(Entries, TEXT(";"), true);
		return Entries;
	}

	/** A number field, false (logged) when it is not one. */
	bool ParseNumber(const FString& Field, float& OutValue, const TCHAR* What)
	{
		if (!Field.IsNumeric())
		{
			UE_LOG(LogLeonEd, Error, "%s: '%s' is not a number", What, *Field);
			return false;
		}
		OutValue = FCString::Atof(*Field);
		return true;
	}

	/** An axis `Name,Min,Max` into Parameter; false (logged) when malformed. An empty setting keeps the default. */
	bool ParseAxis(const FString& Setting, FBlendParameter& Parameter)
	{
		if (Setting.IsEmpty())
		{
			return true;
		}
		const TArray<FString> Fields = SplitFields(Setting);
		if (Fields.Num() != 3 || !ParseNumber(Fields[1], Parameter.Min, TEXT("BlendSpaceFactory axis")) ||
			!ParseNumber(Fields[2], Parameter.Max, TEXT("BlendSpaceFactory axis")) || Parameter.Max <= Parameter.Min)
		{
			UE_LOG(LogLeonEd, Error, "BlendSpaceFactory: an axis is 'Name,Min,Max' with Min < Max, not '%s'", *Setting);
			return false;
		}
		Parameter.DisplayName = Fields[0];
		return true;
	}

} // namespace

UBlendSpaceFactoryNew::UBlendSpaceFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UBlendSpace::StaticClass();
	bCreateNew = 1;
}

UBlendSpaceFactory1D::UBlendSpaceFactory1D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UBlendSpace1D::StaticClass();
}

UAimOffsetBlendSpaceFactory1D::UAimOffsetBlendSpaceFactory1D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UAimOffsetBlendSpace1D::StaticClass();
}

UObject* UBlendSpaceFactoryNew::FactoryCreateNew(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context)
{
	(void)Context;
	UClass* Class = InClass != nullptr && InClass->IsChildOf(SupportedClass.Get()) ? InClass : SupportedClass.Get();
	UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(CreateOrOverwriteAsset(Class, InParent, InName, Flags));
	if (BlendSpace == nullptr)
	{
		return nullptr;
	}
	// Made again from the description, whatever the asset held.
	BlendSpace->ClearSamples();
	const UBlendSpaceBase* Defaults = Class->GetDefaultObject<UBlendSpaceBase>();
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		BlendSpace->BlendParameters[Axis] = Defaults->BlendParameters[Axis];
	}
	if (!ParseAxis(AxisX, BlendSpace->BlendParameters[0]) ||
		(GetNumAxes() > 1 && !ParseAxis(AxisY, BlendSpace->BlendParameters[1])))
	{
		return nullptr;
	}

	for (const FString& Entry : SplitEntries(Sample))
	{
		const TArray<FString> Fields = SplitFields(Entry);
		FVector Value = FVector::ZeroVector;
		if (Fields.Num() != 1 + GetNumAxes() || !ParseNumber(Fields[1], Value.X, TEXT("BlendSpaceFactory sample")) ||
			(GetNumAxes() > 1 && !ParseNumber(Fields[2], Value.Y, TEXT("BlendSpaceFactory sample"))))
		{
			UE_LOG(LogLeonEd, Error, "BlendSpaceFactory: a sample of %s is 'Animation,%s', not '%s'",
				*InName.ToString(), GetNumAxes() > 1 ? TEXT("X,Y") : TEXT("X"), *Entry);
			return nullptr;
		}
		UAnimSequence* Animation = LoadReferencedAsset<UAnimSequence>(Fields[0], InParent, TEXT("BlendSpaceFactory"));
		if (Animation == nullptr || !BlendSpace->AddSample(Animation, Value))
		{
			return nullptr;
		}
	}
	if (BlendSpace->GetBlendSamples().Num() == 0)
	{
		UE_LOG(LogLeonEd, Error, "BlendSpaceFactory: %s has no samples (+Sample=Animation,X[,Y])", *InName.ToString());
		return nullptr;
	}

	USkeleton* LocalSkeleton = Skeleton.IsEmpty()
		? BlendSpace->GetBlendSamples()[0].Animation->GetSkeleton()
		: LoadReferencedAsset<USkeleton>(Skeleton, InParent, TEXT("BlendSpaceFactory"));
	BlendSpace->SetSkeleton(LocalSkeleton);

	if (UAimOffsetBlendSpace1D* AimOffset = Cast<UAimOffsetBlendSpace1D>(BlendSpace))
	{
		AimOffset->BasePose = BasePose.IsEmpty()
			? nullptr
			: LoadReferencedAsset<UAnimSequence>(BasePose, InParent, TEXT("BlendSpaceFactory"));
		if (!BasePose.IsEmpty() && AimOffset->BasePose == nullptr)
		{
			return nullptr;
		}
	}
	return BlendSpace;
}

UAnimMontageFactory::UAnimMontageFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UAnimMontage::StaticClass();
	bCreateNew = 1;
}

UObject* UAnimMontageFactory::FactoryCreateNew(
	UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context)
{
	(void)InClass;
	(void)Context;
	UAnimSequence* Clip = LoadReferencedAsset<UAnimSequence>(Animation, InParent, TEXT("AnimMontageFactory"));
	if (Clip == nullptr)
	{
		UE_LOG(LogLeonEd, Error, "AnimMontageFactory: %s needs an Animation", *InName.ToString());
		return nullptr;
	}
	UAnimMontage* Montage = CreateOrOverwriteAsset<UAnimMontage>(InParent, InName, Flags);
	if (Montage == nullptr)
	{
		return nullptr;
	}
	Montage->CompositeSections.Reset();
	Montage->Notifies.Reset();
	Montage->SetAnimation(Clip);
	Montage->SlotName = SlotName.IsEmpty() ? UAnimMontage::DefaultSlotName : FName(*SlotName);
	Montage->BlendInTime = FMath::Max(BlendInTime, 0.0f);
	Montage->BlendOutTime = FMath::Max(BlendOutTime, 0.0f);

	for (const FString& Entry : SplitEntries(Section))
	{
		const TArray<FString> Fields = SplitFields(Entry);
		float Start = 0.0f;
		if ((Fields.Num() != 2 && Fields.Num() != 3) || Fields[0].IsEmpty() ||
			!ParseNumber(Fields[1], Start, TEXT("AnimMontageFactory section")))
		{
			UE_LOG(
				LogLeonEd, Error, "AnimMontageFactory: a section is 'Name,StartTime[,NextSection]', not '%s'", *Entry);
			return nullptr;
		}
		(void)Montage->AddSection(FName(*Fields[0]), Start, Fields.Num() == 3 ? FName(*Fields[2]) : NAME_None);
	}
	for (const FCompositeSection& Link : Montage->CompositeSections)
	{
		if (!Link.NextSectionName.IsNone() && Montage->GetSectionIndex(Link.NextSectionName) == INDEX_NONE)
		{
			UE_LOG(LogLeonEd, Error, "AnimMontageFactory: the section %s links to %s, which %s does not have",
				*Link.SectionName.ToString(), *Link.NextSectionName.ToString(), *InName.ToString());
			return nullptr;
		}
	}
	for (const FString& Entry : SplitEntries(Notify))
	{
		const TArray<FString> Fields = SplitFields(Entry);
		float Time = 0.0f;
		if (Fields.Num() != 2 || Fields[0].IsEmpty() ||
			!ParseNumber(Fields[1], Time, TEXT("AnimMontageFactory notify")))
		{
			UE_LOG(LogLeonEd, Error, "AnimMontageFactory: a notify is 'Name,Time', not '%s'", *Entry);
			return nullptr;
		}
		(void)Montage->AddNotify(FName(*Fields[0]), Time);
	}
	return Montage;
}
