#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPTravelNavigationComponent.h"
#include "SPHyperdriveVisualComponent.generated.h"

class UCameraComponent;
class UMaterialInterface;
class UProceduralMeshComponent;

USTRUCT(BlueprintType)
struct FSPHyperdriveVisualStats
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ESPTravelPhase Phase = ESPTravelPhase::Flight;
    UPROPERTY(BlueprintReadOnly) bool bEffectActive = false;
    UPROPERTY(BlueprintReadOnly) bool bMeshVisible = false;
    UPROPERTY(BlueprintReadOnly) int32 AlignmentMarkers = 0;
    UPROPERTY(BlueprintReadOnly) int32 StarStreaks = 0;
    UPROPERTY(BlueprintReadOnly) int32 VertexCount = 0;
    UPROPERTY(BlueprintReadOnly) float AlignmentRadiusCm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AnimationSeconds = 0.0f;
};

/**
 * Camera-local hyperdrive visuals without Niagara or a full-screen flash.
 * Alignment brackets converge during JumpCharging; muted radial star streaks
 * move through the viewport only during JumpTransit. The navigation component
 * is the authority; visuals clear immediately on cancel or exterior arrival.
 */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPHyperdriveVisualComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPHyperdriveVisualComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hyperdrive|Accessibility") bool bSpeedEffectsEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hyperdrive|Visual") TObjectPtr<UMaterialInterface> EffectMaterial;

    UFUNCTION(BlueprintCallable, Category="Hyperdrive|Setup") bool SetNavigationComponent(USPTravelNavigationComponent* InNavigation);
    UFUNCTION(BlueprintCallable, Category="Hyperdrive|Setup") bool BindToCamera(UCameraComponent* InCamera);
    UFUNCTION(BlueprintCallable, Category="Hyperdrive|Visual") void RefreshVisuals(float DeltaSeconds);
    UFUNCTION(BlueprintPure, Category="Hyperdrive|Visual") FSPHyperdriveVisualStats GetVisualStats() const { return Stats; }
    UFUNCTION(BlueprintPure, Category="Hyperdrive|Visual") UProceduralMeshComponent* GetVisualMesh() const { return VisualMesh.Get(); }

private:
    UPROPERTY(Transient) TObjectPtr<USPTravelNavigationComponent> Navigation;
    UPROPERTY(Transient) TObjectPtr<UCameraComponent> ViewCamera;
    UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> VisualMesh;
    UPROPERTY(Transient) FSPHyperdriveVisualStats Stats;
    ESPTravelPhase PreviousPhase = ESPTravelPhase::Flight;
    float PhaseTimeSeconds = 0.0f;

    void EnsureVisualMesh();
    void ClearVisualMesh();
};
