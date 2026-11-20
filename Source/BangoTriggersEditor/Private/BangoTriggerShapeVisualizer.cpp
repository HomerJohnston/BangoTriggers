#include "BangoTriggerShapeVisualizer.h"

#include "BangoTrigger.h"
#include "BangoTriggerRegion.h"
#include "BangoTriggerShapeComponent.h"
#include "CanvasTypes.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SceneManagement.h"
#include "ScopedTransaction.h"
#include "Settings/LevelEditorViewportSettings.h"

IMPLEMENT_HIT_PROXY(HBangoRegionProxy, HComponentVisProxy);

#define LOCTEXT_NAMESPACE "BangoTriggerShapeVisualizer"

namespace
{
	const FLinearColor HandleColor(1.0f, 0.55f, 0.1f);
	const FLinearColor CentreColor(1.0f, 1.0f, 1.0f);
	const FLinearColor RotateColor(0.3f, 0.8f, 1.0f);
	const FLinearColor SelectedColor(1.0f, 1.0f, 0.2f);
	const FLinearColor EdgeColor(1.0f, 0.55f, 0.1f, 0.8f);
	constexpr float PointSize = 14.0f;
	constexpr float SmallSize = 12.0f;
	constexpr float SelectedSize = 18.0f;
	constexpr float EdgeThickness = 3.0f;

	/** How far beyond the +X side an area's rotate handle is, and its ring's radius (cm). */
	constexpr double RotateHandleDistance = 100.0;
	constexpr double RotateHandleRadius = 20.0;
	/** A rectangle or ellipse can't be made smaller than this (cm, in the region's local space). */
	constexpr double MinShapeSize = 1.0;

	/** An area's size handles, as directions from its centre in its local space: 4 side middles, then 4 corners. */
	const FVector2D SizeHandleDirs[] = {
		FVector2D(1.0, 0.0), FVector2D(-1.0, 0.0), FVector2D(0.0, 1.0), FVector2D(0.0, -1.0),
		FVector2D(1.0, 1.0), FVector2D(-1.0, 1.0), FVector2D(-1.0, -1.0), FVector2D(1.0, -1.0),
	};
	constexpr int32 NumSizeHandles = UE_ARRAY_COUNT(SizeHandleDirs);

	ABangoTrigger* GetTrigger(const UActorComponent* Component)
	{
		const UBangoTriggerShapeComponent* Shape = Cast<UBangoTriggerShapeComponent>(Component);
		return Shape ? Shape->GetTrigger() : nullptr;
	}

	FProperty* GetRegionsProperty()
	{
		static FProperty* Property = FindFProperty<FProperty>(ABangoTrigger::StaticClass(), GET_MEMBER_NAME_CHECKED(ABangoTrigger, Regions));
		return Property;
	}

	FVector SizeHandleLocation(const FBangoAreaRegion& Area, const FTransform& Transform, int32 Index)
	{
		return Area.LocalToWorld(Transform, SizeHandleDirs[Index] * Area.Size * 0.5);
	}

	FVector RotateHandleLocation(const FBangoAreaRegion& Area, const FTransform& Transform)
	{
		const FVector Centre = Area.GetCentre(Transform);
		const FVector Side = Area.LocalToWorld(Transform, FVector2D(Area.Size.X * 0.5, 0.0));
		FVector Dir = (Side - Centre).GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			Dir = FRotator(0.0, Area.GetWorldYaw(Transform), 0.0).Vector();
		}
		return Side + Dir * RotateHandleDistance;
	}

	double AngleOf(const FVector& Offset)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
	}
}

// ---- Region type checks ----
// Regions are instanced structs: their real type is known from the trigger's list, so these look it up there.

namespace
{
	template<typename T>
	T* GetRegionAs(ABangoTrigger* Trigger, int32 Index)
	{
		return (Trigger && Trigger->Regions.IsValidIndex(Index)) ? Trigger->Regions[Index].GetMutablePtr<T>() : nullptr;
	}

	template<typename T>
	const T* GetRegionAs(const ABangoTrigger* Trigger, int32 Index)
	{
		return (Trigger && Trigger->Regions.IsValidIndex(Index)) ? Trigger->Regions[Index].GetPtr<T>() : nullptr;
	}

