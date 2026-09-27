#include "SPFlightPawn.h"

#include "SPHyperdriveVisualComponent.h"
#include "SPWorldSurface.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "EngineUtils.h"

ASPFlightPawn::ASPFlightPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bAddDefaultMovementBindings = false;
    FlightCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FlightCamera"));
    FlightCamera->SetupAttachment(RootComponent);
    FlightCamera->bUsePawnControlRotation = false;
    FlightCamera->SetRelativeLocation(FVector(-2200.0f, 0.0f, 650.0f));
    FlightCamera->SetRelativeRotation(FRotator(-5.0f, 0.0f, 0.0f));
    TravelNavigation = CreateDefaultSubobject<USPTravelNavigationComponent>(TEXT("TravelNavigation"));
    HyperdriveVisual = CreateDefaultSubobject<USPHyperdriveVisualComponent>(TEXT("HyperdriveVisual"));
}

void ASPFlightPawn::BeginPlay()
{
    Super::BeginPlay();
    UpdateGearMeshes();
    if (TravelNavigation) TravelNavigation->LoadWorldCatalog();
    if (GetWorld())
    {
        for (TActorIterator<ASPWorldSurface> It(GetWorld()); It; ++It)
        {
            SetTravelWorldSurface(*It);
            break;
        }
    }
    if (TravelWorldSurface)
    {
        if (TravelNavigation && TravelWorldSurface->WorldId != TravelNavigation->GetNavigationState().CurrentWorldId)
        {
            FSPTravelNavigationState Saved = TravelNavigation->GetNavigationState();
            Saved.CurrentWorldId = TravelWorldSurface->WorldId;
            TravelNavigation->RestoreSaveState(Saved);
        }
    }
    if (HyperdriveVisual)
    {
        HyperdriveVisual->SetNavigationComponent(TravelNavigation);
        HyperdriveVisual->BindToCamera(FlightCamera);
    }
}

void ASPFlightPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    check(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("SPForward"), this, &ASPFlightPawn::SetForward);
    PlayerInputComponent->BindAxis(TEXT("SPRight"), this, &ASPFlightPawn::SetRight);
    PlayerInputComponent->BindAxis(TEXT("SPUp"), this, &ASPFlightPawn::SetUp);
    PlayerInputComponent->BindAxis(TEXT("SPPitch"), this, &ASPFlightPawn::SetPitch);
    PlayerInputComponent->BindAxis(TEXT("SPYaw"), this, &ASPFlightPawn::SetYaw);
    PlayerInputComponent->BindAxis(TEXT("SPRoll"), this, &ASPFlightPawn::SetRoll);
    PlayerInputComponent->BindAxis(TEXT("SPMousePitch"), this, &ASPFlightPawn::SetMousePitch);
    PlayerInputComponent->BindAxis(TEXT("SPMouseYaw"), this, &ASPFlightPawn::SetMouseYaw);
    PlayerInputComponent->BindAxis(TEXT("SPThrottle"), this, &ASPFlightPawn::AdjustThrottle);
    PlayerInputComponent->BindAction(TEXT("SPBrake"), IE_Pressed, this, &ASPFlightPawn::StartBrake);
    PlayerInputComponent->BindAction(TEXT("SPBrake"), IE_Released, this, &ASPFlightPawn::StopBrake);
    PlayerInputComponent->BindAction(TEXT("SPBoost"), IE_Pressed, this, &ASPFlightPawn::StartBoost);
    PlayerInputComponent->BindAction(TEXT("SPBoost"), IE_Released, this, &ASPFlightPawn::StopBoost);
    PlayerInputComponent->BindAction(TEXT("SPGear"), IE_Pressed, this, &ASPFlightPawn::ToggleGear);
    PlayerInputComponent->BindAction(TEXT("SPAssist"), IE_Pressed, this, &ASPFlightPawn::ToggleAssist);
    PlayerInputComponent->BindAction(TEXT("SPCruise"), IE_Pressed, this, &ASPFlightPawn::ToggleCruise);
    PlayerInputComponent->BindAction(TEXT("SPPower"), IE_Pressed, this, &ASPFlightPawn::TogglePower);
    PlayerInputComponent->BindAction(TEXT("SPCamera"), IE_Pressed, this, &ASPFlightPawn::ToggleCamera);
    PlayerInputComponent->BindAction(TEXT("SPLand"), IE_Pressed, this, &ASPFlightPawn::TryLand);
    PlayerInputComponent->BindAction(TEXT("SPTravelNext"), IE_Pressed, this, &ASPFlightPawn::InputNextTravelDestination);
    PlayerInputComponent->BindAction(TEXT("SPTravelJump"), IE_Pressed, this, &ASPFlightPawn::InputBeginHyperdriveJump);
    PlayerInputComponent->BindAction(TEXT("SPTravelCancel"), IE_Pressed, this, &ASPFlightPawn::InputCancelHyperdriveJump);
}

void ASPFlightPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (DeltaSeconds <= 0.0f) return;
    const float Dt = FMath::Min(DeltaSeconds, 0.1f);
    if (TickTravel(Dt)) return;

    if (bLandingPending)
    {
        if (bBrakeHeld)
        {
            bLandingPending = false;
        }
        else
        {
            SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(), LandingPosition, Dt, 4500.0f), true);
            SetActorRotation(FQuat::Slerp(GetActorQuat(), LandingRotation, FMath::Clamp(Dt * 2.0f, 0.0f, 1.0f)));
            if (FVector::DistSquared(GetActorLocation(), LandingPosition) < FMath::Square(5.0f))
            {
                SetActorLocation(LandingPosition);
                SetActorRotation(LandingRotation);
                FlightVelocityCmPerSecond = FVector::ZeroVector;
                bFlying = false;
                bLandingPending = false;
                OnFlightStateChanged.Broadcast();
            }
            return;
        }
    }
    if (!bFlying)
    {
        if (UpInput > 0.1f) Launch();
        return;
    }

    const bool bCanThrust = bPowered && FuelPercent > 0.0f;
    const bool bBoosting = bBoostHeld && bCanThrust && HeatPercent < 90.0f;
    const float TurnLimit = TurnRateDegreesPerSecond * (bGearDown ? 0.6f : 1.0f);
    const FVector RateInput(PitchInput, YawInput, RollInput);
    AngularVelocityDegreesPerSecond = FMath::VInterpConstantTo(
        AngularVelocityDegreesPerSecond,
        RateInput.GetClampedToMaxSize(1.5f) * TurnLimit,
        Dt, TurnLimit * 5.0f);
    if (bCanThrust)
    {
        const FRotator DeltaRotation(
            AngularVelocityDegreesPerSecond.X * Dt,
            AngularVelocityDegreesPerSecond.Y * Dt,
            AngularVelocityDegreesPerSecond.Z * Dt);
        SetActorRotation(GetActorQuat() * DeltaRotation.Quaternion());
        const FRotator MouseRotation(MousePitch * 0.12f, MouseYaw * 0.12f, 0.0f);
        SetActorRotation(GetActorQuat() * MouseRotation.Quaternion());
    }

    FVector Translation(ForwardInput, RightInput, UpInput);
    if (bCruise && Translation.X >= 0.0f) Translation.X = 1.0f;
    Translation = Translation.GetClampedToMaxSize(1.0f);
    const float SpeedLimit = MaxSpeedCmPerSecond * ThrottleLimit
        * (bGearDown ? 0.35f : 1.0f) * (bBoosting ? 2.65f : 1.0f)
        * (bCruise ? 4.0f : 1.0f);
    const float Acceleration = AccelerationCmPerSecondSquared * (bBoosting ? 3.0f : 1.0f);
    if (bCanThrust)
    {
        const FVector DesiredVelocity = GetActorQuat().RotateVector(Translation) * SpeedLimit;
        if (bBrakeHeld)
        {
            FlightVelocityCmPerSecond = FMath::VInterpConstantTo(
                FlightVelocityCmPerSecond, FVector::ZeroVector, Dt, Acceleration * 2.4f);
        }
        else if (bFlightAssist)
        {
            FlightVelocityCmPerSecond = FMath::VInterpConstantTo(
                FlightVelocityCmPerSecond, DesiredVelocity, Dt, Acceleration);
        }
        else if (!Translation.IsNearlyZero())
        {
            const FVector Direction = DesiredVelocity.GetSafeNormal();
            const float Step = FMath::Clamp(
                DesiredVelocity.Size() - FVector::DotProduct(FlightVelocityCmPerSecond, Direction),
                0.0f, Acceleration * Dt);
            FlightVelocityCmPerSecond += Direction * Step;
        }
    }
    FHitResult Hit;
    AddActorWorldOffset(FlightVelocityCmPerSecond * Dt, true, &Hit);
    if (Hit.bBlockingHit)
    {
        FlightVelocityCmPerSecond = FlightVelocityCmPerSecond.MirrorByVector(Hit.Normal) * 0.15f;
        bCruise = false;
    }
    const float Load = Translation.Size() + (bBrakeHeld ? 1.0f : 0.0f);
    if (bCanThrust && Load > 0.0f)
    {
        FuelPercent = FMath::Max(0.0f, FuelPercent - Dt * (bBoosting ? 0.12f : 0.012f) * Load);
    }
    HeatPercent = FMath::Clamp(HeatPercent + Dt * (bBoosting ? 24.0f : -6.0f), 0.0f, 100.0f);
}

