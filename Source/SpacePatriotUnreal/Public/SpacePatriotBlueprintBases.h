#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "SpacePatriotSystemsComponent.h"
#include "SpacePatriotBlueprintBases.generated.h"

UENUM(BlueprintType)
enum class ESPCitizenActivity : uint8
{
    Work, Trade, Rest, Travel, Socialize, Investigate, Evade
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSPCockpitChangedSignature);

UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPWorldRuntime : public AActor
{
    GENERATED_BODY()
public:
    ASPWorldRuntime();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot") TObjectPtr<USpacePatriotSystemsComponent> Systems;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation", meta=(ClampMin="0")) float WorldHoursPerRealSecond = 0.1f;
protected:
    virtual void Tick(float DeltaSeconds) override;
};

UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPPlayerShipPawn : public APawn
{
    GENERATED_BODY()
public:
    ASPPlayerShipPawn();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship") TObjectPtr<USceneComponent> ShipRoot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ship|Flight") float Throttle = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ship|Flight") float ForwardSpeed = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ship|Flight") float Acceleration = 2200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ship|Cargo") float HullMass = 18000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ship|Cargo") int32 Organics = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ship|Cargo") int32 Ore = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ship|Cargo") int32 Crystal = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ship|Cargo") float LoadedMass = 18000.0f;
    UFUNCTION(BlueprintCallable, Category="Ship|Flight") void SetFlightThrottle(float Value);
    UFUNCTION(BlueprintCallable, Category="Ship|Cargo") void RefreshLoadedMass();
    UFUNCTION(BlueprintCallable, Category="Ship|Cargo") bool TransferCargo(int32 OrganicsDelta, int32 OreDelta, int32 CrystalDelta);
};

UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPWildlifeEncounter : public AActor
{
    GENERATED_BODY()
public:
    ASPWildlifeEncounter();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife") FString CreatureId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife") float MaxHealth = 100.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wildlife") float Health = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife") bool bBoss = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife|Combat") float Aggression = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife|Combat") float AttackDamage = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife|Combat") FString Ability;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife|Combat", meta=(ClampMin="0")) float DetectionRangeCm = 1800.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife|Combat", meta=(ClampMin="0")) float AttackRangeCm = 260.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife|Combat", meta=(ClampMin="0")) float ChaseSpeedCmPerSecond = 260.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife|Combat", meta=(ClampMin="0")) float AttackCooldownSeconds = 1.4f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wildlife|Combat") bool bDefeated = false;
    UPROPERTY(BlueprintAssignable, Category="Wildlife") FSPWildlifeHealthSignature OnHealthChanged;
    /** Make the Shooter template's Projectile object channel hit this actor. */
    bool ConfigureProjectileHitbox();
    UFUNCTION(BlueprintCallable, Category="Wildlife|Combat") bool ReceiveWeaponHit(float Damage);
    UFUNCTION(BlueprintCallable, Category="Wildlife|Combat") void ResetEncounter();
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator, AActor* DamageCauser) override;

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    float AttackCooldownRemaining = 0.0f;
    FVector SpawnLocation = FVector::ZeroVector;
    bool bSpawnLocationRecorded = false;
    float SpawnGroundClearanceCm = 0.0f;
    float DefeatCleanupRemaining = 0.0f;
};

UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPCitizenAgent : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Society") FString CharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Society") FString Job = TEXT("merchant");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Society") FString HomeWorld = TEXT("earth");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Society") int32 DecisionSeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Society", meta=(ClampMin="0", ClampMax="100")) float Hunger = 15.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Society", meta=(ClampMin="0", ClampMax="100")) float Fatigue = 10.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Society") ESPCitizenActivity CurrentActivity = ESPCitizenActivity::Work;
    UFUNCTION(BlueprintCallable, Category="Society|Daily Life") ESPCitizenActivity RollDailyActivity(int64 Day, bool bThreatNearby);
};

UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPCockpitMFD : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cockpit") int32 ActivePage = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cockpit") int32 PageCount = 6;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cockpit") float Brightness = 1.0f;
    UPROPERTY(BlueprintAssignable, Category="Cockpit") FSPCockpitChangedSignature OnCockpitControlChanged;
    UFUNCTION(BlueprintCallable, Category="Cockpit|MFD") void NextPage(int32 Direction);
    UFUNCTION(BlueprintCallable, Category="Cockpit|Controls") void AdjustBrightness(float Delta);
};
