#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SPPlayLoopWidget.generated.h"

class UTextBlock;
class UBorder;
class USPStoryCampaignComponent;

/** Small, persistent navigation and interaction panel for the unified play map. */
UCLASS()
class SPACEPATRIOTUNREAL_API USPPlayLoopWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetStatus(const FString& World, const FString& Action, const FString& Detail);
    /** Formats the live Storyworks cases; also usable by the campaign automation test. */
    static FString FormatJournal(const USPStoryCampaignComponent* Campaign);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Journal") void ToggleJournal();
    UFUNCTION(BlueprintPure, Category="Space Patriot|Journal") bool IsJournalOpen() const { return bJournalOpen; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WorldText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ActionText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailText;
    UPROPERTY(Transient) TObjectPtr<UBorder> JournalFrame;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> JournalBody;
    UPROPERTY(Transient) TObjectPtr<USPStoryCampaignComponent> Campaign;
    bool bJournalOpen = false;
    float NextJournalRefreshSeconds = 0.0f;
    void RefreshJournal();
};
