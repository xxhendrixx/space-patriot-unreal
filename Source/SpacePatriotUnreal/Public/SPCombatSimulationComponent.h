#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPCombatSimulationComponent.generated.h"

/** Source combat.js uses kilometres; this API uses metres and seconds. Convert UE centimetres at the actor boundary. */
UENUM(BlueprintType)
enum class ESPCombatWeapon : uint8
{
    Kinetic,
    Laser,
    Missile,
    Rifle,
    Sidearm
};

UENUM(BlueprintType)
enum class ESPCombatEventType : uint8
{
    Fired,
    Impact,
    Destroyed,
    PlayerDisabled,
    Reloaded,
    SectorClear
};

USTRUCT(BlueprintType)
struct FSPCombatWeaponSpec
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) ESPCombatWeapon Weapon = ESPCombatWeapon::Kinetic;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) float SpeedMetersPerSecond = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Damage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float IntervalSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 MagazineCapacity = 0;
    UPROPERTY(BlueprintReadOnly) int32 ReserveCapacity = 0;
    UPROPERTY(BlueprintReadOnly) float ReloadSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) float RangeMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Heat01 = 0.0f;
};

USTRUCT(BlueprintType)
struct FSPCombatAmmo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPCombatWeapon Weapon = ESPCombatWeapon::Kinetic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Magazine = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Reserve = 0;
};

/** A contact is a compact authoritative simulation record, not a rendered actor. */
USTRUCT(BlueprintType)
struct FSPCombatContact
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector PositionMeters = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector AnchorMeters = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector VelocityMetersPerSecond = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Up = FVector::UpVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Forward = -FVector::ForwardVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Hull = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Shield = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RadiusMeters = 11.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Phase = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FireInSeconds = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Thrust01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHostile = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCivilian = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAIShip = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAISentry = false;
};

USTRUCT(BlueprintType)
struct FSPCombatProjectile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 Id = 0;
    UPROPERTY(BlueprintReadOnly) ESPCombatWeapon Weapon = ESPCombatWeapon::Kinetic;
    UPROPERTY(BlueprintReadOnly) FString OwnerId;
    UPROPERTY(BlueprintReadOnly) FString TargetId;
    UPROPERTY(BlueprintReadOnly) FVector PositionMeters = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector PreviousMeters = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector Direction = FVector::ForwardVector;
    UPROPERTY(BlueprintReadOnly) FVector VelocityMetersPerSecond = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float Damage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float TimeToLiveSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AgeSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct FSPCombatEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) ESPCombatEventType Type = ESPCombatEventType::Fired;
    UPROPERTY(BlueprintReadOnly) ESPCombatWeapon Weapon = ESPCombatWeapon::Kinetic;
    UPROPERTY(BlueprintReadOnly) FString SourceId;
    UPROPERTY(BlueprintReadOnly) FString TargetId;
    UPROPERTY(BlueprintReadOnly) FVector PositionMeters = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float Damage = 0.0f;
};

USTRUCT(BlueprintType)
struct FSPCombatStepInput
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector PlayerPositionMeters = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector AimDirection = FVector::ForwardVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector PlayerRight = FVector::RightVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector PlayerUp = FVector::UpVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector PlayerVelocityMetersPerSecond = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MuzzleOffsetMeters = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeaponFactor = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CoolerFactor = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ShieldFactor = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShipPowerOn = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSCMMode = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInstrumentsOpen = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOnFoot = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTurretSeat = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTrigger = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAuthority = true;
};

USTRUCT(BlueprintType)
struct FSPCombatTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) ESPCombatWeapon ActiveWeapon = ESPCombatWeapon::Kinetic;
    UPROPERTY(BlueprintReadOnly) FSPCombatAmmo Ammo;
    UPROPERTY(BlueprintReadOnly) FString TargetId;
    UPROPERTY(BlueprintReadOnly) FString Status;
    UPROPERTY(BlueprintReadOnly) float Lock01 = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Heat01 = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Capacitor = 100.0f;
    UPROPERTY(BlueprintReadOnly) float Shield = 100.0f;
    UPROPERTY(BlueprintReadOnly) float Hull = 100.0f;
    UPROPERTY(BlueprintReadOnly) float Suit = 100.0f;
    UPROPERTY(BlueprintReadOnly) float ReloadSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) bool bArmed = false;
    UPROPERTY(BlueprintReadOnly) bool bOverheated = false;
    UPROPERTY(BlueprintReadOnly) bool bDisabled = false;
    UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
    UPROPERTY(BlueprintReadOnly) int32 ActiveProjectiles = 0;
};

USTRUCT(BlueprintType)
struct FSPCombatFiringSolution
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FVector PointMeters = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float TimeSeconds = -1.0f;
    UPROPERTY(BlueprintReadOnly) float DistanceMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ClosingMetersPerSecond = 0.0f;
    UPROPERTY(BlueprintReadOnly) bool bReachable = false;
};

USTRUCT(BlueprintType)
struct FSPCombatSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSPCombatAmmo> Ammo;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSPCombatContact> Contacts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSPCombatProjectile> Projectiles;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPCombatWeapon ShipWeapon = ESPCombatWeapon::Kinetic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPCombatWeapon GroundWeapon = ESPCombatWeapon::Rifle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPCombatWeapon ReloadWeapon = ESPCombatWeapon::Kinetic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TimeSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CooldownSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ReloadSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Heat01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Capacitor = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Shield = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Hull = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Suit = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LastDamageSeconds = -100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Lock01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Kills = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Serial = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bArmed = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOverheated = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDisabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSectorCleared = false;
};

