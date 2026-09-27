#include "SPFlightPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedLandingWorld
    {
        UWorld* World = nullptr;

        FScopedLandingWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("PhysicalLandingTest")),
                GetTransientPackage());
            if (!World)
            {
                GEngine->DestroyWorldContext(Context.World());
                return;
            }
            World->AddToRoot();
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
        }

        ~FScopedLandingWorld()
        {
            if (!World) return;
            GEngine->ShutdownWorldNetDriver(World);
            World->DestroyWorld(true);
            World->SetPhysicsScene(nullptr);
            GEngine->DestroyWorldContext(World);
            World->RemoveFromRoot();
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPhysicalLandingFootprintTest,
    "SpacePatriot.Travel.PhysicalLandingFootprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPPhysicalLandingFootprintTest::RunTest(const FString& Parameters)
{
    FScopedLandingWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;

    AActor* Ground = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("landing surface actor"), Ground)) return false;
    UBoxComponent* Floor = NewObject<UBoxComponent>(Ground, TEXT("LandingFloor"));
    if (!TestNotNull(TEXT("landing surface collider"), Floor)) return false;
    Ground->SetRootComponent(Floor);
    Ground->AddInstanceComponent(Floor);
    Floor->SetBoxExtent(FVector(10000.0f, 10000.0f, 100.0f));
    Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Floor->SetCollisionResponseToAllChannels(ECR_Block);
    Floor->RegisterComponent();
    Ground->SetActorLocation(FVector(0.0f, 0.0f, -100.0f));

    ASPFlightPawn* Ship = World->SpawnActor<ASPFlightPawn>();
    if (!TestNotNull(TEXT("flyable ship"), Ship)) return false;
    Ship->SetActorLocation(FVector(0.0f, 0.0f, 5000.0f));
    Ship->bUseRadialSurface = false;
    Ship->bFlying = true;
    Ship->bGearDown = false; // Travel and jump require retracted gear.
    if (!TestTrue(TEXT("landing action accepts broad ground without a separate gear key"),
        Ship->RequestSurfaceLanding())) return false;
    TestTrue(TEXT("landing sequence extends gear automatically"), Ship->bGearDown);
    TestTrue(TEXT("MFD reports landing engaged"), Ship->LandingFeedback.Contains(TEXT("descent engaged")));

    // A center ray by itself would accept this tiny pad, stranding the player
    // when the hatch opens. The full gear and hatch footprint must reject it.
    Floor->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f), true);
    ASPFlightPawn* NarrowShip = World->SpawnActor<ASPFlightPawn>();
    if (!TestNotNull(TEXT("second flyable ship"), NarrowShip)) return false;
    NarrowShip->SetActorLocation(FVector(0.0f, 0.0f, 5000.0f));
    NarrowShip->bUseRadialSurface = false;
    NarrowShip->bFlying = true;
    NarrowShip->bGearDown = false;
    TestFalse(TEXT("center support without gear/hatch ground is rejected"),
        NarrowShip->RequestSurfaceLanding());
    TestFalse(TEXT("failed landing does not extend gear"), NarrowShip->bGearDown);
    TestTrue(TEXT("failure tells the pilot to seek a broad site"),
        NarrowShip->LandingFeedback.Contains(TEXT("broad")));
    return true;
}

#endif