	/** The region's own rotation as a quaternion relative to the trigger, for regions that have one. */
	bool GetRelativeRotation(const ABangoTrigger* Trigger, int32 Index, FQuat& OutRotation)
	{
		if (const FBangoAreaRegion* Area = GetRegionAs<FBangoAreaRegion>(Trigger, Index))
		{
			OutRotation = FRotator(0.0, Area->Yaw, 0.0).Quaternion();
			return true;
		}
		if (const FBangoBoxRegion* Box = GetRegionAs<FBangoBoxRegion>(Trigger, Index))
		{
			OutRotation = Box->Rotation.Quaternion();
			return true;
		}
		if (const FBangoCapsuleRegion* Capsule = GetRegionAs<FBangoCapsuleRegion>(Trigger, Index))
		{
			OutRotation = Capsule->Rotation.Quaternion();
			return true;
		}
		return false;
	}
}

// ---- Drawing ----

void FBangoTriggerShapeVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const ABangoTrigger* Trigger = GetTrigger(Component);
	if (!Trigger)
	{
		return;
	}
	const FTransform& Transform = Trigger->GetActorTransform();
	const bool bEditingThis = GetEditedComponent() == Component;

	for (int32 RegionIndex = 0; RegionIndex < Trigger->Regions.Num(); ++RegionIndex)
	{
		const FBangoTriggerRegion* Region = Trigger->Regions[RegionIndex].GetPtr();
		if (!Region)
		{
			continue;
		}
		const bool bThisRegion = bEditingThis && RegionIndex == SelectedRegion;
		auto IsSelected = [&](EKind Kind, int32 Index = INDEX_NONE)
		{
			return bThisRegion && SelectedKind == Kind && (Index == INDEX_NONE || Index == SelectedIndex);
		};

		if (const FBangoAreaRegion* Area = GetRegionAs<FBangoAreaRegion>(Trigger, RegionIndex))
		{
			if (Area->Shape == EBangoAreaShape::Polygon)
			{
				const int32 Num = Area->Points.Num();
				// Edges first, so points are on top and easier to click.
				for (int32 Index = 0; Index < Num && Num >= 2; ++Index)
				{
					const FVector A = Area->LocalToWorld(Transform, Area->Points[Index]);
					const FVector B = Area->LocalToWorld(Transform, Area->Points[(Index + 1) % Num]);
					PDI->SetHitProxy(new HBangoRegionProxy(Component, RegionIndex, EKind::Edge, Index));
					PDI->DrawLine(A, B, IsSelected(EKind::Edge, Index) ? SelectedColor : EdgeColor, SDPG_Foreground, EdgeThickness);
					PDI->SetHitProxy(nullptr);
				}
				for (int32 Index = 0; Index < Num; ++Index)
				{
					const bool bSelected = IsSelected(EKind::Point, Index);
					PDI->SetHitProxy(new HBangoRegionProxy(Component, RegionIndex, EKind::Point, Index));
					PDI->DrawPoint(Area->LocalToWorld(Transform, Area->Points[Index]), bSelected ? SelectedColor : HandleColor,
						bSelected ? SelectedSize : PointSize, SDPG_Foreground);
					PDI->SetHitProxy(nullptr);
				}
				if (IsSelected(EKind::Edge))
				{
					PDI->DrawPoint(EdgeClickLocation, SelectedColor, PointSize * 0.75f, SDPG_Foreground);
				}
			}
			else
			{
				// An ellipse's corner handles sit on its bounding box, so draw that faintly.
				if (Area->Shape == EBangoAreaShape::Ellipse)
				{
					for (int32 Corner = 4; Corner < NumSizeHandles; ++Corner)
					{
						const int32 Next = Corner + 1 < NumSizeHandles ? Corner + 1 : 4;
						DrawDashedLine(PDI, SizeHandleLocation(*Area, Transform, Corner), SizeHandleLocation(*Area, Transform, Next), EdgeColor * 0.6f, 20.0, SDPG_Foreground);
					}
				}
				for (int32 Index = 0; Index < NumSizeHandles; ++Index)
				{
					const bool bSelected = IsSelected(EKind::Size, Index);
					PDI->SetHitProxy(new HBangoRegionProxy(Component, RegionIndex, EKind::Size, Index));
					PDI->DrawPoint(SizeHandleLocation(*Area, Transform, Index), bSelected ? SelectedColor : HandleColor,
						bSelected ? SelectedSize : (Index < 4 ? PointSize : SmallSize), SDPG_Foreground);
					PDI->SetHitProxy(nullptr);
				}

				const bool bRotateSelected = IsSelected(EKind::Rotate);
				const FLinearColor Color = bRotateSelected ? SelectedColor : RotateColor;
				const FVector Handle = RotateHandleLocation(*Area, Transform);
				DrawDashedLine(PDI, SizeHandleLocation(*Area, Transform, 0), Handle, Color, 10.0, SDPG_Foreground);
				PDI->SetHitProxy(new HBangoRegionProxy(Component, RegionIndex, EKind::Rotate));
				DrawCircle(PDI, Handle, FVector::ForwardVector, FVector::RightVector, Color, RotateHandleRadius, 24, SDPG_Foreground, 2.0f);
				PDI->DrawPoint(Handle, Color, bRotateSelected ? PointSize : SmallSize, SDPG_Foreground);
				PDI->SetHitProxy(nullptr);
			}
		}

		const bool bCentreSelected = IsSelected(EKind::Centre);
		PDI->SetHitProxy(new HBangoRegionProxy(Component, RegionIndex, EKind::Centre));
		PDI->DrawPoint(Region->GetCentre(Transform), bCentreSelected ? SelectedColor : CentreColor, bCentreSelected ? PointSize : SmallSize, SDPG_Foreground);
		PDI->SetHitProxy(nullptr);
	}
}

void FBangoTriggerShapeVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	if (bAddingPoints && Canvas && GEngine && GetEditedComponent() == Component)
	{
		Canvas->DrawShadowedString(20.0, 60.0,
			TEXT("Adding area points: click in the level to add a point. Esc or Enter to stop."),
			GEngine->GetLargeFont(), SelectedColor);
	}
}

// ---- Selection ----

UActorComponent* FBangoTriggerShapeVisualizer::GetEditedComponent() const
{
	return ShapePropertyPath.GetComponent();
}

ABangoTrigger* FBangoTriggerShapeVisualizer::GetEditedTrigger() const
{
	return GetTrigger(GetEditedComponent());
}

FBangoTriggerRegion* FBangoTriggerShapeVisualizer::GetSelectedRegion() const
{
	return GetRegionAs<FBangoTriggerRegion>(GetEditedTrigger(), SelectedRegion);
}

FBangoAreaRegion* FBangoTriggerShapeVisualizer::GetSelectedArea() const
{
	return GetRegionAs<FBangoAreaRegion>(GetEditedTrigger(), SelectedRegion);
}

FBangoAreaRegion* FBangoTriggerShapeVisualizer::GetSelectedPolygon(bool bEdge) const
{
	FBangoAreaRegion* Area = GetSelectedArea();
	if (!Area || Area->Shape != EBangoAreaShape::Polygon || SelectedKind != (bEdge ? EKind::Edge : EKind::Point))
	{
		return nullptr;
	}
	return Area->Points.IsValidIndex(SelectedIndex) ? Area : nullptr;
}

bool FBangoTriggerShapeVisualizer::IsSizedArea(const FBangoAreaRegion* Area) const
{
	return Area && (Area->Shape == EBangoAreaShape::Rectangle || Area->Shape == EBangoAreaShape::Ellipse);
}

bool FBangoTriggerShapeVisualizer::HasSelection() const
{
	if (!GetSelectedRegion())
	{
		return false;
	}
	switch (SelectedKind)
	{
	case EKind::Centre: return true;
	case EKind::Point:  return GetSelectedPolygon(false) != nullptr;
	case EKind::Edge:   return GetSelectedPolygon(true) != nullptr;
	case EKind::Size:   return IsSizedArea(GetSelectedArea()) && SelectedIndex >= 0 && SelectedIndex < NumSizeHandles;
	case EKind::Rotate: return IsSizedArea(GetSelectedArea());
	}
	return false;
}

void FBangoTriggerShapeVisualizer::ClearSelection()
{
	SelectedRegion = INDEX_NONE;
	SelectedKind = EKind::Centre;
	SelectedIndex = INDEX_NONE;
	bAddingPoints = false;
	bRotateDragging = false;
}

