#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/SaveGame.h"
#include "SPInventoryComponent.generated.h"

/** Data-driven InventoryWorks definition. IDs and slot names use the source's lowercase spelling. */
USTRUCT(BlueprintType)
struct FSPInventoryDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Type = TEXT("material");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Rarity = TEXT("common");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double WeightKg = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Value = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxStack = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString EquipSlot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTwoHanded = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxDurability = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FString, double> Stats;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FString, int32> Salvage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double HealthEffect = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double ManaEffect = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double CooldownSeconds = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bQuest = false;
};

USTRUCT(BlueprintType)
struct FSPInventoryRecipe
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FString, int32> Inputs;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString OutputId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 OutputQuantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GoldCost = 0;
};

USTRUCT(BlueprintType)
struct FSPInventoryItem
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Uid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DefinitionId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 1;
    /** -1 means this definition has no durability. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Durability = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLocked = false;
};

USTRUCT(BlueprintType)
struct FSPInventoryPlayer
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double Health = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double Mana = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Gold = 420;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Level = 12;
};

/** Compact native save; all instances are placed in exactly one container or equipment slot. */
USTRUCT(BlueprintType)
struct FSPInventorySnapshot
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Version = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Revision = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NextId = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 RandomState = 90210;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double WeightLimitKg = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSPInventoryItem> Items;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Backpack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Stash;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Loot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FString, FString> Equipment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Hotbar;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FString, double> Cooldowns;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FSPInventoryPlayer Player;
};

USTRUCT(BlueprintType)
struct FSPInventoryResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bOk = false;
    UPROPERTY(BlueprintReadOnly) FString Code;
    UPROPERTY(BlueprintReadOnly) FString Message;
    UPROPERTY(BlueprintReadOnly) FString ItemUid;
    UPROPERTY(BlueprintReadOnly) FString DefinitionId;
    UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FString> ItemUids;
};

/** Pass actual city/terminal interaction and Machineworks power, not a UI-side assumption. */
USTRUCT(BlueprintType)
struct FSPFabricationContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInCity = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOperationsFloor = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double TerminalDistanceMeters = 1000000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double TerminalPower = 0;
};

UCLASS()
class SPACEPATRIOTUNREAL_API USPSInventorySaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame, BlueprintReadOnly) FSPInventorySnapshot Snapshot;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPInventoryChangedSignature, const FString&, Action, const FSPInventoryResult&, Result);

