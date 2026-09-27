#include "SPVesselSystemsComponent.h"

namespace
{
    constexpr int32 SubsystemCount = 6;
    constexpr int32 BusCount = 3;

    constexpr ESPVesselSubsystem DamageTargets[] = {
        ESPVesselSubsystem::Engines,
        ESPVesselSubsystem::Cooler,
        ESPVesselSubsystem::Weapons,
        ESPVesselSubsystem::Shields
    };

    FString SubsystemLabel(ESPVesselSubsystem Subsystem)
    {
        switch (Subsystem)
        {
        case ESPVesselSubsystem::Reactor: return TEXT("REACTOR");
        case ESPVesselSubsystem::Engines: return TEXT("ENGINES");
        case ESPVesselSubsystem::Weapons: return TEXT("WEAPONS");
        case ESPVesselSubsystem::Shields: return TEXT("SHIELDS");
        case ESPVesselSubsystem::Cooler: return TEXT("COOLER");
        case ESPVesselSubsystem::LifeSupport: return TEXT("LIFESUPPORT");
        default: return TEXT("UNKNOWN");
        }
    }
}

USPVesselSystemsComponent::USPVesselSystemsComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // An authoritative flight controller calls StepSystems.
    ResetToDefaults();
}

bool USPVesselSystemsComponent::IsValidSubsystem(ESPVesselSubsystem Subsystem)
{
    return static_cast<int32>(Subsystem) >= 0 && static_cast<int32>(Subsystem) < SubsystemCount;
}

bool USPVesselSystemsComponent::IsValidBus(ESPVesselBus Bus)
{
    return static_cast<int32>(Bus) >= 0 && static_cast<int32>(Bus) < BusCount;
}

const FSPVesselComponentStatus* USPVesselSystemsComponent::FindComponent(ESPVesselSubsystem Subsystem) const
{
    const int32 Index = static_cast<int32>(Subsystem);
    return IsValidSubsystem(Subsystem) && Components.IsValidIndex(Index) ? &Components[Index] : nullptr;
}

FSPVesselComponentStatus* USPVesselSystemsComponent::FindComponent(ESPVesselSubsystem Subsystem)
{
    const int32 Index = static_cast<int32>(Subsystem);
    return IsValidSubsystem(Subsystem) && Components.IsValidIndex(Index) ? &Components[Index] : nullptr;
}

void USPVesselSystemsComponent::ResetToDefaults(float InitialHullPercent)
{
    Mode = ESPVesselMode::SCM;
    Profile = ESPVesselProfile::Balanced;
    for (int32& Allocation : Allocations) Allocation = 4;
    Components.Reset(SubsystemCount);
    for (int32 Index = 0; Index < SubsystemCount; ++Index)
    {
        FSPVesselComponentStatus Status;
        Status.Subsystem = static_cast<ESPVesselSubsystem>(Index);
        Components.Add(Status);
    }
    PressurePercent = 100.0f;
    SpareParts = 120.0f;
    bRepairInProgress = false;
    Repairing = ESPVesselSubsystem::Reactor;
    ElapsedSeconds = 0.0f;
    LastHullPercent = FMath::IsFinite(InitialHullPercent) ? FMath::Clamp(InitialHullPercent, 0.0f, 100.0f) : 100.0f;
    Warning = TEXT("BUS NOMINAL");
    bCancelCruisePending = false;
    bCancelJumpPending = false;
}

int32 USPVesselSystemsComponent::GetAllocation(ESPVesselBus Bus) const
{
    return IsValidBus(Bus) ? Allocations[static_cast<int32>(Bus)] : 0;
}

float USPVesselSystemsComponent::GetFactor(ESPVesselSubsystem Subsystem, bool bPowerOn) const
{
    const FSPVesselComponentStatus* Component = FindComponent(Subsystem);
    const FSPVesselComponentStatus* Reactor = FindComponent(ESPVesselSubsystem::Reactor);
    if (!bPowerOn || !Component || !Reactor || !Component->bEnabled || !Reactor->bEnabled) return 0.0f;

    const float Integrity = (Component->HealthPercent / 100.0f) * (Reactor->HealthPercent / 100.0f);
    const float Thermal = FMath::Clamp(1.0f - FMath::Max(0.0f, Component->TemperatureCelsius - 82.0f) / 48.0f,
        0.15f, 1.0f);
    int32 Allocation = 4;
    switch (Subsystem)
    {
    case ESPVesselSubsystem::Engines: Allocation = Allocations[0]; break;
    case ESPVesselSubsystem::Weapons: Allocation = Allocations[1]; break;
    case ESPVesselSubsystem::Shields: Allocation = Allocations[2]; break;
    default: break;
    }
    return Integrity * Thermal * (static_cast<float>(Allocation) / 4.0f);
}