bool FBangoTriggerShapeVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (!VisProxy || !VisProxy->Component.IsValid() || !VisProxy->IsA(HBangoRegionProxy::StaticGetType()))
	{
		return false;
	}
	const HBangoRegionProxy* Proxy = static_cast<HBangoRegionProxy*>(VisProxy);
	const ABangoTrigger* Trigger = GetTrigger(Proxy->Component.Get());
	if (!Trigger || !Trigger->Regions.IsValidIndex(Proxy->Region))
	{
		return false;
	}

	// Adding points carries on while clicking the same region's points.
	const bool bKeepAdding = bAddingPoints && GetEditedComponent() == Proxy->Component.Get() && SelectedRegion == Proxy->Region && Proxy->Kind == EKind::Point;
	ClearSelection();
	ShapePropertyPath = FComponentPropertyPath(Proxy->Component.Get());
	SelectedRegion = Proxy->Region;
	SelectedKind = Proxy->Kind;
	SelectedIndex = Proxy->Index;
	bAddingPoints = bKeepAdding;

	if (Proxy->Kind == EKind::Edge)
	{
		// The point on the edge nearest the click's ray.
		if (const FBangoAreaRegion* Area = GetRegionAs<FBangoAreaRegion>(Trigger, Proxy->Region); Area && Area->Points.IsValidIndex(Proxy->Index))
		{
			const FTransform& Transform = Trigger->GetActorTransform();
			const FVector A = Area->LocalToWorld(Transform, Area->Points[Proxy->Index]);
			const FVector B = Area->LocalToWorld(Transform, Area->Points[(Proxy->Index + 1) % Area->Points.Num()]);
			FVector OnEdge, OnRay;
			FMath::SegmentDistToSegmentSafe(A, B, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX, OnEdge, OnRay);
			EdgeClickLocation = OnEdge;
		}
	}
	return true;
}

void FBangoTriggerShapeVisualizer::EndEditing()
{
	// Undo and redo end editing too; the selection is checked again where it's used.
	if (!GIsTransacting)
	{
		ShapePropertyPath.Reset();
		ClearSelection();
		bDragChanged = false;
	}
}

// ---- Gizmo ----

bool FBangoTriggerShapeVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	const ABangoTrigger* Trigger = GetEditedTrigger();
	if (!Trigger || !HasSelection())
	{
		return false;
	}
	const FTransform& Transform = Trigger->GetActorTransform();
	const FBangoAreaRegion* Area = GetSelectedArea();
	switch (SelectedKind)
	{
	case EKind::Centre:
		OutLocation = GetSelectedRegion()->GetCentre(Transform);
		return true;
	case EKind::Point:
		OutLocation = Area->LocalToWorld(Transform, Area->Points[SelectedIndex]);
		return true;
	case EKind::Size:
		OutLocation = SizeHandleLocation(*Area, Transform, SelectedIndex);
		return true;
	case EKind::Rotate:
		// While dragging, the gizmo follows the mouse; the handle itself may be held back by rotation snapping.
		OutLocation = bRotateDragging ? RotateDragHandle : RotateHandleLocation(*Area, Transform);
		return true;
	case EKind::Edge:
		return false;
	}
	return false;
}

bool FBangoTriggerShapeVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	const ABangoTrigger* Trigger = GetEditedTrigger();
	if (!Trigger || !ViewportClient || ViewportClient->GetWidgetCoordSystemSpace() != COORD_Local || !HasSelection())
	{
		return false;
	}
	FQuat Relative;
	if (!GetRelativeRotation(Trigger, SelectedRegion, Relative))
	{
		// A sphere: the trigger's own rotation.
		OutMatrix = FRotationMatrix(Trigger->GetActorRotation());
		return true;
	}
	if (GetSelectedArea())
	{
		// An area only turns with yaw.
		OutMatrix = FRotationMatrix(FRotator(0.0, GetSelectedArea()->GetWorldYaw(Trigger->GetActorTransform()), 0.0));
		return true;
	}
	OutMatrix = FRotationMatrix((Trigger->GetActorQuat() * Relative).Rotator());
	return true;
}

