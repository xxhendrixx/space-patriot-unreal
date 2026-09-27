#include "SPTravelNavigationComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPTravelCatalogAndDestinationTest,
    "SpacePatriot.Travel.CatalogAndDestination",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPTravelCatalogAndDestinationTest::RunTest(const FString& Parameters)
{
    USPTravelNavigationComponent* Nav = NewObject<USPTravelNavigationComponent>(GetTransientPackage());
    if (!TestTrue(TEXT("load original world catalog"), Nav->LoadWorldCatalog())) return false;
    TestEqual(TEXT("all nineteen original worlds"), Nav->GetWorlds().Num(), 19);
    const FString Expected[] = {
        TEXT("mercury"), TEXT("venus"), TEXT("earth"), TEXT("mars"), TEXT("jupiter"),
        TEXT("saturn"), TEXT("uranus"), TEXT("neptune"), TEXT("trappist-1-b"),
        TEXT("trappist-1-c"), TEXT("trappist-1-d"), TEXT("trappist-1-e"),
        TEXT("trappist-1-f"), TEXT("trappist-1-g"), TEXT("trappist-1-h"),
        TEXT("toi-270-b"), TEXT("toi-270-c"), TEXT("k2-141-b"), TEXT("wasp-76-b")
    };
    for (int32 Index = 0; Index < 19; ++Index)
    {
        TestEqual(TEXT("source world ID and order"), Nav->GetWorlds()[Index].Id, FString(Expected[Index]));
    }
    FSPTravelWorld Mars;
    TestTrue(TEXT("canonicalized world lookup"), Nav->FindWorld(TEXT("SP-MARS"), Mars));
    TestEqual(TEXT("world is in source system"), Mars.System, FString(TEXT("Sun")));
    TestFalse(TEXT("unknown destination rejected"), Nav->SelectDestination(TEXT("bogus")));
    TestFalse(TEXT("current world cannot be destination"), Nav->SelectDestination(TEXT("earth")));
    TestTrue(TEXT("explicit destination"), Nav->SelectDestination(TEXT("mars")));
    TestEqual(TEXT("selected destination preserved"), Nav->GetNavigationState().DestinationWorldId, FString(TEXT("mars")));
    TestTrue(TEXT("cycling from Mars"), Nav->CycleDestination());
    TestEqual(TEXT("next world selected"), Nav->GetNavigationState().DestinationWorldId, FString(TEXT("jupiter")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPTravelJumpAndModeTest,
    "SpacePatriot.Travel.JumpAndMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPTravelJumpAndModeTest::RunTest(const FString& Parameters)
{
    USPTravelNavigationComponent* Nav = NewObject<USPTravelNavigationComponent>(GetTransientPackage());
    if (!TestTrue(TEXT("catalog loads"), Nav->LoadWorldCatalog())) return false;
    FSPTravelContext Context;
    Context.AltitudeKm = 4.0;
    Context.NearestStationDistanceKm = 6.0;
    TestTrue(TEXT("Mars selected"), Nav->SelectDestination(TEXT("mars")));
    Context.bGearRetracted = false;
    TestFalse(TEXT("gear blocks jump"), Nav->BeginJump(Context));
    Context.bGearRetracted = true;
    Context.AltitudeKm = 2.0;
    TestFalse(TEXT("atmosphere/terrain clearance blocks jump"), Nav->BeginJump(Context));
    Context.AltitudeKm = 4.0;
    Context.NearbyBodyRadiusKm = 1000.0;
    TestFalse(TEXT("large-world scaled clearance blocks jump"), Nav->BeginJump(Context));
    Context.NearbyBodyRadiusKm = 0.0;
    Context.NearestStationDistanceKm = 5.0;
    TestFalse(TEXT("station clearance blocks jump"), Nav->BeginJump(Context));
    Context.NearestStationDistanceKm = 6.0;
    Context.bCargoHatchClosed = false;
    TestFalse(TEXT("open cargo hatch blocks jump"), Nav->BeginJump(Context));
    Context.bCargoHatchClosed = true;
    TestTrue(TEXT("resources set"), Nav->SetDriveResources(8.0f, 0.83f));
    TestTrue(TEXT("safe jump begins spool"), Nav->BeginJump(Context));
    TestEqual(TEXT("jump switches to NAV"), Nav->GetNavigationState().DriveMode, ESPTravelDriveMode::NAV);
    TestTrue(TEXT("NAV inhibits weapons"), Nav->AreWeaponsInhibited());
    TestFalse(TEXT("destination locked during spool"), Nav->SelectDestination(TEXT("venus")));
    for (int32 Index = 0; Index < 5; ++Index) Nav->AdvanceDrive(0.1f, Context, false, false);
    TestTrue(TEXT("charge advances"), Nav->GetJumpChargeFraction() > 0.2f);
    TestTrue(TEXT("brake cancels spool"), Nav->CancelJump());
    TestEqual(TEXT("cancel returns manual flight"), Nav->GetNavigationState().Phase, ESPTravelPhase::Flight);
    TestEqual(TEXT("cancel consumes no fuel"), Nav->GetNavigationState().FuelPercent, 8.0f);
    TestTrue(TEXT("spool starts again"), Nav->BeginJump(Context));
    for (int32 Index = 0; Index < 21; ++Index) Nav->AdvanceDrive(0.1f, Context, false, false);
    TestEqual(TEXT("two-second spool enters transit"), Nav->GetNavigationState().Phase, ESPTravelPhase::JumpTransit);
    TestEqual(TEXT("jump burns seven fuel once"), Nav->GetNavigationState().FuelPercent, 1.0f);
    TestTrue(TEXT("jump heats drive"), FMath::IsNearlyEqual(Nav->GetNavigationState().HeatNormalized, 0.99f, 0.001f));
    TestFalse(TEXT("cannot switch to SCM in transit"), Nav->SetDriveMode(ESPTravelDriveMode::SCM));
    TestFalse(TEXT("cannot cancel paid transit"), Nav->CancelJump());
    TestEqual(TEXT("world stays Earth until route confirms exterior arrival"), Nav->GetNavigationState().CurrentWorldId, FString(TEXT("earth")));
    TestTrue(TEXT("external spatial route confirms arrival"), Nav->ConfirmJumpArrival());
    TestEqual(TEXT("arrival at selected source world"), Nav->GetNavigationState().CurrentWorldId, FString(TEXT("mars")));
    TestEqual(TEXT("arrival restores manual flight"), Nav->GetNavigationState().Phase, ESPTravelPhase::Flight);
    TestFalse(TEXT("duplicate arrival rejected"), Nav->ConfirmJumpArrival());
    TestTrue(TEXT("SCM selected after arrival"), Nav->SetDriveMode(ESPTravelDriveMode::SCM));
    TestFalse(TEXT("SCM does not inhibit weapons"), Nav->AreWeaponsInhibited());
    TestTrue(TEXT("drive cools below cruise thermal interlock"), Nav->SetDriveResources(1.0f, 0.5f));
    TestTrue(TEXT("cruise latch engages"), Nav->SetCruiseLatched(true, Context));
    for (int32 Index = 0; Index < 5; ++Index) Nav->AdvanceDrive(0.1f, Context, false, false);
    TestTrue(TEXT("cruise spools over three seconds"), Nav->GetNavigationState().CruiseSpool > 0.1f);
    Nav->AdvanceDrive(0.1f, Context, false, true);
    TestFalse(TEXT("brake releases cruise latch"), Nav->GetNavigationState().bCruiseLatched);
    TestTrue(TEXT("brake winds spool down"), Nav->GetNavigationState().CruiseSpool < 0.17f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPTravelLandingDockSaveTest,
    "SpacePatriot.Travel.LandingDockSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPTravelLandingDockSaveTest::RunTest(const FString& Parameters)
{
    USPTravelNavigationComponent* Nav = NewObject<USPTravelNavigationComponent>(GetTransientPackage());
    if (!TestTrue(TEXT("catalog loads"), Nav->LoadWorldCatalog())) return false;
    FSPTravelContext Context;
    Context.SurfaceClearanceMeters = 299.0;
    Context.SpeedMetersPerSecond = 79.0;
    Context.bSafeLandingFootprint = false;
    TestFalse(TEXT("unsafe surface footprint refused"), Nav->RequestSurfaceLanding(Context));
    Context.bSafeLandingFootprint = true;
    Context.SpeedMetersPerSecond = 80.0;
    TestFalse(TEXT("landing speed must be under eighty metres per second"), Nav->RequestSurfaceLanding(Context));
    Context.SpeedMetersPerSecond = 79.0;
    TestTrue(TEXT("safe surface away from port accepted"), Nav->RequestSurfaceLanding(Context));
    TestEqual(TEXT("landing request awaits physical touchdown"), Nav->GetNavigationState().Phase, ESPTravelPhase::LandingRequested);
    Context.bSafeLandingFootprint = false;
    TestFalse(TEXT("unsafe last contact cannot confirm"), Nav->ConfirmSurfaceTouchdown(Context));
    Context.bSafeLandingFootprint = true;
    TestTrue(TEXT("physical contact confirms landed"), Nav->ConfirmSurfaceTouchdown(Context));
    TestFalse(TEXT("ordinary surface cannot dock"), Nav->DockAtStation(Context));
    Context.bAtStationPad = true;
    Context.StationId = TEXT("earth-station");
    TestTrue(TEXT("station clamp after touchdown"), Nav->DockAtStation(Context));
    TestEqual(TEXT("dock phase"), Nav->GetNavigationState().Phase, ESPTravelPhase::Docked);
    const FSPTravelNavigationState Save = Nav->CaptureSaveState();
    USPTravelNavigationComponent* Restored = NewObject<USPTravelNavigationComponent>(GetTransientPackage());
    TestTrue(TEXT("valid nav state restores"), Restored->RestoreSaveState(Save));
    TestEqual(TEXT("station ID survives save"), Restored->GetNavigationState().DockedStationId, FString(TEXT("earth-station")));
    TestTrue(TEXT("undock retains landed surface"), Restored->Undock());
    TestEqual(TEXT("undock does not launch"), Restored->GetNavigationState().Phase, ESPTravelPhase::Landed);
    Context.bShipPowered = false;
    TestFalse(TEXT("unpowered launch blocked"), Restored->Launch(Context));
    Context.bShipPowered = true;
    TestTrue(TEXT("launch returns flight"), Restored->Launch(Context));

    FSPTravelNavigationState Invalid = Save;
    Invalid.CurrentWorldId = TEXT("not-a-world");
    TestFalse(TEXT("unknown world save rejected"), Restored->RestoreSaveState(Invalid));
    Invalid = Save;
    Invalid.DockedStationId.Empty();
    TestFalse(TEXT("dock without station rejected atomically"), Restored->RestoreSaveState(Invalid));
    TestEqual(TEXT("invalid restore left state untouched"), Restored->GetNavigationState().Phase, ESPTravelPhase::Flight);

    // Save during spool and transit, including the once-paid fuel debit.
    FSPTravelContext Space;
    Space.AltitudeKm = 3.0;
    Space.NearestStationDistanceKm = 6.0;
    TestTrue(TEXT("destination after launch"), Restored->SelectDestination(TEXT("trappist-1-f")));
    TestTrue(TEXT("spool after launch"), Restored->BeginJump(Space));
    for (int32 Index = 0; Index < 8; ++Index) Restored->AdvanceDrive(0.1f, Space, false, false);
    USPTravelNavigationComponent* Resume = NewObject<USPTravelNavigationComponent>(GetTransientPackage());
    TestTrue(TEXT("partial spool survives save"), Resume->RestoreSaveState(Restored->CaptureSaveState()));
    TestTrue(TEXT("partial charge survives"), Resume->GetJumpChargeFraction() > 0.3f);
    for (int32 Index = 0; Index < 13; ++Index) Resume->AdvanceDrive(0.1f, Space, false, false);
    TestEqual(TEXT("resumed spool reaches transit"), Resume->GetNavigationState().Phase, ESPTravelPhase::JumpTransit);
    USPTravelNavigationComponent* TransitResume = NewObject<USPTravelNavigationComponent>(GetTransientPackage());
    TestTrue(TEXT("paid transit survives save"), TransitResume->RestoreSaveState(Resume->CaptureSaveState()));
    const float PaidFuel = TransitResume->GetNavigationState().FuelPercent;
    TransitResume->AdvanceDrive(0.1f, Space, false, false);
    TestEqual(TEXT("resumed transit does not pay twice"), TransitResume->GetNavigationState().FuelPercent, PaidFuel);
    TestTrue(TEXT("resumed transit can arrive"), TransitResume->ConfirmJumpArrival());
    TestEqual(TEXT("resumed arrival world"), TransitResume->GetNavigationState().CurrentWorldId, FString(TEXT("trappist-1-f")));
    return true;
}
#endif
