#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BangoTriggerLogic.h"
#include "BangoTriggerRegion.h"
#include "BangoTriggerTypes.h"

// Run from the editor: Tools > Session Frontend > Automation, filter "Triggers.". Or BuildBangoTriggers.bat test.

namespace BangoTriggerTestsPrivate
{
	FString ToString(const TArray<int32>& Values)
	{
		TArray<FString> Parts;
		for (const int32 Value : Values)
		{
			Parts.Add(FString::FromInt(Value));
		}
		return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(", ")));
	}

	/** A trigger transform: location, yaw and scale. */
	FTransform Trigger(const FVector& Location = FVector::ZeroVector, double Yaw = 0.0, const FVector& Scale = FVector::OneVector)
	{
		return FTransform(FRotator(0.0, Yaw, 0.0), Location, Scale);
	}
}

using namespace BangoTriggerTestsPrivate;

// ---- Regions ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBangoTriggersAreaRegionTest, "Triggers.Regions.Area",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBangoTriggersAreaRegionTest::RunTest(const FString& Parameters)
{
	// The default: a polygon square, 400 wide, centred on the trigger.
	{
		const FBangoAreaRegion Square;
		TestEqual(TEXT("Default polygon has 4 points"), Square.Points.Num(), 4);
		TestTrue(TEXT("Square: centre inside"), Square.IsInside(Trigger(), FVector(0, 0, 0)));
		TestTrue(TEXT("Square: height doesn't matter"), Square.IsInside(Trigger(), FVector(199, 0, 5000)));
		TestTrue(TEXT("Square: below doesn't matter"), Square.IsInside(Trigger(), FVector(0, -199, -5000)));
		TestFalse(TEXT("Square: just outside"), Square.IsInside(Trigger(), FVector(201, 0, 0)));
	}

	// A polygon that crosses itself uses the even-odd rule: a bow tie is two triangles, left and right.
	{
		FBangoAreaRegion BowTie;
		BowTie.Points = { FVector2D(-100, -100), FVector2D(100, 100), FVector2D(100, -100), FVector2D(-100, 100) };
		TestTrue(TEXT("Bow tie: right triangle inside"), BowTie.IsInside(Trigger(), FVector(50, 0, 0)));
		TestTrue(TEXT("Bow tie: left triangle inside"), BowTie.IsInside(Trigger(), FVector(-50, 0, 0)));
		TestFalse(TEXT("Bow tie: top gap outside"), BowTie.IsInside(Trigger(), FVector(0, 50, 0)));
		TestFalse(TEXT("Bow tie: bottom gap outside"), BowTie.IsInside(Trigger(), FVector(0, -50, 0)));
	}

	// Fewer than 3 points has no inside.
	{
		FBangoAreaRegion Line;
		Line.Points = { FVector2D(-100, 0), FVector2D(100, 0) };
		TestFalse(TEXT("Two points: never inside"), Line.IsInside(Trigger(), FVector(0, 0, 0)));
	}

	// A rectangle with its own Offset and Yaw: 400 by 200, turned 90 degrees, at X 1000.
	{
		FBangoAreaRegion Rect;
		Rect.Shape = EBangoAreaShape::Rectangle;
		Rect.Size = FVector2D(400, 200);
		Rect.Offset = FVector2D(1000, 0);
		Rect.Yaw = 90.0f;
		TestTrue(TEXT("Turned rectangle: along its length (world Y)"), Rect.IsInside(Trigger(), FVector(1000, 150, 0)));
		TestFalse(TEXT("Turned rectangle: across its width (world X)"), Rect.IsInside(Trigger(), FVector(1150, 0, 0)));
		TestFalse(TEXT("Turned rectangle: not at the trigger"), Rect.IsInside(Trigger(), FVector(0, 0, 0)));
	}

	// The trigger's location, yaw and X/Y scale apply: a 100 by 100 rectangle, the trigger at X 500, turned 90, X scale 2.
	{
		FBangoAreaRegion Rect;
		Rect.Shape = EBangoAreaShape::Rectangle;
		Rect.Size = FVector2D(100, 100);
		const FTransform T = Trigger(FVector(500, 0, 0), 90.0, FVector(2, 1, 1));
		// The trigger's X (scaled to 200 wide) points along world Y; its Y (100 wide) along world -X.
		TestTrue(TEXT("Scaled, turned trigger: along its stretched X"), Rect.IsInside(T, FVector(500, 90, 0)));
		TestFalse(TEXT("Scaled, turned trigger: past its stretched X"), Rect.IsInside(T, FVector(500, 110, 0)));
		TestTrue(TEXT("Scaled, turned trigger: within its Y"), Rect.IsInside(T, FVector(540, 0, 0)));
		TestFalse(TEXT("Scaled, turned trigger: past its Y"), Rect.IsInside(T, FVector(560, 0, 0)));
	}

	// An ellipse: 400 by 200.
	{
		FBangoAreaRegion Ellipse;
		Ellipse.Shape = EBangoAreaShape::Ellipse;
		Ellipse.Size = FVector2D(400, 200);
		TestTrue(TEXT("Ellipse: near the X end"), Ellipse.IsInside(Trigger(), FVector(190, 0, 0)));
		TestFalse(TEXT("Ellipse: past the Y end"), Ellipse.IsInside(Trigger(), FVector(0, 110, 0)));
		TestTrue(TEXT("Ellipse: inside off the axes"), Ellipse.IsInside(Trigger(), FVector(150, 60, 0)));
		TestFalse(TEXT("Ellipse: the rectangle's corner is outside"), Ellipse.IsInside(Trigger(), FVector(190, 90, 0)));
	}

	// A flattened trigger has no inside.
	{
		const FBangoAreaRegion Square;
		TestFalse(TEXT("Flattened trigger: never inside"), Square.IsInside(Trigger(FVector::ZeroVector, 0.0, FVector(0, 1, 1)), FVector(0, 0, 0)));
	}

	// Local to world and back, with everything turned, moved and scaled.
	{
		FBangoAreaRegion Area;
		Area.Offset = FVector2D(30, -40);
		Area.Yaw = 25.0f;
		const FTransform T = Trigger(FVector(100, 200, 300), 70.0, FVector(1.5, 0.5, 2));
		const FVector2D Local(12, 34);
		const FVector World = Area.LocalToWorld(T, Local);
		const FVector2D Back = Area.WorldToLocal(T, World);
		TestTrue(FString::Printf(TEXT("Round trip: %s back as %s"), *Local.ToString(), *Back.ToString()), Back.Equals(Local, 0.001));
		TestEqual(TEXT("Round trip: at the trigger's height"), World.Z, 300.0);
		TestTrue(TEXT("Centre is the Offset's place"), Area.GetCentre(T).Equals(Area.LocalToWorld(T, FVector2D::ZeroVector)));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBangoTriggersVolumeRegionsTest, "Triggers.Regions.Volumes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBangoTriggersVolumeRegionsTest::RunTest(const FString& Parameters)
{
	// A box, 200 by 100 by 50, turned 90 degrees, 100 up.
	{
		FBangoBoxRegion Box;
		Box.Extent = FVector(100, 50, 25);
		Box.Rotation = FRotator(0, 90, 0);
		Box.Offset = FVector(0, 0, 100);
		TestTrue(TEXT("Box: along its X (world Y)"), Box.IsInside(Trigger(), FVector(0, 90, 100)));
		TestFalse(TEXT("Box: across its Y (world X)"), Box.IsInside(Trigger(), FVector(90, 0, 100)));
		TestFalse(TEXT("Box: above it"), Box.IsInside(Trigger(), FVector(0, 0, 130)));
		TestFalse(TEXT("Box: at the trigger, below it"), Box.IsInside(Trigger(), FVector(0, 0, 0)));
		TestTrue(TEXT("Box: centre handle is its centre"), Box.GetCentre(Trigger()).Equals(FVector(0, 0, 100)));
		const FBox Bounds = Box.GetBounds(Trigger());
		TestTrue(TEXT("Box: bounds hold a corner"), Bounds.ExpandBy(0.01).IsInside(FVector(50, 100, 125)));
	}

	// The trigger's scale stretches a box, even unevenly.
	{
		FBangoBoxRegion Box;
		const FTransform T = Trigger(FVector::ZeroVector, 0.0, FVector(2, 1, 1));
		TestTrue(TEXT("Stretched box: within its stretched X"), Box.IsInside(T, FVector(150, 0, 0)));
		TestFalse(TEXT("Stretched box: past its stretched X"), Box.IsInside(T, FVector(210, 0, 0)));
		TestFalse(TEXT("Stretched box: Y not stretched"), Box.IsInside(T, FVector(0, 150, 0)));
	}

	// A sphere stays a sphere: its radius is scaled by the trigger's smallest scale.
	{
		FBangoSphereRegion Sphere;
		Sphere.Radius = 100.0f;
		Sphere.Offset = FVector(100, 0, 0);
		const FTransform T = Trigger(FVector::ZeroVector, 0.0, FVector(3, 1, 2));
		TestTrue(TEXT("Sphere: centre is moved by the scale"), Sphere.GetCentre(T).Equals(FVector(300, 0, 0)));
		TestEqual(TEXT("Sphere: radius scaled by the smallest scale"), Sphere.GetWorldRadius(T), 100.0);
		TestTrue(TEXT("Sphere: inside"), Sphere.IsInside(T, FVector(390, 0, 0)));
		TestFalse(TEXT("Sphere: outside"), Sphere.IsInside(T, FVector(410, 0, 0)));
	}

	// A capsule: radius 50, half height 100 (so a straight part from -50 to 50), standing up.
	{
		FBangoCapsuleRegion Capsule;
		Capsule.Radius = 50.0f;
		Capsule.HalfHeight = 100.0f;
		TestTrue(TEXT("Capsule: near the top tip"), Capsule.IsInside(Trigger(), FVector(0, 0, 90)));
		TestFalse(TEXT("Capsule: past the top tip"), Capsule.IsInside(Trigger(), FVector(0, 0, 110)));
		TestTrue(TEXT("Capsule: within the radius"), Capsule.IsInside(Trigger(), FVector(40, 0, 0)));
		TestFalse(TEXT("Capsule: past the radius"), Capsule.IsInside(Trigger(), FVector(60, 0, 0)));
		TestTrue(TEXT("Capsule: inside the rounded end"), Capsule.IsInside(Trigger(), FVector(30, 0, 85)));
		TestFalse(TEXT("Capsule: the cylinder's corner is outside the rounded end"), Capsule.IsInside(Trigger(), FVector(40, 0, 90)));
	}

	// A capsule lying down along X (pitch 90 turns its Z axis onto X).
	{
		FBangoCapsuleRegion Capsule;
		Capsule.Radius = 50.0f;
		Capsule.HalfHeight = 100.0f;
		Capsule.Rotation = FRotator(90, 0, 0);
		TestTrue(TEXT("Lying capsule: along X"), Capsule.IsInside(Trigger(), FVector(90, 0, 0)));
		TestFalse(TEXT("Lying capsule: up Z"), Capsule.IsInside(Trigger(), FVector(0, 0, 90)));
	}

	return true;
}

// ---- Rule ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBangoTriggersRuleTest, "Triggers.Rule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBangoTriggersRuleTest::RunTest(const FString& Parameters)
{
	using namespace BangoTriggerLogic;

	auto Met = [](EBangoTriggerRule Rule, int32 Gathered, int32 Potential, int32 Actual, int32 Count = 1, int32 Min = 1, int32 Max = 1)
	{
		FRuleInput In;
		In.Rule = Rule;
		In.Count = Count;
		In.Min = Min;
		In.Max = Max;
		In.NumGathered = Gathered;
		In.NumPotential = Potential;
		In.NumActual = Actual;
		return IsRuleMet(In);
	};

	// No sources: only the Condition's answer.
	{
		FRuleInput In;
		In.bHasSources = false;
		In.bNoSourceMet = true;
		TestTrue(TEXT("No sources, Condition true: met"), IsRuleMet(In));
		In.bNoSourceMet = false;
		TestFalse(TEXT("No sources, Condition false: not met"), IsRuleMet(In));
	}

	// Never while no actor is potential, whatever the rule.
	TestFalse(TEXT("No potential: All"), Met(EBangoTriggerRule::All, 0, 0, 0));
	TestFalse(TEXT("No potential, gathered outside: All"), Met(EBangoTriggerRule::All, 2, 0, 0));
	TestFalse(TEXT("No potential: At Least 0"), Met(EBangoTriggerRule::AtLeast, 2, 0, 0, 0));
	TestFalse(TEXT("No potential: At Most 5"), Met(EBangoTriggerRule::AtMost, 2, 0, 0, 5));
	TestFalse(TEXT("No potential: Between 0 and 5"), Met(EBangoTriggerRule::Between, 2, 0, 0, 1, 0, 5));

	// All: every gathered actor is actual.
	TestTrue(TEXT("All: 3 of 3"), Met(EBangoTriggerRule::All, 3, 3, 3));
	TestFalse(TEXT("All: 2 of 3 actual"), Met(EBangoTriggerRule::All, 3, 3, 2));
	TestFalse(TEXT("All: one gathered isn't potential"), Met(EBangoTriggerRule::All, 3, 2, 2));

	// At Least.
	TestTrue(TEXT("At Least 2: 2"), Met(EBangoTriggerRule::AtLeast, 3, 3, 2, 2));
	TestFalse(TEXT("At Least 2: 1"), Met(EBangoTriggerRule::AtLeast, 3, 3, 1, 2));

	// At Most counts 0.
	TestTrue(TEXT("At Most 1: 0"), Met(EBangoTriggerRule::AtMost, 2, 2, 0, 1));
	TestTrue(TEXT("At Most 1: 1"), Met(EBangoTriggerRule::AtMost, 2, 2, 1, 1));
	TestFalse(TEXT("At Most 1: 2"), Met(EBangoTriggerRule::AtMost, 2, 2, 2, 1));

	// Between is inclusive.
	TestFalse(TEXT("Between 1 and 2: 0"), Met(EBangoTriggerRule::Between, 3, 3, 0, 1, 1, 2));
	TestTrue(TEXT("Between 1 and 2: 1"), Met(EBangoTriggerRule::Between, 3, 3, 1, 1, 1, 2));
	TestTrue(TEXT("Between 1 and 2: 2"), Met(EBangoTriggerRule::Between, 3, 3, 2, 1, 1, 2));
	TestFalse(TEXT("Between 1 and 2: 3"), Met(EBangoTriggerRule::Between, 3, 3, 3, 1, 1, 2));

	return true;
}

// ---- Actual set ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBangoTriggersActualSetTest, "Triggers.ActualSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBangoTriggersActualSetTest::RunTest(const FString& Parameters)
{
	using namespace BangoTriggerLogic;

	// Stand-ins for actors: only their addresses matter.
	int32 A = 0, B = 0, C = 0;
	const void* PA = &A;
	const void* PB = &B;
	const void* PC = &C;

	auto Expect = [this](const TCHAR* What, const TArray<int32>& Got, const TArray<int32>& Want)
	{
		TestTrue(FString::Printf(TEXT("%s: got %s, want %s"), What, *ToString(Got), *ToString(Want)), Got == Want);
	};

	// From nothing: the satisfied potential actors join, in potential order.
	{
		const FActualDiff Diff = DiffActual({}, { PA, PB, PC }, { true, false, true });
		Expect(TEXT("First: kept"), Diff.Kept, {});
		Expect(TEXT("First: left"), Diff.Left, {});
		Expect(TEXT("First: joined"), Diff.Joined, { 0, 2 });
	}

	// Some stay, some leave (Condition failed), some join.
	{
		const FActualDiff Diff = DiffActual({ PA, PC }, { PC, PB, PA }, { true, true, false });
		Expect(TEXT("Mixed: kept (C)"), Diff.Kept, { 1 });
		Expect(TEXT("Mixed: left (A, Condition failed)"), Diff.Left, { 0 });
		Expect(TEXT("Mixed: joined (B)"), Diff.Joined, { 1 });
	}

	// An actor that stopped being potential leaves.
	{
		const FActualDiff Diff = DiffActual({ PA, PB }, { PB }, { true });
		Expect(TEXT("No longer potential: kept"), Diff.Kept, { 1 });
		Expect(TEXT("No longer potential: left"), Diff.Left, { 0 });
		Expect(TEXT("No longer potential: joined"), Diff.Joined, {});
	}

	// A destroyed actor (null) leaves.
	{
		const FActualDiff Diff = DiffActual({ PA, nullptr }, { PA }, { true });
		Expect(TEXT("Destroyed: kept"), Diff.Kept, { 0 });
		Expect(TEXT("Destroyed: left"), Diff.Left, { 1 });
	}

	// Those staying keep the order they joined in, not the potential order.
	{
		const FActualDiff Diff = DiffActual({ PC, PA }, { PA, PC }, { true, true });
		Expect(TEXT("Order: kept"), Diff.Kept, { 0, 1 });
		Expect(TEXT("Order: joined"), Diff.Joined, {});
	}

	// Nobody potential: everyone leaves.
	{
		const FActualDiff Diff = DiffActual({ PA, PB }, {}, {});
		Expect(TEXT("Empty: left"), Diff.Left, { 0, 1 });
	}

	return true;
}

// ---- State ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBangoTriggersStepsTest, "Triggers.Steps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBangoTriggersStepsTest::RunTest(const FString& Parameters)
{
	using namespace BangoTriggerLogic;
	using EState = EBangoTriggerState;
	using EMode = EBangoTriggerTimerMode;

	auto Expect = [this](const TCHAR* What, EStep Got, EStep Want)
	{
		TestEqual(What, (int32)Got, (int32)Want);
	};

	Expect(TEXT("Undetermined, met: settle activated"), NextStep(EState::Undetermined, true, EMode::Timeout, EMode::Timeout), EStep::SettleActivated);
	Expect(TEXT("Undetermined, not met: settle deactivated"), NextStep(EState::Undetermined, false, EMode::Countdown, EMode::Countdown), EStep::SettleDeactivated);

	Expect(TEXT("Idle, met: start activating"), NextStep(EState::Idle, true, EMode::None, EMode::None), EStep::StartActivation);
	Expect(TEXT("Idle, not met: nothing"), NextStep(EState::Idle, false, EMode::None, EMode::None), EStep::None);

	Expect(TEXT("Activating, not met, Timeout: cancel"), NextStep(EState::Activating, false, EMode::Timeout, EMode::None), EStep::CancelActivation);
	Expect(TEXT("Activating, not met, Countdown: carry on"), NextStep(EState::Activating, false, EMode::Countdown, EMode::None), EStep::None);
	Expect(TEXT("Activating, met: carry on"), NextStep(EState::Activating, true, EMode::Timeout, EMode::None), EStep::None);

	Expect(TEXT("Active, not met: start deactivating"), NextStep(EState::Active, false, EMode::None, EMode::None), EStep::StartDeactivation);
	Expect(TEXT("Active, met: nothing (no activating again)"), NextStep(EState::Active, true, EMode::None, EMode::None), EStep::None);

	Expect(TEXT("Deactivating, met, Timeout: cancel"), NextStep(EState::Deactivating, true, EMode::None, EMode::Timeout), EStep::CancelDeactivation);
	Expect(TEXT("Deactivating, met, Countdown: carry on"), NextStep(EState::Deactivating, true, EMode::None, EMode::Countdown), EStep::None);
	Expect(TEXT("Deactivating, not met: carry on"), NextStep(EState::Deactivating, false, EMode::None, EMode::Timeout), EStep::None);

	Expect(TEXT("Finished, met: nothing"), NextStep(EState::Finished, true, EMode::None, EMode::None), EStep::None);
	Expect(TEXT("Finished, not met: nothing"), NextStep(EState::Finished, false, EMode::None, EMode::None), EStep::None);

	TestFalse(TEXT("No limit: never finished"), IsFinishedAfterDeactivation(0, 100));
	TestTrue(TEXT("Max 1, 1 activation: finished"), IsFinishedAfterDeactivation(1, 1));
	TestFalse(TEXT("Max 3, 2 activations: not yet"), IsFinishedAfterDeactivation(3, 2));
	TestTrue(TEXT("Max 3, 3 activations: finished"), IsFinishedAfterDeactivation(3, 3));

	return true;
}

// ---- Timers ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBangoTriggersTimerTest, "Triggers.Timer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBangoTriggersTimerTest::RunTest(const FString& Parameters)
{
	FBangoTriggerTimer Timer;
	Timer.Mode = EBangoTriggerTimerMode::Timeout;

	Timer.MinTime = 2.0f;
	Timer.MaxTime = 2.0f;
	TestEqual(TEXT("Equal times: that time"), Timer.PickTime(), 2.0f);

	Timer.MinTime = 1.0f;
	Timer.MaxTime = 3.0f;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		const float Time = Timer.PickTime();
		if (!TestTrue(FString::Printf(TEXT("Between 1 and 3: %f"), Time), Time >= 1.0f && Time <= 3.0f))
		{
			break;
		}
	}

	Timer.MinTime = 3.0f;
	Timer.MaxTime = 1.0f;
	const float Swapped = Timer.PickTime();
	TestTrue(FString::Printf(TEXT("Min above Max still picks between them: %f"), Swapped), Swapped >= 1.0f && Swapped <= 3.0f);

	Timer.MinTime = -5.0f;
	Timer.MaxTime = -1.0f;
	TestEqual(TEXT("Negative times count as 0"), Timer.PickTime(), 0.0f);

	TestFalse(TEXT("Mode None: not used"), FBangoTriggerTimer().IsUsed());
	TestTrue(TEXT("Mode Timeout: used"), Timer.IsUsed());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
