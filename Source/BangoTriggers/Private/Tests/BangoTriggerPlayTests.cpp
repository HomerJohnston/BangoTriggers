#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BangoGlobalsSubsystem.h"
#include "BangoIDActorComponent.h"
#include "BangoScriptSubsystem.h"
#include "BangoTrigger.h"
#include "BangoTriggerComponent.h"
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

		/** A standalone trigger at the origin, with no sources or regions and a Poll Interval of 0, set up by Setup before it begins play. */
		UBangoTriggerComponent* SpawnTrigger(TFunctionRef<void(UBangoTriggerComponent&)> Setup)
		{
			ABangoTrigger* Actor = World->SpawnActorDeferred<ABangoTrigger>(ABangoTrigger::StaticClass(), FTransform::Identity);
			UBangoTriggerComponent* Trigger = Actor->GetTrigger();
			Clear(*Trigger);
			Setup(*Trigger);
			Actor->FinishSpawning(FTransform::Identity);
			Listener->Listen(Trigger);
			return Trigger;
		}

		/** A trigger component added to Owner while playing (so it begins play at once), set up like SpawnTrigger's. */
		UBangoTriggerComponent* AddTrigger(AActor* Owner, TFunctionRef<void(UBangoTriggerComponent&)> Setup)
		{
			UBangoTriggerComponent* Trigger = NewObject<UBangoTriggerComponent>(Owner, TEXT("DoorTrigger"));
			Clear(*Trigger);
			Setup(*Trigger);
			Trigger->RegisterComponent();
			Listener->Listen(Trigger);
			return Trigger;
		}

		static void Clear(UBangoTriggerComponent& Trigger)
		{
			// A new trigger has no sources or regions already.
			Trigger.PollInterval = 0.0f;
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

		/** A global float's value, or -1 if it isn't set. */
		double GetGlobalFloat(const TCHAR* Name) const
		{
			const UBangoGlobalsSubsystem* Globals = UBangoGlobalsSubsystem::Get(World);
			const FBangoValue* Value = Globals ? Globals->Find(Name) : nullptr;
			return (Value && !Value->IsNil()) ? Value->GetDouble() : -1.0;
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

	/** A 200 by 200 area around the trigger's actor. */
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
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
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
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
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
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
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
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
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
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
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
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
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

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayScriptHandlesTest, "ScriptHandles")

bool FBangoTriggersPlayScriptHandlesTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Outside);
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
	{
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
		// Each stops the other (Abort(0) does nothing), then waits, so it's still running when the other starts.
		T.ActivatedScript.Source = TEXT("global int seenDeact = -1;\nseenDeact = _deactivatedScriptHandle;\nAbort(_deactivatedScriptHandle);\nSleep(100);");
		T.DeactivatedScript.Source = TEXT("global int seenAct = -1;\nseenAct = _activatedScriptHandle;\nAbort(_activatedScriptHandle);\nSleep(100);");
	});
	const UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(W.World);
	if (!TestNotNull(TEXT("Script subsystem"), Scripts))
	{
		return false;
	}

	A->SetActorLocation(Inside);
	W.Tick();
	const FBangoScriptHandle Activated = Trigger->GetActivatedScriptHandle();
	TestTrue(TEXT("On Activated started"), Activated.IsValid() && Scripts->IsRunning(Activated));
	TestEqual(TEXT("No On Deactivated yet: 0"), W.GetGlobalInt(TEXT("seenDeact")), 0);

	A->SetActorLocation(Outside);
	W.Tick();
	const FBangoScriptHandle Deactivated = Trigger->GetDeactivatedScriptHandle();
	TestTrue(TEXT("On Deactivated started"), Deactivated.IsValid() && Scripts->IsRunning(Deactivated));
	TestEqual(TEXT("On Deactivated got On Activated's id"), W.GetGlobalInt(TEXT("seenAct")), Activated.Id);
	TestFalse(TEXT("On Deactivated stopped On Activated"), Scripts->IsRunning(Activated));

	A->SetActorLocation(Inside);
	W.Tick();
	TestEqual(TEXT("On Activated got On Deactivated's id"), W.GetGlobalInt(TEXT("seenDeact")), Deactivated.Id);
	TestFalse(TEXT("On Activated stopped On Deactivated"), Scripts->IsRunning(Deactivated));
	TestTrue(TEXT("A new On Activated is running"), Scripts->IsRunning(Trigger->GetActivatedScriptHandle()));
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayOnActorTest, "OnActor")

