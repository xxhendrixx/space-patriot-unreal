#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SPPortTerminalWidget.generated.h"

class UTextBlock;

/** Persistent nearby port readout for freight, market, and case choices. */
UCLASS()
class SPACEPATRIOTUNREAL_API USPPortTerminalWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetReadout(const FString& WorldName, const FString& Message);

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HeaderText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> BodyText;
};
