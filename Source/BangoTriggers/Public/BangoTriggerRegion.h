#pragma once

#include "CoreMinimal.h"
#include "BangoTriggerRegion.generated.h"

/** One line of a region's drawing, in world space. */
struct FBangoTriggerLine
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	/** Ground lines are drawn thicker than walls and other helper lines. */
	bool bMain = true;
};

/**
 * Part of where a trigger is: "where" (spec section 3). Placed relative to the trigger, so it moves, turns and scales
 * with it. A trigger's Regions list holds these as instanced structs; game modules add their own by subclassing.
 */
USTRUCT()
struct BANGOTRIGGERS_API FBangoTriggerRegion
{
	GENERATED_BODY()

	virtual ~FBangoTriggerRegion() = default;

	/** True if Location (a world location) is inside. TriggerTransform is the trigger's. */
	virtual bool IsInside(const FTransform& TriggerTransform, const FVector& Location) const { return false; }

	/** Its world-space bounds, for the editor. */
	virtual FBox GetBounds(const FTransform& TriggerTransform) const { return FBox(ForceInit); }

	/** Adds the lines it's drawn with in the editor, in world space. */
	virtual void AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const {}

	/** Where its centre handle is in the editor, in world space. */
	virtual FVector GetCentre(const FTransform& TriggerTransform) const { return TriggerTransform.GetLocation(); }
};

UENUM(BlueprintType)
enum class EBangoAreaShape : uint8
{
	Polygon,
	Rectangle,
	Ellipse,
};

/**
 * A 2D shape seen from above, with infinitely high walls: inside if a location's X and Y are inside the shape. Only the
 * trigger's location, yaw and X/Y scale apply to it (and its own Offset and Yaw).
 */
USTRUCT(meta = (DisplayName = "Area"))
struct BANGOTRIGGERS_API FBangoAreaRegion : public FBangoTriggerRegion
{
	GENERATED_BODY()

	FBangoAreaRegion();

	UPROPERTY(EditAnywhere, Category = "Region")
	EBangoAreaShape Shape = EBangoAreaShape::Polygon;

	/** The polygon's points in the region's local space (cm). The last joins back to the first. Needs at least 3. */
	UPROPERTY(EditAnywhere, Category = "Region", meta = (EditCondition = "Shape == EBangoAreaShape::Polygon", EditConditionHides))
	TArray<FVector2D> Points;

	/** The full width (X) and depth (Y) of the rectangle or ellipse, centred on the region's origin. */
	UPROPERTY(EditAnywhere, Category = "Region", meta = (EditCondition = "Shape != EBangoAreaShape::Polygon", EditConditionHides))
	FVector2D Size = FVector2D(400.0, 400.0);

	/** Where the region's origin is, in the trigger's local space (X and Y, cm). */
	UPROPERTY(EditAnywhere, Category = "Region")
	FVector2D Offset = FVector2D::ZeroVector;

	/** How the region is turned, on top of the trigger's yaw (degrees). */
	UPROPERTY(EditAnywhere, Category = "Region", meta = (Units = "deg"))
	float Yaw = 0.0f;

	/** How tall the walls are drawn in the editor. Only for drawing: the walls are infinitely high. */
	UPROPERTY(EditAnywhere, Category = "Region", meta = (ClampMin = "0", Units = "cm"))
	float DisplayHeight = 300.0f;

	virtual bool IsInside(const FTransform& TriggerTransform, const FVector& Location) const override;
	virtual FBox GetBounds(const FTransform& TriggerTransform) const override;
	virtual void AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const override;
	virtual FVector GetCentre(const FTransform& TriggerTransform) const override;

	/** A world location in the region's local 2D space. Far away if the trigger is flattened (X or Y scale 0). */
	FVector2D WorldToLocal(const FTransform& TriggerTransform, const FVector& Location) const;
	/** A region-local 2D point in the world, at the trigger's height. */
	FVector LocalToWorld(const FTransform& TriggerTransform, const FVector2D& Point) const;
	/** A region-local 2D offset in the trigger's local 2D space (turned by Yaw). */
	FVector2D LocalToTrigger(const FVector2D& Point) const;
	/** The region's yaw in the world: the trigger's yaw and its own. */
	double GetWorldYaw(const FTransform& TriggerTransform) const;

