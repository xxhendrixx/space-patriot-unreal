#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SPTravelNavigationComponent.h"
#include "SPJourneySave.generated.h"

class APlayerController;
class APawn;
class ASPFlightPawn;
class ASPWorldSurface;
class USPTravelNavigationComponent;

/** One stable point in the walk / board / fly / jump / land journey. */
USTRUCT(BlueprintType)
struct FSPJourneyState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString WorldId = TEXT("earth");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FSPTravelNavigationState Navigation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform ShipTransform = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform GroundPawnTransform = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPiloting = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFlying = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGearDown = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPowered = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFlightAssist = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCruise = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FuelPercent = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeatPercent = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HullPercent = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ThrottleLimit = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TimeOfDayHours = 9.0f;
};

UCLASS()
class SPACEPATRIOTUNREAL_API USPSJourneySaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() FSPJourneyState Journey;
};

/**
 * Saves the physical play loop, while society, campaign and survey retain
 * their existing independent slots. Capture only at stable points: a jump
 * charge/transit and a landing animation cannot safely be resumed mid-frame.
 */
UCLASS()
class SPACEPATRIOTUNREAL_API USPJourneySaveLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Journey Save")
    static bool CaptureJourney(const APlayerController* Player, const APawn* GroundPawn,
        const ASPFlightPawn* Ship, const USPTravelNavigationComponent* Navigation,
        FSPJourneyState& OutState);

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Journey Save")
    static bool SaveJourney(const FSPJourneyState& State, const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Journey Save")
    static bool LoadJourney(const FString& SlotName, FSPJourneyState& OutState);

    /** Run only after the director has spawned/configured the ship and ground pawn. */
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Journey Save")
    static bool RestoreJourney(const FSPJourneyState& State, APlayerController* Player,
        APawn* GroundPawn, ASPFlightPawn* Ship, ASPWorldSurface* Surface,
        USPTravelNavigationComponent* Navigation);

    /** Reject malformed, conflicting or mid-transition snapshots before mutation. */
    UFUNCTION(BlueprintPure, Category="Space Patriot|Journey Save")
    static bool IsJourneyStateValid(const FSPJourneyState& State);
};
