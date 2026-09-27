#include "SPPlayLoopDirector.h"

#include "SPFlightPawn.h"
#include "SPHyperjumpRouteComponent.h"
#include "SPJourneySave.h"
#include "SPPlayLoopWidget.h"
#include "SPTravelNavigationComponent.h"
#include "SPWorldSurface.h"
#include "SPWorldDressing.h"
#include "SPDayNightCycleComponent.h"
#include "SpacePatriotBlueprintBases.h"
#include "SpacePatriotSystemsComponent.h"
#include "SPSocietySimulationComponent.h"
#include "SPStoryCampaignComponent.h"
#include "SPFieldSurveyComponent.h"

#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

ASPPlayLoopDirector::ASPPlayLoopDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PlayLoopRoot"));
    SetRootComponent(SceneRoot);
    HyperjumpRoute = CreateDefaultSubobject<USPHyperjumpRouteComponent>(TEXT("HyperjumpRoute"));
    DayNightCycle = CreateDefaultSubobject<USPDayNightCycleComponent>(TEXT("DayNightCycle"));
    AutoReceiveInput = EAutoReceiveInput::Player0;
    InputPriority = 5;
}

void ASPPlayLoopDirector::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<ASPWorldSurface> It(GetWorld()); It; ++It)
    {
        Surface = *It;
        break;
    }
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        RespawnStart = *It;
        break;
    }
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && Actor->ActorHasTag(TEXT("SP_DisplayOnly")))
        {
            // An editor-only cruiser display shares the live flight pawn's parking spot.
            // It must never mask the boardable ship or block its takeoff in PIE.
            Actor->SetActorHiddenInGame(true);
            Actor->SetActorEnableCollision(false);
            Actor->SetActorTickEnabled(false);
        }
        if (!Actor || !Actor->ActorHasTag(TEXT("SP_EarthOnly"))) continue;
        FPortActorState& State = EarthOnlyActors.AddDefaulted_GetRef();
        State.Actor = Actor;
        State.bHidden = Actor->IsHidden();
        State.bCollision = Actor->GetActorEnableCollision();
        State.bTickEnabled = Actor->IsActorTickEnabled();
    }
    if (HyperjumpRoute) HyperjumpRoute->OnArrived.AddDynamic(this, &ASPPlayLoopDirector::OnWorldArrived);
    InitializeWhenReady();
}

void ASPPlayLoopDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    InitializeWhenReady();
    RefreshTravelPhase();
    RefreshSurveyContext();
    RefreshStatus();
}

void ASPPlayLoopDirector::BindPlayerInput()
{
    if (bInputBound || !Player) return;
    EnableInput(Player);
    if (!InputComponent) return;
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ASPPlayLoopDirector::OnInteract).bConsumeInput = false;
    InputComponent->BindKey(EKeys::N, IE_Pressed, this, &ASPPlayLoopDirector::OnCycleDestination).bConsumeInput = false;
    InputComponent->BindKey(EKeys::J, IE_Pressed, this, &ASPPlayLoopDirector::OnJump).bConsumeInput = false;
    // Function keys F8/F9 are Editor viewport shortcuts during PIE. Keep
    // journey controls usable in Selected Viewport as well as packaged play.
    InputComponent->BindKey(EKeys::K, IE_Pressed, this, &ASPPlayLoopDirector::OnSaveJourney).bConsumeInput = false;
    InputComponent->BindKey(EKeys::O, IE_Pressed, this, &ASPPlayLoopDirector::OnLoadJourney).bConsumeInput = false;
    InputComponent->BindKey(EKeys::U, IE_Pressed, this, &ASPPlayLoopDirector::OnSkipTime).bConsumeInput = false;
    // A shifted chord masks the plain B binding in Unreal's input stack.
    // Explicit bindings also work in PIE, where modifier polling in a B
    // callback does not reliably report Shift as held.
    InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ASPPlayLoopDirector::OnScanPressed).bConsumeInput = false;
    InputComponent->BindKey(FInputChord(EKeys::B, true, false, false, false), IE_Pressed,
        this, &ASPPlayLoopDirector::OnSamplePressed).bConsumeInput = false;
    bInputBound = true;
}