bool FBangoTriggerShapeVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	ABangoTrigger* Trigger = GetEditedTrigger();
	if (!Trigger || !HasSelection())
	{
		return false;
	}
	// The level editor has already opened a transaction for the drag.
	FBangoTriggerRegion& Region = *GetSelectedRegion();
	FBangoAreaRegion* Area = GetSelectedArea();

	switch (SelectedKind)
	{
	case EKind::Centre:
		if (!DeltaTranslate.IsNearlyZero())
		{
			MoveCentre(Trigger, Region, DeltaTranslate);
		}
		if (!DeltaRotate.IsNearlyZero())
		{
			TurnRegion(Trigger, Region, DeltaRotate);
		}
		break;
	case EKind::Point:
		if (!DeltaTranslate.IsNearlyZero())
		{
			MovePoint(Trigger, *Area, DeltaTranslate);
		}
		break;
	case EKind::Size:
		if (!DeltaTranslate.IsNearlyZero())
		{
			MoveSizeHandle(Trigger, *Area, DeltaTranslate);
		}
		if (!FMath::IsNearlyZero(DeltaRotate.Yaw))
		{
			TurnRegion(Trigger, Region, DeltaRotate);
		}
		break;
	case EKind::Rotate:
		if (!DeltaTranslate.IsNearlyZero())
		{
			MoveRotateHandle(Trigger, *Area, DeltaTranslate);
		}
		if (!FMath::IsNearlyZero(DeltaRotate.Yaw))
		{
			TurnRegion(Trigger, Region, DeltaRotate);
		}
		break;
	case EKind::Edge:
		return false;
	}

	// Swallow everything else (scaling a handle means nothing), so the trigger itself isn't changed instead.
	return true;
}

void FBangoTriggerShapeVisualizer::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	if (bDragChanged)
	{
		if (ABangoTrigger* Trigger = GetEditedTrigger())
		{
			EndChange(Trigger, false);
		}
	}
	bDragChanged = false;
	bRotateDragging = false;
}

void FBangoTriggerShapeVisualizer::MoveCentre(ABangoTrigger* Trigger, FBangoTriggerRegion& Region, const FVector& Delta)
{
	const FTransform& Transform = Trigger->GetActorTransform();
	Trigger->Modify();
	if (FBangoAreaRegion* Area = GetSelectedArea())
	{
		// The new centre, in the trigger's local 2D space: the area's Offset.
		const FVector NewCentre = Area->GetCentre(Transform) + FVector(Delta.X, Delta.Y, 0.0);
		const FVector2D InRegion = Area->WorldToLocal(Transform, NewCentre);
		Area->Offset += Area->LocalToTrigger(InRegion);
	}
	else if (FBangoBoxRegion* Box = GetRegionAs<FBangoBoxRegion>(Trigger, SelectedRegion))
	{
		Box->Offset += Transform.InverseTransformVector(Delta);
	}
	else if (FBangoSphereRegion* Sphere = GetRegionAs<FBangoSphereRegion>(Trigger, SelectedRegion))
	{
		Sphere->Offset += Transform.InverseTransformVector(Delta);
	}
	else if (FBangoCapsuleRegion* Capsule = GetRegionAs<FBangoCapsuleRegion>(Trigger, SelectedRegion))
	{
		Capsule->Offset += Transform.InverseTransformVector(Delta);
	}
	else
	{
		return;
	}
	bDragChanged = true;
	EndChange(Trigger, true);
}

void FBangoTriggerShapeVisualizer::TurnRegion(ABangoTrigger* Trigger, FBangoTriggerRegion& Region, const FRotator& Delta)
{
	Trigger->Modify();
	if (FBangoAreaRegion* Area = GetSelectedArea())
	{
		Area->Yaw = (float)FRotator::NormalizeAxis((double)Area->Yaw + Delta.Yaw);
	}
	else
	{
		// The gizmo's delta is in world space: apply it to the region's world rotation, then take the trigger's back off.
		const FQuat TriggerRotation = Trigger->GetActorQuat();
		auto Turn = [&](FRotator& Rotation)
		{
			const FQuat World = TriggerRotation * Rotation.Quaternion();
			Rotation = (TriggerRotation.Inverse() * (Delta.Quaternion() * World)).Rotator();
		};
		if (FBangoBoxRegion* Box = GetRegionAs<FBangoBoxRegion>(Trigger, SelectedRegion))
		{
			Turn(Box->Rotation);
		}
		else if (FBangoCapsuleRegion* Capsule = GetRegionAs<FBangoCapsuleRegion>(Trigger, SelectedRegion))
		{
			Turn(Capsule->Rotation);
		}
		else
		{
			return;
		}
	}
	bDragChanged = true;
	EndChange(Trigger, true);
}

