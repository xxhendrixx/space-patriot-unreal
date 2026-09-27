#include "SPSocietySimulationComponent.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr float FixedStepSeconds = 20.0f;
    constexpr float MaxOfflineSeconds = 6.0f * 3600.0f;
    constexpr int32 MaxJobs = 1000;
    constexpr int32 MaxNews = 48;
    constexpr int32 MaxConvoys = 60;

    TSharedPtr<FJsonObject> ReadJson(const FString& Path)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path)) return nullptr;
        TSharedPtr<FJsonObject> Root;
        return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) ? Root : nullptr;
    }

    FString StringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
    {
        FString Value;
        if (Object.IsValid()) Object->TryGetStringField(Field, Value);
        return Value;
    }

    int32 SeedField(const TSharedPtr<FJsonObject>& Object)
    {
        double Value = 0.0;
        if (Object.IsValid()) Object->TryGetNumberField(TEXT("seed"), Value);
        return static_cast<int32>(static_cast<uint32>(static_cast<int64>(Value)));
    }

    FString PrimaryCity(const FString& WorldId)
    {
        return WorldId + TEXT(":city:0");
    }

    FString JobKey(const FString& CityId, const FString& Kind)
    {
        return CityId + TEXT("|") + Kind;
    }

    float Towards(float Current, float Target, float MaxDelta)
    {
        return Current + FMath::Clamp(Target - Current, -MaxDelta, MaxDelta);
    }

    FString FactionForSeed(int32 Seed)
    {
        static const TCHAR* Factions[] = { TEXT("union"), TEXT("helix"), TEXT("redwake") };
        return Factions[static_cast<uint32>(Seed) % UE_ARRAY_COUNT(Factions)];
    }
}

USPSocietySimulationComponent::USPSocietySimulationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f;
}

void USPSocietySimulationComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoLoad && InitializeSociety(true)) AdvanceOfflineFromUtcNow();
}

void USPSocietySimulationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bAutoSave && State) SaveSociety();
    Super::EndPlay(EndPlayReason);
}

void USPSocietySimulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceSimulation(DeltaTime);
}

bool USPSocietySimulationComponent::LoadCatalog(TArray<FSPSocietySettlement>& OutCatalog)
{
    const FString DataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"));
    const TSharedPtr<FJsonObject> WorldRoot = ReadJson(FPaths::Combine(DataDir, TEXT("Worlds.json")));
    const TSharedPtr<FJsonObject> CityRoot = ReadJson(FPaths::Combine(DataDir, TEXT("Settlements.json")));
    if (!WorldRoot.IsValid() || !CityRoot.IsValid()) return false;

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    WorldCatalog.Reset();
    if (!WorldRoot->TryGetArrayField(TEXT("worlds"), Values) || !Values) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) continue;
        FWorldSeed World;
        World.Id = StringField(Obj, TEXT("id"));
        World.Name = StringField(Obj, TEXT("name"));
        World.System = StringField(Obj, TEXT("system"));
        World.Biome = StringField(Obj, TEXT("biome"));
        World.Seed = SeedField(Obj);
        if (!World.Id.IsEmpty()) WorldCatalog.Add(MoveTemp(World));
    }
    if (WorldCatalog.IsEmpty()) return false;

    Values = nullptr;
    OutCatalog.Reset();
    if (!CityRoot->TryGetArrayField(TEXT("settlements"), Values) || !Values) return false;
    TSet<FString> SeenIds;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid()) continue;
        FSPSocietySettlement City;
        City.Id = StringField(Obj, TEXT("id"));
        City.WorldId = StringField(Obj, TEXT("world"));
        City.Name = StringField(Obj, TEXT("name"));
        City.Kind = StringField(Obj, TEXT("kind"));
        City.Seed = SeedField(Obj);
        Obj->TryGetBoolField(TEXT("primary"), City.bPrimary);
        if (City.Id.IsEmpty() || SeenIds.Contains(City.Id) ||
            !WorldCatalog.ContainsByPredicate([&](const FWorldSeed& World) { return World.Id == City.WorldId; })) continue;
        SeenIds.Add(City.Id);
        OutCatalog.Add(MoveTemp(City));
    }
    return OutCatalog.Num() == 420 && WorldCatalog.Num() == 19;
}

bool USPSocietySimulationComponent::InitializeSociety(bool bLoadExisting)
{
    TArray<FSPSocietySettlement> Catalog;
    if (!LoadCatalog(Catalog)) return false;
    State = bLoadExisting ? Cast<USPSocietySaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)) : nullptr;
    if (!State || State->Version != 1 || State->Settlements.Num() != Catalog.Num() || State->Markets.Num() != WorldCatalog.Num() || State->Residents.Num() < 4000)
    {
        State = Cast<USPSocietySaveGame>(UGameplayStatics::CreateSaveGameObject(USPSocietySaveGame::StaticClass()));
        if (!State) return false;
        BuildNewState(Catalog);
    }
    RebuildLookup();
    return State->Residents.Num() > 4000 && State->Settlements.Num() == 420 && State->Markets.Num() == 19;
}

