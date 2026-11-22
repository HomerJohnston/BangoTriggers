#include "BangoTriggerConstant.h"

#include "BangoActorRegistry.h"
#include "BangoCompiler.h"
#include "BangoIDActorComponent.h"
#include "GameFramework/Actor.h"

bool FBangoTriggerConstant::GetValue(FBangoValue& OutValue, FString& OutError) const
{
	FBangoType ParsedType;
	if (!FBangoType::FromString(Type, ParsedType))
	{
		OutValue = FBangoValue();
		OutError = FString::Printf(TEXT("'%s' isn't a type."), *Type);
		return false;
	}
	OutValue = FBangoValue::MakeNil(ParsedType);

	if (!IsActorType())
	{
		FBangoValue Parsed;
		if (!Bango::ParseFixedValue(Type, Value, Parsed, OutError))
		{
			return false;
		}
		OutValue = Parsed;
		return true;
	}

	if (!Actor)
	{
		return true;
	}
	// Worked out as the trigger begins play, maybe before this actor has: its Bango ID name is registered first, so it
	// isn't given another ID (registering again is a no-op).
	if (UBangoIDActorComponent* ID = Actor->FindComponentByClass<UBangoIDActorComponent>())
	{
		ID->RegisterName();
	}
	FBangoValue Entity;
	if (!Bango::ActorToEntity(Actor, Entity, OutError))
	{
		return false;
	}
	OutValue = Entity;
	return true;
}
