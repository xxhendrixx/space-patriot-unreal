#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/SaveGame.h"
#include "SPSocietySimulationComponent.generated.h"

USTRUCT(BlueprintType)
struct FSPSocietySettlement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString WorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Kind;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Owner;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPrimary = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Food = 75.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Water = 90.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Power = 95.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Health = 90.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Security = 85.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Prosperity = 50.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Days = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Crime = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Deliveries = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Repairs = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double NextIncident = 300.0;
};

USTRUCT(BlueprintType)
struct FSPResidentRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Job;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString WorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString HomeWorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString HomeCityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DestinationWorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Activity = TEXT("Off shift");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PartnerId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ShipId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Faction = TEXT("union");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PersonId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bContractRequested = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bContractPaid = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ShipIndex = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Shift = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Visits = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Deliveries = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Credits = 120;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Friendship = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Life = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FromNode = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ToNode = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Hunger = 15.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Fatigue = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double DepartTime = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double UntilTime = 0.0;
};

USTRUCT(BlueprintType)
struct FSPSocialBond
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString FirstId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SecondId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Trust = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Meetings = 0;
};

USTRUCT(BlueprintType)
struct FSPSocietyJob
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString WorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString IssuerId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Brief;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Kind;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Good;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Status = TEXT("offered");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Outcome;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Units = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Reward = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Stage = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUrgent = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double CreatedTime = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double DeadlineTime = 0.0;
};

USTRUCT(BlueprintType)
struct FSPMarketRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString WorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Biome;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Faction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Ore = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Organics = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Crystal = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Activity = 0;
};

USTRUCT(BlueprintType)
struct FSPConvoyRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString FromWorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ToWorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Good;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Faction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Units = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double ArriveTime = 0.0;
};

USTRUCT(BlueprintType)
struct FSPCargoEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CityId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Good;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Units = 0;
};

USTRUCT(BlueprintType)
struct FSPFreightContract
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString FromWorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ToWorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Good;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Status = TEXT("available");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Units = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Reward = 240;
};

USTRUCT(BlueprintType)
struct FSPSocietyNews
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString WorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Kind;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Summary;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double Time = 0.0;
};

/** Actor-free persistent state: all sectors advance even when their level is unloaded. */
UCLASS()
class SPACEPATRIOTUNREAL_API USPSocietySaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 1;
    UPROPERTY() double SimulationTime = 0.0;
    UPROPERTY() double PartialStepSeconds = 0.0;
    UPROPERTY() double NextTradeTime = 60.0;
    UPROPERTY() double NextStoryTime = 600.0;
    UPROPERTY() int64 SavedUtcSeconds = 0;
    UPROPERTY() int32 Sequence = 0;
    UPROPERTY() int32 TradeSequence = 0;
    UPROPERTY() int32 PlayerCredits = 1800;
    UPROPERTY() int32 UnionStanding = 0;
    UPROPERTY() int32 HelixStanding = 0;
    UPROPERTY() int32 RedwakeStanding = 0;
    UPROPERTY() int32 UnionTreasury = 60000;
    UPROPERTY() int32 HelixTreasury = 80000;
    UPROPERTY() int32 RedwakeTreasury = 45000;
    UPROPERTY() float UnionInfluence = 34.0f;
    UPROPERTY() float HelixInfluence = 36.0f;
    UPROPERTY() float RedwakeInfluence = 30.0f;
    UPROPERTY() int32 Spares = 0;
    UPROPERTY() int32 Alloy = 0;
    UPROPERTY() int32 Samples = 0;
    UPROPERTY() int32 HoldCapacity = 24;
    UPROPERTY() int32 HoldOrganics = 4;
    UPROPERTY() int32 HoldOre = 0;
    UPROPERTY() int32 HoldCrystal = 0;
    UPROPERTY() int32 RifleReserve = 0;
    UPROPERTY() TArray<FSPSocietySettlement> Settlements;
    UPROPERTY() TArray<FSPResidentRecord> Residents;
    UPROPERTY() TArray<FSPSocialBond> Bonds;
    UPROPERTY() TArray<FSPSocietyJob> Jobs;
    UPROPERTY() TArray<FSPMarketRecord> Markets;
    UPROPERTY() TArray<FSPConvoyRecord> Convoys;
    UPROPERTY() TArray<FSPCargoEntry> StagedCargo;
    UPROPERTY() TArray<FSPFreightContract> FreightContracts;
    UPROPERTY() TArray<FSPSocietyNews> News;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPSocietyNewsSignature, const FSPSocietyNews&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPSocietyJobSignature, const FSPSocietyJob&, Job);