	/** The outline in region-local space: the polygon's points, the rectangle's corners, or points around the ellipse. */
	void GetLocalOutline(TArray<FVector2D>& OutPoints) const;
};

/** A box: inside if the location is in it. */
USTRUCT(meta = (DisplayName = "Box"))
struct BANGOTRIGGERS_API FBangoBoxRegion : public FBangoTriggerRegion
{
	GENERATED_BODY()

	/** The centre, in the trigger's local space (cm). */
	UPROPERTY(EditAnywhere, Category = "Region")
	FVector Offset = FVector::ZeroVector;

	/** How it's turned, relative to the trigger. */
	UPROPERTY(EditAnywhere, Category = "Region")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Half its size on each axis (cm). */
	UPROPERTY(EditAnywhere, Category = "Region", meta = (ClampMin = "0", Units = "cm"))
	FVector Extent = FVector(100.0);

	virtual bool IsInside(const FTransform& TriggerTransform, const FVector& Location) const override;
	virtual FBox GetBounds(const FTransform& TriggerTransform) const override;
	virtual void AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const override;
	virtual FVector GetCentre(const FTransform& TriggerTransform) const override;

	/** The box's own transform in the world (its rotation and centre, then the trigger's). */
	FTransform GetWorldTransform(const FTransform& TriggerTransform) const;
};

/** A sphere: inside if the location is within Radius of its centre. */
USTRUCT(meta = (DisplayName = "Sphere"))
struct BANGOTRIGGERS_API FBangoSphereRegion : public FBangoTriggerRegion
{
	GENERATED_BODY()

	/** The centre, in the trigger's local space (cm). */
	UPROPERTY(EditAnywhere, Category = "Region")
	FVector Offset = FVector::ZeroVector;

	/** Scaled by the trigger's smallest scale, so it stays a sphere. */
	UPROPERTY(EditAnywhere, Category = "Region", meta = (ClampMin = "0", Units = "cm"))
	float Radius = 100.0f;

	virtual bool IsInside(const FTransform& TriggerTransform, const FVector& Location) const override;
	virtual FBox GetBounds(const FTransform& TriggerTransform) const override;
	virtual void AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const override;
	virtual FVector GetCentre(const FTransform& TriggerTransform) const override;

	double GetWorldRadius(const FTransform& TriggerTransform) const;
};

/** A capsule: inside if the location is within Radius of its centre line. */
USTRUCT(meta = (DisplayName = "Capsule"))
struct BANGOTRIGGERS_API FBangoCapsuleRegion : public FBangoTriggerRegion
{
	GENERATED_BODY()

	/** The centre, in the trigger's local space (cm). */
	UPROPERTY(EditAnywhere, Category = "Region")
	FVector Offset = FVector::ZeroVector;

	/** How it's turned, relative to the trigger. Its length is along its Z axis. */
	UPROPERTY(EditAnywhere, Category = "Region")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Scaled by the trigger's smaller X/Y scale. */
	UPROPERTY(EditAnywhere, Category = "Region", meta = (ClampMin = "0", Units = "cm"))
	float Radius = 50.0f;

	/** From the centre to the tip of either end (cm), including the rounded end. Scaled by the trigger's Z scale. */
	UPROPERTY(EditAnywhere, Category = "Region", meta = (ClampMin = "0", Units = "cm"))
	float HalfHeight = 100.0f;

	virtual bool IsInside(const FTransform& TriggerTransform, const FVector& Location) const override;
	virtual FBox GetBounds(const FTransform& TriggerTransform) const override;
	virtual void AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const override;
	virtual FVector GetCentre(const FTransform& TriggerTransform) const override;

	/** The world centre, the unit axis along its length, its radius, and the half length of its straight part. */
	void GetWorldShape(const FTransform& TriggerTransform, FVector& OutCentre, FVector& OutAxis, double& OutRadius, double& OutHalfLine) const;
};