void FBangoTriggerShapeVisualizer::MovePoint(ABangoTrigger* Trigger, FBangoAreaRegion& Area, const FVector& Delta)
{
	const FTransform& Transform = Trigger->GetActorTransform();
	Trigger->Modify();
	const FVector Current = Area.LocalToWorld(Transform, Area.Points[SelectedIndex]);
	Area.Points[SelectedIndex] = Area.WorldToLocal(Transform, Current + FVector(Delta.X, Delta.Y, 0.0));
	bDragChanged = true;
	EndChange(Trigger, true);
}

void FBangoTriggerShapeVisualizer::MoveSizeHandle(ABangoTrigger* Trigger, FBangoAreaRegion& Area, const FVector& Delta)
{
	const FTransform& Transform = Trigger->GetActorTransform();
	const FVector Scale = Transform.GetScale3D();
	if (FMath::Abs(Scale.X) < UE_KINDA_SMALL_NUMBER || FMath::Abs(Scale.Y) < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector2D Dir = SizeHandleDirs[SelectedIndex];
	const FVector Handle = SizeHandleLocation(Area, Transform, SelectedIndex);
	const FVector2D LocalDelta = Area.WorldToLocal(Transform, Handle + FVector(Delta.X, Delta.Y, 0.0)) - Area.WorldToLocal(Transform, Handle);

	// The dragged side moves; the opposite side stays, so the centre moves by half the change.
	FVector2D NewSize = Area.Size;
	FVector2D Shift = FVector2D::ZeroVector;
	if (Dir.X != 0.0)
	{
		NewSize.X = FMath::Max(MinShapeSize, Area.Size.X + Dir.X * LocalDelta.X);
		Shift.X = Dir.X * (NewSize.X - Area.Size.X) * 0.5;
	}
	if (Dir.Y != 0.0)
	{
		NewSize.Y = FMath::Max(MinShapeSize, Area.Size.Y + Dir.Y * LocalDelta.Y);
		Shift.Y = Dir.Y * (NewSize.Y - Area.Size.Y) * 0.5;
	}
	if (NewSize.Equals(Area.Size))
	{
		return;
	}

	Trigger->Modify();
	Area.Size = NewSize;
	Area.Offset += Area.LocalToTrigger(Shift);
	bDragChanged = true;
	EndChange(Trigger, true);
}

void FBangoTriggerShapeVisualizer::MoveRotateHandle(ABangoTrigger* Trigger, FBangoAreaRegion& Area, const FVector& Delta)
{
	const FTransform& Transform = Trigger->GetActorTransform();
	const FVector Centre = Area.GetCentre(Transform);
	if (!bRotateDragging)
	{
		bRotateDragging = true;
		RotateDragHandle = RotateHandleLocation(Area, Transform);
		RotateDragStartYaw = Area.Yaw;
		RotateDragStartAngle = AngleOf(RotateDragHandle - Centre);
	}

	RotateDragHandle += FVector(Delta.X, Delta.Y, 0.0);
	const FVector Offset = RotateDragHandle - Centre;
	if (Offset.SizeSquared2D() < 1.0)
	{
		return;
	}

	double NewYaw = RotateDragStartYaw + FMath::FindDeltaAngleDegrees(RotateDragStartAngle, AngleOf(Offset));
	if (GEditor && GetDefault<ULevelEditorViewportSettings>()->RotGridEnabled)
	{
		const double Grid = GEditor->GetRotGridSize().Yaw;
		if (Grid > 0.0)
		{
			NewYaw = FMath::GridSnap(NewYaw, Grid);
		}
	}
	NewYaw = FRotator::NormalizeAxis(NewYaw);
	if (FMath::IsNearlyEqual(NewYaw, (double)Area.Yaw))
	{
		return;
	}
	Trigger->Modify();
	Area.Yaw = (float)NewYaw;
	bDragChanged = true;
	EndChange(Trigger, true);
}

// ---- Keys ----

bool FBangoTriggerShapeVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	FBangoAreaRegion* Area = GetSelectedArea();
	if (!Area || Area->Shape != EBangoAreaShape::Polygon)
	{
		bAddingPoints = false;
		return false;
	}

	if (bAddingPoints)
	{
		if (Key == EKeys::LeftMouseButton)
		{
			// The level editor has just traced the click into the level (and snapped it to the grid if snapping is on).
			if (Event == IE_Pressed && GEditor)
			{
				const bool bPointSelected = SelectedKind == EKind::Point && Area->Points.IsValidIndex(SelectedIndex);
				InsertPoint(bPointSelected ? SelectedIndex + 1 : Area->Points.Num(), GEditor->ClickLocation);
			}
			// Swallow press, release and double click, so the click doesn't also select or deselect things.
			return true;
		}
		if ((Key == EKeys::Escape || Key == EKeys::Enter) && Event == IE_Pressed)
		{
			StopAddingPoints();
			return true;
		}
	}

	if (Key == EKeys::Delete && Event == IE_Pressed && GetSelectedPolygon(false))
	{
		if (CanDeleteSelectedPoint())
		{
			DeleteSelectedPoint();
		}
		// Swallowed either way, so Delete never deletes the whole trigger while editing a point.
		return true;
	}
	return false;
}

