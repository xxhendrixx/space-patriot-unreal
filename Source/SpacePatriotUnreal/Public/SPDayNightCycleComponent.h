#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPDayNightCycleComponent.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;

/** A prototype clock and sun cycle. World travel does not simulate orbits. */
UCLASS(ClassGroup=(SpacePatriot), meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPDayNightCycleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USPDayNightCycleComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    /** Real seconds for one game day; short enough to inspect in PIE. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Day Night", meta=(ClampMin="60"))
    float DayLengthSeconds = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Day Night")
    float StartingHour = 9.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Day Night")
    bool bAdvanceTime = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|Day Night")
    float CurrentHour = 9.0f;

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Day Night")
    void SetHour(float NewHour);

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Day Night")
    void AdvanceHours(float Hours);

    UFUNCTION(BlueprintPure, Category="Space Patriot|Day Night")
    FString GetClockText() const;

private:
    UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> SunLight;
    UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> MoonLight;
    UPROPERTY(Transient) TObjectPtr<USkyLightComponent> AmbientLight;
    float SunDayIntensity = 1.0f;
    float AmbientDayIntensity = 1.0f;
    float LightingUpdateCountdown = 0.0f;

    static float WrapHour(float Hour);
    void FindSceneLights();
    void ApplyLighting();
};