bool FBangoTriggersPlayOnActorTest::RunTest(const FString& Parameters)
{
	// A trigger on a door: the door is a test actor too, and inside its own area, but is never gathered.
	FPlayWorld W;
	const FVector DoorAt(5000, 0, 0);
	ABangoTriggerTestActor* Door = W.Spawn(TEXT("door"), DoorAt);
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), DoorAt);
	UBangoTriggerComponent* Trigger = W.AddTrigger(Door, [](UBangoTriggerComponent& T)
	{
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
	});

	W.Tick();
	const TArray<AActor*> Gathered = Trigger->GetGatheredActors();
	TestTrue(TEXT("Its own actor isn't gathered"), Gathered.Num() == 1 && Gathered[0] == A);
	TestEqual(TEXT("A at the door"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));

	// The regions are placed from the actor: moving the door moves the area away from a.
	Door->SetActorLocation(FVector(8000, 0, 0));
	W.Tick();
	TestEqual(TEXT("The door moved away"), W.Listener->Take(), FString(TEXT("Out:a|Deactivated:a")));

	A->SetActorLocation(FVector(8050, 50, 0));
	W.Tick();
	TestEqual(TEXT("A followed it"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlaySelfAndTriggerTest, "SelfAndTrigger")

bool FBangoTriggersPlaySelfAndTriggerTest::RunTest(const FString& Parameters)
{
	// In a trigger component's scripts, self is its actor and _trigger is the component.
	FPlayWorld W;
	ABangoTriggerTestActor* Door = W.Spawn(TEXT("door"), Inside);
	W.Spawn(TEXT("a"), Inside);
	UBangoTriggerComponent* Trigger = W.AddTrigger(Door, [](UBangoTriggerComponent& T)
	{
		T.Sources.Add(IDs({ TEXT("a") }));
		T.Regions.Add(Box200());
		T.ActivatedScript.Source = TEXT("global int selfIsDoor = 0;\nif (self == @door) { selfIsDoor = 1; }\nglobal int count = -1;\ncount = _trigger call GetActivationCount();");
		// The Condition reads the component too.
		T.Condition.Source = TEXT("_trigger call IsTriggerEnabled()");
	});

	W.Tick(3);
	TestEqual(TEXT("Activated (the Condition read _trigger)"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));
	TestEqual(TEXT("self is the door"), W.GetGlobalInt(TEXT("selfIsDoor")), 1);
	TestEqual(TEXT("_trigger is the component"), W.GetGlobalInt(TEXT("count")), Trigger->GetActivationCount());
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayManualTest, "Manual")

bool FBangoTriggersPlayManualTest::RunTest(const FString& Parameters)
{
	// No sources and no Condition: a manual trigger. Nothing changes it but Force Activate and Force Deactivate.
	FPlayWorld W;
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
	{
		T.ActivatedScript.Source = TEXT("global int ons = 0;\nons = ons + 1;");
	});
	TestTrue(TEXT("It's manual"), Trigger->IsManualTrigger());

	W.Tick(20);
	TestEqual(TEXT("Left alone: nothing"), W.Listener->Take(), FString());
	TestEqual(TEXT("Left alone: idle"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Idle);
	TestFalse(TEXT("Its Rule is never met"), Trigger->IsRuleMet());

	Trigger->ForceActivate();
	W.Tick(20);
	TestEqual(TEXT("Forced on, and it stays on"), W.Listener->Take(), FString(TEXT("Activated:")));
	TestTrue(TEXT("Active"), Trigger->IsTriggerActive());
	TestEqual(TEXT("Its script ran once"), W.GetGlobalInt(TEXT("ons")), 1);

	Trigger->ForceDeactivate();
	W.Tick(20);
	TestEqual(TEXT("Forced off, and it stays off"), W.Listener->Take(), FString(TEXT("Deactivated:")));
	TestEqual(TEXT("Idle again"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Idle);
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayManualUndeterminedTest, "Manual.Undetermined")

bool FBangoTriggersPlayManualUndeterminedTest::RunTest(const FString& Parameters)
{
	// An Undetermined manual trigger settles once, as Deactivated (nothing is met), then waits to be forced.
	FPlayWorld W;
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
	{
		T.StartState = EBangoTriggerStartState::Undetermined;
	});
	W.Tick(1);
	TestEqual(TEXT("Settled off"), W.Listener->Take(), FString(TEXT("Deactivated(initial):")));
	W.Tick(20);
	TestEqual(TEXT("Then nothing"), W.Listener->Take(), FString());
	Trigger->ForceActivate();
	TestEqual(TEXT("Forced on"), W.Listener->Take(), FString(TEXT("Activated:")));
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayPersistentPerTriggerTest, "PersistentPerTrigger")

bool FBangoTriggersPlayPersistentPerTriggerTest::RunTest(const FString& Parameters)
{
	// Two triggers on one actor with the same script, ID and all (a component duplicated in a Blueprint): each keeps
	// its own persistent variables, since it's their holder, though both spawn them with the actor as the owner.
	FPlayWorld W;
	ABangoTriggerTestActor* Door = W.Spawn(TEXT("door"), Inside);
	const FGuid SharedId = FGuid::NewGuid();
	auto Setup = [SharedId](int32 Which)
	{
		return [SharedId, Which](UBangoTriggerComponent& T)
		{
			T.ActivatedScript.ScriptId = SharedId;
			T.ActivatedScript.Source = TEXT("persistent int _count ?= 0;\n_count = _count + 1;\nglobal int c1 = 0;\nglobal int c2 = 0;\n")
				TEXT("if (_which == 1) { c1 = _count; } else { c2 = _count; }");
			FBangoConstant WhichConstant;
			WhichConstant.Name = TEXT("_which");
			WhichConstant.Type = TEXT("int");
			WhichConstant.Value = FString::FromInt(Which);
			T.Constants.Add(WhichConstant);
		};
	};
	UBangoTriggerComponent* First = W.AddTrigger(Door, Setup(1));
	UBangoTriggerComponent* Second = NewObject<UBangoTriggerComponent>(Door, TEXT("DoorTrigger2"));
	W.Clear(*Second);
	Setup(2)(*Second);
	Second->RegisterComponent();

	// Manual triggers: forced on and off.
	First->ForceActivate();
	First->ForceDeactivate();
	First->ForceActivate();
	TestEqual(TEXT("The first counted its own runs"), W.GetGlobalInt(TEXT("c1")), 2);

	Second->ForceActivate();
	TestEqual(TEXT("The second starts from its own, not the first's"), W.GetGlobalInt(TEXT("c2")), 1);
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayNoSourcesTest, "NoSources")

bool FBangoTriggersPlayNoSourcesTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
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
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
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

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayStartUndeterminedOffTest, "StartUndetermined.Deactivated")