// ---- Context menu ----

TSharedPtr<SWidget> FBangoTriggerShapeVisualizer::GenerateContextMenu() const
{
	if (!HasSelection())
	{
		return nullptr;
	}
	FBangoTriggerShapeVisualizer* This = const_cast<FBangoTriggerShapeVisualizer*>(this);
	const FBangoAreaRegion* Area = GetSelectedArea();
	const bool bPolygon = Area && Area->Shape == EBangoAreaShape::Polygon;

	FMenuBuilder Menu(true, nullptr);
	Menu.BeginSection("BangoTriggerRegion", LOCTEXT("Region", "Trigger Region"));

	if (SelectedKind == EKind::Point)
	{
		Menu.AddMenuEntry(
			LOCTEXT("AddPointAfter", "Add Point After"),
			LOCTEXT("AddPointAfterTooltip", "Add a point halfway between this point and the next one."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(This, &FBangoTriggerShapeVisualizer::AddPointAfterSelected)));
		Menu.AddMenuEntry(
			LOCTEXT("DeletePoint", "Delete Point"),
			LOCTEXT("DeletePointTooltip", "Delete this point (Delete key). A polygon keeps at least 3 points."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(This, &FBangoTriggerShapeVisualizer::DeleteSelectedPoint),
				FCanExecuteAction::CreateRaw(This, &FBangoTriggerShapeVisualizer::CanDeleteSelectedPoint)));
	}
	else if (SelectedKind == EKind::Edge)
	{
		Menu.AddMenuEntry(
			LOCTEXT("InsertPointHere", "Insert Point Here"),
			LOCTEXT("InsertPointHereTooltip", "Add a point on this edge, where it was clicked."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(This, &FBangoTriggerShapeVisualizer::InsertPointOnSelectedEdge)));
	}

	if (bPolygon && (SelectedKind == EKind::Point || SelectedKind == EKind::Edge || SelectedKind == EKind::Centre))
	{
		if (bAddingPoints)
		{
			Menu.AddMenuEntry(
				LOCTEXT("StopAddingPoints", "Stop Adding Points"),
				LOCTEXT("StopAddingPointsTooltip", "Stop adding points by clicking (Esc or Enter)."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(This, &FBangoTriggerShapeVisualizer::StopAddingPoints)));
		}
		else
		{
			Menu.AddMenuEntry(
				LOCTEXT("AddPointsByClicking", "Add Points By Clicking"),
				LOCTEXT("AddPointsByClickingTooltip", "Each click in the level adds a point after the selected one. Esc or Enter stops."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(This, &FBangoTriggerShapeVisualizer::StartAddingPoints)));
		}
	}

	if (CanResetRotation())
	{
		Menu.AddMenuEntry(
			LOCTEXT("ResetRotation", "Reset Rotation"),
			LOCTEXT("ResetRotationTooltip", "Turn this region back to the trigger's own rotation."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(This, &FBangoTriggerShapeVisualizer::ResetRotation)));
	}

	Menu.EndSection();
	return Menu.MakeWidget();
}

// ---- Changes ----

