#include "SPWorldDressing.h"

#include "SPFlightPawn.h"
#include "SPWorldSurface.h"
#include "SpacePatriotBlueprintBases.h"
#include "SpacePatriotSystemsComponent.h"
#include "SPSocietySimulationComponent.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"

namespace
{
    constexpr double EgressClearRadiusCm = 3000.0; // 30 m from the landing focus
    constexpr double LandingRecenterDistanceCm = 120000.0; // 1.2 km
    constexpr int32 RockCount = 26;
    constexpr int32 TemperateFoliageCount = 16;
    constexpr int32 DesertFoliageCount = 8;
    const FVector2D FieldSiteCenterCm(16000.0, 9000.0);

    struct FWorldPalette
    {
        FLinearColor OutpostColor;
        int32 PrimaryRock = 0;
        int32 SecondaryRock = 1;
    };

    FWorldPalette PaletteForBiome(const FString& Biome)
    {
        if (Biome == TEXT("desert")) return {FLinearColor(0.42f, 0.27f, 0.12f), 1, 2};
        if (Biome == TEXT("volcanic")) return {FLinearColor(0.28f, 0.11f, 0.08f), 0, 1};
        if (Biome == TEXT("ice")) return {FLinearColor(0.32f, 0.45f, 0.54f), 2, 0};
        if (Biome == TEXT("temperate")) return {FLinearColor(0.21f, 0.34f, 0.25f), 2, 1};
        return {FLinearColor(0.27f, 0.30f, 0.33f), 0, 2};
    }

    uint32 StableSiteSeed(const FString& WorldId, const FVector& SiteRadial)
    {
        const uint32 WorldHash = FCrc::StrCrc32(*WorldId.ToLower());
        const FIntVector Cell(
            FMath::FloorToInt(SiteRadial.X * 32.0),
            FMath::FloorToInt(SiteRadial.Y * 32.0),
            FMath::FloorToInt(SiteRadial.Z * 32.0));
        return WorldHash ^ (static_cast<uint32>(Cell.X) * 73856093u)
            ^ (static_cast<uint32>(Cell.Y) * 19349663u)
            ^ (static_cast<uint32>(Cell.Z) * 83492791u);
    }

    /** The source globe's nominal radius is 3 m below its actor origin. */
    double NominalSurfaceRadiusCm(const ASPWorldSurface* Surface)
    {
        return FVector::Dist(Surface->GetActorLocation(), Surface->GetPlanetCenterWorld()) - 300.0;
    }

    FVector SiteRadialFor(const ASPWorldSurface* Surface, const AActor* Focus)
    {
        return (Focus->GetActorLocation() - Surface->GetPlanetCenterWorld()).GetSafeNormal(
            SMALL_NUMBER, Surface->GetActorUpVector());
    }

    FVector SurfacePoint(const ASPWorldSurface* Surface, const FVector& SiteRadial,
                         const FVector& East, const FVector& North, const FVector2D& OffsetCm)
    {
        const FVector Center = Surface->GetPlanetCenterWorld();
        const double RadiusCm = NominalSurfaceRadiusCm(Surface);
        const FVector Radial = (SiteRadial * RadiusCm + East * OffsetCm.X + North * OffsetCm.Y).GetSafeNormal();
        const FVector Probe = Center + Radial * RadiusCm;
        const FSPWorldSurfaceSample Sample = Surface->SampleAtWorldLocation(Probe);
        // DetailMesh adds a 3.5 cm z-fighting guard over the analytic surface.
        return Center + Radial * (RadiusCm + Sample.ElevationMeters * 100.0 + 3.5);
    }

    FQuat FaceSurface(const FVector& Up, float YawDegrees)
    {
        return FQuat(Up, FMath::DegreesToRadians(YawDegrees)) * FRotationMatrix::MakeFromZ(Up).ToQuat();
    }

    FVector MeshOriginAtGround(const FVector& Ground, const FVector& Up,
                               const UStaticMesh* Mesh, const FVector& Scale)
    {
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const double LocalBottom = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale.Z;
        return Ground - Up * LocalBottom;
    }

