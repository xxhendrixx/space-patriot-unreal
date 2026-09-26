#include "SpacePatriotBlueprintBases.h"

ASPWorldRuntime::ASPWorldRuntime()
{
    PrimaryActorTick.bCanEverTick = true;
    Systems = CreateDefaultSubobject<USpacePatriotSystemsComponent>(TEXT("SpacePatriotSystems"));
}

void ASPWorldRuntime::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Systems) Systems->AdvanceUniverse(DeltaSeconds * WorldHoursPerRealSecond);
}

ASPPlayerShipPawn::ASPPlayerShipPawn()
{
    ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
    SetRootComponent(ShipRoot);
    AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ASPPlayerShipPawn::SetFlightThrottle(float Value)
{
    Throttle = FMath::Clamp(Value, -1.0f, 1.0f);
    if (GetWorld()) ForwardSpeed = FMath::Clamp(ForwardSpeed + Throttle * Acceleration * GetWorld()->GetDeltaSeconds(), -12000.0f, 12000.0f);
}

void ASPPlayerShipPawn::RefreshLoadedMass()
{
    LoadedMass = HullMass + FMath::Max(0, Organics) * 0.5f + FMath::Max(0, Ore) * 1.5f + FMath::Max(0, Crystal) * 0.8f;
}

bool ASPPlayerShipPawn::TransferCargo(int32 OrganicsDelta, int32 OreDelta, int32 CrystalDelta)
{
    if (Organics + OrganicsDelta < 0 || Ore + OreDelta < 0 || Crystal + CrystalDelta < 0) return false;
    Organics += OrganicsDelta; Ore += OreDelta; Crystal += CrystalDelta; RefreshLoadedMass(); return true;
}

bool ASPWildlifeEncounter::ReceiveWeaponHit(float Damage)
{
    if (Damage <= 0.0f || Health <= 0.0f) return false;
    Health = FMath::Max(0.0f, Health - Damage);
    OnHealthChanged.Broadcast(CreatureId, Health);
    return true;
}

void ASPWildlifeEncounter::ResetEncounter() { Health = MaxHealth; }

ESPCitizenActivity ASPCitizenAgent::RollDailyActivity(int64 Day, bool bThreatNearby)
{
    FRandomStream Roll(DecisionSeed ^ GetTypeHash(CharacterId) ^ static_cast<int32>(Day * 104729));
    if (bThreatNearby && Roll.FRand() < 0.65f) CurrentActivity = ESPCitizenActivity::Evade;
    else if (Fatigue > 75.0f && Roll.FRand() < 0.75f) CurrentActivity = ESPCitizenActivity::Rest;
    else if (Hunger > 60.0f && Roll.FRand() < 0.55f) CurrentActivity = ESPCitizenActivity::Trade;
    else CurrentActivity = static_cast<ESPCitizenActivity>(Roll.RandRange(0, 5));
    return CurrentActivity;
}

void ASPCockpitMFD::NextPage(int32 Direction)
{
    PageCount = FMath::Max(PageCount, 1);
    ActivePage = (ActivePage + Direction + PageCount) % PageCount;
    OnCockpitControlChanged.Broadcast();
}

void ASPCockpitMFD::AdjustBrightness(float Delta)
{
    Brightness = FMath::Clamp(Brightness + Delta, 0.15f, 2.0f);
    OnCockpitControlChanged.Broadcast();
}
