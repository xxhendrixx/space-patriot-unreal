#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPTravelNavigationComponent.generated.h"

/** The source game's SCM/NAV switch also gates weapons in NAV. */
UENUM(BlueprintType)
enum class ESPTravelDriveMode : uint8
{
    SCM,
    NAV
};

/** Travel is deliberately separate from the pawn's physical motion and collision. */
UENUM(BlueprintType)
enum class ESPTravelPhase : uint8
{
    Flight,
    JumpCharging,
    JumpTransit,
    LandingRequested,
    Landed,
    Docked
};

USTRUCT(BlueprintType)
struct FSPTravelWorld
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString System;
    UPROPERTY(BlueprintReadOnly) FString Biome;
};

/** Collision, motion, and pilot facts supplied by the live ship/world actors. */
USTRUCT(BlueprintType)
struct FSPTravelContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPilotAtControls = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShipPowered = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGearRetracted = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShipTransitioning = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCargoHatchClosed = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSafeLandingFootprint = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAtStationPad = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double AltitudeKm = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double NearbyBodyRadiusKm = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double NearestStationDistanceKm = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double SpeedMetersPerSecond = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double SurfaceClearanceMeters = 1000000.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString StationId;
};

/** Save-game payload; restoring never invents a rendered route or a touchdown. */
USTRUCT(BlueprintType)
struct FSPTravelNavigationState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CurrentWorldId = TEXT("earth");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DestinationWorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DockedStationId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPTravelDriveMode DriveMode = ESPTravelDriveMode::SCM;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPTravelPhase Phase = ESPTravelPhase::Flight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCruiseLatched = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CruiseSpool = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float JumpChargeSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FuelPercent = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeatNormalized = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPTravelPhaseChanged, ESPTravelPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPTravelWorldArrived, const FString&, WorldId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPTravelResourceChanged, float, FuelPercent);

/**
 * Native travel authority from expedition.js and systems.js. The 19 IDs come
 * from Data/Worlds.json. The caller must feed live collision/flight context,
 * move the ship along its route, and explicitly confirm transit and touchdown.
 */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPTravelNavigationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPTravelNavigationComponent();

    UPROPERTY(BlueprintAssignable, Category="Travel|Events") FSPTravelPhaseChanged OnPhaseChanged;
    UPROPERTY(BlueprintAssignable, Category="Travel|Events") FSPTravelWorldArrived OnWorldArrived;
    UPROPERTY(BlueprintAssignable, Category="Travel|Events") FSPTravelResourceChanged OnFuelChanged;

    UFUNCTION(BlueprintCallable, Category="Travel|Worlds") bool LoadWorldCatalog();
    UFUNCTION(BlueprintPure, Category="Travel|Worlds") TArray<FSPTravelWorld> GetWorlds() const { return Worlds; }
    UFUNCTION(BlueprintPure, Category="Travel|Worlds") bool FindWorld(const FString& WorldId, FSPTravelWorld& OutWorld) const;
    UFUNCTION(BlueprintCallable, Category="Travel|Worlds") bool SelectDestination(const FString& WorldId);
    UFUNCTION(BlueprintCallable, Category="Travel|Worlds") bool CycleDestination();
    UFUNCTION(BlueprintPure, Category="Travel|Status") FSPTravelNavigationState GetNavigationState() const { return State; }
    UFUNCTION(BlueprintPure, Category="Travel|Status") bool AreWeaponsInhibited() const { return State.DriveMode == ESPTravelDriveMode::NAV; }
    UFUNCTION(BlueprintPure, Category="Travel|Status") float GetJumpChargeFraction() const { return FMath::Clamp(State.JumpChargeSeconds / 2.0f, 0.0f, 1.0f); }

    UFUNCTION(BlueprintCallable, Category="Travel|Drive") bool SetDriveResources(float FuelPercent, float HeatNormalized);
    UFUNCTION(BlueprintCallable, Category="Travel|Drive") bool SetDriveMode(ESPTravelDriveMode Mode);
    UFUNCTION(BlueprintCallable, Category="Travel|Drive") bool SetCruiseLatched(bool bLatched, const FSPTravelContext& Context);
    UFUNCTION(BlueprintCallable, Category="Travel|Drive") void AdvanceDrive(float DeltaSeconds, const FSPTravelContext& Context, bool bCruiseHeld, bool bBrakeHeld);
    UFUNCTION(BlueprintCallable, Category="Travel|Jump") bool BeginJump(const FSPTravelContext& Context);
    UFUNCTION(BlueprintCallable, Category="Travel|Jump") bool CancelJump();
    /** Spatial route failed after charge; return control without inventing an arrival or refund. */
    UFUNCTION(BlueprintCallable, Category="Travel|Jump") bool AbortJumpTransit();
    /** Call only after the spatial route actor reaches its exterior arrival marker. */
    UFUNCTION(BlueprintCallable, Category="Travel|Jump") bool ConfirmJumpArrival();

    /** Request at any safe dry surface, not only a designated landing pad. */
    UFUNCTION(BlueprintCallable, Category="Travel|Landing") bool RequestSurfaceLanding(const FSPTravelContext& Context);
    /** Physical landing actor confirms contact after its collision/animation checks. */
    UFUNCTION(BlueprintCallable, Category="Travel|Landing") bool ConfirmSurfaceTouchdown(const FSPTravelContext& Context);
    UFUNCTION(BlueprintCallable, Category="Travel|Landing") bool CancelSurfaceLanding();
    UFUNCTION(BlueprintCallable, Category="Travel|Dock") bool DockAtStation(const FSPTravelContext& Context);
    UFUNCTION(BlueprintCallable, Category="Travel|Dock") bool Undock();
    UFUNCTION(BlueprintCallable, Category="Travel|Landing") bool Launch(const FSPTravelContext& Context);

    UFUNCTION(BlueprintPure, Category="Travel|Save") FSPTravelNavigationState CaptureSaveState() const { return State; }
    UFUNCTION(BlueprintCallable, Category="Travel|Save") bool RestoreSaveState(const FSPTravelNavigationState& Saved);

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="Travel|Worlds") TArray<FSPTravelWorld> Worlds;
    UPROPERTY(VisibleAnywhere, Category="Travel|Status") FSPTravelNavigationState State;

    FString CanonicalWorldId(const FString& WorldId) const;
    bool IsValidWorld(const FString& WorldId) const;
    bool IsJumpSafe(const FSPTravelContext& Context) const;
    bool IsLandingSafe(const FSPTravelContext& Context) const;
    void ChangePhase(ESPTravelPhase NewPhase);
};
