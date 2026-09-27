#include "SPSocietySimulationComponent.h"
#include "SPStoryCampaignComponent.h"
#include "SPFieldSurveyComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPrototypeCheckpointRoundTripTest,
    "SpacePatriot.Prototype.SystemCheckpointRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPPrototypeCheckpointRoundTripTest::RunTest(const FString& Parameters)
{
    const FString SocietySlot = TEXT("SP_TestSociety_Journey");
    const FString CampaignSlot = TEXT("SP_TestCampaign_Journey");
    const FString SurveySlot = TEXT("SP_TestSurvey_Journey");
    struct FDeleteTestSlots
    {
        TArray<FString> Slots;
        ~FDeleteTestSlots()
        {
            for (const FString& Slot : Slots) UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }
    } Cleanup{{SocietySlot, CampaignSlot, SurveySlot}};

    USPSocietySimulationComponent* Society = NewObject<USPSocietySimulationComponent>(GetTransientPackage());
    USPStoryCampaignComponent* Campaign = NewObject<USPStoryCampaignComponent>(GetTransientPackage());
    USPFieldSurveyComponent* Survey = NewObject<USPFieldSurveyComponent>(GetTransientPackage());
    Society->SaveSlot = SocietySlot;
    Campaign->SaveSlot = CampaignSlot;
    Survey->SaveSlot = SurveySlot;
    Society->bAutoSave = false;
    Campaign->bAutoSave = false;
    Survey->bAutoSave = false;
    Campaign->Society = Society;
    if (!TestTrue(TEXT("society initializes"), Society->InitializeSociety(false)) ||
        !TestTrue(TEXT("campaign initializes"), Campaign->InitializeCampaign(false)) ||
        !TestTrue(TEXT("survey initializes"), Survey->InitializeSurveys(false))) return false;

    const int32 StartingSpares = Society->GetPlayerSupply(TEXT("spares"));
    if (!TestTrue(TEXT("inventory changes"), Society->AddPlayerSupply(TEXT("spares"), 3)) ||
        !TestTrue(TEXT("case starts"), Campaign->StartQuest(TEXT("water"))) ||
        !TestTrue(TEXT("survey site activates"), Survey->ActivateWorld(TEXT("earth"), FVector::ZeroVector)) ||
        !TestTrue(TEXT("on-foot arrival progresses"), Survey->UpdatePlayerContext(FVector::ZeroVector, true, false))) return false;
    if (!TestTrue(TEXT("society checkpoint saved"), Society->SaveSociety()) ||
        !TestTrue(TEXT("campaign checkpoint saved"), Campaign->SaveCampaign()) ||
        !TestTrue(TEXT("survey checkpoint saved"), Survey->SaveSurveys())) return false;

    // These later live changes must not replace the explicit K checkpoint.
    TestTrue(TEXT("live inventory changes after checkpoint"), Society->AddPlayerSupply(TEXT("spares"), 5));
    TestTrue(TEXT("live campaign advances after checkpoint"), Campaign->Choose(TEXT("water"), TEXT("begin")));
    TestTrue(TEXT("live survey scans after checkpoint"), Survey->NotifyWorldScanned(TEXT("earth"), true));

    if (!TestTrue(TEXT("society reloads first"), Society->InitializeSociety(true)) ||
        !TestTrue(TEXT("campaign reloads second"), Campaign->InitializeCampaign(true)) ||
        !TestTrue(TEXT("survey reloads third"), Survey->InitializeSurveys(true))) return false;
    TestEqual(TEXT("inventory returns to checkpoint"), Society->GetPlayerSupply(TEXT("spares")), StartingSpares + 3);
    FSPStoryNodeView Node;
    TestTrue(TEXT("campaign node exists after load"), Campaign->GetCurrentNode(TEXT("water"), Node));
    TestEqual(TEXT("campaign returns to opening dialogue"), Node.Type, FString(TEXT("dialogue")));
    TestTrue(TEXT("survey rebinds to restored world and site"), Survey->ActivateWorld(TEXT("earth"), FVector(1000, 0, 0)));
    FSPFieldSurveyStatus Status;
    TestTrue(TEXT("survey status exists after load"), Survey->GetCurrentStatus(Status));
    TestEqual(TEXT("survey phase returns to pre-scan"), Status.Phase, FString(TEXT("scan")));
    TestFalse(TEXT("site rebind does not repeat arrival"), Survey->UpdatePlayerContext(FVector(1000, 0, 0), true, false));
    TestEqual(TEXT("survey journal survives rebind"), Survey->GetJournal(TEXT("earth")).Num(), 1);
    return true;
}
#endif
