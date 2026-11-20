#include "BangoTriggerRegion.h"

namespace
{
	constexpr int32 EllipseSegments = 48;
	constexpr int32 CircleSegments = 32;

	/** Even-odd rule, so a polygon that crosses itself still has a clear inside. */
	bool IsInsidePolygon(const TArray<FVector2D>& Polygon, const FVector2D& P)
	{
		const int32 Num = Polygon.Num();
		if (Num < 3)
		{
			return false;
		}
		bool bInside = false;
		for (int32 I = 0, J = Num - 1; I < Num; J = I++)
		{
			const FVector2D& A = Polygon[I];
			const FVector2D& B = Polygon[J];
			if ((A.Y > P.Y) != (B.Y > P.Y))
			{
				const double CrossX = A.X + (P.Y - A.Y) * (B.X - A.X) / (B.Y - A.Y);
				if (P.X < CrossX)
				{
					bInside = !bInside;
				}
			}
		}
		return bInside;
	}

	FVector2D Rotate2D(const FVector2D& P, double Degrees)
	{
		double Sin, Cos;
		FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(Degrees));
		return FVector2D(P.X * Cos - P.Y * Sin, P.X * Sin + P.Y * Cos);
	}

	/** A circle of lines around Centre, in the plane of the unit vectors X and Y. */
	void AddCircle(TArray<FBangoTriggerLine>& OutLines, const FVector& Centre, const FVector& X, const FVector& Y, double Radius, bool bMain,
		double FromDegrees = 0.0, double ToDegrees = 360.0)
	{
		const int32 Segments = FMath::Max(2, FMath::CeilToInt(CircleSegments * (ToDegrees - FromDegrees) / 360.0));
		FVector Last = FVector::ZeroVector;
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			double Sin, Cos;
			FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(FMath::Lerp(FromDegrees, ToDegrees, (double)Index / Segments)));
			const FVector Point = Centre + (X * Cos + Y * Sin) * Radius;
			if (Index > 0)
			{
				OutLines.Add({ Last, Point, bMain });
			}
			Last = Point;
		}
	}
}

// ---- Area ----

FBangoAreaRegion::FBangoAreaRegion()
{
	Points = {
		FVector2D(-200.0, -200.0),
		FVector2D(200.0, -200.0),
		FVector2D(200.0, 200.0),
		FVector2D(-200.0, 200.0),
	};
}

double FBangoAreaRegion::GetWorldYaw(const FTransform& TriggerTransform) const
{
	return TriggerTransform.Rotator().Yaw + Yaw;
}

FVector2D FBangoAreaRegion::LocalToTrigger(const FVector2D& Point) const
{
	return Rotate2D(Point, Yaw);
}

FVector2D FBangoAreaRegion::WorldToLocal(const FTransform& TriggerTransform, const FVector& Location) const
{
	const FVector Scale = TriggerTransform.GetScale3D();
	if (FMath::Abs(Scale.X) < UE_KINDA_SMALL_NUMBER || FMath::Abs(Scale.Y) < UE_KINDA_SMALL_NUMBER)
	{
		return FVector2D(UE_BIG_NUMBER, UE_BIG_NUMBER);
	}
	// World -> the trigger's local 2D space (its yaw and X/Y scale only) -> the region's (its Offset and Yaw).
	const FVector Offset3 = Location - TriggerTransform.GetLocation();
	const FVector2D Turned = Rotate2D(FVector2D(Offset3.X, Offset3.Y), -TriggerTransform.Rotator().Yaw);
	const FVector2D InTrigger(Turned.X / Scale.X, Turned.Y / Scale.Y);
	return Rotate2D(InTrigger - Offset, -Yaw);
}

FVector FBangoAreaRegion::LocalToWorld(const FTransform& TriggerTransform, const FVector2D& Point) const
{
	const FVector Scale = TriggerTransform.GetScale3D();
	const FVector2D InTrigger = LocalToTrigger(Point) + Offset;
	const FVector2D Turned = Rotate2D(FVector2D(InTrigger.X * Scale.X, InTrigger.Y * Scale.Y), TriggerTransform.Rotator().Yaw);
	const FVector Origin = TriggerTransform.GetLocation();
	return FVector(Origin.X + Turned.X, Origin.Y + Turned.Y, Origin.Z);
}

FVector FBangoAreaRegion::GetCentre(const FTransform& TriggerTransform) const
{
	return LocalToWorld(TriggerTransform, FVector2D::ZeroVector);
}