void ASPPlayLoopDirector::InitializeWhenReady()
{
    if (!Player) Player = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!Player || !Surface) return;
    BindPlayerInput();
    if (APawn* CurrentPawn = Player->GetPawn(); IsValid(CurrentPawn) && CurrentPawn != Ship && CurrentPawn != GroundPawn)
    {
        // The Shooter controller replaces its pawn after death. Boarding and
        // surface focus must follow that new pawn, not a destroyed pointer.
        GroundPawn = CurrentPawn;
        Surface->FocusActor = GroundPawn;
    }
    if (!IsValid(GroundPawn)) return;

    if (!StatusWidget && Player->IsLocalController())
    {
        StatusWidget = CreateWidget<USPPlayLoopWidget>(Player, USPPlayLoopWidget::StaticClass());
        if (StatusWidget) StatusWidget->AddToPlayerScreen(35);
    }
    if (Ship) return;

    if (!ShipClass)
    {
        ShipClass = LoadClass<ASPFlightPawn>(nullptr,
            TEXT("/Game/SpacePatriot/Blueprints/BP_KestrelFlyable.BP_KestrelFlyable_C"));
    }
    if (!ShipClass)
    {
        LastAction = TEXT("Kestrel flight Blueprint is missing. Run the local dependency installer.");
        return;
    }

    const FTransform Transform(ParkedShipRotation, ParkedShipLocation);
    Ship = GetWorld()->SpawnActorDeferred<ASPFlightPawn>(ShipClass, Transform, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Ship)
    {
        LastAction = TEXT("Could not spawn the Kestrel at the port.");
        return;
    }
    Ship->AutoPossessPlayer = EAutoReceiveInput::Disabled;
    Ship->bFlying = false;
    Ship->bGearDown = true;
    UGameplayStatics::FinishSpawningActor(Ship, Transform);
    if (!HyperjumpRoute || !HyperjumpRoute->Configure(Ship, Surface))
    {
        LastAction = TEXT("World navigation could not initialize.");
        return;
    }
    Surface->FocusActor = GroundPawn;
    if (!WorldDressing)
    {
        WorldDressing = GetWorld()->SpawnActor<ASPWorldDressing>(ASPWorldDressing::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator);
    }
    if (WorldDressing) WorldDressing->ApplyWorld(Surface->WorldId, Surface, Ship);
    bShipWasFlying = false;
    if (USPTravelNavigationComponent* Navigation = HyperjumpRoute->GetNavigationComponent())
    {
        FSPTravelNavigationState State = Navigation->GetNavigationState();
        State.Phase = ESPTravelPhase::Landed;
        State.DriveMode = ESPTravelDriveMode::SCM;
        Navigation->RestoreSaveState(State);
    }
    SetEarthPortVisible(Surface->WorldId == TEXT("earth"));
    SetSurveySiteAtShip(Surface->WorldId);
    LastAction = TEXT("Walk to the Kestrel's port-side hatch to board.");
    FeedbackUntilSeconds = GetWorld()->GetTimeSeconds() + 8.0f;
}

FVector ASPPlayLoopDirector::GetBoardingLocation() const
{
    return Ship ? Ship->GetActorTransform().TransformPosition(BoardingOffsetLocal) : ParkedShipLocation;
}

bool ASPPlayLoopDirector::IsPiloting() const
{
    return Player && Ship && Player->GetPawn() == Ship;
}

