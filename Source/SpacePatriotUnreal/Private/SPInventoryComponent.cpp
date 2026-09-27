#include "SPInventoryComponent.h"

#include "UObject/UObjectGlobals.h"

namespace
{
    const TArray<FString>& EquipmentSlots()
    {
        static const TArray<FString> Slots = {
            TEXT("head"), TEXT("body"), TEXT("hands"), TEXT("feet"),
            TEXT("mainhand"), TEXT("offhand"), TEXT("amulet"), TEXT("ring")
        };
        return Slots;
    }

    const TArray<FString>& StatNames()
    {
        static const TArray<FString> Names = {
            TEXT("damage"), TEXT("armor"), TEXT("magic"), TEXT("haste"),
            TEXT("luck"), TEXT("maxHealth"), TEXT("maxMana")
        };
        return Names;
    }

    bool ValidContainerSize(const int32 Size) { return Size >= 1 && Size <= 512; }
    bool FiniteRange(const double Value, const double Min, const double Max)
    {
        return FMath::IsFinite(Value) && Value >= Min && Value <= Max;
    }
}

USPInventoryComponent::USPInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

FSPInventoryResult USPInventoryComponent::Success(const FString& DefinitionId, const int32 Quantity, const FString& Uid)
{
    FSPInventoryResult Result;
    Result.bOk = true;
    Result.Code = TEXT("OK");
    Result.DefinitionId = DefinitionId;
    Result.Quantity = Quantity;
    Result.ItemUid = Uid;
    if (!Uid.IsEmpty()) Result.ItemUids.Add(Uid);
    return Result;
}

FSPInventoryResult USPInventoryComponent::Failure(const FString& Code, const FString& Message)
{
    FSPInventoryResult Result;
    Result.Code = Code;
    Result.Message = Message;
    return Result;
}

