#include "SPJourneySave.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SPFlightPawn.h"
#include "SPWorldSurface.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"

namespace
{
    struct FScopedJourneyWorld
    {
        UWorld* World = nullptr;

        FScopedJourneyWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("JourneySaveTest")),
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

        ~FScopedJourneyWorld()
        {
            if (!World) return;
            GEngine->ShutdownWorldNetDriver(World);
            World->DestroyWorld(true);
            World->SetPhysicsScene(nullptr);
            GEngine->DestroyWorldContext(World);
            World->RemoveFromRoot();
        }
    };

    struct FScopedJourneySlot
    {
        FString Name = TEXT("SPJourneyTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        ~FScopedJourneySlot() { UGameplayStatics::DeleteGameInSlot(Name, 0); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPJourneySaveRoundTripTest,
    "SpacePatriot.Travel.JourneySaveRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPJourneySaveRoundTripTest::RunTest(const FString& Parameters)
{
    FScopedJourneyWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;
    ASPWorldSurface* Surface = World->SpawnActor<ASPWorldSurface>();
    ASPFlightPawn* Ship = World->SpawnActor<ASPFlightPawn>();
    ACharacter* GroundPawn = World->SpawnActor<ACharacter>();
    APlayerController* Player = World->SpawnActor<APlayerController>();
    AActor* NavigationOwner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("surface"), Surface) || !TestNotNull(TEXT("ship"), Ship) ||
        !TestNotNull(TEXT("ground pawn"), GroundPawn) || !TestNotNull(TEXT("controller"), Player) ||
        !TestNotNull(TEXT("navigation owner"), NavigationOwner)) return false;

    USPTravelNavigationComponent* Navigation = NewObject<USPTravelNavigationComponent>(NavigationOwner);
    NavigationOwner->AddInstanceComponent(Navigation);
    Navigation->RegisterComponent();
    if (!TestTrue(TEXT("world catalog loads"), Navigation->LoadWorldCatalog()) ||
        !TestTrue(TEXT("Earth source world activates"), Surface->ActivateWorld(TEXT("earth")))) return false;
    FSPTravelNavigationState NavState = Navigation->CaptureSaveState();
    NavState.Phase = ESPTravelPhase::Landed;
    NavState.DestinationWorldId = TEXT("mars");
    if (!TestTrue(TEXT("landed navigation state restores"), Navigation->RestoreSaveState(NavState))) return false;

    Ship->SetActorLocation(FVector(4200.0, 1700.0, 300.0));
    GroundPawn->SetActorLocation(FVector(3800.0, 1600.0, 120.0));
    Ship->bFlying = false;
    Ship->bGearDown = true;
    Ship->FuelPercent = 61.0f;
    Ship->HeatPercent = 23.0f;
    Player->Possess(GroundPawn);

    FSPJourneyState Captured;
    if (!TestTrue(TEXT("captures coherent on-foot journey"),
        USPJourneySaveLibrary::CaptureJourney(Player, GroundPawn, Ship, Navigation, Captured))) return false;
    TestEqual(TEXT("saved world is Earth"), Captured.WorldId, FString(TEXT("earth")));
    TestEqual(TEXT("saved destination is Mars"), Captured.Navigation.DestinationWorldId, FString(TEXT("mars")));
    TestFalse(TEXT("player is on foot"), Captured.bPiloting);
    Captured.TimeOfDayHours = 22.75f;

    FScopedJourneySlot Slot;
    if (!TestTrue(TEXT("journey writes its own save slot"),
        USPJourneySaveLibrary::SaveJourney(Captured, Slot.Name))) return false;
    FSPJourneyState Loaded;
    if (!TestTrue(TEXT("journey loads from disk"),
        USPJourneySaveLibrary::LoadJourney(Slot.Name, Loaded))) return false;
    TestEqual(TEXT("fuel round trips"), Loaded.FuelPercent, 61.0f);
    TestEqual(TEXT("local time round trips"), Loaded.TimeOfDayHours, 22.75f);
    TestEqual(TEXT("ground position round trips"), Loaded.GroundPawnTransform.GetLocation(),
        Captured.GroundPawnTransform.GetLocation());

    FSPJourneyState Corrupt = Loaded;
    Corrupt.WorldId = TEXT("jupiter");
    TestFalse(TEXT("world/navigation mismatch is rejected"), USPJourneySaveLibrary::IsJourneyStateValid(Corrupt));
    Corrupt = Loaded;
    Corrupt.bFlying = true;
    TestFalse(TEXT("on-foot airborne state is rejected"), USPJourneySaveLibrary::IsJourneyStateValid(Corrupt));
    Corrupt = Loaded;
    Corrupt.Navigation.Phase = ESPTravelPhase::JumpTransit;
    TestFalse(TEXT("mid-jump state is rejected"), USPJourneySaveLibrary::IsJourneyStateValid(Corrupt));
    Corrupt = Loaded;
    Corrupt.Navigation.DestinationWorldId = TEXT("earth");
    TestFalse(TEXT("self-destination is rejected"), USPJourneySaveLibrary::IsJourneyStateValid(Corrupt));
    Corrupt = Loaded;
    Corrupt.TimeOfDayHours = 25.0f;
    TestFalse(TEXT("out-of-range local time is rejected"), USPJourneySaveLibrary::IsJourneyStateValid(Corrupt));
    TestFalse(TEXT("unsafe slot path is rejected"), USPJourneySaveLibrary::SaveJourney(Loaded, TEXT("../escape")));

    USPSJourneySaveGame* Unsupported = NewObject<USPSJourneySaveGame>();
    Unsupported->Version = 2;
    Unsupported->Journey = Loaded;
    if (!TestTrue(TEXT("test writes an unsupported version"),
        UGameplayStatics::SaveGameToSlot(Unsupported, Slot.Name, 0))) return false;
    FSPJourneyState Rejected;
    TestFalse(TEXT("unsupported save version is rejected"),
        USPJourneySaveLibrary::LoadJourney(Slot.Name, Rejected));

    NavState = Navigation->CaptureSaveState();
    NavState.CurrentWorldId = TEXT("mars");
    NavState.DestinationWorldId.Empty();
    NavState.Phase = ESPTravelPhase::Flight;
    if (!TestTrue(TEXT("Mars surface activates"), Surface->ActivateWorld(TEXT("mars"))) ||
        !TestTrue(TEXT("Mars navigation activates"), Navigation->RestoreSaveState(NavState))) return false;
    Ship->bFlying = true;
    Ship->FuelPercent = 11.0f;
    Ship->SetActorLocation(FVector(0.0, 0.0, 350000.0));
    GroundPawn->SetActorLocation(FVector(0.0, 0.0, 0.0));
    if (!TestTrue(TEXT("restore returns to saved world and pawn"),
        USPJourneySaveLibrary::RestoreJourney(Loaded, Player, GroundPawn, Ship, Surface, Navigation))) return false;
    TestEqual(TEXT("physical surface restored"), Surface->WorldId, FString(TEXT("earth")));
    TestEqual(TEXT("navigation world restored"), Navigation->CaptureSaveState().CurrentWorldId, FString(TEXT("earth")));
    TestTrue(TEXT("controller on foot again"), Player->GetPawn() == GroundPawn);
    TestTrue(TEXT("ship parked with gear down"), !Ship->bFlying && Ship->bGearDown);
    TestEqual(TEXT("ship fuel restored"), Ship->FuelPercent, 61.0f);
    TestEqual(TEXT("ship position restored"), Ship->GetActorLocation(), Captured.ShipTransform.GetLocation());

    Player->Possess(Ship);
    Ship->bFlying = true;
    Ship->bGearDown = false;
    NavState = Navigation->CaptureSaveState();
    NavState.Phase = ESPTravelPhase::Flight;
    if (!TestTrue(TEXT("flight navigation resumes"), Navigation->RestoreSaveState(NavState))) return false;
    FSPJourneyState Piloting;
    if (!TestTrue(TEXT("captures piloted flight"),
        USPJourneySaveLibrary::CaptureJourney(Player, GroundPawn, Ship, Navigation, Piloting))) return false;
    Player->Possess(GroundPawn);
    Ship->bFlying = false;
    Ship->bGearDown = true;
    if (!TestTrue(TEXT("flight restore re-possesses ship"),
        USPJourneySaveLibrary::RestoreJourney(Piloting, Player, GroundPawn, Ship, Surface, Navigation))) return false;
    TestTrue(TEXT("controller back in pilot seat"), Player->GetPawn() == Ship);
    TestTrue(TEXT("ground pawn hidden while piloting"), GroundPawn->IsHidden());
    TestTrue(TEXT("flight and retracted gear restored"), Ship->bFlying && !Ship->bGearDown);
    return true;
}

#endif
