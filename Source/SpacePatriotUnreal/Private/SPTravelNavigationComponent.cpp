#include "SPTravelNavigationComponent.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

USPTravelNavigationComponent::USPTravelNavigationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USPTravelNavigationComponent::BeginPlay()
{
    Super::BeginPlay();
    if (Worlds.IsEmpty()) LoadWorldCatalog();
}

FString USPTravelNavigationComponent::CanonicalWorldId(const FString& WorldId) const
{
    FString Id = WorldId.TrimStartAndEnd().ToLower();
    if (Id.StartsWith(TEXT("sp-"))) Id.RightChopInline(3);
    return Id;
}

bool USPTravelNavigationComponent::IsValidWorld(const FString& WorldId) const
{
    return Worlds.ContainsByPredicate([&WorldId](const FSPTravelWorld& World) { return World.Id == WorldId; });
}

bool USPTravelNavigationComponent::LoadWorldCatalog()
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Worlds.json")))) return false;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Root->TryGetArrayField(TEXT("worlds"), Values) || !Values) return false;

    TArray<FSPTravelWorld> Loaded;
    TSet<FString> Seen;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Object.IsValid()) return false;
        FSPTravelWorld World;
        if (!Object->TryGetStringField(TEXT("id"), World.Id) ||
            !Object->TryGetStringField(TEXT("name"), World.Name) ||
            !Object->TryGetStringField(TEXT("system"), World.System) ||
            !Object->TryGetStringField(TEXT("biome"), World.Biome)) return false;
        World.Id = CanonicalWorldId(World.Id);
        if (World.Id.IsEmpty() || World.Name.IsEmpty() || Seen.Contains(World.Id)) return false;
        Seen.Add(World.Id);
        Loaded.Add(MoveTemp(World));
    }
    if (Loaded.Num() != 19 || !Seen.Contains(TEXT("earth"))) return false;
    Worlds = MoveTemp(Loaded);
    return true;
}

bool USPTravelNavigationComponent::FindWorld(const FString& WorldId, FSPTravelWorld& OutWorld) const
{
    const FString Key = CanonicalWorldId(WorldId);
    if (const FSPTravelWorld* World = Worlds.FindByPredicate([&Key](const FSPTravelWorld& Item) { return Item.Id == Key; }))
    {
        OutWorld = *World;
        return true;
    }
    return false;
}

bool USPTravelNavigationComponent::SelectDestination(const FString& WorldId)
{
    const FString Key = CanonicalWorldId(WorldId);
    if (State.Phase == ESPTravelPhase::JumpCharging || State.Phase == ESPTravelPhase::JumpTransit ||
        Key == State.CurrentWorldId || !IsValidWorld(Key)) return false;
    State.DestinationWorldId = Key;
    return true;
}

bool USPTravelNavigationComponent::CycleDestination()
{
    if (State.Phase == ESPTravelPhase::JumpCharging || State.Phase == ESPTravelPhase::JumpTransit || Worlds.Num() < 2) return false;
    int32 Index = Worlds.IndexOfByPredicate([this](const FSPTravelWorld& World) { return World.Id == State.DestinationWorldId; });
    for (int32 Count = 0; Count < Worlds.Num(); ++Count)
    {
        Index = (Index + 1) % Worlds.Num();
        if (Worlds[Index].Id != State.CurrentWorldId)
        {
            State.DestinationWorldId = Worlds[Index].Id;
            return true;
        }
    }
    return false;
}

bool USPTravelNavigationComponent::SetDriveResources(float FuelPercent, float HeatNormalized)
{
    if (!FMath::IsFinite(FuelPercent) || FuelPercent < 0.0f || FuelPercent > 100.0f ||
        !FMath::IsFinite(HeatNormalized) || HeatNormalized < 0.0f || HeatNormalized > 1.0f) return false;
    State.FuelPercent = FuelPercent;
    State.HeatNormalized = HeatNormalized;
    return true;
}

void USPTravelNavigationComponent::ChangePhase(ESPTravelPhase NewPhase)
{
    if (State.Phase == NewPhase) return;
    State.Phase = NewPhase;
    OnPhaseChanged.Broadcast(NewPhase);
}

bool USPTravelNavigationComponent::SetDriveMode(ESPTravelDriveMode Mode)
{
    if (State.Phase == ESPTravelPhase::JumpTransit) return false;
    if (Mode == ESPTravelDriveMode::SCM)
    {
        State.bCruiseLatched = false;
        State.CruiseSpool = 0.0f;
        if (State.Phase == ESPTravelPhase::JumpCharging)
        {
            State.JumpChargeSeconds = 0.0f;
            ChangePhase(ESPTravelPhase::Flight);
        }
    }
    State.DriveMode = Mode;
    return true;
}

bool USPTravelNavigationComponent::SetCruiseLatched(bool bLatched, const FSPTravelContext& Context)
{
    if (bLatched && (State.Phase != ESPTravelPhase::Flight || !Context.bPilotAtControls ||
        !Context.bShipPowered || !Context.bGearRetracted || Context.bShipTransitioning ||
        State.FuelPercent <= 0.0f || State.HeatNormalized >= 0.94f)) return false;
    State.bCruiseLatched = bLatched;
    if (bLatched) State.DriveMode = ESPTravelDriveMode::NAV;
    return true;
}