bool ASPPlayLoopDirector::TryBoard()
{
    if (!Player || !GroundPawn || !Ship)
    {
        LastAction = TEXT("The Kestrel is not ready. Check that its local asset dependency is installed.");
        return false;
    }
    if (Player->GetPawn() != GroundPawn)
    {
        LastAction = TEXT("Return to your on-foot character before boarding.");
        return false;
    }
    if (Ship->bFlying)
    {
        LastAction = TEXT("Land the ship before boarding.");
        return false;
    }
    const float Distance = FVector::Distance(GroundPawn->GetActorLocation(), GetBoardingLocation());
    if (Distance > BoardingRangeCm)
    {
        LastAction = FString::Printf(TEXT("Kestrel hatch %0.0f m away. Move to its port side and press E."),
            Distance / 100.0f);
        return false;
    }

    GroundPawn->SetActorHiddenInGame(true);
    GroundPawn->SetActorEnableCollision(false);
    if (ACharacter* Character = Cast<ACharacter>(GroundPawn))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement()) Movement->DisableMovement();
    }
    Player->Possess(Ship);
    if (Player->GetPawn() != Ship)
    {
        GroundPawn->SetActorHiddenInGame(false);
        GroundPawn->SetActorEnableCollision(true);
        LastAction = TEXT("The pilot seat did not accept possession.");
        return false;
    }
    Surface->FocusActor = Ship;
    LastAction = TEXT("Kestrel boarded. Space launches; N selects a world; J initiates hyperjump.");
    return true;
}

bool ASPPlayLoopDirector::FindEgressLocation(FVector& OutLocation) const
{
    if (!Ship || !GetWorld()) return false;
    const FVector Up = Ship->GetActorUpVector().GetSafeNormal();
    const FVector Hatch = GetBoardingLocation();
    const FVector Start = Hatch + Up * 650.0f;
    const FVector End = Hatch - Up * 2500.0f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SpacePatriotEgress), false, Ship);
    if (GroundPawn) Params.AddIgnoredActor(GroundPawn);
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ||
        FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), Up) < 0.45f) return false;
    float StandingHeight = 100.0f;
    if (const ACharacter* Character = Cast<ACharacter>(GroundPawn))
    {
        if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
            StandingHeight = Capsule->GetScaledCapsuleHalfHeight() + 10.0f;
    }
    OutLocation = Hit.ImpactPoint + Up * StandingHeight;
    return true;
}

void ASPPlayLoopDirector::UpdateRespawnAnchor()
{
    if (!IsValid(RespawnStart) || !IsValid(Ship) || Ship->bFlying) return;
    FVector Location;
    if (FindEgressLocation(Location))
    {
        RespawnStart->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
        RespawnStart->SetActorRotation(FRotator(0.0f, Ship->GetActorRotation().Yaw, 0.0f));
    }
}

bool ASPPlayLoopDirector::TryDisembark()
{
    if (!IsPiloting() || Ship->bFlying || !GroundPawn || !HyperjumpRoute) return false;
    const FSPHyperjumpRouteStatus Status = HyperjumpRoute->GetRouteStatus();
    if (!Status.bSurfaceLandable)
    {
        LastAction = TEXT("This world has no landable surface.");
        return false;
    }
    FVector ExitLocation;
    if (!FindEgressLocation(ExitLocation))
    {
        LastAction = TEXT("No safe ground at the hatch. Reposition the ship before exiting.");
        return false;
    }
    GroundPawn->SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);
    GroundPawn->SetActorRotation(FRotator(0.0f, Ship->GetActorRotation().Yaw, 0.0f));
    GroundPawn->SetActorEnableCollision(true);
    GroundPawn->SetActorHiddenInGame(false);
    if (ACharacter* Character = Cast<ACharacter>(GroundPawn))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
            Movement->SetMovementMode(MOVE_Walking);
    }
    Player->Possess(GroundPawn);
    if (Player->GetPawn() != GroundPawn)
    {
        GroundPawn->SetActorEnableCollision(false);
        GroundPawn->SetActorHiddenInGame(true);
        LastAction = TEXT("Could not leave the pilot seat.");
        return false;
    }
    Surface->FocusActor = GroundPawn;
    UpdateRespawnAnchor();
    LastAction = TEXT("On foot. Explore, then return to the hatch and press E to board.");
    return true;
}

