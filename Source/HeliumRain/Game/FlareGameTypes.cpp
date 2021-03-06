#include "../Flare.h"
#include "FlareGameTypes.h"

#define LOCTEXT_NAMESPACE "FlareNavigationHUD"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareGameTypes::UFlareGameTypes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


/*----------------------------------------------------
	Text
----------------------------------------------------*/

FText UFlareGameTypes::GetCombatGroupDescription(EFlareCombatGroup::Type Type)
{
	FText Result;

	switch (Type)
	{
		case EFlareCombatGroup::AllMilitary:   Result = LOCTEXT("AllMilitary",  "All military ships");   break;
		case EFlareCombatGroup::Capitals:      Result = LOCTEXT("AllCapitals",  "Capital ships");        break;
		case EFlareCombatGroup::Fighters:      Result = LOCTEXT("AllFighters",  "Fighters");             break;
		case EFlareCombatGroup::Civilan:       Result = LOCTEXT("AllCivilians", "Freighters");           break;
	}

	return Result;
}

FText UFlareGameTypes::GetCombatTacticDescription(EFlareCombatTactic::Type Type)
{
	FText Result;

	switch (Type)
	{
		case EFlareCombatTactic::ProtectMe:        Result = LOCTEXT("ProtectMe",       "Protect me");         break;
		case EFlareCombatTactic::AttackMilitary:   Result = LOCTEXT("AttackMilitary",  "Attack military");    break;
		case EFlareCombatTactic::AttackStations:   Result = LOCTEXT("AttackStations",  "Attack stations");    break;
		case EFlareCombatTactic::AttackCivilians:  Result = LOCTEXT("AttackCivilians", "Attack freighters");  break;
		case EFlareCombatTactic::StandDown:        Result = LOCTEXT("StandDown",       "Stand down");         break;
	}

	return Result;
}

const FSlateBrush* UFlareGameTypes::GetCombatGroupIcon(EFlareCombatGroup::Type Type)
{
	const FSlateBrush* Result = NULL;

	switch (Type)
	{
		case EFlareCombatGroup::AllMilitary:   Result = FFlareStyleSet::GetIcon("AllMilitary");   break;
		case EFlareCombatGroup::Capitals:      Result = FFlareStyleSet::GetIcon("AllCapitals");   break;
		case EFlareCombatGroup::Fighters:      Result = FFlareStyleSet::GetIcon("AllFighters");   break;
		case EFlareCombatGroup::Civilan:       Result = FFlareStyleSet::GetIcon("AllFreighters"); break;
	}

	return Result;
}

/*----------------------------------------------------
	Float buffer
----------------------------------------------------*/

void FFlareFloatBuffer::Init(int32 Size)
{
	MaxSize = Size;
	Values.Empty(MaxSize);
	WriteIndex = 0;
}

void FFlareFloatBuffer::Resize(int32 Size)
{
	if(Size <= Values.Num())
	{
		TArray<float> NewValues;
		for (int Age = Size-1; Age >= 0; Age--)
		{
			NewValues.Add(GetValue(Age));
		}
		// Override
		Values = NewValues;
		WriteIndex = 0;
	}

	MaxSize = Size;
}

void FFlareFloatBuffer::Append(float NewValue)
{
	if(Values.Num() <= WriteIndex)
	{
		Values.Add(NewValue);
		WriteIndex = Values.Num();
	}
	else
	{
		Values[WriteIndex] = NewValue;
		WriteIndex++;
	}

	if(WriteIndex >= MaxSize)
	{
		WriteIndex = 0;
	}
}

float FFlareFloatBuffer::GetValue(int32 Age)
{
	if(Values.Num() == 0)
	{
		return 0.f;
	}

	if(Age >= Values.Num())
	{
		Age = Values.Num() - 1;
	}

	int32 ReadIndex = WriteIndex - 1 - Age;
	if (ReadIndex < 0)
	{
		ReadIndex += Values.Num();
	}

	return Values[ReadIndex];
}

float FFlareFloatBuffer::GetMean(int32 StartAge, int32 EndAge)
{
	float Count = 0.f;
	float Sum = 0.f;

	if(StartAge >= Values.Num())
	{
		StartAge = Values.Num() - 1;
	}

	if(EndAge >= Values.Num())
	{
		EndAge = Values.Num() - 1;
	}

	for (int Age = StartAge; Age <= EndAge; Age++)
	{
		Sum += GetValue(Age);
		Count += 1.f;
	}

	if(Count == 0.f)
	{
		return 0.f;
	}
	return Sum/Count;
}


#undef LOCTEXT_NAMESPACE
