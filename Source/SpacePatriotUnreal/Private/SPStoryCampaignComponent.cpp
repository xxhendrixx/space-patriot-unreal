#include "SPStoryCampaignComponent.h"

#include "SPSocietySimulationComponent.h"
#include "Dom/JsonValue.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    TSharedPtr<FJsonObject> ReadJson(const FString& Path, FString* RawText = nullptr)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path)) return nullptr;
        TSharedPtr<FJsonObject> Object;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object)) return nullptr;
        if (RawText) *RawText = MoveTemp(Text);
        return Object;
    }

    FString String(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
    {
        FString Result;
        if (Object.IsValid()) Object->TryGetStringField(Key, Result);
        return Result;
    }

    int32 Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32 Default = 0)
    {
        double Result = 0.0;
        return Object.IsValid() && Object->TryGetNumberField(Key, Result) ? static_cast<int32>(Result) : Default;
    }

    TSharedPtr<FJsonObject> Object(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Key)
    {
        const TSharedPtr<FJsonObject>* Result = nullptr;
        return Parent.IsValid() && Parent->TryGetObjectField(Key, Result) && Result ? *Result : nullptr;
    }

    const TArray<TSharedPtr<FJsonValue>>* Array(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Key)
    {
        const TArray<TSharedPtr<FJsonValue>>* Result = nullptr;
        return Parent.IsValid() && Parent->TryGetArrayField(Key, Result) ? Result : nullptr;
    }
}

USPStoryCampaignComponent::USPStoryCampaignComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USPStoryCampaignComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!Society && GetOwner()) Society = GetOwner()->FindComponentByClass<USPSocietySimulationComponent>();
    InitializeCampaign(bAutoLoad);
    ApplyPendingRewards();
}

void USPStoryCampaignComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bAutoSave && State) SaveCampaign();
    Super::EndPlay(EndPlayReason);
}