bool USPVesselSystemsComponent::CanFireShipWeapons(bool bWalking, bool bPowerOn) const
{
    return bWalking || (Mode == ESPVesselMode::SCM && GetFactor(ESPVesselSubsystem::Weapons, bPowerOn) >= 0.06f);
}

bool USPVesselSystemsComponent::SetMode(ESPVesselMode NewMode, bool bJumpInTransit)
{
    if ((NewMode != ESPVesselMode::SCM && NewMode != ESPVesselMode::NAV) || bJumpInTransit) return false;
    Mode = NewMode;
    if (Mode == ESPVesselMode::SCM)
    {
        bCancelCruisePending = true;
        bCancelJumpPending = true;
    }
    else
    {
        bCancelCruisePending = false;
        bCancelJumpPending = false;
    }
    return true;
}

bool USPVesselSystemsComponent::ApplyPreset(ESPVesselProfile NewProfile)
{
    switch (NewProfile)
    {
    case ESPVesselProfile::Balanced: Allocations[0] = 4; Allocations[1] = 4; Allocations[2] = 4; break;
    case ESPVesselProfile::Combat: Allocations[0] = 3; Allocations[1] = 6; Allocations[2] = 3; break;
    case ESPVesselProfile::Defense: Allocations[0] = 2; Allocations[1] = 2; Allocations[2] = 8; break;
    case ESPVesselProfile::Travel: Allocations[0] = 8; Allocations[1] = 2; Allocations[2] = 2; break;
    default: return false;
    }
    Profile = NewProfile;
    return true;
}

bool USPVesselSystemsComponent::AdjustAllocation(ESPVesselBus Bus, int32 Delta)
{
    if (!IsValidBus(Bus) || (Delta != 1 && Delta != -1)) return false;
    const int32 Target = static_cast<int32>(Bus);
    if (Allocations[Target] + Delta < 0 || Allocations[Target] + Delta > 10) return false;

    int32 Other = INDEX_NONE;
    for (int32 Index = 0; Index < BusCount; ++Index)
    {
        if (Index == Target || (Delta > 0 ? Allocations[Index] <= 0 : Allocations[Index] >= 10)) continue;
        if (Other == INDEX_NONE || (Delta > 0 ? Allocations[Index] > Allocations[Other] : Allocations[Index] < Allocations[Other]))
            Other = Index;
    }
    if (Other == INDEX_NONE) return false;
    Allocations[Target] += Delta;
    Allocations[Other] -= Delta;
    Profile = ESPVesselProfile::Custom;
    return true;
}

bool USPVesselSystemsComponent::ToggleComponent(ESPVesselSubsystem Subsystem)
{
    FSPVesselComponentStatus* Component = FindComponent(Subsystem);
    return Component && SetComponentEnabled(Subsystem, !Component->bEnabled);
}

bool USPVesselSystemsComponent::SetComponentEnabled(ESPVesselSubsystem Subsystem, bool bEnabled)
{
    FSPVesselComponentStatus* Component = FindComponent(Subsystem);
    if (!Component) return false;
    Component->bEnabled = bEnabled;
    if (bEnabled && bRepairInProgress && Repairing == Subsystem) bRepairInProgress = false;
    return true;
}

bool USPVesselSystemsComponent::ApplyComponentDamage(ESPVesselSubsystem Subsystem, float DamagePercent)
{
    FSPVesselComponentStatus* Component = FindComponent(Subsystem);
    if (!Component || !FMath::IsFinite(DamagePercent) || DamagePercent <= 0.0f) return false;
    Component->HealthPercent = FMath::Max(0.0f, Component->HealthPercent - DamagePercent);
    return true;
}

