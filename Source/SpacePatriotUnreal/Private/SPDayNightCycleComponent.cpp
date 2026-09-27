#include "SPDayNightCycleComponent.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"

USPDayNightCycleComponent::USPDayNightCycleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USPDayNightCycleComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentHour = WrapHour(StartingHour);
    FindSceneLights();
    ApplyLighting();
}

void USPDayNightCycleComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bAdvanceTime || DeltaTime <= 0.0f) return;
    CurrentHour = WrapHour(CurrentHour + DeltaTime * 24.0f / FMath::Max(DayLengthSeconds, 60.0f));
    LightingUpdateCountdown -= DeltaTime;
    if (LightingUpdateCountdown <= 0.0f)
    {
        ApplyLighting();
        LightingUpdateCountdown = 0.5f;
    }
}

float USPDayNightCycleComponent::WrapHour(float Hour)
{
    if (!FMath::IsFinite(Hour)) return 9.0f;
    float Wrapped = FMath::Fmod(Hour, 24.0f);
    if (Wrapped < 0.0f) Wrapped += 24.0f;
    return Wrapped;
}

void USPDayNightCycleComponent::SetHour(float NewHour)
{
    CurrentHour = WrapHour(NewHour);
    ApplyLighting();
    LightingUpdateCountdown = 0.5f;
}

void USPDayNightCycleComponent::AdvanceHours(float Hours)
{
    if (FMath::IsFinite(Hours)) SetHour(CurrentHour + Hours);
}

FString USPDayNightCycleComponent::GetClockText() const
{
    const int32 TotalMinutes = FMath::FloorToInt(WrapHour(CurrentHour) * 60.0f) % (24 * 60);
    return FString::Printf(TEXT("%02d:%02d"), TotalMinutes / 60, TotalMinutes % 60);
}

void USPDayNightCycleComponent::FindSceneLights()
{
    if (!GetWorld()) return;
    UDirectionalLightComponent* FirstSun = nullptr;
    for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
    {
        UDirectionalLightComponent* Candidate = Cast<UDirectionalLightComponent>(It->GetLightComponent());
        if (!Candidate) continue;
        if (It->ActorHasTag(TEXT("SP_PrototypeMoon")))
        {
            MoonLight = Candidate;
            continue;
        }
        if (!FirstSun) FirstSun = Candidate;
        if (It->ActorHasTag(TEXT("SP_PrototypeSun")))
        {
            SunLight = Candidate;
            break;
        }
    }
    if (!SunLight) SunLight = FirstSun;
    if (!SunLight)
    {
        // The same component also works in a bare prototype map with no placed sun.
        ADirectionalLight* Spawned = GetWorld()->SpawnActor<ADirectionalLight>();
        if (Spawned) SunLight = Cast<UDirectionalLightComponent>(Spawned->GetLightComponent());
    }
    if (SunLight)
    {
        SunDayIntensity = FMath::Max(0.01f, SunLight->Intensity);
        SunLight->SetMobility(EComponentMobility::Movable);
        SunLight->SetAtmosphereSunLight(true);
    }

    if (!MoonLight)
    {
        // A cheap, cool fill keeps the prototype playable after the sun sets.
        // This is a local lighting cue, not an orbital or lunar simulation.
        ADirectionalLight* Spawned = GetWorld()->SpawnActor<ADirectionalLight>();
        if (Spawned)
        {
            Spawned->Tags.AddUnique(TEXT("SP_PrototypeMoon"));
            MoonLight = Cast<UDirectionalLightComponent>(Spawned->GetLightComponent());
            if (MoonLight) MoonLight->SetCastShadows(false);
        }
    }
    if (MoonLight)
    {
        MoonLight->SetMobility(EComponentMobility::Movable);
        MoonLight->SetAtmosphereSunLight(false);
        MoonLight->SetLightColor(FLinearColor(0.52f, 0.66f, 0.80f), false);
    }

    for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
    {
        AmbientLight = It->GetLightComponent();
        break;
    }
    if (AmbientLight)
    {
        AmbientDayIntensity = FMath::Max(0.01f, AmbientLight->Intensity);
        AmbientLight->SetMobility(EComponentMobility::Movable);
    }
}

void USPDayNightCycleComponent::ApplyLighting()
{
    // One repeating local-day clock is enough for the prototype. It does not
    // infer orbital positions, axial tilt, seasons, or body-specific ephemerides.
    const float Phase = (CurrentHour - 6.0f) * (2.0f * PI / 24.0f);
    const float AltitudeDegrees = FMath::Sin(Phase) * 70.0f;
    const float AzimuthDegrees = CurrentHour * 15.0f + 45.0f;
    const float Daylight = FMath::SmoothStep(-7.0f, 12.0f, AltitudeDegrees);
    // Bring the fill up as daylight fades so dusk stays playable as well as night.
    const float NightFill = 1.0f - Daylight;
    if (SunLight)
    {
        SunLight->SetForwardShadingPriority(Daylight >= 0.5f ? 1 : 0);
        SunLight->SetWorldRotation(FRotator(-AltitudeDegrees, AzimuthDegrees, 0.0f));
        SunLight->SetIntensity(SunDayIntensity * Daylight);
    }
    if (MoonLight)
    {
        MoonLight->SetForwardShadingPriority(Daylight < 0.5f ? 1 : 0);
        MoonLight->SetWorldRotation(FRotator(-48.0f, AzimuthDegrees + 130.0f, 0.0f));
        MoonLight->SetIntensity(SunDayIntensity * 0.35f * NightFill);
    }
    if (AmbientLight)
        AmbientLight->SetIntensity(AmbientDayIntensity * FMath::Lerp(0.28f, 1.0f, Daylight));
}
