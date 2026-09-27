#include "SPFieldSurveyComponent.h"
#include "SPStoryCampaignComponent.h"
#include "SPSocietySimulationComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedFieldStoryWorld
    {
        UWorld* World = nullptr;

        FScopedFieldStoryWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("FieldStoryTest")),
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

        ~FScopedFieldStoryWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPFieldStoryCampaignIntegrationTest,
    "SpacePatriot.PlayLoop.FieldSampleAdvancesCampaign",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPFieldStoryCampaignIntegrationTest::RunTest(const FString& Parameters)
{
    FScopedFieldStoryWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;

    AActor* Host = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("shared world runtime"), Host)) return false;
    USPSocietySimulationComponent* Society = NewObject<USPSocietySimulationComponent>(Host);
    USPStoryCampaignComponent* Campaign = NewObject<USPStoryCampaignComponent>(Host);
    USPFieldSurveyComponent* Survey = NewObject<USPFieldSurveyComponent>(Host);
    Host->AddInstanceComponent(Society);
    Host->AddInstanceComponent(Campaign);
    Host->AddInstanceComponent(Survey);
    Society->bAutoSave = false;
    Campaign->bAutoSave = false;
    Campaign->Society = Society;
    Survey->bAutoSave = false;
    if (!TestTrue(TEXT("society initialized"), Society->InitializeSociety(false)) ||
        !TestTrue(TEXT("campaign initialized"), Campaign->InitializeCampaign(false)) ||
        !TestTrue(TEXT("survey initialized"), Survey->InitializeSurveys(false))) return false;

    if (!TestTrue(TEXT("orchard case starts"), Campaign->StartQuest(TEXT("orchard"))) ||
        !TestTrue(TEXT("opening briefing completes"), Campaign->Choose(TEXT("orchard"), TEXT("begin")))) return false;
    FSPStoryNodeView Node;
    if (!TestTrue(TEXT("sample objective active"), Campaign->GetCurrentNode(TEXT("orchard"), Node))) return false;
    TestEqual(TEXT("objective kind"), Node.ObjectiveKind, FString(TEXT("sample")));
    const FString SampleNodeId = Node.Id;
    if (!TestTrue(TEXT("survey site activates"), Survey->ActivateWorld(Node.WorldId, FVector::ZeroVector)) ||
        !TestTrue(TEXT("player reaches the site on foot"), Survey->UpdatePlayerContext(FVector::ZeroVector, true, false)) ||
        !TestTrue(TEXT("scanner records the site"), Survey->NotifyWorldScanned(Node.WorldId, true))) return false;
    TestFalse(TEXT("invalid sampler does not advance the case"),
        Survey->NotifyFieldSample(Node.WorldId, TEXT("station-cargo"), true));
    TestTrue(TEXT("case stays at sample objective"), Campaign->GetCurrentNode(TEXT("orchard"), Node));
    TestEqual(TEXT("objective unchanged after invalid sampler"), Node.Id, SampleNodeId);
    if (!TestTrue(TEXT("valid field sample is collected"),
        Survey->NotifyFieldSample(Node.WorldId, TEXT("surface-sampler"), true))) return false;
    if (!TestTrue(TEXT("case advances from real field sample"),
        Campaign->GetCurrentNode(TEXT("orchard"), Node))) return false;
    TestEqual(TEXT("next node is report dialogue"), Node.Type, FString(TEXT("dialogue")));
    TestNotEqual(TEXT("objective was cleared"), Node.Id, SampleNodeId);
    TestFalse(TEXT("sample cannot be replayed for progress"),
        Survey->NotifyFieldSample(Node.WorldId, TEXT("surface-sampler"), true));
    return true;
}

#endif