bool USPStoryCampaignComponent::InitializeCampaign(bool bLoadExisting)
{
    FString Raw;
    const TSharedPtr<FJsonObject> Root = ReadJson(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/StoryCampaign.json")), &Raw);
    if (!Root.IsValid() || String(Root, TEXT("format")) != TEXT("storyworks-project") || Number(Root, TEXT("version")) != 1)
        return false;
    const TArray<TSharedPtr<FJsonValue>>* QuestValues = Array(Root, TEXT("quests"));
    const TArray<TSharedPtr<FJsonValue>>* NodeValues = Array(Root, TEXT("nodes"));
    if (!QuestValues || !NodeValues || QuestValues->Num() != 9 || NodeValues->Num() != 90) return false;

    TMap<FString, TSharedPtr<FJsonObject>> NewQuests;
    TMap<FString, TSharedPtr<FJsonObject>> NewNodes;
    TArray<FString> NewOrder;
    for (const TSharedPtr<FJsonValue>& Value : *QuestValues)
    {
        const TSharedPtr<FJsonObject> Def = Value->AsObject();
        const FString Id = String(Def, TEXT("id"));
        if (Id.IsEmpty() || NewQuests.Contains(Id) || String(Def, TEXT("entry")).IsEmpty()) return false;
        NewQuests.Add(Id, Def);
        NewOrder.Add(Id);
    }
    int32 ObjectiveCount = 0;
    int32 EndingCount = 0;
    for (const TSharedPtr<FJsonValue>& Value : *NodeValues)
    {
        const TSharedPtr<FJsonObject> Def = Value->AsObject();
        const FString Id = String(Def, TEXT("id"));
        const FString QuestId = String(Def, TEXT("questId"));
        if (Id.IsEmpty() || NewNodes.Contains(Id) || !NewQuests.Contains(QuestId)) return false;
        NewNodes.Add(Id, Def);
        ObjectiveCount += String(Def, TEXT("type")) == TEXT("objective") ? 1 : 0;
        EndingCount += String(Def, TEXT("type")) == TEXT("end") ? 1 : 0;
    }
    if (ObjectiveCount != 27 || EndingCount != 18) return false;
    for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : NewQuests)
    {
        const TSharedPtr<FJsonObject>* Entry = NewNodes.Find(String(Pair.Value, TEXT("entry")));
        if (!Entry || String(*Entry, TEXT("questId")) != Pair.Key) return false;
        if (const TSharedPtr<FJsonObject> Requires = Object(Pair.Value, TEXT("requires")))
        {
            const TArray<TSharedPtr<FJsonValue>>* Conditions = Array(Requires, TEXT("all"));
            if (!Conditions) return false;
            for (const TSharedPtr<FJsonValue>& Condition : *Conditions)
            {
                const TSharedPtr<FJsonObject> Required = Condition->AsObject();
                if (String(Required, TEXT("source")) != TEXT("quest") ||
                    String(Required, TEXT("value")) != TEXT("completed") ||
                    !NewQuests.Contains(String(Required, TEXT("key")))) return false;
            }
        }
    }
    for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : NewNodes)
    {
        const TSharedPtr<FJsonObject> Def = Pair.Value;
        if (String(Def, TEXT("type")) == TEXT("end")) continue;
        TArray<FString> Targets;
        if (String(Def, TEXT("type")) == TEXT("dialogue"))
        {
            const TArray<TSharedPtr<FJsonValue>>* Choices = Array(Def, TEXT("choices"));
            if (!Choices || Choices->IsEmpty()) return false;
            for (const TSharedPtr<FJsonValue>& Choice : *Choices) Targets.Add(String(Choice->AsObject(), TEXT("next")));
        }
        else Targets.Add(String(Def, TEXT("next")));
        for (const FString& TargetId : Targets)
        {
            const TSharedPtr<FJsonObject>* Target = NewNodes.Find(TargetId);
            if (!Target || String(*Target, TEXT("questId")) != String(Def, TEXT("questId"))) return false;
        }
    }

    const TSharedPtr<FJsonObject> WorldsRoot = ReadJson(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Worlds.json")));
    const TArray<TSharedPtr<FJsonValue>>* WorldValues = Array(WorldsRoot, TEXT("worlds"));
    if (!WorldValues || WorldValues->Num() != 19) return false;
    TMap<FString, FString> NewWorlds;
    for (const TSharedPtr<FJsonValue>& Value : *WorldValues)
    {
        const TSharedPtr<FJsonObject> Def = Value->AsObject();
        const FString WorldId = String(Def, TEXT("id"));
        if (WorldId.IsEmpty()) return false;
        NewWorlds.Add(WorldId.ToLower(), WorldId);
        NewWorlds.Add(String(Def, TEXT("name")).ToLower(), WorldId);
    }
    Quests = MoveTemp(NewQuests);
    Nodes = MoveTemp(NewNodes);
    QuestOrder = MoveTemp(NewOrder);
    WorldIdByName = MoveTemp(NewWorlds);
    ProjectId = String(Root, TEXT("id"));
    CatalogHash = FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*Raw));

    USPSStoryCampaignSaveGame* Loaded = bLoadExisting ? Cast<USPSStoryCampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)) : nullptr;
    if (ValidateLoadedState(Loaded)) State = Loaded;
    else
    {
        State = NewObject<USPSStoryCampaignSaveGame>(this);
        State->ProjectId = ProjectId;
        State->CatalogHash = CatalogHash;
        for (const FString& Id : QuestOrder)
        {
            FSPStoryQuestState QuestState;
            QuestState.Id = Id;
            State->Quests.Add(MoveTemp(QuestState));
        }
    }
    ApplyPendingRewards();
    return true;
}

