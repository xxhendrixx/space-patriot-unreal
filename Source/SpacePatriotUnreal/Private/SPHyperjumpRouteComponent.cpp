#include "SPHyperjumpRouteComponent.h"

#include "SPFlightPawn.h"
#include "SPWorldSurface.h"

#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    // Matches ASPWorldSurface's currently fixed 18 km globe radius. The center
    // itself is queried from the surface actor so its transform stays valid.
    constexpr double GlobeRadiusCm = 1800000.0;
}

USPHyperjumpRouteComponent::USPHyperjumpRouteComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USPHyperjumpRouteComponent::BeginPlay()
{
    Super::BeginPlay();
    if (Ship && Surface) Configure(Ship.Get(), Surface.Get(), Navigation.Get());
}

bool USPHyperjumpRouteComponent::Configure(ASPFlightPawn* InShip, ASPWorldSurface* InSurface,
    USPTravelNavigationComponent* InNavigation)
{
    if (!IsValid(InShip) || !IsValid(InSurface) || !IsValid(GetOwner()) ||
        InShip->GetWorld() != InSurface->GetWorld() || InShip->GetWorld() != GetWorld())
    {
        SetMessage(TEXT("Ship and world surface must be in the same running level."));
        return false;
    }

    // The flight pawn already owns navigation for standalone maps. Reuse it in
    // the integrated play loop so keyboard, MFD, visuals and journey saves all
    // observe the same route state.
    USPTravelNavigationComponent* Candidate = InNavigation ? InNavigation : InShip->TravelNavigation.Get();
    if (!Candidate) Candidate = GetOwner()->FindComponentByClass<USPTravelNavigationComponent>();
    if (!Candidate)
    {
        Candidate = NewObject<USPTravelNavigationComponent>(GetOwner(), TEXT("LiveTravelNavigation"));
        if (!Candidate) return false;
        GetOwner()->AddInstanceComponent(Candidate);
        Candidate->RegisterComponent();
    }
    if ((Candidate->GetOwner() != GetOwner() && Candidate->GetOwner() != InShip) ||
        !Candidate->LoadWorldCatalog())
    {
        SetMessage(TEXT("Original world catalog is unavailable."));
        return false;
    }
    FSPTravelWorld World;
    if (!Candidate->FindWorld(InSurface->WorldId, World))
    {
        SetMessage(TEXT("This surface has no source-world profile."));
        return false;
    }

    // A placed map may open on a world other than Earth. The surface is the
    // scene authority at startup, so align navigation before accepting input.
    FSPTravelNavigationState State = Candidate->GetNavigationState();
    if (State.CurrentWorldId != World.Id)
    {
        if (State.Phase != ESPTravelPhase::Flight) return false;
        State.CurrentWorldId = World.Id;
        State.DestinationWorldId.Empty();
        if (!Candidate->RestoreSaveState(State)) return false;
    }

    Ship = InShip;
    Surface = InSurface;
    Navigation = Candidate;
    Ship->BindIntegratedRoute(this);
    Surface->FocusActor = InShip;
    Ship->PlanetCenterCm = Surface->GetPlanetCenterWorld();
    Ship->bUseRadialSurface = true;
    TransitElapsedSeconds = 0.0f;
    SetMessage(TEXT("Select a destination, climb clear of the atmosphere, and retract the gear."));
    return true;
}

FSPTravelContext USPHyperjumpRouteComponent::BuildContext() const
{
    FSPTravelContext Context;
    if (!Ship || !Surface)
    {
        Context.bPilotAtControls = false;
        Context.bShipPowered = false;
        return Context;
    }
    Context.bPilotAtControls = Ship->GetController() != nullptr;
    Context.bShipPowered = Ship->bPowered && Ship->bFlying;
    Context.bGearRetracted = !Ship->bGearDown;
    Context.bShipTransitioning = bShipTransitioning;
    Context.bCargoHatchClosed = bCargoHatchClosed;
    Context.AltitudeKm = FMath::Max(0.0, (FVector::Distance(Ship->GetActorLocation(), Ship->PlanetCenterCm)
        - GlobeRadiusCm) / 100000.0);
    Context.NearbyBodyRadiusKm = GlobeRadiusCm / 100000.0;
    Context.NearestStationDistanceKm = TNumericLimits<double>::Max();
    Context.SpeedMetersPerSecond = Ship->GetFlightTelemetry().SpeedMetersPerSecond;

    // Authored stations can mark their actors with either tag; no tagged
    // station means there is no local docking exclusion to fabricate.
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            const AActor* Actor = *It;
            if (Actor && Actor != Ship &&
                (Actor->ActorHasTag(TEXT("Station")) || Actor->ActorHasTag(TEXT("JumpExclusion"))))
            {
                Context.NearestStationDistanceKm = FMath::Min(Context.NearestStationDistanceKm,
                    FVector::Distance(Ship->GetActorLocation(), Actor->GetActorLocation()) / 100000.0);
            }
        }
    }
    return Context;
}

