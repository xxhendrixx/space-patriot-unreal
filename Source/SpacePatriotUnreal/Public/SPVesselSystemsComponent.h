#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPVesselSystemsComponent.generated.h"

UENUM(BlueprintType)
enum class ESPVesselSubsystem : uint8
{
    Reactor,
    Engines,
    Weapons,
    Shields,
    Cooler,
    LifeSupport
};

UENUM(BlueprintType)
enum class ESPVesselBus : uint8
{
    Engines,
    Weapons,
    Shields
};

UENUM(BlueprintType)
enum class ESPVesselProfile : uint8
{
    Balanced,
    Combat,
    Defense,
    Travel,
    Custom
};

UENUM(BlueprintType)
enum class ESPVesselMode : uint8
{
    SCM,
    NAV
};

USTRUCT(BlueprintType)
struct FSPVesselComponentStatus
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPVesselSubsystem Subsystem = ESPVesselSubsystem::Reactor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HealthPercent = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TemperatureCelsius = 24.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = true;
};

/** Inputs supplied by the authoritative ship/combat controller once per simulation step. */
USTRUCT(BlueprintType)
struct FSPVesselSimulationInput
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPowerOn = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bJumpActive = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bJumpInTransit = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CruiseSpool01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HullPercent = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShipHeat01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Thrust01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeaponHeat01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShieldPercent = 100.0f;
};

/** Apply these values/requests to the real flight and combat controllers after each step. */
USTRUCT(BlueprintType)
struct FSPVesselSimulationOutput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float ShipHeat01 = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ShieldPercent = 100.0f;
    UPROPERTY(BlueprintReadOnly) float EngineFactor = 0.0f;
    UPROPERTY(BlueprintReadOnly) float WeaponFactor = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ShieldFactor = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CoolingFactor = 0.0f;
    UPROPERTY(BlueprintReadOnly) float LifeSupportFactor = 0.0f;
    UPROPERTY(BlueprintReadOnly) bool bDisarmWeapons = false;
    UPROPERTY(BlueprintReadOnly) bool bCancelCruise = false;
    UPROPERTY(BlueprintReadOnly) bool bCancelJump = false;
};

USTRUCT(BlueprintType)
struct FSPVesselTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) ESPVesselMode Mode = ESPVesselMode::SCM;
    UPROPERTY(BlueprintReadOnly) ESPVesselProfile Profile = ESPVesselProfile::Balanced;
    UPROPERTY(BlueprintReadOnly) int32 EngineAllocation = 4;
    UPROPERTY(BlueprintReadOnly) int32 WeaponAllocation = 4;
    UPROPERTY(BlueprintReadOnly) int32 ShieldAllocation = 4;
    UPROPERTY(BlueprintReadOnly) float PressurePercent = 100.0f;
    UPROPERTY(BlueprintReadOnly) float SpareParts = 120.0f;
    UPROPERTY(BlueprintReadOnly) ESPVesselSubsystem Repairing = ESPVesselSubsystem::Reactor;
    UPROPERTY(BlueprintReadOnly) bool bRepairInProgress = false;
    UPROPERTY(BlueprintReadOnly) FString Warning = TEXT("BUS NOMINAL");
    UPROPERTY(BlueprintReadOnly) TArray<FSPVesselComponentStatus> Components;
    UPROPERTY(BlueprintReadOnly) float EngineFactor = 0.0f;
    UPROPERTY(BlueprintReadOnly) float WeaponFactor = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ShieldFactor = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CoolingFactor = 0.0f;
    UPROPERTY(BlueprintReadOnly) float LifeSupportFactor = 0.0f;
};

/** Compact state for integration with a ship SaveGame; no asset or renderer is stored. */
USTRUCT(BlueprintType)
struct FSPVesselSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPVesselMode Mode = ESPVesselMode::SCM;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPVesselProfile Profile = ESPVesselProfile::Balanced;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 EngineAllocation = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WeaponAllocation = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ShieldAllocation = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSPVesselComponentStatus> Components;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PressurePercent = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpareParts = 120.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRepairInProgress = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPVesselSubsystem Repairing = ESPVesselSubsystem::Reactor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ElapsedSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LastHullPercent = 100.0f;
};

/** Source VesselSystems power, damage, thermal and repair rules; presentation remains Blueprint-owned. */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPVesselSystemsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USPVesselSystemsComponent();

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") void ResetToDefaults(float InitialHullPercent = 100.0f);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") FSPVesselSimulationOutput StepSystems(float DeltaSeconds, const FSPVesselSimulationInput& Input);
    UFUNCTION(BlueprintPure, Category="Space Patriot|Vessel") FSPVesselTelemetry GetTelemetry(bool bPowerOn) const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Vessel") bool GetComponentStatus(ESPVesselSubsystem Subsystem, FSPVesselComponentStatus& Status) const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Vessel") float GetFactor(ESPVesselSubsystem Subsystem, bool bPowerOn) const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Vessel") int32 GetAllocation(ESPVesselBus Bus) const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Vessel") bool CanFireShipWeapons(bool bWalking, bool bPowerOn) const;
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") bool SetMode(ESPVesselMode NewMode, bool bJumpInTransit);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") bool ApplyPreset(ESPVesselProfile NewProfile);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") bool AdjustAllocation(ESPVesselBus Bus, int32 Delta);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") bool ToggleComponent(ESPVesselSubsystem Subsystem);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") bool SetComponentEnabled(ESPVesselSubsystem Subsystem, bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") bool ApplyComponentDamage(ESPVesselSubsystem Subsystem, float DamagePercent);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") bool StartRepair(ESPVesselSubsystem Subsystem);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel") bool TryService(bool bLandedOrDocked, bool bWalking, float CurrentHullPercent);
    UFUNCTION(BlueprintPure, Category="Space Patriot|Vessel|Save") FSPVesselSnapshot CaptureState() const;
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Vessel|Save") bool RestoreState(const FSPVesselSnapshot& Snapshot);

private:
    UPROPERTY(SaveGame) TArray<FSPVesselComponentStatus> Components;
    UPROPERTY(SaveGame) ESPVesselMode Mode = ESPVesselMode::SCM;
    UPROPERTY(SaveGame) ESPVesselProfile Profile = ESPVesselProfile::Balanced;
    UPROPERTY(SaveGame) int32 Allocations[3] = {4, 4, 4};
    UPROPERTY(SaveGame) float PressurePercent = 100.0f;
    UPROPERTY(SaveGame) float SpareParts = 120.0f;
    UPROPERTY(SaveGame) bool bRepairInProgress = false;
    UPROPERTY(SaveGame) ESPVesselSubsystem Repairing = ESPVesselSubsystem::Reactor;
    UPROPERTY(SaveGame) float ElapsedSeconds = 0.0f;
    UPROPERTY(SaveGame) float LastHullPercent = 100.0f;
    UPROPERTY() FString Warning = TEXT("BUS NOMINAL");
    bool bCancelCruisePending = false;
    bool bCancelJumpPending = false;

    static bool IsValidSubsystem(ESPVesselSubsystem Subsystem);
    static bool IsValidBus(ESPVesselBus Bus);
    const FSPVesselComponentStatus* FindComponent(ESPVesselSubsystem Subsystem) const;
    FSPVesselComponentStatus* FindComponent(ESPVesselSubsystem Subsystem);
};