bool USPStoryCampaignComponent::ValidateLoadedState(const USPSStoryCampaignSaveGame* Candidate) const
{
    if (!Candidate || Candidate->Version != 1 || Candidate->ProjectId != ProjectId ||
        Candidate->CatalogHash != CatalogHash || Candidate->Quests.Num() != QuestOrder.Num()) return false;
    TSet<FString> Seen;
    for (const FSPStoryQuestState& Quest : Candidate->Quests)
    {
        if (Seen.Contains(Quest.Id) || !Quests.Contains(Quest.Id) ||
            (Quest.Status != TEXT("available") && Quest.Status != TEXT("active") && Quest.Status != TEXT("completed"))) return false;
        Seen.Add(Quest.Id);
        if (Quest.Status != TEXT("available"))
        {
            const TSharedPtr<FJsonObject>* Node = Nodes.Find(Quest.NodeId);
            if (!Node || String(*Node, TEXT("questId")) != Quest.Id) return false;
        }
    }
    TSet<FString> Rewards;
    for (const FSPStoryReward& Reward : Candidate->Rewards)
    {
        if (Rewards.Contains(Reward.QuestId) || !Seen.Contains(Reward.QuestId)) return false;
        Rewards.Add(Reward.QuestId);
    }
    return true;
}

bool USPStoryCampaignComponent::SaveCampaign()
{
    return State && UGameplayStatics::SaveGameToSlot(State, SaveSlot, 0);
}

FSPStoryQuestState* USPStoryCampaignComponent::FindQuestState(const FString& QuestId)
{
    return State ? State->Quests.FindByPredicate([&](const FSPStoryQuestState& Quest) { return Quest.Id == QuestId; }) : nullptr;
}

const FSPStoryQuestState* USPStoryCampaignComponent::FindQuestState(const FString& QuestId) const
{
    return State ? State->Quests.FindByPredicate([&](const FSPStoryQuestState& Quest) { return Quest.Id == QuestId; }) : nullptr;
}

FString USPStoryCampaignComponent::CanonicalWorldId(const FString& NameOrId) const
{
    return WorldIdByName.FindRef(NameOrId.ToLower());
}

bool USPStoryCampaignComponent::IsUnlocked(const FString& QuestId) const
{
    const TSharedPtr<FJsonObject>* Quest = Quests.Find(QuestId);
    if (!Quest) return false;
    const TSharedPtr<FJsonObject> Requires = Object(*Quest, TEXT("requires"));
    const TArray<TSharedPtr<FJsonValue>>* Conditions = Array(Requires, TEXT("all"));
    if (!Conditions) return true;
    for (const TSharedPtr<FJsonValue>& Value : *Conditions)
    {
        const FSPStoryQuestState* Dependency = FindQuestState(String(Value->AsObject(), TEXT("key")));
        if (!Dependency || Dependency->Status != TEXT("completed")) return false;
    }
    return true;
}

TArray<FSPStoryQuestView> USPStoryCampaignComponent::GetQuestSummaries() const
{
    TArray<FSPStoryQuestView> Result;
    for (const FString& Id : QuestOrder)
    {
        FSPStoryQuestView View;
        View.Id = Id;
        View.Title = String(Quests.FindRef(Id), TEXT("title"));
        if (const FSPStoryQuestState* Quest = FindQuestState(Id))
        {
            View.Status = Quest->Status;
            View.NodeId = Quest->NodeId;
            View.ChoiceId = Quest->ChoiceId;
            View.bUnlocked = IsUnlocked(Id);
        }
        Result.Add(MoveTemp(View));
    }
    return Result;
}

bool USPStoryCampaignComponent::GetCurrentNode(const FString& QuestId, FSPStoryNodeView& OutNode) const
{
    OutNode = FSPStoryNodeView();
    const FSPStoryQuestState* Quest = FindQuestState(QuestId);
    const TSharedPtr<FJsonObject>* Def = Quest ? Nodes.Find(Quest->NodeId) : nullptr;
    if (!Def) return false;
    OutNode.Id = Quest->NodeId;
    OutNode.QuestId = QuestId;
    OutNode.Type = String(*Def, TEXT("type"));
    OutNode.Title = String(*Def, TEXT("title"));
    OutNode.Speaker = String(*Def, TEXT("speaker"));
    OutNode.Text = String(*Def, TEXT("text"));
    OutNode.Progress = Quest->Progress;
    OutNode.Goal = Number(*Def, TEXT("goal"), OutNode.Type == TEXT("objective") ? 1 : 0);
    const TSharedPtr<FJsonObject> Location = Object(*Def, TEXT("location"));
    OutNode.ObjectiveKind = String(Location, TEXT("kind"));
    OutNode.WorldId = CanonicalWorldId(String(Location, TEXT("world")));
    OutNode.Good = String(Location, TEXT("good"));
    OutNode.Units = Number(Location, TEXT("units"));
    if (const TArray<TSharedPtr<FJsonValue>>* Choices = Array(*Def, TEXT("choices")))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Choices)
        {
            FSPStoryChoiceView Choice;
            Choice.Id = String(Value->AsObject(), TEXT("id"));
            Choice.Text = String(Value->AsObject(), TEXT("text"));
            OutNode.Choices.Add(MoveTemp(Choice));
        }
    }
    return true;
}