bool USPTravelNavigationComponent::IsJumpSafe(const FSPTravelContext& Context) const
{
    return (State.Phase == ESPTravelPhase::Flight || State.Phase == ESPTravelPhase::JumpCharging) &&
        Context.bPilotAtControls && Context.bShipPowered && Context.bGearRetracted &&
        !Context.bShipTransitioning && Context.bCargoHatchClosed &&
        FMath::IsFinite(Context.AltitudeKm) && FMath::IsFinite(Context.NearbyBodyRadiusKm) &&
        Context.NearbyBodyRadiusKm >= 0.0 &&
        Context.AltitudeKm > FMath::Max(2.0, Context.NearbyBodyRadiusKm * 0.006) &&
        FMath::IsFinite(Context.NearestStationDistanceKm) && Context.NearestStationDistanceKm > 5.0 &&
        State.FuelPercent >= 8.0f && State.HeatNormalized < 0.84f;
}

bool USPTravelNavigationComponent::BeginJump(const FSPTravelContext& Context)
{
    if (State.Phase != ESPTravelPhase::Flight || State.DestinationWorldId.IsEmpty() ||
        State.DestinationWorldId == State.CurrentWorldId || !IsValidWorld(State.DestinationWorldId) ||
        !IsJumpSafe(Context)) return false;
    State.DriveMode = ESPTravelDriveMode::NAV;
    State.bCruiseLatched = false;
    State.CruiseSpool = 0.0f;
    State.JumpChargeSeconds = 0.0f;
    ChangePhase(ESPTravelPhase::JumpCharging);
    return true;
}

bool USPTravelNavigationComponent::CancelJump()
{
    if (State.Phase != ESPTravelPhase::JumpCharging) return false;
    State.JumpChargeSeconds = 0.0f;
    ChangePhase(ESPTravelPhase::Flight);
    return true;
}

void USPTravelNavigationComponent::AdvanceDrive(float DeltaSeconds, const FSPTravelContext& Context, bool bCruiseHeld, bool bBrakeHeld)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f) return;
    const float Dt = FMath::Min(DeltaSeconds, 0.1f);
    if (bBrakeHeld)
    {
        State.bCruiseLatched = false;
        CancelJump();
    }
    if (State.Phase == ESPTravelPhase::JumpCharging)
    {
        if (!IsJumpSafe(Context))
        {
            CancelJump();
            return;
        }
        State.JumpChargeSeconds = FMath::Min(2.0f, State.JumpChargeSeconds + Dt);
        if (State.JumpChargeSeconds >= 2.0f)
        {
            State.FuelPercent = FMath::Max(0.0f, State.FuelPercent - 7.0f);
            State.HeatNormalized = FMath::Min(1.0f, State.HeatNormalized + 0.16f);
            OnFuelChanged.Broadcast(State.FuelPercent);
            ChangePhase(ESPTravelPhase::JumpTransit);
        }
        return;
    }
    const bool bCanCruise = State.Phase == ESPTravelPhase::Flight && Context.bPilotAtControls &&
        Context.bShipPowered && Context.bGearRetracted && !Context.bShipTransitioning &&
        State.FuelPercent > 0.0f && State.HeatNormalized < 0.94f;
    const bool bWantsCruise = bCanCruise && !bBrakeHeld && (bCruiseHeld || State.bCruiseLatched);
    if (!bCanCruise) State.bCruiseLatched = false;
    State.CruiseSpool = FMath::FInterpConstantTo(State.CruiseSpool, bWantsCruise ? 1.0f : 0.0f,
        Dt, bWantsCruise ? (1.0f / 3.0f) : (1.0f / 1.2f));
    if (State.CruiseSpool > 0.02f) State.DriveMode = ESPTravelDriveMode::NAV;
}

bool USPTravelNavigationComponent::ConfirmJumpArrival()
{
    if (State.Phase != ESPTravelPhase::JumpTransit || !IsValidWorld(State.DestinationWorldId)) return false;
    State.CurrentWorldId = State.DestinationWorldId;
    State.DestinationWorldId.Empty();
    State.JumpChargeSeconds = 0.0f;
    ChangePhase(ESPTravelPhase::Flight);
    OnWorldArrived.Broadcast(State.CurrentWorldId);
    return true;
}

bool USPTravelNavigationComponent::IsLandingSafe(const FSPTravelContext& Context) const
{
    return Context.bPilotAtControls && !Context.bShipTransitioning && Context.bSafeLandingFootprint &&
        FMath::IsFinite(Context.SpeedMetersPerSecond) && Context.SpeedMetersPerSecond >= 0.0 &&
        Context.SpeedMetersPerSecond < 80.0 &&
        FMath::IsFinite(Context.SurfaceClearanceMeters) && Context.SurfaceClearanceMeters >= 0.0 &&
        Context.SurfaceClearanceMeters <= 300.0;
}

