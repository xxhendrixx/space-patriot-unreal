#include "SPPlayLoopDirector.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SPFieldSurveyComponent.h"
#include "SPFlightPawn.h"
#include "SPSocietySimulationComponent.h"
#include "SPWorldSurface.h"

#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedSurveyWorld
    {
        UWorld* World = nullptr;

        FScopedSurveyWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("SurveyLoopTest")),
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

        ~FScopedSurveyWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPlayLoopSurveyTest,
    "SpacePatriot.PlayLoop.OnFootScanAndSample",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPPlayLoopSurveyTest::RunTest(const FString& Parameters)
{
    FScopedSurveyWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;

    ASPWorldSurface* Surface = World->SpawnActor<ASPWorldSurface>();
    AActor* Runtime = World->SpawnActor<AActor>();
    ACharacter* GroundPawn = World->SpawnActor<ACharacter>();
    APlayerController* Player = World->SpawnActor<APlayerController>();
    ASPPlayLoopDirector* Director = World->SpawnActor<ASPPlayLoopDirector>();
    if (!TestNotNull(TEXT("surface"), Surface) || !TestNotNull(TEXT("system host"), Runtime) ||
        !TestNotNull(TEXT("ground pawn"), GroundPawn) || !TestNotNull(TEXT("player"), Player) ||
        !TestNotNull(TEXT("director"), Director)) return false;
    if (!TestTrue(TEXT("Earth loads"), Surface->ActivateWorld(TEXT("earth")))) return false;

    USPFieldSurveyComponent* Survey = NewObject<USPFieldSurveyComponent>(Runtime);
    USPSocietySimulationComponent* Society = NewObject<USPSocietySimulationComponent>(Runtime);
    Survey->bAutoLoad = false;
    Survey->bAutoSave = false;
    Society->bAutoLoad = false;
    Society->bAutoSave = false;
    Runtime->AddInstanceComponent(Survey);
    Runtime->AddInstanceComponent(Society);
    Survey->RegisterComponent();
    Society->RegisterComponent();
    if (!TestTrue(TEXT("survey state initializes"), Survey->InitializeSurveys(false)) ||
        !TestTrue(TEXT("inventory state initializes"), Society->InitializeSociety(false))) return false;

    Director->ShipClass = ASPFlightPawn::StaticClass();
    Director->ParkedShipLocation = FVector(0.0, 0.0, 200.0);
    GroundPawn->SetActorLocation(FVector(40000.0, 0.0, 100.0));
    Player->Possess(GroundPawn);
    Director->DispatchBeginPlay();
    if (!TestNotNull(TEXT("physical ship spawned"), Director->Ship.Get())) return false;
    TestTrue(TEXT("plain B scanner is registered for the live director"), Director->InputComponent &&
        Director->InputComponent->KeyBindings.ContainsByPredicate([](const FInputKeyBinding& Binding)
        {
            return Binding.Chord.Key == EKeys::B && !Binding.Chord.bShift && Binding.KeyEvent == IE_Pressed;
        }));
    TestTrue(TEXT("Shift+B sampler is a distinct live input chord"), Director->InputComponent &&
        Director->InputComponent->KeyBindings.ContainsByPredicate([](const FInputKeyBinding& Binding)
        {
            return Binding.Chord.Key == EKeys::B && Binding.Chord.bShift && Binding.KeyEvent == IE_Pressed;
        }));
    const FInputChord PlainScan(EKeys::B, false, false, false, false);
    const FInputChord ShiftedSample(EKeys::B, true, false, false, false);
    TestTrue(TEXT("Shift+B masks plain B in Unreal input resolution"),
        ShiftedSample.GetRelationship(PlainScan) == FInputChord::ERelationshipType::Masks);

    FSPFieldSurveyStatus Status;
    TestFalse(TEXT("far scan cannot complete site survey"), Director->TrySurveyAction(false));
    TestTrue(TEXT("survey remains at arrival phase"), Survey->GetCurrentStatus(Status) &&
        Status.Phase == TEXT("arrival"));
    GroundPawn->SetActorLocation(FVector(-300.0, -800.0, 100.0));
    if (!TestTrue(TEXT("nearby on-foot scan advances the live survey"), Director->TrySurveyAction(false))) return false;
    TestTrue(TEXT("scan now asks for a sample"), Survey->GetCurrentStatus(Status) &&
        Status.Phase == TEXT("sample"));
    TestEqual(TEXT("scanner alone does not award cargo"), Society->GetPlayerSupply(TEXT("sample")), 0);
    GroundPawn->SetActorLocation(FVector(40000.0, 0.0, 100.0));
    TestFalse(TEXT("out-of-range sampler cannot award cargo"), Director->TrySurveyAction(true));
    TestEqual(TEXT("rejected sample does not change inventory"), Society->GetPlayerSupply(TEXT("sample")), 0);
    GroundPawn->SetActorLocation(FVector(-300.0, -800.0, 100.0));
    if (!TestTrue(TEXT("sampler archives report"), Director->TrySurveyAction(true))) return false;
    TestTrue(TEXT("report is complete"), Survey->GetCurrentStatus(Status) && Status.bComplete);
    TestEqual(TEXT("sample appears in the economy inventory"), Society->GetPlayerSupply(TEXT("sample")), 1);
    TestFalse(TEXT("completed report cannot be farmed"), Director->TrySurveyAction(true));
    TestEqual(TEXT("duplicate sample awards no extra cargo"), Society->GetPlayerSupply(TEXT("sample")), 1);
    Player->Possess(Director->Ship);
    TestFalse(TEXT("scanner does not act while piloting"), Director->TrySurveyAction(false));
    return true;
}

#endif