bool USPVesselSystemsComponent::StartRepair(ESPVesselSubsystem Subsystem)
{
    const FSPVesselComponentStatus* Component = FindComponent(Subsystem);
    if (!Component || Component->bEnabled || Component->HealthPercent >= 100.0f || SpareParts <= 0.0f)
    {
        Warning = TEXT("ISOLATE A DAMAGED COMPONENT TO REPAIR");
        return false;
    }
    Repairing = Subsystem;
    bRepairInProgress = true;
    return true;
}

bool USPVesselSystemsComponent::TryService(bool bLandedOrDocked, bool bWalking, float CurrentHullPercent)
{
    if (!bLandedOrDocked || bWalking)
    {
        Warning = TEXT("LAND OR DOCK TO SERVICE");
        return false;
    }
    for (FSPVesselComponentStatus& Component : Components)
    {
        Component.HealthPercent = 100.0f;
        Component.TemperatureCelsius = 24.0f;
        Component.bEnabled = true;
    }
    PressurePercent = 100.0f;
    SpareParts = 120.0f;
    bRepairInProgress = false;
    LastHullPercent = FMath::IsFinite(CurrentHullPercent) ? FMath::Clamp(CurrentHullPercent, 0.0f, 100.0f) : 100.0f;
    Warning = TEXT("BUS NOMINAL");
    return true;
}

