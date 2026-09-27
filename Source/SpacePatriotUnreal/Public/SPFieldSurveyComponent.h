#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/SaveGame.h"
#include "SPFieldSurveyComponent.generated.h"

USTRUCT(BlueprintType)
struct FSPFieldSurveyJournalEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString WorldId;
    UPROPERTY(BlueprintReadOnly) FString Text;
    UPROPERTY(BlueprintReadOnly) int32 Sequence = 0;
};

USTRUCT(BlueprintType)
struct FSPFieldSurveyRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString WorldId;
    /** The source's automatic briefing and beacon nodes resolve immediately. */
    UPROPERTY(BlueprintReadOnly) FString Phase = TEXT("arrival");
    UPROPERTY(BlueprintReadOnly) bool bScanned = false;
    UPROPERTY(BlueprintReadOnly) bool bBeaconEmitted = false;
    UPROPERTY(BlueprintReadOnly) bool bComplete = false;
    UPROPERTY(BlueprintReadOnly) TArray<FSPFieldSurveyJournalEntry> Journal;
};

USTRUCT(BlueprintType)
struct FSPFieldSurveyStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString WorldId;
    UPROPERTY(BlueprintReadOnly) FString Body;
    UPROPERTY(BlueprintReadOnly) FString Profile;
    UPROPERTY(BlueprintReadOnly) FString Description;
    UPROPERTY(BlueprintReadOnly) FString Phase;
    UPROPERTY(BlueprintReadOnly) FString Title;
    UPROPERTY(BlueprintReadOnly) bool bComplete = false;
    UPROPERTY(BlueprintReadOnly) double DistanceMeters = 0.0;
    UPROPERTY(BlueprintReadOnly) TArray<FSPFieldSurveyJournalEntry> Journal;
};

USTRUCT(BlueprintType)
struct FSPFieldBeaconPulse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString WorldId;
    UPROPERTY(BlueprintReadOnly) FVector Position = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FString Preset;
};

/** Persistent per-world field-story state. Site transforms and visual pulses are runtime data. */
UCLASS()
class SPACEPATRIOTUNREAL_API USPSFieldSurveySaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() TArray<FSPFieldSurveyRecord> Records;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPFieldSurveyStatusSignature, const FSPFieldSurveyStatus&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPFieldBeaconSignature, const FSPFieldBeaconPulse&, Pulse);

/**
 * Native parity of original source/field-story.js. Gameplay owns the actual scan,
 * sampler, walking state and survey-site transform; this component gates and
 * persists the source Storyworks progression for each world independently.
 * Unreal coordinates are centimetres, so the source's 0.3 km arrival radius
 * becomes 30,000 cm.
 */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPFieldSurveyComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPFieldSurveyComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field Survey|Save") FString SaveSlot = TEXT("SpacePatriotFieldSurveys");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field Survey|Save") bool bAutoLoad = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Field Survey|Save") bool bAutoSave = true;
    UPROPERTY(BlueprintReadOnly, Category="Field Survey|Save") TObjectPtr<USPSFieldSurveySaveGame> State;
    UPROPERTY(BlueprintAssignable, Category="Field Survey|Events") FSPFieldSurveyStatusSignature OnStatusChanged;
    /** Connect this to Spellworks rendering. A frost preset is emitted for glacial worlds. */
    UPROPERTY(BlueprintAssignable, Category="Field Survey|Events") FSPFieldBeaconSignature OnBeaconPulse;

    UFUNCTION(BlueprintCallable, Category="Field Survey|Save") bool InitializeSurveys(bool bLoadExisting = true);
    UFUNCTION(BlueprintCallable, Category="Field Survey|Save") bool SaveSurveys();
    /** Worldworks supplies its actual seeded survey-site location when the world loads. */
    UFUNCTION(BlueprintCallable, Category="Field Survey|World") bool ActivateWorld(const FString& WorldId, FVector SurveySiteWorldLocation);
    /** Call from the authoritative player update after walking and bridge state change. */
    UFUNCTION(BlueprintCallable, Category="Field Survey|Gameplay") bool UpdatePlayerContext(FVector PlayerWorldLocation, bool bWalking, bool bBridgeWalk);
    /** Call only after the actual ecology scanner succeeds; a pre-arrival scan is remembered. */
    UFUNCTION(BlueprintCallable, Category="Field Survey|Gameplay") bool NotifyWorldScanned(const FString& WorldId, bool bScanSucceeded);
    /** Call only after collection succeeds. Source must equal surface-sampler. */
    UFUNCTION(BlueprintCallable, Category="Field Survey|Gameplay") bool NotifyFieldSample(const FString& WorldId, const FString& Source, bool bCollectionSucceeded);
    UFUNCTION(BlueprintPure, Category="Field Survey|Status") bool GetStatus(const FString& WorldId, FSPFieldSurveyStatus& OutStatus) const;
    UFUNCTION(BlueprintPure, Category="Field Survey|Status") bool GetCurrentStatus(FSPFieldSurveyStatus& OutStatus) const;
    UFUNCTION(BlueprintPure, Category="Field Survey|Status") TArray<FSPFieldSurveyJournalEntry> GetJournal(const FString& WorldId) const;
    /** Matches the source visual queue's cap of eight; consuming does not change saved story state. */
    UFUNCTION(BlueprintCallable, Category="Field Survey|Events") TArray<FSPFieldBeaconPulse> TakePendingBeaconPulses();
    UFUNCTION(BlueprintPure, Category="Field Survey|World") int32 GetWorldCount() const { return Definitions.Num(); }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FDefinition
    {
        FString Name;
        FString Profile;
        FString Description;
        int32 Type = -1;
    };

    TMap<FString, FDefinition> Definitions;
    TMap<FString, FVector> SurveySites;
    TArray<FSPFieldBeaconPulse> PendingPulses;
    FString CurrentWorldId;
    FVector PlayerLocation = FVector::ZeroVector;
    bool bHasPlayerContext = false;
    bool bPlayerWalking = false;
    bool bPlayerBridgeWalking = false;

    bool LoadDefinitions();
    FString CanonicalWorldId(const FString& WorldId) const;
    FSPFieldSurveyRecord* FindRecord(const FString& WorldId);
    const FSPFieldSurveyRecord* FindRecord(const FString& WorldId) const;
    FSPFieldSurveyRecord* EnsureRecord(const FString& WorldId);
    bool IsNearCurrentSite() const;
    void ResolveScan(FSPFieldSurveyRecord& Record);
    void Complete(FSPFieldSurveyRecord& Record);
    void AddJournal(FSPFieldSurveyRecord& Record, const FString& Text);
    void BroadcastStatus(const FString& WorldId);
    bool ValidateState(const USPSFieldSurveySaveGame* Candidate) const;
};
