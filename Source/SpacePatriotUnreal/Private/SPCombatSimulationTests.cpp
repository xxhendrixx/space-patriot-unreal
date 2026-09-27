#include "SPCombatSimulationComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPCombatSourceParityTest,
    "SpacePatriot.Combat.SourceParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPCombatSourceParityTest::RunTest(const FString& Parameters)
{
    USPCombatSimulationComponent* Combat = NewObject<USPCombatSimulationComponent>(GetTransientPackage());
    if (!TestNotNull(TEXT("combat simulation exists"), Combat)) return false;
    const struct { ESPCombatWeapon Weapon; float Speed; float Damage; int32 Mag; int32 Reserve; float Range; } Expected[] = {
        { ESPCombatWeapon::Kinetic, 1650, 14, 120, 960, 3200 },
        { ESPCombatWeapon::Laser, 0, 19, 100, 0, 2400 },
        { ESPCombatWeapon::Missile, 320, 105, 6, 12, 5000 },
        { ESPCombatWeapon::Rifle, 820, 22, 30, 180, 800 },
        { ESPCombatWeapon::Sidearm, 550, 29, 12, 72, 400 }
    };
    for (const auto& Row : Expected)
    {
        const FSPCombatWeaponSpec Spec = Combat->GetWeaponSpec(Row.Weapon);
        TestEqual(TEXT("source weapon speed in metres per second"), Spec.SpeedMetersPerSecond, Row.Speed);
        TestEqual(TEXT("source weapon damage"), Spec.Damage, Row.Damage);
        TestEqual(TEXT("source magazine capacity"), Spec.MagazineCapacity, Row.Mag);
        TestEqual(TEXT("source reserve capacity"), Spec.ReserveCapacity, Row.Reserve);
        TestEqual(TEXT("source weapon range in metres"), Spec.RangeMeters, Row.Range);
    }

    FSPCombatStepInput Input;
    TestFalse(TEXT("safe gun does not fire"), Combat->TryFire(Input));
    TestTrue(TEXT("SCM gun can arm"), Combat->SetArmed(true, Input));
    TestTrue(TEXT("K-28 fires"), Combat->TryFire(Input));
    TestEqual(TEXT("shot consumes one kinetic round"), Combat->GetTelemetry(Input).Ammo.Magazine, 119);
    TestFalse(TEXT("cadence gates immediate second shot"), Combat->TryFire(Input));
    TestTrue(TEXT("reload starts with partial magazine"), Combat->RequestReload(Input));
    TestFalse(TEXT("reload blocks firing"), Combat->TryFire(Input));
    for (int32 Index = 0; Index < 28; ++Index) Combat->StepCombat(0.1f, Input);
    TestEqual(TEXT("reload fills only missing round"), Combat->GetTelemetry(Input).Ammo.Magazine, 120);
    TestEqual(TEXT("reload conserves reserve"), Combat->GetTelemetry(Input).Ammo.Reserve, 959);
    TestFalse(TEXT("full magazine cannot reload"), Combat->RequestReload(Input));

    Combat->SelectWeapon(ESPCombatWeapon::Laser);
    Combat->StepCombat(0.1f, Input);
    Combat->StepCombat(0.1f, Input);
    TestTrue(TEXT("L-9 pulse can fire without magazine consumption"), Combat->TryFire(Input));
    const FSPCombatTelemetry Laser = Combat->GetTelemetry(Input);
    TestEqual(TEXT("laser uses capacitor 12"), Laser.Capacitor, 88.0f);
    TestEqual(TEXT("laser nominal magazine is not ammunition"), Laser.Ammo.Magazine, 100);
    TestFalse(TEXT("laser cannot reload"), Combat->RequestReload(Input));
    Combat->SelectWeapon(ESPCombatWeapon::Rifle);
    Input.bOnFoot = true;
    Combat->StepCombat(0.1f, Input);
    Combat->StepCombat(0.1f, Input);
    TestTrue(TEXT("on-foot rifle fires"), Combat->TryFire(Input));
    TestEqual(TEXT("rifle magazine independently consumed"), Combat->GetTelemetry(Input).Ammo.Magazine, 29);

    FSPCombatSnapshot Snapshot = Combat->CaptureState();
    Snapshot.Ammo[0].Reserve = 961;
    TestFalse(TEXT("over-capacity save rejected"), Combat->RestoreState(Snapshot));
    TestEqual(TEXT("failed restore leaves live ammo untouched"), Combat->GetTelemetry(Input).Ammo.Magazine, 29);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPCombatTargetingDamageTest,
    "SpacePatriot.Combat.TargetingDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPCombatTargetingDamageTest::RunTest(const FString& Parameters)
{
    USPCombatSimulationComponent* Combat = NewObject<USPCombatSimulationComponent>(GetTransientPackage());
    FSPCombatStepInput Input;
    Combat->SetArmed(true, Input);
    FSPCombatContact Raider;
    Raider.Id = TEXT("raider"); Raider.Name = TEXT("RAIDER 01");
    Raider.PositionMeters = FVector(500, 0, 0); Raider.AnchorMeters = Raider.PositionMeters;
    Raider.RadiusMeters = 14; Raider.Shield = 40; Raider.Hull = 100;
    TestTrue(TEXT("valid hostile contact accepted"), Combat->AddContact(Raider));
    TestFalse(TEXT("duplicate contact rejected"), Combat->AddContact(Raider));
    Combat->SetTargetId(TEXT("raider"));
    Combat->SelectWeapon(ESPCombatWeapon::Missile);
    const FSPCombatFiringSolution Solution = Combat->GetFiringSolution(Input);
    TestTrue(TEXT("stationary target has reachable intercept"), Solution.bReachable);
    TestTrue(TEXT("missile intercept time uses source speed"), FMath::IsNearlyEqual(Solution.TimeSeconds, 500.0f / 320.0f, 0.001f));
    TestFalse(TEXT("missile cannot fire before lock"), Combat->TryFire(Input));
    for (int32 Index = 0; Index < 16; ++Index) Combat->StepCombat(0.1f, Input);
    TestTrue(TEXT("aimed seeker reaches full lock"), FMath::IsNearlyEqual(Combat->GetTelemetry(Input).Lock01, 1.0f));
    TestTrue(TEXT("locked missile fires"), Combat->TryFire(Input));
    TestEqual(TEXT("missile leaves five in magazine"), Combat->GetTelemetry(Input).Ammo.Magazine, 5);
    for (int32 Index = 0; Index < 20; ++Index) Combat->StepCombat(0.1f, Input);
    const TArray<FSPCombatContact> Contacts = Combat->GetContacts();
    TestTrue(TEXT("missile consumes all 40 shield first"), FMath::IsNearlyZero(Contacts[0].Shield));
    TestTrue(TEXT("remaining 65 damage leaves 35 hull"), FMath::IsNearlyEqual(Contacts[0].Hull, 35.0f));
    TestEqual(TEXT("missile removed after impact"), Combat->GetProjectiles().Num(), 0);
    Combat->SelectWeapon(ESPCombatWeapon::Laser);
    Combat->StepCombat(0.1f, Input);
    TestTrue(TEXT("laser hitscan fires"), Combat->TryFire(Input));
    TestTrue(TEXT("laser hits same target after shield breaks"), FMath::IsNearlyEqual(Combat->GetContacts()[0].Hull, 16.0f));
    Combat->StepCombat(0.1f, Input);
    Combat->StepCombat(0.1f, Input);
    TestTrue(TEXT("second laser shot fires after cadence"), Combat->TryFire(Input));
    TestEqual(TEXT("destroyed target gives one kill"), Combat->GetTelemetry(Input).Kills, 1);
    TestEqual(TEXT("destroyed target leaves zero hull"), Combat->GetContacts()[0].Hull, 0.0f);
    TestTrue(TEXT("target selection clears on kill"), Combat->GetTelemetry(Input).TargetId.IsEmpty());
    Combat->StepCombat(0.1f, Input);
    const TArray<FSPCombatEvent> Events = Combat->DrainEvents();
    TestEqual(TEXT("sector-clear event emitted once"), Events.FilterByPredicate([](const FSPCombatEvent& Event) { return Event.Type == ESPCombatEventType::SectorClear; }).Num(), 1);
    Combat->StepCombat(0.1f, Input);
    TestEqual(TEXT("sector-clear event does not repeat"), Combat->DrainEvents().Num(), 0);

    Combat->ApplyPlayerDamage(30.0f, false);
    TestTrue(TEXT("ship shield takes hit before hull"), FMath::IsNearlyEqual(Combat->GetTelemetry(Input).Shield, 70.0f));
    TestEqual(TEXT("hull unharmed while shield remains"), Combat->GetTelemetry(Input).Hull, 100.0f);
    Combat->ApplyPlayerDamage(80.0f, false);
    TestEqual(TEXT("shield exhausted"), Combat->GetTelemetry(Input).Shield, 0.0f);
    TestEqual(TEXT("overflow reaches hull"), Combat->GetTelemetry(Input).Hull, 90.0f);
    for (int32 Index = 0; Index < 61; ++Index) Combat->StepCombat(0.1f, Input);
    TestTrue(TEXT("shield regenerates only after six-second no-damage delay"), Combat->GetTelemetry(Input).Shield > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPCombatAuthorityAndSaveTest,
    "SpacePatriot.Combat.AuthorityAndSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPCombatAuthorityAndSaveTest::RunTest(const FString& Parameters)
{
    USPCombatSimulationComponent* A = NewObject<USPCombatSimulationComponent>(GetTransientPackage());
    USPCombatSimulationComponent* B = NewObject<USPCombatSimulationComponent>(GetTransientPackage());
    FSPCombatStepInput Input;
    TestTrue(TEXT("authoritative space sortie starts"), A->StartSortie(false, Input));
    TestTrue(TEXT("second identical sortie starts"), B->StartSortie(false, Input));
    TestEqual(TEXT("source space sortie spawns five raiders"), A->GetContacts().Num(), 5);
    for (int32 Index = 0; Index < 60; ++Index) { A->StepCombat(0.1f, Input); B->StepCombat(0.1f, Input); }
    const TArray<FSPCombatContact> ContactsA = A->GetContacts();
    const TArray<FSPCombatContact> ContactsB = B->GetContacts();
    for (int32 Index = 0; Index < ContactsA.Num(); ++Index)
        TestTrue(TEXT("raider patrol remains deterministic across peers"), ContactsA[Index].PositionMeters.Equals(ContactsB[Index].PositionMeters, 0.001));
    TestEqual(TEXT("host AI fires same number of bolts"), A->GetProjectiles().Num(), B->GetProjectiles().Num());
    TestTrue(TEXT("host AI eventually fires"), A->GetProjectiles().Num() > 0);

    const FSPCombatSnapshot Snapshot = A->CaptureState();
    USPCombatSimulationComponent* Restored = NewObject<USPCombatSimulationComponent>(GetTransientPackage());
    TestTrue(TEXT("compact simulation snapshot restores"), Restored->RestoreState(Snapshot));
    TestEqual(TEXT("snapshot restores contact count"), Restored->GetContacts().Num(), 5);
    TestEqual(TEXT("snapshot restores projectile count"), Restored->GetProjectiles().Num(), A->GetProjectiles().Num());
    TestEqual(TEXT("snapshot restores target"), Restored->GetTelemetry(Input).TargetId, A->GetTelemetry(Input).TargetId);
    FSPCombatSnapshot Corrupt = Snapshot;
    Corrupt.Ammo[1].Weapon = ESPCombatWeapon::Kinetic;
    TestFalse(TEXT("duplicate weapon save rejected"), Restored->RestoreState(Corrupt));
    TestEqual(TEXT("failed restore preserves contacts"), Restored->GetContacts().Num(), 5);

    Input.bAuthority = false;
    USPCombatSimulationComponent* Client = NewObject<USPCombatSimulationComponent>(GetTransientPackage());
    TestFalse(TEXT("client cannot start authoritative sortie"), Client->StartSortie(false, Input));
    TestTrue(TEXT("client can hydrate host snapshot"), Client->RestoreState(Snapshot));
    const int32 ClientCount = Client->GetProjectiles().Num();
    Client->StepCombat(0.1f, Input);
    TestTrue(TEXT("client does not create AI fire"), Client->GetProjectiles().Num() <= ClientCount);
    TestFalse(TEXT("service blocked in flight"), A->Service(false, false));
    TestTrue(TEXT("service permitted while docked"), A->Service(true, false));
    TestEqual(TEXT("service restores shield"), A->GetTelemetry(FSPCombatStepInput()).Shield, 100.0f);

    USPCombatSimulationComponent* Thermal = NewObject<USPCombatSimulationComponent>(GetTransientPackage());
    FSPCombatStepInput HeatInput;
    Thermal->SetArmed(true, HeatInput);
    HeatInput.CoolerFactor = 0.0f;
    HeatInput.bTrigger = true;
    for (int32 Index = 0; Index < 100; ++Index) Thermal->StepCombat(0.1f, HeatInput);
    TestTrue(TEXT("sustained fire with failed cooler trips overheat"), Thermal->GetTelemetry(HeatInput).bOverheated);
    HeatInput.bTrigger = false;
    HeatInput.CoolerFactor = 1.0f;
    for (int32 Index = 0; Index < 55; ++Index) Thermal->StepCombat(0.1f, HeatInput);
    TestFalse(TEXT("cooling below 35 percent clears thermal lockout"), Thermal->GetTelemetry(HeatInput).bOverheated);
    FSPCombatStepInput TurretInput;
    TurretInput.bTurretSeat = true;
    TurretInput.bSCMMode = false;
    TestFalse(TEXT("turret cannot arm in NAV mode"), Thermal->SetArmed(true, TurretInput));
    return true;
}
#endif
