#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "BangoTriggerShapeComponent.generated.h"

class UBangoTriggerComponent;
struct FBangoTriggerLine;

/**
 * Draws a Bango trigger component's regions in editor worlds. Made by the trigger component itself, on the same actor,
 * as a transient visualization component (like a scene component's editor sprite): it's never saved, and never made
 * in game. No collision.
 */
UCLASS(ClassGroup = (Bango), NotBlueprintable, HideCategories = (Collision, Physics, Lighting, Rendering, Mobile, Navigation, HLOD, Cooking, LOD, MaterialParameters, TextureStreaming))
class BANGOTRIGGERS_API UBangoTriggerShapeComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UBangoTriggerShapeComponent();

	void SetTrigger(const UBangoTriggerComponent* InTrigger) { Trigger = InTrigger; }
	const UBangoTriggerComponent* GetTrigger() const { return Trigger.Get(); }

	/** Every region's lines, in world space. Empty without a trigger. */
	void GetLines(TArray<FBangoTriggerLine>& OutLines) const;

	/** Rebuilds the drawing and bounds. */
	void ShapeChanged();

	// UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

private:
	TWeakObjectPtr<const UBangoTriggerComponent> Trigger;
};
