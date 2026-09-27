#include "SPFlightPawn.h"
#include "SPCockpitMFDWidget.h"
#include "SPStoryCampaignComponent.h"
#include "SpacePatriotBlueprintBases.h"

#include "SPHyperdriveVisualComponent.h"
#include "SPWorldSurface.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

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
    CockpitReadout = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CockpitReadout"));
    CockpitReadout->SetupAttachment(RootComponent);
    CockpitReadout->SetHorizontalAlignment(EHTA_Center);
    CockpitReadout->SetVerticalAlignment(EVRTA_TextCenter);
    CockpitReadout->SetWorldSize(28.0f);
    CockpitReadout->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
    CockpitReadout->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CockpitReadout->SetHiddenInGame(true);
    CockpitReadout->SetVisibility(false);
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
    UpdateCockpitReadout();
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
    PlayerInputComponent->BindAction(TEXT("SPMFDPointer"), IE_Pressed, this, &ASPFlightPawn::ToggleMFDPointer);
    PlayerInputComponent->BindAction(TEXT("SPLand"), IE_Pressed, this, &ASPFlightPawn::TryLand);
    PlayerInputComponent->BindAction(TEXT("SPTravelNext"), IE_Pressed, this, &ASPFlightPawn::InputNextTravelDestination);
    PlayerInputComponent->BindAction(TEXT("SPTravelJump"), IE_Pressed, this, &ASPFlightPawn::InputBeginHyperdriveJump);
    PlayerInputComponent->BindAction(TEXT("SPTravelCancel"), IE_Pressed, this, &ASPFlightPawn::InputCancelHyperdriveJump);
}

void ASPFlightPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    EnsureMFDWidget();
#if WITH_EDITOR
    // Opt-in visual validation captures the actual viewport/Slate composite
    // after the pawn has run, unlike a startup HighResShot of the scene only.
    if (!bMFDValidationShotQueued && MFDWidget && GetGameTimeSinceCreation() > 4.0f
        && FParse::Param(FCommandLine::Get(), TEXT("SPCaptureMFD")))
    {
        FScreenshotRequest::RequestScreenshot(TEXT("SPMFDViewport.png"), true, false);
        bMFDValidationShotQueued = true;
    }
#endif
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

    const bool bCanThrust = bPowered && FuelPercent > 0.0f && VesselEngineFactor > 0.01f;
    const bool bBoosting = bBoostHeld && bCanThrust && HeatPercent < 90.0f;
    const float TurnLimit = TurnRateDegreesPerSecond * (bGearDown ? 0.6f : 1.0f)
        * FMath::Clamp(VesselEngineFactor, 0.05f, 1.0f);
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
    const float SpeedLimit = MaxSpeedCmPerSecond * ThrottleLimit * FMath::Clamp(VesselEngineFactor, 0.0f, 1.0f)
        * (bGearDown ? 0.35f : 1.0f) * (bBoosting ? 2.65f : 1.0f)
        * (bCruise ? 4.0f : 1.0f);
    const float Acceleration = AccelerationCmPerSecondSquared * (bBoosting ? 3.0f : 1.0f)
        * FMath::Clamp(VesselEngineFactor, 0.05f, 1.0f);
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

FSPVesselSimulationInput ASPFlightPawn::BuildVesselSimulationInput() const
{
    FSPVesselSimulationInput Input;
    Input.bPowerOn = bPowered;
    Input.CruiseSpool01 = bCruise ? 1.0f : 0.0f;
    Input.HullPercent = FMath::Clamp(HullPercent, 0.0f, 100.0f);
    Input.ShipHeat01 = FMath::Clamp(HeatPercent / 100.0f, 0.0f, 1.0f);
    Input.Thrust01 = FMath::Clamp(FVector(ForwardInput, RightInput, UpInput).Size(), 0.0f, 1.0f);
    Input.ShieldPercent = FMath::Clamp(VesselShieldPercent, 0.0f, 100.0f);
    return Input;
}

void ASPFlightPawn::ApplyVesselSimulationOutput(const FSPVesselSimulationOutput& Output)
{
    HeatPercent = FMath::Clamp(Output.ShipHeat01 * 100.0f, 0.0f, 100.0f);
    VesselShieldPercent = FMath::Clamp(Output.ShieldPercent, 0.0f, 100.0f);
    VesselEngineFactor = FMath::Clamp(Output.EngineFactor, 0.0f, 1.0f);
    if (Output.bCancelCruise) bCruise = false;
    UpdateCockpitReadout();
    OnFlightStateChanged.Broadcast();
}

void ASPFlightPawn::CycleMFDPage(int32 Direction)
{
    CockpitMFDPage = (CockpitMFDPage + (Direction < 0 ? -1 : 1) + 4) % 4;
    UpdateCockpitReadout();
    OnFlightStateChanged.Broadcast();
}

void ASPFlightPawn::AdjustMFDBrightness(float Delta)
{
    CockpitMFDBrightness = FMath::Clamp(CockpitMFDBrightness + Delta, 0.05f, 1.0f);
    UpdateCockpitReadout();
    OnFlightStateChanged.Broadcast();
}

void ASPFlightPawn::RefreshMFD()
{
    UpdateCockpitReadout();
    OnFlightStateChanged.Broadcast();
}

