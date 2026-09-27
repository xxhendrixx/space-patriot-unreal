#include "SPFlightPawn.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "SPHyperdriveVisualComponent.h"
#include "SPWorldSurface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPTravelPlayablePawnTest,
    "SpacePatriot.Travel.PlayablePawnJourney",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPTravelPlayablePawnTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && (Context.WorldType == EWorldType::Editor ||
                Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
            {
                World = Context.World();
                break;
            }
        }
    }
    if (!TestNotNull(TEXT("editor world"), World)) return false;

    ASPWorldSurface* Surface = World->SpawnActor<ASPWorldSurface>();
    ASPFlightPawn* Pawn = World->SpawnActor<ASPFlightPawn>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("world surface"), Surface) ||
        !TestNotNull(TEXT("flyable ship"), Pawn) ||
        !TestNotNull(TEXT("pilot controller"), Controller)) return false;

    Surface->SetFlags(RF_Transient);
    Pawn->SetFlags(RF_Transient);
    Controller->SetFlags(RF_Transient);
    Surface->WorldId = TEXT("earth");
    TestTrue(TEXT("Earth source surface activates"), Surface->RebuildSurface());
    Pawn->SetActorLocation(FVector(700000.0, 0.0, 500000.0));
    Pawn->SetTravelWorldSurface(Surface);
    Controller->Possess(Pawn);
    USPTravelNavigationComponent* Nav = Pawn->TravelNavigation;
    USPHyperdriveVisualComponent* Visual = Pawn->HyperdriveVisual;
    if (!TestNotNull(TEXT("native travel component inherited by pawn"), Nav) ||
        !TestNotNull(TEXT("native visual component inherited by pawn"), Visual))
    {
        Controller->Destroy(); Pawn->Destroy(); Surface->Destroy();
        return false;
    }
    TestTrue(TEXT("source destination catalog loads"), Nav->LoadWorldCatalog());
    TestTrue(TEXT("visual follows pawn navigation"), Visual->SetNavigationComponent(Nav));
    TestTrue(TEXT("visual mesh follows pawn camera"), Visual->BindToCamera(Pawn->FlightCamera));
    TestTrue(TEXT("controller is at flight controls"), Pawn->GetTravelContext().bPilotAtControls);
    TestTrue(TEXT("exterior altitude is measured from generated Earth surface"),
        Pawn->GetTravelContext().AltitudeKm > 2.0);
    TestTrue(TEXT("destination selected"), Nav->SelectDestination(TEXT("mars")));
    TestFalse(TEXT("landing gear blocks live charge"), Pawn->BeginHyperdriveJump());
    Pawn->SetGearDown(false);
    TestTrue(TEXT("Kestrel input action begins charge"), Pawn->BeginHyperdriveJump());
    Visual->RefreshVisuals(0.0f);
    TestEqual(TEXT("alignment brackets visible"), Visual->GetVisualStats().AlignmentMarkers, 36);
    for (int32 Index = 0; Index < 21; ++Index) Pawn->Tick(0.1f);
    TestEqual(TEXT("live resources and context advance into transit"),
        Nav->GetNavigationState().Phase, ESPTravelPhase::JumpTransit);
    TestTrue(TEXT("jump cost reaches pawn fuel"), Pawn->FuelPercent < 94.0f);
    Visual->RefreshVisuals(0.0f);
    TestEqual(TEXT("transit streaks visible"), Visual->GetVisualStats().StarStreaks, 72);
    for (int32 Index = 0; Index < 35; ++Index) Pawn->Tick(0.1f);
    TestEqual(TEXT("arrival activates destination source world"), Surface->WorldId, FString(TEXT("mars")));
    TestEqual(TEXT("navigation records arrival only after surface activation"),
        Nav->GetNavigationState().CurrentWorldId, FString(TEXT("mars")));
    TestEqual(TEXT("arrival returns to manual flight"), Nav->GetNavigationState().Phase, ESPTravelPhase::Flight);
    TestEqual(TEXT("arrival selects SCM"), Nav->GetNavigationState().DriveMode, ESPTravelDriveMode::SCM);
    TestTrue(TEXT("ship remains safely above destination surface"), Pawn->GetTravelContext().AltitudeKm > 2.0);
    TestTrue(TEXT("flight assist restored"), Pawn->bFlightAssist);
    Visual->RefreshVisuals(0.0f);
    TestFalse(TEXT("exterior arrival clears star geometry"), Visual->GetVisualStats().bEffectActive);
    Controller->UnPossess();
    Controller->Destroy(); Pawn->Destroy(); Surface->Destroy();
    return true;
}
#endif
