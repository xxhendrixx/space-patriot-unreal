#include "SPFlightPawn.h"
#include "SPHyperjumpRouteComponent.h"
#include "SPPlayLoopDirector.h"
#include "SPPortTerminal.h"
#include "SPSocietySimulationComponent.h"
#include "SPTravelNavigationComponent.h"
#include "SPWorldSurface.h"
#include "SpacePatriotBlueprintBases.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedArrivalWorld
    {
        UWorld* World = nullptr;

        FScopedArrivalWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("ArrivalPlayLoopTest")),
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

        ~FScopedArrivalWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPArrivalPlayLoopTest,
    "SpacePatriot.PlayLoop.ArriveLandExitUsePortAndReboard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPArrivalPlayLoopTest::RunTest(const FString& Parameters)
{
    FScopedArrivalWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;

    ASPWorldSurface* Surface = World->SpawnActor<ASPWorldSurface>();
    ASPWorldRuntime* Runtime = World->SpawnActor<ASPWorldRuntime>();
    ACharacter* Character = World->SpawnActor<ACharacter>();
    APlayerController* Player = World->SpawnActor<APlayerController>();
    ASPPlayLoopDirector* Director = World->SpawnActor<ASPPlayLoopDirector>();
    ASPPortTerminal* RemotePort = World->SpawnActor<ASPPortTerminal>();
    if (!TestNotNull(TEXT("surface"), Surface) || !TestNotNull(TEXT("runtime"), Runtime) ||
        !TestNotNull(TEXT("ground pawn"), Character) || !TestNotNull(TEXT("player"), Player) ||
        !TestNotNull(TEXT("journey director"), Director) ||
        !TestNotNull(TEXT("remote port board"), RemotePort)) return false;
    RemotePort->bRemotePortProxy = true;
    RemotePort->SetActorHiddenInGame(true);
    RemotePort->SetActorEnableCollision(false);

    if (!TestTrue(TEXT("Earth surface activates"), Surface->ActivateWorld(TEXT("earth"))) ||
        !TestTrue(TEXT("source catalogs load"), Runtime->Systems->LoadSourceCatalogs()) ||
        !TestTrue(TEXT("Earth society activates"), Runtime->Systems->SetActiveWorld(TEXT("earth")))) return false;
    USPSocietySimulationComponent* Society = NewObject<USPSocietySimulationComponent>(Runtime);
    Runtime->AddInstanceComponent(Society);
    Society->bAutoSave = false;
    if (!TestTrue(TEXT("society services initialize"), Society->InitializeSociety(false))) return false;

    Director->ShipClass = ASPFlightPawn::StaticClass();
    Director->ParkedShipLocation = FVector(0.0, 0.0, 200.0);
    const FVector Hatch = Director->ParkedShipLocation + Director->BoardingOffsetLocal;
    Character->SetActorLocation(Hatch + FVector(0.0, -200.0, 100.0));
    Player->Possess(Character);
    Director->DispatchBeginPlay();
    ASPFlightPawn* Ship = Director->Ship;
    if (!TestNotNull(TEXT("same physical ship exists"), Ship) ||
        !TestTrue(TEXT("player boards ship"), Director->TryBoard()) ||
        !TestTrue(TEXT("ship launches"), Ship->Launch())) return false;
    Director->Tick(0.016f);
    USPHyperjumpRouteComponent* Route = Director->HyperjumpRoute;
    USPTravelNavigationComponent* Navigation = Route ? Route->GetNavigationComponent() : nullptr;
    if (!TestNotNull(TEXT("live route"), Route) || !TestNotNull(TEXT("navigation"), Navigation)) return false;
    if (!TestTrue(TEXT("Mars selected"), Route->SelectDestination(TEXT("mars")))) return false;

    Ship->SetGearDown(false);
    Ship->SetActorLocation(Surface->GetActorTransform().TransformPosition(FVector(0.0, 0.0, 350000.0)),
        false, nullptr, ETeleportType::TeleportPhysics);
    if (!TestTrue(TEXT("jump starts on boarded physical ship"), Route->RequestJump())) return false;
    for (int32 Step = 0; Step < 25; ++Step) Route->TickComponent(0.1f, LEVELTICK_All, nullptr);
    Route->TickComponent(1.0f, LEVELTICK_All, nullptr);
    if (!TestEqual(TEXT("destination surface is Mars"), Surface->WorldId, FString(TEXT("mars"))) ||
        !TestEqual(TEXT("runtime society is on Mars"), Runtime->Systems->ActiveWorldId,
            FString(TEXT("mars")))) return false;

    // Try nearby physical touchdown sites until the real footprint checks
    // accept one. No navigation state or landing flag is forced by the test.
    bool bLandingRequested = false;
    const FVector Center = Surface->GetPlanetCenterWorld();
    for (int32 X = -2; X <= 2 && !bLandingRequested; ++X)
    {
        for (int32 Y = -2; Y <= 2 && !bLandingRequested; ++Y)
        {
            const FVector Probe = Surface->GetActorLocation() + FVector(X * 5000.0, Y * 5000.0, 0.0);
            const FVector Radial = (Probe - Center).GetSafeNormal();
            const FVector Ground = Center + Radial *
                (1800000.0 + Surface->SampleAtWorldLocation(Probe).ElevationMeters * 100.0);
            Ship->SetActorLocation(Ground + Radial * 15000.0, false, nullptr, ETeleportType::TeleportPhysics);
            Ship->SetActorRotation(FRotationMatrix::MakeFromXZ(FVector::ForwardVector, Radial).Rotator());
            Ship->ResetMotionAfterWarp();
            Ship->SetGearDown(false);
            bLandingRequested = Ship->RequestSurfaceLanding();
        }
    }
    if (!TestTrue(TEXT("real Mars landing request succeeds"), bLandingRequested)) return false;
    for (int32 Step = 0; Step < 50 && Ship->bFlying; ++Step) Ship->Tick(0.1f);
    if (!TestFalse(TEXT("ship has touched down"), Ship->bFlying)) return false;
    Director->Tick(0.016f);
    if (!TestTrue(TEXT("pilot exits onto Mars terrain"), Director->TryDisembark()) ||
        !TestTrue(TEXT("controller possesses ground pawn"), Player->GetPawn() == Character)) return false;
    TestFalse(TEXT("on-foot pawn is visible"), Character->IsHidden());
    TestTrue(TEXT("on-foot pawn can collide"), Character->GetActorEnableCollision());

    RemotePort->Tick(0.25f);
    if (!TestFalse(TEXT("remote port board appears beside landed ship"), RemotePort->IsHidden())) return false;
    TestTrue(TEXT("remote port remains within a short walk of the hatch"),
        FVector::Distance(Character->GetActorLocation(), RemotePort->GetActorLocation()) < 4000.0f);
    Character->SetActorLocation(RemotePort->GetActorLocation() + Ship->GetActorUpVector() * 100.0f,
        false, nullptr, ETeleportType::TeleportPhysics);
    const int32 BeforeCredits = Society->GetPlayerCredits();
    if (!TestTrue(TEXT("Mars market selects organics"), RemotePort->TryCycleCargoGood())) return false;
    if (!TestTrue(TEXT("on-foot player buys cargo at Mars board"), RemotePort->TryBuyCargo())) return false;
    TestTrue(TEXT("purchase uses the actual society ledger"), Society->GetPlayerCredits() < BeforeCredits);

    Character->SetActorLocation(Ship->GetActorTransform().TransformPosition(Director->BoardingOffsetLocal)
        + Ship->GetActorUpVector() * 100.0f, false, nullptr, ETeleportType::TeleportPhysics);
    if (!TestTrue(TEXT("player returns to same ship after using port"), Director->TryBoard())) return false;
    TestTrue(TEXT("pilot controls original ship again"), Player->GetPawn() == Ship);
    return true;
}

#endif
