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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife") FString CreatureId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife") float MaxHealth = 100.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wildlife") float Health = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wildlife") bool bBoss = false;
    UPROPERTY(BlueprintAssignable, Category="Wildlife") FSPWildlifeHealthSignature OnHealthChanged;
    UFUNCTION(BlueprintCallable, Category="Wildlife|Combat") bool ReceiveWeaponHit(float Damage);
    UFUNCTION(BlueprintCallable, Category="Wildlife|Combat") void ResetEncounter();
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
