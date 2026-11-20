#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BangoGlobalsSubsystem.h"
#include "BangoIDActorComponent.h"
#include "BangoTrigger.h"
#include "BangoTriggerTestActor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

// Whole triggers in a game world: sources, regions, the Condition's watches, the Rule, events and scripts.
// Run from the editor: Tools > Session Frontend > Automation, filter "Triggers.Play". Or BuildBangoTriggers.bat test.

namespace BangoTriggerPlayTestsPrivate
{
	constexpr float FrameSeconds = 0.1f;

	/**
	 * A game instance with a game world that has begun play, for one test. A game instance, because Bango's globals
	 * (and so watched Conditions) live in one. No game mode: play is begun by hand.
	 */
	struct FPlayWorld
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		UBangoTriggerTestListener* Listener = nullptr;

		FPlayWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			check(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			World->GetWorldSettings()->NotifyBeginPlay();

			Listener = NewObject<UBangoTriggerTestListener>();
			Listener->AddToRoot();
		}

		~FPlayWorld()
		{
			Listener->RemoveFromRoot();
			World->EndPlay(EEndPlayReason::Quit);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			GameInstance->Shutdown();
			GameInstance->RemoveFromRoot();
		}

		void Tick(int32 Frames = 2)
		{
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				World->Tick(LEVELTICK_All, FrameSeconds);
			}
		}

		/** A movable actor with the Bango ID Name. */
		ABangoTriggerTestActor* Spawn(const TCHAR* Name, const FVector& Location)
		{
			ABangoTriggerTestActor* Actor = World->SpawnActor<ABangoTriggerTestActor>(Location, FRotator::ZeroRotator);
			UBangoIDActorComponent* ID = NewObject<UBangoIDActorComponent>(Actor);
			ID->BangoName = Name;
			// The actor has begun play, so registering the component begins its play too, which registers its name.
			ID->RegisterComponent();
			return Actor;
		}

		/** A trigger with no sources or regions and a Poll Interval of 0, set up by Setup before it begins play. */
		ABangoTrigger* SpawnTrigger(TFunctionRef<void(ABangoTrigger&)> Setup)
		{
			ABangoTrigger* Trigger = World->SpawnActorDeferred<ABangoTrigger>(ABangoTrigger::StaticClass(), FTransform::Identity);
			Trigger->Sources.Reset();
			Trigger->Regions.Reset();
			Trigger->PollInterval = 0.0f;
			Setup(*Trigger);
			Trigger->FinishSpawning(FTransform::Identity);
			Listener->Listen(Trigger);
			return Trigger;
		}

		void SetGlobal(const TCHAR* Name, const FBangoValue& Value)
		{
			UBangoGlobalsSubsystem* Globals = UBangoGlobalsSubsystem::Get(World);
			check(Globals);
			FString Error;
			Globals->Declare(Name, Value.GetType(), Error);
			Globals->Set(Name, Value, Error);
		}

		/** A global int's value, or -1 if it isn't set. */
		int32 GetGlobalInt(const TCHAR* Name) const
		{
			const UBangoGlobalsSubsystem* Globals = UBangoGlobalsSubsystem::Get(World);
			const FBangoValue* Value = Globals ? Globals->Find(Name) : nullptr;
			return (Value && !Value->IsNil()) ? Value->GetInt() : -1;
		}
	};

	TInstancedStruct<FBangoActivatorSource> TestActors()
	{
		FBangoActorClassSource Source;
		Source.Classes.Add(ABangoTriggerTestActor::StaticClass());
		return TInstancedStruct<FBangoActivatorSource>::Make<FBangoActorClassSource>(Source);
	}

	TInstancedStruct<FBangoActivatorSource> IDs(const TArray<FString>& Names)
	{
		FBangoIDsSource Source;
		Source.Names = Names;
		return TInstancedStruct<FBangoActivatorSource>::Make<FBangoIDsSource>(Source);
	}

	/** A 200 by 200 area around the trigger (at the origin). */
	TInstancedStruct<FBangoTriggerRegion> Box200()
	{
		FBangoAreaRegion Area;
		Area.Shape = EBangoAreaShape::Rectangle;
		Area.Size = FVector2D(200.0, 200.0);
		return TInstancedStruct<FBangoTriggerRegion>::Make<FBangoAreaRegion>(Area);
	}

	const FVector Inside(0, 0, 0);
	const FVector Outside(1000, 0, 0);
}

