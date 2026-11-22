#include "BangoTriggerDebugDraw.h"

#if !UE_BUILD_SHIPPING

#include "BangoTriggerComponent.h"
#include "BangoTriggerRegion.h"
#include "CanvasItem.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "BangoTriggerDebugDraw"

namespace
{
	/** Show > Developer > Bango Triggers. Off by default; never in shipping. */
	TCustomShowFlag<EShowFlagShippingValue::ForceDisabled> ShowBangoTriggers(TEXT("BangoTriggers"), false, SFG_Developer,
		LOCTEXT("ShowFlagBangoTriggers", "Bango Triggers"));

	FDelegateHandle DrawHandle;

	const FLinearColor ActualColor(0.1f, 0.9f, 0.2f);
	const FLinearColor UnmetColor(0.95f, 0.15f, 0.1f);
	const FLinearColor ElsewhereColor(0.5f, 0.5f, 0.5f, 0.5f);

	FLinearColor GetStateColor(EBangoTriggerState State)
	{
		switch (State)
		{
		case EBangoTriggerState::Undetermined: return FLinearColor(0.95f, 0.75f, 0.05f);
		case EBangoTriggerState::Activating:   return FLinearColor(1.0f, 0.45f, 0.05f);
		case EBangoTriggerState::Active:       return ActualColor;
		case EBangoTriggerState::Deactivating: return FLinearColor(0.2f, 0.55f, 1.0f);
		case EBangoTriggerState::Finished:     return FLinearColor(0.25f, 0.25f, 0.25f);
		case EBangoTriggerState::Idle:
		default:                               return FLinearColor(0.7f, 0.7f, 0.7f);
		}
	}

	FString GetStateName(EBangoTriggerState State)
	{
		return StaticEnum<EBangoTriggerState>()->GetDisplayNameTextByValue((int64)State).ToString();
	}

	FString GetRuleText(const UBangoTriggerComponent& Trigger)
	{
		switch (Trigger.Rule)
		{
		case EBangoTriggerRule::All:     return TEXT("All");
		case EBangoTriggerRule::AtLeast: return FString::Printf(TEXT("At Least %d"), Trigger.RuleCount);
		case EBangoTriggerRule::AtMost:  return FString::Printf(TEXT("At Most %d"), Trigger.RuleCount);
		case EBangoTriggerRule::Between: return FString::Printf(TEXT("Between %d and %d"), Trigger.RuleMin, Trigger.RuleMax);
		default:                         return FString();
		}
	}

	/** Draws on one view's canvas. */
	struct FPainter
	{
		UCanvas& Canvas;
		const FSceneView& View;

		/** Cuts a line where it passes behind the camera; false if it's all behind. */
		bool ClipToView(FVector& A, FVector& B) const
		{
			const FVector Origin = View.ViewMatrices.GetViewOrigin();
			const FVector Forward = View.GetViewDirection();
			constexpr double Near = 10.0;
			const double DistA = FVector::DotProduct(A - Origin, Forward) - Near;
			const double DistB = FVector::DotProduct(B - Origin, Forward) - Near;
			if (DistA < 0.0 && DistB < 0.0)
			{
				return false;
			}
			if (DistA < 0.0)
			{
				A = A + (B - A) * (DistA / (DistA - DistB));
			}
			else if (DistB < 0.0)
			{
				B = B + (A - B) * (DistB / (DistB - DistA));
			}
			return true;
		}

		bool IsInFront(const FVector& Location) const
		{
			return FVector::DotProduct(Location - View.ViewMatrices.GetViewOrigin(), View.GetViewDirection()) > 10.0;
		}

		void Line(FVector A, FVector B, const FLinearColor& Color, float Thickness) const
		{
			if (!ClipToView(A, B))
			{
				return;
			}
			const FVector ScreenA = Canvas.Project(A, false);
			const FVector ScreenB = Canvas.Project(B, false);
			FCanvasLineItem Item(FVector2D(ScreenA.X, ScreenA.Y), FVector2D(ScreenB.X, ScreenB.Y));
			Item.SetColor(Color);
			Item.LineThickness = Thickness;
			Canvas.DrawItem(Item);
		}

		/** Lines of text, centred above Location. */
		void Label(const FVector& Location, const TArray<FString>& Lines, const FLinearColor& Color) const
		{
			if (!IsInFront(Location) || !GEngine)
			{
				return;
			}
			const UFont* Font = GEngine->GetSmallFont();
			const float LineHeight = Font ? Font->GetMaxCharHeight() + 1.0f : 12.0f;
			const FVector Screen = Canvas.Project(Location, false);
			float Y = Screen.Y - LineHeight * Lines.Num();
			for (const FString& Text : Lines)
			{
				FCanvasTextItem Item(FVector2D(Screen.X, Y), FText::FromString(Text), Font, Color);
				Item.bCentreX = true;
				Item.EnableShadow(FLinearColor::Black);
				Canvas.DrawItem(Item);
				Y += LineHeight;
			}
		}
	};

	void DrawTrigger(const FPainter& Painter, const UBangoTriggerComponent& Trigger, bool bPlaying)
	{
		const FTransform Anchor = Trigger.GetAnchorTransform();

		// The label goes over the top of its regions (or of its actor, with none).
		FBox Bounds(ForceInit);
		for (const TInstancedStruct<FBangoTriggerRegion>& Region : Trigger.Regions)
		{
			if (const FBangoTriggerRegion* Ptr = Region.GetPtr())
			{
				Bounds += Ptr->GetBounds(Anchor);
			}
		}
		const FVector LabelAt = Bounds.IsValid
			? FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z + 20.0)
			: Anchor.GetLocation() + FVector(0.0, 0.0, 120.0);

