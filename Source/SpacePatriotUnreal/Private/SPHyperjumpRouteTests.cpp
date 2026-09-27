#include "SPHyperjumpRouteComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SPFlightPawn.h"
#include "SPWorldSurface.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

namespace
{
    /** A separate world keeps the route test out of the editor's saved maps. */
    struct FScopedHyperjumpWorld
    {
        UWorld* World = nullptr;

        FScopedHyperjumpWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("HyperjumpRouteTest")),
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

        ~FScopedHyperjumpWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPHyperjumpRouteWorldTransitionTest,
    "SpacePatriot.Travel.HyperjumpRouteWorldTransition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPHyperjumpRouteWorldTransitionTest::RunTest(const FString& Parameters)
{
    FScopedHyperjumpWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;

    ASPWorldSurface* Surface = World->SpawnActor<ASPWorldSurface>();
    ASPFlightPawn* Ship = World->SpawnActor<ASPFlightPawn>();
    APlayerController* Pilot = World->SpawnActor<APlayerController>();
    AActor* RouteOwner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("source world surface"), Surface) ||
        !TestNotNull(TEXT("flight pawn"), Ship) ||
        !TestNotNull(TEXT("pilot controller"), Pilot) ||
        !TestNotNull(TEXT("route owner"), RouteOwner)) return false;
    if (!TestTrue(TEXT("Earth source terrain and climate fields load"), Surface->ActivateWorld(TEXT("earth")))) return false;

    USPHyperjumpRouteComponent* Route = NewObject<USPHyperjumpRouteComponent>(RouteOwner);
    if (!TestNotNull(TEXT("route component"), Route)) return false;
    RouteOwner->AddInstanceComponent(Route);
    Route->RegisterComponent();
    Ship->SetActorLocation(FVector(0.0, 0.0, 350000.0), false, nullptr, ETeleportType::TeleportPhysics);
    Ship->bFlying = true;
    Ship->bPowered = true;
    Ship->bGearDown = true;
    Pilot->Possess(Ship);
    if (!TestTrue(TEXT("route uses live ship and world actors"), Route->Configure(Ship, Surface))) return false;
    TestEqual(TEXT("original catalog contains all worlds"), Route->GetNavigationComponent()->GetWorlds().Num(), 19);

    TestTrue(TEXT("Mars is a selectable source destination"), Route->SelectDestination(TEXT("mars")));
    TestFalse(TEXT("deployed gear blocks an otherwise safe jump"), Route->RequestJump());
    Ship->bGearDown = false;
    TestTrue(TEXT("jump begins with pilot, power, altitude, resources and retracted gear"), Route->RequestJump());
    TestEqual(TEXT("charging does not change the physical world"), Surface->WorldId, FString(TEXT("earth")));
    TestEqual(TEXT("charge starts before any fuel debit"), Ship->FuelPercent, 100.0f);

    for (int32 Step = 0; Step < 25; ++Step)
        Route->TickComponent(0.1f, LEVELTICK_All, nullptr);
    const FSPHyperjumpRouteStatus Transit = Route->GetRouteStatus();
    TestEqual(TEXT("two-second charge enters transit"), Transit.Navigation.Phase, ESPTravelPhase::JumpTransit);
    TestEqual(TEXT("world remains Earth during transit"), Surface->WorldId, FString(TEXT("earth")));
    TestEqual(TEXT("jump debits ship fuel exactly once"), Ship->FuelPercent, 93.0f);
    TestTrue(TEXT("NAV inhibits weapons during transit"), Route->GetNavigationComponent()->AreWeaponsInhibited());

    Route->TickComponent(1.0f, LEVELTICK_All, nullptr);
    const FSPHyperjumpRouteStatus Arrival = Route->GetRouteStatus();
    TestEqual(TEXT("destination source profile becomes active"), Surface->WorldId, FString(TEXT("mars")));
    TestEqual(TEXT("navigation confirms physical arrival"), Arrival.Navigation.CurrentWorldId, FString(TEXT("mars")));
    TestEqual(TEXT("navigation returns to flight"), Arrival.Navigation.Phase, ESPTravelPhase::Flight);
    TestFalse(TEXT("SCM restores weapons after arrival"), Route->GetNavigationComponent()->AreWeaponsInhibited());
    TestTrue(TEXT("ship arrives above Mars surface"),
        FMath::IsNearlyEqual(Ship->GetActorLocation().Z, 350000.0, 500.0));
    TestTrue(TEXT("Mars can be surface-landed"), Arrival.bSurfaceLandable);
    TestEqual(TEXT("ship keeps remaining fuel"), Ship->FuelPercent, 93.0f);

    TestTrue(TEXT("next route can target another star-system body"), Route->SelectDestination(TEXT("jupiter")));
    TestTrue(TEXT("ship can jump again after arrival"), Route->RequestJump());
    for (int32 Step = 0; Step < 25; ++Step)
        Route->TickComponent(0.1f, LEVELTICK_All, nullptr);
    Route->TickComponent(1.0f, LEVELTICK_All, nullptr);
    const FSPHyperjumpRouteStatus GasArrival = Route->GetRouteStatus();
    TestEqual(TEXT("Jupiter source profile is activated"), Surface->WorldId, FString(TEXT("jupiter")));
    TestEqual(TEXT("second destination is authoritative"), GasArrival.Navigation.CurrentWorldId, FString(TEXT("jupiter")));
    TestFalse(TEXT("gas world cannot be surface-landed"), GasArrival.bSurfaceLandable);
    TestEqual(TEXT("second jump consumes one additional fuel debit"), Ship->FuelPercent, 86.0f);
    return true;
}

#endif