void ASPPlayLoopDirector::RefreshTravelPhase()
{
    if (!Ship || !HyperjumpRoute || Ship->bFlying == bShipWasFlying) return;
    bShipWasFlying = Ship->bFlying;
    USPTravelNavigationComponent* Navigation = HyperjumpRoute->GetNavigationComponent();
    if (!Navigation) return;
    FSPTravelContext Context;
    Context.bPilotAtControls = IsPiloting();
    Context.bShipPowered = Ship->bPowered;
    Context.bShipTransitioning = false;
    Context.bSafeLandingFootprint = true; // Physical landing trace already established a safe contact.
    Context.SpeedMetersPerSecond = Ship->GetFlightTelemetry().SpeedMetersPerSecond;
    Context.SurfaceClearanceMeters = 0.0;
    if (Ship->bFlying)
    {
        Navigation->Launch(Context);
        LastAction = TEXT("Airborne. Retract gear with G and climb above 2 km to jump.");
    }
    else
    {
        if (Navigation->RequestSurfaceLanding(Context)) Navigation->ConfirmSurfaceTouchdown(Context);
        // Arrival dressing is initially anchored under the ship's exterior
        // jump position. Rebuild it at the actual touchdown site before the
        // player steps out or the nearby port board resolves its settlement.
        if (WorldDressing) WorldDressing->ApplyWorld(Surface->WorldId, Surface, Ship);
        SetSurveySiteAtShip(Surface->WorldId);
        UpdateRespawnAnchor();
        LastAction = TEXT("Landed. Press E to leave the ship.");
    }
}

void ASPPlayLoopDirector::SetSurveySiteAtShip(const FString& WorldId)
{
    if (!IsValid(Ship)) return;
    if (!IsValid(ActiveSurvey))
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (USPFieldSurveyComponent* Survey = It->FindComponentByClass<USPFieldSurveyComponent>())
            {
                ActiveSurvey = Survey;
                break;
            }
        }
    }
    // The current landing site is the prototype's survey anchor. Rebinding
    // this runtime position never resets a saved per-world survey record.
    bSurveySiteBound = IsValid(ActiveSurvey) && ActiveSurvey->State &&
        ActiveSurvey->ActivateWorld(WorldId, Ship->GetActorLocation());
}

void ASPPlayLoopDirector::RefreshSurveyContext()
{
    if (!IsValid(GroundPawn) || !Player || !Surface) return;
    if (!IsValid(ActiveSurvey) || !bSurveySiteBound) SetSurveySiteAtShip(Surface->WorldId);
    if (IsValid(ActiveSurvey))
        ActiveSurvey->UpdatePlayerContext(GroundPawn->GetActorLocation(),
            Player->GetPawn() == GroundPawn, false);
}

void ASPPlayLoopDirector::SetEarthPortVisible(bool bVisible)
{
    for (const FPortActorState& State : EarthOnlyActors)
    {
        AActor* Actor = State.Actor.Get();
        if (!Actor) continue;
        const bool bDisplayOnly = Actor->ActorHasTag(TEXT("SP_DisplayOnly"));
        Actor->SetActorHiddenInGame(bDisplayOnly || !bVisible || State.bHidden);
        Actor->SetActorEnableCollision(!bDisplayOnly && bVisible && State.bCollision);
        Actor->SetActorTickEnabled(!bDisplayOnly && bVisible && State.bTickEnabled);
    }
}

void ASPPlayLoopDirector::OnWorldArrived(const FString& WorldId)
{
    SetEarthPortVisible(WorldId == TEXT("earth"));
    if (Surface) Surface->FocusActor = Ship;
    if (WorldDressing) WorldDressing->ApplyWorld(WorldId, Surface, Ship);
    SetSurveySiteAtShip(WorldId);
    // The offscreen society, Storyworks and wildlife catalog must follow the
    // same world the player can see after the jump.
    for (TActorIterator<ASPWorldRuntime> It(GetWorld()); It; ++It)
    {
        if (It->Systems) It->Systems->SetActiveWorld(WorldId);
    }
    LastAction = WorldId == TEXT("earth")
        ? TEXT("Earth arrival. Descend toward Kellen Reach or land elsewhere.")
        : TEXT("New world. Descend within 650 m, level the ship, then press L to land.");
}

void ASPPlayLoopDirector::OnInteract()
{
    FeedbackUntilSeconds = GetWorld()->GetTimeSeconds() + 5.0f;
    if (IsPiloting()) TryDisembark();
    else TryBoard();
}

void ASPPlayLoopDirector::OnCycleDestination()
{
    if (IsPiloting() && HyperjumpRoute && HyperjumpRoute->CycleDestination())
        LastAction = TEXT("Navigation destination changed. Press J when clear to jump.");
}

