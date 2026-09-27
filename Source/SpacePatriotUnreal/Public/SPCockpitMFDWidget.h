#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SPCockpitMFDWidget.generated.h"

class ASPFlightPawn;
class UCanvasPanelSlot;
class UTextBlock;

// Camera-independent, dark-backed flight display. The ship supplies live data;
// buttons invoke the same vessel APIs used by the Blueprint keyboard graph.
UCLASS()
class SPACEPATRIOTUNREAL_API USPCockpitMFDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetShip(ASPFlightPawn* InShip);
    void SetReadout(const FText& Readout, float Brightness);
    void SetPresentation(bool bCockpit, bool bPointerActive);

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HeaderText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> BodyText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
    TWeakObjectPtr<ASPFlightPawn> Ship;
    UCanvasPanelSlot* FrameSlot = nullptr;

    UFUNCTION() void OnPreviousPage();
    UFUNCTION() void OnNextPage();
    UFUNCTION() void OnTravelPreset();
    UFUNCTION() void OnCombatPreset();
    UFUNCTION() void OnDim();
};
