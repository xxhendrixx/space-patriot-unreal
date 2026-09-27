#include "SPHyperdriveVisualComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    FSPTravelContext SafeSpace()
    {
        FSPTravelContext Context;
        Context.AltitudeKm = 4.0;
        Context.NearestStationDistanceKm = 6.0;
        return Context;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPHyperdriveVisualStateTest,
    "SpacePatriot.Travel.HyperdriveVisualState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPHyperdriveVisualStateTest::RunTest(const FString& Parameters)
{
    USPTravelNavigationComponent* Nav = NewObject<USPTravelNavigationComponent>(GetTransientPackage());
    USPHyperdriveVisualComponent* Visual = NewObject<USPHyperdriveVisualComponent>(GetTransientPackage());
    if (!TestTrue(TEXT("source worlds load"), Nav->LoadWorldCatalog())) return false;
    TestNotNull(TEXT("cooked vertex-color FX material loads"), Visual->EffectMaterial.Get());
    TestTrue(TEXT("visual reads authoritative travel component"), Visual->SetNavigationComponent(Nav));
    TestFalse(TEXT("ordinary flight draws no effect"), Visual->GetVisualStats().bEffectActive);
    TestEqual(TEXT("idle has no geometry"), Visual->GetVisualStats().VertexCount, 0);

    const FSPTravelContext Context = SafeSpace();
    TestTrue(TEXT("destination set"), Nav->SelectDestination(TEXT("mars")));
    TestTrue(TEXT("sector charge starts"), Nav->BeginJump(Context));
    Visual->RefreshVisuals(0.0f);
    FSPHyperdriveVisualStats Stats = Visual->GetVisualStats();
    TestEqual(TEXT("charging phase drives visuals"), Stats.Phase, ESPTravelPhase::JumpCharging);
    TestTrue(TEXT("charging shows alignment"), Stats.bEffectActive);
    TestEqual(TEXT("charging has four bracket groups and inner ring"), Stats.AlignmentMarkers, 36);
    TestEqual(TEXT("each alignment marker is a mesh quad"), Stats.VertexCount, 144);
    TestEqual(TEXT("charging has no transit stars"), Stats.StarStreaks, 0);
    const float InitialRadius = Stats.AlignmentRadiusCm;
    for (int32 Index = 0; Index < 10; ++Index) Nav->AdvanceDrive(0.1f, Context, false, false);
    Visual->RefreshVisuals(0.1f);
    TestTrue(TEXT("alignment ring closes as spool advances"), Visual->GetVisualStats().AlignmentRadiusCm < InitialRadius);
    TestTrue(TEXT("brake cancels charge"), Nav->CancelJump());
    Visual->RefreshVisuals(0.0f);
    TestFalse(TEXT("cancel clears visual immediately"), Visual->GetVisualStats().bEffectActive);
    TestEqual(TEXT("cancel clears geometry"), Visual->GetVisualStats().VertexCount, 0);

    TestTrue(TEXT("charge restarts"), Nav->BeginJump(Context));
    for (int32 Index = 0; Index < 21; ++Index) Nav->AdvanceDrive(0.1f, Context, false, false);
    Visual->RefreshVisuals(0.0f);
    Stats = Visual->GetVisualStats();
    TestEqual(TEXT("paid transit drives stars"), Stats.Phase, ESPTravelPhase::JumpTransit);
    TestEqual(TEXT("bounded star streak count"), Stats.StarStreaks, 72);
    TestEqual(TEXT("star streak mesh vertex count"), Stats.VertexCount, 288);
    TestEqual(TEXT("alignment removed in transit"), Stats.AlignmentMarkers, 0);
    Visual->RefreshVisuals(0.1f);
    TestTrue(TEXT("streaks animate with time"), Visual->GetVisualStats().AnimationSeconds > 0.0f);
    Visual->bSpeedEffectsEnabled = false;
    Visual->RefreshVisuals(0.1f);
    TestFalse(TEXT("accessibility switch stops streaks"), Visual->GetVisualStats().bEffectActive);
    TestEqual(TEXT("disabled effect removes geometry"), Visual->GetVisualStats().VertexCount, 0);
    Visual->bSpeedEffectsEnabled = true;
    Visual->RefreshVisuals(0.0f);
    TestTrue(TEXT("effect resumes in active transit"), Visual->GetVisualStats().bEffectActive);
    TestTrue(TEXT("route confirms exterior arrival"), Nav->ConfirmJumpArrival());
    Visual->RefreshVisuals(0.0f);
    TestFalse(TEXT("exterior arrival stops stars"), Visual->GetVisualStats().bEffectActive);
    TestEqual(TEXT("exterior arrival clears geometry"), Visual->GetVisualStats().VertexCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPHyperdriveVisualMeshTest,
    "SpacePatriot.Travel.HyperdriveCameraMesh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPHyperdriveVisualMeshTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && (Context.WorldType == EWorldType::Editor ||
                Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
            {
                World = Context.World();
                break;
            }
        }
    }
    if (!TestNotNull(TEXT("editor game world for mesh attachment"), World)) return false;
    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("transient test ship"), Owner)) return false;
    Owner->SetFlags(RF_Transient);
    USceneComponent* Root = NewObject<USceneComponent>(Owner);
    Owner->SetRootComponent(Root);
    Root->RegisterComponent();
    UCameraComponent* Camera = NewObject<UCameraComponent>(Owner);
    Camera->SetupAttachment(Root);
    Camera->RegisterComponent();
    USPTravelNavigationComponent* Nav = NewObject<USPTravelNavigationComponent>(Owner);
    Nav->RegisterComponent();
    USPHyperdriveVisualComponent* Visual = NewObject<USPHyperdriveVisualComponent>(Owner);
    Visual->RegisterComponent();
    if (!TestTrue(TEXT("catalog loads"), Nav->LoadWorldCatalog()))
    {
        Owner->Destroy();
        return false;
    }
    Visual->SetNavigationComponent(Nav);
    TestTrue(TEXT("camera-bound procedural mesh created"), Visual->BindToCamera(Camera));
    UProceduralMeshComponent* Mesh = Visual->GetVisualMesh();
    TestNotNull(TEXT("procedural mesh exists"), Mesh);
    if (Mesh)
    {
        TestEqual(TEXT("mesh follows active camera"), Mesh->GetAttachParent(), static_cast<USceneComponent*>(Camera));
        TestEqual(TEXT("unlit material assigned"), Mesh->GetMaterial(0), Visual->EffectMaterial.Get());
    }
    const FSPTravelContext Context = SafeSpace();
    TestTrue(TEXT("route selected"), Nav->SelectDestination(TEXT("mars")));
    TestTrue(TEXT("charge starts"), Nav->BeginJump(Context));
    Visual->RefreshVisuals(0.0f);
    TestTrue(TEXT("alignment mesh visible"), Visual->GetVisualStats().bMeshVisible);
    if (Mesh && Mesh->GetProcMeshSection(0))
        TestEqual(TEXT("alignment geometry uploaded"), Mesh->GetProcMeshSection(0)->ProcVertexBuffer.Num(), 144);
    else AddError(TEXT("Alignment mesh section was not created"));
    TestTrue(TEXT("charge cancelled"), Nav->CancelJump());
    Visual->RefreshVisuals(0.0f);
    TestFalse(TEXT("cancel hides mesh"), Visual->GetVisualStats().bMeshVisible);
    TestTrue(TEXT("cancel deletes mesh section"), Mesh && Mesh->GetProcMeshSection(0) == nullptr);
    TestTrue(TEXT("charge restarts for transit mesh check"), Nav->BeginJump(Context));
    for (int32 Index = 0; Index < 21; ++Index) Nav->AdvanceDrive(0.1f, Context, false, false);
    Visual->RefreshVisuals(0.1f);
    TestTrue(TEXT("transit mesh visible"), Visual->GetVisualStats().bMeshVisible);
    if (Mesh && Mesh->GetProcMeshSection(0))
        TestEqual(TEXT("star streak geometry uploaded"), Mesh->GetProcMeshSection(0)->ProcVertexBuffer.Num(), 288);
    else AddError(TEXT("Star streak mesh section was not created"));
    TestTrue(TEXT("spatial route confirms arrival"), Nav->ConfirmJumpArrival());
    Visual->RefreshVisuals(0.0f);
    TestFalse(TEXT("arrival hides star mesh"), Visual->GetVisualStats().bMeshVisible);
    TestTrue(TEXT("arrival deletes star section"), Mesh && Mesh->GetProcMeshSection(0) == nullptr);
    Owner->Destroy();
    return true;
}
#endif