void FBangoAreaRegion::GetLocalOutline(TArray<FVector2D>& OutPoints) const
{
	OutPoints.Reset();
	const FVector2D Half = Size * 0.5;
	switch (Shape)
	{
	case EBangoAreaShape::Polygon:
		OutPoints = Points;
		break;

	case EBangoAreaShape::Rectangle:
		OutPoints.Add(FVector2D(-Half.X, -Half.Y));
		OutPoints.Add(FVector2D(Half.X, -Half.Y));
		OutPoints.Add(FVector2D(Half.X, Half.Y));
		OutPoints.Add(FVector2D(-Half.X, Half.Y));
		break;

	case EBangoAreaShape::Ellipse:
		OutPoints.Reserve(EllipseSegments);
		for (int32 Index = 0; Index < EllipseSegments; ++Index)
		{
			double Sin, Cos;
			FMath::SinCos(&Sin, &Cos, UE_DOUBLE_TWO_PI * Index / EllipseSegments);
			OutPoints.Add(FVector2D(Half.X * Cos, Half.Y * Sin));
		}
		break;
	}
}

bool FBangoAreaRegion::IsInside(const FTransform& TriggerTransform, const FVector& Location) const
{
	const FVector2D P = WorldToLocal(TriggerTransform, Location);
	switch (Shape)
	{
	case EBangoAreaShape::Polygon:
		return IsInsidePolygon(Points, P);

	case EBangoAreaShape::Rectangle:
		return FMath::Abs(P.X) <= Size.X * 0.5 && FMath::Abs(P.Y) <= Size.Y * 0.5;

	case EBangoAreaShape::Ellipse:
	{
		const double A = Size.X * 0.5;
		const double B = Size.Y * 0.5;
		return A > 0.0 && B > 0.0 && FMath::Square(P.X / A) + FMath::Square(P.Y / B) <= 1.0;
	}
	}
	return false;
}

FBox FBangoAreaRegion::GetBounds(const FTransform& TriggerTransform) const
{
	TArray<FVector2D> Outline;
	GetLocalOutline(Outline);
	FBox Box(ForceInit);
	const FVector Up(0.0, 0.0, DisplayHeight);
	for (const FVector2D& Point : Outline)
	{
		const FVector World = LocalToWorld(TriggerTransform, Point);
		Box += World;
		Box += World + Up;
	}
	return Box;
}

void FBangoAreaRegion::AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const
{
	TArray<FVector2D> Outline;
	GetLocalOutline(Outline);
	const int32 Num = Outline.Num();
	if (Num < 2)
	{
		return;
	}
	const FVector Up(0.0, 0.0, DisplayHeight);
	TArray<FVector> Ground;
	Ground.Reserve(Num);
	for (const FVector2D& Point : Outline)
	{
		Ground.Add(LocalToWorld(TriggerTransform, Point));
	}
	// An ellipse has too many points for a wall line at each; every 6th is enough.
	const int32 WallStep = Shape == EBangoAreaShape::Ellipse ? 6 : 1;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FVector& A = Ground[Index];
		const FVector& B = Ground[(Index + 1) % Num];
		OutLines.Add({ A, B, true });
		if (DisplayHeight > 0.0f)
		{
			OutLines.Add({ A + Up, B + Up, false });
			if (Index % WallStep == 0)
			{
				OutLines.Add({ A, A + Up, false });
			}
		}
	}
}

// ---- Box ----

FTransform FBangoBoxRegion::GetWorldTransform(const FTransform& TriggerTransform) const
{
	return FTransform(Rotation, Offset) * TriggerTransform;
}

FVector FBangoBoxRegion::GetCentre(const FTransform& TriggerTransform) const
{
	return TriggerTransform.TransformPosition(Offset);
}

bool FBangoBoxRegion::IsInside(const FTransform& TriggerTransform, const FVector& Location) const
{
	// In the box's own space, so its rotation and the trigger's scale (even non-uniform) are taken into account.
	const FVector Local = GetWorldTransform(TriggerTransform).InverseTransformPosition(Location);
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

FBox FBangoBoxRegion::GetBounds(const FTransform& TriggerTransform) const
{
	const FTransform World = GetWorldTransform(TriggerTransform);
	FBox Box(ForceInit);
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Local((Corner & 1) ? Extent.X : -Extent.X, (Corner & 2) ? Extent.Y : -Extent.Y, (Corner & 4) ? Extent.Z : -Extent.Z);
		Box += World.TransformPosition(Local);
	}
	return Box;
}

void FBangoBoxRegion::AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const
{
	const FTransform World = GetWorldTransform(TriggerTransform);
	FVector Corners[8];
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Local((Corner & 1) ? Extent.X : -Extent.X, (Corner & 2) ? Extent.Y : -Extent.Y, (Corner & 4) ? Extent.Z : -Extent.Z);
		Corners[Corner] = World.TransformPosition(Local);
	}
	// The 12 edges: pairs of corners that differ in one bit. The bottom four (no Z bit) are the main ones.
	for (int32 A = 0; A < 8; ++A)
	{
		for (int32 Bit = 1; Bit < 8; Bit <<= 1)
		{
			const int32 B = A | Bit;
			if (B != A)
			{
				OutLines.Add({ Corners[A], Corners[B], (A & 4) == 0 && (B & 4) == 0 });
			}
		}
	}
}

