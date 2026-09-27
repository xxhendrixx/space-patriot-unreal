#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "SPFlightPawn.generated.h"

class UCameraComponent;

USTRUCT(BlueprintType)
struct FSPFlightTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Space Patriot|Flight") float SpeedMetersPerSecond = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Space Patriot|Flight") float ThrottlePercent = 100.0f;
    UPROPERTY(BlueprintReadOnly, Category="Space Patriot|Flight") float FuelPercent = 100.0f;
    UPROPERTY(BlueprintReadOnly, Category="Space Patriot|Flight") float HeatPercent = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Space Patriot|Flight") bool bGearDown = true;
    UPROPERTY(BlueprintReadOnly, Category="Space Patriot|Flight") bool bFlightAssist = true;
    UPROPERTY(BlueprintReadOnly, Category="Space Patriot|Flight") bool bFlying = true;
    UPROPERTY(BlueprintReadOnly, Category="Space Patriot|Flight") bool bCruise = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSPFlightStateChanged);

// A native, six-axis bridge for the original Unity FlightMotor behavior.
// Unreal units are centimetres; X is the nose, Y is starboard, Z is up.
UCLASS(BlueprintType, Blueprintable)
class SPACEPATRIOTUNREAL_API ASPFlightPawn : public ADefaultPawn
{
    GENERATED_BODY()

public:
    ASPFlightPawn();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Space Patriot|Flight")
    TObjectPtr<UCameraComponent> FlightCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight", meta=(ClampMin="100"))
    float MaxSpeedCmPerSecond = 12000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight", meta=(ClampMin="1"))
    float AccelerationCmPerSecondSquared = 2200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight", meta=(ClampMin="1"))
    float TurnRateDegreesPerSecond = 75.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight", meta=(ClampMin="0.05", ClampMax="3.0"))
    float ThrottleLimit = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight", meta=(ClampMin="0"))
    float StandHeightCm = 250.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight")
    FVector PlanetCenterCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight")
    bool bUseRadialSurface = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight")
    bool bFlying = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight")
    bool bPowered = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight")
    bool bFlightAssist = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight")
    bool bGearDown = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight")
    bool bCruise = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight", meta=(ClampMin="0", ClampMax="100"))
    float FuelPercent = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Space Patriot|Flight", meta=(ClampMin="0", ClampMax="100"))
    float HeatPercent = 0.0f;
    UPROPERTY(BlueprintAssignable, Category="Space Patriot|Flight")
    FSPFlightStateChanged OnFlightStateChanged;

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Flight") FSPFlightTelemetry GetFlightTelemetry() const;
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Flight") void SetThrottleLimit(float NewLimit);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Flight") void SetFlightAssist(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Flight") void SetGearDown(bool bDown);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Flight") bool Launch();
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Flight") bool RequestSurfaceLanding();

private:
    FVector FlightVelocityCmPerSecond = FVector::ZeroVector;
    FVector AngularVelocityDegreesPerSecond = FVector::ZeroVector;
    FVector LandingPosition = FVector::ZeroVector;
    FQuat LandingRotation = FQuat::Identity;
    FVector SurfaceNormal = FVector::UpVector;
    float ForwardInput = 0.0f;
    float RightInput = 0.0f;
    float UpInput = 0.0f;
    float PitchInput = 0.0f;
    float YawInput = 0.0f;
    float RollInput = 0.0f;
    float MousePitch = 0.0f;
    float MouseYaw = 0.0f;
    bool bBrakeHeld = false;
    bool bBoostHeld = false;
    bool bLandingPending = false;
    bool bCockpitCamera = false;

    void SetForward(float Value) { ForwardInput = Value; }
    void SetRight(float Value) { RightInput = Value; }
    void SetUp(float Value) { UpInput = Value; }
    void SetPitch(float Value) { PitchInput = Value; }
    void SetYaw(float Value) { YawInput = Value; }
    void SetRoll(float Value) { RollInput = Value; }
    void SetMousePitch(float Value) { MousePitch = Value; }
    void SetMouseYaw(float Value) { MouseYaw = Value; }
    void AdjustThrottle(float Value);
    void StartBrake() { bBrakeHeld = true; bCruise = false; }
    void StopBrake() { bBrakeHeld = false; }
    void StartBoost() { bBoostHeld = true; }
    void StopBoost() { bBoostHeld = false; }
    void ToggleGear() { SetGearDown(!bGearDown); }
    void ToggleAssist() { SetFlightAssist(!bFlightAssist); }
    void ToggleCruise();
    void TogglePower();
    void ToggleCamera();
    void TryLand() { RequestSurfaceLanding(); }
    FVector TraceDown() const;
    void UpdateGearMeshes();
};
