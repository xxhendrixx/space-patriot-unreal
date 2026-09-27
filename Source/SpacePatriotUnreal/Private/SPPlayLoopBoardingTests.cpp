#include "SPPlayLoopDirector.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SPFlightPawn.h"
#include "SPHyperjumpRouteComponent.h"
#include "SPWorldSurface.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

namespace
{
    /** Exercise possession in a game world without changing a saved editor map. */
    struct FScopedBoardingWorld
    {
        UWorld* World = nullptr;

        FScopedBoardingWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("BoardingTest")),
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

        ~FScopedBoardingWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPlayLoopBoardingTest,
    "SpacePatriot.PlayLoop.BoardAndLaunchPhysicalShip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPPlayLoopBoardingTest::RunTest(const FString& Parameters)
{
    FScopedBoardingWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;

    ASPWorldSurface* Surface = World->SpawnActor<ASPWorldSurface>();
    ACharacter* GroundPawn = World->SpawnActor<ACharacter>();
    APlayerController* Player = World->SpawnActor<APlayerController>();
    ASPPlayLoopDirector* Director = World->SpawnActor<ASPPlayLoopDirector>();
    if (!TestNotNull(TEXT("Earth surface"), Surface) ||
        !TestNotNull(TEXT("ground character"), GroundPawn) ||
        !TestNotNull(TEXT("player controller"), Player) ||
        !TestNotNull(TEXT("physical journey director"), Director)) return false;
    if (!TestTrue(TEXT("original Earth profile loads"), Surface->ActivateWorld(TEXT("earth")))) return false;

    Director->ShipClass = ASPFlightPawn::StaticClass();
    Director->ParkedShipLocation = FVector(0.0, 0.0, 200.0);
    Director->BoardingOffsetLocal = FVector(-300.0, -800.0, -170.0);
    Director->BoardingRangeCm = 1200.0f;
    const FVector Hatch = Director->ParkedShipLocation + Director->BoardingOffsetLocal;
    GroundPawn->SetActorLocation(Hatch + FVector(1300.0, 0.0, 0.0));
    Player->Possess(GroundPawn);
    if (!TestTrue(TEXT("controller starts on foot"), Player->GetPawn() == GroundPawn)) return false;

    Director->DispatchBeginPlay();
    ASPFlightPawn* Ship = Director->Ship;
    if (!TestNotNull(TEXT("director spawns a physical flyable ship"), Ship)) return false;
    if (!TestTrue(TEXT("ship begins parked, unpossessed"), !Ship->bFlying && Ship->GetController() == nullptr))
        return false;
    if (!TestTrue(TEXT("travel route initializes for the spawned ship"),
        Director->HyperjumpRoute && Director->HyperjumpRoute->GetNavigationComponent() != nullptr &&
        Ship->PlanetCenterCm.Equals(Surface->GetPlanetCenterWorld(), 0.1))) return false;

    TestFalse(TEXT("interaction outside boarding range does not possess the ship"), Director->TryBoard());
    TestTrue(TEXT("player stays on foot outside boarding range"), Player->GetPawn() == GroundPawn);
    GroundPawn->SetActorLocation(Hatch + FVector(-600.0, -600.0, 90.0));
    if (!TestTrue(TEXT("interaction at an ordinary nearby spawn boards the physical ship"), Director->TryBoard())) return false;
    TestTrue(TEXT("controller now pilots the same spawned ship"), Player->GetPawn() == Ship);
    TestTrue(TEXT("on-foot character is hidden while boarded"), GroundPawn->IsHidden());
    TestFalse(TEXT("on-foot character no longer collides while boarded"), GroundPawn->GetActorEnableCollision());
    TestTrue(TEXT("boarded ship can launch under power"), Ship->Launch());
    TestTrue(TEXT("launch changes the physical ship to flight"), Ship->bFlying);
    return true;
}

#endif
