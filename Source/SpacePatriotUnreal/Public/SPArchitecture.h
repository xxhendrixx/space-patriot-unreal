#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPArchitecture.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;

/** Dimensions and IDs come from the original DeckPlans.json, in metres. */
USTRUCT(BlueprintType)
struct FSPArchitectureRoom
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Architecture") FString Id;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") FString Name;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") int32 Deck = 0;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") float FloorMeters = 0;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") FVector2D MinMeters = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") FVector2D MaxMeters = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") bool bCorridor = false;
};

USTRUCT(BlueprintType)
struct FSPArchitectureDoor
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Architecture") FString Id;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") FString Name;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") int32 Deck = 0;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") float FloorMeters = 0;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") FVector2D CenterMeters = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") float WidthMeters = 1.0f;
    /** Source axis x means the wall lies at fixed x; z means fixed z. */
    UPROPERTY(BlueprintReadOnly, Category="Architecture") bool bFixedX = true;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") bool bOpen = true;
};

USTRUCT(BlueprintType)
struct FSPArchitectureDeck
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Architecture") int32 Index = 0;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") FString Name;
    UPROPERTY(BlueprintReadOnly, Category="Architecture") float FloorMeters = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPArchitectureDoorChanged, const FString&, DoorId, bool, bOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPArchitectureLiftArrived, int32, DeckIndex, const FString&, DeckName);

/**
 * Native, bounded ArchitectureWorks/DeckWalk slice for ship interiors.
 * Source coordinates are x / y-up / z-aft metres; local Unreal coordinates
 * are +X across / +Z up / -Y aft centimetres. The actor may be attached to
 * any ship root without changing source room and door IDs.
 */
UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPArchitecture : public AActor
{
    GENERATED_BODY()

public:
    ASPArchitecture();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Geometry") TObjectPtr<USceneComponent> ArchitectureRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Geometry") TObjectPtr<UInstancedStaticMeshComponent> Floors;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Geometry") TObjectPtr<UInstancedStaticMeshComponent> Ceilings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Geometry") TObjectPtr<UInstancedStaticMeshComponent> Bulkheads;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Geometry") TObjectPtr<UInstancedStaticMeshComponent> Fixtures;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Geometry") TObjectPtr<UInstancedStaticMeshComponent> DecorativeFixtures;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Geometry") TObjectPtr<UStaticMeshComponent> LiftPlatform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture|Source", meta=(ClampMin="0", ClampMax="9")) int32 InteriorFamily = 9;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture|Source") bool bLoadOnBeginPlay = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture|Materials") TObjectPtr<UMaterialInterface> FloorMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture|Materials") TObjectPtr<UMaterialInterface> BulkheadMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture|Materials") TObjectPtr<UMaterialInterface> FixtureMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture|Materials") TObjectPtr<UMaterialInterface> DoorMaterial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Source") TArray<FSPArchitectureDeck> Decks;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Source") TArray<FSPArchitectureRoom> Rooms;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Source") TArray<FSPArchitectureDoor> Doors;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Diagnostics") int32 FixtureCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Diagnostics") int32 StructureInstances = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Diagnostics") FString LastBuildError;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture|Lift") bool bLiftPowered = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture|Lift", meta=(ClampMin="0.1", ClampMax="8")) float LiftSpeedMetersPerSecond = 1.65f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Lift") int32 LiftDeck = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Lift") int32 LiftTargetDeck = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Architecture|Lift") bool bLiftMoving = false;
    UPROPERTY(BlueprintAssignable, Category="Architecture|Events") FSPArchitectureDoorChanged OnDoorChanged;
    UPROPERTY(BlueprintAssignable, Category="Architecture|Events") FSPArchitectureLiftArrived OnLiftArrived;

    /** Loads the preserved Unity DeckPlans source and rebuilds collision geometry. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Architecture") bool RebuildFromSource();
    UFUNCTION(BlueprintCallable, Category="Architecture") bool LoadInteriorFamily(int32 Family);
    UFUNCTION(BlueprintCallable, Category="Architecture") bool SetDoorOpen(const FString& DoorId, bool bOpen);
    UFUNCTION(BlueprintPure, Category="Architecture") bool IsDoorOpen(const FString& DoorId) const;
    UFUNCTION(BlueprintCallable, Category="Architecture") bool RequestLiftToDeck(int32 DeckIndex);
    UFUNCTION(BlueprintCallable, Category="Architecture") void SetLiftPower(bool bPowered);
    /** Same movement step used by Tick; exposed for deterministic simulation/tests. */
    UFUNCTION(BlueprintCallable, Category="Architecture") void AdvanceLift(float DeltaSeconds);
    UFUNCTION(BlueprintPure, Category="Architecture") FSPArchitectureRoom RoomAtLocalLocation(FVector LocalCentimetres, bool& bFound) const;
    UFUNCTION(BlueprintPure, Category="Architecture") bool CanOccupyLocalLocation(FVector LocalCentimetres, float RadiusCentimetres = 25.0f) const;
    /** Returns source room IDs in travel order, honouring closed doors and lift power. */
    UFUNCTION(BlueprintCallable, Category="Architecture") bool FindRoomRoute(const FString& StartRoomId, const FString& GoalRoomId, TArray<FString>& RoomIds) const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    struct FFixture
    {
        FString Id;
        FString Type;
        int32 Deck = 0;
        float X = 0, Z = 0, Y = 0, W = 0, D = 0, H = 0;
        bool bSolid = false;
    };
    TArray<FFixture> SourceFixtures;
    TArray<FVector> LiftStopsMeters;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> DoorMeshes;
    TMap<FString, int32> DoorLookup;
    float LiftHeightMeters = 0.0f;

    bool ParseSource(const FString& JsonText, int32 Family);
    bool BuildGeometry();
    void ClearGeometry();
    void UpdateDoorMesh(int32 DoorIndex);
    void AddBox(UInstancedStaticMeshComponent* Component, float X, float Y, float Z, float Width, float Height, float Depth);
    void AddWallWithOpenings(bool bFixedX, float Fixed, float A, float B, int32 Deck, float FloorY);
    int32 FindRoomIndex(float X, float Z, int32 Deck, bool bPreferNonCorridor) const;
    int32 FindDeckAtHeight(float HeightMeters) const;
};
