#include "BangoTrigger.h"

#include "BangoTriggerComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

ABangoTrigger::ABangoTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Trigger = CreateDefaultSubobject<UBangoTriggerComponent>(TEXT("Trigger"));

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite && !IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> TriggerTexture(TEXT("/Engine/EditorResources/S_Trigger"));
		Sprite->Sprite = TriggerTexture.Get();
		Sprite->SetupAttachment(Root);
		Sprite->bIsScreenSizeScaled = true;
		Sprite->SetRelativeScale3D(FVector(0.5f));
	}
#endif
}