FSPTravelContext ASPFlightPawn::GetTravelContext() const
{
    FSPTravelContext Context;
    Context.bPilotAtControls = GetController() != nullptr;
    Context.bShipPowered = bPowered;
    Context.bGearRetracted = !bGearDown;
    Context.bShipTransitioning = bLandingPending || !bFlying;
    Context.bCargoHatchClosed = bCargoHatchClosed;
    Context.SpeedMetersPerSecond = FlightVelocityCmPerSecond.Size() / 100.0;
    Context.AltitudeKm = -1.0;
    Context.NearestStationDistanceKm = 0.0;
    if (IsValid(TravelWorldSurface) && TravelWorldSurface->SourceClimateWidth > 0)
    {
        const float AltitudeMeters = TravelWorldSurface->GetAltitudeMetersAtWorldLocation(GetActorLocation());
        Context.AltitudeKm = AltitudeMeters / 1000.0;
        Context.SurfaceClearanceMeters = AltitudeMeters;
        Context.NearbyBodyRadiusKm = TravelWorldSurface->GetScaledRadiusKm();
        // The authored Kellen Reach pad is at the surface actor's origin.
        Context.NearestStationDistanceKm = FVector::Dist(GetActorLocation(), TravelWorldSurface->GetActorLocation()) / 100000.0;
    }
    return Context;
}

bool ASPFlightPawn::SelectNextTravelDestination()
{
    if (!TravelNavigation || !TravelNavigation->CycleDestination()) return false;
    FSPTravelWorld Destination;
    if (TravelNavigation->FindWorld(TravelNavigation->GetNavigationState().DestinationWorldId, Destination))
        AnnounceTravel(FString::Printf(TEXT("NAV target: %s  |  J charge  K cancel"), *Destination.Name));
    return true;
}

void ASPFlightPawn::AnnounceTravel(const FString& Message) const
{
    UE_LOG(LogTemp, Display, TEXT("Kestrel travel: %s"), *Message);
    if (GEngine && GetWorld() && GetWorld()->IsGameWorld())
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor(77, 183, 194), Message);
}

void ASPFlightPawn::SetTravelWorldSurface(ASPWorldSurface* InSurface)
{
    TravelWorldSurface = InSurface;
    if (IsValid(InSurface)) InSurface->FocusActor = this;
}

bool ASPFlightPawn::BeginHyperdriveJump()
{
    if (!bFlying || !IsValid(TravelWorldSurface) || !TravelNavigation || !TravelNavigation->BeginJump(GetTravelContext()))
    {
        AnnounceTravel(TEXT("Jump blocked: set a target, retract gear, clear the port, climb above 2 km, and check fuel/heat"));
        return false;
    }
    bCruise = false;
    TransitElapsedSeconds = 0.0f;
    AnnounceTravel(TEXT("Hyperdrive charging: K cancels before transit"));
    if (HyperdriveVisual) HyperdriveVisual->RefreshVisuals(0.0f);
    OnFlightStateChanged.Broadcast();
    return true;
}

bool ASPFlightPawn::CancelHyperdriveJump()
{
    if (!TravelNavigation || !TravelNavigation->CancelJump()) return false;
    TravelNavigation->SetDriveMode(ESPTravelDriveMode::SCM);
    bCruise = false;
    AnnounceTravel(TEXT("Charge canceled: manual flight restored"));
    if (HyperdriveVisual) HyperdriveVisual->RefreshVisuals(0.0f);
    OnFlightStateChanged.Broadcast();
    return true;
}

