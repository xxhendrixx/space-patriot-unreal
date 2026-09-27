#include "SPCockpitMFDWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SPFlightPawn.h"
#include "SPHyperjumpRouteComponent.h"
#include "SPPlayLoopDirector.h"
#include "SPWorldSurface.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedMFDNavigationWorld
    {
        UWorld* World = nullptr;

        FScopedMFDNavigationWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("MFDNavigationTest")),
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

        ~FScopedMFDNavigationWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPCockpitMFDNavigationTest,
    "SpacePatriot.Cockpit.MFDNavigationControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPCockpitMFDNavigationTest::RunTest(const FString& Parameters)
{
    FScopedMFDNavigationWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;

    ASPWorldSurface* Surface = World->SpawnActor<ASPWorldSurface>();
    ASPFlightPawn* Ship = World->SpawnActor<ASPFlightPawn>();
    APlayerController* Pilot = World->SpawnActor<APlayerController>();
    ASPPlayLoopDirector* Director = World->SpawnActor<ASPPlayLoopDirector>();
    if (!TestNotNull(TEXT("source surface"), Surface) ||
        !TestNotNull(TEXT("live ship"), Ship) ||
        !TestNotNull(TEXT("pilot"), Pilot) ||
        !TestNotNull(TEXT("route director"), Director)) return false;
    if (!TestTrue(TEXT("Earth source world activates"), Surface->ActivateWorld(TEXT("earth")))) return false;
    Director->Ship = Ship;
    Ship->SetActorLocation(FVector(0.0, 0.0, 350000.0), false, nullptr, ETeleportType::TeleportPhysics);
    Ship->bFlying = true;
    Ship->bPowered = true;
    Ship->bGearDown = true;
    ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    if (!TestNotNull(TEXT("widget test local player"), LocalPlayer)) return false;
    // Native UUserWidget::Initialize requires a valid local-player context.
    // Link the isolated test controller without starting an Editor viewport.
    Pilot->Player = LocalPlayer;
    LocalPlayer->PlayerController = Pilot;
    Pilot->Possess(Ship);
    if (!TestTrue(TEXT("route authority configures for this ship"),
        Director->HyperjumpRoute->Configure(Ship, Surface))) return false;

    USPCockpitMFDWidget* MFD = NewObject<USPCockpitMFDWidget>(Pilot);
    if (!TestNotNull(TEXT("cockpit MFD"), MFD)) return false;
    MFD->SetOwningPlayer(Pilot);
    if (!TestTrue(TEXT("MFD widget initializes"), MFD->Initialize())) return false;
    MFD->SetShip(Ship);
    UButton* DestButton = Cast<UButton>(MFD->GetWidgetFromName(TEXT("MFDDestination")));
    UButton* JumpButton = Cast<UButton>(MFD->GetWidgetFromName(TEXT("MFDJump")));
    UHorizontalBox* NavControls = Cast<UHorizontalBox>(MFD->GetWidgetFromName(TEXT("MFDNavControls")));
    if (!TestNotNull(TEXT("clickable destination button"), DestButton) ||
        !TestNotNull(TEXT("clickable jump button"), JumpButton) ||
        !TestNotNull(TEXT("navigation-only controls"), NavControls)) return false;

    Ship->RefreshMFD();
    TestTrue(TEXT("NAV names the physical departure world"),
        Ship->CockpitReadout->Text.ToString().Contains(TEXT("HERE EARTH")));
    TestTrue(TEXT("NAV shows no destination before selection"),
        Ship->CockpitReadout->Text.ToString().Contains(TEXT("DEST NONE")));
    TestEqual(TEXT("NAV buttons are visible on the NAV page"), NavControls->GetVisibility(), ESlateVisibility::Visible);

    DestButton->OnClicked.Broadcast();
    const FSPHyperjumpRouteStatus Selected = Director->HyperjumpRoute->GetRouteStatus();
    TestFalse(TEXT("DEST click selects a different source world"), Selected.Navigation.DestinationWorldId.IsEmpty());
    TestNotEqual(TEXT("destination differs from the current world"),
        Selected.Navigation.DestinationWorldId, Selected.Navigation.CurrentWorldId);
    TestTrue(TEXT("selected destination appears on the same cockpit screen"),
        Ship->CockpitReadout->Text.ToString().Contains(TEXT("DEST ")) &&
        Ship->CockpitReadout->Text.ToString().Contains(Selected.DestinationWorldName.ToUpper()));

    JumpButton->OnClicked.Broadcast();
    TestEqual(TEXT("gear interlock prevents the MFD click from starting a jump"),
        Director->HyperjumpRoute->GetRouteStatus().Navigation.Phase, ESPTravelPhase::Flight);
    TestTrue(TEXT("gear interlock reason is visible on the MFD"),
        Ship->CockpitReadout->Text.ToString().Contains(TEXT("Retract the landing gear")));

    Ship->SetGearDown(false);
    JumpButton->OnClicked.Broadcast();
    TestEqual(TEXT("same MFD button starts the real route once safe"),
        Director->HyperjumpRoute->GetRouteStatus().Navigation.Phase, ESPTravelPhase::JumpCharging);
    TestTrue(TEXT("MFD shows charging feedback"),
        Ship->CockpitReadout->Text.ToString().Contains(TEXT("Hyperdrive charging")));

    Ship->CycleMFDPage(1);
    MFD->SetReadout(Ship->CockpitReadout->Text, Ship->CockpitMFDBrightness);
    TestEqual(TEXT("route controls hide on systems page"), NavControls->GetVisibility(), ESlateVisibility::Collapsed);
    return true;
}

#endif
