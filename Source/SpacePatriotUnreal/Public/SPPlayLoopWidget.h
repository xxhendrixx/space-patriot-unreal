#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SPPlayLoopWidget.generated.h"

class UTextBlock;

/** Small, persistent navigation and interaction panel for the unified play map. */
UCLASS()
class SPACEPATRIOTUNREAL_API USPPlayLoopWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetStatus(const FString& World, const FString& Action, const FString& Detail);

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WorldText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ActionText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailText;
};
