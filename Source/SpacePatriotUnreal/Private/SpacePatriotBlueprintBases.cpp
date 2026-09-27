#include "SpacePatriotBlueprintBases.h"

#include "SPWorldSurface.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

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

ASPWildlifeEncounter::ASPWildlifeEncounter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
}

bool ASPWildlifeEncounter::ConfigureProjectileHitbox()
{
    UPrimitiveComponent* Hitbox = Cast<UPrimitiveComponent>(GetRootComponent());
    if (!Hitbox) return false;
    // The installed Shooter projectile has the "Projectile" object type,
    // configured as ECC_GameTraceChannel1 in DefaultEngine.ini. Keep pawn and
    // world collision unchanged; only shots should block against the proxy.
    Hitbox->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    return true;
}

bool ASPWildlifeEncounter::ReceiveWeaponHit(float Damage)
{
    if (Damage <= 0.0f || Health <= 0.0f) return false;
    Health = FMath::Max(0.0f, Health - Damage);
    OnHealthChanged.Broadcast(CreatureId, Health);
    if (Health <= 0.0f)
    {
        bDefeated = true;
        DefeatCleanupRemaining = 2.0f;
        SetActorEnableCollision(false);
    }
    return true;
}

void ASPWildlifeEncounter::ResetEncounter()
{
    Health = FMath::Max(1.0f, MaxHealth);
    bDefeated = false;
    AttackCooldownRemaining = 0.0f;
    DefeatCleanupRemaining = 0.0f;
    bSpawnLocationRecorded = false;
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);
}

float ASPWildlifeEncounter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f || Health <= 0.0f) return 0.0f;
    const float Applied = FMath::Min(DamageAmount, Health);
    // Shooter's BP_ShooterProjectileBase invokes ApplyDamage, which reaches
    // AActor::TakeDamage. Preserve ordinary damage delegates for Blueprint
    // effects, then update the creature's actual health.
    Super::TakeDamage(Applied, DamageEvent, EventInstigator, DamageCauser);
    ReceiveWeaponHit(Applied);
    return Applied;
}

void ASPWildlifeEncounter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bDefeated)
    {
        DefeatCleanupRemaining -= DeltaSeconds;
        if (DefeatCleanupRemaining <= 0.0f)
        {
            SetActorHiddenInGame(true);
            SetActorTickEnabled(false);
        }
        return;
    }
    if (Aggression < 0.5f || AttackDamage <= 0.0f || !GetWorld()) return;

    AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaSeconds);
    ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!IsValid(Player)) return; // The player is aboard the ship or absent.
    const FVector Position = GetActorLocation();
    const double Distance = FVector::Distance(Position, Player->GetActorLocation());
    if (Distance > DetectionRangeCm) return;

    ASPWorldSurface* Surface = nullptr;
    for (TActorIterator<ASPWorldSurface> It(GetWorld()); It; ++It)
    {
        Surface = *It;
        break;
    }
    if (!Surface) return;
    const FVector Center = Surface->GetPlanetCenterWorld();
    const FVector Up = (Position - Center).GetSafeNormal();
    const double NominalRadiusCm = FVector::Distance(Surface->GetActorLocation(), Center) - 300.0;
    if (!bSpawnLocationRecorded)
    {
        SpawnLocation = Position;
        const FSPWorldSurfaceSample Ground = Surface->SampleAtWorldLocation(
            Center + Up * NominalRadiusCm);
        SpawnGroundClearanceCm = static_cast<float>(FVector::Distance(Position, Center) -
            NominalRadiusCm - Ground.ElevationMeters * 100.0 - 3.5);
        bSpawnLocationRecorded = true;
    }

    if (Distance > AttackRangeCm)
    {
        const FVector TowardPlayer = FVector::VectorPlaneProject(
            Player->GetActorLocation() - Position, Up).GetSafeNormal();
        if (!TowardPlayer.IsNearlyZero())
        {
            const FVector Proposed = Position + TowardPlayer *
                FMath::Min(static_cast<double>(ChaseSpeedCmPerSecond * DeltaSeconds), Distance - AttackRangeCm * 0.8);
            if (FVector::Distance(Proposed, SpawnLocation) <= DetectionRangeCm)
            {
                FCollisionQueryParams Query(SCENE_QUERY_STAT(SPWildlifeChase), false, this);
                const FVector EyeOffset = Up * 80.0;
                FHitResult Obstacle;
                if (!GetWorld()->LineTraceSingleByChannel(Obstacle, Position + EyeOffset,
                    Proposed + EyeOffset, ECC_Visibility, Query))
                {
                    const FVector NextUp = (Proposed - Center).GetSafeNormal();
                    const FSPWorldSurfaceSample NextGround = Surface->SampleAtWorldLocation(
                        Center + NextUp * NominalRadiusCm);
                    SetActorLocation(Center + NextUp * (NominalRadiusCm +
                        NextGround.ElevationMeters * 100.0 + 3.5 + SpawnGroundClearanceCm));
                }
            }
        }
        return;
    }

    if (AttackCooldownRemaining > 0.0f) return;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SPWildlifeAttack), false, this);
    FHitResult Obstacle;
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Obstacle,
        Position + Up * 80.0, Player->GetActorLocation() + Up * 80.0,
        ECC_Visibility, Query) && Obstacle.GetActor() != Player;
    if (bBlocked) return;
    UGameplayStatics::ApplyDamage(Player, AttackDamage, nullptr, this, nullptr);
    AttackCooldownRemaining = FMath::Max(0.1f, AttackCooldownSeconds);
}

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
