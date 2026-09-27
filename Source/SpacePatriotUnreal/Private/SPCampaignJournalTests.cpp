#include "SPPlayLoopWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "SPStoryCampaignComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPCampaignJournalTest,
    "SpacePatriot.PlayLoop.LiveCampaignJournal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPCampaignJournalTest::RunTest(const FString& Parameters)
{
    USPStoryCampaignComponent* Campaign = NewObject<USPStoryCampaignComponent>(GetTransientPackage());
    Campaign->bAutoSave = false;
    if (!TestTrue(TEXT("source campaign loads"), Campaign->InitializeCampaign(false))) return false;

    FString Journal = USPPlayLoopWidget::FormatJournal(Campaign);
    TestTrue(TEXT("available first case appears before accepting it"), Journal.Contains(TEXT("The Water Ledger")));
    TestTrue(TEXT("journal tells the player how to start"), Journal.Contains(TEXT("port board with M")));
    TestFalse(TEXT("no objective is invented before case starts"), Journal.Contains(TEXT("Destination: earth")));

    if (!TestTrue(TEXT("case can start"), Campaign->StartQuest(TEXT("water")))) return false;
    Journal = USPPlayLoopWidget::FormatJournal(Campaign);
    TestTrue(TEXT("current source dialogue is readable"), Journal.Contains(TEXT("Ivo Rook")));
    TestTrue(TEXT("source dialogue choice is readable"), Journal.Contains(TEXT("Take the case")));

    if (!TestTrue(TEXT("accept case"), Campaign->Choose(TEXT("water"), TEXT("begin")))) return false;
    Journal = USPPlayLoopWidget::FormatJournal(Campaign);
    TestTrue(TEXT("objective title follows graph transition"),
        Journal.Contains(TEXT("Read a city operations terminal on Earth")));
    TestTrue(TEXT("correct target world is shown"), Journal.Contains(TEXT("Destination: earth")));
    TestTrue(TEXT("objective progress is shown"), Journal.Contains(TEXT("[0 / 1]")));
    TestFalse(TEXT("old dialogue choice is no longer current"), Journal.Contains(TEXT("1. Take the case")));

    if (!TestTrue(TEXT("world interaction advances live campaign"),
        Campaign->SignalGameplay(TEXT("terminal"), TEXT("earth"), TEXT("journal-test-terminal")))) return false;
    Journal = USPPlayLoopWidget::FormatJournal(Campaign);
    TestTrue(TEXT("new lead replaces completed objective"), Journal.Contains(TEXT("Continue the investigation")));
    TestTrue(TEXT("source record appears in recent entries"), Journal.Contains(TEXT("The manifests are genuine")));
    TestFalse(TEXT("old objective no longer appears as active"),
        Journal.Contains(TEXT("Read a city operations terminal on Earth")));
    return true;
}
#endif