bool FBangoTriggersPlayStartUndeterminedOffTest::RunTest(const FString& Parameters)
{
	// Nobody there at the start: an Undetermined trigger still runs On Deactivated, so a light it switches goes off.
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Outside);
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
	{
		T.StartState = EBangoTriggerStartState::Undetermined;
		T.PollInterval = 5.0f;
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
		T.DeactivatedScript.Source = TEXT("global int initialOff = -1;\nif (_initial) { initialOff = 1; } else { initialOff = 0; }");
	});
	TestEqual(TEXT("Before its first check: undetermined"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Undetermined);
	TestFalse(TEXT("Undetermined isn't active"), Trigger->IsTriggerActive());

	// On the first frame, even with a long Poll Interval.
	W.Tick(1);
	TestEqual(TEXT("Settled deactivated, about nobody"), W.Listener->Take(), FString(TEXT("Deactivated(initial):")));
	TestEqual(TEXT("Idle"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Idle);
	TestEqual(TEXT("_initial was true"), W.GetGlobalInt(TEXT("initialOff")), 1);
	TestEqual(TEXT("Not an activation"), Trigger->GetActivationCount(), 0);

	// From then on, as usual.
	Trigger->PollInterval = 0.0f;
	A->SetActorLocation(Inside);
	W.Tick(60);
	A->SetActorLocation(Outside);
	W.Tick();
	TestEqual(TEXT("Then as usual"), W.Listener->Take(), FString(TEXT("In:a|Activated:a|Out:a|Deactivated:a")));
	TestEqual(TEXT("_initial false after"), W.GetGlobalInt(TEXT("initialOff")), 0);
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayStartUndeterminedOnTest, "StartUndetermined.Activated")

bool FBangoTriggersPlayStartUndeterminedOnTest::RunTest(const FString& Parameters)
{
	// Someone there at the start: In, then On Activated at once, skipping the Activation Timer.
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Inside);
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
	{
		T.StartState = EBangoTriggerStartState::Undetermined;
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
		T.ActivationTimer.Mode = EBangoTriggerTimerMode::Timeout;
		T.ActivationTimer.MinTime = 5.0f;
		T.ActivationTimer.MaxTime = 5.0f;
		T.ActivatedScript.Source = TEXT("global int initialOn = -1;\nif (_initial) { initialOn = 1; } else { initialOn = 0; }");
	});

	W.Tick(1);
	TestEqual(TEXT("Settled activated at once"), W.Listener->Take(), FString(TEXT("In:a|Activated(initial):a")));
	TestTrue(TEXT("Active"), Trigger->IsTriggerActive());
	TestEqual(TEXT("_initial was true"), W.GetGlobalInt(TEXT("initialOn")), 1);
	TestEqual(TEXT("It counts as an activation"), Trigger->GetActivationCount(), 1);

	// From then on, the timer applies.
	A->SetActorLocation(Outside);
	W.Tick();
	A->SetActorLocation(Inside);
	W.Tick();
	TestEqual(TEXT("Then as usual"), W.Listener->Take(), FString(TEXT("Out:a|Deactivated:a|In:a")));
	TestEqual(TEXT("The Activation Timer runs now"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Activating);
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayStartUndeterminedLaterTest, "StartUndetermined.DisabledAndReset")

