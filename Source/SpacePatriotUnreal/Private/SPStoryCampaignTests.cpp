#include "SPStoryCampaignComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "SPSocietySimulationComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPStoryCampaignGraphTest,
    "SpacePatriot.StoryCampaign.FullGraph",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPStoryCampaignGraphTest::RunTest(const FString& Parameters)
{
    static const TArray<FString> Order = {
        TEXT("water"), TEXT("orchard"), TEXT("furnace"), TEXT("missing"), TEXT("relay"),
        TEXT("mirror"), TEXT("witness"), TEXT("assembly"), TEXT("patriot")
    };
    for (int32 Branch = 0; Branch < 2; ++Branch)
    {
        USPSocietySimulationComponent* Society = NewObject<USPSocietySimulationComponent>(GetTransientPackage());
        USPStoryCampaignComponent* Campaign = NewObject<USPStoryCampaignComponent>(GetTransientPackage());
        if (!TestTrue(TEXT("society starts"), Society->InitializeSociety(false))) return false;
        Society->bAutoSave = false;
        Society->State->HoldOrganics = 100;
        Society->State->HoldOre = 100;
        Society->State->HoldCrystal = 100;
        Society->State->HoldCapacity = 400;
        Campaign->bAutoSave = false;
        Campaign->Society = Society;
        if (!TestTrue(TEXT("source Storyworks graph loads"), Campaign->InitializeCampaign(false))) return false;
        TestEqual(TEXT("all nine original cases"), Campaign->GetQuestCount(), 9);
        TestEqual(TEXT("all ninety original nodes"), Campaign->GetNodeCount(), 90);
        TestEqual(TEXT("nine case views"), Campaign->GetQuestSummaries().Num(), 9);
        TestFalse(TEXT("prerequisite locks a late case"), Campaign->StartQuest(TEXT("patriot")));
        TestFalse(TEXT("prerequisite locks witness"), Campaign->StartQuest(TEXT("witness")));
        int32 Milestones = 0;
        int32 CreditsFromRewards = 0;

        for (const FString& QuestId : Order)
        {
            if (!TestTrue(*FString::Printf(TEXT("%s begins after prerequisites"), *QuestId), Campaign->StartQuest(QuestId))) return false;
            FSPStoryNodeView Node;
            if (!TestTrue(TEXT("brief is available"), Campaign->GetCurrentNode(QuestId, Node))) return false;
            TestEqual(TEXT("brief is dialogue"), Node.Type, FString(TEXT("dialogue")));
            TestEqual(TEXT("brief has opening choice"), Node.Choices.Num(), 1);
            TestTrue(TEXT("beginning choice works"), Campaign->Choose(QuestId, TEXT("begin")));
            for (int32 Step = 0; Step < 3; ++Step)
            {
                if (!TestTrue(TEXT("objective is available"), Campaign->GetCurrentNode(QuestId, Node))) return false;
                TestEqual(TEXT("case step is gameplay objective"), Node.Type, FString(TEXT("objective")));
                TestFalse(TEXT("objective has a concrete world"), Node.WorldId.IsEmpty());
                const FString BeforeNode = Node.Id;
                if (Node.ObjectiveKind == TEXT("delivery"))
                {
                    TestFalse(TEXT("delivery cannot skip cargo transaction"), Campaign->SignalGameplay(TEXT("delivery"), Node.WorldId));
                    TestFalse(TEXT("delivery rejects wrong destination"), Campaign->DeliverObjective(QuestId, TEXT("mercury"), true));
                    TestFalse(TEXT("delivery rejects undocked cargo"), Campaign->DeliverObjective(QuestId, Node.WorldId, false));
                    const int32 CargoBefore = Society->GetHoldCargo(Node.Good);
                    FSPMarketRecord MarketBefore;
                    Society->GetMarket(Node.WorldId, MarketBefore);
                    if (!TestTrue(TEXT("cargo-backed delivery completes"), Campaign->DeliverObjective(QuestId, Node.WorldId, true))) return false;
                    TestEqual(TEXT("delivery consumes exact cargo units"), Society->GetHoldCargo(Node.Good), CargoBefore - Node.Units);
                    FSPMarketRecord MarketAfter;
                    Society->GetMarket(Node.WorldId, MarketAfter);
                    const float BeforeStock = Node.Good == TEXT("ore") ? MarketBefore.Ore :
                        Node.Good == TEXT("crystal") ? MarketBefore.Crystal : MarketBefore.Organics;
                    const float AfterStock = Node.Good == TEXT("ore") ? MarketAfter.Ore :
                        Node.Good == TEXT("crystal") ? MarketAfter.Crystal : MarketAfter.Organics;
                    TestEqual(TEXT("delivered cargo enters destination market"), AfterStock, BeforeStock + Node.Units);
                }
                else
                {
                    const FString EventId = QuestId + TEXT(":") + FString::FromInt(Step);
                    TestTrue(TEXT("gameplay interaction signal accepted"), Campaign->SignalGameplay(Node.ObjectiveKind, Node.WorldId, EventId));
                    TestFalse(TEXT("event ID cannot be replayed"), Campaign->SignalGameplay(Node.ObjectiveKind, Node.WorldId, EventId));
                }
                ++Milestones;
                if (!TestTrue(TEXT("report follows objective"), Campaign->GetCurrentNode(QuestId, Node))) return false;
                TestEqual(TEXT("report is dialogue"), Node.Type, FString(TEXT("dialogue")));
                TestNotEqual(TEXT("objective advanced"), Node.Id, BeforeNode);
                TestTrue(TEXT("report continues"), Campaign->Choose(QuestId, TEXT("continue")));
            }
            if (!TestTrue(TEXT("decision node reached"), Campaign->GetCurrentNode(QuestId, Node))) return false;
            TestEqual(TEXT("decision has two outcomes"), Node.Choices.Num(), 2);
            const FString ChoiceId = Branch == 0 ? TEXT("choice0") : TEXT("choice1");
            const int32 CreditsBefore = Society->GetPlayerCredits();
            if (!TestTrue(TEXT("branch choice resolves"), Campaign->Choose(QuestId, ChoiceId))) return false;
            FSPStoryNodeView Ending;
            TestTrue(TEXT("ending remains queryable"), Campaign->GetCurrentNode(QuestId, Ending));
            TestEqual(TEXT("ending is an end node"), Ending.Type, FString(TEXT("end")));
            TestTrue(TEXT("reward is recorded once"), Campaign->State->Rewards.ContainsByPredicate(
                [&](const FSPStoryReward& Reward) { return Reward.QuestId == QuestId && Reward.bAppliedToSociety; }));
            TestTrue(TEXT("source choice flag persisted"), Campaign->State->Flags.Contains(QuestId + TEXT("-") + ChoiceId));
            TestFalse(TEXT("resolved choice cannot replay"), Campaign->Choose(QuestId, ChoiceId));
            TestFalse(TEXT("completed case cannot restart"), Campaign->StartQuest(QuestId));
            const FSPStoryReward& Reward = Campaign->State->Rewards.Last();
            CreditsFromRewards += Reward.Credits;
            TestEqual(TEXT("credits applied to shared economy"), Society->GetPlayerCredits(), CreditsBefore + Reward.Credits);
        }
        TestEqual(TEXT("every source milestone traversed"), Milestones, 27);
        TestEqual(TEXT("every case completed"), Campaign->State->Rewards.Num(), 9);
        TestEqual(TEXT("journal retains source dialogue and ending text"), Campaign->GetJournal().Num(), 54);
        TestTrue(TEXT("reward sum applied exactly"), Society->GetPlayerCredits() == 1800 + CreditsFromRewards);
        TestTrue(TEXT("branch-specific final record"), Campaign->State->Quests.Last().ChoiceId ==
            (Branch == 0 ? TEXT("choice0") : TEXT("choice1")));
        if (Branch == 0)
        {
            TestEqual(TEXT("first source outcome amount"), Campaign->State->Rewards[0].Credits, 650);
            TestEqual(TEXT("first source outcome standing"), Campaign->State->Rewards[0].Standing, 12);
        }
        else TestEqual(TEXT("alternate source outcome amount"), Campaign->State->Rewards[0].Credits, 1100);

        TArray<uint8> Bytes;
        if (!TestTrue(TEXT("entire campaign serializes"), UGameplayStatics::SaveGameToMemory(Campaign->State, Bytes))) return false;
        TestTrue(TEXT("campaign remains a compact save"), Bytes.Num() > 0 && Bytes.Num() < 256 * 1024);
        USPSStoryCampaignSaveGame* Restored = Cast<USPSStoryCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
        if (!TestNotNull(TEXT("campaign save deserializes"), Restored)) return false;
        USPStoryCampaignComponent* Reloaded = NewObject<USPStoryCampaignComponent>(GetTransientPackage());
        Reloaded->bAutoSave = false;
        Reloaded->Society = Society;
        if (!TestTrue(TEXT("campaign catalog reloads"), Reloaded->InitializeCampaign(false))) return false;
        Reloaded->State = Restored;
        TestEqual(TEXT("all endings survive save roundtrip"), Reloaded->State->Rewards.Num(), 9);
        TestEqual(TEXT("journal survives save roundtrip"), Reloaded->GetJournal().Num(), 54);
        TestEqual(TEXT("applied rewards never double-pay after reload"), Reloaded->ApplyPendingRewards(), 0);
        TestEqual(TEXT("credits stay stable after reload"), Society->GetPlayerCredits(), 1800 + CreditsFromRewards);
        TestFalse(TEXT("completed case stays completed after reload"), Reloaded->StartQuest(TEXT("water")));
    }
    return true;
}
#endif