bool USPInventoryComponent::ValidId(const FString& Id)
{
    if (Id.IsEmpty() || Id.Len() > 64 || Id == TEXT("constructor") || Id == TEXT("prototype") || Id == TEXT("__proto__")) return false;
    if (Id[0] < TEXT('a') || Id[0] > TEXT('z')) return false;
    for (const TCHAR Ch : Id)
        if (!((Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_') || Ch == TEXT('-'))) return false;
    return true;
}

bool USPInventoryComponent::ValidSlot(const FString& Slot) { return EquipmentSlots().Contains(Slot); }

const FSPInventoryDefinition* USPInventoryComponent::Definition(const FString& Id) const { return Definitions.Find(Id); }

const FSPInventoryItem* USPInventoryComponent::FindItem(const FString& Uid) const
{
    return State.Items.FindByPredicate([&](const FSPInventoryItem& Item) { return Item.Uid == Uid; });
}

FSPInventoryItem* USPInventoryComponent::FindItem(const FString& Uid)
{
    return State.Items.FindByPredicate([&](const FSPInventoryItem& Item) { return Item.Uid == Uid; });
}

const TArray<FString>* USPInventoryComponent::Slots(const FString& Container) const
{
    if (Container == TEXT("backpack")) return &State.Backpack;
    if (Container == TEXT("stash")) return &State.Stash;
    if (Container == TEXT("loot")) return &State.Loot;
    return nullptr;
}

TArray<FString>* USPInventoryComponent::Slots(const FString& Container)
{
    return const_cast<TArray<FString>*>(static_cast<const USPInventoryComponent*>(this)->Slots(Container));
}

bool USPInventoryComponent::Locate(const FString& Uid, FString& ContainerOrSlot, int32& Index) const
{
    for (const FString Container : {TEXT("backpack"), TEXT("stash"), TEXT("loot")})
    {
        const TArray<FString>* List = Slots(Container);
        Index = List->Find(Uid);
        if (Index != INDEX_NONE) { ContainerOrSlot = Container; return true; }
    }
    for (const FString& Slot : EquipmentSlots())
        if (State.Equipment.FindRef(Slot) == Uid) { ContainerOrSlot = Slot; Index = INDEX_NONE; return true; }
    ContainerOrSlot.Empty();
    Index = INDEX_NONE;
    return false;
}

void USPInventoryComponent::Unlink(const FString& Uid)
{
    FString Location;
    int32 Index;
    if (!Locate(Uid, Location, Index)) return;
    if (TArray<FString>* List = Slots(Location)) (*List)[Index].Empty();
    else State.Equipment.FindOrAdd(Location).Empty();
}

FSPInventoryResult USPInventoryComponent::InitializeCatalog(const TArray<FSPInventoryDefinition>& InDefinitions,
    const TArray<FSPInventoryRecipe>& InRecipes, const int32 BackpackSize, const int32 StashSize,
    const int32 LootSize, const double WeightLimitKg, const int64 RandomSeed)
{
    if (InDefinitions.IsEmpty() || InDefinitions.Num() > 2048 || InRecipes.Num() > 2048 ||
        !ValidContainerSize(BackpackSize) || !ValidContainerSize(StashSize) || !ValidContainerSize(LootSize) ||
        !FiniteRange(WeightLimitKg, 0.1, 100000) || RandomSeed < 1 || RandomSeed > 0xffffffffLL)
        return Failure(TEXT("INVALID"), TEXT("Invalid catalog size, capacity, weight limit or seed."));

    TMap<FString, FSPInventoryDefinition> NewDefinitions;
    for (const FSPInventoryDefinition& Def : InDefinitions)
    {
        if (!ValidId(Def.Id) || NewDefinitions.Contains(Def.Id) || Def.Name.IsEmpty() || Def.Name.Len() > 100 ||
            !TArray<FString>{TEXT("weapon"), TEXT("armor"), TEXT("accessory"), TEXT("consumable"), TEXT("material"), TEXT("quest")}.Contains(Def.Type) ||
            !TArray<FString>{TEXT("common"), TEXT("uncommon"), TEXT("rare"), TEXT("epic"), TEXT("legendary")}.Contains(Def.Rarity) ||
            !FiniteRange(Def.WeightKg, 0, 10000) || Def.Value < 0 || Def.Value > 10000000 ||
            Def.MaxStack < 1 || Def.MaxStack > 9999 || Def.MaxDurability < 0 || Def.MaxDurability > 100000 ||
            !FiniteRange(Def.HealthEffect, 0, 100000) || !FiniteRange(Def.ManaEffect, 0, 100000) ||
            !FiniteRange(Def.CooldownSeconds, 0, 3600) ||
            (!Def.EquipSlot.IsEmpty() && (!ValidSlot(Def.EquipSlot) || Def.MaxStack != 1)) ||
            (Def.bTwoHanded && Def.EquipSlot != TEXT("mainhand")) ||
            (Def.bQuest && Def.Type != TEXT("quest")) ||
            ((Def.HealthEffect > 0 || Def.ManaEffect > 0) && Def.Type != TEXT("consumable")))
            return Failure(TEXT("INVALID"), FString::Printf(TEXT("Invalid definition: %s"), *Def.Id));
        for (const auto& Pair : Def.Stats)
            if (!StatNames().Contains(Pair.Key) || !FiniteRange(Pair.Value, -1000, 100000))
                return Failure(TEXT("INVALID"), FString::Printf(TEXT("Invalid stat on %s"), *Def.Id));
        NewDefinitions.Add(Def.Id, Def);
    }
    for (const auto& Pair : NewDefinitions)
        for (const auto& Salvage : Pair.Value.Salvage)
            if (!NewDefinitions.Contains(Salvage.Key) || Salvage.Value < 1 || Salvage.Value > 9999)
                return Failure(TEXT("INVALID"), FString::Printf(TEXT("Invalid salvage output on %s"), *Pair.Key));

    TMap<FString, FSPInventoryRecipe> NewRecipes;
    for (const FSPInventoryRecipe& Recipe : InRecipes)
    {
        if (!ValidId(Recipe.Id) || NewRecipes.Contains(Recipe.Id) || Recipe.Name.IsEmpty() || Recipe.Name.Len() > 100 ||
            Recipe.Inputs.IsEmpty() || !NewDefinitions.Contains(Recipe.OutputId) ||
            Recipe.OutputQuantity < 1 || Recipe.OutputQuantity > 9999 || Recipe.GoldCost < 0 || Recipe.GoldCost > 10000000)
            return Failure(TEXT("INVALID"), FString::Printf(TEXT("Invalid recipe: %s"), *Recipe.Id));
        for (const auto& Input : Recipe.Inputs)
            if (!NewDefinitions.Contains(Input.Key) || Input.Value < 1 || Input.Value > 9999)
                return Failure(TEXT("INVALID"), FString::Printf(TEXT("Invalid ingredient on %s"), *Recipe.Id));
        NewRecipes.Add(Recipe.Id, Recipe);
    }

    Definitions = MoveTemp(NewDefinitions);
    Recipes = MoveTemp(NewRecipes);
    State = FSPInventorySnapshot();
    State.Backpack.Init(FString(), BackpackSize);
    State.Stash.Init(FString(), StashSize);
    State.Loot.Init(FString(), LootSize);
    State.Hotbar.Init(FString(), 6);
    State.WeightLimitKg = WeightLimitKg;
    State.RandomState = RandomSeed;
    for (const FString& Slot : EquipmentSlots()) State.Equipment.Add(Slot, FString());
    bCatalogReady = true;
    return Success();
}

FSPInventoryResult USPInventoryComponent::InitializeFieldKit(const int64 WorldSeed, const int32 RifleReserve,
    const int32 SidearmReserve, const int32 InitialSpares)
{
    if (WorldSeed < 1 || WorldSeed > 0xffffffffLL || RifleReserve < 0 || SidearmReserve < 0 || InitialSpares < 0 ||
        RifleReserve > 9999 || SidearmReserve > 9999 || InitialSpares > 9999)
        return Failure(TEXT("INVALID"), TEXT("Field-kit seed and reserve counts must be valid."));
    auto MakeDef = [](const TCHAR* Id, const TCHAR* Name, const double Weight)
    {
        FSPInventoryDefinition Def;
        Def.Id = Id; Def.Name = Name; Def.Type = TEXT("material"); Def.WeightKg = Weight; Def.MaxStack = 9999;
        return Def;
    };
    const TArray<FSPInventoryDefinition> FieldDefinitions = {
        MakeDef(TEXT("rifle_ammo"), TEXT("AR-30 rounds"), 0.015),
        MakeDef(TEXT("sidearm_ammo"), TEXT("Sidearm rounds"), 0.01),
        MakeDef(TEXT("spares"), TEXT("Repair components"), 0.04),
        MakeDef(TEXT("alloy"), TEXT("Recovered alloy"), 0.06),
        MakeDef(TEXT("sample"), TEXT("Field samples"), 0.1)
    };
    auto MakeRecipe = [](const TCHAR* Id, const TCHAR* Name, const TCHAR* InputId, const int32 Input,
        const TCHAR* OutputId, const int32 Output)
    {
        FSPInventoryRecipe Recipe;
        Recipe.Id = Id; Recipe.Name = Name; Recipe.Inputs.Add(InputId, Input);
        Recipe.OutputId = OutputId; Recipe.OutputQuantity = Output;
        return Recipe;
    };
    const TArray<FSPInventoryRecipe> FieldRecipes = {
        MakeRecipe(TEXT("ammo"), TEXT("Fabricate 30 rifle rounds"), TEXT("alloy"), 6, TEXT("rifle_ammo"), 30),
        MakeRecipe(TEXT("parts"), TEXT("Fabricate repair components"), TEXT("alloy"), 3, TEXT("spares"), 4),
        MakeRecipe(TEXT("analyze"), TEXT("Analyze a field sample"), TEXT("sample"), 1, TEXT("alloy"), 4)
    };
    const FSPInventoryResult Init = InitializeCatalog(FieldDefinitions, FieldRecipes, 32, 56, 24, 200, WorldSeed);
    if (!Init.bOk) return Init;
    for (const auto& Pair : TArray<TPair<FString, int32>>{
        {TEXT("rifle_ammo"), RifleReserve}, {TEXT("sidearm_ammo"), SidearmReserve},
        {TEXT("spares"), InitialSpares}, {TEXT("alloy"), 18}})
    {
        if (Pair.Value <= 0) continue;
        const FSPInventoryResult Added = AddItem(Pair.Key, Pair.Value);
        if (!Added.bOk)
        {
            bCatalogReady = false;
            State = FSPInventorySnapshot();
            return Added;
        }
    }
    return Success();
}

int32 USPInventoryComponent::Count(const FString& DefinitionId, const FString& Container, const bool bUnlockedOnly) const
{
    const TArray<FString>* List = Slots(Container);
    if (!List) return 0;
    int32 Total = 0;
    for (const FString& Uid : *List)
        if (const FSPInventoryItem* Item = FindItem(Uid))
            if (Item->DefinitionId == DefinitionId && (!bUnlockedOnly || !Item->bLocked)) Total += Item->Quantity;
    return Total;
}

double USPInventoryComponent::CarriedWeightKg() const
{
    double Total = 0;
    auto AddWeight = [&](const FString& Uid)
    {
        if (const FSPInventoryItem* Item = FindItem(Uid))
            if (const FSPInventoryDefinition* Def = Definition(Item->DefinitionId)) Total += Def->WeightKg * Item->Quantity;
    };
    for (const FString& Uid : State.Backpack) AddWeight(Uid);
    for (const auto& Pair : State.Equipment) AddWeight(Pair.Value);
    return FMath::RoundToDouble(Total * 1000.0) / 1000.0;
}

TMap<FString, double> USPInventoryComponent::EffectiveStats() const
{
    TMap<FString, double> Stats = {{TEXT("damage"), 8}, {TEXT("armor"), 0}, {TEXT("magic"), 0},
        {TEXT("haste"), 0}, {TEXT("luck"), 0}, {TEXT("maxHealth"), 100}, {TEXT("maxMana"), 100}};
    for (const auto& Pair : State.Equipment)
    {
        const FSPInventoryItem* Item = FindItem(Pair.Value);
        if (!Item || Item->Durability == 0) continue;
        if (const FSPInventoryDefinition* Def = Definition(Item->DefinitionId))
            for (const auto& Stat : Def->Stats) Stats.FindOrAdd(Stat.Key) += Stat.Value;
    }
    Stats[TEXT("maxHealth")] = FMath::Max(1.0, Stats[TEXT("maxHealth")]);
    Stats[TEXT("maxMana")] = FMath::Max(1.0, Stats[TEXT("maxMana")]);
    return Stats;
}

bool USPInventoryComponent::GetItem(const FString& Uid, FSPInventoryItem& Item) const
{
    if (const FSPInventoryItem* Found = FindItem(Uid)) { Item = *Found; return true; }
    return false;
}

TArray<FSPInventoryItem> USPInventoryComponent::GetContainerItems(const FString& Container) const
{
    TArray<FSPInventoryItem> Result;
    if (const TArray<FString>* List = Slots(Container))
        for (const FString& Uid : *List)
            if (const FSPInventoryItem* Item = FindItem(Uid)) Result.Add(*Item);
    return Result;
}

FSPInventoryResult USPInventoryComponent::Transact(const FString& Action, TFunctionRef<FSPInventoryResult()> Mutation)
{
    if (!bCatalogReady) return Failure(TEXT("INVALID"), TEXT("Inventory catalog has not been initialized."));
    const FSPInventorySnapshot Before = State;
    FSPInventoryResult Result = Mutation();
    if (Result.bOk)
    {
        Normalize();
        FString Error;
        if (!Validate(Error)) Result = Failure(Error == TEXT("Carry limit exceeded.") ? TEXT("OVERWEIGHT") : TEXT("INVALID"), Error);
    }
    if (!Result.bOk) { State = Before; return Result; }
    State.Revision = Before.Revision + 1;
    OnInventoryChanged.Broadcast(Action, Result);
    return Result;
}

FSPInventoryResult USPInventoryComponent::AddInternal(const FString& DefinitionId, const int32 Quantity,
    const FString& Container, const bool bLocked, int32 Durability)
{
    const FSPInventoryDefinition* Def = Definition(DefinitionId);
    TArray<FString>* List = Slots(Container);
    if (!Def) return Failure(TEXT("INVALID"), TEXT("Unknown item definition."));
    if (!List) return Failure(TEXT("INVALID"), TEXT("Unknown container."));
    if (Quantity < 1 || Quantity > 100000) return Failure(TEXT("INVALID"), TEXT("Quantity must be 1–100000."));
    if (Durability == -2) Durability = Def->MaxDurability > 0 ? Def->MaxDurability : -1;
    if ((Def->MaxDurability == 0 && Durability != -1) ||
        (Def->MaxDurability > 0 && (Durability < 0 || Durability > Def->MaxDurability)))
        return Failure(TEXT("INVALID"), TEXT("Invalid item durability."));
    int32 Remaining = Quantity;
    FSPInventoryResult Result = Success(DefinitionId, Quantity);
    for (const FString& Uid : *List)
    {
        FSPInventoryItem* Item = FindItem(Uid);
        if (!Item || Item->DefinitionId != DefinitionId || Item->Durability != Durability ||
            Item->bLocked != bLocked || Item->Quantity >= Def->MaxStack) continue;
        const int32 Added = FMath::Min(Remaining, Def->MaxStack - Item->Quantity);
        Item->Quantity += Added;
        Remaining -= Added;
        Result.ItemUids.AddUnique(Uid);
        if (Remaining == 0) return Result;
    }
    while (Remaining > 0)
    {
        const int32 FreeIndex = List->Find(FString());
        if (FreeIndex == INDEX_NONE) return Failure(TEXT("FULL"), Container + TEXT(" has no free slot."));
        FSPInventoryItem& New = State.Items.AddDefaulted_GetRef();
        New.Uid = FString::Printf(TEXT("i%d"), State.NextId++);
        New.DefinitionId = DefinitionId;
        New.Quantity = FMath::Min(Remaining, Def->MaxStack);
        New.Durability = Durability;
        New.bLocked = bLocked;
        (*List)[FreeIndex] = New.Uid;
        Remaining -= New.Quantity;
        Result.ItemUids.Add(New.Uid);
    }
    if (!Result.ItemUids.IsEmpty()) Result.ItemUid = Result.ItemUids[0];
    return Result;
}

FSPInventoryResult USPInventoryComponent::PlaceExisting(const FString& Uid, const FString& Container)
{
    TArray<FString>* List = Slots(Container);
    if (!List) return Failure(TEXT("INVALID"), TEXT("Unknown container."));
    const int32 Index = List->Find(FString());
    if (Index == INDEX_NONE) return Failure(TEXT("FULL"), Container + TEXT(" has no free slot."));
    (*List)[Index] = Uid;
    return Success(FindItem(Uid)->DefinitionId, FindItem(Uid)->Quantity, Uid);
}

FSPInventoryResult USPInventoryComponent::AddItem(const FString& DefinitionId, const int32 Quantity,
    const FString& Container, const bool bLocked)
{
    return Transact(TEXT("inventory:added"), [&] { return AddInternal(DefinitionId, Quantity, Container, bLocked); });
}

FSPInventoryResult USPInventoryComponent::RemoveInternal(const FString& Uid, const int32 Quantity, const bool bProtect)
{
    // A hotbar caller may pass a reference into Backpack. Unlink clears that
    // slot, so keep a stable UID across the mutation.
    const FString StableUid = Uid;
    FSPInventoryItem* Item = FindItem(StableUid);
    if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
    const FSPInventoryDefinition* Def = Definition(Item->DefinitionId);
    if (Quantity < 1 || Quantity > Item->Quantity) return Failure(TEXT("INVALID"), TEXT("Invalid removal quantity."));
    if (bProtect && Item->bLocked) return Failure(TEXT("LOCKED"), TEXT("Unlock this item first."));
    if (bProtect && Def->bQuest) return Failure(TEXT("QUEST"), TEXT("Quest items are protected."));
    const FString DefinitionId = Item->DefinitionId;
    Item->Quantity -= Quantity;
    if (Item->Quantity == 0)
    {
        Unlink(StableUid);
        State.Items.RemoveAll([&](const FSPInventoryItem& Candidate) { return Candidate.Uid == StableUid; });
    }
    return Success(DefinitionId, Quantity, StableUid);
}

FSPInventoryResult USPInventoryComponent::RemoveItem(const FString& Uid, const int32 Quantity)
{
    return Transact(TEXT("inventory:removed"), [&] { return RemoveInternal(Uid, Quantity, true); });
}

FSPInventoryResult USPInventoryComponent::SpendInternal(const FString& DefinitionId, const int32 Quantity)
{
    if (!Definition(DefinitionId) || Quantity < 1 || Quantity > 100000)
        return Failure(TEXT("INVALID"), TEXT("Invalid item or quantity."));
    if (Count(DefinitionId, TEXT("backpack"), true) < Quantity)
        return Failure(TEXT("INGREDIENTS"), TEXT("Not enough unlocked materials."));
    int32 Remaining = Quantity;
    for (const FString Uid : State.Backpack)
    {
        const FSPInventoryItem* Item = FindItem(Uid);
        if (!Item || Item->DefinitionId != DefinitionId || Item->bLocked) continue;
        const int32 Taken = FMath::Min(Remaining, Item->Quantity);
        const FSPInventoryResult Removed = RemoveInternal(Uid, Taken, true);
        if (!Removed.bOk) return Removed;
        Remaining -= Taken;
        if (Remaining == 0) break;
    }
    return Success(DefinitionId, Quantity);
}

FSPInventoryResult USPInventoryComponent::Spend(const FString& DefinitionId, const int32 Quantity)
{
    return Transact(TEXT("inventory:spent"), [&] { return SpendInternal(DefinitionId, Quantity); });
}

FSPInventoryResult USPInventoryComponent::SetCount(const FString& DefinitionId, const int32 DesiredQuantity)
{
    return Transact(TEXT("inventory:count"), [&]
    {
        if (!Definition(DefinitionId)) return Failure(TEXT("INVALID"), TEXT("Unknown item definition."));
        const int32 Desired = FMath::Clamp(DesiredQuantity, 0, 9999);
        const int32 Current = Count(DefinitionId);
        if (Desired > Current) return AddInternal(DefinitionId, Desired - Current, TEXT("backpack"), false);
        if (Desired < Current) return SpendInternal(DefinitionId, Current - Desired);
        return Success(DefinitionId, 0);
    });
}

FSPInventoryResult USPInventoryComponent::Transfer(const FString& Uid, const FString& Destination, const int32 Quantity)
{
    return Transact(TEXT("inventory:transferred"), [&]
    {
        const FString StableUid = Uid;
        const FSPInventoryItem* Item = FindItem(StableUid);
        if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
        FString From;
        int32 Index;
        if (!Locate(StableUid, From, Index) || !Slots(From)) return Failure(TEXT("INVALID"), TEXT("Transfer from a container only."));
        if (!Slots(Destination) || Destination == From) return Failure(TEXT("INVALID"), TEXT("Choose a different container."));
        if (Quantity < 1 || Quantity > Item->Quantity) return Failure(TEXT("INVALID"), TEXT("Invalid transfer quantity."));
        const FString DefId = Item->DefinitionId;
        const int32 Durability = Item->Durability;
        const bool bLocked = Item->bLocked;
        // Like the source #place, a full-stack move keeps its identity when
        // there is no compatible destination stack to merge into.
        bool bCanMerge = false;
        const FSPInventoryDefinition* Def = Definition(DefId);
        if (Def->MaxStack > 1)
            for (const FString& DestUid : *Slots(Destination))
                if (const FSPInventoryItem* Other = FindItem(DestUid))
                    if (Other->DefinitionId == DefId && Other->Durability == Durability &&
                        Other->bLocked == bLocked && Other->Quantity < Def->MaxStack) { bCanMerge = true; break; }
        if (Quantity == Item->Quantity && !bCanMerge)
        {
            Unlink(StableUid);
            return PlaceExisting(StableUid, Destination);
        }
        const FSPInventoryResult Added = AddInternal(DefId, Quantity, Destination, bLocked, Durability);
        if (!Added.bOk) return Added;
        const FSPInventoryResult Removed = RemoveInternal(StableUid, Quantity, false);
        if (!Removed.bOk) return Removed;
        return Added;
    });
}

FSPInventoryResult USPInventoryComponent::SplitStack(const FString& Uid, const int32 Quantity, const FString& Destination)
{
    return Transact(TEXT("inventory:split"), [&]
    {
        FSPInventoryItem* Item = FindItem(Uid);
        if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
        FString From;
        int32 Index;
        if (!Locate(Uid, From, Index) || !Slots(From)) return Failure(TEXT("INVALID"), TEXT("Equipped items cannot be split."));
        TArray<FString>* List = Slots(Destination);
        if (!List || Quantity < 1 || Quantity >= Item->Quantity) return Failure(TEXT("INVALID"), TEXT("Invalid split quantity or container."));
        const int32 Free = List->Find(FString());
        if (Free == INDEX_NONE) return Failure(TEXT("FULL"), TEXT("No empty slot for the split stack."));
        const FSPInventoryItem Copy = *Item;
        Item->Quantity -= Quantity;
        FSPInventoryItem& New = State.Items.AddDefaulted_GetRef();
        New = Copy;
        New.Uid = FString::Printf(TEXT("i%d"), State.NextId++);
        New.Quantity = Quantity;
        (*List)[Free] = New.Uid;
        return Success(New.DefinitionId, Quantity, New.Uid);
    });
}

FSPInventoryResult USPInventoryComponent::EquipInternal(const FString& Uid)
{
    const FString StableUid = Uid;
    const FSPInventoryItem* Item = FindItem(StableUid);
    if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
    const FSPInventoryDefinition* Def = Definition(Item->DefinitionId);
    if (Def->EquipSlot.IsEmpty()) return Failure(TEXT("SLOT"), TEXT("This item is not equipment."));
    FString Location;
    int32 Index;
    if (!Locate(StableUid, Location, Index) || Location != TEXT("backpack"))
        return Failure(TEXT("INVALID"), TEXT("Equipment must be in your backpack."));
    const FString DefId = Item->DefinitionId;
    Unlink(StableUid);
    TArray<FString> Displace = {Def->EquipSlot};
    if (Def->bTwoHanded) Displace.Add(TEXT("offhand"));
    if (Def->EquipSlot == TEXT("offhand"))
    {
        const FString MainUid = State.Equipment.FindRef(TEXT("mainhand"));
        if (const FSPInventoryItem* Main = FindItem(MainUid))
            if (Definition(Main->DefinitionId)->bTwoHanded) Displace.Add(TEXT("mainhand"));
    }
    for (const FString& Slot : Displace)
    {
        const FString Old = State.Equipment.FindRef(Slot);
        if (Old.IsEmpty()) continue;
        State.Equipment.FindOrAdd(Slot).Empty();
        const FSPInventoryResult Placed = PlaceExisting(Old, TEXT("backpack"));
        if (!Placed.bOk) return Placed;
    }
    State.Equipment.FindOrAdd(Def->EquipSlot) = StableUid;
    return Success(DefId, 1, StableUid);
}

FSPInventoryResult USPInventoryComponent::Equip(const FString& Uid)
{
    return Transact(TEXT("equipment:changed"), [&] { return EquipInternal(Uid); });
}

FSPInventoryResult USPInventoryComponent::Unequip(const FString& Slot, const FString& Destination)
{
    return Transact(TEXT("equipment:changed"), [&]
    {
        if (!ValidSlot(Slot)) return Failure(TEXT("SLOT"), TEXT("Unknown equipment slot."));
        const FString Uid = State.Equipment.FindRef(Slot);
        if (Uid.IsEmpty()) return Failure(TEXT("INVALID"), TEXT("This equipment slot is empty."));
        State.Equipment.FindOrAdd(Slot).Empty();
        return PlaceExisting(Uid, Destination);
    });
}

FSPInventoryResult USPInventoryComponent::SetLocked(const FString& Uid, const bool bLocked)
{
    return Transact(TEXT("inventory:locked"), [&]
    {
        FSPInventoryItem* Item = FindItem(Uid);
        if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
        Item->bLocked = bLocked;
        return Success(Item->DefinitionId, Item->Quantity, Uid);
    });
}

FSPInventoryResult USPInventoryComponent::ConsumeInternal(const FString& Uid)
{
    const FSPInventoryItem* Item = FindItem(Uid);
    if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
    const FSPInventoryDefinition* Def = Definition(Item->DefinitionId);
    FString Location;
    int32 Index;
    if (!Locate(Uid, Location, Index) || Location != TEXT("backpack"))
        return Failure(TEXT("INVALID"), TEXT("Consumables must be in your backpack."));
    if (Def->Type != TEXT("consumable") || (Def->HealthEffect <= 0 && Def->ManaEffect <= 0))
        return Failure(TEXT("INVALID"), TEXT("This item cannot be consumed."));
    if (Item->bLocked) return Failure(TEXT("LOCKED"), TEXT("Unlock this item before using it."));
    if (State.Cooldowns.FindRef(Def->Id) > 0) return Failure(TEXT("COOLDOWN"), TEXT("This item is cooling down."));
    const TMap<FString, double> Stats = EffectiveStats();
    const double Health = FMath::Min(Def->HealthEffect, Stats.FindRef(TEXT("maxHealth")) - State.Player.Health);
    const double Mana = FMath::Min(Def->ManaEffect, Stats.FindRef(TEXT("maxMana")) - State.Player.Mana);
    if (Health <= 0 && Mana <= 0) return Failure(TEXT("FULL_RESOURCE"), TEXT("Relevant resources are already full."));
    State.Player.Health += FMath::Max(0.0, Health);
    State.Player.Mana += FMath::Max(0.0, Mana);
    State.Cooldowns.Add(Def->Id, Def->CooldownSeconds);
    return RemoveInternal(Uid, 1, false);
}

FSPInventoryResult USPInventoryComponent::Consume(const FString& Uid)
{
    return Transact(TEXT("item:used"), [&] { return ConsumeInternal(Uid); });
}

FSPInventoryResult USPInventoryComponent::AssignHotbar(const int32 Index, const FString& Uid)
{
    return Transact(TEXT("hotbar:changed"), [&]
    {
        if (Index < 0 || Index >= 6) return Failure(TEXT("INVALID"), TEXT("Hotbar index must be 0–5."));
        if (Uid.IsEmpty()) { State.Hotbar[Index].Empty(); return Success(); }
        const FSPInventoryItem* Item = FindItem(Uid);
        if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
        FString Location;
        int32 SlotIndex;
        if (!Locate(Uid, Location, SlotIndex) || (Location != TEXT("backpack") && !ValidSlot(Location)))
            return Failure(TEXT("INVALID"), TEXT("Only carried items can be assigned."));
        const FSPInventoryDefinition* Def = Definition(Item->DefinitionId);
        if (Def->EquipSlot.IsEmpty() && Def->HealthEffect <= 0 && Def->ManaEffect <= 0)
            return Failure(TEXT("INVALID"), TEXT("Only consumables and equipment can be assigned."));
        State.Hotbar[Index] = Def->Id;
        return Success(Def->Id, 1, Uid);
    });
}

FSPInventoryResult USPInventoryComponent::ActivateHotbar(const int32 Index)
{
    return Transact(TEXT("hotbar:used"), [&]
    {
        if (Index < 0 || Index >= 6 || State.Hotbar[Index].IsEmpty())
            return Failure(TEXT("INVALID"), TEXT("This hotbar slot is empty."));
        const FString DefId = State.Hotbar[Index];
        const FSPInventoryDefinition* Def = Definition(DefId);
        for (const FString& Uid : State.Backpack)
        {
            const FSPInventoryItem* Item = FindItem(Uid);
            if (!Item || Item->DefinitionId != DefId || ((Def->HealthEffect > 0 || Def->ManaEffect > 0) && Item->bLocked)) continue;
            const FString StableUid = Uid;
            return (Def->HealthEffect > 0 || Def->ManaEffect > 0) ? ConsumeInternal(StableUid) : EquipInternal(StableUid);
        }
        return Failure(TEXT("NOT_FOUND"), TEXT("No usable copy is in the backpack."));
    });
}

void USPInventoryComponent::AdvanceCooldowns(const double DeltaSeconds)
{
    if (!bCatalogReady || !FiniteRange(DeltaSeconds, 0, 3600)) return;
    for (auto It = State.Cooldowns.CreateIterator(); It; ++It)
    {
        const double Remaining = FMath::Max(0.0, It.Value() - DeltaSeconds);
        if (Remaining == 0) It.RemoveCurrent(); else It.Value() = Remaining;
    }
}

FSPInventoryResult USPInventoryComponent::SetPlayerResources(const double Health, const double Mana, const int32 Gold, const int32 Level)
{
    return Transact(TEXT("player:changed"), [&]
    {
        State.Player.Health = Health; State.Player.Mana = Mana;
        State.Player.Gold = Gold; State.Player.Level = Level;
        return Success();
    });
}

FSPInventoryResult USPInventoryComponent::SetWeightLimit(const double NewWeightLimitKg)
{
    return Transact(TEXT("settings:changed"), [&]
    {
        State.WeightLimitKg = NewWeightLimitKg;
        return Success();
    });
}

FSPInventoryResult USPInventoryComponent::CraftInternal(const FString& RecipeId, const int32 Times)
{
    const FSPInventoryRecipe* Recipe = Recipes.Find(RecipeId);
    if (!Recipe) return Failure(TEXT("INVALID"), TEXT("Unknown recipe."));
    if (Times < 1 || Times > 99) return Failure(TEXT("INVALID"), TEXT("Craft count must be 1–99."));
    const int64 GoldCost = static_cast<int64>(Recipe->GoldCost) * Times;
    if (State.Player.Gold < GoldCost) return Failure(TEXT("GOLD"), TEXT("Not enough gold."));
    for (const auto& Input : Recipe->Inputs)
    {
        const int64 Needed = static_cast<int64>(Input.Value) * Times;
        if (Needed > 100000) return Failure(TEXT("INVALID"), TEXT("Ingredient quantity exceeds transaction limit."));
        const FSPInventoryResult Spent = SpendInternal(Input.Key, static_cast<int32>(Needed));
        if (!Spent.bOk) return Spent;
    }
    State.Player.Gold -= static_cast<int32>(GoldCost);
    const int64 OutputQuantity = static_cast<int64>(Recipe->OutputQuantity) * Times;
    if (OutputQuantity > 100000) return Failure(TEXT("INVALID"), TEXT("Output quantity exceeds transaction limit."));
    return AddInternal(Recipe->OutputId, static_cast<int32>(OutputQuantity), TEXT("backpack"), false);
}

FSPInventoryResult USPInventoryComponent::CanCraft(const FString& RecipeId, const int32 Times) const
{
    if (!bCatalogReady) return Failure(TEXT("INVALID"), TEXT("Inventory catalog has not been initialized."));
    USPInventoryComponent* Trial = NewObject<USPInventoryComponent>(GetTransientPackage());
    Trial->Definitions = Definitions;
    Trial->Recipes = Recipes;
    Trial->State = State;
    Trial->bCatalogReady = true;
    return Trial->Craft(RecipeId, Times);
}

FSPInventoryResult USPInventoryComponent::Craft(const FString& RecipeId, const int32 Times)
{
    return Transact(TEXT("inventory:crafted"), [&] { return CraftInternal(RecipeId, Times); });
}

FSPInventoryResult USPInventoryComponent::CraftAtTerminal(const FString& RecipeId, const int32 Times, const FSPFabricationContext& Context)
{
    if (!Context.bInCity) return Failure(TEXT("STATION"), TEXT("Use a city fabrication terminal."));
    if (!Context.bOperationsFloor || !FiniteRange(Context.TerminalDistanceMeters, 0, 3))
        return Failure(TEXT("STATION"), TEXT("Move within 3 m of the floor-1 operations terminal."));
    if (!FMath::IsFinite(Context.TerminalPower) || Context.TerminalPower < 0.05)
        return Failure(TEXT("POWER"), TEXT("Fabrication terminal needs power."));
    return Craft(RecipeId, Times);
}

FSPInventoryResult USPInventoryComponent::Salvage(const FString& Uid)
{
    return Transact(TEXT("inventory:salvaged"), [&]
    {
        const FSPInventoryItem* Item = FindItem(Uid);
        if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
        const FSPInventoryDefinition* Def = Definition(Item->DefinitionId);
        FString Location;
        int32 Index;
        if (!Locate(Uid, Location, Index) || Location != TEXT("backpack") || Def->Salvage.IsEmpty())
            return Failure(TEXT("INVALID"), TEXT("Item cannot be salvaged from this location."));
        const FSPInventoryResult Removed = RemoveInternal(Uid, 1, true);
        if (!Removed.bOk) return Removed;
        for (const auto& Pair : Def->Salvage)
        {
            const FSPInventoryResult Added = AddInternal(Pair.Key, Pair.Value, TEXT("backpack"), false);
            if (!Added.bOk) return Added;
        }
        return Success(Def->Id, 1, Uid);
    });
}

FSPInventoryResult USPInventoryComponent::DamageEquipment(const FString& Slot, const int32 Damage)
{
    return Transact(TEXT("equipment:damaged"), [&]
    {
        if (!ValidSlot(Slot) || Damage < 1 || Damage > 100000)
            return Failure(TEXT("INVALID"), TEXT("Invalid equipment slot or damage."));
        const FString Uid = State.Equipment.FindRef(Slot);
        FSPInventoryItem* Item = FindItem(Uid);
        if (!Item || Item->Durability < 0) return Failure(TEXT("INVALID"), TEXT("No durable item in this slot."));
        Item->Durability = FMath::Max(0, Item->Durability - Damage);
        return Success(Item->DefinitionId, Damage, Uid);
    });
}

FSPInventoryResult USPInventoryComponent::Repair(const FString& Uid)
{
    return Transact(TEXT("equipment:repaired"), [&]
    {
        FSPInventoryItem* Item = FindItem(Uid);
        if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
        const FSPInventoryDefinition* Def = Definition(Item->DefinitionId);
        FString Location;
        int32 Index;
        if (!Locate(Uid, Location, Index) || (Location != TEXT("backpack") && !ValidSlot(Location)) ||
            Item->Durability < 0 || Item->Durability >= Def->MaxDurability)
            return Failure(TEXT("INVALID"), TEXT("This carried item does not need repair."));
        const int32 Cost = FMath::CeilToInt((Def->MaxDurability - Item->Durability) /
            static_cast<double>(Def->MaxDurability) * Def->Value * 0.25);
        if (State.Player.Gold < Cost) return Failure(TEXT("GOLD"), TEXT("Not enough gold."));
        State.Player.Gold -= Cost;
        Item->Durability = Def->MaxDurability;
        return Success(Item->DefinitionId, Cost, Uid);
    });
}

FSPInventoryResult USPInventoryComponent::Buy(const FString& DefinitionId, const int32 Quantity)
{
    return Transact(TEXT("merchant:bought"), [&]
    {
        const FSPInventoryDefinition* Def = Definition(DefinitionId);
        if (!Def || Def->bQuest || Def->Value <= 0 || Quantity < 1 || Quantity > 9999)
            return Failure(TEXT("INVALID"), TEXT("This item is not for sale."));
        const int64 Cost = static_cast<int64>(Def->Value) * Quantity;
        if (State.Player.Gold < Cost) return Failure(TEXT("GOLD"), TEXT("Not enough gold."));
        State.Player.Gold -= static_cast<int32>(Cost);
        return AddInternal(DefinitionId, Quantity, TEXT("backpack"), false);
    });
}

FSPInventoryResult USPInventoryComponent::Sell(const FString& Uid, const int32 Quantity)
{
    return Transact(TEXT("merchant:sold"), [&]
    {
        const FSPInventoryItem* Item = FindItem(Uid);
        if (!Item) return Failure(TEXT("NOT_FOUND"), TEXT("Item no longer exists."));
        FString Location;
        int32 Index;
        if (!Locate(Uid, Location, Index) || Location != TEXT("backpack"))
            return Failure(TEXT("INVALID"), TEXT("Sell from the backpack only."));
        const FSPInventoryDefinition* Def = Definition(Item->DefinitionId);
        const int64 Earned = static_cast<int64>(FMath::FloorToInt(Def->Value * 0.4)) * Quantity;
        if (Earned <= 0 || Earned > MAX_int32 || State.Player.Gold > 1000000000 - Earned)
            return Failure(TEXT("INVALID"), TEXT("Item has no resale value or gold limit reached."));
        const FSPInventoryResult Removed = RemoveInternal(Uid, Quantity, true);
        if (!Removed.bOk) return Removed;
        State.Player.Gold += static_cast<int32>(Earned);
        return Success(Def->Id, static_cast<int32>(Earned), Uid);
    });
}

void USPInventoryComponent::Normalize()
{
    const TMap<FString, double> Stats = EffectiveStats();
    State.Player.Health = FMath::Min(State.Player.Health, Stats.FindRef(TEXT("maxHealth")));
    State.Player.Mana = FMath::Min(State.Player.Mana, Stats.FindRef(TEXT("maxMana")));
    for (FString& DefId : State.Hotbar)
    {
        bool bCarried = Count(DefId) > 0;
        if (!bCarried)
            for (const auto& Pair : State.Equipment)
                if (const FSPInventoryItem* Item = FindItem(Pair.Value))
                    if (Item->DefinitionId == DefId) { bCarried = true; break; }
        if (!bCarried) DefId.Empty();
    }
}

bool USPInventoryComponent::Validate(FString& Error) const
{
    auto Fail = [&](const TCHAR* Message) { Error = Message; return false; };
    if (!bCatalogReady || State.Version != 1 || State.Revision < 0 || State.NextId < 1 ||
        State.RandomState < 1 || State.RandomState > 0xffffffffLL ||
        !ValidContainerSize(State.Backpack.Num()) || !ValidContainerSize(State.Stash.Num()) ||
        !ValidContainerSize(State.Loot.Num()) || !FiniteRange(State.WeightLimitKg, 0.1, 100000) ||
        State.Items.Num() > 4096 || State.Hotbar.Num() != 6 || State.Equipment.Num() != EquipmentSlots().Num())
        return Fail(TEXT("Invalid inventory save header or container sizes."));
    TSet<FString> Seen;
    TSet<FString> ItemIds;
    int32 HighestId = 0;
    for (const FSPInventoryItem& Item : State.Items)
    {
        if (!Item.Uid.StartsWith(TEXT("i")) || Item.Uid.Len() < 2 || Item.Uid[1] == TEXT('0'))
            return Fail(TEXT("Invalid item UID."));
        for (int32 Pos = 1; Pos < Item.Uid.Len(); ++Pos)
            if (Item.Uid[Pos] < TEXT('0') || Item.Uid[Pos] > TEXT('9')) return Fail(TEXT("Invalid item UID."));
        if (ItemIds.Contains(Item.Uid)) return Fail(TEXT("Duplicate item UID."));
        ItemIds.Add(Item.Uid);
        HighestId = FMath::Max(HighestId, FCString::Atoi(*Item.Uid.RightChop(1)));
        const FSPInventoryDefinition* Def = Definition(Item.DefinitionId);
        if (!Def || Item.Quantity < 1 || Item.Quantity > Def->MaxStack ||
            (Def->MaxDurability == 0 && Item.Durability != -1) ||
            (Def->MaxDurability > 0 && (Item.Durability < 0 || Item.Durability > Def->MaxDurability)))
            return Fail(TEXT("Invalid item definition, quantity or durability."));
    }
    if (State.NextId <= HighestId) return Fail(TEXT("Next item ID collides with existing item."));
    auto Visit = [&](const FString& Uid) -> bool
    {
        if (Uid.IsEmpty()) return true;
        if (!ItemIds.Contains(Uid) || Seen.Contains(Uid)) return false;
        Seen.Add(Uid);
        return true;
    };
    for (const FString& Uid : State.Backpack) if (!Visit(Uid)) return Fail(TEXT("Duplicate or missing backpack item."));
    for (const FString& Uid : State.Stash) if (!Visit(Uid)) return Fail(TEXT("Duplicate or missing stash item."));
    for (const FString& Uid : State.Loot) if (!Visit(Uid)) return Fail(TEXT("Duplicate or missing loot item."));
    for (const FString& Slot : EquipmentSlots())
    {
        const FString* Uid = State.Equipment.Find(Slot);
        if (!Uid || !Visit(*Uid)) return Fail(TEXT("Duplicate or missing equipped item."));
        if (!Uid->IsEmpty())
        {
            const FSPInventoryItem* Item = FindItem(*Uid);
            if (!Item || Definition(Item->DefinitionId)->EquipSlot != Slot) return Fail(TEXT("Item does not fit equipped slot."));
        }
    }
    if (Seen.Num() != ItemIds.Num()) return Fail(TEXT("Save contains an orphaned item."));
    const FSPInventoryItem* Main = FindItem(State.Equipment.FindRef(TEXT("mainhand")));
    if (Main && Definition(Main->DefinitionId)->bTwoHanded && !State.Equipment.FindRef(TEXT("offhand")).IsEmpty())
        return Fail(TEXT("Two-handed weapon conflicts with off-hand item."));
    for (const FString& DefId : State.Hotbar)
    {
        if (DefId.IsEmpty()) continue;
        const FSPInventoryDefinition* Def = Definition(DefId);
        bool bCarried = Count(DefId) > 0;
        for (const auto& Pair : State.Equipment)
            if (const FSPInventoryItem* Item = FindItem(Pair.Value))
                if (Item->DefinitionId == DefId) bCarried = true;
        if (!Def || !bCarried || (Def->EquipSlot.IsEmpty() && Def->HealthEffect <= 0 && Def->ManaEffect <= 0))
            return Fail(TEXT("Hotbar references an unavailable item."));
    }
    for (const auto& Cooldown : State.Cooldowns)
        if (!Definition(Cooldown.Key) || !FiniteRange(Cooldown.Value, 0, 3600)) return Fail(TEXT("Invalid cooldown."));
    const TMap<FString, double> Stats = EffectiveStats();
    if (State.Player.Gold < 0 || State.Player.Gold > 1000000000 || State.Player.Level < 1 || State.Player.Level > 9999 ||
        !FiniteRange(State.Player.Health, 0, Stats.FindRef(TEXT("maxHealth"))) ||
        !FiniteRange(State.Player.Mana, 0, Stats.FindRef(TEXT("maxMana"))))
        return Fail(TEXT("Invalid player resources."));
    if (CarriedWeightKg() > State.WeightLimitKg + 0.000001) return Fail(TEXT("Carry limit exceeded."));
    Error.Empty();
    return true;
}

bool USPInventoryComponent::ValidateState(FString& Error) const { return Validate(Error); }

FSPInventorySnapshot USPInventoryComponent::CaptureState() const { return State; }

FSPInventoryResult USPInventoryComponent::RestoreState(const FSPInventorySnapshot& Snapshot)
{
    if (!bCatalogReady) return Failure(TEXT("INVALID"), TEXT("Initialize the same catalog before restoring a save."));
    const FSPInventorySnapshot Before = State;
    State = Snapshot;
    FString Error;
    if (!Validate(Error))
    {
        State = Before;
        return Failure(Error == TEXT("Carry limit exceeded.") ? TEXT("OVERWEIGHT") : TEXT("INVALID"), Error);
    }
    const FSPInventoryResult Result = Success();
    OnInventoryChanged.Broadcast(TEXT("inventory:loaded"), Result);
    return Result;
}

USPSInventorySaveGame* USPInventoryComponent::CreateSaveGame() const
{
    USPSInventorySaveGame* Save = NewObject<USPSInventorySaveGame>(GetTransientPackage());
    Save->Snapshot = State;
    return Save;
}

FSPInventoryResult USPInventoryComponent::RestoreSaveGame(const USPSInventorySaveGame* Save)
{
    return Save ? RestoreState(Save->Snapshot) : Failure(TEXT("INVALID"), TEXT("Missing inventory save."));
}
