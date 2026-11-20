#include "BangoTriggerShapeComponent.h"

#include "BangoTrigger.h"
#include "BangoTriggerRegion.h"
#include "Engine/CollisionProfile.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

namespace
{
	/** Draws world-space lines, made on the game thread when the proxy is created. */
	class FBangoTriggerShapeSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		FBangoTriggerShapeSceneProxy(const UBangoTriggerShapeComponent* Component)
			: FPrimitiveSceneProxy(Component)
			, Color(Component->ShapeColor)
		{
			Component->GetLines(Lines);
			bWillEverBeLit = false;
		}

		virtual SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
			uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			const FLinearColor MainColor = IsSelected() ? Color : Color * 0.6f;
			const FLinearColor OtherColor = MainColor * 0.7f;
			const float MainThickness = IsSelected() ? 3.0f : 2.0f;

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				if (!(VisibilityMap & (1 << ViewIndex)))
				{
					continue;
				}
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				for (const FBangoTriggerLine& Line : Lines)
				{
					PDI->DrawLine(Line.Start, Line.End, Line.bMain ? MainColor : OtherColor, SDPG_World, Line.bMain ? MainThickness : 1.0f);
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = false;
			Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Result;
		}

		virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
		uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize(); }

	private:
		TArray<FBangoTriggerLine> Lines;
		FLinearColor Color;
	};
}

UBangoTriggerShapeComponent::UBangoTriggerShapeComponent()
{
	SetHiddenInGame(true);
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
	bUseEditorCompositing = true;
	bWantsOnUpdateTransform = true;
	SetCanEverAffectNavigation(false);
}

ABangoTrigger* UBangoTriggerShapeComponent::GetTrigger() const
{
	return Cast<ABangoTrigger>(GetOwner());
}

void UBangoTriggerShapeComponent::GetLines(TArray<FBangoTriggerLine>& OutLines) const
{
	OutLines.Reset();
	if (const ABangoTrigger* Trigger = GetTrigger())
	{
		const FTransform& Transform = Trigger->GetActorTransform();
		for (const TInstancedStruct<FBangoTriggerRegion>& Region : Trigger->Regions)
		{
			if (const FBangoTriggerRegion* Ptr = Region.GetPtr())
			{
				Ptr->AddDrawLines(Transform, OutLines);
			}
		}
	}
}

void UBangoTriggerShapeComponent::ShapeChanged()
{
	UpdateBounds();
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UBangoTriggerShapeComponent::CreateSceneProxy()
{
	return new FBangoTriggerShapeSceneProxy(this);
}

FBoxSphereBounds UBangoTriggerShapeComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// The regions are placed from the trigger's own transform, so LocalToWorld isn't used.
	FBox Box(ForceInit);
	if (const ABangoTrigger* Trigger = GetTrigger())
	{
		const FTransform& Transform = Trigger->GetActorTransform();
		for (const TInstancedStruct<FBangoTriggerRegion>& Region : Trigger->Regions)
		{
			if (const FBangoTriggerRegion* Ptr = Region.GetPtr())
			{
				Box += Ptr->GetBounds(Transform);
			}
		}
	}
	if (!Box.IsValid)
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0);
	}
	return FBoxSphereBounds(Box);
}

void UBangoTriggerShapeComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	// The drawing is in world space, so it's rebuilt when the trigger moves.
	MarkRenderStateDirty();
}