void USPSocietySimulationComponent::BuildNewState(const TArray<FSPSocietySettlement>& Catalog)
{
    State->Settlements = Catalog;
    static const TCHAR* FirstNames[] = { TEXT("Ivo"), TEXT("Mara"), TEXT("Sana"), TEXT("Dane"), TEXT("Tomas"), TEXT("Ari"), TEXT("Imani"), TEXT("Rin"), TEXT("Leah"), TEXT("Oren"), TEXT("Niko"), TEXT("Edda") };
    static const TCHAR* LastNames[] = { TEXT("Vale"), TEXT("Rook"), TEXT("Mercer"), TEXT("Okafor"), TEXT("Chen"), TEXT("Alvarez"), TEXT("Sato"), TEXT("Bell"), TEXT("Navarro"), TEXT("Hale"), TEXT("Singh"), TEXT("Ward") };
    static const TCHAR* Jobs[] = { TEXT("Freight pilot"), TEXT("Port engineer"), TEXT("Survey pilot"), TEXT("Dockworker"), TEXT("Patrol pilot"), TEXT("Merchant"), TEXT("Medic"), TEXT("Farmer") };
    static const TCHAR* LocalFirst[] = { TEXT("Tamsin"), TEXT("Elias"), TEXT("Nadia"), TEXT("Amir"), TEXT("Leonie"), TEXT("Ada"), TEXT("Jun"), TEXT("Kellan") };
    static const TCHAR* LocalLast[] = { TEXT("Cho"), TEXT("Venn"), TEXT("Serrat"), TEXT("Keene"), TEXT("Park"), TEXT("Okoro"), TEXT("Mercer"), TEXT("Reyes") };
    static const TCHAR* LocalJobs[] = { TEXT("Dockworker"), TEXT("Port engineer"), TEXT("Merchant"), TEXT("Medic"), TEXT("Farmer"), TEXT("Freight pilot") };

    for (int32 WorldIndex = 0; WorldIndex < WorldCatalog.Num(); ++WorldIndex)
    {
        const FWorldSeed& World = WorldCatalog[WorldIndex];
        FSPMarketRecord Market;
        Market.WorldId = World.Id;
        Market.Biome = World.Biome;
        Market.Faction = FactionForSeed(WorldIndex);
        Market.Ore = 90 + FMath::Abs(World.Seed % 130);
        Market.Organics = 90 + FMath::Abs((World.Seed / 3) % 130);
        Market.Crystal = 90 + FMath::Abs((World.Seed / 7) % 130);
        State->Markets.Add(MoveTemp(Market));

        for (int32 Index = 0; Index < 32; ++Index)
        {
            FSPResidentRecord Resident;
            Resident.Id = FString::Printf(TEXT("%s-resident-%d"), *World.Id, Index);
            Resident.Name = FString::Printf(TEXT("%s %s"), FirstNames[(Index + WorldIndex) % 12], LastNames[(Index * 5 + WorldIndex * 3) % 12]);
            Resident.Job = Jobs[Index % UE_ARRAY_COUNT(Jobs)];
            Resident.WorldId = World.Id;
            Resident.HomeWorldId = World.Id;
            Resident.CityId = PrimaryCity(World.Id);
            Resident.HomeCityId = Resident.CityId;
            Resident.Faction = FactionForSeed(WorldIndex);
            Resident.Shift = Index % 3;
            if (Resident.Job.Contains(TEXT("pilot"), ESearchCase::IgnoreCase))
            {
                Resident.ShipId = FString::Printf(TEXT("%s-vessel-%d"), *World.Id, Index);
                Resident.ShipIndex = (Index == 0 ? (WorldIndex % 5 == 0 ? 90 : 70) : Index % 8 == 0 ? 20 : Index % 8 == 2 ? 40 : 10) + Index % 10;
            }
            Resident.FromNode = Index % 6;
            Resident.ToNode = (Index + 1) % 6;
            Resident.UntilTime = 5 + Index * 2 + WorldIndex;
            State->Residents.Add(MoveTemp(Resident));
        }
        static const TCHAR* NamedIds[] = { TEXT("rook"), TEXT("vale"), TEXT("mara") };
        static const TCHAR* NamedJobs[] = { TEXT("Freight pilot"), TEXT("Merchant"), TEXT("Medic") };
        for (int32 Index = 0; Index < 3; ++Index)
        {
            FSPResidentRecord& Named = State->Residents[State->Residents.Num() - 32 + Index];
            Named.PersonId = NamedIds[Index];
            Named.Job = NamedJobs[Index];
            Named.FromNode = 7;
            Named.ToNode = 8;
            Named.UntilTime = 120.0;
            if (World.Name == TEXT("Earth"))
            {
                static const TCHAR* EarthNames[] = { TEXT("Ivo Rook"), TEXT("Seren Vale"), TEXT("Dr. Mara Sol") };
                Named.Name = EarthNames[Index];
            }
            if (Index > 0) { Named.ShipId.Reset(); Named.ShipIndex = 0; }
            else Named.ShipIndex = World.Seed % 5 == 0 ? 90 : 70;
        }
    }

    for (FSPSocietySettlement& City : State->Settlements)
    {
        City.Owner = FactionForSeed(City.Seed);
        City.Food = 45 + static_cast<uint32>(City.Seed) % 40;
        City.Power = 65 + static_cast<uint32>(City.Seed) % 31;
        City.Water = 60 + static_cast<uint32>(City.Seed) % 30;
        City.NextIncident = 120 + static_cast<uint32>(City.Seed) % 900;
        if (City.bPrimary) continue;
        const int32 Count = City.Kind == TEXT("city") ? 12 : 6;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FSPResidentRecord Resident;
            Resident.Id = FString::Printf(TEXT("%s-citizen-%d"), *City.Id, Index);
            Resident.Name = FString::Printf(TEXT("%s %s"), LocalFirst[(static_cast<uint32>(City.Seed) + Index) % 8], LocalLast[(static_cast<uint32>(City.Seed / 8) + Index * 3) % 8]);
            Resident.Job = LocalJobs[Index % UE_ARRAY_COUNT(LocalJobs)];
            Resident.WorldId = City.WorldId;
            Resident.HomeWorldId = City.WorldId;
            Resident.CityId = City.Id;
            Resident.HomeCityId = City.Id;
            Resident.Faction = City.Owner;
            Resident.Shift = Index % 3;
            Resident.FromNode = 5;
            Resident.ToNode = Index % 7;
            Resident.UntilTime = 30 + Index * 19 + static_cast<uint32>(City.Seed) % 60;
            if (Resident.Job.Contains(TEXT("pilot"), ESearchCase::IgnoreCase))
            {
                Resident.ShipId = FString::Printf(TEXT("%s-ship-%d"), *City.Id, Index);
                Resident.ShipIndex = 20 + static_cast<uint32>(City.Seed) % 10;
            }
            State->Residents.Add(MoveTemp(Resident));
        }
    }
}

void USPSocietySimulationComponent::RebuildLookup()
{
    CityIndices.Reset(); MarketIndices.Reset(); ResidentIndices.Reset();
    if (!State) return;
    for (int32 Index = 0; Index < State->Settlements.Num(); ++Index) CityIndices.Add(State->Settlements[Index].Id, Index);
    for (int32 Index = 0; Index < State->Markets.Num(); ++Index) MarketIndices.Add(State->Markets[Index].WorldId, Index);
    for (int32 Index = 0; Index < State->Residents.Num(); ++Index) ResidentIndices.Add(State->Residents[Index].Id, Index);
}

bool USPSocietySimulationComponent::SaveSociety()
{
    if (!State || SaveSlot.IsEmpty()) return false;
    State->SavedUtcSeconds = FDateTime::UtcNow().ToUnixTimestamp();
    return UGameplayStatics::SaveGameToSlot(State, SaveSlot, 0);
}

void USPSocietySimulationComponent::AdvanceOfflineFromUtcNow()
{
    if (!State) return;
    const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
    if (State->SavedUtcSeconds > 0 && Now > State->SavedUtcSeconds)
    {
        AdvanceSimulation(static_cast<float>(FMath::Min<int64>(Now - State->SavedUtcSeconds, static_cast<int64>(MaxOfflineSeconds))));
    }
    State->SavedUtcSeconds = Now;
}

void USPSocietySimulationComponent::AdvanceSimulation(float ElapsedSeconds)
{
    if (!State || !FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds <= 0.0f) return;
    State->PartialStepSeconds += FMath::Min(ElapsedSeconds, MaxOfflineSeconds);
    int32 Steps = 0;
    while (State->PartialStepSeconds >= FixedStepSeconds && Steps++ < static_cast<int32>(MaxOfflineSeconds / FixedStepSeconds))
    {
        State->PartialStepSeconds -= FixedStepSeconds;
        AdvanceFixedStep(FixedStepSeconds);
    }
}

double USPSocietySimulationComponent::GetSimulationSeconds() const
{
    return State ? State->SimulationTime : 0.0;
}

void USPSocietySimulationComponent::AdvanceFixedStep(float Seconds)
{
    State->SimulationTime += Seconds;
    AdvanceCities(Seconds);
    AdvanceMarkets(Seconds);
    AdvanceResidents(Seconds);
    if (State->SimulationTime >= State->NextStoryTime)
    {
        State->NextStoryTime = State->SimulationTime + 600.0;
        RollLocalStories();
    }
}

