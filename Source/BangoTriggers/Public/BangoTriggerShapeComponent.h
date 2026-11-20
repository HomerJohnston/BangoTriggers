#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "BangoTriggerShapeComponent.generated.h"

class ABangoTrigger;
struct FBangoTriggerLine;

/**
 * Draws its ABangoTrigger's regions in the editor (hidden in game, no collision). The trigger holds the regions; the
 * editor's region handles belong to this component.
 */
UCLASS(ClassGroup = (Bango), HideCategories = (Collision, Physics, Lighting, Rendering, Mobile, Navigation, HLOD, Cooking, LOD, MaterialParameters, TextureStreaming))
class BANGOTRIGGERS_API UBangoTriggerShapeComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UBangoTriggerShapeComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	FLinearColor ShapeColor = FLinearColor(1.0f, 0.55f, 0.1f);

	ABangoTrigger* GetTrigger() const;

	/** Every region's lines, in world space. Empty if the owner isn't a trigger. */
	void GetLines(TArray<FBangoTriggerLine>& OutLines) const;

	/** Rebuilds the drawing and bounds. */
	void ShapeChanged();

	// UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
};