void ASPPlayLoopDirector::OnJump()
{
    if (!IsPiloting() || !HyperjumpRoute) return;
    const bool bStarted = HyperjumpRoute->RequestJump();
    LastAction = bStarted ? TEXT("Hyperdrive charging. Maintain clearance and keep the gear retracted.")
        : HyperjumpRoute->GetRouteStatus().Message;
    FeedbackUntilSeconds = GetWorld()->GetTimeSeconds() + 6.0f;
}

void ASPPlayLoopDirector::OnScanPressed()
{
    if (!IsPiloting()) TrySurveyAction(false);
}

void ASPPlayLoopDirector::OnSamplePressed()
{
    if (!IsPiloting()) TrySurveyAction(true);
}

bool ASPPlayLoopDirector::TrySurveyAction(bool bCollectSample)
{
    if (!GetWorld() || !Player || !IsValid(GroundPawn) || Player->GetPawn() != GroundPawn ||
        !IsValid(Surface)) return false;

    RefreshSurveyContext();
    FeedbackUntilSeconds = GetWorld()->GetTimeSeconds() + 6.0f;
    FSPFieldSurveyStatus Status;
    if (!IsValid(ActiveSurvey) || !ActiveSurvey->GetCurrentStatus(Status))
    {
        LastAction = TEXT("Field survey is unavailable at this landing site.");
        return false;
    }
    constexpr double SurveyRangeMeters = 300.0;
    const bool bAtSite = Status.DistanceMeters >= 0.0 && Status.DistanceMeters < SurveyRangeMeters;

    if (bCollectSample)
    {
        if (Status.Phase != TEXT("sample"))
        {
            LastAction = Status.bComplete ? TEXT("This field report is already complete.")
                : TEXT("Scan the area with B before collecting a sample.");
            return false;
        }
        if (!bAtSite)
        {
            LastAction = FString::Printf(TEXT("Return to the survey site (%0.0f m away) to take a sample."),
                Status.DistanceMeters);
            return false;
        }
        USPSocietySimulationComponent* Society = nullptr;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            Society = It->FindComponentByClass<USPSocietySimulationComponent>();
            if (Society) break;
        }
        if (!Society || !Society->AddPlayerSupply(TEXT("sample"), 1))
        {
            LastAction = TEXT("The sample could not be stowed in the cargo inventory.");
            return false;
        }
        if (!ActiveSurvey->NotifyFieldSample(Status.WorldId, TEXT("surface-sampler"), true))
        {
            Society->AddPlayerSupply(TEXT("sample"), -1);
            LastAction = TEXT("Sample collection failed. Stand at the survey site and try again.");
            return false;
        }
        if (Society->bAutoSave) Society->SaveSociety();
        LastAction = FString::Printf(TEXT("%s field report archived. One sample added to cargo."), *Status.Body);
        return true;
    }

    const FSPWorldSurfaceSample Terrain = Surface->SampleAtWorldLocation(GroundPawn->GetActorLocation());
    FString Wildlife = TEXT("no wildlife within 80 m");
    double NearestCreatureDistance = 8000.0;
    for (TActorIterator<ASPWildlifeEncounter> It(GetWorld()); It; ++It)
    {
        if (It->IsHidden() || It->Health <= 0.0f) continue;
        const double Distance = FVector::Distance(GroundPawn->GetActorLocation(), It->GetActorLocation());
        if (Distance >= NearestCreatureDistance) continue;
        NearestCreatureDistance = Distance;
        Wildlife = FString::Printf(TEXT("wildlife %s at %0.0f m"), *It->CreatureId, Distance / 100.0);
    }
    const FString Readout = FString::Printf(TEXT("%s terrain, rock %d%%, moisture %d%%; %s"),
        *Terrain.RegionStyle.ToString(),
        FMath::RoundToInt(FMath::Clamp(Terrain.Rock, 0.0f, 1.0f) * 100.0f),
        FMath::RoundToInt(FMath::Clamp(Terrain.Moisture, 0.0f, 1.0f) * 100.0f), *Wildlife);
    if (!bAtSite)
    {
        LastAction = FString::Printf(TEXT("Local scan: %s. Field site %0.0f m away."),
            *Readout, Status.DistanceMeters);
        return false;
    }
    if (Status.Phase != TEXT("scan"))
    {
        LastAction = FString::Printf(TEXT("Local scan: %s. %s"), *Readout, *Status.Title);
        return false;
    }
    if (!ActiveSurvey->NotifyWorldScanned(Status.WorldId, true))
    {
        LastAction = TEXT("Scanner did not register this survey. Try again at the site.");
        return false;
    }
    FSPFieldSurveyStatus Updated;
    ActiveSurvey->GetCurrentStatus(Updated);
    LastAction = FString::Printf(TEXT("Survey scan: %s. %s"), *Readout, *Updated.Title);
    return true;
}