void USPSocietySimulationComponent::AdvanceCities(float Seconds)
{
    ActiveJobKinds.Reset(); ActiveJobsByCity.Reset();
    for (FSPSocietyJob& Job : State->Jobs)
    {
        if ((Job.Status == TEXT("offered") || Job.Status == TEXT("accepted")) && State->SimulationTime > Job.DeadlineTime)
        {
            Job.Status = TEXT("expired");
            Job.Outcome = TEXT("The requester reassigned the job after its deadline.");
            OnSocietyJobChanged.Broadcast(Job);
        }
        if (Job.Status == TEXT("offered") || Job.Status == TEXT("accepted"))
        {
            ActiveJobKinds.Add(JobKey(Job.CityId, Job.Kind));
            ActiveJobsByCity.FindOrAdd(Job.CityId)++;
        }
    }
    if (State->Jobs.Num() >= MaxJobs)
    {
        State->Jobs.RemoveAll([this](const FSPSocietyJob& Job)
        {
            return (Job.Status == TEXT("expired") || Job.Status == TEXT("completed")) &&
                State->SimulationTime - Job.CreatedTime > 3600.0;
        });
    }

    for (FSPSocietySettlement& City : State->Settlements)
    {
        City.Food = FMath::Clamp(City.Food - Seconds * 0.008f, 0.0f, 120.0f);
        City.Water = FMath::Clamp(City.Water - Seconds * 0.004f, 0.0f, 120.0f);
        const float Welfare = (City.Food + City.Water + City.Power + City.Security) * 0.25f;
        City.Health = Towards(City.Health, Welfare, Seconds * 0.007f);
        City.Prosperity = Towards(City.Prosperity, (City.Health + City.Security) * 0.5f, Seconds * 0.002f);
        if (State->SimulationTime >= City.NextIncident)
        {
            City.Days++;
            City.NextIncident = State->SimulationTime + 720 + (City.Days * 97 + City.Id.Len() * 31) % 480;
            switch (City.Days % 4)
            {
            case 0: City.Food = FMath::Max(8.0f, City.Food - 16.0f); break;
            case 1: City.Power = FMath::Max(20.0f, City.Power - 22.0f); break;
            case 2: City.Security = FMath::Max(25.0f, City.Security - 12.0f); City.Crime++; break;
            default: City.Water = FMath::Max(15.0f, City.Water - 18.0f); break;
            }
        }
        if (City.Power < 78.0f) OfferJob(City, TEXT("repair"), TEXT("District bus fault"), TEXT("Bring two repair components and reconnect the district bus."), TEXT("spares"), 2, 160, true);
        if (City.Food < 68.0f) OfferJob(City, TEXT("food"), TEXT("Canteen supplies running low"), TEXT("Unload four food crates at the receiving dock."), TEXT("organics"), 4, 210, City.Food < 30.0f);
        if (City.Water < 70.0f) OfferJob(City, TEXT("water"), TEXT("Waterworks filter replacement"), TEXT("Fit three alloy filters at the utility station."), TEXT("alloy"), 3, 140, true);
        if (City.Security < 78.0f) OfferJob(City, TEXT("patrol"), TEXT("Inspect the outer service route"), TEXT("Verify the dock, market and utility checkpoints."), TEXT(""), 3, 190, false);
        if (City.Days % 3 == 1) OfferJob(City, TEXT("survey"), TEXT("Assay the regional geology"), TEXT("Deliver a field sample to the research clinic."), TEXT("sample"), 1, 180, false);
        if (City.Health < 65.0f) OfferJob(City, TEXT("medical"), TEXT("Clinic equipment shortage"), TEXT("Bring two service components to the clinic."), TEXT("spares"), 2, 175, true);
        if (City.Days % 4 == 2) OfferJob(City, TEXT("courier"), TEXT("A misrouted shipping ledger"), TEXT("Collect a signed ledger and carry it to the exchange."), TEXT(""), 1, 120, false);
    }
}

void USPSocietySimulationComponent::OfferJob(const FSPSocietySettlement& City, const FString& Kind, const FString& Title, const FString& Brief, const FString& Good, int32 Units, int32 Reward, bool bUrgent)
{
    if (State->Jobs.Num() >= MaxJobs || ActiveJobsByCity.FindRef(City.Id) >= 2 || ActiveJobKinds.Contains(JobKey(City.Id, Kind))) return;
    FSPSocietyJob Job;
    Job.Id = FString::Printf(TEXT("society-%d"), ++State->Sequence);
    Job.CityId = City.Id;
    Job.WorldId = City.WorldId;
    const FString IssuerJob = Kind == TEXT("repair") || Kind == TEXT("water") ? TEXT("Port engineer") : Kind == TEXT("medical") ? TEXT("Medic") : TEXT("Merchant");
    const FSPResidentRecord* Issuer = State->Residents.FindByPredicate([&](const FSPResidentRecord& Person)
    {
        return Person.HomeCityId == City.Id && Person.Job == IssuerJob;
    });
    Job.IssuerId = Issuer ? Issuer->Id : City.Id;
    Job.Title = Title;
    Job.Brief = Brief;
    Job.Kind = Kind;
    Job.Good = Good;
    Job.Units = Units;
    Job.Reward = Reward;
    Job.bUrgent = bUrgent;
    Job.CreatedTime = State->SimulationTime;
    Job.DeadlineTime = State->SimulationTime + 2400.0;
    State->Jobs.Add(Job);
    ActiveJobKinds.Add(JobKey(City.Id, Kind));
    ActiveJobsByCity.FindOrAdd(City.Id)++;
    OnSocietyJobChanged.Broadcast(Job);
}

void USPSocietySimulationComponent::AdvanceMarkets(float Seconds)
{
    for (FSPMarketRecord& Market : State->Markets)
    {
        const float OreRate = (Market.Biome == TEXT("rock") || Market.Biome == TEXT("desert") || Market.Biome == TEXT("volcanic") ? 0.35f : 0.08f) - 0.11f;
        const float FoodRate = (Market.Biome == TEXT("temperate") ? 0.42f : 0.015f) - 0.10f;
        const float CrystalRate = (Market.Biome == TEXT("gas") || Market.Biome == TEXT("ice") || Market.Biome == TEXT("volcanic") ? 0.22f : 0.045f) - 0.055f;
        Market.Ore = FMath::Clamp(Market.Ore + OreRate * Seconds, 5.0f, 600.0f);
        Market.Organics = FMath::Clamp(Market.Organics + FoodRate * Seconds, 5.0f, 600.0f);
        Market.Crystal = FMath::Clamp(Market.Crystal + CrystalRate * Seconds, 5.0f, 600.0f);
    }

    if (State->SimulationTime >= State->NextTradeTime && WorldCatalog.Num() > 1)
    {
        State->NextTradeTime = State->SimulationTime + 60.0;
        if (State->Convoys.Num() < MaxConvoys)
        {
            const int32 Serial = State->TradeSequence++;
            const FWorldSeed& Source = WorldCatalog[Serial % WorldCatalog.Num()];
            int32 Destination = (Serial + 7) % WorldCatalog.Num();
            for (int32 Offset = 1; Offset < WorldCatalog.Num(); ++Offset)
            {
                const int32 Candidate = (Serial + Offset) % WorldCatalog.Num();
                if (WorldCatalog[Candidate].Id != Source.Id && WorldCatalog[Candidate].System == Source.System) { Destination = Candidate; break; }
            }
            if (WorldCatalog[Destination].Id == Source.Id) Destination = (Serial + 1) % WorldCatalog.Num();
            static const TCHAR* Goods[] = { TEXT("ore"), TEXT("organics"), TEXT("crystal") };
            const FString Good = Goods[Serial % UE_ARRAY_COUNT(Goods)];
            const int32 MarketIndex = FindMarketIndex(Source.Id);
            if (MarketIndex != INDEX_NONE)
            {
                FSPMarketRecord& Market = State->Markets[MarketIndex];
                const int32 Units = FMath::Min(14, FMath::FloorToInt(Stock(Market, Good) / 5.0f));
                int32* Treasury = Market.Faction == TEXT("union") ? &State->UnionTreasury : Market.Faction == TEXT("helix") ? &State->HelixTreasury : &State->RedwakeTreasury;
                const int32 Cost = Units * GetPrice(Source.Id, Good, false);
                if (Units > 0 && *Treasury >= Cost)
                {
                    AddStock(Market, Good, -Units);
                    *Treasury -= Cost;
                    FSPConvoyRecord Convoy;
                    Convoy.Id = FString::Printf(TEXT("trade-%d"), Serial);
                    Convoy.FromWorldId = Source.Id;
                    Convoy.ToWorldId = WorldCatalog[Destination].Id;
                    Convoy.Good = Good;
                    Convoy.Faction = Market.Faction;
                    Convoy.Units = Units;
                    Convoy.ArriveTime = State->SimulationTime + 65 + (Serial % 7) * 18;
                    State->Convoys.Add(MoveTemp(Convoy));
                }
            }
        }
    }
    for (int32 Index = State->Convoys.Num() - 1; Index >= 0; --Index)
    {
        const FSPConvoyRecord Convoy = State->Convoys[Index];
        if (Convoy.ArriveTime > State->SimulationTime) continue;
        const int32 MarketIndex = FindMarketIndex(Convoy.ToWorldId);
        if (MarketIndex != INDEX_NONE)
        {
            FSPMarketRecord& Market = State->Markets[MarketIndex];
            AddStock(Market, Convoy.Good, Convoy.Units);
            Market.Activity++;
            int32* Treasury = Convoy.Faction == TEXT("union") ? &State->UnionTreasury : Convoy.Faction == TEXT("helix") ? &State->HelixTreasury : &State->RedwakeTreasury;
            float* Influence = Convoy.Faction == TEXT("union") ? &State->UnionInfluence : Convoy.Faction == TEXT("helix") ? &State->HelixInfluence : &State->RedwakeInfluence;
            *Treasury += Convoy.Units * GetPrice(Convoy.ToWorldId, Convoy.Good, true);
            *Influence = FMath::Clamp(*Influence + 0.08f, 10.0f, 80.0f);
            AddNews(Convoy.ToWorldId, TEXT("trade"), FString::Printf(TEXT("%s delivered %d %s from %s."), *Convoy.Id, Convoy.Units, *Convoy.Good, *Convoy.FromWorldId));
        }
        State->Convoys.RemoveAt(Index);
    }
}