void FBangoTriggerShapeVisualizer::EndChange(ABangoTrigger* Trigger, bool bInteractive) const
{
	if (!bInteractive)
	{
		// While dragging, only redraw; the full change notice is sent when the drag stops.
		FPropertyChangedEvent ChangedEvent(GetRegionsProperty(), EPropertyChangeType::ValueSet);
		Trigger->PostEditChangeProperty(ChangedEvent);
	}
	Trigger->ShapeChanged();
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FBangoTriggerShapeVisualizer::InsertPoint(int32 Index, const FVector& WorldLocation)
{
	ABangoTrigger* Trigger = GetEditedTrigger();
	FBangoAreaRegion* Area = GetSelectedArea();
	if (!Trigger || !Area || Area->Shape != EBangoAreaShape::Polygon)
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("AddAreaPoint", "Add Area Point"));
	Trigger->Modify();
	Index = FMath::Clamp(Index, 0, Area->Points.Num());
	Area->Points.Insert(Area->WorldToLocal(Trigger->GetActorTransform(), WorldLocation), Index);
	SelectedKind = EKind::Point;
	SelectedIndex = Index;
	EndChange(Trigger, false);
}

void FBangoTriggerShapeVisualizer::AddPointAfterSelected()
{
	const ABangoTrigger* Trigger = GetEditedTrigger();
	const FBangoAreaRegion* Area = GetSelectedPolygon(false);
	if (!Trigger || !Area)
	{
		return;
	}
	const int32 Num = Area->Points.Num();
	const FVector2D Middle = (Area->Points[SelectedIndex] + Area->Points[(SelectedIndex + 1) % Num]) * 0.5;
	InsertPoint(SelectedIndex + 1, Area->LocalToWorld(Trigger->GetActorTransform(), Middle));
}

bool FBangoTriggerShapeVisualizer::CanDeleteSelectedPoint() const
{
	const FBangoAreaRegion* Area = GetSelectedPolygon(false);
	return Area && Area->Points.Num() > 3;
}

void FBangoTriggerShapeVisualizer::DeleteSelectedPoint()
{
	ABangoTrigger* Trigger = GetEditedTrigger();
	FBangoAreaRegion* Area = GetSelectedPolygon(false);
	if (!Trigger || !CanDeleteSelectedPoint())
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("DeleteAreaPoint", "Delete Area Point"));
	Trigger->Modify();
	Area->Points.RemoveAt(SelectedIndex);
	// Select the point before it, so Delete can be pressed again.
	SelectedIndex = (SelectedIndex + Area->Points.Num() - 1) % Area->Points.Num();
	EndChange(Trigger, false);
}

void FBangoTriggerShapeVisualizer::InsertPointOnSelectedEdge()
{
	if (GetSelectedPolygon(true))
	{
		InsertPoint(SelectedIndex + 1, EdgeClickLocation);
	}
}

void FBangoTriggerShapeVisualizer::StartAddingPoints()
{
	const FBangoAreaRegion* Area = GetSelectedArea();
	bAddingPoints = Area && Area->Shape == EBangoAreaShape::Polygon;
	if (SelectedKind == EKind::Edge)
	{
		SelectedKind = EKind::Centre;
		SelectedIndex = INDEX_NONE;
	}
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void FBangoTriggerShapeVisualizer::StopAddingPoints()
{
	bAddingPoints = false;
	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports(true);
	}
}

bool FBangoTriggerShapeVisualizer::CanResetRotation() const
{
	FQuat Relative;
	return GetRelativeRotation(GetEditedTrigger(), SelectedRegion, Relative)
		&& (SelectedKind == EKind::Centre || SelectedKind == EKind::Size || SelectedKind == EKind::Rotate);
}

void FBangoTriggerShapeVisualizer::ResetRotation()
{
	ABangoTrigger* Trigger = GetEditedTrigger();
	if (!Trigger || !CanResetRotation())
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("ResetRegionRotation", "Reset Region Rotation"));
	Trigger->Modify();
	if (FBangoAreaRegion* Area = GetSelectedArea())
	{
		Area->Yaw = 0.0f;
	}
	else if (FBangoBoxRegion* Box = GetRegionAs<FBangoBoxRegion>(Trigger, SelectedRegion))
	{
		Box->Rotation = FRotator::ZeroRotator;
	}
	else if (FBangoCapsuleRegion* Capsule = GetRegionAs<FBangoCapsuleRegion>(Trigger, SelectedRegion))
	{
		Capsule->Rotation = FRotator::ZeroRotator;
	}
	EndChange(Trigger, false);
}

#undef LOCTEXT_NAMESPACE
