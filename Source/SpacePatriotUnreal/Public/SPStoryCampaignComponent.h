#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/SaveGame.h"
#include "Dom/JsonObject.h"
#include "SPStoryCampaignComponent.generated.h"

class USPSocietySimulationComponent;

USTRUCT(BlueprintType)
struct FSPStoryChoiceView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Text;
};

USTRUCT(BlueprintType)
struct FSPStoryNodeView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString QuestId;
    UPROPERTY(BlueprintReadOnly) FString Type;
    UPROPERTY(BlueprintReadOnly) FString Title;
    UPROPERTY(BlueprintReadOnly) FString Speaker;
    UPROPERTY(BlueprintReadOnly) FString Text;
    UPROPERTY(BlueprintReadOnly) FString ObjectiveKind;
    UPROPERTY(BlueprintReadOnly) FString WorldId;
    UPROPERTY(BlueprintReadOnly) FString Good;
    UPROPERTY(BlueprintReadOnly) int32 Units = 0;
    UPROPERTY(BlueprintReadOnly) int32 Goal = 0;
    UPROPERTY(BlueprintReadOnly) int32 Progress = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FSPStoryChoiceView> Choices;
};

USTRUCT(BlueprintType)
struct FSPStoryQuestView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Title;
    UPROPERTY(BlueprintReadOnly) FString Status;
    UPROPERTY(BlueprintReadOnly) FString NodeId;
    UPROPERTY(BlueprintReadOnly) FString ChoiceId;
    UPROPERTY(BlueprintReadOnly) bool bUnlocked = false;
};

USTRUCT(BlueprintType)
struct FSPStoryQuestState
{
    GENERATED_BODY()
    UPROPERTY() FString Id;
    UPROPERTY() FString Status = TEXT("available");
    UPROPERTY() FString NodeId;
    UPROPERTY() int32 Progress = 0;
    UPROPERTY() FString ChoiceId;
};

USTRUCT(BlueprintType)
struct FSPStoryJournalEntry
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString QuestId;
    UPROPERTY(BlueprintReadOnly) FString NodeId;
    UPROPERTY(BlueprintReadOnly) FString Text;
    UPROPERTY(BlueprintReadOnly) int32 Sequence = 0;
};

USTRUCT(BlueprintType)
struct FSPStoryReward
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString QuestId;
    UPROPERTY(BlueprintReadOnly) FString FactionId;
    UPROPERTY(BlueprintReadOnly) int32 Standing = 0;
    UPROPERTY(BlueprintReadOnly) int32 Credits = 0;
    UPROPERTY(BlueprintReadOnly) FString WorldId;
    UPROPERTY(BlueprintReadOnly) FString Good;
    UPROPERTY(BlueprintReadOnly) int32 Stock = 0;
    UPROPERTY(BlueprintReadOnly) bool bAppliedToSociety = false;
};

/** State is actor-independent; the caller can save it to its campaign slot or a larger host save. */
UCLASS()
class SPACEPATRIOTUNREAL_API USPSStoryCampaignSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() FString ProjectId;
    UPROPERTY() FString CatalogHash;
    UPROPERTY() int32 Sequence = 0;
    UPROPERTY() TArray<FSPStoryQuestState> Quests;
    UPROPERTY() TArray<FSPStoryJournalEntry> Journal;
    UPROPERTY() TArray<FSPStoryReward> Rewards;
    UPROPERTY() TArray<FString> Flags;
    UPROPERTY() TArray<FString> SeenEventIds;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPStoryNodeSignature, const FSPStoryNodeView&, Node);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPStoryRewardSignature, const FSPStoryReward&, Reward);

/** The Long Debt's source Storyworks dialogue/objective graph and persistent consequences. */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPStoryCampaignComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPStoryCampaignComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story|Save") FString SaveSlot = TEXT("SpacePatriotLongDebt");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story|Save") bool bAutoLoad = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story|Save") bool bAutoSave = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story|Economy") TObjectPtr<USPSocietySimulationComponent> Society;
    UPROPERTY(BlueprintReadOnly, Category="Story|Save") TObjectPtr<USPSStoryCampaignSaveGame> State;
    UPROPERTY(BlueprintAssignable, Category="Story|Events") FSPStoryNodeSignature OnNodeChanged;
    UPROPERTY(BlueprintAssignable, Category="Story|Events") FSPStoryRewardSignature OnRewardGranted;

    UFUNCTION(BlueprintCallable, Category="Story|Save") bool InitializeCampaign(bool bLoadExisting = true);
    UFUNCTION(BlueprintCallable, Category="Story|Save") bool SaveCampaign();
    UFUNCTION(BlueprintCallable, Category="Story|Cases") bool StartQuest(const FString& QuestId);
    UFUNCTION(BlueprintCallable, Category="Story|Cases") bool Choose(const FString& QuestId, const FString& ChoiceId);
    /** Called by actual station, terminal, combat, sample and power interactions. Delivery is cargo-gated separately. */
    UFUNCTION(BlueprintCallable, Category="Story|Events") bool SignalGameplay(const FString& Kind, const FString& WorldId, const FString& EventId = TEXT(""));
    /** Docking system must confirm the player is in port before cargo can be transferred. */
    UFUNCTION(BlueprintCallable, Category="Story|Events") bool DeliverObjective(const FString& QuestId, const FString& WorldId, bool bConfirmedAtPort);
    UFUNCTION(BlueprintCallable, Category="Story|Economy") int32 ApplyPendingRewards();
    UFUNCTION(BlueprintPure, Category="Story|Cases") TArray<FSPStoryQuestView> GetQuestSummaries() const;
    UFUNCTION(BlueprintPure, Category="Story|Cases") bool GetCurrentNode(const FString& QuestId, FSPStoryNodeView& OutNode) const;
    UFUNCTION(BlueprintPure, Category="Story|Cases") TArray<FSPStoryJournalEntry> GetJournal() const;
    UFUNCTION(BlueprintPure, Category="Story|Cases") TArray<FSPStoryReward> GetRewardHistory() const;
    UFUNCTION(BlueprintPure, Category="Story|Data") int32 GetQuestCount() const { return QuestOrder.Num(); }
    UFUNCTION(BlueprintPure, Category="Story|Data") int32 GetNodeCount() const { return Nodes.Num(); }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TMap<FString, TSharedPtr<FJsonObject>> Quests;
    TMap<FString, TSharedPtr<FJsonObject>> Nodes;
    TArray<FString> QuestOrder;
    TMap<FString, FString> WorldIdByName;
    FString ProjectId;
    FString CatalogHash;
    FSPStoryQuestState* FindQuestState(const FString& QuestId);
    const FSPStoryQuestState* FindQuestState(const FString& QuestId) const;
    FString CanonicalWorldId(const FString& NameOrId) const;
    bool IsUnlocked(const FString& QuestId) const;
    bool EnterNode(const FString& QuestId, const FString& NodeId);
    bool MatchObjective(const FString& QuestId, const FString& Kind, const FString& WorldId) const;
    void ApplyNodeActions(const FString& QuestId, const FString& NodeId, const TSharedPtr<FJsonObject>& Node);
    bool ValidateLoadedState(const USPSStoryCampaignSaveGame* Candidate) const;
};
