#include "SPInventoryComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInventoryFieldSourceParityTest,
    "SpacePatriot.Inventory.FieldSourceParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPInventoryFieldSourceParityTest::RunTest(const FString& Parameters)
{
    USPInventoryComponent* Kit = NewObject<USPInventoryComponent>(GetTransientPackage());
    if (!TestTrue(TEXT("field-inventory.js initializes from authoritative reserves"),
        Kit->InitializeFieldKit(90210, 90, 24, 120).bOk)) return false;
    TestEqual(TEXT("source starts with 18 recovered alloy"), Kit->Count(TEXT("alloy")), 18);
    TestEqual(TEXT("source rifle reserve"), Kit->Count(TEXT("rifle_ammo")), 90);
    TestEqual(TEXT("source sidearm reserve"), Kit->Count(TEXT("sidearm_ammo")), 24);
    TestEqual(TEXT("source repair spares"), Kit->Count(TEXT("spares")), 120);
    USPInventoryComponent* MercuryKit = NewObject<USPInventoryComponent>(GetTransientPackage());
    TestTrue(TEXT("source 32-bit unsigned world seed survives Blueprint API"),
        MercuryKit->InitializeFieldKit(3321527684LL, 0, 0, 0).bOk);
    TestEqual(TEXT("source seed saved without signed overflow"), MercuryKit->CaptureState().RandomState, 3321527684LL);
    TestTrue(TEXT("source weight formula"), FMath::IsNearlyEqual(Kit->CarriedWeightKg(), 7.47, 0.0001));
    TestEqual(TEXT("field backpack has 32 slots"), Kit->CaptureState().Backpack.Num(), 32);
    FString Error;
    TestTrue(TEXT("initial inventory validates"), Kit->ValidateState(Error));

    FSPFabricationContext Context;
    FSPInventoryResult Result = Kit->CraftAtTerminal(TEXT("ammo"), 1, Context);
    TestEqual(TEXT("cannot craft away from a city"), Result.Code, FString(TEXT("STATION")));
    Context.bInCity = true;
    Context.bOperationsFloor = true;
    Context.TerminalDistanceMeters = 3.01;
    Context.TerminalPower = 1;
    TestEqual(TEXT("more than 3 m rejects"), Kit->CraftAtTerminal(TEXT("ammo"), 1, Context).Code, FString(TEXT("STATION")));
    Context.TerminalDistanceMeters = 3;
    Context.bOperationsFloor = false;
    TestEqual(TEXT("wrong floor rejects"), Kit->CraftAtTerminal(TEXT("ammo"), 1, Context).Code, FString(TEXT("STATION")));
    Context.bOperationsFloor = true;
    Context.TerminalPower = 0.049;
    TestEqual(TEXT("Machineworks power below 0.05 rejects"), Kit->CraftAtTerminal(TEXT("ammo"), 1, Context).Code, FString(TEXT("POWER")));
    TestEqual(TEXT("failed terminal attempts did not spend alloy"), Kit->Count(TEXT("alloy")), 18);
    Context.TerminalPower = 0.05;
    TestTrue(TEXT("threshold power fabricates 30 rounds"), Kit->CraftAtTerminal(TEXT("ammo"), 1, Context).bOk);
    TestEqual(TEXT("ammunition reserve uses inventory"), Kit->Count(TEXT("rifle_ammo")), 120);
    TestEqual(TEXT("six alloy paid for ammunition"), Kit->Count(TEXT("alloy")), 12);
    TestTrue(TEXT("spares recipe succeeds"), Kit->CraftAtTerminal(TEXT("parts"), 1, Context).bOk);
    TestEqual(TEXT("four spares fabricated"), Kit->Count(TEXT("spares")), 124);
    TestEqual(TEXT("three alloy paid for spares"), Kit->Count(TEXT("alloy")), 9);
    TestFalse(TEXT("sample analysis cannot synthesize nonexistent samples"), Kit->CraftAtTerminal(TEXT("analyze"), 1, Context).bOk);
    TestEqual(TEXT("failed analysis conserves alloy"), Kit->Count(TEXT("alloy")), 9);
    TestTrue(TEXT("successful collection records two field samples"), Kit->AddItem(TEXT("sample"), 2).bOk);
    TestTrue(TEXT("sample analysis succeeds at powered terminal"), Kit->CraftAtTerminal(TEXT("analyze"), 1, Context).bOk);
    TestEqual(TEXT("one sample consumed"), Kit->Count(TEXT("sample")), 1);
    TestEqual(TEXT("four alloy recovered"), Kit->Count(TEXT("alloy")), 13);

    const FSPInventorySnapshot BeforePreview = Kit->CaptureState();
    TestTrue(TEXT("canCraft simulates recipe"), Kit->CanCraft(TEXT("ammo"), 1).bOk);
    TestEqual(TEXT("preview does not spend materials"), Kit->Count(TEXT("alloy")), 13);
    TestEqual(TEXT("preview does not advance revision"), Kit->GetRevision(), BeforePreview.Revision);
    const double CurrentWeight = Kit->CarriedWeightKg();
    TestTrue(TEXT("carry limit can be lowered to current weight"), Kit->SetWeightLimit(CurrentWeight).bOk);
    const int32 BeforeFailedCraft = Kit->GetRevision();
    TestFalse(TEXT("weight-increasing recipe rejects at current limit"), Kit->CraftAtTerminal(TEXT("ammo"), 1, Context).bOk);
    TestEqual(TEXT("overweight craft uses source code"), Kit->CanCraft(TEXT("ammo"), 1).Code, FString(TEXT("OVERWEIGHT")));
    TestEqual(TEXT("atomic failure retains alloy"), Kit->Count(TEXT("alloy")), 13);
    TestEqual(TEXT("atomic failure retains rounds"), Kit->Count(TEXT("rifle_ammo")), 120);
    TestEqual(TEXT("atomic failure retains revision"), Kit->GetRevision(), BeforeFailedCraft);
    TestTrue(TEXT("source valid state after failed craft"), Kit->ValidateState(Error));

    const FSPInventorySnapshot Good = Kit->CaptureState();
    FSPInventorySnapshot Bad = Good;
    Bad.Backpack[0] = Bad.Backpack[1];
    TestFalse(TEXT("malformed duplicate-slot save rejected"), Kit->RestoreState(Bad).bOk);
    TestEqual(TEXT("failed load rolls back"), Kit->CaptureState().Revision, Good.Revision);
    USPSInventorySaveGame* Save = Kit->CreateSaveGame();
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("native SaveGame serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    TestTrue(TEXT("field save is compact"), Bytes.Num() > 0 && Bytes.Num() < 64 * 1024);
    USPSInventorySaveGame* Loaded = Cast<USPSInventorySaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("native SaveGame loads"), Loaded)) return false;
    USPInventoryComponent* Reopened = NewObject<USPInventoryComponent>(GetTransientPackage());
    if (!TestTrue(TEXT("catalog reinitialized before restore"), Reopened->InitializeFieldKit(90210, 0, 0, 0).bOk)) return false;
    TestTrue(TEXT("native save restores"), Reopened->RestoreSaveGame(Loaded).bOk);
    TestEqual(TEXT("restored ammo reserve"), Reopened->Count(TEXT("rifle_ammo")), 120);
    TestEqual(TEXT("restored sample count"), Reopened->Count(TEXT("sample")), 1);
    TestEqual(TEXT("restored revision"), Reopened->GetRevision(), Good.Revision);
    TestTrue(TEXT("restored save validates"), Reopened->ValidateState(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInventoryEngineSourceParityTest,
    "SpacePatriot.Inventory.EngineSourceParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPInventoryEngineSourceParityTest::RunTest(const FString& Parameters)
{
    auto Def = [](const TCHAR* Id, const TCHAR* Name, const TCHAR* Type, const int32 MaxStack, const double Weight)
    {
        FSPInventoryDefinition Out;
        Out.Id = Id; Out.Name = Name; Out.Type = Type; Out.MaxStack = MaxStack; Out.WeightKg = Weight;
        return Out;
    };
    FSPInventoryDefinition Ore = Def(TEXT("ore"), TEXT("Ore"), TEXT("material"), 3, 0.5);
    FSPInventoryDefinition Sword = Def(TEXT("sword"), TEXT("Two-hand sword"), TEXT("weapon"), 1, 0.3);
    Sword.EquipSlot = TEXT("mainhand"); Sword.bTwoHanded = true; Sword.MaxDurability = 40;
    Sword.Stats.Add(TEXT("damage"), 12); Sword.Value = 100;
    FSPInventoryDefinition Shield = Def(TEXT("shield"), TEXT("Shield"), TEXT("armor"), 1, 0.2);
    Shield.EquipSlot = TEXT("offhand"); Shield.Stats.Add(TEXT("armor"), 5);
    FSPInventoryDefinition Potion = Def(TEXT("potion"), TEXT("Medgel"), TEXT("consumable"), 3, 0.1);
    Potion.HealthEffect = 25; Potion.CooldownSeconds = 5;
    FSPInventoryDefinition Quest = Def(TEXT("quest_id"), TEXT("Protected token"), TEXT("quest"), 1, 0);
    Quest.bQuest = true;
    Ore.Salvage.Add(TEXT("ore"), 1);
    FSPInventoryRecipe Recipe;
    Recipe.Id = TEXT("forge"); Recipe.Name = TEXT("Forge sword"); Recipe.Inputs.Add(TEXT("ore"), 2);
    Recipe.OutputId = TEXT("sword"); Recipe.OutputQuantity = 1; Recipe.GoldCost = 10;
    USPInventoryComponent* Inventory = NewObject<USPInventoryComponent>(GetTransientPackage());
    if (!TestTrue(TEXT("generic InventoryWorks catalog accepted"),
        Inventory->InitializeCatalog({Ore, Sword, Shield, Potion, Quest}, {Recipe}, 8, 4, 4, 10, 123).bOk)) return false;
    TestTrue(TEXT("four ore creates a stack of three plus one"), Inventory->AddItem(TEXT("ore"), 4).bOk);
    TestEqual(TEXT("stack quantity totals four"), Inventory->Count(TEXT("ore")), 4);
    const TArray<FSPInventoryItem> OreStacks = Inventory->GetContainerItems(TEXT("backpack"));
    TestEqual(TEXT("max stack splits into two instances"), OreStacks.Num(), 2);
    if (OreStacks.Num() < 2) return false;
    TestTrue(TEXT("first ore stack can lock"), Inventory->SetLocked(OreStacks[0].Uid, true).bOk);
    TestFalse(TEXT("locked item cannot be removed"), Inventory->RemoveItem(OreStacks[0].Uid, 1).bOk);
    TestEqual(TEXT("only one unlocked ore is visible to recipes"), Inventory->Count(TEXT("ore"), TEXT("backpack"), true), 1);
    TestEqual(TEXT("locked ore blocks recipe"), Inventory->CanCraft(TEXT("forge"), 1).Code, FString(TEXT("INGREDIENTS")));
    TestTrue(TEXT("unlock stack"), Inventory->SetLocked(OreStacks[0].Uid, false).bOk);
    TestTrue(TEXT("recipe spends material and gold"), Inventory->Craft(TEXT("forge"), 1).bOk);
    TestEqual(TEXT("two ore remain"), Inventory->Count(TEXT("ore")), 2);
    TestEqual(TEXT("source default gold 420 less recipe cost"), Inventory->GetPlayer().Gold, 410);
    const TArray<FSPInventoryItem> Backpack = Inventory->GetContainerItems(TEXT("backpack"));
    FString SwordUid;
    for (const FSPInventoryItem& Item : Backpack) if (Item.DefinitionId == TEXT("sword")) SwordUid = Item.Uid;
    if (!TestFalse(TEXT("crafted sword UID found"), SwordUid.IsEmpty())) return false;

    TestTrue(TEXT("shield enters backpack"), Inventory->AddItem(TEXT("shield"), 1).bOk);
    FString ShieldUid;
    for (const FSPInventoryItem& Item : Inventory->GetContainerItems(TEXT("backpack"))) if (Item.DefinitionId == TEXT("shield")) ShieldUid = Item.Uid;
    TestTrue(TEXT("offhand equips"), Inventory->Equip(ShieldUid).bOk);
    TestEqual(TEXT("equipped shield armor"), Inventory->EffectiveStats().FindRef(TEXT("armor")), 5.0);
    TestTrue(TEXT("two-handed sword equips and displaces shield"), Inventory->Equip(SwordUid).bOk);
    TestEqual(TEXT("sword boosts source base damage"), Inventory->EffectiveStats().FindRef(TEXT("damage")), 20.0);
    TestEqual(TEXT("offhand armor removed"), Inventory->EffectiveStats().FindRef(TEXT("armor")), 0.0);
    TestTrue(TEXT("shield stowed back in pack"), Inventory->GetContainerItems(TEXT("backpack")).ContainsByPredicate(
        [&](const FSPInventoryItem& Item) { return Item.Uid == ShieldUid; }));
    TestTrue(TEXT("broken equipment loses stats"), Inventory->DamageEquipment(TEXT("mainhand"), 40).bOk);
    TestEqual(TEXT("base damage after break"), Inventory->EffectiveStats().FindRef(TEXT("damage")), 8.0);
    TestTrue(TEXT("repair spends quarter of item value"), Inventory->Repair(SwordUid).bOk);
    TestEqual(TEXT("repair cost 25"), Inventory->GetPlayer().Gold, 385);
    TestEqual(TEXT("damage returns after repair"), Inventory->EffectiveStats().FindRef(TEXT("damage")), 20.0);

    TestTrue(TEXT("medgel enters backpack"), Inventory->AddItem(TEXT("potion"), 2).bOk);
    FString PotionUid;
    for (const FSPInventoryItem& Item : Inventory->GetContainerItems(TEXT("backpack"))) if (Item.DefinitionId == TEXT("potion")) PotionUid = Item.Uid;
    TestTrue(TEXT("medgel can be assigned to hotbar"), Inventory->AssignHotbar(0, PotionUid).bOk);
    TestEqual(TEXT("full health rejects consumption"), Inventory->ActivateHotbar(0).Code, FString(TEXT("FULL_RESOURCE")));
    TestTrue(TEXT("player damage applied"), Inventory->SetPlayerResources(50, 100, 385, 12).bOk);
    TestTrue(TEXT("hotbar uses medgel"), Inventory->ActivateHotbar(0).bOk);
    TestEqual(TEXT("medgel restores health"), Inventory->GetPlayer().Health, 75.0);
    TestEqual(TEXT("cooldown rejects immediate second use"), Inventory->ActivateHotbar(0).Code, FString(TEXT("COOLDOWN")));
    Inventory->AdvanceCooldowns(5);
    TestTrue(TEXT("cooldown expiry permits use"), Inventory->ActivateHotbar(0).bOk);
    TestEqual(TEXT("second medgel reaches full health"), Inventory->GetPlayer().Health, 100.0);
    TestTrue(TEXT("hotbar loses removed definition"), Inventory->CaptureState().Hotbar[0].IsEmpty());

    TestTrue(TEXT("protected quest item added"), Inventory->AddItem(TEXT("quest_id"), 1).bOk);
    FString QuestUid;
    for (const FSPInventoryItem& Item : Inventory->GetContainerItems(TEXT("backpack"))) if (Item.DefinitionId == TEXT("quest_id")) QuestUid = Item.Uid;
    TestEqual(TEXT("quest removal blocked"), Inventory->RemoveItem(QuestUid, 1).Code, FString(TEXT("QUEST")));
    TestTrue(TEXT("finite container transfer to stash"), Inventory->Transfer(QuestUid, TEXT("stash"), 1).bOk);
    TestEqual(TEXT("stash owns transferred quest item"), Inventory->Count(TEXT("quest_id"), TEXT("stash")), 1);
    TestTrue(TEXT("full-stack transfer keeps source UID"), Inventory->GetContainerItems(TEXT("stash")).ContainsByPredicate(
        [&](const FSPInventoryItem& Item) { return Item.Uid == QuestUid; }));
    FString Error;
    TestTrue(TEXT("equipment, cooldown and containers stay valid"), Inventory->ValidateState(Error));

    USPInventoryComponent* Small = NewObject<USPInventoryComponent>(GetTransientPackage());
    TestTrue(TEXT("small capacity catalog"), Small->InitializeCatalog({Ore}, {}, 1, 1, 1, 2, 456).bOk);
    TestTrue(TEXT("one full ore stack fills slot"), Small->AddItem(TEXT("ore"), 3).bOk);
    const int32 BeforeFailedAdd = Small->GetRevision();
    TestEqual(TEXT("second stack rejects full container"), Small->AddItem(TEXT("ore"), 1).Code, FString(TEXT("FULL")));
    TestEqual(TEXT("failed add did not change count"), Small->Count(TEXT("ore")), 3);
    TestEqual(TEXT("failed add did not change revision"), Small->GetRevision(), BeforeFailedAdd);
    TestTrue(TEXT("stack transfer frees backpack"), Small->Transfer(Small->GetContainerItems(TEXT("backpack"))[0].Uid, TEXT("stash"), 3).bOk);
    TestEqual(TEXT("backpack now empty"), Small->Count(TEXT("ore")), 0);
    TestEqual(TEXT("stash received three"), Small->Count(TEXT("ore"), TEXT("stash")), 3);
    TestTrue(TEXT("unbounded stash weight is not carried"), FMath::IsNearlyZero(Small->CarriedWeightKg()));
    TestTrue(TEXT("small state validates"), Small->ValidateState(Error));
    return true;
}
#endif