void USPSocietySimulationComponent::AdvanceResidents(float Seconds)
{
    TMap<FString, int32> WaitingAtCanteen;
    for (int32 Index = 0; Index < State->Residents.Num(); ++Index)
    {
        FSPResidentRecord& Resident = State->Residents[Index];
        Resident.Hunger = FMath::Clamp(Resident.Hunger + Seconds * 0.022f, 0.0f, 100.0f);
        Resident.Fatigue = FMath::Clamp(Resident.Fatigue + Seconds * 0.015f, 0.0f, 100.0f);
        if (State->SimulationTime < Resident.UntilTime) continue;

        if (Resident.Activity == TEXT("In transit"))
        {
            Resident.WorldId = Resident.DestinationWorldId;
            Resident.CityId = Resident.WorldId == Resident.HomeWorldId ? Resident.HomeCityId : PrimaryCity(Resident.WorldId);
            Resident.Deliveries++;
            Resident.Credits += Resident.Job == TEXT("Freight pilot") ? 45 : 24;
            const int32 MarketIndex = FindMarketIndex(Resident.WorldId);
            if (MarketIndex != INDEX_NONE && Resident.Job == TEXT("Freight pilot"))
            {
                AddStock(State->Markets[MarketIndex], TEXT("organics"), 6.0f);
                State->Markets[MarketIndex].Activity++;
            }
            AddNews(Resident.WorldId, TEXT("arrival"), Resident.Name + TEXT(" arrived aboard ") + Resident.ShipId + TEXT(" after a ") + Resident.Job + TEXT(" sortie."));
            Resident.Activity = TEXT("Dock service");
            Resident.FromNode = 0;
            Resident.ToNode = 1;
            Resident.DepartTime = State->SimulationTime;
            Resident.UntilTime = State->SimulationTime + 55.0;
            continue;
        }
        if (Resident.Activity == TEXT("Resting")) Resident.Fatigue = FMath::Max(0.0f, Resident.Fatigue - 60.0f);
        if (Resident.Activity == TEXT("Meal break"))
        {
            Resident.Hunger = FMath::Max(0.0f, Resident.Hunger - 65.0f);
            Resident.Credits = FMath::Max(0, Resident.Credits - 6);
        }
        if (Resident.Activity == TEXT("Working"))
        {
            Resident.Credits += 12;
            const int32 CityIndex = FindSettlementIndex(Resident.CityId);
            if (CityIndex != INDEX_NONE)
            {
                FSPSocietySettlement& City = State->Settlements[CityIndex];
                if (Resident.Job == TEXT("Farmer")) City.Food = FMath::Min(120.0f, City.Food + 2.0f);
                if (Resident.Job == TEXT("Port engineer")) City.Power = FMath::Min(100.0f, City.Power + 0.3f);
                if (Resident.Job == TEXT("Medic") && City.Power > 30.0f && City.Water > 15.0f) City.Health = FMath::Min(100.0f, City.Health + 0.4f);
                if (Resident.Job == TEXT("Patrol pilot")) City.Security = FMath::Min(100.0f, City.Security + 1.0f);
                if (Resident.Job == TEXT("Dockworker")) City.Prosperity = FMath::Min(100.0f, City.Prosperity + 0.15f);
            }
            const int32 MarketIndex = FindMarketIndex(Resident.WorldId);
            if (MarketIndex != INDEX_NONE)
            {
                FSPMarketRecord& Market = State->Markets[MarketIndex];
                if (Resident.Job == TEXT("Farmer")) AddStock(Market, TEXT("organics"), 3.0f);
                if (Resident.Job == TEXT("Dockworker")) Market.Activity++;
                if (Resident.Job == TEXT("Port engineer")) AddStock(Market, TEXT("ore"), -FMath::Min(1.0f, Market.Ore));
            }
        }
        Resident.FromNode = Resident.ToNode;
        Resident.DepartTime = State->SimulationTime;
        Resident.PartnerId.Reset();
        Resident.Visits++;
        if (Resident.Hunger > 65.0f)
        {
            Resident.Activity = TEXT("Meal break"); Resident.ToNode = 3; Resident.UntilTime = State->SimulationTime + 80.0;
        }
        else if (Resident.Fatigue > 72.0f || (static_cast<int32>(State->SimulationTime / 600.0) % 3 != Resident.Shift && Resident.Visits % 3 == 0))
        {
            Resident.Activity = TEXT("Resting"); Resident.ToNode = 5; Resident.UntilTime = State->SimulationTime + 180.0;
        }
        else if (!Resident.ShipId.IsEmpty() && WorldCatalog.Num() > 1)
        {
            int32 CurrentWorld = WorldCatalog.IndexOfByPredicate([&](const FWorldSeed& W) { return W.Id == Resident.WorldId; });
            if (CurrentWorld == INDEX_NONE) CurrentWorld = 0;
            int32 Destination = (CurrentWorld + 1 + Resident.Visits % (WorldCatalog.Num() - 1)) % WorldCatalog.Num();
            if (Resident.Job == TEXT("Freight pilot"))
            {
                const int32 FromMarket = FindMarketIndex(Resident.WorldId);
                if (FromMarket == INDEX_NONE || State->Markets[FromMarket].Organics < 8.0f)
                {
                    Resident.Activity = TEXT("Awaiting freight"); Resident.ToNode = 1; Resident.UntilTime = State->SimulationTime + 35.0; continue;
                }
                AddStock(State->Markets[FromMarket], TEXT("organics"), -6.0f);
                float LowestFood = TNumericLimits<float>::Max();
                for (int32 Candidate = 0; Candidate < WorldCatalog.Num(); ++Candidate)
                {
                    if (Candidate == CurrentWorld) continue;
                    const int32 CandidateMarket = FindMarketIndex(WorldCatalog[Candidate].Id);
                    if (CandidateMarket != INDEX_NONE && State->Markets[CandidateMarket].Organics < LowestFood)
                    {
                        LowestFood = State->Markets[CandidateMarket].Organics;
                        Destination = Candidate;
                    }
                }
            }
            if (Resident.Job == TEXT("Patrol pilot") && Resident.Visits % 3 != 0) Destination = CurrentWorld;
            Resident.DestinationWorldId = WorldCatalog[Destination].Id;
            Resident.Activity = TEXT("In transit");
            Resident.ToNode = 0;
            Resident.UntilTime = State->SimulationTime + 150 + Resident.Visits % 5 * 30;
        }
        else if (Resident.Visits % 4 == 0)
        {
            Resident.Activity = TEXT("Social break"); Resident.ToNode = 3; Resident.UntilTime = State->SimulationTime + 60.0;
            if (int32* OtherIndex = WaitingAtCanteen.Find(Resident.CityId))
            {
                FSPResidentRecord& Other = State->Residents[*OtherIndex];
                Resident.PartnerId = Other.Id;
                Other.PartnerId = Resident.Id;
                Resident.Friendship++;
                Other.Friendship++;
                Remember(Resident, Other);
                AddNews(Resident.WorldId, TEXT("friendship"), Resident.Name + TEXT(" and ") + Other.Name + TEXT(" met at the port canteen."));
                WaitingAtCanteen.Remove(Resident.CityId);
            }
            else WaitingAtCanteen.Add(Resident.CityId, Index);
        }
        else
        {
            Resident.Activity = TEXT("Working");
            Resident.ToNode = Resident.Job == TEXT("Port engineer") ? 2 : Resident.Job == TEXT("Merchant") ? 3 : Resident.Job == TEXT("Medic") ? 4 : Resident.Job == TEXT("Farmer") ? 6 : 1;
            Resident.UntilTime = State->SimulationTime + 90 + Resident.Visits % 4 * 20;
        }
        if (Resident.Activity != TEXT("In transit")) Resident.UntilTime += 10.0 + FMath::Abs(Resident.FromNode - Resident.ToNode) * 8.0;
    }
}