bool USPHyperjumpRouteComponent::SelectDestination(const FString& WorldId)
{
    if (!Navigation) return false;
    const bool bSelected = Navigation->SelectDestination(WorldId);
    if (!bSelected) SetMessage(TEXT("Choose another world before charging the drive."));
    else SetMessage(TEXT("Destination locked. Climb above 2 km and retract the gear."));
    return bSelected;
}

bool USPHyperjumpRouteComponent::CycleDestination()
{
    if (!Navigation) return false;
    const bool bSelected = Navigation->CycleDestination();
    if (bSelected) SetMessage(TEXT("Destination selected. Climb above 2 km and retract the gear."));
    return bSelected;
}

bool USPHyperjumpRouteComponent::RequestJump()
{
    if (!Navigation || !Ship || !Surface) return false;
    Navigation->SetDriveResources(Ship->FuelPercent, Ship->HeatPercent / 100.0f);
    const FSPTravelContext Context = BuildContext();
    if (!Navigation->BeginJump(Context))
    {
        if (!Context.bPilotAtControls) SetMessage(TEXT("Take the pilot seat to jump."));
        else if (!Context.bShipPowered) SetMessage(TEXT("Launch and power the ship before jumping."));
        else if (!Context.bGearRetracted) SetMessage(TEXT("Retract the landing gear."));
        else if (Context.AltitudeKm <= 2.0) SetMessage(TEXT("Climb above 2 km to clear the atmosphere."));
        else if (Context.NearestStationDistanceKm <= 5.0) SetMessage(TEXT("Move at least 5 km clear of the station."));
        else if (!Context.bCargoHatchClosed) SetMessage(TEXT("Close the cargo hatch."));
        else if (Navigation->GetNavigationState().FuelPercent < 8.0f) SetMessage(TEXT("At least 8% fuel is required."));
        else if (Navigation->GetNavigationState().HeatNormalized >= 0.84f) SetMessage(TEXT("Let the drive cool before jumping."));
        else SetMessage(TEXT("Select another destination before jumping."));
        return false;
    }
    TransitElapsedSeconds = 0.0f;
    SetMessage(TEXT("Hyperdrive charging."));
    return true;
}

bool USPHyperjumpRouteComponent::CancelJump()
{
    if (!Navigation || !Navigation->CancelJump()) return false;
    Navigation->SetDriveMode(ESPTravelDriveMode::SCM);
    TransitElapsedSeconds = 0.0f;
    SetMessage(TEXT("Jump cancelled."));
    return true;
}

void USPHyperjumpRouteComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!Navigation || !Ship || !Surface || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f) return;

    const ESPTravelPhase Previous = Navigation->GetNavigationState().Phase;
    if (Previous == ESPTravelPhase::JumpCharging)
    {
        Navigation->SetDriveResources(Ship->FuelPercent, Ship->HeatPercent / 100.0f);
        Navigation->AdvanceDrive(DeltaTime, BuildContext(), false, false);
        const FSPTravelNavigationState State = Navigation->GetNavigationState();
        if (State.Phase == ESPTravelPhase::JumpTransit)
        {
            // The travel state machine applies the one-time jump debit. Mirror
            // it to the actual flight pawn before the next flight tick.
            Ship->FuelPercent = State.FuelPercent;
            Ship->HeatPercent = State.HeatNormalized * 100.0f;
            TransitElapsedSeconds = 0.0f;
            SetMessage(TEXT("In hyperjump transit."));
        }
        else if (State.Phase == ESPTravelPhase::Flight)
        {
            Navigation->SetDriveMode(ESPTravelDriveMode::SCM);
            SetMessage(TEXT("Jump interlock opened; charging cancelled."));
        }
    }
    else if (Previous == ESPTravelPhase::JumpTransit)
    {
        TransitElapsedSeconds += DeltaTime;
        if (TransitElapsedSeconds >= FMath::Max(0.1f, TransitDurationSeconds)) CompleteTransit();
    }
}