using namespace BangoTriggerPlayTestsPrivate;

#define BANGO_TRIGGER_PLAY_TEST(Class, Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, "Triggers.Play." Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayEnterLeaveTest, "EnterAndLeave")

bool FBangoTriggersPlayEnterLeaveTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Outside);
	ABangoTrigger* Trigger = W.SpawnTrigger([](ABangoTrigger& T)
	{
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
	});

	W.Tick();
	TestEqual(TEXT("Outside: nothing"), W.Listener->Take(), FString());
	TestEqual(TEXT("Outside: gathered"), Trigger->GetGatheredActors().Num(), 1);
	TestEqual(TEXT("Outside: not potential"), Trigger->GetPotentialActivators().Num(), 0);
	TestFalse(TEXT("Outside: not active"), Trigger->IsTriggerActive());

	A->SetActorLocation(Inside);
	W.Tick();
	TestEqual(TEXT("Enter: In, then Activated"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));
	TestTrue(TEXT("Enter: active"), Trigger->IsTriggerActive());
	TestEqual(TEXT("Enter: actual"), Trigger->GetActualActivators().Num(), 1);

	W.Tick(5);
	TestEqual(TEXT("Staying: nothing more"), W.Listener->Take(), FString());

	A->SetActorLocation(Outside);
	W.Tick();
	TestEqual(TEXT("Leave: Out, then Deactivated"), W.Listener->Take(), FString(TEXT("Out:a|Deactivated:a")));
	TestFalse(TEXT("Leave: not active"), Trigger->IsTriggerActive());
	TestEqual(TEXT("Activated once"), Trigger->GetActivationCount(), 1);
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayConditionTest, "Condition")

bool FBangoTriggersPlayConditionTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	W.Spawn(TEXT("a"), Inside);
	ABangoTrigger* Trigger = W.SpawnTrigger([](ABangoTrigger& T)
	{
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
		T.Condition.Source = TEXT("allowed");
	});

	W.Tick();
	TestEqual(TEXT("Global not set yet (Unset): nothing"), W.Listener->Take(), FString());
	TestEqual(TEXT("Unset: potential"), Trigger->GetPotentialActivators().Num(), 1);
	TestEqual(TEXT("Unset: not actual"), Trigger->GetActualActivators().Num(), 0);

	W.SetGlobal(TEXT("allowed"), FBangoValue::MakeBool(true));
	W.Tick();
	TestEqual(TEXT("Global true: In, then Activated"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));

	W.SetGlobal(TEXT("allowed"), FBangoValue::MakeBool(false));
	W.Tick();
	TestEqual(TEXT("Global false: Out, then Deactivated"), W.Listener->Take(), FString(TEXT("Out:a|Deactivated:a")));
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayNotPresentAllTest, "NotPresentAll")

bool FBangoTriggersPlayNotPresentAllTest::RunTest(const FString& Parameters)
{
	// "Neither a nor b is in the area."
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Outside);
	W.Spawn(TEXT("b"), Outside + FVector(0, 500, 0));
	ABangoTrigger* Trigger = W.SpawnTrigger([](ABangoTrigger& T)
	{
		T.Sources.Add(IDs({ TEXT("a"), TEXT("b") }));
		T.Regions.Add(Box200());
		T.Presence = EBangoTriggerPresence::NotPresent;
		T.Rule = EBangoTriggerRule::All;
	});

	W.Tick();
	TestEqual(TEXT("Both outside: both in, activated"), W.Listener->Take(), FString(TEXT("In:a,b|Activated:a,b")));

	A->SetActorLocation(Inside);
	W.Tick();
	TestEqual(TEXT("a inside: a out, deactivated"), W.Listener->Take(), FString(TEXT("Out:a|Deactivated:a")));
	TestEqual(TEXT("a inside: still gathered"), Trigger->GetGatheredActors().Num(), 2);

	A->SetActorLocation(Outside);
	W.Tick();
	TestEqual(TEXT("a outside again: a in, activated"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayDestroyedTest, "Destroyed")

bool FBangoTriggersPlayDestroyedTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Inside);
	ABangoTrigger* Trigger = W.SpawnTrigger([](ABangoTrigger& T)
	{
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
	});
	W.Tick();
	TestEqual(TEXT("Inside: in, activated"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));

	A->Destroy();
	W.Tick();
	TestEqual(TEXT("Destroyed: out as nil, deactivated"), W.Listener->Take(), FString(TEXT("Out:nil|Deactivated:nil")));
	TestEqual(TEXT("Destroyed: nobody actual"), Trigger->GetActualActivators().Num(), 0);
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayMaxActivationsTest, "MaxActivations")

bool FBangoTriggersPlayMaxActivationsTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Inside);
	ABangoTrigger* Trigger = W.SpawnTrigger([](ABangoTrigger& T)
	{
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
		T.MaxActivations = 1;
	});
	W.Tick();
	TestEqual(TEXT("First time: in, activated"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));

	A->SetActorLocation(Outside);
	W.Tick();
	TestEqual(TEXT("Leaving: out, deactivated"), W.Listener->Take(), FString(TEXT("Out:a|Deactivated:a")));
	TestEqual(TEXT("Then finished"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Finished);

	A->SetActorLocation(Inside);
	W.Tick();
	TestEqual(TEXT("Finished: nothing more"), W.Listener->Take(), FString());
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayScriptsTest, "Scripts")

bool FBangoTriggersPlayScriptsTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	ABangoTrigger* Trigger = W.SpawnTrigger([](ABangoTrigger& T)
	{
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
		T.Rule = EBangoTriggerRule::AtLeast;
		T.RuleCount = 2;
		T.ActivatorInScript.Source = TEXT("global int ins = 0;\nins = ins + Count(_activators);");
		T.ActivatedScript.Source = TEXT("global int activeCount = 0;\nactiveCount = Count(_actual);");
	});

	// Two arriving together join together.
	W.Spawn(TEXT("a"), Inside);
	W.Spawn(TEXT("b"), Inside + FVector(50, 0, 0));
	W.Tick(3);
	TestEqual(TEXT("Two at once: one In for both, then Activated"), W.Listener->Take(), FString(TEXT("In:a,b|Activated:a,b")));
	TestEqual(TEXT("On Activator In saw both in _activators"), W.GetGlobalInt(TEXT("ins")), 2);
	TestEqual(TEXT("On Activated saw both in _actual"), W.GetGlobalInt(TEXT("activeCount")), 2);
	TestTrue(TEXT("Active with 2"), Trigger->IsTriggerActive());
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayNoSourcesTest, "NoSources")

bool FBangoTriggersPlayNoSourcesTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	ABangoTrigger* Trigger = W.SpawnTrigger([](ABangoTrigger& T)
	{
		T.Condition.Source = TEXT("allowed");
	});

	W.Tick();
	TestFalse(TEXT("Global not set: not active"), Trigger->IsTriggerActive());

	W.SetGlobal(TEXT("allowed"), FBangoValue::MakeBool(true));
	W.Tick();
	TestEqual(TEXT("Global true: activated, with nobody"), W.Listener->Take(), FString(TEXT("Activated:")));

	W.SetGlobal(TEXT("allowed"), FBangoValue::MakeBool(false));
	W.Tick();
	TestEqual(TEXT("Global false: deactivated"), W.Listener->Take(), FString(TEXT("Deactivated:")));
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayTimeoutTest, "Timeout")

bool FBangoTriggersPlayTimeoutTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Outside);
	ABangoTrigger* Trigger = W.SpawnTrigger([](ABangoTrigger& T)
	{
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
		T.ActivationTimer.Mode = EBangoTriggerTimerMode::Timeout;
		T.ActivationTimer.MinTime = 0.5f;
		T.ActivationTimer.MaxTime = 0.5f;
	});
	W.Tick();

	// In for a moment only: the timer is cancelled.
	A->SetActorLocation(Inside);
	W.Tick(2);
	TestEqual(TEXT("Briefly in: activating"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Activating);
	A->SetActorLocation(Outside);
	W.Tick();
	TestEqual(TEXT("Briefly in: in, out, no activation"), W.Listener->Take(), FString(TEXT("In:a|Out:a")));
	TestEqual(TEXT("Briefly in: idle"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Idle);

	// In for long enough.
	A->SetActorLocation(Inside);
	W.Tick(8);
	TestEqual(TEXT("Staying in: in, then activated after the timer"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));
	TestTrue(TEXT("Staying in: active"), Trigger->IsTriggerActive());
	return true;
}

#undef BANGO_TRIGGER_PLAY_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
