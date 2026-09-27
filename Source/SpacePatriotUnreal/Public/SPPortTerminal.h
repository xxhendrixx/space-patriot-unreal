#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPPortTerminal.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USPPortTerminalWidget;
class USPSocietySimulationComponent;
class USPStoryCampaignComponent;
struct FSPStoryQuestView;

/** A nearby, on-foot port board backed by the persistent source society simulation. */
UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPPortTerminal : public AActor
{
    GENERATED_BODY()

public:
    ASPPortTerminal();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Port|Visual")
    TObjectPtr<UStaticMeshComponent> TerminalMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Port|Visual")
    TObjectPtr<UTextRenderComponent> TerminalLabel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Port|Interaction", meta=(ClampMin="100", ClampMax="1000"))
    float InteractionRadiusCm = 350.0f;
    /** One untagged instance follows the landed ship on non-Earth worlds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Port|Interaction")
    bool bRemotePortProxy = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Port|Interaction")
    FString LastMessage;
    UPROPERTY(Transient)
    TObjectPtr<USPPortTerminalWidget> StatusWidget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Port|Missions")
    FString SelectedQuestId;
    /** Dock-board selection; only goods and money in SocietyState persist. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Port|Cargo")
    FString SelectedCargoGood = TEXT("ore");

    /** F while standing near the board. Freight remains available when no port objective can advance. */
    UFUNCTION(BlueprintCallable, Category="Port|Interaction")
    bool TryInteract();

    /** M starts the next unlocked case or advances its one-choice dialogue at the port. */
    UFUNCTION(BlueprintCallable, Category="Port|Missions")
    bool TryMissionInteract();

    /** H selects among active and unlocked cases, so no case is forced ahead of another. */
    UFUNCTION(BlueprintCallable, Category="Port|Missions")
    bool TryCycleMission();

    /** Choose a numbered response when the current case offers multiple choices. */
    UFUNCTION(BlueprintCallable, Category="Port|Missions")
    bool TryMissionChoice(int32 ChoiceNumber);

    /** 3 cycles ore / organics / crystal and displays the local market. */
    UFUNCTION(BlueprintCallable, Category="Port|Cargo") bool TryCycleCargoGood();
    /** 4 buys one unit into the dock's staging area. */
    UFUNCTION(BlueprintCallable, Category="Port|Cargo") bool TryBuyCargo();
    /** 5 moves one staged unit to the ship, subject to hold capacity. */
    UFUNCTION(BlueprintCallable, Category="Port|Cargo") bool TryLoadCargo();
    /** 6 moves one unit from the ship to this dock. */
    UFUNCTION(BlueprintCallable, Category="Port|Cargo") bool TryUnloadCargo();
    /** 7 sells one staged unit, except sealed contract cargo. */
    UFUNCTION(BlueprintCallable, Category="Port|Cargo") bool TrySellCargo();

private:
    bool bInputBound = false;
    FString DisplayedWorldId;
    void EnsurePlayerInput();
    void OnInteractPressed();
    void OnMissionPressed();
    void OnCycleMissionPressed();
    void OnMissionChoiceOne();
    void OnMissionChoiceTwo();
    void OnCycleCargoGood();
    void OnBuyCargo();
    void OnLoadCargo();
    void OnUnloadCargo();
    void OnSellCargo();
    void UpdateRemoteProxy();
    void Report(const FString& Message, bool bSuccess, float DurationSeconds = 5.0f);
    bool CanPlayerUseTerminal() const;
    bool ShowMissionNode(USPStoryCampaignComponent* Campaign, const FSPStoryQuestView& Quest);
    bool FindServices(FString& WorldId, FString& CityId,
        USPSocietySimulationComponent*& Society, USPStoryCampaignComponent*& Campaign) const;
    bool AdvanceTerminalObjective(USPStoryCampaignComponent* Campaign, const FString& WorldId);
    bool ProcessFreight(USPSocietySimulationComponent* Society, const FString& WorldId, const FString& CityId);
    bool FindCargoServices(FString& WorldId, FString& CityId, USPSocietySimulationComponent*& Society);
    void ReportCargoStatus(USPSocietySimulationComponent* Society, const FString& WorldId, const FString& CityId,
        const FString& Action, bool bSuccess);
};