		TArray<FString> Lines;
		Lines.Add(Trigger.GetTriggerName());

		// In the editor the regions are drawn already, and nothing has happened yet: what it'll start as.
		if (!bPlaying)
		{
			const FString StartState = StaticEnum<EBangoTriggerStartState>()->GetDisplayNameTextByValue((int64)Trigger.StartState).ToString();
			Lines.Add(FString::Printf(TEXT("Starts %s"), *StartState));
			Painter.Label(LabelAt, Lines, FLinearColor::White);
			return;
		}

		const EBangoTriggerState State = Trigger.GetTriggerState();
		const bool bLive = Trigger.IsRunningHere() && Trigger.IsTriggerEnabled();
		FLinearColor Color = GetStateColor(State);
		if (!bLive)
		{
			Color.A = 0.35f;
		}

		// Regions, in the state's colour.
		TArray<FBangoTriggerLine> RegionLines;
		for (const TInstancedStruct<FBangoTriggerRegion>& Region : Trigger.Regions)
		{
			if (const FBangoTriggerRegion* Ptr = Region.GetPtr())
			{
				Ptr->AddDrawLines(Anchor, RegionLines);
			}
		}
		for (const FBangoTriggerLine& RegionLine : RegionLines)
		{
			Painter.Line(RegionLine.Start, RegionLine.End, Color, RegionLine.bMain ? 2.0f : 1.0f);
		}

		if (!Trigger.IsRunningHere())
		{
			Lines.Add(TEXT("Not running here"));
			Painter.Label(LabelAt, Lines, Color);
			return;
		}

		// A line to each actor it has gathered: actual, potential (Condition false), or not in the right place.
		const FVector From = Anchor.GetLocation();
		const TArray<AActor*> Actual = Trigger.GetActualActivators();
		const TArray<AActor*> Potential = Trigger.GetPotentialActivators();
		const TArray<AActor*> Gathered = Trigger.GetGatheredActors();
		for (const AActor* Actor : Gathered)
		{
			if (!Actor)
			{
				continue;
			}
			const bool bActual = Actual.Contains(Actor);
			const bool bPotential = bActual || Potential.Contains(Actor);
			const FLinearColor ActorColor = bActual ? ActualColor : bPotential ? UnmetColor : ElsewhereColor;
			Painter.Line(From, Actor->GetActorLocation(), ActorColor, bActual ? 2.0f : 1.0f);
		}

		// Name, state; Rule; timer; activations.
		FString StateLine = GetStateName(State);
		if (!Trigger.IsTriggerEnabled())
		{
			StateLine += TEXT(" (disabled)");
		}
		Lines.Add(StateLine);

		const TCHAR* Met = Trigger.IsRuleMet() ? TEXT("met") : TEXT("not met");
		if (Trigger.IsManualTrigger())
		{
			Lines.Add(TEXT("Manual: forced only"));
		}
		else if (Trigger.Sources.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("Condition only: %s"), Met));
		}
		else
		{
			Lines.Add(FString::Printf(TEXT("%d actual, %d potential, %d gathered"), Actual.Num(), Potential.Num(), Gathered.Num()));
			Lines.Add(FString::Printf(TEXT("%s: %s"), *GetRuleText(Trigger), Met));
		}

		const double TimeLeft = Trigger.GetTimerTimeLeft();
		if (TimeLeft >= 0.0)
		{
			const TCHAR* What = State == EBangoTriggerState::Activating ? TEXT("Activates") : TEXT("Deactivates");
			Lines.Add(FString::Printf(TEXT("%s in %.1fs%s"), What, TimeLeft, Trigger.IsTriggerEnabled() ? TEXT("") : TEXT(" (paused)")));
		}

		Lines.Add(Trigger.MaxActivations > 0
			? FString::Printf(TEXT("Activations %d / %d"), Trigger.GetActivationCount(), Trigger.MaxActivations)
			: FString::Printf(TEXT("Activations %d"), Trigger.GetActivationCount()));

		Painter.Label(LabelAt, Lines, Color);
	}

	void Draw(UCanvas* Canvas, APlayerController* PlayerController)
	{
		if (!Canvas || !Canvas->SceneView || !Canvas->SceneView->Family || !Canvas->SceneView->Family->Scene)
		{
			return;
		}
		const UWorld* World = Canvas->SceneView->Family->Scene->GetWorld();
		if (!World)
		{
			return;
		}
		const bool bPlaying = World->IsGameWorld();
		const FPainter Painter{ *Canvas, *Canvas->SceneView };
		// Only while the flag is on, so finding them each frame is fine.
		for (TObjectIterator<UBangoTriggerComponent> It; It; ++It)
		{
			const UBangoTriggerComponent* Trigger = *It;
			if (Trigger->GetWorld() == World && Trigger->IsRegistered() && !Trigger->IsTemplate())
			{
				DrawTrigger(Painter, *Trigger, bPlaying);
			}
		}
	}
}

void BangoTriggerDebugDraw::Register()
{
	DrawHandle = UDebugDrawService::Register(TEXT("BangoTriggers"), FDebugDrawDelegate::CreateStatic(&Draw));
}

void BangoTriggerDebugDraw::Unregister()
{
	UDebugDrawService::Unregister(DrawHandle);
	DrawHandle.Reset();
}

#undef LOCTEXT_NAMESPACE

#else

void BangoTriggerDebugDraw::Register() {}
void BangoTriggerDebugDraw::Unregister() {}

#endif