FSPVesselSimulationOutput USPVesselSystemsComponent::StepSystems(float DeltaSeconds, const FSPVesselSimulationInput& Input)
{
    FSPVesselSimulationOutput Output;
    Output.ShipHeat01 = FMath::IsFinite(Input.ShipHeat01) ? FMath::Clamp(Input.ShipHeat01, 0.0f, 1.0f) : 0.0f;
    Output.ShieldPercent = FMath::IsFinite(Input.ShieldPercent) ? FMath::Clamp(Input.ShieldPercent, 0.0f, 100.0f) : 0.0f;
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f) return Output;
    if (!FMath::IsFinite(Input.HullPercent) || !FMath::IsFinite(Input.Thrust01)
        || !FMath::IsFinite(Input.WeaponHeat01) || !FMath::IsFinite(Input.CruiseSpool01)) return Output;

    const float Dt = FMath::Min(DeltaSeconds, 0.1f); // Original update wrapper caps long frames.
    ElapsedSeconds += Dt;
    const bool bCancellingNavigation = bCancelCruisePending || bCancelJumpPending;
    if (Mode == ESPVesselMode::SCM && !bCancellingNavigation
        && (Input.CruiseSpool01 > 0.02f || Input.bJumpActive) && !Input.bJumpInTransit)
        SetMode(ESPVesselMode::NAV, false);

    const float Hull = FMath::Clamp(Input.HullPercent, 0.0f, 100.0f);
    const float Damage = FMath::Max(0.0f, LastHullPercent - Hull);
    LastHullPercent = Hull;
    if (Damage > 0.0f)
    {
        const int32 Target = FMath::FloorToInt(ElapsedSeconds * 17.0f) % UE_ARRAY_COUNT(DamageTargets);
        ApplyComponentDamage(DamageTargets[Target], Damage * 0.6f);
    }

    const float Cooling = GetFactor(ESPVesselSubsystem::Cooler, Input.bPowerOn);
    for (FSPVesselComponentStatus& Component : Components)
    {
        float Load = 0.15f;
        int32 Allocation = 4;
        switch (Component.Subsystem)
        {
        case ESPVesselSubsystem::Engines:
            Load = FMath::Clamp(Input.Thrust01, 0.0f, 1.0f);
            Allocation = Allocations[0];
            break;
        case ESPVesselSubsystem::Weapons:
            Load = FMath::Clamp(Input.WeaponHeat01, 0.0f, 1.0f);
            Allocation = Allocations[1];
            break;
        case ESPVesselSubsystem::Shields:
            Load = (100.0f - Output.ShieldPercent) / 120.0f;
            Allocation = Allocations[2];
            break;
        default: break;
        }
        const float Target = 24.0f + (Component.bEnabled
            ? Load * 46.0f + Output.ShipHeat01 * 38.0f + FMath::Max(0, Allocation - 4) * 3.0f : 0.0f);
        Component.TemperatureCelsius += (Target - Component.TemperatureCelsius)
            * FMath::Min(1.0f, Dt * (0.22f + 0.15f * Cooling));
        if (Component.TemperatureCelsius > 100.0f)
            Component.HealthPercent = FMath::Max(0.0f, Component.HealthPercent - Dt * 0.18f);
    }
    if (Cooling < 0.7f && Input.Thrust01 > 0.1f)
        Output.ShipHeat01 = FMath::Clamp(Output.ShipHeat01 + Dt * (0.7f - Cooling) * 0.04f, 0.0f, 1.0f);
    if (Mode == ESPVesselMode::NAV)
        Output.ShieldPercent = FMath::Max(0.0f, Output.ShieldPercent - Dt * 16.0f);

    PressurePercent = FMath::Clamp(PressurePercent + Dt
        * (GetFactor(ESPVesselSubsystem::LifeSupport, Input.bPowerOn) > 0.15f ? 1.5f : -0.7f), 0.0f, 100.0f);
    if (bRepairInProgress)
    {
        FSPVesselComponentStatus* Component = FindComponent(Repairing);
        if (!Component || Component->bEnabled || Component->HealthPercent >= 100.0f || SpareParts <= 0.0f)
            bRepairInProgress = false;
        else
        {
            const float Amount = FMath::Min3(Dt * 3.0f, SpareParts, 100.0f - Component->HealthPercent);
            Component->HealthPercent += Amount;
            SpareParts -= Amount;
        }
    }

    if (PressurePercent < 70.0f)
        Warning = TEXT("CABIN PRESSURE LOW");
    else
    {
        const FSPVesselComponentStatus* Bad = Components.FindByPredicate([](const FSPVesselComponentStatus& Component)
        {
            return Component.HealthPercent < 35.0f || Component.TemperatureCelsius > 95.0f;
        });
        if (Bad) Warning = SubsystemLabel(Bad->Subsystem) + TEXT(" SERVICE REQUIRED");
        else if (Mode == ESPVesselMode::NAV) Warning = TEXT("NAV · SHIELDS DISCHARGING");
        else if (bRepairInProgress) Warning = TEXT("REPAIR IN PROGRESS");
        else Warning = TEXT("BUS NOMINAL");
    }

    Output.EngineFactor = GetFactor(ESPVesselSubsystem::Engines, Input.bPowerOn);
    Output.WeaponFactor = GetFactor(ESPVesselSubsystem::Weapons, Input.bPowerOn);
    Output.ShieldFactor = GetFactor(ESPVesselSubsystem::Shields, Input.bPowerOn);
    Output.CoolingFactor = GetFactor(ESPVesselSubsystem::Cooler, Input.bPowerOn);
    Output.LifeSupportFactor = GetFactor(ESPVesselSubsystem::LifeSupport, Input.bPowerOn);
    Output.bDisarmWeapons = Mode == ESPVesselMode::NAV;
    Output.bCancelCruise = bCancelCruisePending;
    Output.bCancelJump = bCancelJumpPending;
    bCancelCruisePending = false;
    bCancelJumpPending = false;
    return Output;
}

bool USPVesselSystemsComponent::GetComponentStatus(ESPVesselSubsystem Subsystem, FSPVesselComponentStatus& Status) const
{
    const FSPVesselComponentStatus* Component = FindComponent(Subsystem);
    if (!Component) return false;
    Status = *Component;
    return true;
}

FSPVesselTelemetry USPVesselSystemsComponent::GetTelemetry(bool bPowerOn) const
{
    FSPVesselTelemetry Telemetry;
    Telemetry.Mode = Mode;
    Telemetry.Profile = Profile;
    Telemetry.EngineAllocation = Allocations[0];
    Telemetry.WeaponAllocation = Allocations[1];
    Telemetry.ShieldAllocation = Allocations[2];
    Telemetry.PressurePercent = PressurePercent;
    Telemetry.SpareParts = SpareParts;
    Telemetry.Repairing = Repairing;
    Telemetry.bRepairInProgress = bRepairInProgress;
    Telemetry.Warning = Warning;
    Telemetry.Components = Components;
    Telemetry.EngineFactor = GetFactor(ESPVesselSubsystem::Engines, bPowerOn);
    Telemetry.WeaponFactor = GetFactor(ESPVesselSubsystem::Weapons, bPowerOn);
    Telemetry.ShieldFactor = GetFactor(ESPVesselSubsystem::Shields, bPowerOn);
    Telemetry.CoolingFactor = GetFactor(ESPVesselSubsystem::Cooler, bPowerOn);
    Telemetry.LifeSupportFactor = GetFactor(ESPVesselSubsystem::LifeSupport, bPowerOn);
    return Telemetry;
}

