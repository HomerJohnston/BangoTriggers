#include "BangoActivatorSource.h"

#include "BangoActorRegistry.h"
#include "BangoTrigger.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void FBangoActorClassSource::Gather(const ABangoTrigger& Trigger, TArray<AActor*>& OutActors) const
{
	UWorld* World = Trigger.GetWorld();
	if (!World)
	{
		return;
	}
	for (const TSubclassOf<AActor>& Class : Classes)
	{
		if (*Class)
		{
			for (TActorIterator<AActor> It(World, *Class); It; ++It)
			{
				OutActors.Add(*It);
			}
		}
	}
}

void FBangoIDsSource::Gather(const ABangoTrigger& Trigger, TArray<AActor*>& OutActors) const
{
	if (const UBangoActorRegistry* Registry = UBangoActorRegistry::Get(&Trigger))
	{
		for (const FString& Name : Names)
		{
			if (AActor* Actor = Registry->FindActor(Name))
			{
				OutActors.Add(Actor);
			}
		}
	}
}

void FBangoPlayersSource::Gather(const ABangoTrigger& Trigger, TArray<AActor*>& OutActors) const
{
	if (const UWorld* World = Trigger.GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			if (APawn* Pawn = Controller ? Controller->GetPawn() : nullptr)
			{
				OutActors.Add(Pawn);
			}
		}
	}
}

void FBangoLocalPlayersSource::Gather(const ABangoTrigger& Trigger, TArray<AActor*>& OutActors) const
{
	if (const UWorld* World = Trigger.GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			if (Controller && Controller->IsLocalController())
			{
				if (APawn* Pawn = Controller->GetPawn())
				{
					OutActors.Add(Pawn);
				}
			}
		}
	}
}
