#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPTravelNavigationComponent.h"
#include "SPHyperjumpRouteComponent.generated.h"

class ASPFlightPawn;
class ASPWorldSurface;

/** The small, live-facing view of a route; the navigation component owns the save state. */
USTRUCT(BlueprintType)
struct FSPHyperjumpRouteStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Hyperjump") FSPTravelNavigationState Navigation;
    UPROPERTY(BlueprintReadOnly, Category="Hyperjump") FString CurrentWorldName;
    UPROPERTY(BlueprintReadOnly, Category="Hyperjump") FString DestinationWorldName;
    UPROPERTY(BlueprintReadOnly, Category="Hyperjump") FString Message;
    UPROPERTY(BlueprintReadOnly, Category="Hyperjump") float ChargeFraction = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Hyperjump") bool bReady = false;
    UPROPERTY(BlueprintReadOnly, Category="Hyperjump") bool bSurfaceLandable = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPHyperjumpRouteArrived, const FString&, WorldId);

/**
 * Connects the source travel state machine to a real ship and streamed world.
 * The surface represents one planet at a time. A jump swaps its source-seeded
 * profile only after charging, then places the ship above the new surface.
 * Flight, possession, and landing remain owned by their respective actors.
 */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPHyperjumpRouteComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USPHyperjumpRouteComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Existing navigation may be supplied; otherwise one is found or created on the owner. */
    UFUNCTION(BlueprintCallable, Category="Hyperjump")
    bool Configure(ASPFlightPawn* InShip, ASPWorldSurface* InSurface, USPTravelNavigationComponent* InNavigation = nullptr);

    UFUNCTION(BlueprintCallable, Category="Hyperjump") bool SelectDestination(const FString& WorldId);
    UFUNCTION(BlueprintCallable, Category="Hyperjump") bool CycleDestination();
    UFUNCTION(BlueprintCallable, Category="Hyperjump") bool RequestJump();
    UFUNCTION(BlueprintCallable, Category="Hyperjump") bool CancelJump();
    UFUNCTION(BlueprintPure, Category="Hyperjump") FSPHyperjumpRouteStatus GetRouteStatus() const;
    UFUNCTION(BlueprintPure, Category="Hyperjump") USPTravelNavigationComponent* GetNavigationComponent() const { return Navigation.Get(); }

    UPROPERTY(BlueprintAssignable, Category="Hyperjump") FSPHyperjumpRouteArrived OnArrived;

    /** Allow a ship/door controller to interlock the jump without changing the travel state machine. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hyperjump|Safety") bool bCargoHatchClosed = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hyperjump|Safety") bool bShipTransitioning = false;

    /** Visible time spent in transit after the two-second charge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hyperjump|Timing", meta=(ClampMin="0.1", ClampMax="10.0"))
    float TransitDurationSeconds = 1.0f;

    /** Arrival is safely above the 18 km local globe. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hyperjump|Arrival", meta=(ClampMin="2500.0", ClampMax="10000.0"))
    float ArrivalAltitudeMeters = 3500.0f;

private:
    UPROPERTY(Transient) TObjectPtr<ASPFlightPawn> Ship;
    UPROPERTY(Transient) TObjectPtr<ASPWorldSurface> Surface;
    UPROPERTY(Transient) TObjectPtr<USPTravelNavigationComponent> Navigation;
    UPROPERTY(Transient) FString LastMessage;
    float TransitElapsedSeconds = 0.0f;

    FSPTravelContext BuildContext() const;
    bool CompleteTransit();
    void SetMessage(const FString& Message);
};
