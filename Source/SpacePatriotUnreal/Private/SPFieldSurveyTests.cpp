#include "SPFieldSurveyComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPFieldSurveySourceParityTest,
    "SpacePatriot.FieldSurvey.SourceParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPFieldSurveySourceParityTest::RunTest(const FString& Parameters)
{
    USPFieldSurveyComponent* Survey = NewObject<USPFieldSurveyComponent>(GetTransientPackage());
    Survey->bAutoSave = false;
    if (!TestTrue(TEXT("source world catalog loads"), Survey->InitializeSurveys(false))) return false;
    TestEqual(TEXT("all nineteen source worlds are available"), Survey->GetWorldCount(), 19);
    TestFalse(TEXT("unknown world is rejected"), Survey->ActivateWorld(TEXT("made-up-planet"), FVector::ZeroVector));

    // A scan made before reaching the field site stays with that world, as in
    // flight.ship.scanned, and resolves after the walking arrival event.
    TestFalse(TEXT("failed scanner action has no effect"), Survey->NotifyWorldScanned(TEXT("earth"), false));
    TestTrue(TEXT("successful pre-arrival scan is remembered"), Survey->NotifyWorldScanned(TEXT("SP-Earth"), true));
    TestTrue(TEXT("Earth activates at an authoritative survey site"), Survey->ActivateWorld(TEXT("Earth"), FVector::ZeroVector));
    FSPFieldSurveyStatus Status;
    TestTrue(TEXT("Earth has source status"), Survey->GetCurrentStatus(Status));
    TestEqual(TEXT("source profile"), Status.Profile, FString(TEXT("Living watershed")));
    TestEqual(TEXT("source arrival title"), Status.Title, FString(TEXT("Reach the survey site on foot")));
    TestFalse(TEXT("ship cannot trigger arrival"), Survey->UpdatePlayerContext(FVector::ZeroVector, false, false));
    TestFalse(TEXT("bridge walk cannot trigger arrival"), Survey->UpdatePlayerContext(FVector::ZeroVector, true, true));
    TestFalse(TEXT("more than 300 m does not trigger arrival"), Survey->UpdatePlayerContext(FVector(30001, 0, 0), true, false));
    TestTrue(TEXT("on-foot arrival inside 300 m resolves briefing and pre-scan"), Survey->UpdatePlayerContext(FVector(29999, 0, 0), true, false));
    TestTrue(TEXT("source status remains queryable"), Survey->GetCurrentStatus(Status));
    TestEqual(TEXT("solid world advances to sample"), Status.Phase, FString(TEXT("sample")));
    TestEqual(TEXT("distance converts centimetres to metres"), Status.DistanceMeters, 299.99);
    TestEqual(TEXT("briefing journal once"), Status.Journal.Num(), 1);
    TestEqual(TEXT("briefing uses original description"), Status.Journal[0].Text, Status.Description);
    TArray<FSPFieldBeaconPulse> Pulses = Survey->TakePendingBeaconPulses();
    TestEqual(TEXT("scan triggered one beacon"), Pulses.Num(), 1);
    if (Pulses.Num() == 1)
    {
        TestEqual(TEXT("Earth beacon preset"), Pulses[0].Preset, FString(TEXT("arcane")));
        TestEqual(TEXT("Earth beacon site"), Pulses[0].Position, FVector::ZeroVector);
    }
    TestFalse(TEXT("duplicate scan cannot emit again"), Survey->NotifyWorldScanned(TEXT("earth"), true));
    TestFalse(TEXT("failed sample cannot complete"), Survey->NotifyFieldSample(TEXT("earth"), TEXT("surface-sampler"), false));
    TestFalse(TEXT("unrelated inventory event cannot complete"), Survey->NotifyFieldSample(TEXT("earth"), TEXT("station-cargo"), true));
    TestFalse(TEXT("sample away from site cannot complete"), Survey->UpdatePlayerContext(FVector(40000, 0, 0), true, false));
    TestFalse(TEXT("out-of-range sampler cannot complete"), Survey->NotifyFieldSample(TEXT("earth"), TEXT("surface-sampler"), true));
    TestFalse(TEXT("ship sample cannot complete"), Survey->UpdatePlayerContext(FVector::ZeroVector, false, false));
    TestFalse(TEXT("ship sample rejected"), Survey->NotifyFieldSample(TEXT("earth"), TEXT("surface-sampler"), true));
    TestFalse(TEXT("bridge walk does not change phase"), Survey->UpdatePlayerContext(FVector::ZeroVector, true, true));
    TestFalse(TEXT("bridge walk sample rejected"), Survey->NotifyFieldSample(TEXT("earth"), TEXT("surface-sampler"), true));
    TestFalse(TEXT("walking refresh does not repeat arrival"), Survey->UpdatePlayerContext(FVector::ZeroVector, true, false));
    TestTrue(TEXT("successful near-site sampler completes"), Survey->NotifyFieldSample(TEXT("earth"), TEXT("surface-sampler"), true));
    TestTrue(TEXT("Earth status reads complete"), Survey->GetCurrentStatus(Status));
    TestEqual(TEXT("Earth survey complete phase"), Status.Phase, FString(TEXT("complete")));
    TestTrue(TEXT("completion flag persisted"), Status.bComplete);
    TestEqual(TEXT("briefing plus archive journal"), Status.Journal.Num(), 2);
    TestEqual(TEXT("source archive journal"), Status.Journal[1].Text,
        FString(TEXT("Field report archived. Survey data and cargo can be sold at an orbital station.")));
    TestFalse(TEXT("completion cannot repeat"), Survey->NotifyFieldSample(TEXT("earth"), TEXT("surface-sampler"), true));

    // Source type 3 surveys are at orbital habitats and finish at the uplink;
    // type 4 sends Spellworks the frost preset before a field sample.
    TestTrue(TEXT("gas world activates"), Survey->ActivateWorld(TEXT("jupiter"), FVector(100000, 0, 0)));
    TestTrue(TEXT("gas habitat arrival"), Survey->UpdatePlayerContext(FVector(100000, 0, 0), true, false));
    TestTrue(TEXT("gas scan accepted"), Survey->NotifyWorldScanned(TEXT("jupiter"), true));
    TestTrue(TEXT("gas status"), Survey->GetCurrentStatus(Status));
    TestEqual(TEXT("gas world skips sample"), Status.Phase, FString(TEXT("complete")));
    TestEqual(TEXT("gas journal archives"), Status.Journal.Num(), 2);
    TestFalse(TEXT("gas world does not accept a sample"), Survey->NotifyFieldSample(TEXT("jupiter"), TEXT("surface-sampler"), true));

    TestTrue(TEXT("glacial world activates"), Survey->ActivateWorld(TEXT("trappist-1-f"), FVector(200000, 0, 0)));
    TestTrue(TEXT("glacial arrival"), Survey->UpdatePlayerContext(FVector(200000, 0, 0), true, false));
    TestTrue(TEXT("glacial scan"), Survey->NotifyWorldScanned(TEXT("trappist-1-f"), true));
    TestTrue(TEXT("glacial status"), Survey->GetCurrentStatus(Status));
    TestEqual(TEXT("glacial profile"), Status.Profile, FString(TEXT("Glacial ridges")));
    TestEqual(TEXT("glacial world still needs sample"), Status.Phase, FString(TEXT("sample")));
    Pulses = Survey->TakePendingBeaconPulses();
    TestEqual(TEXT("gas and glacial beacons queued"), Pulses.Num(), 2);
    if (Pulses.Num() == 2)
    {
        TestEqual(TEXT("gas uses arcane"), Pulses[0].Preset, FString(TEXT("arcane")));
        TestEqual(TEXT("glacial uses frost"), Pulses[1].Preset, FString(TEXT("frost")));
    }
    TestTrue(TEXT("Earth's record survives changing worlds"), Survey->GetStatus(TEXT("earth"), Status));
    TestTrue(TEXT("Earth remains complete"), Status.bComplete);

    // Source visuals retain no more than eight pulses across successive worlds.
    const TArray<FString> ExtraWorlds = {
        TEXT("mercury"), TEXT("venus"), TEXT("mars"), TEXT("saturn"), TEXT("uranus"),
        TEXT("neptune"), TEXT("trappist-1-b"), TEXT("trappist-1-c"), TEXT("trappist-1-d")
    };
    for (int32 Index = 0; Index < ExtraWorlds.Num(); ++Index)
    {
        const FVector Site(300000.0 + Index * 100000.0, 0, 0);
        if (!TestTrue(TEXT("another source world activates"), Survey->ActivateWorld(ExtraWorlds[Index], Site))) return false;
        if (!TestTrue(TEXT("on-foot arrival in another world"), Survey->UpdatePlayerContext(Site, true, false))) return false;
        if (!TestTrue(TEXT("its own scan resolves"), Survey->NotifyWorldScanned(ExtraWorlds[Index], true))) return false;
    }
    Pulses = Survey->TakePendingBeaconPulses();
    TestEqual(TEXT("beacon queue keeps only eight newest pulses"), Pulses.Num(), 8);
    if (Pulses.Num() == 8)
    {
        TestEqual(TEXT("oldest overflow pulse was discarded"), Pulses[0].WorldId, FString(TEXT("venus")));
        TestEqual(TEXT("newest pulse remains"), Pulses.Last().WorldId, FString(TEXT("trappist-1-d")));
    }

    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("survey state serializes"), UGameplayStatics::SaveGameToMemory(Survey->State, Bytes))) return false;
    TestTrue(TEXT("survey save stays compact"), Bytes.Num() > 0 && Bytes.Num() < 64 * 1024);
    USPSFieldSurveySaveGame* Restored = Cast<USPSFieldSurveySaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("survey state deserializes"), Restored)) return false;
    USPFieldSurveyComponent* Reloaded = NewObject<USPFieldSurveyComponent>(GetTransientPackage());
    Reloaded->bAutoSave = false;
    if (!TestTrue(TEXT("catalog reloads"), Reloaded->InitializeSurveys(false))) return false;
    Reloaded->State = Restored;
    TestTrue(TEXT("restored Earth status"), Reloaded->GetStatus(TEXT("earth"), Status));
    TestTrue(TEXT("restored completion"), Status.bComplete);
    TestEqual(TEXT("restored journal"), Status.Journal.Num(), 2);
    TestTrue(TEXT("revisit Earth"), Reloaded->ActivateWorld(TEXT("earth"), FVector::ZeroVector));
    TestFalse(TEXT("revisit does not repeat arrival"), Reloaded->UpdatePlayerContext(FVector::ZeroVector, true, false));
    TestEqual(TEXT("reload does not replay beacon"), Reloaded->TakePendingBeaconPulses().Num(), 0);
    TestTrue(TEXT("restore glacial survey"), Reloaded->GetStatus(TEXT("trappist-1-f"), Status));
    TestEqual(TEXT("uncompleted glacial phase is preserved"), Status.Phase, FString(TEXT("sample")));
    return true;
}
#endif
