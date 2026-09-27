#include "SPHyperdriveVisualComponent.h"

#include "Camera/CameraComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float PlaneDepthCm = 500.0f;
    constexpr float TwoPi = 6.28318530718f;

    struct FMeshFrame
    {
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UV;
        TArray<FLinearColor> Colors;

        void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D,
            const FLinearColor& Outer, const FLinearColor& Inner)
        {
            const int32 First = Vertices.Num();
            Vertices.Append({A, B, C, D});
            Triangles.Append({First, First + 1, First + 2, First, First + 2, First + 3});
            for (int32 Index = 0; Index < 4; ++Index) Normals.Add(FVector(-1.0, 0.0, 0.0));
            UV.Append({FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1)});
            Colors.Append({Outer, Outer, Inner, Inner});
        }
    };

    float Hash01(uint32 Value)
    {
        Value ^= Value >> 16;
        Value *= 0x7feb352du;
        Value ^= Value >> 15;
        Value *= 0x846ca68bu;
        Value ^= Value >> 16;
        return static_cast<float>(Value & 0xffffu) / 65535.0f;
    }

    FVector OnScreenPlane(float Radius, float Angle)
    {
        return FVector(PlaneDepthCm, FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
    }

    void RingArc(FMeshFrame& Mesh, float Radius, float HalfWidth, float Start, float End, const FLinearColor& Color)
    {
        Mesh.Quad(OnScreenPlane(Radius - HalfWidth, Start), OnScreenPlane(Radius + HalfWidth, Start),
            OnScreenPlane(Radius + HalfWidth, End), OnScreenPlane(Radius - HalfWidth, End), Color, Color);
    }

    void BuildAlignment(FMeshFrame& Mesh, float Charge, float Time, int32& MarkerCount)
    {
        const float Radius = 150.0f - 68.0f * FMath::Clamp(Charge, 0.0f, 1.0f);
        const float Rotation = Time * 0.38f;
        const float Alpha = 0.18f + 0.31f * Charge;
        const FLinearColor Teal(0.11f, 0.54f, 0.62f, Alpha);
        const FLinearColor Amber(0.62f, 0.42f, 0.13f, Alpha * 0.85f);
        for (int32 Quadrant = 0; Quadrant < 4; ++Quadrant)
        {
            const float Center = Rotation + Quadrant * TwoPi / 4.0f;
            for (int32 Segment = 0; Segment < 5; ++Segment)
            {
                const float Start = Center - 0.39f + Segment * 0.15f;
                RingArc(Mesh, Radius, 1.3f, Start, Start + 0.105f, Teal);
                ++MarkerCount;
            }
            const float TickAngle = Center + 0.01f;
            const float Inner = Radius - 14.0f;
            Mesh.Quad(OnScreenPlane(Inner, TickAngle - 0.006f), OnScreenPlane(Radius + 12.0f, TickAngle - 0.006f),
                OnScreenPlane(Radius + 12.0f, TickAngle + 0.006f), OnScreenPlane(Inner, TickAngle + 0.006f),
                Amber, Amber);
            ++MarkerCount;
        }
        const float InnerRadius = 27.0f + 17.0f * (1.0f - Charge);
        for (int32 Segment = 0; Segment < 16; ++Segment)
        {
            if (Segment % 4 == 3) continue;
            const float Start = Segment * TwoPi / 16.0f - Rotation * 0.5f;
            RingArc(Mesh, InnerRadius, 0.7f, Start, Start + TwoPi / 20.0f,
                FLinearColor(0.25f, 0.58f, 0.60f, Alpha * 0.7f));
            ++MarkerCount;
        }
    }

    void BuildStreaks(FMeshFrame& Mesh, float Time, int32& StreakCount)
    {
        constexpr int32 Count = 72;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const float Angle = Hash01(Index * 3719u + 91u) * TwoPi;
            const float Offset = Hash01(Index * 733u + 47u);
            const float Speed = 0.46f + Hash01(Index * 1297u + 13u) * 0.72f;
            const float Progress = FMath::Frac(Offset + Time * Speed);
            const float Head = 18.0f + Progress * Progress * 213.0f;
            const float Tail = FMath::Max(10.0f, Head - (9.0f + Progress * 53.0f));
            const float HalfAngle = (0.0022f + Progress * 0.0030f);
            const float Fade = FMath::Min(1.0f, (1.0f - Progress) * 6.0f) * FMath::Min(1.0f, Progress * 8.0f);
            const float Alpha = (0.10f + 0.28f * Progress) * Fade;
            const bool bWarm = (Index % 11 == 0);
            const FLinearColor Outer = bWarm
                ? FLinearColor(0.56f, 0.41f, 0.24f, Alpha * 0.18f)
                : FLinearColor(0.17f, 0.43f, 0.56f, Alpha * 0.18f);
            const FLinearColor Inner = bWarm
                ? FLinearColor(0.56f, 0.41f, 0.24f, Alpha)
                : FLinearColor(0.17f, 0.43f, 0.56f, Alpha);
            Mesh.Quad(OnScreenPlane(Head, Angle - HalfAngle), OnScreenPlane(Head, Angle + HalfAngle),
                OnScreenPlane(Tail, Angle + HalfAngle), OnScreenPlane(Tail, Angle - HalfAngle),
                Outer, Inner);
            ++StreakCount;
        }
    }
}

