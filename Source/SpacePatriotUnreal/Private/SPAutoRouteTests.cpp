#include "SPFlightPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SPHyperjumpRouteComponent.h"
#include "SPPlayLoopDirector.h"
#include "SPTravelNavigationComponent.h"
#include "SPWorldSurface.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedAutoRouteWorld
    {
        UWorld* World = nullptr;
        FScopedAutoRouteWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("AutoRouteTest")),
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
        ~FScopedAutoRouteWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPAutoRouteJourneyTest,
    "SpacePatriot.Travel.AutoRouteEarthMarsAndManualCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPAutoRouteJourneyTest::RunTest(const FString& Parameters)
{
    FScopedAutoRouteWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;
    ASPWorldSurface* Surface = World->SpawnActor<ASPWorldSurface>();
    ACharacter* Character = World->SpawnActor<ACharacter>();
    APlayerController* Player = World->SpawnActor<APlayerController>();
    ASPPlayLoopDirector* Director = World->SpawnActor<ASPPlayLoopDirector>();
    if (!TestNotNull(TEXT("surface"), Surface) || !TestNotNull(TEXT("ground pawn"), Character) ||
        !TestNotNull(TEXT("player controller"), Player) || !TestNotNull(TEXT("director"), Director)) return false;
    if (!TestTrue(TEXT("Earth source world loads"), Surface->ActivateWorld(TEXT("earth")))) return false;

    Director->ShipClass = ASPFlightPawn::StaticClass();
    Director->ParkedShipLocation = FVector(0.0, 0.0, 200.0);
    Character->SetActorLocation(Director->ParkedShipLocation + Director->BoardingOffsetLocal +
        FVector(0.0, 0.0, 100.0));
    Player->Possess(Character);
    Director->DispatchBeginPlay();
    ASPFlightPawn* Ship = Director->Ship;
    USPHyperjumpRouteComponent* Route = Director->HyperjumpRoute;
    if (!TestNotNull(TEXT("flyable ship"), Ship) || !TestNotNull(TEXT("shared route"), Route) ||
        !TestTrue(TEXT("player boards"), Director->TryBoard()) ||
        !TestTrue(TEXT("Mars destination selected"), Route->SelectDestination(TEXT("mars"))) ||
        !TestTrue(TEXT("auto route launches from the parked ship"), Ship->StartAutoRoute())) return false;

    bool bLanded = false;
    bool bArrivalDetailBuilt = false;
    for (int32 Step = 0; Step < 500; ++Step)
    {
        Director->Tick(0.1f);
        Route->TickComponent(0.1f, LEVELTICK_All, nullptr);
        Ship->Tick(0.1f);
        // The fixture advances actors explicitly; rebuild the near-field
        // collision once where the running world would stream it on Tick.
        if (!bArrivalDetailBuilt && Surface->WorldId == TEXT("mars") &&
            Surface->GetAltitudeMetersAtWorldLocation(Ship->GetActorLocation()) < 1000.0f)
        {
            Surface->RebuildSurface();
            bArrivalDetailBuilt = true;
        }
        if (!Ship->bFlying && !Ship->IsAutoRouteActive() && Surface->WorldId == TEXT("mars"))
        {
            bLanded = true;
            break;
        }
        if (!Ship->IsAutoRouteActive()) break;
    }
    if (!TestTrue(TEXT("one auto route reaches a physical Mars touchdown"), bLanded) ||
        !TestEqual(TEXT("route and ship agree on Mars"),
            Ship->TravelNavigation->GetNavigationState().CurrentWorldId, FString(TEXT("mars"))) ||
        !TestTrue(TEXT("ship gear is down after landing"), Ship->bGearDown) ||
        !TestEqual(TEXT("jump charged fuel once"), Ship->FuelPercent, 93.0f)) return false;

    // The Earth harbor pad must not flatten the Worldworks field on Mars.
    // Probe several points inside the old shared pad footprint.
    float MinHeight = TNumericLimits<float>::Max();
    float MaxHeight = -TNumericLimits<float>::Max();
    for (const FVector& Probe : {FVector(0.0, 0.0, 0.0), FVector(10000.0, 10000.0, 0.0),
        FVector(-10000.0, -10000.0, 0.0), FVector(12000.0, -8000.0, 0.0)})
    {
        const float Height = Surface->SampleAtWorldLocation(Probe).ElevationMeters;
        MinHeight = FMath::Min(MinHeight, Height);
        MaxHeight = FMath::Max(MaxHeight, Height);
    }
    TestTrue(TEXT("Mars keeps terrain relief near the landing site"), MaxHeight - MinHeight > 1.0f);
    const FLinearColor MarsGround = Surface->SampleAtWorldLocation(Ship->GetActorLocation()).Color;
    TestTrue(TEXT("Mars generated terrain palette is warm"), MarsGround.R > MarsGround.B);

    if (!TestTrue(TEXT("pilot can leave the auto-landed ship"), Director->TryDisembark())) return false;
    Character->SetActorLocation(Ship->GetActorTransform().TransformPosition(Director->BoardingOffsetLocal)
        + Ship->GetActorUpVector() * 100.0f);
    if (!TestTrue(TEXT("pilot can reboard and continue"), Director->TryBoard()) ||
        !TestTrue(TEXT("Venus destination selected"), Route->SelectDestination(TEXT("venus"))) ||
        !TestTrue(TEXT("second auto route begins"), Ship->StartAutoRoute())) return false;
    for (int32 Step = 0; Step < 12; ++Step)
    {
        Director->Tick(0.1f);
        Route->TickComponent(0.1f, LEVELTICK_All, nullptr);
        Ship->Tick(0.1f);
    }
    Ship->CancelAutoRoute();
    TestFalse(TEXT("manual cancel disables guidance"), Ship->IsAutoRouteActive());
    TestTrue(TEXT("ship remains controllable in flight"), Ship->bFlying);
    TestEqual(TEXT("cancel does not change world"), Surface->WorldId, FString(TEXT("mars")));
    TestEqual(TEXT("cancel does not spend jump fuel"), Ship->FuelPercent, 93.0f);
    return true;
}

#endif
