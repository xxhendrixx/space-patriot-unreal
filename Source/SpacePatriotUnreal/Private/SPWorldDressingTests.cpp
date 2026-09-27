#include "SPWorldDressing.h"
#include "SPWorldSurface.h"
#include "SPSocietySimulationComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPWorldDressingPlanTest,
    "SpacePatriot.WorldDressing.DeterministicLandingPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPWorldDressingPlanTest::RunTest(const FString& Parameters)
{
    const FVector Radial = FVector::UpVector;
    const TArray<FSPWorldRockPlacement> MarsA = ASPWorldDressing::PlanRocks(TEXT("mars"), TEXT("desert"), Radial);
    const TArray<FSPWorldRockPlacement> MarsB = ASPWorldDressing::PlanRocks(TEXT("mars"), TEXT("desert"), Radial);
    const TArray<FSPWorldRockPlacement> Ice = ASPWorldDressing::PlanRocks(TEXT("trappist-1-f"), TEXT("ice"), Radial);
    const TArray<FSPWorldRockPlacement> OtherSite = ASPWorldDressing::PlanRocks(
        TEXT("mars"), TEXT("desert"), FVector(0.35, 0.0, 0.94).GetSafeNormal());
    TestEqual(TEXT("bounded rocks per site"), MarsA.Num(), 26);
    TestEqual(TEXT("second generation has equal count"), MarsB.Num(), MarsA.Num());
    TestEqual(TEXT("other-world plan has equal bound"), Ice.Num(), MarsA.Num());
    TestEqual(TEXT("other-site plan has equal bound"), OtherSite.Num(), MarsA.Num());
    bool bIdentical = MarsA.Num() == MarsB.Num();
    bool bWorldVaries = false;
    bool bSiteVaries = false;
    for (int32 Index = 0; Index < MarsA.Num(); ++Index)
    {
        const FSPWorldRockPlacement& A = MarsA[Index];
        const FSPWorldRockPlacement& B = MarsB[Index];
        bIdentical &= A.OffsetCm.Equals(B.OffsetCm, 0.001) &&
            FMath::IsNearlyEqual(A.Scale, B.Scale) &&
            FMath::IsNearlyEqual(A.YawDegrees, B.YawDegrees) && A.MeshIndex == B.MeshIndex;
        const double RadiusCm = A.OffsetCm.Size();
        TestTrue(FString::Printf(TEXT("rock %d clears ship egress (radius %.1f cm)"), Index, RadiusCm), RadiusCm >= 3000.0);
        TestTrue(FString::Printf(TEXT("rock %d stays within local patch"), Index), RadiusCm <= 65000.0);
        TestTrue(FString::Printf(TEXT("rock %d has a valid pack mesh"), Index), A.MeshIndex >= 0 && A.MeshIndex < 3);
        if (Ice.IsValidIndex(Index))
            bWorldVaries |= !A.OffsetCm.Equals(Ice[Index].OffsetCm, 0.001) || A.MeshIndex != Ice[Index].MeshIndex;
        if (OtherSite.IsValidIndex(Index))
            bSiteVaries |= !A.OffsetCm.Equals(OtherSite[Index].OffsetCm, 0.001);
    }
    TestTrue(TEXT("same world and site reproduce exactly"), bIdentical);
    TestTrue(TEXT("biome/world variation affects layout"), bWorldVaries);
    TestTrue(TEXT("landing elsewhere changes local layout"), bSiteVaries);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPWorldFoliagePlanTest,
    "SpacePatriot.WorldDressing.DeterministicBiomeFoliage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPWorldFoliagePlanTest::RunTest(const FString& Parameters)
{
    const FVector Radial = FVector::UpVector;
    const TArray<FSPWorldFoliagePlacement> Desert = ASPWorldDressing::PlanFoliage(TEXT("mars"), TEXT("desert"), Radial);
    const TArray<FSPWorldFoliagePlacement> Repeat = ASPWorldDressing::PlanFoliage(TEXT("mars"), TEXT("desert"), Radial);
    const TArray<FSPWorldFoliagePlacement> Temperate = ASPWorldDressing::PlanFoliage(TEXT("kepler-186-f"), TEXT("temperate"), Radial);
    const TArray<FSPWorldFoliagePlacement> OtherSite = ASPWorldDressing::PlanFoliage(
        TEXT("mars"), TEXT("desert"), FVector(0.35, 0.0, 0.94).GetSafeNormal());
    TestEqual(TEXT("desert site has a bounded scrub/tree population"), Desert.Num(), 8);
    TestEqual(TEXT("temperate site has a bounded shrub population"), Temperate.Num(), 16);
    TestEqual(TEXT("repeat has the same count"), Repeat.Num(), Desert.Num());
    TestEqual(TEXT("other landing site has the same count"), OtherSite.Num(), Desert.Num());
    const TCHAR* BarrenBiomes[] = {TEXT("rock"), TEXT("ice"), TEXT("volcanic"), TEXT("gas")};
    for (const TCHAR* BarrenBiome : BarrenBiomes)
        TestEqual(FString::Printf(TEXT("%s is barren"), BarrenBiome),
            ASPWorldDressing::PlanFoliage(TEXT("test-world"), BarrenBiome, Radial).Num(), 0);
    TestEqual(TEXT("Earth remains outside the placeholder dressing"),
        ASPWorldDressing::PlanFoliage(TEXT("earth"), TEXT("temperate"), Radial).Num(), 0);

    const TArray<FSPWorldRockPlacement> Rocks = ASPWorldDressing::PlanRocks(TEXT("mars"), TEXT("desert"), Radial);
    bool bReproduced = Desert.Num() == Repeat.Num();
    bool bSiteChanged = Desert.Num() == OtherSite.Num();
    bool bSawDryScrub = false;
    bool bSawTree = false;
    for (int32 Index = 0; Index < Desert.Num(); ++Index)
    {
        const FSPWorldFoliagePlacement& P = Desert[Index];
        const FSPWorldFoliagePlacement& Copy = Repeat[Index];
        bReproduced &= P.OffsetCm.Equals(Copy.OffsetCm, 0.001) &&
            FMath::IsNearlyEqual(P.Scale, Copy.Scale) &&
            FMath::IsNearlyEqual(P.YawDegrees, Copy.YawDegrees) && P.MeshIndex == Copy.MeshIndex;
        bSiteChanged &= !P.OffsetCm.Equals(OtherSite[Index].OffsetCm, 0.001);
        bSawDryScrub |= P.MeshIndex == 0;
        bSawTree |= P.MeshIndex == 2;
        const double Radius = P.OffsetCm.Size();
        TestTrue(FString::Printf(TEXT("desert plant %d outside boarding ring"), Index), Radius >= 6500.0);
        TestTrue(FString::Printf(TEXT("desert plant %d within local patch"), Index), Radius <= 42000.0);
        TestTrue(FString::Printf(TEXT("desert plant %d clears outpost"), Index),
            FVector2D::Distance(P.OffsetCm, FVector2D(16000.0, 9000.0)) >= 8500.0);
        TestTrue(FString::Printf(TEXT("desert plant %d clears first wildlife site"), Index),
            FVector2D::Distance(P.OffsetCm, FVector2D(-23000.0, 12000.0)) >= 4500.0);
        TestTrue(FString::Printf(TEXT("desert plant %d clears second wildlife site"), Index),
            FVector2D::Distance(P.OffsetCm, FVector2D(26000.0, -18000.0)) >= 4500.0);
        TestTrue(FString::Printf(TEXT("desert plant %d is not on top of a rock"), Index),
            !Rocks.ContainsByPredicate([&P](const FSPWorldRockPlacement& Rock)
            { return FVector2D::Distance(P.OffsetCm, Rock.OffsetCm) < 700.0; }));
        for (int32 Prior = 0; Prior < Index; ++Prior)
            TestTrue(FString::Printf(TEXT("desert plant %d clears plant %d"), Index, Prior),
                FVector2D::Distance(P.OffsetCm, Desert[Prior].OffsetCm) >= 550.0);
    }
    bool bSawGreenShrub = false;
    for (const FSPWorldFoliagePlacement& P : Temperate)
    {
        bSawGreenShrub |= P.MeshIndex == 1;
        TestTrue(TEXT("temperate uses only green or dry shrubs"), P.MeshIndex == 0 || P.MeshIndex == 1);
        TestTrue(TEXT("temperate foliage clears landing"), P.OffsetCm.Size() >= 6500.0);
    }
    TestTrue(TEXT("desert includes dry scrub"), bSawDryScrub);
    TestTrue(TEXT("desert includes a quiver tree landmark"), bSawTree);
    TestTrue(TEXT("temperate includes green shrubs"), bSawGreenShrub);
    TestTrue(TEXT("same world/site reproduces foliage"), bReproduced);
    TestTrue(TEXT("landing elsewhere changes foliage"), bSiteChanged);

    FSPWorldSurfaceSample Dry;
    Dry.Moisture = 0.08f;
    Dry.Rock = 0.72f;
    FSPWorldSurfaceSample Wetter = Dry;
    Wetter.Moisture = 0.34f;
    Wetter.Rock = 0.42f;
    TestTrue(TEXT("desert scrub is denser in a wetter local pocket"),
        ASPWorldDressing::FoliageDensityFor(TEXT("desert"), Wetter) >
        ASPWorldDressing::FoliageDensityFor(TEXT("desert"), Dry));
    FSPWorldSurfaceSample Wooded = Wetter;
    Wooded.Moisture = 0.75f;
    Wooded.Forest = 0.85f;
    TestTrue(TEXT("temperate woodland has more plants than exposed highland"),
        ASPWorldDressing::FoliageDensityFor(TEXT("temperate"), Wooded) >
        ASPWorldDressing::FoliageDensityFor(TEXT("temperate"), Dry));
    TestEqual(TEXT("ice remains unplanted"),
        ASPWorldDressing::FoliageDensityFor(TEXT("ice"), Wooded), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPWorldFieldSitePlanTest,
    "SpacePatriot.WorldDressing.ModularFieldSites",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPWorldFieldSitePlanTest::RunTest(const FString& Parameters)
{
    const FVector SiteRadial = FVector::UpVector;
    const FVector OtherRadial = FVector(0.35, 0.0, 0.94).GetSafeNormal();
    const FString SiteId = ASPWorldDressing::SiteIdFor(TEXT("mars"), SiteRadial);
    TestEqual(TEXT("field-site ID is repeatable"), SiteId,
        ASPWorldDressing::SiteIdFor(TEXT("mars"), SiteRadial));
    TestNotEqual(TEXT("landing cell changes field-site ID"), SiteId,
        ASPWorldDressing::SiteIdFor(TEXT("mars"), OtherRadial));
    TestNotEqual(TEXT("world changes field-site ID"), SiteId,
        ASPWorldDressing::SiteIdFor(TEXT("venus"), SiteRadial));

    const FVector2D Center(16000.0, 9000.0);
    TSet<FName> SeenKinds;
    for (int32 Site = 0; Site < 24; ++Site)
    {
        const float Angle = Site * 0.52f;
        const FVector Radial = FVector(FMath::Cos(Angle) * 0.32f,
            FMath::Sin(Angle) * 0.32f, 0.95f).GetSafeNormal();
        const FName Kind = ASPWorldDressing::SiteKindFor(TEXT("mars"), Radial);
        SeenKinds.Add(Kind);
        const TArray<FSPWorldSitePiece> Pieces = ASPWorldDressing::PlanSite(TEXT("mars"), Radial);
        for (const FSPWorldRockPlacement& Rock : ASPWorldDressing::PlanRocks(TEXT("mars"), TEXT("desert"), Radial))
        {
            TestTrue(TEXT("site footprint remains clear of rocks"),
                FVector2D::Distance(Rock.OffsetCm, Center) >= 8499.0);
            const double SegmentT = FVector2D::DotProduct(Rock.OffsetCm, Center) / Center.SizeSquared();
            if (SegmentT > 0.0 && SegmentT < 1.0)
                TestTrue(TEXT("ship-to-yard approach remains clear of rocks"),
                    FVector2D::Distance(Rock.OffsetCm, Center * SegmentT) >= 1499.0);
        }
        TestTrue(TEXT("site has bounded modules"), Pieces.Num() >= 9 && Pieces.Num() <= 20);
        bool bHasWalkableYard = false;
        for (const FSPWorldSitePiece& Piece : Pieces)
        {
            TestTrue(TEXT("site piece clears the ship egress"), Piece.OffsetCm.Size() > 3000.0);
            TestTrue(TEXT("site piece stays local"), Piece.OffsetCm.Size() < 50000.0);
            TestTrue(TEXT("site uses an installed mesh class"), Piece.MeshIndex >= 0 && Piece.MeshIndex <= 2);
            TestTrue(TEXT("site piece has positive scale"), Piece.Scale.GetMin() > 0.0);
            bHasWalkableYard |= Piece.MeshIndex == 0 && Piece.Scale.Z <= 0.30 &&
                Piece.OffsetCm.Equals(Center, 0.001);
            if (Piece.bCollision && Piece.Scale.Z > 0.5f)
            {
                const double SegmentT = FVector2D::DotProduct(Piece.OffsetCm, Center) / Center.SizeSquared();
                if (SegmentT > 0.0 && SegmentT < 1.0)
                {
                    const double ClearanceCm = FVector2D::Distance(Piece.OffsetCm, Center * SegmentT);
                    TestTrue(TEXT("ship-to-yard approach remains open"), ClearanceCm > 600.0);
                }
            }
        }
        TestTrue(TEXT("site keeps a low walkable center"), bHasWalkableYard);
    }
    TestTrue(TEXT("procedural locations include port, industrial and habitat"),
        SeenKinds.Contains(FName(TEXT("Port"))) &&
        SeenKinds.Contains(FName(TEXT("Industrial"))) &&
        SeenKinds.Contains(FName(TEXT("Habitat"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPWorldSettlementTemplateTest,
    "SpacePatriot.WorldDressing.SettlementServiceTemplates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPWorldSettlementTemplateTest::RunTest(const FString& Parameters)
{
    FString Text;
    if (!TestTrue(TEXT("source settlement catalog loads"), FFileHelper::LoadFileToString(Text,
        *FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Settlements.json"))))) return false;
    TSharedPtr<FJsonObject> Root;
    if (!TestTrue(TEXT("settlement JSON parses"),
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())) return false;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!TestTrue(TEXT("settlements array exists"), Root->TryGetArrayField(TEXT("settlements"), Values) && Values))
        return false;

    TArray<FSPSocietySettlement> Settlements;
    TSet<FString> IDs;
    TSet<FString> Worlds;
    TSet<FName> Templates;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Obj.IsValid()) return false;
        FSPSocietySettlement& Settlement = Settlements.AddDefaulted_GetRef();
        double Seed = 0.0;
        if (!Obj->TryGetStringField(TEXT("id"), Settlement.Id) ||
            !Obj->TryGetStringField(TEXT("world"), Settlement.WorldId) ||
            !Obj->TryGetStringField(TEXT("name"), Settlement.Name) ||
            !Obj->TryGetStringField(TEXT("kind"), Settlement.Kind) ||
            !Obj->TryGetNumberField(TEXT("seed"), Seed)) return false;
        Settlement.Seed = static_cast<int32>(static_cast<uint32>(static_cast<uint64>(Seed)));
        TestFalse(TEXT("settlement ID is unique"), IDs.Contains(Settlement.Id));
        IDs.Add(Settlement.Id);
        Worlds.Add(Settlement.WorldId);
        const FName Template = ASPWorldDressing::TemplateForSettlement(Settlement);
        TestTrue(TEXT("every named destination has an available field-site template"),
            Template == FName(TEXT("Port")) || Template == FName(TEXT("Industrial")) ||
            Template == FName(TEXT("Habitat")));
        Templates.Add(Template);
    }
    TestEqual(TEXT("all original settlements are covered"), Settlements.Num(), 420);
    TestEqual(TEXT("all source worlds are covered"), Worlds.Num(), 19);
    TestEqual(TEXT("all three templates are used"), Templates.Num(), 3);

    TArray<FSPSocietySettlement> Reordered = Settlements;
    Reordered.Sort([](const FSPSocietySettlement& A, const FSPSocietySettlement& B)
    {
        return A.Id > B.Id;
    });
    for (const FString& World : Worlds)
    {
        FString ChosenId, ReorderedId;
        FName ChosenTemplate, ReorderedTemplate;
        const FVector Site = FVector(0.24, -0.18, 0.95).GetSafeNormal();
        TestTrue(TEXT("each world resolves a named service ledger"),
            ASPWorldDressing::ResolveServiceSettlement(World, Site, Settlements, ChosenId, ChosenTemplate));
        TestTrue(TEXT("catalog order does not change site assignment"),
            ASPWorldDressing::ResolveServiceSettlement(World, Site, Reordered, ReorderedId, ReorderedTemplate));
        TestEqual(TEXT("same landing cell selects the same ID"), ChosenId, ReorderedId);
        TestEqual(TEXT("same landing cell selects the same template"), ChosenTemplate, ReorderedTemplate);
        TestTrue(TEXT("resolved ID belongs to the active world"),
            Settlements.ContainsByPredicate([&](const FSPSocietySettlement& Settlement)
            { return Settlement.Id == ChosenId && Settlement.WorldId == World; }));
    }
    return true;
}
#endif
