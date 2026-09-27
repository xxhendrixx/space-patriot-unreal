#include "SPJourneySave.h"

#include "SPFlightPawn.h"
#include "SPWorldSurface.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    bool ValidSlot(const FString& Slot)
    {
        if (Slot.IsEmpty() || Slot.Len() > 64) return false;
        for (const TCHAR Character : Slot)
        {
            if (!((Character >= TEXT('a') && Character <= TEXT('z')) ||
                (Character >= TEXT('A') && Character <= TEXT('Z')) ||
                (Character >= TEXT('0') && Character <= TEXT('9')) ||
                Character == TEXT('_') || Character == TEXT('-'))) return false;
        }
        return true;
    }

    bool ValidTransform(const FTransform& Transform)
    {
        if (Transform.ContainsNaN() || !Transform.GetRotation().IsNormalized()) return false;
        const FVector Position = Transform.GetLocation();
        const FVector Scale = Transform.GetScale3D();
        return FMath::Abs(Position.X) < 1000000000.0 &&
            FMath::Abs(Position.Y) < 1000000000.0 &&
            FMath::Abs(Position.Z) < 1000000000.0 &&
            Scale.GetMin() > 0.001 && Scale.GetMax() < 100.0;
    }

    bool Percent(float Value)
    {
        return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 100.0f;
    }

    bool ValidWorldId(const FString& Id)
    {
        if (Id.IsEmpty() || Id.Len() > 64) return false;
        for (const TCHAR Character : Id)
        {
            if (!((Character >= TEXT('a') && Character <= TEXT('z')) ||
                (Character >= TEXT('0') && Character <= TEXT('9')) ||
                Character == TEXT('_') || Character == TEXT('-'))) return false;
        }
        return true;
    }
}

bool USPJourneySaveLibrary::IsJourneyStateValid(const FSPJourneyState& State)
{
    const FSPTravelNavigationState& Navigation = State.Navigation;
    if (!ValidWorldId(State.WorldId) || State.WorldId != Navigation.CurrentWorldId ||
        (!Navigation.DestinationWorldId.IsEmpty() &&
            (!ValidWorldId(Navigation.DestinationWorldId) || Navigation.DestinationWorldId == State.WorldId)) ||
        !ValidTransform(State.ShipTransform) || !ValidTransform(State.GroundPawnTransform) ||
        !Percent(State.FuelPercent) || !Percent(State.HeatPercent) || !Percent(State.HullPercent) ||
        !FMath::IsFinite(State.TimeOfDayHours) || State.TimeOfDayHours < 0.0f ||
        State.TimeOfDayHours >= 24.0f ||
        !FMath::IsFinite(State.ThrottleLimit) || State.ThrottleLimit < 0.05f || State.ThrottleLimit > 3.0f ||
        !FMath::IsNearlyEqual(Navigation.FuelPercent, State.FuelPercent, 0.01f) ||
        !FMath::IsNearlyEqual(Navigation.HeatNormalized, State.HeatPercent / 100.0f, 0.001f) ||
        !FMath::IsFinite(Navigation.CruiseSpool) || Navigation.CruiseSpool < 0.0f || Navigation.CruiseSpool > 1.0f ||
        !FMath::IsFinite(Navigation.JumpChargeSeconds) ||
        static_cast<uint8>(Navigation.Phase) > static_cast<uint8>(ESPTravelPhase::Docked) ||
        static_cast<uint8>(Navigation.DriveMode) > static_cast<uint8>(ESPTravelDriveMode::NAV)) return false;

    // An in-progress jump or landing animation needs a live route/physics
    // timeline that a stationary snapshot cannot reconstruct.
    if (Navigation.Phase != ESPTravelPhase::Flight &&
        Navigation.Phase != ESPTravelPhase::Landed &&
        Navigation.Phase != ESPTravelPhase::Docked) return false;
    const bool bAirborne = Navigation.Phase == ESPTravelPhase::Flight;
    if (State.bFlying != bAirborne || (!State.bPiloting && bAirborne) ||
        (!State.bFlying && !State.bGearDown) ||
        (!State.bFlying && State.bCruise) ||
        Navigation.JumpChargeSeconds != 0.0f ||
        (Navigation.Phase == ESPTravelPhase::Docked) != !Navigation.DockedStationId.IsEmpty() ||
        (!bAirborne && (Navigation.CruiseSpool != 0.0f || Navigation.bCruiseLatched)) ||
        (Navigation.DriveMode == ESPTravelDriveMode::SCM &&
            (Navigation.CruiseSpool > 0.02f || Navigation.bCruiseLatched))) return false;
    return true;
}

bool USPJourneySaveLibrary::CaptureJourney(const APlayerController* Player, const APawn* GroundPawn,
    const ASPFlightPawn* Ship, const USPTravelNavigationComponent* Navigation,
    FSPJourneyState& OutState)
{
    if (!IsValid(Player) || !IsValid(GroundPawn) || !IsValid(Ship) || !IsValid(Navigation) ||
        GroundPawn == Ship || Player->GetWorld() != Ship->GetWorld() ||
        GroundPawn->GetWorld() != Ship->GetWorld() || Navigation->GetWorld() != Ship->GetWorld() ||
        (Player->GetPawn() != GroundPawn && Player->GetPawn() != Ship)) return false;

    FSPJourneyState Candidate;
    Candidate.Navigation = Navigation->CaptureSaveState();
    Candidate.WorldId = Candidate.Navigation.CurrentWorldId;
    Candidate.ShipTransform = Ship->GetActorTransform();
    Candidate.GroundPawnTransform = GroundPawn->GetActorTransform();
    Candidate.bPiloting = Player->GetPawn() == Ship;
    Candidate.bFlying = Ship->bFlying;
    Candidate.bGearDown = Ship->bGearDown;
    Candidate.bPowered = Ship->bPowered;
    Candidate.bFlightAssist = Ship->bFlightAssist;
    Candidate.bCruise = Ship->bCruise;
    Candidate.FuelPercent = Ship->FuelPercent;
    Candidate.HeatPercent = Ship->HeatPercent;
    Candidate.HullPercent = Ship->HullPercent;
    Candidate.ThrottleLimit = Ship->ThrottleLimit;
    Candidate.Navigation.FuelPercent = Candidate.FuelPercent;
    Candidate.Navigation.HeatNormalized = Candidate.HeatPercent / 100.0f;
    if (!IsJourneyStateValid(Candidate)) return false;
    OutState = MoveTemp(Candidate);
    return true;
}