void USPSocietySimulationComponent::Remember(FSPResidentRecord& A, FSPResidentRecord& B)
{
    const FString& First = A.Id < B.Id ? A.Id : B.Id;
    const FString& Second = A.Id < B.Id ? B.Id : A.Id;
    FSPSocialBond* Bond = State->Bonds.FindByPredicate([&](const FSPSocialBond& Entry) { return Entry.FirstId == First && Entry.SecondId == Second; });
    if (!Bond)
    {
        FSPSocialBond NewBond; NewBond.FirstId = First; NewBond.SecondId = Second;
        Bond = &State->Bonds.Add_GetRef(MoveTemp(NewBond));
    }
    Bond->Meetings++;
    Bond->Trust = FMath::Min(100, Bond->Trust + 1);
}

void USPSocietySimulationComponent::RollLocalStories()
{
    static const TCHAR* Kinds[] = { TEXT("romance"), TEXT("intrigue"), TEXT("betrayal"), TEXT("alliance"), TEXT("good business"), TEXT("bad business"), TEXT("family feud"), TEXT("rescue") };
    const int32 Epoch = static_cast<int32>(State->SimulationTime / 600.0);
    for (const FWorldSeed& World : WorldCatalog)
    {
        TArray<int32> LocalPeople;
        for (int32 Index = 0; Index < State->Residents.Num(); ++Index)
            if (State->Residents[Index].WorldId == World.Id) LocalPeople.Add(Index);
        if (LocalPeople.Num() < 2) continue;
        FRandomStream Roll(World.Seed ^ static_cast<int32>(FCrc::StrCrc32(*World.Id)) ^ (Epoch * 104729));
        const int32 APosition = Roll.RandRange(0, LocalPeople.Num() - 1);
        int32 BPosition = Roll.RandRange(0, LocalPeople.Num() - 2);
        if (BPosition >= APosition) BPosition++;
        const int32 AIndex = LocalPeople[APosition];
        const int32 OtherIndex = LocalPeople[BPosition];
        FSPResidentRecord& A = State->Residents[AIndex];
        FSPResidentRecord& B = State->Residents[OtherIndex];
        const FString Kind = Kinds[Roll.RandRange(0, UE_ARRAY_COUNT(Kinds) - 1)];
        if (Kind == TEXT("romance") || Kind == TEXT("alliance") || Kind == TEXT("rescue"))
        {
            Remember(A, B); A.Friendship++; B.Friendship++;
        }
        else if (Kind == TEXT("betrayal") || Kind == TEXT("family feud"))
        {
            const FString& First = A.Id < B.Id ? A.Id : B.Id;
            const FString& Second = A.Id < B.Id ? B.Id : A.Id;
            FSPSocialBond* Bond = State->Bonds.FindByPredicate([&](const FSPSocialBond& Entry) { return Entry.FirstId == First && Entry.SecondId == Second; });
            if (Bond) Bond->Trust = FMath::Max(-100, Bond->Trust - 12);
            const int32 CityIndex = FindSettlementIndex(A.CityId);
            if (CityIndex != INDEX_NONE) State->Settlements[CityIndex].Security = FMath::Max(0.0f, State->Settlements[CityIndex].Security - 0.5f);
        }
        else if (Kind == TEXT("good business"))
        {
            A.Credits += 15; B.Credits += 10;
            const int32 MarketIndex = FindMarketIndex(World.Id);
            if (MarketIndex != INDEX_NONE) State->Markets[MarketIndex].Activity++;
        }
        else if (Kind == TEXT("bad business")) { A.Credits = FMath::Max(0, A.Credits - 12); B.Credits = FMath::Max(0, B.Credits - 8); }
        AddNews(World.Id, Kind, A.Name + TEXT(" and ") + B.Name + TEXT(" were drawn into ") + Kind + TEXT(".") );
    }
}

void USPSocietySimulationComponent::AddNews(const FString& WorldId, const FString& Kind, const FString& Summary)
{
    FSPSocietyNews Item;
    Item.Id = FString::Printf(TEXT("news-%d"), ++State->Sequence);
    Item.WorldId = WorldId;
    Item.Kind = Kind;
    Item.Summary = Summary;
    Item.Time = State->SimulationTime;
    State->News.Add(Item);
    if (State->News.Num() > MaxNews) State->News.RemoveAt(0, State->News.Num() - MaxNews);
    OnSocietyNews.Broadcast(Item);
}

TArray<FSPSocietySettlement> USPSocietySimulationComponent::GetSettlementsForWorld(const FString& WorldId) const
{
    TArray<FSPSocietySettlement> Result;
    if (State) for (const FSPSocietySettlement& Item : State->Settlements) if (Item.WorldId == WorldId) Result.Add(Item);
    return Result;
}

TArray<FSPResidentRecord> USPSocietySimulationComponent::GetResidentsForSettlement(const FString& CityId) const
{
    TArray<FSPResidentRecord> Result;
    if (State) for (const FSPResidentRecord& Item : State->Residents) if (Item.CityId == CityId) Result.Add(Item);
    return Result;
}

TArray<FSPResidentRecord> USPSocietySimulationComponent::GetPilotsForWorld(const FString& WorldId) const
{
    TArray<FSPResidentRecord> Result;
    if (State) for (const FSPResidentRecord& Item : State->Residents)
        if (!Item.ShipId.IsEmpty() && (Item.WorldId == WorldId || Item.DestinationWorldId == WorldId)) Result.Add(Item);
    return Result;
}

TArray<FSPSocietyJob> USPSocietySimulationComponent::GetJobsForSettlement(const FString& CityId) const
{
    TArray<FSPSocietyJob> Result;
    if (State) for (const FSPSocietyJob& Item : State->Jobs) if (Item.CityId == CityId && (Item.Status == TEXT("offered") || Item.Status == TEXT("accepted"))) Result.Add(Item);
    return Result;
}

TArray<FSPSocietyNews> USPSocietySimulationComponent::GetRecentNews(const FString& WorldId) const
{
    TArray<FSPSocietyNews> Result;
    if (State) for (const FSPSocietyNews& Item : State->News) if (WorldId.IsEmpty() || Item.WorldId == WorldId) Result.Add(Item);
    return Result;
}