FSPVesselSnapshot USPVesselSystemsComponent::CaptureState() const
{
    FSPVesselSnapshot Snapshot;
    Snapshot.Mode = Mode;
    Snapshot.Profile = Profile;
    Snapshot.EngineAllocation = Allocations[0];
    Snapshot.WeaponAllocation = Allocations[1];
    Snapshot.ShieldAllocation = Allocations[2];
    Snapshot.Components = Components;
    Snapshot.PressurePercent = PressurePercent;
    Snapshot.SpareParts = SpareParts;
    Snapshot.bRepairInProgress = bRepairInProgress;
    Snapshot.Repairing = Repairing;
    Snapshot.ElapsedSeconds = ElapsedSeconds;
    Snapshot.LastHullPercent = LastHullPercent;
    return Snapshot;
}

bool USPVesselSystemsComponent::RestoreState(const FSPVesselSnapshot& Snapshot)
{
    if ((Snapshot.Mode != ESPVesselMode::SCM && Snapshot.Mode != ESPVesselMode::NAV)
        || static_cast<int32>(Snapshot.Profile) < 0 || static_cast<int32>(Snapshot.Profile) > static_cast<int32>(ESPVesselProfile::Custom)
        || Snapshot.EngineAllocation < 0 || Snapshot.EngineAllocation > 10
        || Snapshot.WeaponAllocation < 0 || Snapshot.WeaponAllocation > 10
        || Snapshot.ShieldAllocation < 0 || Snapshot.ShieldAllocation > 10
        || Snapshot.EngineAllocation + Snapshot.WeaponAllocation + Snapshot.ShieldAllocation != 12
        || Snapshot.Components.Num() != SubsystemCount || !IsValidSubsystem(Snapshot.Repairing)
        || !FMath::IsFinite(Snapshot.PressurePercent) || Snapshot.PressurePercent < 0.0f || Snapshot.PressurePercent > 100.0f
        || !FMath::IsFinite(Snapshot.SpareParts) || Snapshot.SpareParts < 0.0f || Snapshot.SpareParts > 120.0f
        || !FMath::IsFinite(Snapshot.ElapsedSeconds) || Snapshot.ElapsedSeconds < 0.0f
        || !FMath::IsFinite(Snapshot.LastHullPercent) || Snapshot.LastHullPercent < 0.0f || Snapshot.LastHullPercent > 100.0f)
        return false;
    for (int32 Index = 0; Index < SubsystemCount; ++Index)
    {
        const FSPVesselComponentStatus& Component = Snapshot.Components[Index];
        if (Component.Subsystem != static_cast<ESPVesselSubsystem>(Index)
            || !FMath::IsFinite(Component.HealthPercent) || Component.HealthPercent < 0.0f || Component.HealthPercent > 100.0f
            || !FMath::IsFinite(Component.TemperatureCelsius) || Component.TemperatureCelsius < -273.15f
            || Component.TemperatureCelsius > 1000.0f)
            return false;
    }
    Mode = Snapshot.Mode;
    Profile = Snapshot.Profile;
    Allocations[0] = Snapshot.EngineAllocation;
    Allocations[1] = Snapshot.WeaponAllocation;
    Allocations[2] = Snapshot.ShieldAllocation;
    Components = Snapshot.Components;
    PressurePercent = Snapshot.PressurePercent;
    SpareParts = Snapshot.SpareParts;
    bRepairInProgress = Snapshot.bRepairInProgress;
    Repairing = Snapshot.Repairing;
    ElapsedSeconds = Snapshot.ElapsedSeconds;
    LastHullPercent = Snapshot.LastHullPercent;
    Warning = TEXT("BUS NOMINAL");
    bCancelCruisePending = false;
    bCancelJumpPending = false;
    return true;
}