bool USPTravelNavigationComponent::RequestSurfaceLanding(const FSPTravelContext& Context)
{
    if (State.Phase != ESPTravelPhase::Flight || State.CruiseSpool > 0.1f ||
        !IsLandingSafe(Context)) return false;
    State.bCruiseLatched = false;
    State.CruiseSpool = 0.0f;
    ChangePhase(ESPTravelPhase::LandingRequested);
    return true;
}

bool USPTravelNavigationComponent::ConfirmSurfaceTouchdown(const FSPTravelContext& Context)
{
    if (State.Phase != ESPTravelPhase::LandingRequested || !IsLandingSafe(Context)) return false;
    ChangePhase(ESPTravelPhase::Landed);
    return true;
}

bool USPTravelNavigationComponent::CancelSurfaceLanding()
{
    if (State.Phase != ESPTravelPhase::LandingRequested) return false;
    ChangePhase(ESPTravelPhase::Flight);
    return true;
}

bool USPTravelNavigationComponent::DockAtStation(const FSPTravelContext& Context)
{
    if (State.Phase != ESPTravelPhase::Landed || !Context.bPilotAtControls ||
        !Context.bAtStationPad || Context.StationId.IsEmpty()) return false;
    State.DockedStationId = Context.StationId;
    ChangePhase(ESPTravelPhase::Docked);
    return true;
}

bool USPTravelNavigationComponent::Undock()
{
    if (State.Phase != ESPTravelPhase::Docked) return false;
    State.DockedStationId.Empty();
    ChangePhase(ESPTravelPhase::Landed);
    return true;
}

bool USPTravelNavigationComponent::Launch(const FSPTravelContext& Context)
{
    if ((State.Phase != ESPTravelPhase::Landed && State.Phase != ESPTravelPhase::Docked) ||
        !Context.bPilotAtControls || !Context.bShipPowered || Context.bShipTransitioning ||
        State.FuelPercent <= 0.0f) return false;
    State.DockedStationId.Empty();
    ChangePhase(ESPTravelPhase::Flight);
    return true;
}

bool USPTravelNavigationComponent::RestoreSaveState(const FSPTravelNavigationState& Saved)
{
    if (Worlds.IsEmpty() && !LoadWorldCatalog()) return false;
    const FString Current = CanonicalWorldId(Saved.CurrentWorldId);
    const FString Destination = CanonicalWorldId(Saved.DestinationWorldId);
    const uint8 PhaseValue = static_cast<uint8>(Saved.Phase);
    const uint8 ModeValue = static_cast<uint8>(Saved.DriveMode);
    if (!IsValidWorld(Current) || (!Destination.IsEmpty() && (!IsValidWorld(Destination) || Destination == Current)) ||
        PhaseValue > static_cast<uint8>(ESPTravelPhase::Docked) || ModeValue > static_cast<uint8>(ESPTravelDriveMode::NAV) ||
        !FMath::IsFinite(Saved.FuelPercent) || Saved.FuelPercent < 0.0f || Saved.FuelPercent > 100.0f ||
        !FMath::IsFinite(Saved.HeatNormalized) || Saved.HeatNormalized < 0.0f || Saved.HeatNormalized > 1.0f ||
        !FMath::IsFinite(Saved.CruiseSpool) || Saved.CruiseSpool < 0.0f || Saved.CruiseSpool > 1.0f ||
        !FMath::IsFinite(Saved.JumpChargeSeconds) || Saved.JumpChargeSeconds < 0.0f || Saved.JumpChargeSeconds > 2.0f) return false;
    const bool bJumping = Saved.Phase == ESPTravelPhase::JumpCharging || Saved.Phase == ESPTravelPhase::JumpTransit;
    if ((bJumping && (Destination.IsEmpty() || Saved.DriveMode != ESPTravelDriveMode::NAV ||
        Saved.CruiseSpool != 0.0f || Saved.bCruiseLatched)) ||
        (Saved.Phase == ESPTravelPhase::JumpTransit && Saved.JumpChargeSeconds != 2.0f) ||
        (Saved.Phase == ESPTravelPhase::JumpCharging && Saved.JumpChargeSeconds >= 2.0f) ||
        (!bJumping && Saved.JumpChargeSeconds != 0.0f) ||
        (Saved.Phase != ESPTravelPhase::Flight && Saved.Phase != ESPTravelPhase::JumpCharging &&
            Saved.Phase != ESPTravelPhase::JumpTransit && (Saved.CruiseSpool != 0.0f || Saved.bCruiseLatched)) ||
        (Saved.Phase == ESPTravelPhase::Docked) != !Saved.DockedStationId.IsEmpty() ||
        (Saved.DriveMode == ESPTravelDriveMode::SCM && (Saved.CruiseSpool > 0.02f || Saved.bCruiseLatched))) return false;
    State = Saved;
    State.CurrentWorldId = Current;
    State.DestinationWorldId = Destination;
    return true;
}