bool ASPPlayLoopDirector::SaveSimulationCheckpoint()
{
    bool bSaved = true;
    bool bFoundSociety = false, bFoundCampaign = false, bFoundSurvey = false;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (USPSocietySimulationComponent* Society = It->FindComponentByClass<USPSocietySimulationComponent>())
        {
            bFoundSociety = true;
            const FString AutoSaveSlot = Society->SaveSlot;
            Society->SaveSlot = AutoSaveSlot + TEXT("_Journey");
            bSaved &= !AutoSaveSlot.IsEmpty() && Society->SaveSociety();
            Society->SaveSlot = AutoSaveSlot;
        }
        if (USPStoryCampaignComponent* Campaign = It->FindComponentByClass<USPStoryCampaignComponent>())
        {
            bFoundCampaign = true;
            const FString AutoSaveSlot = Campaign->SaveSlot;
            Campaign->SaveSlot = AutoSaveSlot + TEXT("_Journey");
            bSaved &= !AutoSaveSlot.IsEmpty() && Campaign->SaveCampaign();
            Campaign->SaveSlot = AutoSaveSlot;
        }
        if (USPFieldSurveyComponent* Survey = It->FindComponentByClass<USPFieldSurveyComponent>())
        {
            bFoundSurvey = true;
            const FString AutoSaveSlot = Survey->SaveSlot;
            Survey->SaveSlot = AutoSaveSlot + TEXT("_Journey");
            bSaved &= !AutoSaveSlot.IsEmpty() && Survey->SaveSurveys();
            Survey->SaveSlot = AutoSaveSlot;
        }
    }
    return bSaved && bFoundSociety && bFoundCampaign && bFoundSurvey;
}

bool ASPPlayLoopDirector::LoadSimulationCheckpoint(const FString& WorldId, bool bValidateOnly)
{
    TArray<USPSocietySimulationComponent*> Societies;
    TArray<USPStoryCampaignComponent*> Campaigns;
    TArray<USPFieldSurveyComponent*> Surveys;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (USPSocietySimulationComponent* Society = It->FindComponentByClass<USPSocietySimulationComponent>()) Societies.Add(Society);
        if (USPStoryCampaignComponent* Campaign = It->FindComponentByClass<USPStoryCampaignComponent>()) Campaigns.Add(Campaign);
        if (USPFieldSurveyComponent* Survey = It->FindComponentByClass<USPFieldSurveyComponent>()) Surveys.Add(Survey);
    }
    if (Societies.IsEmpty() || Campaigns.IsEmpty() || Surveys.IsEmpty()) return false;
    // Check every manual slot before replacing any live component state. The
    // components' ordinary autosave slots continue to update independently.
    for (const USPSocietySimulationComponent* Society : Societies)
        if (!Cast<USPSocietySaveGame>(UGameplayStatics::LoadGameFromSlot(Society->SaveSlot + TEXT("_Journey"), 0))) return false;
    for (const USPStoryCampaignComponent* Campaign : Campaigns)
        if (!Cast<USPSStoryCampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(Campaign->SaveSlot + TEXT("_Journey"), 0))) return false;
    for (const USPFieldSurveyComponent* Survey : Surveys)
        if (!Cast<USPSFieldSurveySaveGame>(UGameplayStatics::LoadGameFromSlot(Survey->SaveSlot + TEXT("_Journey"), 0))) return false;
    if (bValidateOnly) return true;

    bool bLoaded = true;
    // Society must be restored before the campaign applies any pending rewards.
    for (USPSocietySimulationComponent* Society : Societies)
    {
        const FString AutoSaveSlot = Society->SaveSlot;
        Society->SaveSlot = AutoSaveSlot + TEXT("_Journey");
        bLoaded &= Society->InitializeSociety(true);
        Society->SaveSlot = AutoSaveSlot;
    }
    for (USPStoryCampaignComponent* Campaign : Campaigns)
    {
        const FString AutoSaveSlot = Campaign->SaveSlot;
        Campaign->SaveSlot = AutoSaveSlot + TEXT("_Journey");
        bLoaded &= Campaign->InitializeCampaign(true);
        Campaign->SaveSlot = AutoSaveSlot;
    }
    for (USPFieldSurveyComponent* Survey : Surveys)
    {
        const FString AutoSaveSlot = Survey->SaveSlot;
        Survey->SaveSlot = AutoSaveSlot + TEXT("_Journey");
        bLoaded &= Survey->InitializeSurveys(true);
        Survey->SaveSlot = AutoSaveSlot;
    }
    if (bLoaded) SetSurveySiteAtShip(WorldId);
    return bLoaded;
}