void ASPFlightPawn::ToggleMFDPointer()
{
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController || !IsLocallyControlled()) return;
    bMFDPointerMode = !bMFDPointerMode;
    PlayerController->bShowMouseCursor = bMFDPointerMode;
    if (bMFDPointerMode)
    {
        MousePitch = MouseYaw = 0.0f;
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(MFDWidget ? MFDWidget->TakeWidget() : TSharedPtr<SWidget>());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        PlayerController->SetInputMode(Mode);
    }
    else
    {
        PlayerController->SetInputMode(FInputModeGameOnly());
    }
    if (MFDWidget) MFDWidget->SetPresentation(bCockpitCamera, bMFDPointerMode);
}

void ASPFlightPawn::EnsureMFDWidget()
{
    if (MFDWidget || !IsLocallyControlled()) return;
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController) return;
    MFDWidget = CreateWidget<USPCockpitMFDWidget>(PlayerController, USPCockpitMFDWidget::StaticClass());
    if (!MFDWidget) return;
    MFDWidget->SetShip(this);
    MFDWidget->AddToPlayerScreen(20);
    MFDWidget->SetPresentation(bCockpitCamera, bMFDPointerMode);
    MFDWidget->SetReadout(CockpitReadout->Text, CockpitMFDBrightness);
    UE_LOG(LogTemp, Display, TEXT("SpacePatriot MFD viewport widget attached to local player %s"), *GetNameSafe(PlayerController));
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
    UpdateCockpitReadout();
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
    UpdateCockpitReadout();
    if (MFDWidget) MFDWidget->SetPresentation(bCockpitCamera, bMFDPointerMode);
    OnFlightStateChanged.Broadcast();
}

void ASPFlightPawn::UpdateCockpitReadout()
{
    if (!CockpitReadout) return;
    const float Luminance = FMath::Clamp(CockpitMFDBrightness, 0.05f, 1.0f);
    CockpitReadout->SetTextRenderColor(FColor(
        static_cast<uint8>(42.0f * Luminance),
        static_cast<uint8>(255.0f * Luminance),
        static_cast<uint8>(125.0f * Luminance), 255));
    const USPVesselSystemsComponent* Systems = FindComponentByClass<USPVesselSystemsComponent>();
    const FSPVesselTelemetry Vessel = Systems ? Systems->GetTelemetry(bPowered) : FSPVesselTelemetry();
    FString Text;
    if (CockpitMFDPage == 0)
    {
        Text = FString::Printf(TEXT("MFD 1/4  NAV\nSPD %04.0f m/s   HDG %03.0f\nFUEL %03.0f%%  GEAR %s\nMODE %s  %s"),
            GetFlightTelemetry().SpeedMetersPerSecond, GetActorRotation().Yaw < 0.0f ? GetActorRotation().Yaw + 360.0f : GetActorRotation().Yaw,
            FuelPercent, bGearDown ? TEXT("DOWN") : TEXT("UP"),
            *StaticEnum<ESPVesselMode>()->GetNameStringByValue(static_cast<int64>(Vessel.Mode)),
            bPowered ? TEXT("PWR ON") : TEXT("PWR OFF"));
    }
    else if (CockpitMFDPage == 1)
    {
        Text = FString::Printf(TEXT("MFD 2/4  SYSTEMS\nENGINE %03.0f%%  SHIELD %03.0f%%\nCABIN %03.0f%%  HEAT %03.0f%%\n%s"),
            Vessel.EngineFactor * 100.0f, VesselShieldPercent,
            Vessel.PressurePercent, HeatPercent, *Vessel.Warning);
    }
    else if (CockpitMFDPage == 2)
    {
        Text = FString::Printf(TEXT("MFD 3/4  POWER\nPROFILE %s\nENG %d  WPN %d  SHD %d\nF1 TRAVEL   F2 COMBAT\nF3 PAGE     F4 DIM"),
            *StaticEnum<ESPVesselProfile>()->GetNameStringByValue(static_cast<int64>(Vessel.Profile)),
            Vessel.EngineAllocation, Vessel.WeaponAllocation, Vessel.ShieldAllocation);
    }
    else
    {
        Text = TEXT("MFD 4/4  MISSION\nF5 START THE WATER LEDGER\nF6 SAVE WORLD");
        if (GetWorld())
        {
            // Multiple runtime actors can coexist in editor previews. Prefer a
            // campaign with an active node instead of whichever actor loads first.
            TArray<AActor*> RuntimeActors;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASPWorldRuntime::StaticClass(), RuntimeActors);
            for (const AActor* WorldRuntime : RuntimeActors)
            {
                const USPStoryCampaignComponent* Campaign = WorldRuntime
                    ? WorldRuntime->FindComponentByClass<USPStoryCampaignComponent>() : nullptr;
                FSPStoryNodeView Node;
                if (Campaign && Campaign->GetCurrentNode(TEXT("water"), Node))
                {
                    Text = FString::Printf(TEXT("MFD 4/4  MISSION\n%s\n%s\n%s  %d/%d"),
                        *Node.Title.Left(30), *Node.Text.Left(72), *Node.ObjectiveKind,
                        Node.Progress, Node.Goal);
                    break;
                }
            }
        }
    }
    CockpitReadout->SetText(FText::FromString(Text));
    if (MFDWidget) MFDWidget->SetReadout(CockpitReadout->Text, CockpitMFDBrightness);
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
