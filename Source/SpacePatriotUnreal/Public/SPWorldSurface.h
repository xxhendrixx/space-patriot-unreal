#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPWorldSurface.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/** A surface query uses the same four channels exported by the original Worldworks engines. */
USTRUCT(BlueprintType)
struct FSPWorldSurfaceSample
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Worldworks") float ElevationMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Worldworks") float Moisture = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Worldworks") float Temperature = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Worldworks") float Rock = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Worldworks") float Forest = 0.0f;
    /** Readable local art direction chosen from the source climate channels. */
    UPROPERTY(BlueprintReadOnly, Category="Worldworks") FName RegionStyle = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="Worldworks") FLinearColor Color = FLinearColor::Gray;
    UPROPERTY(BlueprintReadOnly, Category="Worldworks") bool bSourceTerrainField = false;
};

/**
 * Source-seeded globe and a bounded collision patch that follows a nearby player.
 * Unreal coordinates are X/Y ground and Z up, in centimetres. The archived
 * Worldworks inputs are Unity X/Z ground and Y up, in metres.
 */
UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPWorldSurface : public AActor
{
    GENERATED_BODY()

public:
    ASPWorldSurface();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Worldworks|Mesh")
    TObjectPtr<UProceduralMeshComponent> PlanetMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Worldworks|Mesh")
    TObjectPtr<UProceduralMeshComponent> DetailMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Worldworks|World")
    FString WorldId = TEXT("earth");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Worldworks|World")
    TObjectPtr<AActor> FocusActor;

    /** Low-cost far-globe material; must read VertexColor RGB for the source climate palette. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Worldworks|Mesh")
    TObjectPtr<UMaterialInterface> SurfaceMaterial;

    /** Optional near-field material for the streamed collision patch; null reuses SurfaceMaterial. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Worldworks|Mesh")
    TObjectPtr<UMaterialInterface> DetailMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Worldworks|Streaming", meta=(ClampMin="500", ClampMax="6000"))
    float PatchHalfSizeMeters = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Worldworks|Streaming", meta=(ClampMin="100", ClampMax="3000"))
    float MaxDetailAltitudeMeters = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Worldworks|Streaming", meta=(ClampMin="0.1", ClampMax="10"))
    float StreamIntervalSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Worldworks|Streaming")
    bool bEnableDetailCollision = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Worldworks|Diagnostics")
    FString ResolvedBiome;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Worldworks|Diagnostics")
    int32 SourceTerrainResolution = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Worldworks|Diagnostics")
    int32 SourceClimateWidth = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Worldworks|Diagnostics")
    int32 DetailVertices = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Worldworks|Diagnostics")
    int32 DetailTriangles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Worldworks|Diagnostics")
    float LastFocusAltitudeMeters = 0.0f;

    UFUNCTION(BlueprintCallable, CallInEditor, Category="Worldworks")
    bool RebuildSurface();

    UFUNCTION(BlueprintCallable, Category="Worldworks")
    bool ActivateWorld(const FString& NewWorldId);

    /** Center of the rendered local globe in Unreal world coordinates. */
    UFUNCTION(BlueprintPure, Category="Worldworks")
    FVector GetPlanetCenterWorld() const;

    UFUNCTION(BlueprintPure, Category="Worldworks")
    FSPWorldSurfaceSample SampleAtWorldLocation(FVector WorldLocation) const;

    /** Distance above this world's generated surface, independent of streamed detail LOD. */
    UFUNCTION(BlueprintPure, Category="Worldworks")
    float GetAltitudeMetersAtWorldLocation(FVector WorldLocation) const;

    UFUNCTION(BlueprintPure, Category="Worldworks")
    float GetScaledRadiusKm() const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    struct FAtlas
    {
        int32 Width = 0;
        int32 Height = 0;
        float SizeMeters = 0.0f;
        uint32 Seed = 0;
        TArray<FVector4f> Values;
        bool Load(const FString& Path, bool bSquareTerrain);
        FVector4f SampleClamped(float X, float Y) const;
        FVector4f SampleClimate(const FVector& Radial) const;
    };

    FAtlas TerrainField;
    FAtlas ClimateField;
    uint32 WorldSeed = 0;
    float WorldRadiusRatio = 1.0f;
    float TerrainAmplitude = 0.003f;
    float TerrainFrequency = 1.0f;
    float TerrainBase = 0.44f;
    FVector NoiseOffset = FVector::ZeroVector;
    FVector DetailAnchor = FVector::ZeroVector;
    int32 DetailLOD = -1;
    bool bDetailSectionCollidable = false;
    float StreamElapsed = 0.0f;

    bool LoadWorldProfile();
    void BuildPlanetMesh();
    void UpdateDetailMesh(bool bForce);
    void BuildDetailMesh(const FVector& FocusLocal, int32 Segments);
    void ApplyCollisionMode(bool bDetailCollidable);
    FVector GetFocusLocal() const;
    FVector GetPlanetCenterLocal() const;
    FVector GetRadialAtLocal(const FVector& LocalPoint) const;
    float GlobeHeightMeters(const FVector& Radial) const;
    float SurfaceHeightMeters(const FVector& Radial) const;
    FVector4f ClimateChannels(const FVector& Radial) const;
    FLinearColor SurfaceColor(const FVector& Radial, float HeightMeters) const;
    FName RegionStyleFor(const FVector4f& Climate) const;
    static float PeriodicNoise(const FVector& P);
};
