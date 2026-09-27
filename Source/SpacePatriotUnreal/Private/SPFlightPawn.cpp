#include "SPFlightPawn.h"
#include "SPCockpitMFDWidget.h"
#include "SPHyperjumpRouteComponent.h"
#include "SPPlayLoopDirector.h"
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
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
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
    // A collaborator downloads this exterior through InstallViktorHeroShip.
    // Keep the native flight pawn and its Blueprint intact; only replace the
    // temporary Kestrel art when the local mesh is available.
    UStaticMesh* ExternalMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/SpacePatriot/OpenAssets/ViktorShips/Cruiser03_UE.Cruiser03_UE"));
    if (ExternalMesh)
    {
        UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(this, TEXT("ViktorCruiserVisual"));
        Visual->SetMobility(EComponentMobility::Movable);
        Visual->SetupAttachment(RootComponent);
        Visual->SetStaticMesh(ExternalMesh);
        Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Visual->RegisterComponent();
        bExternalShipVisualActive = true;
        TArray<UStaticMeshComponent*> Meshes;
        GetComponents<UStaticMeshComponent>(Meshes);
        for (UStaticMeshComponent* Mesh : Meshes)
        {
            if (!Mesh || Mesh == Visual || !Mesh->GetStaticMesh()) continue;
            if (!Mesh->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Game/SpacePatriot/Ships/KestrelK017/"))) continue;
            Mesh->SetVisibility(false);
            Mesh->SetHiddenInGame(true);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
    UpdateCockpitReadout();
}

void ASPFlightPawn::UnPossessed()
{
    // The same viewport alternates between on-foot and flight pawns. A ship
    // widget left on the player screen obscures the on-foot HUD after egress.
    if (MFDWidget)
    {
        MFDWidget->RemoveFromParent();
        MFDWidget = nullptr;
    }
    bMFDPointerMode = false;
    Super::UnPossessed();
}

void ASPFlightPawn::ResetMotionAfterWarp()
{
    FlightVelocityCmPerSecond = FVector::ZeroVector;
    AngularVelocityDegreesPerSecond = FVector::ZeroVector;
    ForwardInput = RightInput = UpInput = 0.0f;
    PitchInput = YawInput = RollInput = 0.0f;
    MousePitch = MouseYaw = 0.0f;
    bBrakeHeld = bBoostHeld = bLandingPending = bCruise = false;
    OnFlightStateChanged.Broadcast();
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
    PlayerInputComponent->BindAction(TEXT("SPLaunch"), IE_Pressed, this, &ASPFlightPawn::OnLaunchPressed);
    PlayerInputComponent->BindAction(TEXT("SPAssist"), IE_Pressed, this, &ASPFlightPawn::ToggleAssist);
    PlayerInputComponent->BindAction(TEXT("SPCruise"), IE_Pressed, this, &ASPFlightPawn::ToggleCruise);
    PlayerInputComponent->BindAction(TEXT("SPPower"), IE_Pressed, this, &ASPFlightPawn::TogglePower);
    PlayerInputComponent->BindAction(TEXT("SPCamera"), IE_Pressed, this, &ASPFlightPawn::ToggleCamera);
    PlayerInputComponent->BindAction(TEXT("SPMFDPointer"), IE_Pressed, this, &ASPFlightPawn::ToggleMFDPointer);
    PlayerInputComponent->BindAction(TEXT("SPLand"), IE_Pressed, this, &ASPFlightPawn::TryLand);
    PlayerInputComponent->BindAction(TEXT("SPTravelNext"), IE_Pressed, this, &ASPFlightPawn::InputNextTravelDestination);
    PlayerInputComponent->BindAction(TEXT("SPTravelJump"), IE_Pressed, this, &ASPFlightPawn::InputBeginHyperdriveJump);
    PlayerInputComponent->BindAction(TEXT("SPAutoRoute"), IE_Pressed, this, &ASPFlightPawn::ToggleAutoRoute);
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
    if (MFDWidget && CockpitMFDPage == 0)
    {
        MFDRefreshSeconds += DeltaSeconds;
        if (MFDRefreshSeconds >= 0.25f)
        {
            MFDRefreshSeconds = 0.0f;
            UpdateCockpitReadout();
        }
    }
    const bool bTravelHoldsPosition = TickTravel(Dt);
    if (bAutoRouteActive)
    {
        AdvanceAutoRoute(Dt);
        if (bAutoRouteActive && AutoRoutePhase != EAutoRoutePhase::Landing) return;
    }
    if (bTravelHoldsPosition) return;

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
    USPHyperjumpRouteComponent* Route = FindMFDHyperjumpRoute();
    const bool bSelected = Route && Route->GetNavigationComponent() == TravelNavigation
        ? Route->CycleDestination()
        : TravelNavigation && TravelNavigation->CycleDestination();
    if (!bSelected) return false;
    AutoRouteFeedback.Empty();
    LandingFeedback.Empty();
    FSPTravelWorld Destination;
    if (TravelNavigation->FindWorld(TravelNavigation->GetNavigationState().DestinationWorldId, Destination))
        AnnounceTravel(FString::Printf(TEXT("NAV target: %s  |  J charge  K cancel"), *Destination.Name));
    UpdateCockpitReadout();
    return true;
}

void ASPFlightPawn::AnnounceTravel(const FString& Message) const
{
    UE_LOG(LogTemp, Display, TEXT("Kestrel travel: %s"), *Message);
    // The integrated play loop has a persistent route HUD and cockpit MFD.
    // Keep transient debug text for the standalone flight map only.
    if (!FindMFDHyperjumpRoute() && GEngine && GetWorld() && GetWorld()->IsGameWorld())
        GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor(77, 183, 194), Message);
}

void ASPFlightPawn::SetTravelWorldSurface(ASPWorldSurface* InSurface)
{
    TravelWorldSurface = InSurface;
    if (IsValid(InSurface)) InSurface->FocusActor = this;
}

void ASPFlightPawn::BindIntegratedRoute(USPHyperjumpRouteComponent* Route)
{
    CachedMFDHyperjumpRoute = Route;
    // Configure binds the ship's existing navigation to the route. Keeping
    // the visual on this same component avoids a second, stale jump display.
    if (HyperdriveVisual && Route)
        HyperdriveVisual->SetNavigationComponent(Route->GetNavigationComponent());
}

bool ASPFlightPawn::StartAutoRoute()
{
    USPHyperjumpRouteComponent* Route = FindMFDHyperjumpRoute();
    if (bAutoRouteActive || !Route || Route->GetNavigationComponent() != TravelNavigation ||
        !IsValid(TravelWorldSurface) || !GetController() || !bPowered || FuelPercent < 8.0f)
    {
        AutoRouteFeedback = TEXT("UNAVAILABLE: board a powered ship with at least 8% fuel");
        UpdateCockpitReadout();
        return false;
    }
    const FSPHyperjumpRouteStatus Status = Route->GetRouteStatus();
    FSPTravelWorld Destination;
    if (!TravelNavigation->FindWorld(Status.Navigation.DestinationWorldId, Destination) ||
        Destination.Biome == TEXT("gas") ||
        (Status.Navigation.Phase != ESPTravelPhase::Flight &&
            Status.Navigation.Phase != ESPTravelPhase::Landed))
    {
        AutoRouteFeedback = TEXT("SELECT A SOLID WORLD BEFORE AUTO ROUTE");
        UpdateCockpitReadout();
        return false;
    }
    if (!bFlying && !Launch())
    {
        AutoRouteFeedback = TEXT("LAUNCH FAILED: CHECK POWER AND CLEARANCE");
        UpdateCockpitReadout();
        return false;
    }
    SetGearDown(false);
    FlightVelocityCmPerSecond = FVector::ZeroVector;
    AngularVelocityDegreesPerSecond = FVector::ZeroVector;
    bCruise = false;
    LandingFeedback.Empty();
    AutoRouteTargetId = Destination.Id;
    AutoRoutePhase = EAutoRoutePhase::Climb;
    bAutoRouteActive = true;
    AutoRouteFeedback = FString::Printf(TEXT("CLIMB TO JUMP CLEARANCE > %s"), *Destination.Name.ToUpper());
    AnnounceTravel(TEXT("Auto route engaged. Move or press X/R to take manual control."));
    UpdateCockpitReadout();
    return true;
}

void ASPFlightPawn::FinishAutoRoute(const FString& Message)
{
    bAutoRouteActive = false;
    AutoRoutePhase = EAutoRoutePhase::None;
    AutoRouteTargetId.Empty();
    AutoRouteFeedback = Message;
    AnnounceTravel(Message);
    UpdateCockpitReadout();
}

void ASPFlightPawn::CancelAutoRoute()
{
    if (!bAutoRouteActive) return;
    if (USPHyperjumpRouteComponent* Route = CachedMFDHyperjumpRoute.Get())
        Route->CancelJump(); // Transit cannot be interrupted; it finishes in manual flight.
    bLandingPending = false;
    FlightVelocityCmPerSecond = FVector::ZeroVector;
    FinishAutoRoute(TEXT("CANCELLED: MANUAL FLIGHT"));
}

void ASPFlightPawn::ToggleAutoRoute()
{
    if (bAutoRouteActive) CancelAutoRoute();
    else StartAutoRoute();
}

bool ASPFlightPawn::TryAutoRouteLanding()
{
    if (!IsValid(TravelWorldSurface)) return false;
    FlightVelocityCmPerSecond = FVector::ZeroVector;
    if (RequestSurfaceLanding()) return true;

    // Survey a small, deterministic neighborhood. Every accepted site still
    // goes through the real four-pad/hatch footprint and descent check.
    const FTransform Original = GetActorTransform();
    const FVector Center = TravelWorldSurface->GetPlanetCenterWorld();
    const FVector Up = (Original.GetLocation() - Center).GetSafeNormal();
    FVector East = FVector::VectorPlaneProject(GetActorForwardVector(), Up).GetSafeNormal();
    if (East.IsNearlyZero()) East = FVector::VectorPlaneProject(FVector::ForwardVector, Up).GetSafeNormal();
    const FVector North = FVector::CrossProduct(Up, East).GetSafeNormal();
    const double RadiusCm = TravelWorldSurface->GetScaledRadiusKm() * 100000.0;
    const int32 OffsetsCm[] = {0, 5000, -5000, 10000, -10000};
    for (const int32 X : OffsetsCm)
    {
        for (const int32 Y : OffsetsCm)
        {
            if (X == 0 && Y == 0) continue;
            const FVector Probe = Original.GetLocation() + East * X + North * Y;
            const FVector Radial = (Probe - Center).GetSafeNormal();
            const float HeightMeters = TravelWorldSurface->SampleAtWorldLocation(Probe).ElevationMeters;
            const FVector Candidate = Center + Radial * (RadiusCm + HeightMeters * 100.0 + 22000.0);
            const FVector Forward = FVector::VectorPlaneProject(Original.GetRotation().GetForwardVector(), Radial).GetSafeNormal();
            const FQuat Rotation = FRotationMatrix::MakeFromXZ(Forward.IsNearlyZero() ? East : Forward, Radial).ToQuat();
            FHitResult Sweep;
            if (!SetActorLocationAndRotation(Candidate, Rotation, true, &Sweep,
                ETeleportType::TeleportPhysics) || Sweep.bBlockingHit) continue;
            if (RequestSurfaceLanding()) return true;
        }
    }
    SetActorTransform(Original, false, nullptr, ETeleportType::TeleportPhysics);
    return false;
}

void ASPFlightPawn::AdvanceAutoRoute(float DeltaSeconds)
{
    if (!bAutoRouteActive) return;
    if (FMath::Abs(ForwardInput) > 0.1f || FMath::Abs(RightInput) > 0.1f ||
        FMath::Abs(UpInput) > 0.1f || bBrakeHeld)
    {
        CancelAutoRoute();
        return;
    }
    USPHyperjumpRouteComponent* Route = CachedMFDHyperjumpRoute.Get();
    if (!Route || !IsValid(TravelWorldSurface) || !TravelNavigation || !bPowered)
    {
        FinishAutoRoute(TEXT("AUTO ROUTE LOST: MANUAL FLIGHT"));
        return;
    }
    const FSPHyperjumpRouteStatus Status = Route->GetRouteStatus();
    if (AutoRoutePhase == EAutoRoutePhase::Climb)
    {
        if (!bFlying || Status.Navigation.DestinationWorldId != AutoRouteTargetId)
        {
            FinishAutoRoute(TEXT("DESTINATION CHANGED: MANUAL FLIGHT"));
            return;
        }
        const float AltitudeMeters = TravelWorldSurface->GetAltitudeMetersAtWorldLocation(GetActorLocation());
        if (AltitudeMeters >= 6200.0f)
        {
            if (Route->RequestJump())
            {
                AutoRoutePhase = EAutoRoutePhase::Charge;
                AutoRouteFeedback = TEXT("JUMP DRIVE CHARGING");
                UpdateCockpitReadout();
                return;
            }
            else if (AltitudeMeters >= 9000.0f ||
                !Route->GetRouteStatus().Message.Contains(TEXT("5 km clear")))
            {
                FinishAutoRoute(Route->GetRouteStatus().Message + TEXT("  MANUAL FLIGHT"));
                return;
            }
        }
        const FVector Up = (GetActorLocation() - TravelWorldSurface->GetPlanetCenterWorld()).GetSafeNormal();
        const FVector Forward = FVector::VectorPlaneProject(GetActorForwardVector(), Up).GetSafeNormal();
        SetActorRotation(FQuat::Slerp(GetActorQuat(),
            FRotationMatrix::MakeFromXZ(Forward.IsNearlyZero() ? FVector::ForwardVector : Forward, Up).ToQuat(),
            FMath::Clamp(DeltaSeconds * 2.0f, 0.0f, 1.0f)));
        FHitResult Hit;
        AddActorWorldOffset(Up * FMath::Min(80000.0f * DeltaSeconds,
            FMath::Max(0.0f, 9200.0f - AltitudeMeters) * 100.0f), true, &Hit);
        if (Hit.bBlockingHit) FinishAutoRoute(TEXT("ASCENT BLOCKED: MANUAL FLIGHT"));
        return;
    }
    if (AutoRoutePhase == EAutoRoutePhase::Charge)
    {
        if (Status.Navigation.Phase == ESPTravelPhase::JumpTransit)
        {
            AutoRoutePhase = EAutoRoutePhase::Transit;
            AutoRouteFeedback = TEXT("HYPERJUMP TRANSIT");
            UpdateCockpitReadout();
        }
        else if (Status.Navigation.Phase != ESPTravelPhase::JumpCharging)
            FinishAutoRoute(TEXT("JUMP INTERRUPTED: MANUAL FLIGHT"));
        return;
    }
    if (AutoRoutePhase == EAutoRoutePhase::Transit)
    {
        if (Status.Navigation.Phase == ESPTravelPhase::Flight &&
            Status.Navigation.CurrentWorldId == AutoRouteTargetId)
        {
            AutoRoutePhase = EAutoRoutePhase::Approach;
            AutoRouteFeedback = TEXT("SURFACE APPROACH");
            UpdateCockpitReadout();
        }
        else if (Status.Navigation.Phase != ESPTravelPhase::JumpTransit)
            FinishAutoRoute(TEXT("ARRIVAL INTERRUPTED: MANUAL FLIGHT"));
        return;
    }
    if (AutoRoutePhase == EAutoRoutePhase::Approach)
    {
        if (TravelWorldSurface->WorldId != AutoRouteTargetId || !bFlying)
        {
            FinishAutoRoute(TEXT("SURFACE UNAVAILABLE: MANUAL FLIGHT"));
            return;
        }
        const float AltitudeMeters = TravelWorldSurface->GetAltitudeMetersAtWorldLocation(GetActorLocation());
        if (AltitudeMeters <= 230.0f)
        {
            if (TryAutoRouteLanding())
            {
                AutoRoutePhase = EAutoRoutePhase::Landing;
                AutoRouteFeedback = TEXT("LANDING GEAR DOWN / DESCENT");
                UpdateCockpitReadout();
            }
            else FinishAutoRoute(TEXT("NO SAFE LANDING SITE: MANUAL FLIGHT"));
            return;
        }
        const FVector Down = -(GetActorLocation() - TravelWorldSurface->GetPlanetCenterWorld()).GetSafeNormal();
        FHitResult Hit;
        AddActorWorldOffset(Down * FMath::Min(45000.0f * DeltaSeconds,
            FMath::Max(0.0f, AltitudeMeters - 220.0f) * 100.0f), true, &Hit);
        if (Hit.bBlockingHit) FinishAutoRoute(TEXT("APPROACH BLOCKED: MANUAL FLIGHT"));
        return;
    }
    if (AutoRoutePhase == EAutoRoutePhase::Landing)
    {
        if (!bFlying) FinishAutoRoute(TEXT("LANDED: E TO EXIT / R FOR NEXT ROUTE"));
        else if (!bLandingPending) FinishAutoRoute(TEXT("LANDING INTERRUPTED: MANUAL FLIGHT"));
    }
}

bool ASPFlightPawn::BeginHyperdriveJump()
{
    USPHyperjumpRouteComponent* Route = FindMFDHyperjumpRoute();
    const bool bIntegrated = Route && Route->GetNavigationComponent() == TravelNavigation;
    const bool bStarted = bFlying && IsValid(TravelWorldSurface) && TravelNavigation &&
        (bIntegrated ? Route->RequestJump() : TravelNavigation->BeginJump(GetTravelContext()));
    if (!bStarted)
    {
        const FString Failure = bIntegrated ? Route->GetRouteStatus().Message :
            FString(TEXT("Jump blocked: set a target, retract gear, clear the port, climb above 2 km, and check fuel/heat"));
        AnnounceTravel(Failure);
        UpdateCockpitReadout();
        return false;
    }
    bCruise = false;
    TransitElapsedSeconds = 0.0f;
    AnnounceTravel(TEXT("Hyperdrive charging: K cancels before transit"));
    if (HyperdriveVisual) HyperdriveVisual->RefreshVisuals(0.0f);
    UpdateCockpitReadout();
    OnFlightStateChanged.Broadcast();
    return true;
}

bool ASPFlightPawn::CancelHyperdriveJump()
{
    USPHyperjumpRouteComponent* Route = FindMFDHyperjumpRoute();
    const bool bIntegrated = Route && Route->GetNavigationComponent() == TravelNavigation;
    if (!(bIntegrated ? Route->CancelJump() : TravelNavigation && TravelNavigation->CancelJump())) return false;
    TravelNavigation->SetDriveMode(ESPTravelDriveMode::SCM);
    bCruise = false;
    AnnounceTravel(TEXT("Charge canceled: manual flight restored"));
    if (HyperdriveVisual) HyperdriveVisual->RefreshVisuals(0.0f);
    UpdateCockpitReadout();
    OnFlightStateChanged.Broadcast();
    return true;
}

bool ASPFlightPawn::TickTravel(float DeltaSeconds)
{
    if (!TravelNavigation) return false;
    if (USPHyperjumpRouteComponent* Route = CachedMFDHyperjumpRoute.Get();
        Route && Route->GetNavigationComponent() == TravelNavigation)
    {
        // The director's route owns charge, transit and world activation.
        // Never advance or complete that same navigation state a second time.
        FSPTravelNavigationState State = TravelNavigation->GetNavigationState();
        if (bBrakeHeld && State.Phase == ESPTravelPhase::JumpCharging)
        {
            Route->CancelJump();
            State = TravelNavigation->GetNavigationState();
        }
        if (State.Phase == ESPTravelPhase::JumpTransit)
        {
            if (PreviousTravelPhase != ESPTravelPhase::JumpTransit)
            {
                FlightVelocityCmPerSecond = FVector::ZeroVector;
                AngularVelocityDegreesPerSecond = FVector::ZeroVector;
                bCruise = false;
                AnnounceTravel(TEXT("Hyperdrive transit: hold position"));
                OnFlightStateChanged.Broadcast();
            }
            PreviousTravelPhase = State.Phase;
            return true;
        }
        PreviousTravelPhase = State.Phase;
        return false;
    }
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

USPHyperjumpRouteComponent* ASPFlightPawn::FindMFDHyperjumpRoute() const
{
    if (CachedMFDHyperjumpRoute.IsValid()) return CachedMFDHyperjumpRoute.Get();
    if (!GetWorld()) return nullptr;
    for (TActorIterator<ASPPlayLoopDirector> It(GetWorld()); It; ++It)
    {
        if (It->Ship != this || !IsValid(It->HyperjumpRoute) ||
            !It->HyperjumpRoute->GetRouteStatus().bReady) continue;
        CachedMFDHyperjumpRoute = It->HyperjumpRoute;
        return It->HyperjumpRoute;
    }
    return nullptr;
}

bool ASPFlightPawn::SelectNextMFDDestination()
{
    USPHyperjumpRouteComponent* Route = FindMFDHyperjumpRoute();
    const bool bSelected = Route && Route->CycleDestination();
    if (bSelected)
    {
        AutoRouteFeedback.Empty();
        LandingFeedback.Empty();
    }
    UpdateCockpitReadout();
    OnFlightStateChanged.Broadcast();
    return bSelected;
}

bool ASPFlightPawn::RequestMFDJump()
{
    USPHyperjumpRouteComponent* Route = FindMFDHyperjumpRoute();
    const bool bStarted = Route && Route->RequestJump();
    UpdateCockpitReadout();
    OnFlightStateChanged.Broadcast();
    return bStarted;
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
        if (bExternalShipVisualActive && StaticMesh &&
            StaticMesh->GetPathName().StartsWith(TEXT("/Game/SpacePatriot/Ships/KestrelK017/")))
        {
            Mesh->SetVisibility(false);
            Mesh->SetHiddenInGame(true);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            continue;
        }
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
        if (const USPHyperjumpRouteComponent* Route = FindMFDHyperjumpRoute())
        {
            const FSPHyperjumpRouteStatus Status = Route->GetRouteStatus();
            const FString Here = Status.CurrentWorldName.IsEmpty()
                ? Status.Navigation.CurrentWorldId.ToUpper() : Status.CurrentWorldName.ToUpper();
            const FString Destination = Status.DestinationWorldName.IsEmpty()
                ? (Status.Navigation.DestinationWorldId.IsEmpty()
                    ? TEXT("NONE") : Status.Navigation.DestinationWorldId.ToUpper())
                : Status.DestinationWorldName.ToUpper();
            FString Phase;
            switch (Status.Navigation.Phase)
            {
            case ESPTravelPhase::JumpCharging:
                Phase = FString::Printf(TEXT("CHARGING %02.0f%%"), Status.ChargeFraction * 100.0f);
                break;
            case ESPTravelPhase::JumpTransit:
                Phase = TEXT("IN TRANSIT");
                break;
            default:
                Phase = Status.Navigation.DestinationWorldId.IsEmpty()
                    ? TEXT("SELECT DESTINATION") : TEXT("ROUTE SELECTED");
                break;
            }
            const FString Feedback = !AutoRouteFeedback.IsEmpty() ? AutoRouteFeedback
                : !LandingFeedback.IsEmpty() ? LandingFeedback
                : Status.Message.IsEmpty() ? TEXT("DEST cycles worlds; JUMP engages drive.") : Status.Message;
            Text = FString::Printf(TEXT("MFD 1/4  NAV\nHERE %s  >  DEST %s\nSPD %03.0f m/s  FUEL %03.0f%%  GEAR %s\n%s\n%s"),
                *Here, *Destination, GetFlightTelemetry().SpeedMetersPerSecond,
                FuelPercent, bGearDown ? TEXT("DOWN") : TEXT("UP"),
                *Phase, *Feedback);
        }
        else
        {
            Text = TEXT("MFD 1/4  NAV\nROUTE OFFLINE\nBoard the live ship to connect navigation.");
        }
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
    if (CockpitMFDPage != 0)
    {
        if (!AutoRouteFeedback.IsEmpty()) Text += TEXT("\n") + AutoRouteFeedback;
        else if (!LandingFeedback.IsEmpty()) Text += TEXT("\n") + LandingFeedback;
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
    const auto Reject = [this](const TCHAR* Reason)
    {
        LandingFeedback = Reason;
        UpdateCockpitReadout();
        OnFlightStateChanged.Broadcast();
        return false;
    };
    if (!bFlying) return Reject(TEXT("LAND: ship is already grounded"));
    // Match the original flight loop's 80 m/s approach limit. Gear extends
    // automatically once a safe footprint is found, as it does in the source.
    if (FlightVelocityCmPerSecond.Size() >= 8000.0f)
        return Reject(TEXT("LAND: slow below 80 m/s"));
    if (!GetWorld()) return Reject(TEXT("LAND: no active world"));
    const FVector Down = TraceDown();
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpacePatriotLanding), false, this);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(),
        GetActorLocation() + Down * (65000.0f + StandHeightCm), ECC_Visibility, Params))
        return Reject(TEXT("LAND: no ground within 650 m"));
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    const float Altitude = FVector::DotProduct(GetActorLocation() - Hit.ImpactPoint, Normal) - StandHeightCm;
    if (Altitude < -200.0f || Altitude >= 65000.0f)
        return Reject(TEXT("LAND: descend within 650 m"));
    if (FVector::DotProduct(GetActorUpVector(), Normal) < 0.6f)
        return Reject(TEXT("LAND: level the ship"));
    FVector Forward = FVector::VectorPlaneProject(GetActorForwardVector(), Normal).GetSafeNormal();
    if (Forward.IsNearlyZero()) Forward = FVector::CrossProduct(Normal, FVector::RightVector).GetSafeNormal();
    const FVector CandidatePosition = Hit.ImpactPoint + Normal * StandHeightCm;
    const FQuat CandidateRotation = FRotationMatrix::MakeFromXZ(Forward, Normal).ToQuat();
    const auto HasSupport = [this, &Params, &Hit, &Normal, &CandidatePosition, &CandidateRotation]
        (const FVector& LocalOffset, float MaxHeightDifferenceCm)
    {
        const FVector Foot = CandidatePosition + CandidateRotation.RotateVector(LocalOffset);
        FHitResult Support;
        if (!GetWorld()->LineTraceSingleByChannel(Support, Foot + Normal * 600.0f,
            Foot - Normal * 1500.0f, ECC_Visibility, Params)) return false;
        return FVector::DotProduct(Support.ImpactNormal.GetSafeNormal(), Normal) >= 0.75f &&
            FMath::Abs(FVector::DotProduct(Support.ImpactPoint - Hit.ImpactPoint, Normal))
                <= MaxHeightDifferenceCm;
    };
    const float PadHeight = -StandHeightCm;
    if (!HasSupport(FVector(300.0f, -220.0f, PadHeight), 150.0f) ||
        !HasSupport(FVector(300.0f, 220.0f, PadHeight), 150.0f) ||
        !HasSupport(FVector(-300.0f, 0.0f, PadHeight), 150.0f) ||
        !HasSupport(FVector(-300.0f, -800.0f, PadHeight), 250.0f))
        return Reject(TEXT("LAND: find a broad, level site with hatch access"));
    LandingPosition = CandidatePosition;
    LandingRotation = CandidateRotation;
    SurfaceNormal = Normal;
    SetGearDown(true);
    bLandingPending = true;
    bCruise = false;
    LandingFeedback = TEXT("LAND: gear extending, descent engaged");
    UpdateCockpitReadout();
    OnFlightStateChanged.Broadcast();
    return true;
}