USPHyperdriveVisualComponent::USPHyperdriveVisualComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> FoundMaterial(
        TEXT("/Game/SpacePatriot/Materials/M_HyperdriveVisual.M_HyperdriveVisual"));
    if (FoundMaterial.Succeeded()) EffectMaterial = FoundMaterial.Object;
}

void USPHyperdriveVisualComponent::BeginPlay()
{
    Super::BeginPlay();
    if (AActor* Owner = GetOwner())
    {
        if (!Navigation) Navigation = Owner->FindComponentByClass<USPTravelNavigationComponent>();
        if (!ViewCamera) ViewCamera = Owner->FindComponentByClass<UCameraComponent>();
    }
    EnsureVisualMesh();
    RefreshVisuals(0.0f);
}

void USPHyperdriveVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearVisualMesh();
    if (IsValid(VisualMesh)) VisualMesh->DestroyComponent();
    VisualMesh = nullptr;
    Super::EndPlay(EndPlayReason);
}

void USPHyperdriveVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    RefreshVisuals(DeltaTime);
}

bool USPHyperdriveVisualComponent::SetNavigationComponent(USPTravelNavigationComponent* InNavigation)
{
    Navigation = InNavigation;
    PreviousPhase = ESPTravelPhase::Flight;
    PhaseTimeSeconds = 0.0f;
    RefreshVisuals(0.0f);
    return Navigation != nullptr;
}

bool USPHyperdriveVisualComponent::BindToCamera(UCameraComponent* InCamera)
{
    ViewCamera = InCamera;
    EnsureVisualMesh();
    RefreshVisuals(0.0f);
    return ViewCamera != nullptr && VisualMesh != nullptr;
}

void USPHyperdriveVisualComponent::EnsureVisualMesh()
{
    AActor* Owner = GetOwner();
    if (!Owner || !ViewCamera || !Owner->GetWorld()) return;
    if (!VisualMesh)
    {
        VisualMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("SPHyperdriveScreenGeometry"));
        Owner->AddInstanceComponent(VisualMesh);
        VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        VisualMesh->SetCastShadow(false);
        VisualMesh->SetReceivesDecals(false);
        VisualMesh->AttachToComponent(ViewCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        VisualMesh->RegisterComponent();
        VisualMesh->SetVisibility(false);
    }
    else if (VisualMesh->GetAttachParent() != ViewCamera)
    {
        VisualMesh->AttachToComponent(ViewCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }
    if (EffectMaterial) VisualMesh->SetMaterial(0, EffectMaterial);
}

void USPHyperdriveVisualComponent::ClearVisualMesh()
{
    if (IsValid(VisualMesh))
    {
        VisualMesh->ClearAllMeshSections();
        VisualMesh->SetVisibility(false);
    }
    Stats.bMeshVisible = false;
}

void USPHyperdriveVisualComponent::RefreshVisuals(float DeltaSeconds)
{
    const ESPTravelPhase Phase = Navigation ? Navigation->GetNavigationState().Phase : ESPTravelPhase::Flight;
    if (Phase != PreviousPhase)
    {
        PreviousPhase = Phase;
        PhaseTimeSeconds = 0.0f;
    }
    else if (FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.0f)
    {
        PhaseTimeSeconds += FMath::Min(DeltaSeconds, 0.1f);
    }
    Stats = FSPHyperdriveVisualStats();
    Stats.Phase = Phase;
    Stats.AnimationSeconds = PhaseTimeSeconds;
    Stats.bEffectActive = bSpeedEffectsEnabled &&
        (Phase == ESPTravelPhase::JumpCharging || Phase == ESPTravelPhase::JumpTransit);
    if (!Stats.bEffectActive)
    {
        ClearVisualMesh();
        return;
    }

    FMeshFrame Frame;
    if (Phase == ESPTravelPhase::JumpCharging)
    {
        const float Charge = Navigation->GetJumpChargeFraction();
        Stats.AlignmentRadiusCm = 150.0f - 68.0f * Charge;
        BuildAlignment(Frame, Charge, PhaseTimeSeconds, Stats.AlignmentMarkers);
    }
    else
    {
        BuildStreaks(Frame, PhaseTimeSeconds, Stats.StarStreaks);
    }
    Stats.VertexCount = Frame.Vertices.Num();
    if (!ViewCamera || !VisualMesh) return;
    VisualMesh->CreateMeshSection_LinearColor(0, Frame.Vertices, Frame.Triangles, Frame.Normals,
        Frame.UV, Frame.Colors, TArray<FProcMeshTangent>(), false);
    VisualMesh->SetVisibility(true);
    Stats.bMeshVisible = true;
}