TArray<FSPStoryJournalEntry> USPStoryCampaignComponent::GetJournal() const
{
    return State ? State->Journal : TArray<FSPStoryJournalEntry>();
}

TArray<FSPStoryReward> USPStoryCampaignComponent::GetRewardHistory() const
{
    return State ? State->Rewards : TArray<FSPStoryReward>();
}

bool USPStoryCampaignComponent::StartQuest(const FString& QuestId)
{
    FSPStoryQuestState* Quest = FindQuestState(QuestId);
    const TSharedPtr<FJsonObject>* Def = Quests.Find(QuestId);
    if (!Quest || !Def || Quest->Status != TEXT("available") || !IsUnlocked(QuestId)) return false;
    Quest->Status = TEXT("active");
    if (!EnterNode(QuestId, String(*Def, TEXT("entry")))) return false;
    if (bAutoSave) SaveCampaign();
    return true;
}

bool USPStoryCampaignComponent::Choose(const FString& QuestId, const FString& ChoiceId)
{
    FSPStoryQuestState* Quest = FindQuestState(QuestId);
    const TSharedPtr<FJsonObject>* Def = Quest ? Nodes.Find(Quest->NodeId) : nullptr;
    if (!Quest || Quest->Status != TEXT("active") || !Def || String(*Def, TEXT("type")) != TEXT("dialogue")) return false;
    const TArray<TSharedPtr<FJsonValue>>* Choices = Array(*Def, TEXT("choices"));
    if (!Choices) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Choices)
    {
        const TSharedPtr<FJsonObject> Choice = Value->AsObject();
        if (String(Choice, TEXT("id")) != ChoiceId) continue;
        const FString Next = String(Choice, TEXT("next"));
        if (!Nodes.Contains(Next)) return false;
        if (const TArray<TSharedPtr<FJsonValue>>* Actions = Array(Choice, TEXT("actions")))
        {
            for (const TSharedPtr<FJsonValue>& ActionValue : *Actions)
            {
                const TSharedPtr<FJsonObject> Action = ActionValue->AsObject();
                if (String(Action, TEXT("type")) == TEXT("flag"))
                {
                    const FString Flag = String(Action, TEXT("key"));
                    if (!Flag.IsEmpty()) State->Flags.AddUnique(Flag);
                }
            }
        }
        if (ChoiceId.StartsWith(TEXT("choice"))) Quest->ChoiceId = ChoiceId;
        if (!EnterNode(QuestId, Next)) return false;
        if (bAutoSave) SaveCampaign();
        return true;
    }
    return false;
}

bool USPStoryCampaignComponent::MatchObjective(const FString& QuestId, const FString& Kind, const FString& WorldId) const
{
    const FSPStoryQuestState* Quest = FindQuestState(QuestId);
    const TSharedPtr<FJsonObject>* Def = Quest ? Nodes.Find(Quest->NodeId) : nullptr;
    if (!Quest || Quest->Status != TEXT("active") || !Def || String(*Def, TEXT("type")) != TEXT("objective") ||
        String(*Def, TEXT("event")) != TEXT("gameplay.signal")) return false;
    const TSharedPtr<FJsonObject> Match = Object(*Def, TEXT("match"));
    return String(Match, TEXT("kind")) == Kind &&
        CanonicalWorldId(String(Match, TEXT("world"))) == CanonicalWorldId(WorldId) &&
        !CanonicalWorldId(WorldId).IsEmpty();
}