    AStaticMeshActor* SpawnProp(UWorld* World, AActor* Owner, UStaticMesh* Mesh,
                                UMaterialInterface* OverrideMaterial, const FVector& Ground,
                                const FVector& Up, const FVector& Scale, float YawDegrees,
                                bool bCollision)
    {
        if (!World || !Mesh) return nullptr;
        const FTransform Transform(FaceSurface(Up, YawDegrees), MeshOriginAtGround(Ground, Up, Mesh, Scale), Scale);
        FActorSpawnParameters Parameters;
        Parameters.Owner = Owner;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform, Parameters);
        if (!Actor) return nullptr;
        UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetStaticMesh(Mesh);
        if (OverrideMaterial) Component->SetMaterial(0, OverrideMaterial);
        Component->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Actor->Tags.AddUnique(TEXT("SP_WorldDressing"));
        return Actor;
    }
}

ASPWorldDressing::ASPWorldDressing()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WorldDressingRoot"));
    SetRootComponent(SceneRoot);
}

TArray<FSPWorldRockPlacement> ASPWorldDressing::PlanRocks(
    const FString& WorldId, const FString& Biome, const FVector& SiteRadial)
{
    const FWorldPalette Palette = PaletteForBiome(Biome.ToLower());
    FRandomStream Random(static_cast<int32>(StableSiteSeed(WorldId, SiteRadial)));
    TArray<FSPWorldRockPlacement> Result;
    Result.Reserve(RockCount);
    for (int32 Index = 0; Index < RockCount; ++Index)
    {
        FSPWorldRockPlacement Placement;
        // Jitter a stratified ring. The 30 m ship/boarding area stays open,
        // while the nearest textured geology is visible on foot.
        const float Angle = (2.0f * PI * Index / RockCount) + Random.FRandRange(-0.09f, 0.09f);
        // The egress ring is a gameplay invariant, not just an assertion in
        // SpawnDressing. Clamp after sampling so a malformed stream value
        // cannot put collision geometry beside a newly landed ship.
        const float SampledRadius = Random.FRandRange(5000.0f, 45000.0f);
        const float RadiusCm = FMath::IsFinite(SampledRadius)
            ? FMath::Clamp(SampledRadius, 5000.0f, 45000.0f) : 5000.0f;
        Placement.OffsetCm = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RadiusCm;
        // Keep the generated site footprint and the walk from the ship free of
        // blocking boulders, regardless of which site module set was selected.
        const FVector2D FromSite = Placement.OffsetCm - FieldSiteCenterCm;
        if (FromSite.Size() < 8500.0)
        {
            FVector2D Away = FromSite.GetSafeNormal();
            if (Away.IsNearlyZero()) Away = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
            Placement.OffsetCm = FieldSiteCenterCm + Away * 8500.0;
        }
        const double ApproachT = FVector2D::DotProduct(Placement.OffsetCm, FieldSiteCenterCm)
            / FieldSiteCenterCm.SizeSquared();
        if (ApproachT > 0.0 && ApproachT < 1.0)
        {
            const FVector2D OnApproach = FieldSiteCenterCm * ApproachT;
            const double Clearance = FVector2D::Distance(Placement.OffsetCm, OnApproach);
            if (Clearance < 1500.0)
            {
                const FVector2D Side(-FieldSiteCenterCm.Y, FieldSiteCenterCm.X);
                const double Sign = FVector2D::DotProduct(Placement.OffsetCm - OnApproach, Side) < 0.0 ? -1.0 : 1.0;
                Placement.OffsetCm += Side.GetSafeNormal() * Sign * (1750.0 - Clearance);
            }
        }
        Placement.Scale = Random.FRandRange(1.0f, 2.6f);
        Placement.YawDegrees = Random.FRandRange(0.0f, 360.0f);
        Placement.MeshIndex = Random.FRand() < 0.75f ? Palette.PrimaryRock : Palette.SecondaryRock;
        Result.Add(Placement);
    }
    return Result;
}

