#include "SPFlightPawn.h"
#include "SPHyperjumpRouteComponent.h"
#include "SPTravelNavigationComponent.h"
#include "SPWorldSurface.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedPilotTravelWorld
    {
        UWorld* World = nullptr;

        FScopedPilotTravelWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("PilotTravelLoopTest")),
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

        ~FScopedPilotTravelWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPilotLaunchAndJumpTest,
    "SpacePatriot.Travel.PilotLaunchAndJump",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPPilotLaunchAndJumpTest::RunTest(const FString& Parameters)
{
    FScopedPilotTravelWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;

    ASPWorldSurface* Surface = World->SpawnActor<ASPWorldSurface>();
    ASPFlightPawn* Ship = World->SpawnActor<ASPFlightPawn>();
    APlayerController* Pilot = World->SpawnActor<APlayerController>();
    AActor* Director = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("planet surface"), Surface) ||
        !TestNotNull(TEXT("physical flight pawn"), Ship) ||
        !TestNotNull(TEXT("pilot controller"), Pilot) ||
        !TestNotNull(TEXT("route owner"), Director)) return false;
    if (!TestTrue(TEXT("Earth activates"), Surface->ActivateWorld(TEXT("earth")))) return false;

    USPHyperjumpRouteComponent* Route = NewObject<USPHyperjumpRouteComponent>(Director);
    if (!TestNotNull(TEXT("route component"), Route)) return false;
    Director->AddInstanceComponent(Route);
    Route->RegisterComponent();
    if (!TestTrue(TEXT("route binds physical ship and planet"), Route->Configure(Ship, Surface))) return false;
    USPTravelNavigationComponent* Navigation = Route->GetNavigationComponent();
    if (!TestNotNull(TEXT("navigation catalog"), Navigation)) return false;

    FSPTravelNavigationState Landed = Navigation->GetNavigationState();
    Landed.Phase = ESPTravelPhase::Landed;
    if (!TestTrue(TEXT("navigation starts landed"), Navigation->RestoreSaveState(Landed))) return false;
    Ship->bFlying = false;
    Ship->bGearDown = true;
    Pilot->Possess(Ship);
    if (!TestTrue(TEXT("pilot takes the actual flight pawn"), Pilot->GetPawn() == Ship)) return false;
    if (!TestTrue(TEXT("powered ship physically launches"), Ship->Launch())) return false;
    TestTrue(TEXT("ship is airborne"), Ship->bFlying);

    FSPTravelContext LaunchContext;
    LaunchContext.bPilotAtControls = Pilot->GetPawn() == Ship;
    LaunchContext.bShipPowered = Ship->bPowered;
    if (!TestTrue(TEXT("navigation follows physical launch"), Navigation->Launch(LaunchContext))) return false;
    if (!TestTrue(TEXT("Mars destination selected"), Route->SelectDestination(TEXT("mars")))) return false;
    TestFalse(TEXT("landing gear blocks jump"), Route->RequestJump());
    Ship->SetGearDown(false);
    TestFalse(TEXT("low altitude still blocks jump"), Route->RequestJump());

    const FVector Departure = Surface->GetActorTransform().TransformPosition(FVector(0.0, 0.0, 350000.0));
    Ship->SetActorLocation(Departure, false, nullptr, ETeleportType::TeleportPhysics);
    if (!TestTrue(TEXT("clearance and retracted gear permit jump"), Route->RequestJump())) return false;
    for (int32 Step = 0; Step < 25; ++Step)
        Route->TickComponent(0.1f, LEVELTICK_All, nullptr);
    Route->TickComponent(1.0f, LEVELTICK_All, nullptr);

    TestEqual(TEXT("physical destination is Mars"), Surface->WorldId, FString(TEXT("mars")));
    TestEqual(TEXT("navigation destination is Mars"), Navigation->GetNavigationState().CurrentWorldId,
        FString(TEXT("mars")));
    TestEqual(TEXT("ship remains flyable after arrival"), Navigation->GetNavigationState().Phase,
        ESPTravelPhase::Flight);
    TestTrue(TEXT("pilot still controls ship after arrival"), Pilot->GetPawn() == Ship);

    // Arrival is only useful if the same physical ship can return to a safe
    // surface and complete touchdown. Search a small deterministic neighborhood
    // rather than assuming the procedural north-pole sample is a level pad.
    bool bLandingRequested = false;
    const FVector Center = Surface->GetPlanetCenterWorld();
    for (int32 X = -2; X <= 2 && !bLandingRequested; ++X)
    {
        for (int32 Y = -2; Y <= 2 && !bLandingRequested; ++Y)
        {
            const FVector Probe = Surface->GetActorLocation() + FVector(X * 5000.0, Y * 5000.0, 0.0);
            const FVector Radial = (Probe - Center).GetSafeNormal();
            const float ElevationMeters = Surface->SampleAtWorldLocation(Probe).ElevationMeters;
            const FVector Ground = Center + Radial * (1800000.0 + ElevationMeters * 100.0);
            Ship->SetActorLocation(Ground + Radial * 15000.0, false, nullptr, ETeleportType::TeleportPhysics);
            Ship->SetActorRotation(FRotationMatrix::MakeFromXZ(FVector::ForwardVector, Radial).Rotator());
            Ship->ResetMotionAfterWarp();
            Ship->SetGearDown(false);
            bLandingRequested = Ship->RequestSurfaceLanding();
        }
    }
    if (!TestTrue(TEXT("Mars offers a physically safe landing after hyperjump"), bLandingRequested)) return false;
    TestTrue(TEXT("landing deploys gear without a separate key press"), Ship->bGearDown);
    for (int32 Step = 0; Step < 50 && Ship->bFlying; ++Step) Ship->Tick(0.1f);
    TestFalse(TEXT("touchdown returns the ship to a landed physical state"), Ship->bFlying);
    return true;
}

#endif
