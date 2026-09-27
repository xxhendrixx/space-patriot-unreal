#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPPlayLoopDirector.generated.h"

class APlayerController;
class APlayerStart;
class APawn;
class ASPFlightPawn;
class ASPWorldSurface;
class ASPWorldDressing;
class USPDayNightCycleComponent;
class USPFieldSurveyComponent;
class UCharacterMovementComponent;
class USPHyperjumpRouteComponent;
class USPPlayLoopWidget;
class USceneComponent;

/**
 * Keeps the on-foot and Kestrel pawns in one level. The physical ship and
 * source-seeded surface remain in the level as the player boards, flies,
 * jumps, lands and disembarks; this avoids separate disconnected play maps.
 */
UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPPlayLoopDirector : public AActor
{
    GENERATED_BODY()

public:
    ASPPlayLoopDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|Play Loop")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|Play Loop")
    TObjectPtr<USPHyperjumpRouteComponent> HyperjumpRoute;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|Play Loop")
    TObjectPtr<USPDayNightCycleComponent> DayNightCycle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Play Loop")
    TSubclassOf<ASPFlightPawn> ShipClass;

    /** The Kestrel mesh origin sits 200 cm above its deployed gear feet. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Play Loop")
    FVector ParkedShipLocation = FVector(0.0, 0.0, 200.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Play Loop")
    FRotator ParkedShipRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Play Loop")
    FVector BoardingOffsetLocal = FVector(-300.0, -800.0, -170.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Play Loop", meta=(ClampMin="100"))
    float BoardingRangeCm = 1200.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|Play Loop")
    TObjectPtr<ASPFlightPawn> Ship;

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Play Loop") bool TryBoard();
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Play Loop") bool TryDisembark();
    UFUNCTION(BlueprintPure, Category="Space Patriot|Play Loop") bool IsPiloting() const;
    /** B scans local terrain/wildlife; Shift+B collects one report sample when the survey asks for it. */
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Field Survey") bool TrySurveyAction(bool bCollectSample);

private:
    struct FPortActorState
    {
        TWeakObjectPtr<AActor> Actor;
        bool bHidden = false;
        bool bCollision = true;
        bool bTickEnabled = true;
    };

    UPROPERTY(Transient) TObjectPtr<APlayerController> Player;
    UPROPERTY(Transient) TObjectPtr<APawn> GroundPawn;
    UPROPERTY(Transient) TObjectPtr<ASPWorldSurface> Surface;
    UPROPERTY(Transient) TObjectPtr<ASPWorldDressing> WorldDressing;
    UPROPERTY(Transient) TObjectPtr<USPFieldSurveyComponent> ActiveSurvey;
    UPROPERTY(Transient) TObjectPtr<USPPlayLoopWidget> StatusWidget;
    UPROPERTY(Transient) TObjectPtr<APlayerStart> RespawnStart;
    TArray<FPortActorState> EarthOnlyActors;
    bool bInputBound = false;
    bool bShipWasFlying = false;
    bool bSurveySiteBound = false;
    FString LastAction;
    float FeedbackUntilSeconds = 0.0f;

    void InitializeWhenReady();
    void BindPlayerInput();
    void RefreshStatus();
    void RefreshTravelPhase();
    void RefreshSurveyContext();
    void SetSurveySiteAtShip(const FString& WorldId);
    bool SaveSimulationCheckpoint();
    bool LoadSimulationCheckpoint(const FString& WorldId, bool bValidateOnly = false);
    void SetEarthPortVisible(bool bVisible);
    FVector GetBoardingLocation() const;
    bool FindEgressLocation(FVector& OutLocation) const;
    void UpdateRespawnAnchor();

    void OnInteract();
    void OnCycleDestination();
    void OnJump();
    void OnSaveJourney();
    void OnLoadJourney();
    void OnSkipTime();
    void OnScanPressed();
    void OnSamplePressed();
    UFUNCTION() void OnWorldArrived(const FString& WorldId);
};