bool FBangoTriggersPlayStartUndeterminedLaterTest::RunTest(const FString& Parameters)
{
	FPlayWorld W;
	W.Spawn(TEXT("a"), Inside);
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
	{
		T.StartState = EBangoTriggerStartState::Undetermined;
		T.bStartEnabled = false;
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
	});

	// Disabled: it stays undetermined until enabled, then settles at its next check.
	W.Tick();
	TestEqual(TEXT("Disabled: nothing"), W.Listener->Take(), FString());
	TestEqual(TEXT("Disabled: undetermined"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Undetermined);
	Trigger->SetTriggerEnabled(true);
	W.Tick(1);
	TestEqual(TEXT("Enabled: settled"), W.Listener->Take(), FString(TEXT("In:a|Activated(initial):a")));

	// Reset: back to undetermined, and it settles again.
	Trigger->ResetTrigger();
	TestEqual(TEXT("Reset: undetermined"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Undetermined);
	W.Tick(1);
	TestEqual(TEXT("Reset: settled again"), W.Listener->Take(), FString(TEXT("In:a|Activated(initial):a")));
	TestEqual(TEXT("Reset: one activation"), Trigger->GetActivationCount(), 1);
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayComponentActivationTest, "ComponentActivation")

bool FBangoTriggersPlayComponentActivationTest::RunTest(const FString& Parameters)
{
	// The engine's component activation (Activate, Deactivate) isn't the trigger's: it changes nothing.
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Outside);
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([](UBangoTriggerComponent& T)
	{
		T.Sources.Add(TestActors());
		T.Regions.Add(Box200());
	});
	W.Tick();

	Trigger->Activate(true);
	W.Tick();
	TestEqual(TEXT("Activate: nothing runs"), W.Listener->Take(), FString());
	TestEqual(TEXT("Activate: still idle"), (int32)Trigger->GetTriggerState(), (int32)EBangoTriggerState::Idle);

	A->SetActorLocation(Inside);
	W.Tick();
	Trigger->Deactivate();
	W.Tick();
	TestEqual(TEXT("Deactivate: only the trigger's own events"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));
	TestTrue(TEXT("Deactivate: still active"), Trigger->IsTriggerActive());
	return true;
}

namespace
{
	FBangoConstant MakeConstant(const TCHAR* Name, const TCHAR* Type, const TCHAR* Value)
	{
		FBangoConstant Constant;
		Constant.Name = Name;
		Constant.Type = Type;
		Constant.Value = Value;
		return Constant;
	}
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayConstantsTest, "Constants")

