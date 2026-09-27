#include "SpacePatriotSystemsComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    TSharedPtr<FJsonObject> ReadJsonFile(const FString& Path)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path)) return nullptr;
        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
    }

    FString FieldString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
    {
        FString Value;
        if (Object.IsValid()) Object->TryGetStringField(Key, Value);
        return Value;
    }

    float FieldNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, float DefaultValue = 0.0f)
    {
        double Value = DefaultValue;
        if (Object.IsValid()) Object->TryGetNumberField(Key, Value);
        return static_cast<float>(Value);
    }
}

USpacePatriotSystemsComponent::USpacePatriotSystemsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USpacePatriotSystemsComponent::BeginPlay()
{
    Super::BeginPlay();
    if (Worlds.IsEmpty()) LoadSourceCatalogs();
    if (!ActiveWorldId.IsEmpty()) SetActiveWorld(ActiveWorldId);
}

bool USpacePatriotSystemsComponent::LoadSourceCatalogs()
{
    const FString DataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"));
    const TSharedPtr<FJsonObject> WorldRoot = ReadJsonFile(FPaths::Combine(DataDir, TEXT("Worlds.json")));
    const TSharedPtr<FJsonObject> CreatureRoot = ReadJsonFile(FPaths::Combine(DataDir, TEXT("CreatureRosters.json")));
    if (!WorldRoot.IsValid() || !CreatureRoot.IsValid()) return false;

    Worlds.Reset();
    const TArray<TSharedPtr<FJsonValue>>* WorldValues = nullptr;
    if (WorldRoot->TryGetArrayField(TEXT("worlds"), WorldValues) && WorldValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *WorldValues)
        {
            const TSharedPtr<FJsonObject> Obj = Value->AsObject();
            if (!Obj.IsValid()) continue;
            FSPWorldRecord Record;
            Record.Id = FieldString(Obj, TEXT("id"));
            Record.Name = FieldString(Obj, TEXT("name"));
            Record.System = FieldString(Obj, TEXT("system"));
            Record.Biome = FieldString(Obj, TEXT("biome"));
            double ExactSeed = 0.0;
            Obj->TryGetNumberField(TEXT("seed"), ExactSeed);
            Record.Seed = static_cast<int32>(static_cast<uint32>(static_cast<int64>(ExactSeed)));
            Record.Temperature = FieldNumber(Obj, TEXT("temperature"));
            Record.Radius = FieldNumber(Obj, TEXT("radius"), 1.0f);
            Worlds.Add(MoveTemp(Record));
        }
    }

    CreatureCatalog.Reset();
    const TArray<TSharedPtr<FJsonValue>>* RosterWorlds = nullptr;
    if (CreatureRoot->TryGetArrayField(TEXT("worlds"), RosterWorlds) && RosterWorlds)
    {
        for (const TSharedPtr<FJsonValue>& WorldValue : *RosterWorlds)
        {
            const TSharedPtr<FJsonObject> WorldObj = WorldValue->AsObject();
            if (!WorldObj.IsValid()) continue;
            const FString WorldId = FieldString(WorldObj, TEXT("world"));
            const TArray<TSharedPtr<FJsonValue>>* Fauna = nullptr;
            if (!WorldObj->TryGetArrayField(TEXT("fauna"), Fauna) || !Fauna) continue;
            for (const TSharedPtr<FJsonValue>& CreatureValue : *Fauna)
            {
                const TSharedPtr<FJsonObject> Obj = CreatureValue->AsObject();
                if (!Obj.IsValid()) continue;
                FSPCreatureRecord Creature;
                Creature.Id = FieldString(Obj, TEXT("id"));
                Creature.WorldId = WorldId;
                Creature.Name = FieldString(Obj, TEXT("name"));
                Creature.Role = FieldString(Obj, TEXT("role"));
                Creature.ModelId = FieldString(Obj, TEXT("model"));
                Creature.Aggression = FieldNumber(Obj, TEXT("aggression"));
                Creature.Damage = FieldNumber(Obj, TEXT("damage"), 8.0f);
                Creature.Health = FieldNumber(Obj, TEXT("health"), 100.0f);
                const TArray<TSharedPtr<FJsonValue>>* Abilities = nullptr;
                if (Obj->TryGetArrayField(TEXT("abilities"), Abilities) && Abilities && Abilities->Num() > 0)
                {
                    // CreatureRosters.json stores ability names as strings. Older
                    // authored records may use {"name": ...}; accept both shapes
                    // without asking JsonValue to cast a string to an object.
                    const TSharedPtr<FJsonValue>& First = (*Abilities)[0];
                    if (First.IsValid() && !First->TryGetString(Creature.Ability))
                    {
                        const TSharedPtr<FJsonObject>* AbilityObject = nullptr;
                        if (First->TryGetObject(AbilityObject) && AbilityObject)
                            Creature.Ability = FieldString(*AbilityObject, TEXT("name"));
                    }
                }
                CreatureCatalog.Add(MoveTemp(Creature));
            }
        }
    }
    return Worlds.Num() > 0 && CreatureCatalog.Num() > 0;
}