bool USPJourneySaveLibrary::SaveJourney(const FSPJourneyState& State, const FString& SlotName)
{
    if (!ValidSlot(SlotName) || !IsJourneyStateValid(State)) return false;
    USPSJourneySaveGame* Save = NewObject<USPSJourneySaveGame>();
    if (!Save) return false;
    Save->Journey = State;
    return UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

bool USPJourneySaveLibrary::LoadJourney(const FString& SlotName, FSPJourneyState& OutState)
{
    if (!ValidSlot(SlotName) || !UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
    const USPSJourneySaveGame* Save = Cast<USPSJourneySaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!Save || Save->Version != 1 || !IsJourneyStateValid(Save->Journey)) return false;
    OutState = Save->Journey;
    return true;
}

bool USPJourneySaveLibrary::RestoreJourney(const FSPJourneyState& State, APlayerController* Player,
    APawn* GroundPawn, ASPFlightPawn* Ship, ASPWorldSurface* Surface,
    USPTravelNavigationComponent* Navigation)
{
    if (!IsJourneyStateValid(State) || !IsValid(Player) || !IsValid(GroundPawn) ||
        !IsValid(Ship) || !IsValid(Surface) || !IsValid(Navigation) || GroundPawn == Ship ||
        Player->GetWorld() != Ship->GetWorld() || GroundPawn->GetWorld() != Ship->GetWorld() ||
        Surface->GetWorld() != Ship->GetWorld() || Navigation->GetWorld() != Ship->GetWorld() ||
        (Player->GetPawn() != GroundPawn && Player->GetPawn() != Ship)) return false;

    if (Navigation->GetWorlds().IsEmpty() && !Navigation->LoadWorldCatalog()) return false;
    FSPTravelWorld World;
    if (!Navigation->FindWorld(State.WorldId, World) ||
        (!State.Navigation.DestinationWorldId.IsEmpty() &&
            !Navigation->FindWorld(State.Navigation.DestinationWorldId, World))) return false;
    if (!Navigation->FindWorld(State.WorldId, World) ||
        (World.Biome == TEXT("gas") && !State.bFlying)) return false;

    const FString PreviousWorld = Surface->WorldId;
    const FSPTravelNavigationState PreviousNavigation = Navigation->CaptureSaveState();
    const FTransform PreviousShip = Ship->GetActorTransform();
    const FTransform PreviousGround = GroundPawn->GetActorTransform();
    APawn* PreviousPawn = Player->GetPawn();
    const bool bChangedWorld = PreviousWorld != State.WorldId;
    if (bChangedWorld && !Surface->ActivateWorld(State.WorldId)) return false;
    if (!Navigation->RestoreSaveState(State.Navigation))
    {
        if (bChangedWorld) Surface->ActivateWorld(PreviousWorld);
        return false;
    }

    const bool bPlaced = Ship->SetActorTransform(State.ShipTransform, false, nullptr,
        ETeleportType::TeleportPhysics) && GroundPawn->SetActorTransform(State.GroundPawnTransform,
        false, nullptr, ETeleportType::TeleportPhysics);
    if (bPlaced) Player->Possess(State.bPiloting ? static_cast<APawn*>(Ship) : GroundPawn);
    if (!bPlaced || Player->GetPawn() != (State.bPiloting ? static_cast<APawn*>(Ship) : GroundPawn))
    {
        Ship->SetActorTransform(PreviousShip, false, nullptr, ETeleportType::TeleportPhysics);
        GroundPawn->SetActorTransform(PreviousGround, false, nullptr, ETeleportType::TeleportPhysics);
        Navigation->RestoreSaveState(PreviousNavigation);
        if (bChangedWorld) Surface->ActivateWorld(PreviousWorld);
        if (PreviousPawn && Player->GetPawn() != PreviousPawn) Player->Possess(PreviousPawn);
        return false;
    }

    Ship->ResetMotionAfterWarp();
    Ship->PlanetCenterCm = Surface->GetPlanetCenterWorld();
    Ship->bUseRadialSurface = true;
    Ship->bFlying = State.bFlying;
    Ship->bPowered = State.bPowered;
    Ship->bCruise = State.bCruise;
    Ship->FuelPercent = State.FuelPercent;
    Ship->HeatPercent = State.HeatPercent;
    Ship->HullPercent = State.HullPercent;
    Ship->SetThrottleLimit(State.ThrottleLimit);
    Ship->SetFlightAssist(State.bFlightAssist);
    Ship->SetGearDown(State.bGearDown);
    GroundPawn->SetActorHiddenInGame(State.bPiloting);
    GroundPawn->SetActorEnableCollision(!State.bPiloting);
    if (ACharacter* Character = Cast<ACharacter>(GroundPawn))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            if (State.bPiloting) Movement->DisableMovement();
            else Movement->SetMovementMode(MOVE_Walking);
        }
    }
    Surface->FocusActor = State.bPiloting ? static_cast<AActor*>(Ship) : GroundPawn;
    Ship->RefreshMFD();
    return true;
}
