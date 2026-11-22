#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class UBangoTriggerComponent;
struct FBangoTriggerRegion;
struct FBangoAreaRegion;

/** A clicked handle of one of the trigger's regions. */
struct HBangoRegionProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	enum class EKind : uint8
	{
		/** The region's centre: moves it (and turns a box, capsule or area with the rotate gizmo). */
		Centre,
		/** An area polygon's point. */
		Point,
		/** An area polygon's edge, from point Index to the next. */
		Edge,
		/** An area rectangle's or ellipse's size handle (4 side middles, then 4 corners). */
		Size,
		/** An area rectangle's or ellipse's rotate handle. */
		Rotate,
	};

	HBangoRegionProxy(const UActorComponent* InComponent, int32 InRegion, EKind InKind, int32 InIndex = INDEX_NONE)
		: HComponentVisProxy(InComponent, InKind == EKind::Edge ? HPP_Wireframe : HPP_Foreground)
		, Region(InRegion)
		, Kind(InKind)
		, Index(InIndex)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return Kind == EKind::Rotate ? EMouseCursor::GrabHand : (Kind == EKind::Edge ? EMouseCursor::Default : EMouseCursor::CardinalCross);
	}

	int32 Region;
	EKind Kind;
	int32 Index;
};

/**
 * Edits a Bango trigger component's regions in the level viewport (spec section 12). Select its actor to see every
 * region's handles:
 * - Every region: a centre handle; drag it with the gizmo to move the region (the rotate gizmo turns a box, capsule or
 *   area).
 * - Area polygons: click a point to move it (only X and Y); Delete removes it (3 stay). Right-click a point: Add Point
 *   After, Delete Point, Add Points By Clicking. Right-click an edge: Insert Point Here.
 * - Area rectangles and ellipses: size handles (the dragged side moves, the opposite one stays) and a rotate handle.
 * - Right-click a centre, size or rotate handle of a region that turns: Reset Rotation.
 */
class FBangoTriggerShapeVisualizer : public FComponentVisualizer
{
public:
	// FComponentVisualizer
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override;
	virtual UActorComponent* GetEditedComponent() const override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;

private:
	using EKind = HBangoRegionProxy::EKind;

	UBangoTriggerComponent* GetEditedTrigger() const;
	/** The selected region, if it's still there. */
	FBangoTriggerRegion* GetSelectedRegion() const;
	/** The selected region, if it's still there and is an area. */
	FBangoAreaRegion* GetSelectedArea() const;
	/** The selected area, if it's a polygon and the selected point (or edge) is still there. */
	FBangoAreaRegion* GetSelectedPolygon(bool bEdge = false) const;
	bool IsSizedArea(const FBangoAreaRegion* Area) const;
	bool HasSelection() const;
	void ClearSelection();

	/** Call after changing the trigger's regions: tells the editor, and redraws. Interactive while dragging. */
	void EndChange(UBangoTriggerComponent* Trigger, bool bInteractive) const;

	// Dragging
	void MoveCentre(UBangoTriggerComponent* Trigger, FBangoTriggerRegion& Region, const FVector& Delta);
	void TurnRegion(UBangoTriggerComponent* Trigger, FBangoTriggerRegion& Region, const FRotator& Delta);
	void MovePoint(UBangoTriggerComponent* Trigger, FBangoAreaRegion& Area, const FVector& Delta);
	void MoveSizeHandle(UBangoTriggerComponent* Trigger, FBangoAreaRegion& Area, const FVector& Delta);
	void MoveRotateHandle(UBangoTriggerComponent* Trigger, FBangoAreaRegion& Area, const FVector& Delta);

	// Menu actions
	void InsertPoint(int32 Index, const FVector& WorldLocation);
	void AddPointAfterSelected();
	void DeleteSelectedPoint();
	bool CanDeleteSelectedPoint() const;
	void InsertPointOnSelectedEdge();
	void StartAddingPoints();
	void StopAddingPoints();
	bool CanResetRotation() const;
	void ResetRotation();

	FComponentPropertyPath ShapePropertyPath;
	int32 SelectedRegion = INDEX_NONE;
	EKind SelectedKind = EKind::Centre;
	/** The selected point, edge or size handle. */
	int32 SelectedIndex = INDEX_NONE;
	/** Where on the selected edge it was clicked. */
	FVector EdgeClickLocation = FVector::ZeroVector;
	bool bAddingPoints = false;

	/** Something changed during the current drag. */
	bool bDragChanged = false;
	/** While dragging a rotate handle: where it would be without snapping, and the region's yaw and handle angle at the start. */
	bool bRotateDragging = false;
	FVector RotateDragHandle = FVector::ZeroVector;
	double RotateDragStartYaw = 0.0;
	double RotateDragStartAngle = 0.0;
};
