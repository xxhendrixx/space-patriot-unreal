#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPWorldDressing.generated.h"

class ASPWorldSurface;
struct FSPWorldSurfaceSample;
struct FSPSocietySettlement;
class UMaterialInstanceDynamic;
class UStaticMesh;
class USceneComponent;
class UUserWidget;

/** A stable, local placement relative to the tangent plane at a landing site. */
struct FSPWorldRockPlacement
{
    FVector2D OffsetCm = FVector2D::ZeroVector;
    float Scale = 1.0f;
    float YawDegrees = 0.0f;
    int32 MeshIndex = 0;
};

/** Foliage pack index: 0 dry rooibos, 1 green shrub, 2 quiver tree. */
struct FSPWorldFoliagePlacement
{
    FVector2D OffsetCm = FVector2D::ZeroVector;
    float Scale = 1.0f;
    float YawDegrees = 0.0f;
    int32 MeshIndex = 0;
};

/** Modular field-site piece: 0 structure, 1 mast, 2 freight crate. */
struct FSPWorldSitePiece
{
    FVector2D OffsetCm = FVector2D::ZeroVector;
    FVector Scale = FVector::OneVector;
    float YawDegrees = 0.0f;
    int32 MeshIndex = 0;
    bool bCollision = true;
};

/**
 * Bounded placeholder dressing for solid non-Earth worlds. Call ApplyWorld
 * after a successful hyperjump arrival; it clears the prior world's actors.
 * The focus is the ship, so touchdown at another site can re-anchor the props.
 * No downloaded asset binaries are part of this class.
 */
UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPWorldDressing : public AActor
{
    GENERATED_BODY()

public:
    ASPWorldDressing();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Returns false when a required locally installed pack mesh is absent. */
    UFUNCTION(BlueprintCallable, Category="Space Patriot|World Dressing")
    bool ApplyWorld(const FString& WorldId, ASPWorldSurface* Surface, AActor* LandingFocus);

    UFUNCTION(BlueprintCallable, Category="Space Patriot|World Dressing")
    void ClearWorld();

    UFUNCTION(BlueprintPure, Category="Space Patriot|World Dressing")
    int32 GetSpawnedActorCount() const { return SpawnedActors.Num(); }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|World Dressing")
    FString ActiveWorldId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|World Dressing")
    FString ActiveBiome;

    /** Source-climate-driven local style, useful to later asset-pack replacements. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|World Dressing")
    FName ActiveRegionStyle = NAME_None;

    /** Procedural field-site key; different from a catalog settlement ID. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|World Dressing")
    FString ActiveSiteId;

    /** Named settlement whose market/services this temporary field site uses. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|World Dressing")
    FString ActiveServiceSettlementId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|World Dressing")
    FName ActiveSiteKind = NAME_None;

    /** Deterministic pure layout math, exposed in C++ for automation tests. */
    static TArray<FSPWorldRockPlacement> PlanRocks(
        const FString& WorldId, const FString& Biome, const FVector& SiteRadial);

    /** Empty for barren or unlandable biomes; deterministic for a landing cell. */
    static TArray<FSPWorldFoliagePlacement> PlanFoliage(
        const FString& WorldId, const FString& Biome, const FVector& SiteRadial);

    /** Bounded visual density, derived from source climate rather than a fixed biome count. */
    static float FoliageDensityFor(const FString& Biome, const FSPWorldSurfaceSample& Sample);

    static FString SiteIdFor(const FString& WorldId, const FVector& SiteRadial);
    static FName SiteKindFor(const FString& WorldId, const FVector& SiteRadial);
    static FName TemplateForSettlement(const FSPSocietySettlement& Settlement);
    static bool ResolveServiceSettlement(const FString& WorldId, const FVector& SiteRadial,
        const TArray<FSPSocietySettlement>& Settlements, FString& OutId, FName& OutTemplate);
    static TArray<FSPWorldSitePiece> PlanSite(const FString& WorldId, const FVector& SiteRadial,
        FName KindOverride = NAME_None);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|World Dressing")
    TObjectPtr<USceneComponent> SceneRoot;

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> SpawnedActors;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> OutpostMaterial;

    /** External Shooter template score screen; the weapon ammo counter is a separate widget. */
    UPROPERTY(Transient)
    TSubclassOf<UUserWidget> ShooterScoreWidgetClass;
    UPROPERTY(Transient)
    TSubclassOf<AActor> ShooterNpcClass;
    UPROPERTY(Transient)
    TSubclassOf<AActor> ShooterAiControllerClass;

    TWeakObjectPtr<ASPWorldSurface> ActiveSurface;
    TWeakObjectPtr<AActor> FocusActor;
    FVector AnchorFocusLocation = FVector::ZeroVector;
    float RecenterElapsed = 0.0f;

    void HideShooterTemplateOverlays();

    void SpawnDressing(ASPWorldSurface* Surface, AActor* LandingFocus,
                       UStaticMesh* const RockMeshes[3], UStaticMesh* const FoliageMeshes[3], UStaticMesh* Crate,
                       UStaticMesh* ChamferCube, UStaticMesh* Cylinder);
};