void ASPPlayLoopDirector::OnSaveJourney()
{
    FeedbackUntilSeconds = GetWorld()->GetTimeSeconds() + 5.0f;
    USPTravelNavigationComponent* Navigation = HyperjumpRoute ? HyperjumpRoute->GetNavigationComponent() : nullptr;
    FSPJourneyState State;
    if (!USPJourneySaveLibrary::CaptureJourney(Player, GroundPawn, Ship, Navigation, State))
    {
        LastAction = TEXT("Save unavailable during a transition or without a valid pilot/ship.");
        return;
    }
    if (DayNightCycle) State.TimeOfDayHours = DayNightCycle->CurrentHour;
    if (!USPJourneySaveLibrary::SaveJourney(State, TEXT("SpacePatriot_Journey")))
    {
        LastAction = TEXT("Save unavailable during a transition or without a valid pilot/ship.");
        return;
    }
    LastAction = SaveSimulationCheckpoint()
        ? TEXT("Journey and systems saved. O restores this checkpoint.")
        : TEXT("Ship and world saved, but a systems checkpoint failed; check the log.");
}

void ASPPlayLoopDirector::OnLoadJourney()
{
    FeedbackUntilSeconds = GetWorld()->GetTimeSeconds() + 5.0f;
    USPTravelNavigationComponent* Navigation = HyperjumpRoute ? HyperjumpRoute->GetNavigationComponent() : nullptr;
    FSPJourneyState State;
    if (!USPJourneySaveLibrary::LoadJourney(TEXT("SpacePatriot_Journey"), State) ||
        !LoadSimulationCheckpoint(State.WorldId, true))
    {
        LastAction = TEXT("No complete journey and systems checkpoint is available. Press K to save one.");
        return;
    }
    if (!USPJourneySaveLibrary::RestoreJourney(State, Player, GroundPawn, Ship, Surface, Navigation))
    {
        LastAction = TEXT("No valid saved journey is available.");
        return;
    }
    if (DayNightCycle) DayNightCycle->SetHour(State.TimeOfDayHours);
    bShipWasFlying = Ship->bFlying;
    if (!Ship->bFlying) UpdateRespawnAnchor();
    SetEarthPortVisible(State.WorldId == TEXT("earth"));
    if (WorldDressing) WorldDressing->ApplyWorld(State.WorldId, Surface, Ship);
    for (TActorIterator<ASPWorldRuntime> It(GetWorld()); It; ++It)
    {
        if (It->Systems) It->Systems->SetActiveWorld(State.WorldId);
    }
    const bool bSystemsLoaded = LoadSimulationCheckpoint(State.WorldId);
    if (!bSystemsLoaded) SetSurveySiteAtShip(State.WorldId);
    LastAction = bSystemsLoaded
        ? FString::Printf(TEXT("Journey and systems restored on %s."), *State.WorldId)
        : FString::Printf(TEXT("Journey restored on %s, but no complete systems checkpoint was found."), *State.WorldId);
}