bool ASPFlightPawn::TickTravel(float DeltaSeconds)
{
    if (!TravelNavigation) return false;
    TravelNavigation->SetDriveResources(FuelPercent, HeatPercent / 100.0f);
    TravelNavigation->AdvanceDrive(DeltaSeconds, GetTravelContext(), bCruise, bBrakeHeld);
    const FSPTravelNavigationState State = TravelNavigation->GetNavigationState();
    FuelPercent = State.FuelPercent;
    HeatPercent = State.HeatNormalized * 100.0f;
    if (State.Phase == ESPTravelPhase::JumpTransit)
    {
        if (PreviousTravelPhase != ESPTravelPhase::JumpTransit)
        {
            TransitElapsedSeconds = 0.0f;
            FlightVelocityCmPerSecond = FVector::ZeroVector;
            AngularVelocityDegreesPerSecond = FVector::ZeroVector;
            bCruise = false;
            AnnounceTravel(TEXT("Hyperdrive transit: hold position"));
            OnFlightStateChanged.Broadcast();
        }
        PreviousTravelPhase = ESPTravelPhase::JumpTransit;
        TransitElapsedSeconds += DeltaSeconds;
        if (TransitElapsedSeconds >= TransitDurationSeconds) CompleteTravelArrival();
        return true;
    }
    if (PreviousTravelPhase == ESPTravelPhase::JumpCharging && State.Phase == ESPTravelPhase::Flight)
        TravelNavigation->SetDriveMode(ESPTravelDriveMode::SCM);
    PreviousTravelPhase = State.Phase;
    return false;
}

bool ASPFlightPawn::CompleteTravelArrival()
{
    if (!TravelNavigation || !IsValid(TravelWorldSurface)) return false;
    const FSPTravelNavigationState State = TravelNavigation->GetNavigationState();
    if (State.Phase != ESPTravelPhase::JumpTransit || State.DestinationWorldId.IsEmpty()) return false;

    const FString PreviousWorldId = TravelWorldSurface->WorldId;
    const FVector Arrival = TravelWorldSurface->GetActorTransform().TransformPosition(ExteriorArrivalOffsetCm);
    if (!TravelWorldSurface->ActivateWorld(State.DestinationWorldId) ||
        TravelWorldSurface->GetAltitudeMetersAtWorldLocation(Arrival) < 2000.0f)
    {
        TravelWorldSurface->ActivateWorld(PreviousWorldId);
        TravelNavigation->AbortJumpTransit();
        if (HyperdriveVisual) HyperdriveVisual->RefreshVisuals(0.0f);
        UE_LOG(LogTemp, Error, TEXT("Travel route failed to activate safe exterior for %s"), *State.DestinationWorldId);
        AnnounceTravel(TEXT("Destination surface unavailable: manual flight restored"));
        return false;
    }

    SetActorLocation(Arrival, false);
    SetActorRotation(TravelWorldSurface->GetActorRotation());
    FlightVelocityCmPerSecond = FVector::ZeroVector;
    AngularVelocityDegreesPerSecond = FVector::ZeroVector;
    bFlightAssist = true;
    bCruise = false;
    if (!TravelNavigation->ConfirmJumpArrival())
    {
        TravelWorldSurface->ActivateWorld(PreviousWorldId);
        TravelNavigation->AbortJumpTransit();
        return false;
    }
    TravelNavigation->SetDriveMode(ESPTravelDriveMode::SCM);
    FSPTravelWorld ArrivedWorld;
    if (TravelNavigation->FindWorld(TravelNavigation->GetNavigationState().CurrentWorldId, ArrivedWorld))
        AnnounceTravel(FString::Printf(TEXT("Arrived outside %s: manual flight restored"), *ArrivedWorld.Name));
    if (HyperdriveVisual) HyperdriveVisual->RefreshVisuals(0.0f);
    OnFlightStateChanged.Broadcast();
    return true;
}

FSPFlightTelemetry ASPFlightPawn::GetFlightTelemetry() const
{
    FSPFlightTelemetry Telemetry;
    Telemetry.SpeedMetersPerSecond = FlightVelocityCmPerSecond.Size() / 100.0f;
    Telemetry.ThrottlePercent = ThrottleLimit * 100.0f;
    Telemetry.FuelPercent = FuelPercent;
    Telemetry.HeatPercent = HeatPercent;
    Telemetry.bGearDown = bGearDown;
    Telemetry.bFlightAssist = bFlightAssist;
    Telemetry.bFlying = bFlying;
    Telemetry.bCruise = bCruise;
    return Telemetry;
}