bool USPHyperjumpRouteComponent::CompleteTransit()
{
    if (!Navigation || !Ship || !Surface) return false;
    const FSPTravelNavigationState State = Navigation->GetNavigationState();
    if (State.Phase != ESPTravelPhase::JumpTransit || State.DestinationWorldId.IsEmpty()) return false;
    if (!Surface->ActivateWorld(State.DestinationWorldId))
    {
        SetMessage(TEXT("Destination surface failed to load; ship remains in transit."));
        TransitElapsedSeconds = 0.0f;
        return false;
    }

    const FVector NewCenter = Surface->GetPlanetCenterWorld();
    const FVector Arrival = Surface->GetActorTransform().TransformPosition(
        FVector(0.0, 0.0, ArrivalAltitudeMeters * 100.0));
    const FTransform OldTransform = Ship->GetActorTransform();
    const FVector OldCenter = Ship->PlanetCenterCm;
    const bool bMoved = Ship->SetActorLocationAndRotation(Arrival, Surface->GetActorRotation(),
        false, nullptr, ETeleportType::TeleportPhysics);
    if (!bMoved)
    {
        Surface->ActivateWorld(State.CurrentWorldId);
        Ship->SetActorTransform(OldTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Ship->PlanetCenterCm = OldCenter;
        SetMessage(TEXT("Could not place the ship at the destination."));
        TransitElapsedSeconds = 0.0f;
        return false;
    }
    Ship->PlanetCenterCm = NewCenter;
    Ship->bUseRadialSurface = true;
    Ship->ResetMotionAfterWarp();
    Surface->FocusActor = Ship.Get();
    if (!Navigation->ConfirmJumpArrival())
    {
        Surface->ActivateWorld(State.CurrentWorldId);
        Ship->SetActorTransform(OldTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Ship->PlanetCenterCm = OldCenter;
        SetMessage(TEXT("Travel state could not confirm arrival."));
        TransitElapsedSeconds = 0.0f;
        return false;
    }
    // Restore ordinary flight and weapon availability after the jump visual
    // ends; otherwise the source NAV interlock would persist indefinitely.
    Navigation->SetDriveMode(ESPTravelDriveMode::SCM);
    TransitElapsedSeconds = 0.0f;
    FSPTravelWorld ArrivedWorld;
    const bool bGasWorld = Navigation->FindWorld(State.DestinationWorldId, ArrivedWorld)
        && ArrivedWorld.Biome == TEXT("gas");
    SetMessage(bGasWorld
        ? FString::Printf(TEXT("Arrived at %s. Gas world: remain in flight."), *State.DestinationWorldId)
        : FString::Printf(TEXT("Arrived at %s. Descend to land anywhere safe."), *State.DestinationWorldId));
    OnArrived.Broadcast(State.DestinationWorldId);
    return true;
}

FSPHyperjumpRouteStatus USPHyperjumpRouteComponent::GetRouteStatus() const
{
    FSPHyperjumpRouteStatus Status;
    Status.bReady = Navigation && Ship && Surface;
    Status.Message = LastMessage;
    if (!Navigation) return Status;
    Status.Navigation = Navigation->GetNavigationState();
    Status.ChargeFraction = Navigation->GetJumpChargeFraction();
    FSPTravelWorld World;
    if (Navigation->FindWorld(Status.Navigation.CurrentWorldId, World))
    {
        Status.CurrentWorldName = World.Name;
        Status.bSurfaceLandable = World.Biome != TEXT("gas");
    }
    if (Navigation->FindWorld(Status.Navigation.DestinationWorldId, World)) Status.DestinationWorldName = World.Name;
    return Status;
}

void USPHyperjumpRouteComponent::SetMessage(const FString& Message)
{
    LastMessage = Message;
}