bool USPSocietySimulationComponent::GetSettlement(const FString& CityId, FSPSocietySettlement& OutSettlement) const
{
    const int32 Index = FindSettlementIndex(CityId);
    if (Index == INDEX_NONE) return false;
    OutSettlement = State->Settlements[Index];
    return true;
}

bool USPSocietySimulationComponent::GetResident(const FString& ResidentId, FSPResidentRecord& OutResident) const
{
    const int32 Index = FindResidentIndex(ResidentId);
    if (Index == INDEX_NONE) return false;
    OutResident = State->Residents[Index];
    return true;
}

bool USPSocietySimulationComponent::GetMarket(const FString& WorldId, FSPMarketRecord& OutMarket) const
{
    const int32 Index = FindMarketIndex(WorldId);
    if (Index == INDEX_NONE) return false;
    OutMarket = State->Markets[Index];
    return true;
}

int32 USPSocietySimulationComponent::GetPrice(const FString& WorldId, const FString& Good, bool bBuy) const
{
    if (!ValidGood(Good)) return 0;
    const int32 MarketIndex = FindMarketIndex(WorldId);
    if (MarketIndex == INDEX_NONE) return Good == TEXT("ore") ? 29 : Good == TEXT("crystal") ? 65 : 38;
    const FSPMarketRecord& Market = State->Markets[MarketIndex];
    const float Influence = Market.Faction == TEXT("union") ? State->UnionInfluence : Market.Faction == TEXT("helix") ? State->HelixInfluence : State->RedwakeInfluence;
    const float Basis = Good == TEXT("ore") ? 26.0f : Good == TEXT("crystal") ? 58.0f : 34.0f;
    return FMath::Max(1, FMath::RoundToInt(Basis * FMath::Clamp(180.0f / FMath::Max(40.0f, Stock(Market, Good)), 0.55f, 3.0f) * (1.0f + (100.0f - Influence) * 0.001f) * (bBuy ? 1.12f : 0.9f)));
}

int32 USPSocietySimulationComponent::GetPlayerCredits() const
{
    return State ? State->PlayerCredits : 0;
}

int32 USPSocietySimulationComponent::GetPlayerSupply(const FString& Good) const
{
    if (!State) return 0;
    if (Good == TEXT("spares")) return State->Spares;
    if (Good == TEXT("alloy")) return State->Alloy;
    if (Good == TEXT("sample")) return State->Samples;
    return 0;
}

bool USPSocietySimulationComponent::AddPlayerSupply(const FString& Good, int32 Units)
{
    if (!State || Units == 0) return false;
    int32* Stockpile = Good == TEXT("spares") ? &State->Spares : Good == TEXT("alloy") ? &State->Alloy : Good == TEXT("sample") ? &State->Samples : nullptr;
    if (!Stockpile || static_cast<int64>(*Stockpile) + Units < 0 || static_cast<int64>(*Stockpile) + Units > 9999) return false;
    *Stockpile += Units;
    return true;
}

int32 USPSocietySimulationComponent::GetFactionStanding(const FString& FactionId) const
{
    if (!State) return 0;
    return FactionId == TEXT("union") ? State->UnionStanding : FactionId == TEXT("helix") ? State->HelixStanding : FactionId == TEXT("redwake") ? State->RedwakeStanding : 0;
}

int32 USPSocietySimulationComponent::GetRifleReserve() const
{
    return State ? State->RifleReserve : 0;
}

int32 USPSocietySimulationComponent::GetStagedCargo(const FString& CityId, const FString& Good) const
{
    if (!State) return 0;
    const FSPCargoEntry* Entry = State->StagedCargo.FindByPredicate([&](const FSPCargoEntry& Item) { return Item.CityId == CityId && Item.Good == Good; });
    return Entry ? Entry->Units : 0;
}

int32 USPSocietySimulationComponent::GetHoldCargo(const FString& Good) const
{
    return State && ValidGood(Good) ? HoldUnits(Good) : 0;
}

int32 USPSocietySimulationComponent::GetHoldCapacity() const
{
    return State ? State->HoldCapacity : 0;
}

bool USPSocietySimulationComponent::BuyAtSettlement(const FString& CityId, const FString& Good, int32 Units)
{
    if (!State || !ValidGood(Good) || Units <= 0 || Units > 1000) return false;
    const int32 CityIndex = FindSettlementIndex(CityId);
    if (CityIndex == INDEX_NONE) return false;
    const int32 MarketIndex = FindMarketIndex(State->Settlements[CityIndex].WorldId);
    if (MarketIndex == INDEX_NONE || Stock(State->Markets[MarketIndex], Good) < Units) return false;
    const int64 Cost = static_cast<int64>(GetPrice(State->Markets[MarketIndex].WorldId, Good, true)) * Units;
    if (Cost > State->PlayerCredits) return false;
    AddStock(State->Markets[MarketIndex], Good, -Units);
    State->PlayerCredits -= static_cast<int32>(Cost);
    FSPCargoEntry* Entry = State->StagedCargo.FindByPredicate([&](const FSPCargoEntry& Item) { return Item.CityId == CityId && Item.Good == Good; });
    if (!Entry) { FSPCargoEntry NewEntry; NewEntry.CityId = CityId; NewEntry.Good = Good; Entry = &State->StagedCargo.Add_GetRef(NewEntry); }
    Entry->Units += Units;
    return true;
}

bool USPSocietySimulationComponent::SellAtSettlement(const FString& CityId, const FString& Good, int32 Units)
{
    if (!State || !ValidGood(Good) || Units <= 0 || Units > GetStagedCargo(CityId, Good)) return false;
    const int32 CityIndex = FindSettlementIndex(CityId);
    if (CityIndex == INDEX_NONE) return false;
    const int32 MarketIndex = FindMarketIndex(State->Settlements[CityIndex].WorldId);
    if (MarketIndex == INDEX_NONE) return false;
    const int64 Income = static_cast<int64>(GetPrice(State->Markets[MarketIndex].WorldId, Good, false)) * Units;
    if (Income > MAX_int32 - State->PlayerCredits) return false;
    AddStock(State->Markets[MarketIndex], Good, Units);
    State->PlayerCredits += static_cast<int32>(Income);
    FSPCargoEntry* Entry = State->StagedCargo.FindByPredicate([&](const FSPCargoEntry& Item) { return Item.CityId == CityId && Item.Good == Good; });
    Entry->Units -= Units;
    return true;
}

bool USPSocietySimulationComponent::TransferCargo(const FString& CityId, const FString& Good, int32 Units, bool bLoad)
{
    if (!State || !ValidGood(Good) || Units <= 0 || FindSettlementIndex(CityId) == INDEX_NONE) return false;
    int32& Hold = HoldUnits(Good);
    FSPCargoEntry* Entry = State->StagedCargo.FindByPredicate([&](const FSPCargoEntry& Item) { return Item.CityId == CityId && Item.Good == Good; });
    if (bLoad)
    {
        if (!Entry || Entry->Units < Units || State->HoldOrganics + State->HoldOre + State->HoldCrystal + Units > State->HoldCapacity) return false;
        Entry->Units -= Units; Hold += Units;
        for (FSPFreightContract& Contract : State->FreightContracts)
            if (Contract.Status == TEXT("awaiting loading") && Contract.Good == Good && Hold >= Contract.Units)
                Contract.Status = TEXT("in transit");
    }
    else
    {
        if (Hold < Units) return false;
        if (!Entry) { FSPCargoEntry NewEntry; NewEntry.CityId = CityId; NewEntry.Good = Good; Entry = &State->StagedCargo.Add_GetRef(NewEntry); }
        Hold -= Units; Entry->Units += Units;
    }
    return true;
}