TArray<FSPWorldFoliagePlacement> ASPWorldDressing::PlanFoliage(
    const FString& WorldId, const FString& Biome, const FVector& SiteRadial)
{
    const FString CanonicalBiome = Biome.TrimStartAndEnd().ToLower();
    const int32 Count = CanonicalBiome == TEXT("temperate") ? TemperateFoliageCount
        : CanonicalBiome == TEXT("desert") ? DesertFoliageCount : 0;
    TArray<FSPWorldFoliagePlacement> Result;
    if (Count == 0 || WorldId.TrimStartAndEnd().Equals(TEXT("earth"), ESearchCase::IgnoreCase)) return Result;
    Result.Reserve(Count);

    // Use a separate stream so changing flora does not reshuffle the rocks.
    FRandomStream Random(static_cast<int32>(StableSiteSeed(WorldId, SiteRadial) ^ 0x4F1A9E37u));
    const TArray<FSPWorldRockPlacement> Rocks = PlanRocks(WorldId, CanonicalBiome, SiteRadial);
    const FVector2D OutpostCenter(16000.0, 9000.0);
    const FVector2D ThreatSites[2] = {FVector2D(-23000.0, 12000.0), FVector2D(26000.0, -18000.0)};
    for (int32 Index = 0; Index < Count; ++Index)
    {
        for (int32 Attempt = 0; Attempt < 32; ++Attempt)
        {
            // A stratified, jittered band leaves room to board the ship and
            // keeps the outpost approach and wildlife encounter sites open.
            const float Angle = 2.0f * PI * Index / Count
                + Random.FRandRange(-0.28f, 0.28f) + Attempt * 2.399963f;
            const float RadiusCm = Random.FRandRange(6500.0f, 42000.0f);
            const FVector2D Offset(FMath::Cos(Angle) * RadiusCm, FMath::Sin(Angle) * RadiusCm);
            if (FVector2D::Distance(Offset, OutpostCenter) < 8500.0) continue;
            if (FVector2D::Distance(Offset, ThreatSites[0]) < 4500.0 ||
                FVector2D::Distance(Offset, ThreatSites[1]) < 4500.0) continue;
            bool bOverlaps = false;
            for (const FSPWorldRockPlacement& Rock : Rocks)
            {
                if (FVector2D::Distance(Offset, Rock.OffsetCm) < 700.0)
                {
                    bOverlaps = true;
                    break;
                }
            }
            if (bOverlaps) continue;
            for (const FSPWorldFoliagePlacement& Existing : Result)
            {
                if (FVector2D::Distance(Offset, Existing.OffsetCm) < 550.0)
                {
                    bOverlaps = true;
                    break;
                }
            }
            if (bOverlaps) continue;

            FSPWorldFoliagePlacement Placement;
            Placement.OffsetCm = Offset;
            Placement.YawDegrees = Random.FRandRange(0.0f, 360.0f);
            if (CanonicalBiome == TEXT("desert"))
            {
                Placement.MeshIndex = Index % 4 == 0 ? 2 : 0; // landmark trees among dry scrub
                Placement.Scale = Placement.MeshIndex == 2
                    ? Random.FRandRange(1.3f, 2.0f) : Random.FRandRange(0.75f, 1.2f);
            }
            else
            {
                Placement.MeshIndex = Index % 4 == 0 ? 0 : 1; // mostly green understory
                Placement.Scale = Placement.MeshIndex == 1
                    ? Random.FRandRange(0.55f, 0.95f) : Random.FRandRange(0.65f, 0.95f);
            }
            Result.Add(Placement);
            break;
        }
    }
    return Result;
}

float ASPWorldDressing::FoliageDensityFor(const FString& Biome, const FSPWorldSurfaceSample& Sample)
{
    const FString Canonical = Biome.TrimStartAndEnd().ToLower();
    if (Canonical == TEXT("desert"))
        return FMath::Clamp(0.12f + Sample.Moisture * 1.55f - Sample.Rock * 0.18f, 0.10f, 0.58f);
    if (Canonical == TEXT("temperate"))
        return FMath::Clamp(0.25f + Sample.Moisture * 0.55f + Sample.Forest * 0.35f
            - Sample.Rock * 0.30f, 0.20f, 0.95f);
    return 0.0f;
}

FString ASPWorldDressing::SiteIdFor(const FString& WorldId, const FVector& SiteRadial)
{
    const FString Canonical = WorldId.TrimStartAndEnd().ToLower();
    return FString::Printf(TEXT("%s:field:%08x"), *Canonical, StableSiteSeed(Canonical, SiteRadial));
}

