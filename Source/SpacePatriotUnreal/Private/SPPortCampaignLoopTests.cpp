#include "SPPortTerminal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SpacePatriotBlueprintBases.h"
#include "SpacePatriotSystemsComponent.h"
#include "SPSocietySimulationComponent.h"
#include "SPStoryCampaignComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedPortWorld
    {
        UWorld* World = nullptr;

        FScopedPortWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("PortCampaignTest")),
                GetTransientPackage());
            if (!World)
            {
                GEngine->DestroyWorldContext(Context.World());
                return;
            }
            World->AddToRoot();
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
        }

        ~FScopedPortWorld()
        {
            if (!World) return;
            GEngine->ShutdownWorldNetDriver(World);
            World->DestroyWorld(true);
            World->SetPhysicsScene(nullptr);
            GEngine->DestroyWorldContext(World);
            World->RemoveFromRoot();
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPortCampaignLoopTest,
    "SpacePatriot.PlayLoop.PortCampaignAndFreight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPPortCampaignLoopTest::RunTest(const FString& Parameters)
{
    FScopedPortWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary game world"), World)) return false;

    ASPWorldRuntime* Runtime = World->SpawnActor<ASPWorldRuntime>();
    ASPPortTerminal* Port = World->SpawnActor<ASPPortTerminal>();
    ACharacter* Character = World->SpawnActor<ACharacter>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("runtime"), Runtime) || !TestNotNull(TEXT("physical port board"), Port) ||
        !TestNotNull(TEXT("on-foot character"), Character) || !TestNotNull(TEXT("player controller"), Controller)) return false;

    if (!TestTrue(TEXT("source world and creature catalogs load"), Runtime->Systems->LoadSourceCatalogs())) return false;
    const FSPCreatureRecord* MercuryAbility = Runtime->Systems->CreatureCatalog.FindByPredicate(
        [](const FSPCreatureRecord& Creature) { return Creature.Id == TEXT("mercury-wild-06"); });
    if (!TestNotNull(TEXT("Mercury creature with an ability"), MercuryAbility)) return false;
    TestEqual(TEXT("string ability from source roster is retained"), MercuryAbility->Ability,
        FString(TEXT("Spore fan")));
    if (!TestTrue(TEXT("Earth is active"), Runtime->Systems->SetActiveWorld(TEXT("earth")))) return false;
    USPSocietySimulationComponent* Society = NewObject<USPSocietySimulationComponent>(Runtime);
    USPStoryCampaignComponent* Campaign = NewObject<USPStoryCampaignComponent>(Runtime);
    Runtime->AddInstanceComponent(Society);
    Runtime->AddInstanceComponent(Campaign);
    Society->bAutoSave = false;
    Campaign->bAutoSave = false;
    Campaign->Society = Society;
    if (!TestTrue(TEXT("society initialized"), Society->InitializeSociety(false)) ||
        !TestTrue(TEXT("campaign initialized"), Campaign->InitializeCampaign(false))) return false;
    Society->State->HoldOrganics = 4;

    Port->SetActorLocation(FVector::ZeroVector);
    Character->SetActorLocation(FVector(100.0, 0.0, 100.0));
    Controller->Possess(Character);
    if (!TestTrue(TEXT("player is on foot beside the board"), Controller->GetPawn() == Character)) return false;

    if (!TestTrue(TEXT("case selector works at the port"), Port->TryCycleMission()) ||
        !TestEqual(TEXT("first case selected"), Port->SelectedQuestId, FString(TEXT("water")))) return false;
    if (!TestTrue(TEXT("another starting case can be selected"), Port->TryCycleMission()) ||
        !TestEqual(TEXT("second case selected"), Port->SelectedQuestId, FString(TEXT("orchard")))) return false;
    Port->TryCycleMission();
    Port->TryCycleMission();
    if (!TestTrue(TEXT("port starts selected case"), Port->TryMissionInteract()) ||
        !TestTrue(TEXT("single-choice dialogue advances"), Port->TryMissionInteract())) return false;

    FSPStoryNodeView Node;
    if (!TestTrue(TEXT("Earth terminal objective is active"), Campaign->GetCurrentNode(TEXT("water"), Node))) return false;
    TestEqual(TEXT("first objective type"), Node.ObjectiveKind, FString(TEXT("terminal")));
    if (!TestTrue(TEXT("F records the port objective"), Port->TryInteract())) return false;
    if (!TestTrue(TEXT("report dialogue advances"), Port->TryMissionInteract())) return false;
    if (!TestTrue(TEXT("Mars delivery objective is active"), Campaign->GetCurrentNode(TEXT("water"), Node))) return false;
    TestEqual(TEXT("delivery objective type"), Node.ObjectiveKind, FString(TEXT("delivery")));

    FSPMarketRecord BeforeMarket;
    Society->GetMarket(TEXT("mars"), BeforeMarket);
    Runtime->Systems->SetActiveWorld(TEXT("mars"));
    if (!TestTrue(TEXT("F at destination delivers real hold cargo"), Port->TryInteract())) return false;
    TestEqual(TEXT("delivery consumes exactly four organics"), Society->GetHoldCargo(TEXT("organics")), 0);
    FSPMarketRecord AfterMarket;
    Society->GetMarket(TEXT("mars"), AfterMarket);
    TestEqual(TEXT("delivered goods enter the destination market"), AfterMarket.Organics, BeforeMarket.Organics + 4.0f);
    if (!TestTrue(TEXT("case reaches its report after delivery"), Campaign->GetCurrentNode(TEXT("water"), Node))) return false;
    TestEqual(TEXT("case report follows physical delivery"), Node.Type, FString(TEXT("dialogue")));

    // The same reachable on-foot board operates the market and physical hold.
    const FString MarsDock = TEXT("mars:city:0");
    const int32 StartCredits = Society->GetPlayerCredits();
    TestTrue(TEXT("3 cycles the dock market to organics"), Port->TryCycleCargoGood());
    TestEqual(TEXT("selected market good"), Port->SelectedCargoGood, FString(TEXT("organics")));
    Society->State->HoldCapacity = Society->GetHoldUsed();
    TestFalse(TEXT("full hold allocation refuses a purchase"), Port->TryBuyCargo());
    TestEqual(TEXT("failed purchase leaves credits unchanged"), Society->GetPlayerCredits(), StartCredits);
    Society->State->HoldCapacity = 24;
    TestTrue(TEXT("4 buys one unit into dock staging"), Port->TryBuyCargo());
    TestEqual(TEXT("purchase stages cargo"), Society->GetStagedCargo(MarsDock, TEXT("organics")), 1);
    TestTrue(TEXT("purchase spends credits"), Society->GetPlayerCredits() < StartCredits);
    const int32 AfterPurchase = Society->GetPlayerCredits();
    Society->State->HoldCapacity = Society->GetHoldUsed();
    TestFalse(TEXT("5 cannot load into a full hold"), Port->TryLoadCargo());
    TestEqual(TEXT("failed load keeps cargo on the dock"), Society->GetStagedCargo(MarsDock, TEXT("organics")), 1);
    Society->State->HoldCapacity = 24;
    TestTrue(TEXT("5 loads one unit"), Port->TryLoadCargo());
    TestEqual(TEXT("ship holds purchased unit"), Society->GetHoldCargo(TEXT("organics")), 1);
    TestTrue(TEXT("6 unloads one unit"), Port->TryUnloadCargo());
    TestEqual(TEXT("unload stages the unit at the current port"), Society->GetStagedCargo(MarsDock, TEXT("organics")), 1);
    TestTrue(TEXT("7 sells one staged unit"), Port->TrySellCargo());
    TestEqual(TEXT("sale clears dock staging"), Society->GetStagedCargo(MarsDock, TEXT("organics")), 0);
    TestTrue(TEXT("sale grants credits"), Society->GetPlayerCredits() > AfterPurchase);

    // Sealed freight loads as a manifest, cannot be sold or unloaded early,
    // and can be partially unloaded by the player before F settles it.
    const FString ContractId = TEXT("mars:earth");
    TestTrue(TEXT("reachable Earth freight offer can be accepted"),
        Society->AcceptFreightContract(MarsDock, ContractId));
    TestTrue(TEXT("3 cycles to contract's crystal good"), Port->TryCycleCargoGood());
    TestEqual(TEXT("contract good selected"), Port->SelectedCargoGood, FString(TEXT("crystal")));
    TestFalse(TEXT("sealed staged freight cannot be sold"), Port->TrySellCargo());
    TestFalse(TEXT("sealed freight requires full manifest load with F"), Port->TryLoadCargo());
    TestTrue(TEXT("F loads the complete manifest into the ship"), Port->TryInteract());
    TestEqual(TEXT("four crystals in hold"), Society->GetHoldCargo(TEXT("crystal")), 4);
    TestFalse(TEXT("sealed freight cannot be unloaded at origin"), Port->TryUnloadCargo());
    const int32 BeforeReward = Society->GetPlayerCredits();
    TestTrue(TEXT("switch to the freight destination"), Runtime->Systems->SetActiveWorld(TEXT("earth")));
    TestTrue(TEXT("6 can unload part of shipment at destination"), Port->TryUnloadCargo());
    TestEqual(TEXT("one crystal staged at destination"),
        Society->GetStagedCargo(TEXT("earth:city:0"), TEXT("crystal")), 1);
    TestFalse(TEXT("sealed staged freight cannot be sold at destination"), Port->TrySellCargo());
    TestTrue(TEXT("F unloads remaining freight and completes delivery"), Port->TryInteract());
    TestEqual(TEXT("contract cargo leaves ship"), Society->GetHoldCargo(TEXT("crystal")), 0);
    TestEqual(TEXT("destination dock has no residual contract cargo"),
        Society->GetStagedCargo(TEXT("earth:city:0"), TEXT("crystal")), 0);
    TestEqual(TEXT("contract reward credited once"), Society->GetPlayerCredits(), BeforeReward + 480);
    TestFalse(TEXT("delivered contract cannot pay twice"), Society->DeliverFreightContract(TEXT("earth:city:0"), ContractId));
    TArray<uint8> SavedBytes;
    TestTrue(TEXT("existing society save format serializes cargo and freight"),
        UGameplayStatics::SaveGameToMemory(Society->State, SavedBytes) && SavedBytes.Num() > 0);

    // After a case objective has advanced, F still reaches the freight board.
    // Clear offers to make this a read-only fallback check with no save-slot writes.
    Society->State->FreightContracts.Reset();
    TestFalse(TEXT("no freight offer is handled"), Port->TryInteract());
    TestEqual(TEXT("freight fallback reports its own result"), Port->LastMessage,
        FString(TEXT("No freight contract is currently available at this port.")));
    return true;
}

#endif