TArray<FSPFreightContract> USPSocietySimulationComponent::GetFreightContractsForWorld(const FString& WorldId) const
{
    TArray<FSPFreightContract> Result;
    if (!State) return Result;
    const FWorldSeed* Source = WorldCatalog.FindByPredicate([&](const FWorldSeed& World) { return World.Id == WorldId; });
    if (!Source) return Result;
    static const TCHAR* Goods[] = { TEXT("ore"), TEXT("organics"), TEXT("crystal") };
    int32 OfferIndex = 0;
    for (const FWorldSeed& Destination : WorldCatalog)
    {
        if (Destination.Id == WorldId || Destination.System != Source->System) continue;
        FSPFreightContract Offer;
        Offer.Id = WorldId + TEXT(":") + Destination.Id;
        Offer.FromWorldId = WorldId;
        Offer.ToWorldId = Destination.Id;
        Offer.Good = Goods[OfferIndex % UE_ARRAY_COUNT(Goods)];
        Offer.Reward = 240 + OfferIndex * 120;
        if (const FSPFreightContract* Existing = State->FreightContracts.FindByPredicate([&](const FSPFreightContract& Item) { return Item.Id == Offer.Id; })) Offer = *Existing;
        Result.Add(MoveTemp(Offer));
        if (++OfferIndex >= 3) break;
    }
    for (const FSPFreightContract& Contract : State->FreightContracts)
        if (Contract.ToWorldId == WorldId && Contract.Status != TEXT("delivered")) Result.Add(Contract);
    return Result;
}

bool USPSocietySimulationComponent::AcceptFreightContract(const FString& FromCityId, const FString& ContractId)
{
    if (!State) return false;
    const int32 CityIndex = FindSettlementIndex(FromCityId);
    if (CityIndex == INDEX_NONE) return false;
    const FString& WorldId = State->Settlements[CityIndex].WorldId;
    FSPFreightContract Offer;
    bool bFound = false;
    for (const FSPFreightContract& Candidate : GetFreightContractsForWorld(WorldId))
        if (Candidate.Id == ContractId && Candidate.FromWorldId == WorldId && Candidate.Status == TEXT("available")) { Offer = Candidate; bFound = true; break; }
    if (!bFound || State->FreightContracts.ContainsByPredicate([&](const FSPFreightContract& Item) { return Item.Id == ContractId; })) return false;
    const int32 MarketIndex = FindMarketIndex(WorldId);
    if (MarketIndex == INDEX_NONE || Stock(State->Markets[MarketIndex], Offer.Good) < Offer.Units) return false;
    AddStock(State->Markets[MarketIndex], Offer.Good, -Offer.Units);
    Offer.Status = TEXT("awaiting loading");
    State->FreightContracts.Add(Offer);
    FSPCargoEntry* Entry = State->StagedCargo.FindByPredicate([&](const FSPCargoEntry& Item) { return Item.CityId == FromCityId && Item.Good == Offer.Good; });
    if (!Entry) { FSPCargoEntry NewEntry; NewEntry.CityId = FromCityId; NewEntry.Good = Offer.Good; Entry = &State->StagedCargo.Add_GetRef(NewEntry); }
    Entry->Units += Offer.Units;
    return true;
}

bool USPSocietySimulationComponent::DeliverFreightContract(const FString& ToCityId, const FString& ContractId)
{
    if (!State) return false;
    const int32 CityIndex = FindSettlementIndex(ToCityId);
    if (CityIndex == INDEX_NONE) return false;
    FSPFreightContract* Contract = State->FreightContracts.FindByPredicate([&](const FSPFreightContract& Item) { return Item.Id == ContractId; });
    if (!Contract || Contract->Status != TEXT("in transit") || Contract->ToWorldId != State->Settlements[CityIndex].WorldId || GetStagedCargo(ToCityId, Contract->Good) < Contract->Units) return false;
    FSPCargoEntry* Entry = State->StagedCargo.FindByPredicate([&](const FSPCargoEntry& Item) { return Item.CityId == ToCityId && Item.Good == Contract->Good; });
    const int32 MarketIndex = FindMarketIndex(Contract->ToWorldId);
    if (!Entry || MarketIndex == INDEX_NONE || Contract->Reward > MAX_int32 - State->PlayerCredits) return false;
    Entry->Units -= Contract->Units;
    AddStock(State->Markets[MarketIndex], Contract->Good, Contract->Units);
    State->Markets[MarketIndex].Activity++;
    State->PlayerCredits += Contract->Reward;
    Contract->Status = TEXT("delivered");
    AddNews(Contract->ToWorldId, TEXT("freight"), FString::Printf(TEXT("%s delivered %d %s from %s."), *Contract->Id, Contract->Units, *Contract->Good, *Contract->FromWorldId));
    return true;
}

bool USPSocietySimulationComponent::AcceptJob(const FString& JobId)
{
    if (!State) return false;
    FSPSocietyJob* Job = State->Jobs.FindByPredicate([&](const FSPSocietyJob& Item) { return Item.Id == JobId; });
    if (!Job || Job->Status != TEXT("offered") || State->SimulationTime > Job->DeadlineTime) return false;
    Job->Status = TEXT("accepted");
    OnSocietyJobChanged.Broadcast(*Job);
    return true;
}

bool USPSocietySimulationComponent::CompleteJobAtNode(const FString& JobId, const FString& CityId, int32 Node, bool bBusRepaired)
{
    if (!State) return false;
    FSPSocietyJob* Job = State->Jobs.FindByPredicate([&](const FSPSocietyJob& Item) { return Item.Id == JobId; });
    const int32 CityIndex = FindSettlementIndex(CityId);
    if (!Job || CityIndex == INDEX_NONE || Job->Status != TEXT("accepted") || Job->CityId != CityId || State->SimulationTime > Job->DeadlineTime) return false;
    if (Job->Kind == TEXT("courier"))
    {
        if (Job->Stage == 0 && Node == 1) { Job->Stage = 1; OnSocietyJobChanged.Broadcast(*Job); return false; }
        if (Job->Stage != 1 || Node != 3) return false;
    }
    else if (Job->Kind == TEXT("patrol"))
    {
        const int32 Expected = Job->Stage == 0 ? 0 : Job->Stage == 1 ? 3 : 2;
        if (Node != Expected) return false;
        Job->Stage++;
        if (Job->Stage < 3) { OnSocietyJobChanged.Broadcast(*Job); return false; }
    }
    else if (Job->Kind == TEXT("food"))
    {
        if (Node != 1 || GetStagedCargo(CityId, TEXT("organics")) < Job->Units) return false;
        FSPCargoEntry* Entry = State->StagedCargo.FindByPredicate([&](const FSPCargoEntry& Item) { return Item.CityId == CityId && Item.Good == TEXT("organics"); });
        Entry->Units -= Job->Units;
    }
    else if (Job->Kind == TEXT("repair"))
    {
        if (Node != 2 || !bBusRepaired || State->Spares < Job->Units) return false;
        State->Spares -= Job->Units;
    }
    else if (Job->Kind == TEXT("water"))
    {
        if (Node != 2 || State->Alloy < Job->Units) return false;
        State->Alloy -= Job->Units;
    }
    else if (Job->Kind == TEXT("medical"))
    {
        if (Node != 4 || State->Spares < Job->Units) return false;
        State->Spares -= Job->Units;
    }
    else if (Job->Kind == TEXT("survey"))
    {
        if (Node != 4 || State->Samples < 1) return false;
        State->Samples--;
    }
    else return false;

    FSPSocietySettlement& City = State->Settlements[CityIndex];
    if (Job->Kind == TEXT("food")) { City.Food = FMath::Min(120.0f, City.Food + 32.0f); City.Deliveries++; }
    if (Job->Kind == TEXT("repair")) { City.Power = 100.0f; City.Repairs++; }
    if (Job->Kind == TEXT("water")) City.Water = FMath::Min(120.0f, City.Water + 30.0f);
    if (Job->Kind == TEXT("medical")) City.Health = FMath::Min(100.0f, City.Health + 20.0f);
    if (Job->Kind == TEXT("patrol")) City.Security = FMath::Min(100.0f, City.Security + 18.0f);
    Job->Status = TEXT("completed");
    Job->Outcome = TEXT("Completed locally. District services and requester records updated.");
    State->PlayerCredits += Job->Reward;
    if (City.Owner == TEXT("union")) State->UnionStanding += 3;
    else if (City.Owner == TEXT("helix")) State->HelixStanding += 3;
    else State->RedwakeStanding += 3;
    const int32 IssuerIndex = FindResidentIndex(Job->IssuerId);
    if (IssuerIndex != INDEX_NONE) State->Residents[IssuerIndex].Friendship += 2;
    ActiveJobKinds.Remove(JobKey(CityId, Job->Kind));
    if (int32* Count = ActiveJobsByCity.Find(CityId)) *Count = FMath::Max(0, *Count - 1);
    OnSocietyJobChanged.Broadcast(*Job);
    return true;
}