FName ASPWorldDressing::SiteKindFor(const FString& WorldId, const FVector& SiteRadial)
{
    switch (StableSiteSeed(WorldId, SiteRadial) % 3u)
    {
    case 0: return FName(TEXT("Port"));
    case 1: return FName(TEXT("Industrial"));
    default: return FName(TEXT("Habitat"));
    }
}

FName ASPWorldDressing::TemplateForSettlement(const FSPSocietySettlement& Settlement)
{
    const FString Name = Settlement.Name.ToLower();
    if (Name.Contains(TEXT("port")) || Name.Contains(TEXT("freight")) || Name.Contains(TEXT("dock")))
        return FName(TEXT("Port"));
    if (Name.Contains(TEXT("depot")) || Name.Contains(TEXT("works")) ||
        Name.Contains(TEXT("mine")) || Name.Contains(TEXT("prospector")) ||
        Name.Contains(TEXT("hydroponics")) || Name.Contains(TEXT("refinery")))
        return FName(TEXT("Industrial"));
    if (Settlement.Kind == TEXT("city")) return FName(TEXT("Habitat"));
    switch (static_cast<uint32>(Settlement.Seed) % 3u)
    {
    case 0: return FName(TEXT("Port"));
    case 1: return FName(TEXT("Industrial"));
    default: return FName(TEXT("Habitat"));
    }
}

bool ASPWorldDressing::ResolveServiceSettlement(const FString& WorldId, const FVector& SiteRadial,
    const TArray<FSPSocietySettlement>& Settlements, FString& OutId, FName& OutTemplate)
{
    OutId.Empty();
    OutTemplate = NAME_None;
    const FString Canonical = WorldId.TrimStartAndEnd().ToLower();
    TArray<FSPSocietySettlement> Candidates;
    for (const FSPSocietySettlement& Settlement : Settlements)
    {
        if (Settlement.WorldId == Canonical && !Settlement.Id.IsEmpty() &&
            (Settlement.Kind == TEXT("city") || Settlement.Kind == TEXT("outpost")))
            Candidates.Add(Settlement);
    }
    if (Candidates.IsEmpty()) return false;
    Candidates.Sort([](const FSPSocietySettlement& A, const FSPSocietySettlement& B)
    {
        return A.Id < B.Id;
    });
    const FSPSocietySettlement& Selected = Candidates[StableSiteSeed(Canonical, SiteRadial) % Candidates.Num()];
    OutId = Selected.Id;
    OutTemplate = TemplateForSettlement(Selected);
    return true;
}