// ---- Sphere ----

FVector FBangoSphereRegion::GetCentre(const FTransform& TriggerTransform) const
{
	return TriggerTransform.TransformPosition(Offset);
}

double FBangoSphereRegion::GetWorldRadius(const FTransform& TriggerTransform) const
{
	return Radius * TriggerTransform.GetScale3D().GetAbs().GetMin();
}

bool FBangoSphereRegion::IsInside(const FTransform& TriggerTransform, const FVector& Location) const
{
	return FVector::DistSquared(Location, GetCentre(TriggerTransform)) <= FMath::Square(GetWorldRadius(TriggerTransform));
}

FBox FBangoSphereRegion::GetBounds(const FTransform& TriggerTransform) const
{
	return FBox::BuildAABB(GetCentre(TriggerTransform), FVector(GetWorldRadius(TriggerTransform)));
}

void FBangoSphereRegion::AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const
{
	const FVector Centre = GetCentre(TriggerTransform);
	const double WorldRadius = GetWorldRadius(TriggerTransform);
	const FQuat Turn = TriggerTransform.GetRotation();
	const FVector X = Turn.GetAxisX(), Y = Turn.GetAxisY(), Z = Turn.GetAxisZ();
	AddCircle(OutLines, Centre, X, Y, WorldRadius, true);
	AddCircle(OutLines, Centre, X, Z, WorldRadius, false);
	AddCircle(OutLines, Centre, Y, Z, WorldRadius, false);
}

// ---- Capsule ----

FVector FBangoCapsuleRegion::GetCentre(const FTransform& TriggerTransform) const
{
	return TriggerTransform.TransformPosition(Offset);
}

void FBangoCapsuleRegion::GetWorldShape(const FTransform& TriggerTransform, FVector& OutCentre, FVector& OutAxis, double& OutRadius, double& OutHalfLine) const
{
	const FVector Scale = TriggerTransform.GetScale3D().GetAbs();
	OutCentre = GetCentre(TriggerTransform);
	OutAxis = (TriggerTransform.GetRotation() * Rotation.Quaternion()).GetAxisZ();
	OutRadius = Radius * FMath::Min(Scale.X, Scale.Y);
	OutHalfLine = FMath::Max(0.0, HalfHeight * Scale.Z - OutRadius);
}

bool FBangoCapsuleRegion::IsInside(const FTransform& TriggerTransform, const FVector& Location) const
{
	FVector Centre, Axis;
	double WorldRadius, HalfLine;
	GetWorldShape(TriggerTransform, Centre, Axis, WorldRadius, HalfLine);
	const double Along = FMath::Clamp(FVector::DotProduct(Location - Centre, Axis), -HalfLine, HalfLine);
	return FVector::DistSquared(Location, Centre + Axis * Along) <= FMath::Square(WorldRadius);
}

FBox FBangoCapsuleRegion::GetBounds(const FTransform& TriggerTransform) const
{
	FVector Centre, Axis;
	double WorldRadius, HalfLine;
	GetWorldShape(TriggerTransform, Centre, Axis, WorldRadius, HalfLine);
	FBox Box = FBox::BuildAABB(Centre + Axis * HalfLine, FVector(WorldRadius));
	Box += FBox::BuildAABB(Centre - Axis * HalfLine, FVector(WorldRadius));
	return Box;
}

void FBangoCapsuleRegion::AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const
{
	FVector Centre, Axis;
	double WorldRadius, HalfLine;
	GetWorldShape(TriggerTransform, Centre, Axis, WorldRadius, HalfLine);
	const FQuat Turn = TriggerTransform.GetRotation() * Rotation.Quaternion();
	const FVector X = Turn.GetAxisX(), Y = Turn.GetAxisY();
	const FVector Top = Centre + Axis * HalfLine;
	const FVector Bottom = Centre - Axis * HalfLine;

	AddCircle(OutLines, Bottom, X, Y, WorldRadius, true);
	AddCircle(OutLines, Top, X, Y, WorldRadius, false);
	for (const FVector& Side : { X, -X, Y, -Y })
	{
		OutLines.Add({ Bottom + Side * WorldRadius, Top + Side * WorldRadius, false });
	}
	// The rounded ends: half circles over the top and under the bottom, in two planes.
	AddCircle(OutLines, Top, X, Axis, WorldRadius, false, 0.0, 180.0);
	AddCircle(OutLines, Top, Y, Axis, WorldRadius, false, 0.0, 180.0);
	AddCircle(OutLines, Bottom, X, -Axis, WorldRadius, false, 0.0, 180.0);
	AddCircle(OutLines, Bottom, Y, -Axis, WorldRadius, false, 0.0, 180.0);
}