void ASPPlayLoopDirector::OnSkipTime()
{
    if (!DayNightCycle) return;
    DayNightCycle->AdvanceHours(3.0f);
    LastAction = FString::Printf(TEXT("Local time %s. U advances three hours."),
        *DayNightCycle->GetClockText());
    FeedbackUntilSeconds = GetWorld()->GetTimeSeconds() + 4.0f;
}

void ASPPlayLoopDirector::RefreshStatus()
{
    if (!StatusWidget || !Surface || !HyperjumpRoute) return;
    const FSPHyperjumpRouteStatus Status = HyperjumpRoute->GetRouteStatus();
    const FString WorldName = Status.CurrentWorldName.IsEmpty() ? Surface->WorldId : Status.CurrentWorldName;
    FString Action;
    FString Detail;
    bool bHasSurveyStatus = false;
    if (IsPiloting())
    {
        if (Status.Navigation.Phase == ESPTravelPhase::JumpCharging)
        {
            Action = FString::Printf(TEXT("HYPERDRIVE CHARGING  %0.0f%%"), Status.ChargeFraction * 100.0f);
        }
        else if (Status.Navigation.Phase == ESPTravelPhase::JumpTransit)
        {
            Action = TEXT("HYPERJUMP IN PROGRESS");
        }
        else if (Ship->bFlying)
        {
            Action = FString::Printf(TEXT("N DESTINATION: %s  |  J JUMP"),
                Status.DestinationWorldName.IsEmpty() ? TEXT("NONE") : *Status.DestinationWorldName);
        }
        else
        {
            Action = TEXT("SPACE LAUNCH  |  E EXIT  |  N CHOOSE DESTINATION");
        }
        if (Ship->bFlying)
        {
            const double AltitudeKm = FMath::Max(0.0,
                (FVector::Distance(Ship->GetActorLocation(), Ship->PlanetCenterCm) - 1800000.0) / 100000.0);
            Detail = FString::Printf(TEXT("ALT %0.2f km / 2.00 km JUMP  |  G GEAR  |  L LAND  |  K SAVE"),
                AltitudeKm);
        }
        else Detail = TEXT("SPACE LAUNCH  |  E EXIT  |  K SAVE  |  O LOAD");
    }
    else if (GroundPawn && Ship)
    {
        const float Distance = FVector::Distance(GroundPawn->GetActorLocation(), GetBoardingLocation());
        Action = Distance <= BoardingRangeCm ? TEXT("E BOARD KESTREL")
            : FString::Printf(TEXT("APPROACH KESTREL HATCH  %0.0f m"), Distance / 100.0f);
        FSPFieldSurveyStatus SurveyStatus;
        bHasSurveyStatus = IsValid(ActiveSurvey) && ActiveSurvey->GetCurrentStatus(SurveyStatus);
        Detail = bHasSurveyStatus
            ? FString::Printf(TEXT("SURVEY: %s  |  SITE %0.0f m  |  K SAVE"),
                *SurveyStatus.Title, SurveyStatus.DistanceMeters)
            : TEXT("WASD MOVE  |  F PORT FREIGHT  |  B SCAN  |  K SAVE");
    }
    else
    {
        Action = TEXT("PREPARING PLAY AREA");
    }
    if (!LastAction.IsEmpty() && (Status.Navigation.Phase == ESPTravelPhase::JumpCharging ||
        Status.Navigation.Phase == ESPTravelPhase::JumpTransit ||
        GetWorld()->GetTimeSeconds() < FeedbackUntilSeconds || (!IsPiloting() && !bHasSurveyStatus)))
        Detail = LastAction;
    else if (IsPiloting() && !Status.Message.IsEmpty() && !Ship->bFlying) Detail = Status.Message;
    const FString Clock = DayNightCycle ? DayNightCycle->GetClockText() : TEXT("--:--");
    StatusWidget->SetStatus(FString::Printf(TEXT("SPACE PATRIOT  /  %s  /  %s"), *WorldName, *Clock),
        Action, Detail);
}