TArray<FSPWorldSitePiece> ASPWorldDressing::PlanSite(const FString& WorldId, const FVector& SiteRadial,
    FName KindOverride)
{
    const uint32 Seed = StableSiteSeed(WorldId, SiteRadial);
    const FName Kind = KindOverride.IsNone() ? SiteKindFor(WorldId, SiteRadial) : KindOverride;
    const float VariationCm = (static_cast<int32>((Seed >> 8) & 3u) - 1.5f) * 240.0f;
    TArray<FSPWorldSitePiece> Result;
    Result.Reserve(20);
    auto Add = [&Result](int32 MeshIndex, float X, float Y, const FVector& Scale,
                       float Yaw = 0.0f, bool bCollision = true)
    {
        FSPWorldSitePiece& Piece = Result.AddDefaulted_GetRef();
        Piece.MeshIndex = MeshIndex;
        Piece.OffsetCm = FieldSiteCenterCm + FVector2D(X, Y);
        Piece.Scale = Scale;
        Piece.YawDegrees = Yaw;
        Piece.bCollision = bCollision;
    };

    // All three modules leave the southwest ship-to-site approach open. The
    // low central slab is a walkable yard, not a wall around the player.
    if (Kind == FName(TEXT("Port")))
    {
        Add(0, 0, 0, FVector(20, 16, 0.25f));
        Add(0, -2600, 2600 + VariationCm, FVector(7, 5, 2.8f));
        Add(0, 2800, 2400 - VariationCm, FVector(6, 5, 2.3f));
        Add(0, 3200, -1700, FVector(5, 4, 2.2f));
        Add(1, -3400, 4200, FVector(0.35f, 0.35f, 8), 0, false);
        Add(1, 4300, 2900, FVector(0.28f, 0.28f, 6), 0, false);
        for (int32 Index = 0; Index < 4; ++Index)
            Add(2, 2200 + Index * 180, -3700 + (Index % 2) * 230,
                FVector(1.45f), Index % 2 ? 90.0f : 0.0f);
    }
    else if (Kind == FName(TEXT("Industrial")))
    {
        Add(0, 0, 0, FVector(24, 18, 0.25f));
        Add(0, -3000, 2700 + VariationCm, FVector(9, 6, 3.0f));
        Add(0, 3000, 2600 - VariationCm, FVector(8, 6, 2.6f));
        Add(0, 3900, -1800, FVector(6, 5, 2.2f));
        Add(1, -3700, 4600, FVector(0.45f, 0.45f, 9), 0, false);
        Add(1, 4500, 4200, FVector(0.40f, 0.40f, 7), 0, false);
        for (int32 Index = 0; Index < 6; ++Index)
            Add(2, 2100 + (Index % 3) * 205, -3800 - (Index / 3) * 230,
                FVector(1.55f), 90.0f);
    }
    else
    {
        Add(0, 0, 0, FVector(18, 18, 0.25f));
        Add(0, -2900, 2700 + VariationCm, FVector(6, 5, 2.4f));
        Add(0, 2900, 2700 - VariationCm, FVector(6, 5, 2.4f));
        Add(0, 3500, -1600, FVector(5, 5, 2.2f));
        Add(1, -3800, 4100, FVector(0.30f, 0.30f, 6), 0, false);
        Add(1, 4000, 3900, FVector(0.24f, 0.24f, 5), 0, false);
        for (int32 Index = 0; Index < 3; ++Index)
            Add(2, 2400 + Index * 230, -3500, FVector(1.1f), 0.0f);
    }
    return Result;
}

void ASPWorldDressing::ClearWorld()
{
    for (AActor* Actor : SpawnedActors)
    {
        if (IsValid(Actor)) Actor->Destroy();
    }
    SpawnedActors.Reset();
    OutpostMaterial = nullptr;
    ActiveWorldId.Empty();
    ActiveBiome.Empty();
    ActiveRegionStyle = NAME_None;
    ActiveSiteId.Empty();
    ActiveServiceSettlementId.Empty();
    ActiveSiteKind = NAME_None;
    ActiveSurface.Reset();
    FocusActor.Reset();
    AnchorFocusLocation = FVector::ZeroVector;
    RecenterElapsed = 0.0f;
}

