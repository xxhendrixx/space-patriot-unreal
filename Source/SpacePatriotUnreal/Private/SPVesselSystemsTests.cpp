#include "SPVesselSystemsComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPVesselSystemsSourceParityTest,
    "SpacePatriot.VesselSystems.SourceParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPVesselSystemsSourceParityTest::RunTest(const FString& Parameters)
{
    USPVesselSystemsComponent* Systems = NewObject<USPVesselSystemsComponent>(GetTransientPackage());
    if (!TestNotNull(TEXT("engineering component exists"), Systems)) return false;
    FSPVesselTelemetry Telemetry = Systems->GetTelemetry(true);
    TestEqual(TEXT("six source subsystems"), Telemetry.Components.Num(), 6);
    TestEqual(TEXT("twelve starting power points"), Telemetry.EngineAllocation + Telemetry.WeaponAllocation + Telemetry.ShieldAllocation, 12);
    TestTrue(TEXT("full healthy engine factor"), FMath::IsNearlyEqual(Telemetry.EngineFactor, 1.0f));
    TestTrue(TEXT("ship weapons initially permitted"), Systems->CanFireShipWeapons(false, true));
    TestFalse(TEXT("no powered ship weapons with reactor off"), Systems->CanFireShipWeapons(false, false));

    struct FProfileCheck { ESPVesselProfile Profile; int32 Engine; int32 Weapon; int32 Shield; };
    const FProfileCheck Profiles[] = {
        { ESPVesselProfile::Balanced, 4, 4, 4 },
        { ESPVesselProfile::Combat, 3, 6, 3 },
        { ESPVesselProfile::Defense, 2, 2, 8 },
        { ESPVesselProfile::Travel, 8, 2, 2 }
    };
    for (const FProfileCheck& Check : Profiles)
    {
        TestTrue(TEXT("source power profile accepted"), Systems->ApplyPreset(Check.Profile));
        Telemetry = Systems->GetTelemetry(true);
        TestEqual(TEXT("profile engine points"), Telemetry.EngineAllocation, Check.Engine);
        TestEqual(TEXT("profile weapon points"), Telemetry.WeaponAllocation, Check.Weapon);
        TestEqual(TEXT("profile shield points"), Telemetry.ShieldAllocation, Check.Shield);
    }
    TestFalse(TEXT("Custom is not a fourth preset"), Systems->ApplyPreset(ESPVesselProfile::Custom));
    Systems->ApplyPreset(ESPVesselProfile::Balanced);
    TestTrue(TEXT("allocation transfers one point"), Systems->AdjustAllocation(ESPVesselBus::Engines, 1));
    Telemetry = Systems->GetTelemetry(true);
    TestEqual(TEXT("engine allocation rises"), Telemetry.EngineAllocation, 5);
    TestEqual(TEXT("deterministic tie breaks to weapons"), Telemetry.WeaponAllocation, 3);
    TestEqual(TEXT("power budget conserved"), Telemetry.EngineAllocation + Telemetry.WeaponAllocation + Telemetry.ShieldAllocation, 12);
    TestTrue(TEXT("custom profile after manual allocation"), Telemetry.Profile == ESPVesselProfile::Custom);
    TestFalse(TEXT("two-point move rejected"), Systems->AdjustAllocation(ESPVesselBus::Engines, 2));

    Systems->ResetToDefaults();
    TestTrue(TEXT("reactor isolation accepted"), Systems->SetComponentEnabled(ESPVesselSubsystem::Reactor, false));
    TestTrue(TEXT("isolated reactor removes engine output"), FMath::IsNearlyZero(Systems->GetFactor(ESPVesselSubsystem::Engines, true)));
    TestTrue(TEXT("isolated reactor removes life support"), FMath::IsNearlyZero(Systems->GetFactor(ESPVesselSubsystem::LifeSupport, true)));
    Systems->SetComponentEnabled(ESPVesselSubsystem::Reactor, true);
    TestTrue(TEXT("power-off removes cooler output"), FMath::IsNearlyZero(Systems->GetFactor(ESPVesselSubsystem::Cooler, false)));

    TestTrue(TEXT("explicit damage applies"), Systems->ApplyComponentDamage(ESPVesselSubsystem::Engines, 50.0f));
    TestFalse(TEXT("online component cannot be repaired"), Systems->StartRepair(ESPVesselSubsystem::Engines));
    Systems->SetComponentEnabled(ESPVesselSubsystem::Engines, false);
    TestTrue(TEXT("isolated damaged component can be repaired"), Systems->StartRepair(ESPVesselSubsystem::Engines));
    FSPVesselSimulationInput Input;
    for (int32 Step = 0; Step < 10; ++Step) Systems->StepSystems(0.1f, Input);
    FSPVesselComponentStatus Engine;
    TestTrue(TEXT("engine status accessible for an MFD"), Systems->GetComponentStatus(ESPVesselSubsystem::Engines, Engine));
    TestTrue(TEXT("repair restores three health for three spares"), FMath::IsNearlyEqual(Engine.HealthPercent, 53.0f, 0.001f));
    Telemetry = Systems->GetTelemetry(true);
    TestTrue(TEXT("spares are finite and consumed"), FMath::IsNearlyEqual(Telemetry.SpareParts, 117.0f, 0.001f));
    Systems->SetComponentEnabled(ESPVesselSubsystem::Engines, true);
    TestFalse(TEXT("re-enabling cancels field repair"), Systems->GetTelemetry(true).bRepairInProgress);
    TestFalse(TEXT("in-flight service is forbidden"), Systems->TryService(false, false, 100.0f));
    TestFalse(TEXT("walkers cannot trigger service"), Systems->TryService(true, true, 100.0f));
    FSPVesselSnapshot NoSpares = Systems->CaptureState();
    NoSpares.SpareParts = 0.0f;
    TestTrue(TEXT("zero-spares state restores"), Systems->RestoreState(NoSpares));
    Systems->SetComponentEnabled(ESPVesselSubsystem::Engines, false);
    TestFalse(TEXT("field repair refuses an empty spare bin"), Systems->StartRepair(ESPVesselSubsystem::Engines));
    TestTrue(TEXT("landed or docked service accepted"), Systems->TryService(true, false, 100.0f));
    Telemetry = Systems->GetTelemetry(true);
    TestTrue(TEXT("service resets finite spares"), FMath::IsNearlyEqual(Telemetry.SpareParts, 120.0f));
    TestTrue(TEXT("service resets component health"), FMath::IsNearlyEqual(Telemetry.Components[1].HealthPercent, 100.0f));

    Systems->ResetToDefaults(100.0f);
    TestTrue(TEXT("mode set to NAV"), Systems->SetMode(ESPVesselMode::NAV, false));
    TestFalse(TEXT("NAV inhibits ship weapons"), Systems->CanFireShipWeapons(false, true));
    TestTrue(TEXT("on-foot personal weapons remain available"), Systems->CanFireShipWeapons(true, true));
    Input.ShieldPercent = 90.0f;
    FSPVesselSimulationOutput Output = Systems->StepSystems(0.1f, Input);
    TestTrue(TEXT("NAV disarms connected combat controller"), Output.bDisarmWeapons);
    TestTrue(TEXT("NAV drains shields 16 percent per second"), FMath::IsNearlyEqual(Output.ShieldPercent, 88.4f, 0.001f));
    TestTrue(TEXT("NAV warning is visible"), Systems->GetTelemetry(true).Warning.Contains(TEXT("SHIELDS DISCHARGING")));
    TestFalse(TEXT("transit prevents mode change"), Systems->SetMode(ESPVesselMode::SCM, true));
    TestTrue(TEXT("SCM selectable after transit"), Systems->SetMode(ESPVesselMode::SCM, false));
    Output = Systems->StepSystems(0.1f, Input);
    TestTrue(TEXT("SCM requests cruise cancellation"), Output.bCancelCruise);
    TestTrue(TEXT("SCM requests jump cancellation"), Output.bCancelJump);
    TestFalse(TEXT("SCM no longer drains shields"), Output.ShieldPercent < Input.ShieldPercent);
    Input.CruiseSpool01 = 0.2f;
    Output = Systems->StepSystems(0.1f, Input);
    TestTrue(TEXT("cruise automatically enters NAV"), Systems->GetTelemetry(true).Mode == ESPVesselMode::NAV);
    Input.CruiseSpool01 = 0.0f;

    Systems->ResetToDefaults();
    Input = FSPVesselSimulationInput();
    Input.HullPercent = 90.0f;
    Systems->StepSystems(0.1f, Input);
    int32 DamagedCount = 0;
    for (const FSPVesselComponentStatus& Component : Systems->GetTelemetry(true).Components)
        if (Component.HealthPercent < 100.0f) { ++DamagedCount; TestTrue(TEXT("hull hit damages source ship subsystems"), FMath::IsNearlyEqual(Component.HealthPercent, 94.0f, 0.001f)); }
    TestEqual(TEXT("one deterministic subsystem damaged by hull loss"), DamagedCount, 1);

    Systems->ResetToDefaults();
    Input = FSPVesselSimulationInput();
    Input.bPowerOn = false;
    for (int32 Step = 0; Step < 500; ++Step) Systems->StepSystems(0.1f, Input);
    Telemetry = Systems->GetTelemetry(false);
    TestTrue(TEXT("pressure stays on a 0-100 scale"), Telemetry.PressurePercent > 60.0f && Telemetry.PressurePercent < 70.0f);
    TestEqual(TEXT("pressure warning takes priority"), Telemetry.Warning, FString(TEXT("CABIN PRESSURE LOW")));
    Input.bPowerOn = true;
    Systems->StepSystems(0.1f, Input);
    TestTrue(TEXT("life support restores pressure"), Systems->GetTelemetry(true).PressurePercent > Telemetry.PressurePercent);

    Systems->ResetToDefaults();
    Input = FSPVesselSimulationInput();
    Input.Thrust01 = 1.0f;
    Input.ShipHeat01 = 0.1f;
    Systems->SetComponentEnabled(ESPVesselSubsystem::Cooler, false);
    Output = Systems->StepSystems(0.1f, Input);
    TestTrue(TEXT("failed cooler raises flight heat under thrust"), Output.ShipHeat01 > Input.ShipHeat01);
    FSPVesselSnapshot Snapshot = Systems->CaptureState();
    Snapshot.Components[1].TemperatureCelsius = 101.0f;
    TestTrue(TEXT("valid state restored"), Systems->RestoreState(Snapshot));
    const float HealthBeforeOverheat = Systems->GetTelemetry(true).Components[1].HealthPercent;
    Systems->StepSystems(0.1f, Input);
    TestTrue(TEXT("overheated component takes thermal damage"), Systems->GetTelemetry(true).Components[1].HealthPercent < HealthBeforeOverheat);
    Snapshot = Systems->CaptureState();
    Snapshot.Components[1].TemperatureCelsius = 200.0f;
    TestTrue(TEXT("hot component state restores"), Systems->RestoreState(Snapshot));
    TestTrue(TEXT("thermal derate has 15 percent floor"), FMath::IsNearlyEqual(Systems->GetFactor(ESPVesselSubsystem::Engines, true), 0.15f * Systems->GetTelemetry(true).Components[1].HealthPercent / 100.0f, 0.001f));
    Snapshot = Systems->CaptureState();
    Snapshot.EngineAllocation = 11;
    TestFalse(TEXT("corrupt save allocation rejected"), Systems->RestoreState(Snapshot));
    TestEqual(TEXT("failed restore preserves state"), Systems->GetTelemetry(true).EngineAllocation, 4);

    return true;
}
#endif