void ASPFlightPawn::SetThrottleLimit(float NewLimit)
{
    ThrottleLimit = FMath::Clamp(NewLimit, 0.05f, 3.0f);
    OnFlightStateChanged.Broadcast();
}

void ASPFlightPawn::AdjustThrottle(float Value)
{
    if (!FMath::IsNearlyZero(Value)) SetThrottleLimit(ThrottleLimit * FMath::Exp(Value * 0.1f));
}

void ASPFlightPawn::SetFlightAssist(bool bEnabled)
{
    bFlightAssist = bEnabled;
    OnFlightStateChanged.Broadcast();
}

void ASPFlightPawn::SetGearDown(bool bDown)
{
    if (!bFlying && !bDown) return;
    bGearDown = bDown;
    UpdateGearMeshes();
    OnFlightStateChanged.Broadcast();
}

void ASPFlightPawn::UpdateGearMeshes()
{
    TArray<UStaticMeshComponent*> Meshes;
    GetComponents<UStaticMeshComponent>(Meshes);
    for (UStaticMeshComponent* Mesh : Meshes)
    {
        const UStaticMesh* StaticMesh = Mesh ? Mesh->GetStaticMesh() : nullptr;
        const bool bIsGear = Mesh && (Mesh->GetName().StartsWith(TEXT("Gear_"))
            || (StaticMesh && StaticMesh->GetName().StartsWith(TEXT("Gear_"))));
        if (bIsGear)
        {
            Mesh->SetVisibility(bGearDown);
            Mesh->SetCollisionEnabled(bGearDown ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        }
    }
}

void ASPFlightPawn::ToggleCruise()
{
    if (bFlying && bPowered && !bGearDown)
    {
        bCruise = !bCruise;
        OnFlightStateChanged.Broadcast();
    }
}

void ASPFlightPawn::TogglePower()
{
    bPowered = !bPowered;
    if (!bPowered) bCruise = false;
    OnFlightStateChanged.Broadcast();
}

void ASPFlightPawn::ToggleCamera()
{
    bCockpitCamera = !bCockpitCamera;
    if (FlightCamera)
    {
        FlightCamera->SetRelativeLocation(bCockpitCamera
            ? FVector(400.0f, 0.0f, 460.0f)
            : FVector(-2200.0f, 0.0f, 650.0f));
        FlightCamera->SetRelativeRotation(bCockpitCamera
            ? FRotator::ZeroRotator : FRotator(-5.0f, 0.0f, 0.0f));
    }
    OnFlightStateChanged.Broadcast();
}

FVector ASPFlightPawn::TraceDown() const
{
    if (bUseRadialSurface)
    {
        const FVector Radial = (GetActorLocation() - PlanetCenterCm).GetSafeNormal();
        if (!Radial.IsNearlyZero()) return -Radial;
    }
    return -FVector::UpVector;
}

bool ASPFlightPawn::Launch()
{
    if (bFlying || !bPowered || FuelPercent <= 0.0f) return false;
    bFlying = true;
    bLandingPending = false;
    const FVector Up = -TraceDown();
    FlightVelocityCmPerSecond = Up * 500.0f;
    AddActorWorldOffset(Up * 150.0f, true);
    OnFlightStateChanged.Broadcast();
    return true;
}

bool ASPFlightPawn::RequestSurfaceLanding()
{
    if (!bFlying || !bGearDown || FlightVelocityCmPerSecond.Size() >= 4500.0f) return false;
    const FVector Down = TraceDown();
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpacePatriotLanding), false, this);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(),
        GetActorLocation() + Down * (65000.0f + StandHeightCm), ECC_Visibility, Params)) return false;
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    const float Altitude = FVector::DotProduct(GetActorLocation() - Hit.ImpactPoint, Normal) - StandHeightCm;
    if (Altitude < -200.0f || Altitude >= 65000.0f || FVector::DotProduct(GetActorUpVector(), Normal) < 0.6f)
        return false;
    FVector Forward = FVector::VectorPlaneProject(GetActorForwardVector(), Normal).GetSafeNormal();
    if (Forward.IsNearlyZero()) Forward = FVector::CrossProduct(Normal, FVector::RightVector).GetSafeNormal();
    LandingPosition = Hit.ImpactPoint + Normal * StandHeightCm;
    LandingRotation = FRotationMatrix::MakeFromXZ(Forward, Normal).ToQuat();
    SurfaceNormal = Normal;
    bLandingPending = true;
    bCruise = false;
    OnFlightStateChanged.Broadcast();
    return true;
}