bool USpacePatriotSystemsComponent::SetActiveWorld(const FString& WorldId)
{
    if (!Worlds.ContainsByPredicate([&WorldId](const FSPWorldRecord& Item) { return Item.Id == WorldId; })) return false;
    ActiveWorldId = WorldId;
    for (const FSPCreatureRecord& Creature : GetWildlifeForWorld(WorldId))
    {
        if (!RuntimeWildlifeHealth.Contains(Creature.Id)) RuntimeWildlifeHealth.Add(Creature.Id, Creature.Health);
    }
    return true;
}

FSPWorldRecord USpacePatriotSystemsComponent::GetActiveWorld() const
{
    const FSPWorldRecord* Match = Worlds.FindByPredicate([this](const FSPWorldRecord& Item) { return Item.Id == ActiveWorldId; });
    return Match ? *Match : FSPWorldRecord();
}

TArray<FSPCreatureRecord> USpacePatriotSystemsComponent::GetWildlifeForWorld(const FString& WorldId) const
{
    TArray<FSPCreatureRecord> Result;
    for (const FSPCreatureRecord& Creature : CreatureCatalog) if (Creature.WorldId == WorldId) Result.Add(Creature);
    return Result;
}

void USpacePatriotSystemsComponent::AdvanceUniverse(float ElapsedHours)
{
    if (ElapsedHours <= 0.0f) return;
    AccumulatedHours += ElapsedHours;
    const int32 DaysPassed = FMath::FloorToInt(AccumulatedHours / 24.0f);
    if (DaysPassed <= 0) return;
    AccumulatedHours -= static_cast<float>(DaysPassed) * 24.0f;
    for (int32 DayIndex = 0; DayIndex < DaysPassed; ++DayIndex)
    {
        ++SimulationDay;
        // A bounded daily roll keeps off-screen society compact and reproducible.
        for (const FSPWorldRecord& World : Worlds)
        {
            FRandomStream Stream(World.Seed ^ static_cast<int32>(SimulationDay * 7919));
            if (Stream.FRand() > 0.23f) continue;
            static const TCHAR* Kinds[] = { TEXT("trade"), TEXT("rumor"), TEXT("dispute"), TEXT("alliance"), TEXT("romance"), TEXT("betrayal"), TEXT("family-feud"), TEXT("rescue") };
            const int32 KindIndex = Stream.RandRange(0, UE_ARRAY_COUNT(Kinds) - 1);
            FSPUniverseEvent Event;
            Event.EventId = FString::Printf(TEXT("%s-%lld-%d"), *World.Id, SimulationDay, KindIndex);
            Event.WorldId = World.Id;
            Event.Kind = Kinds[KindIndex];
            Event.Day = SimulationDay;
            Event.Seed = Stream.GetCurrentSeed();
            Event.Summary = FString::Printf(TEXT("A %s story beat unfolded off-screen on %s."), *Event.Kind.ToLower(), *World.Name);
            RecentEvents.Add(Event);
            OnUniverseEvent.Broadcast(Event);
        }
    }
    if (RecentEvents.Num() > 128) RecentEvents.RemoveAt(0, RecentEvents.Num() - 128, EAllowShrinking::No);
}

bool USpacePatriotSystemsComponent::ApplyWildlifeDamage(const FString& CreatureId, float Damage)
{
    if (Damage <= 0.0f) return false;
    const FSPCreatureRecord* Definition = CreatureCatalog.FindByPredicate([&CreatureId](const FSPCreatureRecord& Item) { return Item.Id == CreatureId; });
    if (!Definition) return false;
    float& Health = RuntimeWildlifeHealth.FindOrAdd(CreatureId, Definition->Health);
    if (Health <= 0.0f) return false;
    Health = FMath::Max(0.0f, Health - Damage);
    OnWildlifeHealthChanged.Broadcast(CreatureId, Health);
    return true;
}

FSPUniverseEvent USpacePatriotSystemsComponent::RollStoryEvent(const FString& WorldId, const FString& CharacterId, int32 Day)
{
    const FSPWorldRecord* World = Worlds.FindByPredicate([&WorldId](const FSPWorldRecord& Item) { return Item.Id == WorldId; });
    if (!World) return FSPUniverseEvent();
    const int32 CharacterSeed = GetTypeHash(CharacterId);
    FRandomStream Stream(World->Seed ^ CharacterSeed ^ (Day * 104729));
    static const TCHAR* Kinds[] = { TEXT("romance"), TEXT("intrigue"), TEXT("betrayal"), TEXT("alliance"), TEXT("good-business"), TEXT("bad-business"), TEXT("family-feud"), TEXT("rescue") };
    FSPUniverseEvent Event;
    Event.WorldId = WorldId;
    Event.Kind = Kinds[Stream.RandRange(0, UE_ARRAY_COUNT(Kinds) - 1)];
    Event.Day = Day;
    Event.Seed = Stream.GetCurrentSeed();
    Event.EventId = FString::Printf(TEXT("%s-%s-%d"), *WorldId, *CharacterId, Day);
    Event.Summary = FString::Printf(TEXT("%s faces a %s turn."), *CharacterId, *Event.Kind);
    RecentEvents.Add(Event);
    OnUniverseEvent.Broadcast(Event);
    return Event;
}

float USpacePatriotSystemsComponent::CalculateLoadedMass(float HullMass, int32 Organics, int32 Ore, int32 Crystal) const
{
    return HullMass + FMath::Max(0, Organics) * 0.5f + FMath::Max(0, Ore) * 1.5f + FMath::Max(0, Crystal) * 0.8f;
}