/** Stable IDs, bounded fixed-step simulation, freight and local work for BP presentation. */
UCLASS(ClassGroup=(SpacePatriot), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class SPACEPATRIOTUNREAL_API USPSocietySimulationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPSocietySimulationComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Society|Save") FString SaveSlot = TEXT("SpacePatriotSociety");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Society|Save") bool bAutoLoad = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Society|Save") bool bAutoSave = true;
    UPROPERTY(BlueprintReadOnly, Category="Society") TObjectPtr<USPSocietySaveGame> State;
    UPROPERTY(BlueprintAssignable, Category="Society") FSPSocietyNewsSignature OnSocietyNews;
    UPROPERTY(BlueprintAssignable, Category="Society") FSPSocietyJobSignature OnSocietyJobChanged;

    UFUNCTION(BlueprintCallable, Category="Society|Save") bool InitializeSociety(bool bLoadExisting = true);
    UFUNCTION(BlueprintCallable, Category="Society|Save") bool SaveSociety();
    UFUNCTION(BlueprintCallable, Category="Society|Simulation") void AdvanceSimulation(float ElapsedSeconds);
    UFUNCTION(BlueprintCallable, Category="Society|Simulation") void AdvanceOfflineFromUtcNow();
    UFUNCTION(BlueprintPure, Category="Society|Simulation") double GetSimulationSeconds() const;
    UFUNCTION(BlueprintPure, Category="Society|World") TArray<FSPSocietySettlement> GetSettlementsForWorld(const FString& WorldId) const;
    UFUNCTION(BlueprintPure, Category="Society|World") TArray<FSPResidentRecord> GetResidentsForSettlement(const FString& CityId) const;
    UFUNCTION(BlueprintPure, Category="Society|World") TArray<FSPResidentRecord> GetPilotsForWorld(const FString& WorldId) const;
    UFUNCTION(BlueprintPure, Category="Society|World") TArray<FSPSocietyJob> GetJobsForSettlement(const FString& CityId) const;
    UFUNCTION(BlueprintPure, Category="Society|World") TArray<FSPSocietyNews> GetRecentNews(const FString& WorldId) const;
    UFUNCTION(BlueprintPure, Category="Society|World") bool GetSettlement(const FString& CityId, FSPSocietySettlement& OutSettlement) const;
    UFUNCTION(BlueprintPure, Category="Society|World") bool GetResident(const FString& ResidentId, FSPResidentRecord& OutResident) const;
    UFUNCTION(BlueprintPure, Category="Society|Economy") bool GetMarket(const FString& WorldId, FSPMarketRecord& OutMarket) const;
    UFUNCTION(BlueprintPure, Category="Society|Economy") int32 GetPrice(const FString& WorldId, const FString& Good, bool bBuy) const;
    UFUNCTION(BlueprintPure, Category="Society|Economy") int32 GetPlayerCredits() const;
    UFUNCTION(BlueprintPure, Category="Society|Economy") int32 GetPlayerSupply(const FString& Good) const;
    UFUNCTION(BlueprintCallable, Category="Society|Economy") bool AddPlayerSupply(const FString& Good, int32 Units);
    UFUNCTION(BlueprintPure, Category="Society|Economy") int32 GetFactionStanding(const FString& FactionId) const;
    UFUNCTION(BlueprintPure, Category="Society|Economy") int32 GetRifleReserve() const;
    UFUNCTION(BlueprintPure, Category="Society|Cargo") int32 GetStagedCargo(const FString& CityId, const FString& Good) const;
    UFUNCTION(BlueprintPure, Category="Society|Cargo") int32 GetHoldCargo(const FString& Good) const;
    UFUNCTION(BlueprintPure, Category="Society|Cargo") int32 GetHoldCapacity() const;
    UFUNCTION(BlueprintCallable, Category="Society|Cargo") bool BuyAtSettlement(const FString& CityId, const FString& Good, int32 Units);
    UFUNCTION(BlueprintCallable, Category="Society|Cargo") bool SellAtSettlement(const FString& CityId, const FString& Good, int32 Units);
    UFUNCTION(BlueprintCallable, Category="Society|Cargo") bool TransferCargo(const FString& CityId, const FString& Good, int32 Units, bool bLoad);
    UFUNCTION(BlueprintPure, Category="Society|Cargo") TArray<FSPFreightContract> GetFreightContractsForWorld(const FString& WorldId) const;
    UFUNCTION(BlueprintCallable, Category="Society|Cargo") bool AcceptFreightContract(const FString& FromCityId, const FString& ContractId);
    UFUNCTION(BlueprintCallable, Category="Society|Cargo") bool DeliverFreightContract(const FString& ToCityId, const FString& ContractId);
    UFUNCTION(BlueprintCallable, Category="Society|Jobs") bool AcceptJob(const FString& JobId);
    UFUNCTION(BlueprintCallable, Category="Society|Jobs") bool CompleteJobAtNode(const FString& JobId, const FString& CityId, int32 Node, bool bBusRepaired);
    UFUNCTION(BlueprintCallable, Category="Society|Residents") bool RestartResidentLife(const FString& ResidentId);
    UFUNCTION(BlueprintCallable, Category="Society|Residents") bool RequestNamedContract(const FString& ResidentId, FString& OutRequest);
    UFUNCTION(BlueprintCallable, Category="Society|Residents") bool FulfillNamedContract(const FString& ResidentId, const FString& CurrentCityId);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    struct FWorldSeed { FString Id; FString Name; FString System; FString Biome; int32 Seed = 0; };
    TArray<FWorldSeed> WorldCatalog;
    TMap<FString, int32> CityIndices;
    TMap<FString, int32> MarketIndices;
    TMap<FString, int32> ResidentIndices;
    TSet<FString> ActiveJobKinds;
    TMap<FString, int32> ActiveJobsByCity;
    void BuildNewState(const TArray<FSPSocietySettlement>& Catalog);
    void RebuildLookup();
    bool LoadCatalog(TArray<FSPSocietySettlement>& OutCatalog);
    void AdvanceFixedStep(float Seconds);
    void AdvanceCities(float Seconds);
    void AdvanceResidents(float Seconds);
    void AdvanceMarkets(float Seconds);
    void RollLocalStories();
    void OfferJob(const FSPSocietySettlement& City, const FString& Kind, const FString& Title, const FString& Brief, const FString& Good, int32 Units, int32 Reward, bool bUrgent);
    void AddNews(const FString& WorldId, const FString& Kind, const FString& Summary);
    void Remember(FSPResidentRecord& A, FSPResidentRecord& B);
    int32 FindSettlementIndex(const FString& CityId) const;
    int32 FindMarketIndex(const FString& WorldId) const;
    int32 FindResidentIndex(const FString& ResidentId) const;
    static bool ValidGood(const FString& Good);
    static float Stock(const FSPMarketRecord& Market, const FString& Good);
    static void AddStock(FSPMarketRecord& Market, const FString& Good, float Delta);
    int32& HoldUnits(const FString& Good);
    const int32& HoldUnits(const FString& Good) const;
};