bool FBangoTriggersPlayConstantsTest::RunTest(const FString& Parameters)
{
	// The trigger's own constants, read by its scripts (after their other inputs) and its Condition (by name).
	FPlayWorld W;
	ABangoTriggerTestActor* A = W.Spawn(TEXT("a"), Inside);
	ABangoTriggerTestActor* Lamp = W.Spawn(TEXT("lamp"), FVector(9000, 0, 0));
	ABangoTriggerTestActor* Bulb = W.Spawn(TEXT("bulb"), FVector(9500, 0, 0));
	UBangoTriggerComponent* Trigger = W.SpawnTrigger([Lamp, Bulb](UBangoTriggerComponent& T)
	{
		T.Sources.Add(IDs({ TEXT("a") }));
		T.Regions.Add(Box200());
		T.Constants.Add(MakeConstant(TEXT("_red"), TEXT("vector3"), TEXT("[1, 0, 0]")));
		T.Constants.Add(MakeConstant(TEXT("_need"), TEXT("int"), TEXT("3")));
		// An entity is a picked actor, and an entity[] a list of them (a gap is a nil element); their Value isn't used.
		FBangoConstant LampConstant = MakeConstant(TEXT("_lamp"), TEXT("entity"), TEXT("ignored"));
		LampConstant.Actor = Lamp;
		T.Constants.Add(LampConstant);
		FBangoConstant LampsConstant = MakeConstant(TEXT("_lamps"), TEXT("entity[]"), TEXT(""));
		LampsConstant.Actors = { Bulb, nullptr };
		T.Constants.Add(LampsConstant);
		// The Condition is only true while the global reaches the constant.
		T.Condition.Source = TEXT("have >= _need");
		T.ActivatorInScript.Source = TEXT("global float inRed = 0;\ninRed = _red.x;");
		T.ActivatedScript.Source = TEXT("global int lampOk = 0;\nif (_lamp == @lamp && _lamps[0] == @bulb && IsNil(_lamps[1]) && !_initial) { lampOk = 1; }");
		T.DeactivatedScript.Source = TEXT("global float outRed = 0;\noutRed = _red.x + _need;");
	});
	W.SetGlobal(TEXT("have"), FBangoValue::MakeInt(0));

	W.Tick();
	TestEqual(TEXT("Condition below the constant: nothing"), W.Listener->Take(), FString());
	W.SetGlobal(TEXT("have"), FBangoValue::MakeInt(3));
	W.Tick();
	TestEqual(TEXT("Condition reaches the constant"), W.Listener->Take(), FString(TEXT("In:a|Activated:a")));
	TestEqual(TEXT("On Activator In read a vector3"), W.GetGlobalFloat(TEXT("inRed")), 1.0);
	TestEqual(TEXT("On Activated read a picked actor and picked actors (after _initial)"), W.GetGlobalInt(TEXT("lampOk")), 1);

	A->SetActorLocation(Outside);
	W.Tick();
	TestEqual(TEXT("Out"), W.Listener->Take(), FString(TEXT("Out:a|Deactivated:a")));
	TestEqual(TEXT("On Deactivated read two"), W.GetGlobalFloat(TEXT("outRed")), 4.0);
	return true;
}

BANGO_TRIGGER_PLAY_TEST(FBangoTriggersPlayConstantsWrongTest, "Constants.Wrong")

bool FBangoTriggersPlayConstantsWrongTest::RunTest(const FString& Parameters)
{
	// A constant can't take a built-in input's name, or another constant's.
	UBangoTriggerComponent* Trigger = NewObject<UBangoTriggerComponent>();
	Trigger->Constants.Add(MakeConstant(TEXT("_count"), TEXT("int"), TEXT("0")));
	FString Error;
	TestTrue(TEXT("A good name"), Trigger->UpdateConstants(&Error));

	Trigger->Constants.Add(MakeConstant(TEXT("_initial"), TEXT("bool"), TEXT("false")));
	TestFalse(TEXT("A built-in input's name"), Trigger->UpdateConstants(&Error));
	TestTrue(TEXT("It says which"), Error.Contains(TEXT("_initial")));

	Trigger->Constants.RemoveAt(1);
	Trigger->Constants.Add(MakeConstant(TEXT("_count"), TEXT("int"), TEXT("1")));
	TestFalse(TEXT("Another constant's name"), Trigger->UpdateConstants(&Error));

	Trigger->Constants.RemoveAt(1);
	TestTrue(TEXT("Fixed again"), Trigger->UpdateConstants(&Error));
	return true;
}

#undef BANGO_TRIGGER_PLAY_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
