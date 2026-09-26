#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpacePatriotSystemsComponent.generated.h"

USTRUCT(BlueprintType)
struct FSPWorldRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString System;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Biome;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Temperature = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Radius = 1.0f;
};

USTRUCT(BlueprintType)
struct FSPCreatureRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString WorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Role;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ModelId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Ability;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Health = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 8.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Aggression = 0.0f;
};

USTRUCT(BlueprintType)
struct FSPUniverseEvent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString EventId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString WorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Kind;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Summary;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 Day = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPUniverseEventSignature, const FSPUniverseEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPWildlifeHealthSignature, const FString&, CreatureId, float, RemainingHealth);

/**
 * Blueprint-facing system hub. BP child components and actors are generated in Content/SpacePatriot/Blueprints.
 * The event-driven state transitions live here; BP classes own presentation, level hooks, and project-specific tuning.
 */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USpacePatriotSystemsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USpacePatriotSystemsComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Universe") FString ActiveWorldId = TEXT("earth");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Universe") int64 SimulationDay = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Universe") TArray<FSPWorldRecord> Worlds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wildlife") TArray<FSPCreatureRecord> CreatureCatalog;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Society") TArray<FSPUniverseEvent> RecentEvents;
    UPROPERTY(BlueprintAssignable, Category="Events") FSPUniverseEventSignature OnUniverseEvent;
    UPROPERTY(BlueprintAssignable, Category="Events") FSPWildlifeHealthSignature OnWildlifeHealthChanged;

    UFUNCTION(BlueprintCallable, Category="Universe|Data") bool LoadSourceCatalogs();
    UFUNCTION(BlueprintCallable, Category="Universe|Worlds") bool SetActiveWorld(const FString& WorldId);
    UFUNCTION(BlueprintPure, Category="Universe|Worlds") FSPWorldRecord GetActiveWorld() const;
    UFUNCTION(BlueprintPure, Category="Wildlife") TArray<FSPCreatureRecord> GetWildlifeForWorld(const FString& WorldId) const;
    UFUNCTION(BlueprintCallable, Category="Society|Simulation") void AdvanceUniverse(float ElapsedHours);
    UFUNCTION(BlueprintCallable, Category="Wildlife|Combat") bool ApplyWildlifeDamage(const FString& CreatureId, float Damage);
    UFUNCTION(BlueprintCallable, Category="Society|Story") FSPUniverseEvent RollStoryEvent(const FString& WorldId, const FString& CharacterId, int32 Day);
    UFUNCTION(BlueprintPure, Category="Cargo") float CalculateLoadedMass(float HullMass, int32 Organics, int32 Ore, int32 Crystal) const;

protected:
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wildlife") TMap<FString, float> RuntimeWildlifeHealth;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Society") TMap<FString, int32> CharacterSeeds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Society") float AccumulatedHours = 0.0f;
};