bool USPStoryCampaignComponent::SignalGameplay(const FString& Kind, const FString& WorldId, const FString& EventId)
{
    if (!State || Kind.IsEmpty() || Kind == TEXT("delivery") || CanonicalWorldId(WorldId).IsEmpty()) return false;
    const FString Id = EventId.IsEmpty() ? FString::Printf(TEXT("campaign:event:%d"), ++State->Sequence) : EventId;
    if (State->SeenEventIds.Contains(Id)) return false;
    State->SeenEventIds.Add(Id);
    if (State->SeenEventIds.Num() > 4096) State->SeenEventIds.RemoveAt(0);
    TArray<FString> Matching;
    for (const FString& QuestId : QuestOrder) if (MatchObjective(QuestId, Kind, WorldId)) Matching.Add(QuestId);
    for (const FString& QuestId : Matching)
    {
        FSPStoryQuestState* Quest = FindQuestState(QuestId);
        const TSharedPtr<FJsonObject>* Def = Quest ? Nodes.Find(Quest->NodeId) : nullptr;
        if (!Quest || !Def) continue;
        Quest->Progress = FMath::Min(Number(*Def, TEXT("goal"), 1), Quest->Progress + 1);
        if (Quest->Progress >= Number(*Def, TEXT("goal"), 1)) EnterNode(QuestId, String(*Def, TEXT("next")));
    }
    if (bAutoSave) SaveCampaign();
    return true;
}

bool USPStoryCampaignComponent::DeliverObjective(const FString& QuestId, const FString& WorldId, bool bConfirmedAtPort)
{
    if (!bConfirmedAtPort || !State || !Society || !Society->State || !MatchObjective(QuestId, TEXT("delivery"), WorldId)) return false;
    FSPStoryQuestState* Quest = FindQuestState(QuestId);
    const TSharedPtr<FJsonObject>* Def = Quest ? Nodes.Find(Quest->NodeId) : nullptr;
    const TSharedPtr<FJsonObject> Location = Def ? Object(*Def, TEXT("location")) : nullptr;
    const FString Good = String(Location, TEXT("good"));
    const int32 Units = Number(Location, TEXT("units"));
    const FString CanonicalWorld = CanonicalWorldId(WorldId);
    if (Units <= 0 || Society->GetHoldCargo(Good) < Units) return false;
    FSPMarketRecord* Market = Society->State->Markets.FindByPredicate([&](const FSPMarketRecord& Item) { return Item.WorldId == CanonicalWorld; });
    int32* Cargo = Good == TEXT("organics") ? &Society->State->HoldOrganics :
        Good == TEXT("ore") ? &Society->State->HoldOre : Good == TEXT("crystal") ? &Society->State->HoldCrystal : nullptr;
    float* Stock = Market ? (Good == TEXT("organics") ? &Market->Organics :
        Good == TEXT("ore") ? &Market->Ore : Good == TEXT("crystal") ? &Market->Crystal : nullptr) : nullptr;
    if (!Cargo || !Stock || !Nodes.Contains(String(*Def, TEXT("next")))) return false;
    *Cargo -= Units;
    *Stock = FMath::Clamp(*Stock + Units, 0.0f, 2000.0f);
    Quest->Progress = Number(*Def, TEXT("goal"), 1);
    if (!EnterNode(QuestId, String(*Def, TEXT("next")))) return false;
    if (bAutoSave)
    {
        Society->SaveSociety();
        SaveCampaign();
    }
    return true;
}

bool USPStoryCampaignComponent::EnterNode(const FString& QuestId, const FString& NodeId)
{
    FSPStoryQuestState* Quest = FindQuestState(QuestId);
    const TSharedPtr<FJsonObject>* Node = Nodes.Find(NodeId);
    if (!Quest || Quest->Status != TEXT("active") || !Node || String(*Node, TEXT("questId")) != QuestId) return false;
    Quest->NodeId = NodeId;
    Quest->Progress = 0;
    // A reward may persist both save slots while this node is entered. Keep the saved
    // quest status terminal as well, so a restart never strands a paid ending as active.
    if (String(*Node, TEXT("type")) == TEXT("end")) Quest->Status = String(*Node, TEXT("result"));
    ApplyNodeActions(QuestId, NodeId, *Node);
    FSPStoryNodeView View;
    if (GetCurrentNode(QuestId, View)) OnNodeChanged.Broadcast(View);
    return true;
}