bool ASPWorldDressing::ApplyWorld(const FString& WorldId, ASPWorldSurface* Surface, AActor* LandingFocus)
{
    if (!GetWorld() || !IsValid(Surface) || !IsValid(LandingFocus)) return false;
    const FString Canonical = WorldId.TrimStartAndEnd().ToLower();
    if (Canonical.IsEmpty() || Surface->WorldId.ToLower() != Canonical) return false;
    const FString Biome = Surface->ResolvedBiome.ToLower();
    if (Canonical == TEXT("earth") || Biome == TEXT("gas"))
    {
        ClearWorld();
        ActiveWorldId = Canonical;
        ActiveBiome = Biome;
        ActiveRegionStyle = Surface->SampleAtWorldLocation(LandingFocus->GetActorLocation()).RegionStyle;
        return true;
    }

    // Soft runtime load: collaborators acquire these packs locally through
    // InstallLocalDependencies.ps1. Do not commit copied Epic/Poly Haven files.
    UStaticMesh* Rocks[3] = {
        LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SpacePatriot/OpenAssets/PolyHaven/Boulder01/boulder_01_LOD1.boulder_01_LOD1")),
        LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SpacePatriot/OpenAssets/PolyHaven/NamaqualandBoulder03/namaqualand_boulder_03_1k.namaqualand_boulder_03_1k")),
        LoadObject<UStaticMesh>(nullptr, TEXT("/Game/SpacePatriot/OpenAssets/PolyHaven/NamaqualandBoulder05/namaqualand_boulder_05_1k.namaqualand_boulder_05_1k")),
    };
    UStaticMesh* Foliage[3] = {nullptr, nullptr, nullptr};
    if (Biome == TEXT("temperate") || Biome == TEXT("desert"))
        Foliage[0] = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage/WildRooibosBush/wild_rooibos_bush_1k.wild_rooibos_bush_1k"));
    if (Biome == TEXT("temperate"))
        Foliage[1] = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage/Shrub02/shrub_02_1k.shrub_02_1k"));
    if (Biome == TEXT("desert"))
        Foliage[2] = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/SpacePatriot/OpenAssets/PolyHaven/Foliage/QuiverTree02/quiver_tree_02_1k.quiver_tree_02_1k"));
    UStaticMesh* Crate = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/SpacePatriot/OpenAssets/PolyHaven/PlasticCrate02/plastic_crate_02_1k.plastic_crate_02_1k"));
    UStaticMesh* ChamferCube = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/LevelPrototyping/Meshes/SM_ChamferCube.SM_ChamferCube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder"));
    UMaterialInterface* FlatColor = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/LevelPrototyping/Materials/M_FlatCol.M_FlatCol"));
    if (!Rocks[0] || !Rocks[1] || !Rocks[2] || !Crate || !ChamferCube || !Cylinder || !FlatColor ||
        ((Biome == TEXT("temperate") || Biome == TEXT("desert")) && !Foliage[0]) ||
        (Biome == TEXT("temperate") && !Foliage[1]) || (Biome == TEXT("desert") && !Foliage[2]))
    {
        UE_LOG(LogTemp, Warning, TEXT("Space Patriot world dressing is missing local pack assets on %s"), *Canonical);
        return false;
    }

    ClearWorld();
    ActiveWorldId = Canonical;
    ActiveBiome = Biome;
    ActiveRegionStyle = Surface->SampleAtWorldLocation(LandingFocus->GetActorLocation()).RegionStyle;
    const FVector SiteRadial = SiteRadialFor(Surface, LandingFocus);
    ActiveSiteId = SiteIdFor(Canonical, SiteRadial);
    ActiveSiteKind = SiteKindFor(Canonical, SiteRadial);
    for (TActorIterator<ASPWorldRuntime> It(GetWorld()); It; ++It)
    {
        if (USPSocietySimulationComponent* Society =
            It->FindComponentByClass<USPSocietySimulationComponent>())
        {
            FString ResolvedId;
            FName ResolvedTemplate;
            if (ResolveServiceSettlement(Canonical, SiteRadial,
                Society->GetSettlementsForWorld(Canonical), ResolvedId, ResolvedTemplate))
            {
                ActiveServiceSettlementId = MoveTemp(ResolvedId);
                ActiveSiteKind = ResolvedTemplate;
            }
            break;
        }
    }
    ActiveSurface = Surface;
    FocusActor = LandingFocus;
    AnchorFocusLocation = LandingFocus->GetActorLocation();
    OutpostMaterial = UMaterialInstanceDynamic::Create(FlatColor, this);
    if (OutpostMaterial)
    {
        const FWorldPalette Palette = PaletteForBiome(Biome);
        OutpostMaterial->SetVectorParameterValue(TEXT("Base Color"), Palette.OutpostColor);
        OutpostMaterial->SetScalarParameterValue(TEXT("Metallic"), 0.35f);
        OutpostMaterial->SetScalarParameterValue(TEXT("Roughness"), 0.68f);
    }
    SpawnDressing(Surface, LandingFocus, Rocks, Foliage, Crate, ChamferCube, Cylinder);
    UE_LOG(LogTemp, Display, TEXT("Space Patriot dressed %s (%s/%s) field %s, service %s [%s], %d actors"),
        *Canonical, *Biome, *ActiveRegionStyle.ToString(), *ActiveSiteId,
        *ActiveServiceSettlementId, *ActiveSiteKind.ToString(), SpawnedActors.Num());
    return SpawnedActors.Num() >= 30;
}

