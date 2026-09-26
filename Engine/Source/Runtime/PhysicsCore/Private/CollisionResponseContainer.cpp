#include "CollisionResponseContainer.h"

namespace
{

	using FResponseMember = TEnumAsByte<ECollisionResponse> FCollisionResponseContainer::*;

	/** The members in channel order (UE keeps a union with EnumArray; Leon indexes the named members). */
	const FResponseMember ResponseMembers[FCollisionResponseContainer::NumChannels] = {
		&FCollisionResponseContainer::WorldStatic,
		&FCollisionResponseContainer::WorldDynamic,
		&FCollisionResponseContainer::Pawn,
		&FCollisionResponseContainer::Visibility,
		&FCollisionResponseContainer::Camera,
		&FCollisionResponseContainer::PhysicsBody,
		&FCollisionResponseContainer::Vehicle,
		&FCollisionResponseContainer::Destructible,
		&FCollisionResponseContainer::EngineTraceChannel1,
		&FCollisionResponseContainer::EngineTraceChannel2,
		&FCollisionResponseContainer::EngineTraceChannel3,
		&FCollisionResponseContainer::EngineTraceChannel4,
		&FCollisionResponseContainer::EngineTraceChannel5,
		&FCollisionResponseContainer::EngineTraceChannel6,
		&FCollisionResponseContainer::GameTraceChannel1,
		&FCollisionResponseContainer::GameTraceChannel2,
		&FCollisionResponseContainer::GameTraceChannel3,
		&FCollisionResponseContainer::GameTraceChannel4,
		&FCollisionResponseContainer::GameTraceChannel5,
		&FCollisionResponseContainer::GameTraceChannel6,
		&FCollisionResponseContainer::GameTraceChannel7,
		&FCollisionResponseContainer::GameTraceChannel8,
		&FCollisionResponseContainer::GameTraceChannel9,
		&FCollisionResponseContainer::GameTraceChannel10,
		&FCollisionResponseContainer::GameTraceChannel11,
		&FCollisionResponseContainer::GameTraceChannel12,
		&FCollisionResponseContainer::GameTraceChannel13,
		&FCollisionResponseContainer::GameTraceChannel14,
		&FCollisionResponseContainer::GameTraceChannel15,
		&FCollisionResponseContainer::GameTraceChannel16,
		&FCollisionResponseContainer::GameTraceChannel17,
		&FCollisionResponseContainer::GameTraceChannel18,
	};

	static_assert(ECC_GameTraceChannel18 + 1 == FCollisionResponseContainer::NumChannels,
		"FCollisionResponseContainer holds one member per channel");

} // namespace

FCollisionResponseContainer FCollisionResponseContainer::DefaultResponseContainer(ECR_Block);

FCollisionResponseContainer::FCollisionResponseContainer()
	: FCollisionResponseContainer(ECR_Block)
{
}

FCollisionResponseContainer::FCollisionResponseContainer(ECollisionResponse DefaultResponse)
{
	for (int32 Index = 0; Index < NumChannels; ++Index)
	{
		*GetResponseRef(Index) = DefaultResponse;
	}
}

TEnumAsByte<ECollisionResponse>* FCollisionResponseContainer::GetResponseRef(int32 Index)
{
	return Index >= 0 && Index < NumChannels ? &(this->*ResponseMembers[Index]) : nullptr;
}

const TEnumAsByte<ECollisionResponse>* FCollisionResponseContainer::GetResponseRef(int32 Index) const
{
	return Index >= 0 && Index < NumChannels ? &(this->*ResponseMembers[Index]) : nullptr;
}

bool FCollisionResponseContainer::SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
	TEnumAsByte<ECollisionResponse>* Response = GetResponseRef(static_cast<int32>(Channel));
	if (Response == nullptr || *Response == NewResponse)
	{
		return false;
	}
	*Response = NewResponse;
	return true;
}

bool FCollisionResponseContainer::SetAllChannels(ECollisionResponse NewResponse)
{
	bool bChanged = false;
	for (int32 Index = 0; Index < NumChannels; ++Index)
	{
		bChanged |= SetResponse(static_cast<ECollisionChannel>(Index), NewResponse);
	}
	return bChanged;
}

bool FCollisionResponseContainer::ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)
{
	bool bChanged = false;
	for (int32 Index = 0; Index < NumChannels; ++Index)
	{
		TEnumAsByte<ECollisionResponse>& Response = *GetResponseRef(Index);
		if (Response == OldResponse && OldResponse != NewResponse)
		{
			Response = NewResponse;
			bChanged = true;
		}
	}
	return bChanged;
}

ECollisionResponse FCollisionResponseContainer::GetResponse(ECollisionChannel Channel) const
{
	const TEnumAsByte<ECollisionResponse>* Response = GetResponseRef(static_cast<int32>(Channel));
	return Response != nullptr ? Response->GetValue() : ECR_Ignore;
}

FCollisionResponseContainer FCollisionResponseContainer::CreateMinContainer(
	const FCollisionResponseContainer& A, const FCollisionResponseContainer& B)
{
	FCollisionResponseContainer Result;
	for (int32 Index = 0; Index < NumChannels; ++Index)
	{
		const ECollisionChannel Channel = static_cast<ECollisionChannel>(Index);
		(void)Result.SetResponse(Channel, FMath::Min(A.GetResponse(Channel), B.GetResponse(Channel)));
	}
	return Result;
}

bool FCollisionResponseContainer::operator==(const FCollisionResponseContainer& Other) const
{
	for (int32 Index = 0; Index < NumChannels; ++Index)
	{
		if (*GetResponseRef(Index) != *Other.GetResponseRef(Index))
		{
			return false;
		}
	}
	return true;
}