/** Deterministic, actor-independent source combat core. Call StepCombat from an authoritative game system. */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPCombatSimulationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USPCombatSimulationComponent();

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") void ResetCombat();
    UFUNCTION(BlueprintPure, Category="Space Patriot|Combat") static FSPCombatWeaponSpec GetWeaponSpec(ESPCombatWeapon Weapon);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") bool SelectWeapon(ESPCombatWeapon Weapon);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") bool SetArmed(bool bActive, const FSPCombatStepInput& Input);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") void SetTargetId(const FString& NewTargetId);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") FString CycleTarget(const FSPCombatStepInput& Input);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") void SetContacts(const TArray<FSPCombatContact>& NewContacts);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") bool AddContact(const FSPCombatContact& Contact);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") bool StartSortie(bool bGround, const FSPCombatStepInput& Input);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") bool RequestReload(const FSPCombatStepInput& Input);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") bool TryFire(const FSPCombatStepInput& Input);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") void StepCombat(float DeltaSeconds, const FSPCombatStepInput& Input);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") void ApplyPlayerDamage(float Amount, bool bOnFoot);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") bool Service(bool bLandedOrDocked, bool bWalking);
    UFUNCTION(BlueprintPure, Category="Space Patriot|Combat") FSPCombatTelemetry GetTelemetry(const FSPCombatStepInput& Input) const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Combat") FSPCombatFiringSolution GetFiringSolution(const FSPCombatStepInput& Input) const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Combat") TArray<FSPCombatContact> GetContacts() const { return Contacts; }
    UFUNCTION(BlueprintPure, Category="Space Patriot|Combat") TArray<FSPCombatProjectile> GetProjectiles() const { return Projectiles; }
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat") TArray<FSPCombatEvent> DrainEvents();
    UFUNCTION(BlueprintPure, Category="Space Patriot|Combat|Save") FSPCombatSnapshot CaptureState() const;
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Combat|Save") bool RestoreState(const FSPCombatSnapshot& Snapshot);

private:
    UPROPERTY(SaveGame) TArray<FSPCombatAmmo> Ammo;
    UPROPERTY(SaveGame) TArray<FSPCombatContact> Contacts;
    UPROPERTY(SaveGame) TArray<FSPCombatProjectile> Projectiles;
    UPROPERTY(SaveGame) ESPCombatWeapon ShipWeapon = ESPCombatWeapon::Kinetic;
    UPROPERTY(SaveGame) ESPCombatWeapon GroundWeapon = ESPCombatWeapon::Rifle;
    UPROPERTY(SaveGame) ESPCombatWeapon ReloadWeapon = ESPCombatWeapon::Kinetic;
    UPROPERTY(SaveGame) FString TargetId;
    UPROPERTY(SaveGame) float TimeSeconds = 0.0f;
    UPROPERTY(SaveGame) float CooldownSeconds = 0.0f;
    UPROPERTY(SaveGame) float ReloadSeconds = 0.0f;
    UPROPERTY(SaveGame) float Heat01 = 0.0f;
    UPROPERTY(SaveGame) float Capacitor = 100.0f;
    UPROPERTY(SaveGame) float Shield = 100.0f;
    UPROPERTY(SaveGame) float Hull = 100.0f;
    UPROPERTY(SaveGame) float Suit = 100.0f;
    UPROPERTY(SaveGame) float LastDamageSeconds = -100.0f;
    UPROPERTY(SaveGame) float Lock01 = 0.0f;
    UPROPERTY(SaveGame) int32 Kills = 0;
    UPROPERTY(SaveGame) int32 Serial = 0;
    UPROPERTY(SaveGame) bool bArmed = false;
    UPROPERTY(SaveGame) bool bOverheated = false;
    UPROPERTY(SaveGame) bool bDisabled = false;
    UPROPERTY(SaveGame) bool bSectorCleared = false;
    TArray<FSPCombatEvent> PendingEvents;

    ESPCombatWeapon ActiveWeapon(const FSPCombatStepInput& Input) const;
    FSPCombatAmmo* FindAmmo(ESPCombatWeapon Weapon);
    const FSPCombatAmmo* FindAmmo(ESPCombatWeapon Weapon) const;
    FSPCombatContact* FindContact(const FString& Id);
    const FSPCombatContact* FindContact(const FString& Id) const;
    void SpawnProjectile(ESPCombatWeapon Weapon, const FString& OwnerId, const FString& ShotTargetId, const FVector& Origin, const FVector& Direction, const FVector& InheritedVelocity);
    void AdvanceEnemies(float DeltaSeconds, const FSPCombatStepInput& Input);
    void AdvanceProjectiles(float DeltaSeconds, const FSPCombatStepInput& Input);
    void ApplyContactDamage(FSPCombatContact& Contact, float Amount, ESPCombatWeapon Weapon, const FVector& Position, const FString& OwnerId);
    void AddEvent(ESPCombatEventType Type, ESPCombatWeapon Weapon, const FString& SourceId, const FString& HitTargetId, const FVector& Position, float Damage = 0.0f);
    static float HeatSignature(const FSPCombatContact& Contact);
    static bool SegmentSphereHit(const FVector& Start, const FVector& End, const FVector& Center, float Radius, double& OutT);
};