/**
 * Native InventoryWorks state machine. Catalog, terminal interaction and external
 * ammo/spares consumers are explicit integration points; no screen is implied.
 */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPInventoryComponent();

    UPROPERTY(BlueprintAssignable, Category="Space Patriot|Inventory") FSPInventoryChangedSignature OnInventoryChanged;

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult InitializeCatalog(const TArray<FSPInventoryDefinition>& InDefinitions, const TArray<FSPInventoryRecipe>& InRecipes, int32 BackpackSize = 40, int32 StashSize = 56, int32 LootSize = 24, double WeightLimitKg = 60, int64 RandomSeed = 90210);
    /** Original field-inventory.js catalog: five reserve/material types, three recipes and 18 starting alloy. */
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult InitializeFieldKit(int64 WorldSeed, int32 RifleReserve, int32 SidearmReserve, int32 InitialSpares);
    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory") int32 Count(const FString& DefinitionId, const FString& Container = TEXT("backpack"), bool bUnlockedOnly = false) const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory") double CarriedWeightKg() const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory") TMap<FString, double> EffectiveStats() const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory") bool GetItem(const FString& Uid, FSPInventoryItem& Item) const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory") TArray<FSPInventoryItem> GetContainerItems(const FString& Container) const;
    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory") FSPInventoryPlayer GetPlayer() const { return State.Player; }
    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory") int32 GetRevision() const { return State.Revision; }
    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory") bool IsReady() const { return bCatalogReady; }

    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult AddItem(const FString& DefinitionId, int32 Quantity = 1, const FString& Container = TEXT("backpack"), bool bLocked = false);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult SetCount(const FString& DefinitionId, int32 DesiredQuantity);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult Spend(const FString& DefinitionId, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult RemoveItem(const FString& Uid, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult Transfer(const FString& Uid, const FString& Destination, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult SplitStack(const FString& Uid, int32 Quantity, const FString& Destination = TEXT("backpack"));
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult Equip(const FString& Uid);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult Unequip(const FString& Slot, const FString& Destination = TEXT("backpack"));
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult SetLocked(const FString& Uid, bool bLocked);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult Consume(const FString& Uid);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult AssignHotbar(int32 Index, const FString& Uid);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult ActivateHotbar(int32 Index);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") void AdvanceCooldowns(double DeltaSeconds);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult SetPlayerResources(double Health, double Mana, int32 Gold, int32 Level);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult SetWeightLimit(double NewWeightLimitKg);

    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory|Fabrication") FSPInventoryResult CanCraft(const FString& RecipeId, int32 Times = 1) const;
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory|Fabrication") FSPInventoryResult Craft(const FString& RecipeId, int32 Times = 1);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory|Fabrication") FSPInventoryResult CraftAtTerminal(const FString& RecipeId, int32 Times, const FSPFabricationContext& Context);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult Salvage(const FString& Uid);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult DamageEquipment(const FString& Slot, int32 Damage);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult Repair(const FString& Uid);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult Buy(const FString& DefinitionId, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory") FSPInventoryResult Sell(const FString& Uid, int32 Quantity);

    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory|Save") FSPInventorySnapshot CaptureState() const;
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory|Save") FSPInventoryResult RestoreState(const FSPInventorySnapshot& Snapshot);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory|Save") USPSInventorySaveGame* CreateSaveGame() const;
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Inventory|Save") FSPInventoryResult RestoreSaveGame(const USPSInventorySaveGame* Save);
    UFUNCTION(BlueprintPure, Category="Space Patriot|Inventory|Save") bool ValidateState(FString& Error) const;

private:
    UPROPERTY(SaveGame) FSPInventorySnapshot State;
    TMap<FString, FSPInventoryDefinition> Definitions;
    TMap<FString, FSPInventoryRecipe> Recipes;
    bool bCatalogReady = false;

    FSPInventoryResult Transact(const FString& Action, TFunctionRef<FSPInventoryResult()> Mutation);
    static FSPInventoryResult Success(const FString& DefinitionId = FString(), int32 Quantity = 0, const FString& Uid = FString());
    static FSPInventoryResult Failure(const FString& Code, const FString& Message);
    static bool ValidId(const FString& Id);
    static bool ValidSlot(const FString& Slot);
    const FSPInventoryDefinition* Definition(const FString& Id) const;
    const FSPInventoryItem* FindItem(const FString& Uid) const;
    FSPInventoryItem* FindItem(const FString& Uid);
    const TArray<FString>* Slots(const FString& Container) const;
    TArray<FString>* Slots(const FString& Container);
    bool Locate(const FString& Uid, FString& ContainerOrSlot, int32& Index) const;
    void Unlink(const FString& Uid);
    FSPInventoryResult PlaceExisting(const FString& Uid, const FString& Container);
    FSPInventoryResult AddInternal(const FString& DefinitionId, int32 Quantity, const FString& Container, bool bLocked, int32 Durability = -2);
    FSPInventoryResult RemoveInternal(const FString& Uid, int32 Quantity, bool bProtect);
    FSPInventoryResult SpendInternal(const FString& DefinitionId, int32 Quantity);
    FSPInventoryResult EquipInternal(const FString& Uid);
    FSPInventoryResult ConsumeInternal(const FString& Uid);
    FSPInventoryResult CraftInternal(const FString& RecipeId, int32 Times);
    void Normalize();
    bool Validate(FString& Error) const;
};