void ASPWorldDressing::SpawnDressing(ASPWorldSurface* Surface, AActor* LandingFocus,
    UStaticMesh* const RockMeshes[3], UStaticMesh* const FoliageMeshes[3], UStaticMesh* Crate,
    UStaticMesh* ChamferCube, UStaticMesh* Cylinder)
{
    const FVector Up = SiteRadialFor(Surface, LandingFocus);
    FVector East = FVector::VectorPlaneProject(Surface->GetActorForwardVector(), Up).GetSafeNormal();
    if (East.IsNearlyZero()) East = FVector::VectorPlaneProject(FVector::ForwardVector, Up).GetSafeNormal();
    if (East.IsNearlyZero()) East = FVector::VectorPlaneProject(FVector::RightVector, Up).GetSafeNormal();
    const FVector North = FVector::CrossProduct(Up, East).GetSafeNormal();
    auto Place = [&](UStaticMesh* Mesh, const FVector2D& Offset, const FVector& Scale,
                     float Yaw, bool bCollision, UMaterialInterface* Material = nullptr)
    {
        checkf(Offset.Size() >= EgressClearRadiusCm, TEXT("World dressing entered the landing egress ring"));
        const FVector Ground = SurfacePoint(Surface, Up, East, North, Offset);
        const FVector Radial = (Ground - Surface->GetPlanetCenterWorld()).GetSafeNormal();
        if (AStaticMeshActor* Actor = SpawnProp(GetWorld(), this, Mesh, Material, Ground,
            Radial, Scale, Yaw, bCollision)) SpawnedActors.Add(Actor);
    };

    const TArray<FSPWorldRockPlacement> Rocks = PlanRocks(ActiveWorldId, ActiveBiome, Up);
    for (const FSPWorldRockPlacement& Placement : Rocks)
    {
        const FSPWorldSurfaceSample Local = Surface->SampleAtWorldLocation(
            SurfacePoint(Surface, Up, East, North, Placement.OffsetCm));
        const float RockScale = FMath::Lerp(0.72f, 1.35f, FMath::Clamp(Local.Rock, 0.0f, 1.0f));
        const float LandformScale = Local.RegionStyle == FName(TEXT("Badlands")) ||
            Local.RegionStyle == FName(TEXT("VolcanicRidge")) ||
            Local.RegionStyle == FName(TEXT("Highland"))
            ? 1.25f : (Local.RegionStyle == FName(TEXT("Dunefield")) ? 0.72f : 1.0f);
        Place(RockMeshes[FMath::Clamp(Placement.MeshIndex, 0, 2)], Placement.OffsetCm,
            FVector(Placement.Scale * RockScale * LandformScale), Placement.YawDegrees, true);
    }

    FRandomStream EcologyRandom(static_cast<int32>(StableSiteSeed(ActiveWorldId, Up) ^ 0x7ECA10C1u));
    for (const FSPWorldFoliagePlacement& Placement : PlanFoliage(ActiveWorldId, ActiveBiome, Up))
    {
        const FSPWorldSurfaceSample Local = Surface->SampleAtWorldLocation(
            SurfacePoint(Surface, Up, East, North, Placement.OffsetCm));
        if (EcologyRandom.FRand() > FoliageDensityFor(ActiveBiome, Local)) continue;
        int32 MeshIndex = Placement.MeshIndex;
        // The one locally installed desert tree belongs in wetter pockets,
        // while exposed dry plains use scrub. These are visual rules only.
        if (ActiveBiome == TEXT("desert") && MeshIndex == 2 && Local.Moisture < 0.21f)
            MeshIndex = 0;
        if (ActiveBiome == TEXT("temperate") && Local.RegionStyle == FName(TEXT("DryPlain")))
            MeshIndex = 0;
        // Foliage does not block foot traversal; its source meshes retain
        // masked two-sided materials and generated distance LODs.
        Place(FoliageMeshes[MeshIndex], Placement.OffsetCm,
            FVector(Placement.Scale), Placement.YawDegrees, false);
    }

    // Temporary, walk-around site modules vary by stable world/landing cell.
    // They are field locations, not the 420 catalogued city destinations.
    for (const FSPWorldSitePiece& Piece : PlanSite(ActiveWorldId, Up, ActiveSiteKind))
    {
        UStaticMesh* Mesh = Piece.MeshIndex == 0 ? ChamferCube
            : (Piece.MeshIndex == 1 ? Cylinder : Crate);
        Place(Mesh, Piece.OffsetCm, Piece.Scale, Piece.YawDegrees,
            Piece.bCollision, Piece.MeshIndex == 2 ? nullptr : OutpostMaterial.Get());
    }

    // The present asset closure has no creature mesh pack. These two actors
    // carry real source creature IDs/health when the runtime catalog is in the
    // map, with obvious temporary geology visuals for later replacement.
    TArray<FSPCreatureRecord> Threats;
    for (TActorIterator<ASPWorldRuntime> It(GetWorld()); It; ++It)
    {
        if (!It->Systems) continue;
        for (const FSPCreatureRecord& Record : It->Systems->GetWildlifeForWorld(ActiveWorldId))
        {
            if (Record.Aggression >= 0.5f) Threats.Add(Record);
            if (Threats.Num() >= 2) break;
        }
        break;
    }
    const FVector2D CreatureOffsets[2] = {FVector2D(-23000, 12000), FVector2D(26000, -18000)};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const FVector Ground = SurfacePoint(Surface, Up, East, North, CreatureOffsets[Index]);
        const FVector CreatureUp = (Ground - Surface->GetPlanetCenterWorld()).GetSafeNormal();
        FActorSpawnParameters Parameters;
        Parameters.Owner = this;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASPWildlifeEncounter* Proxy = GetWorld()->SpawnActor<ASPWildlifeEncounter>(
            ASPWildlifeEncounter::StaticClass(), FTransform(FaceSurface(CreatureUp, Index * 90.0f), Ground), Parameters);
        if (!Proxy) continue;
        UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(Proxy, TEXT("CreatureProxyVisual"));
        Proxy->SetRootComponent(Visual);
        Proxy->AddInstanceComponent(Visual);
        Visual->SetMobility(EComponentMobility::Movable);
        Visual->SetStaticMesh(RockMeshes[Index]);
        Visual->SetWorldScale3D(FVector(1.3f, 1.7f, 1.1f));
        Visual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Visual->SetCollisionResponseToAllChannels(ECR_Ignore);
        Visual->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Proxy->ConfigureProjectileHitbox();
        Visual->RegisterComponent();
        const FVector ProxyLocation = MeshOriginAtGround(Ground, CreatureUp, RockMeshes[Index], FVector(1.3f, 1.7f, 1.1f));
        Proxy->SetActorLocation(ProxyLocation);
        Proxy->CreatureId = Threats.IsValidIndex(Index)
            ? Threats[Index].Id : FString::Printf(TEXT("%s-threat-proxy-%d"), *ActiveWorldId, Index + 1);
        Proxy->MaxHealth = Threats.IsValidIndex(Index) ? Threats[Index].Health : 100.0f;
        Proxy->Aggression = Threats.IsValidIndex(Index) ? Threats[Index].Aggression : 0.0f;
        Proxy->AttackDamage = Threats.IsValidIndex(Index) ? Threats[Index].Damage : 0.0f;
        Proxy->Ability = Threats.IsValidIndex(Index) ? Threats[Index].Ability : FString();
        Proxy->bBoss = Threats.IsValidIndex(Index) &&
            (Threats[Index].Role == TEXT("champion") || Threats[Index].Role == TEXT("apex"));
        Proxy->ResetEncounter();
        Proxy->Tags.AddUnique(TEXT("SP_WorldDressing"));
        Proxy->Tags.AddUnique(TEXT("SP_WildlifeProxy"));
        SpawnedActors.Add(Proxy);
    }
}

void ASPWorldDressing::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (ActiveWorldId.IsEmpty() || ActiveWorldId == TEXT("earth") || ActiveBiome == TEXT("gas")) return;
    ASPFlightPawn* Ship = Cast<ASPFlightPawn>(FocusActor.Get());
    if (!Ship || Ship->bFlying || !ActiveSurface.IsValid()) return;
    RecenterElapsed += DeltaSeconds;
    if (RecenterElapsed < 2.0f) return;
    RecenterElapsed = 0.0f;
    if (FVector::Dist(Ship->GetActorLocation(), AnchorFocusLocation) > LandingRecenterDistanceCm)
        ApplyWorld(ActiveWorldId, ActiveSurface.Get(), Ship);
}

void ASPWorldDressing::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearWorld();
    Super::EndPlay(EndPlayReason);
}
