// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneVectorSection.h"
#include "MovieSceneVectorTrack.h"


/* FMovieSceneVectorKeyStruct interface
 *****************************************************************************/

void FMovieSceneVectorKeyStruct::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	for (int32 Index = 0; Index <= 3; ++Index)
	{
		Keys[Index]->Value = Vector[Index];
	}
}


/* UMovieSceneVectorSection structors
 *****************************************************************************/

UMovieSceneVectorSection::UMovieSceneVectorSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChannelsUsed = 0;
}


/* UMovieSceneVectorSection interface
 *****************************************************************************/

FVector4 UMovieSceneVectorSection::Eval(float Position, const FVector4& DefaultVector) const
{
	return FVector4(
		Curves[0].Eval(Position, DefaultVector.X),
		Curves[1].Eval(Position, DefaultVector.Y),
		Curves[2].Eval(Position, DefaultVector.Z),
		Curves[3].Eval(Position, DefaultVector.W));
}


/* UMovieSceneSection interface
 *****************************************************************************/

void UMovieSceneVectorSection::MoveSection(float DeltaTime, TSet<FKeyHandle>& KeyHandles)
{
	check(ChannelsUsed >= 2 && ChannelsUsed <= 4);
	Super::MoveSection(DeltaTime, KeyHandles);

	for (int32 i = 0; i < ChannelsUsed; ++i)
	{
		Curves[i].ShiftCurve(DeltaTime, KeyHandles);
	}
}


void UMovieSceneVectorSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	check(ChannelsUsed >= 2 && ChannelsUsed <= 4);
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	for (int32 i = 0; i < ChannelsUsed; ++i)
	{
		Curves[i].ScaleCurve(Origin, DilationFactor, KeyHandles);
	}
}


void UMovieSceneVectorSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (int32 i = 0; i < ChannelsUsed; ++i)
	{
		for (auto It(Curves[i].GetKeyHandleIterator()); It; ++It)
		{
			float Time = Curves[i].GetKeyTime(It.Key());
			if (TimeRange.Contains(Time))
			{
				OutKeyHandles.Add(It.Key());
			}
		}
	}
}


TSharedPtr<FStructOnScope> UMovieSceneVectorSection::GetKeyStruct(const TArray<FKeyHandle>& KeyHandles)
{
	TSharedRef<FStructOnScope> KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneVectorKeyStruct::StaticStruct()));
	auto Struct = (FMovieSceneVectorKeyStruct*)KeyStruct->GetStructMemory();
	{
		for (int32 Index = 0; Index <= 3; ++Index)
		{
			Struct->Keys[Index] = Curves[Index].GetFirstMatchingKey(KeyHandles);
			check(Struct->Keys[Index] != nullptr);
			Struct->Vector[Index] = Struct->Keys[Index]->Value;
		}
	}

	return KeyStruct;
}


/* IKeyframeSection interface
 *****************************************************************************/

template<typename CurveType>
CurveType* GetCurveForChannel(EKeyVectorChannel Channel, CurveType* Curves, int32 ChannelsUsed)
{
	switch (Channel)
	{
		case EKeyVectorChannel::X:
			return &Curves[0];
		case EKeyVectorChannel::Y:
			return &Curves[1];
		case EKeyVectorChannel::Z:
			checkf(ChannelsUsed >= 3, TEXT("Can not get Z channel, it is not in use on this section."));
			return &Curves[2];
		case EKeyVectorChannel::W:
			checkf(ChannelsUsed >= 4, TEXT("Can not get W channel, it is not in use on this section."));
			return &Curves[3];
	}
	checkf(false, TEXT("Invalid channel requested"));
	return nullptr;
}


void UMovieSceneVectorSection::AddKey(float Time, const FVectorKey& Key, EMovieSceneKeyInterpolation KeyInterpolation)
{
	FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, Curves, ChannelsUsed);
	AddKeyToCurve(*ChannelCurve, Time, Key.Value, KeyInterpolation);
}


bool UMovieSceneVectorSection::NewKeyIsNewData(float Time, const FVectorKey& Key) const
{
	const FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, Curves, ChannelsUsed);
	return FMath::IsNearlyEqual(ChannelCurve->Eval(Time), Key.Value) == false;
}


bool UMovieSceneVectorSection::HasKeys(const FVectorKey& Key) const
{
	const FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, Curves, ChannelsUsed);
	return ChannelCurve->GetNumKeys() > 0;
}


void UMovieSceneVectorSection::SetDefault(const FVectorKey& Key)
{
	FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, Curves, ChannelsUsed);
	ChannelCurve->SetDefaultValue(Key.Value);
}