bool USPSocietySimulationComponent::RestartResidentLife(const FString& ResidentId)
{
    if (!State || WorldCatalog.Num() < 2) return false;
    const int32 Index = FindResidentIndex(ResidentId);
    if (Index == INDEX_NONE) return false;
    FSPResidentRecord& Resident = State->Residents[Index];
    const int32 NewWorld = static_cast<int32>((FCrc::StrCrc32(*Resident.Id) + static_cast<uint32>(Resident.Life + 1) * 7919u) % WorldCatalog.Num());
    static const TCHAR* Jobs[] = { TEXT("Freight pilot"), TEXT("Port engineer"), TEXT("Dockworker"), TEXT("Medic"), TEXT("Farmer"), TEXT("Merchant") };
    Resident.Life++;
    Resident.WorldId = WorldCatalog[NewWorld].Id;
    Resident.HomeWorldId = Resident.WorldId;
    Resident.CityId = PrimaryCity(Resident.WorldId);
    Resident.HomeCityId = Resident.CityId;
    Resident.DestinationWorldId.Reset();
    Resident.Job = Jobs[(Resident.Life + NewWorld) % UE_ARRAY_COUNT(Jobs)];
    Resident.ShipId = Resident.Job == TEXT("Freight pilot") ? Resident.Id + TEXT("-life-") + FString::FromInt(Resident.Life) + TEXT("-ship") : TEXT("");
    Resident.ShipIndex = Resident.ShipId.IsEmpty() ? 0 : 20 + NewWorld % 10;
    Resident.Faction = FactionForSeed(NewWorld);
    Resident.Activity = TEXT("Starting over");
    Resident.PartnerId.Reset();
    Resident.Hunger = 15.0f;
    Resident.Fatigue = 10.0f;
    Resident.FromNode = 5;
    Resident.ToNode = 1;
    Resident.DepartTime = State->SimulationTime;
    Resident.UntilTime = State->SimulationTime + 120.0;
    AddNews(Resident.WorldId, TEXT("new life"), Resident.Name + TEXT(" began life ") + FString::FromInt(Resident.Life) + TEXT(" at ") + Resident.CityId + TEXT(" as a ") + Resident.Job + TEXT("."));
    return true;
}

bool USPSocietySimulationComponent::RequestNamedContract(const FString& ResidentId, FString& OutRequest)
{
    OutRequest.Reset();
    const int32 Index = FindResidentIndex(ResidentId);
    if (Index == INDEX_NONE) return false;
    FSPResidentRecord& Resident = State->Residents[Index];
    if (Resident.PersonId == TEXT("vale")) OutRequest = TEXT("Seren Vale: Six alloy for thirty rifle rounds. The Union remembers a counted delivery.");
    else if (Resident.PersonId == TEXT("rook")) OutRequest = TEXT("Ivo Rook: Bring three repair components. I will clear your crew with the Compact.");
    else if (Resident.PersonId == TEXT("mara")) OutRequest = TEXT("Dr. Mara Sol: Bring one field sample. I can assay it into eight alloy and credit the Union research ledger.");
    else return false;
    Resident.bContractRequested = true;
    return true;
}

bool USPSocietySimulationComponent::FulfillNamedContract(const FString& ResidentId, const FString& CurrentCityId)
{
    const int32 Index = FindResidentIndex(ResidentId);
    if (Index == INDEX_NONE) return false;
    FSPResidentRecord& Resident = State->Residents[Index];
    if (!Resident.bContractRequested || Resident.bContractPaid || Resident.CityId != CurrentCityId) return false;
    if (Resident.PersonId == TEXT("vale"))
    {
        if (State->Alloy < 6 || State->RifleReserve > 9969) return false;
        State->Alloy -= 6;
        State->RifleReserve += 30;
        State->UnionStanding = FMath::Clamp(State->UnionStanding + 15, -100, 100);
    }
    else if (Resident.PersonId == TEXT("rook"))
    {
        if (State->Spares < 3) return false;
        State->Spares -= 3;
        State->HelixStanding = FMath::Max(5, State->HelixStanding);
    }
    else if (Resident.PersonId == TEXT("mara"))
    {
        if (State->Samples < 1 || State->Alloy > 9991) return false;
        State->Samples--;
        State->Alloy += 8;
        State->UnionStanding = FMath::Clamp(State->UnionStanding + 15, -100, 100);
    }
    else return false;
    Resident.bContractPaid = true;
    Resident.Friendship += 15;
    AddNews(Resident.WorldId, TEXT("contract"), Resident.Name + TEXT("'s request was fulfilled at ") + CurrentCityId + TEXT("."));
    return true;
}

int32 USPSocietySimulationComponent::FindSettlementIndex(const FString& CityId) const
{
    const int32* Index = CityIndices.Find(CityId);
    return State && Index ? *Index : INDEX_NONE;
}

int32 USPSocietySimulationComponent::FindMarketIndex(const FString& WorldId) const
{
    const int32* Index = MarketIndices.Find(WorldId);
    return State && Index ? *Index : INDEX_NONE;
}

int32 USPSocietySimulationComponent::FindResidentIndex(const FString& ResidentId) const
{
    const int32* Index = ResidentIndices.Find(ResidentId);
    return State && Index ? *Index : INDEX_NONE;
}

bool USPSocietySimulationComponent::ValidGood(const FString& Good)
{
    return Good == TEXT("ore") || Good == TEXT("organics") || Good == TEXT("crystal");
}

float USPSocietySimulationComponent::Stock(const FSPMarketRecord& Market, const FString& Good)
{
    return Good == TEXT("ore") ? Market.Ore : Good == TEXT("crystal") ? Market.Crystal : Market.Organics;
}

void USPSocietySimulationComponent::AddStock(FSPMarketRecord& Market, const FString& Good, float Delta)
{
    if (Good == TEXT("ore")) Market.Ore = FMath::Max(0.0f, Market.Ore + Delta);
    else if (Good == TEXT("crystal")) Market.Crystal = FMath::Max(0.0f, Market.Crystal + Delta);
    else Market.Organics = FMath::Max(0.0f, Market.Organics + Delta);
}

int32& USPSocietySimulationComponent::HoldUnits(const FString& Good)
{
    return Good == TEXT("ore") ? State->HoldOre : Good == TEXT("crystal") ? State->HoldCrystal : State->HoldOrganics;
}

const int32& USPSocietySimulationComponent::HoldUnits(const FString& Good) const
{
    return Good == TEXT("ore") ? State->HoldOre : Good == TEXT("crystal") ? State->HoldCrystal : State->HoldOrganics;
}