void USPStoryCampaignComponent::ApplyNodeActions(const FString& QuestId, const FString& NodeId, const TSharedPtr<FJsonObject>& Node)
{
    const TArray<TSharedPtr<FJsonValue>>* Actions = Array(Node, TEXT("actions"));
    if (!State || !Actions) return;
    for (const TSharedPtr<FJsonValue>& Value : *Actions)
    {
        const TSharedPtr<FJsonObject> Action = Value->AsObject();
        if (String(Action, TEXT("type")) == TEXT("journal"))
        {
            FSPStoryJournalEntry Entry;
            Entry.QuestId = QuestId;
            Entry.NodeId = NodeId;
            Entry.Text = String(Action, TEXT("text"));
            Entry.Sequence = ++State->Sequence;
            State->Journal.Add(MoveTemp(Entry));
            if (State->Journal.Num() > 2000) State->Journal.RemoveAt(0);
        }
        else if (String(Action, TEXT("type")) == TEXT("command") && String(Action, TEXT("command")) == TEXT("campaign.reward"))
        {
            if (State->Rewards.ContainsByPredicate([&](const FSPStoryReward& Reward) { return Reward.QuestId == QuestId; })) continue;
            const TSharedPtr<FJsonObject> Payload = Object(Action, TEXT("payload"));
            const TSharedPtr<FJsonObject> Market = Object(Payload, TEXT("market"));
            FSPStoryReward Reward;
            Reward.QuestId = String(Payload, TEXT("key"));
            Reward.FactionId = String(Payload, TEXT("faction"));
            Reward.Standing = Number(Payload, TEXT("standing"));
            Reward.Credits = Number(Payload, TEXT("credits"));
            Reward.WorldId = CanonicalWorldId(String(Market, TEXT("world")));
            Reward.Good = String(Market, TEXT("good"));
            Reward.Stock = Number(Market, TEXT("stock"));
            if (Reward.QuestId != QuestId || Reward.WorldId.IsEmpty()) continue;
            State->Rewards.Add(Reward);
            ApplyPendingRewards();
            OnRewardGranted.Broadcast(State->Rewards.Last());
        }
    }
}

int32 USPStoryCampaignComponent::ApplyPendingRewards()
{
    if (!State || !Society || !Society->State) return 0;
    int32 Applied = 0;
    for (FSPStoryReward& Reward : State->Rewards)
    {
        if (Reward.bAppliedToSociety) continue;
        FSPMarketRecord* Market = Society->State->Markets.FindByPredicate([&](const FSPMarketRecord& Item) { return Item.WorldId == Reward.WorldId; });
        int32* Standing = Reward.FactionId == TEXT("union") ? &Society->State->UnionStanding :
            Reward.FactionId == TEXT("helix") ? &Society->State->HelixStanding :
            Reward.FactionId == TEXT("redwake") ? &Society->State->RedwakeStanding : nullptr;
        float* Stock = Market ? (Reward.Good == TEXT("organics") ? &Market->Organics :
            Reward.Good == TEXT("ore") ? &Market->Ore : Reward.Good == TEXT("crystal") ? &Market->Crystal : nullptr) : nullptr;
        if (!Standing || !Stock) continue;
        Society->State->PlayerCredits += Reward.Credits;
        *Standing = FMath::Clamp(*Standing + Reward.Standing, -100, 100);
        *Stock = FMath::Clamp(*Stock + Reward.Stock, 0.0f, 2000.0f);
        Reward.bAppliedToSociety = true;
        ++Applied;
    }
    if (Applied > 0 && bAutoSave)
    {
        Society->SaveSociety();
        SaveCampaign();
    }
    return Applied;
}
