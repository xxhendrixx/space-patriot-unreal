#include "SPFieldSurveyComponent.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr double ArrivalRadiusCm = 30000.0; // Source: 0.3 km.
    constexpr int32 MaxPendingPulses = 8;
    constexpr const TCHAR* CompletionJournal = TEXT("Field report archived. Survey data and cargo can be sold at an orbital station.");

    bool ProfileForBiome(const FString& Biome, int32& OutType, FString& OutProfile, FString& OutDescription)
    {
        if (Biome == TEXT("rock"))
        {
            OutType = 0;
            OutProfile = TEXT("Crater highlands");
            OutDescription = TEXT("Map the crater highlands and recover a mineral sample.");
        }
        else if (Biome == TEXT("temperate"))
        {
            OutType = 1;
            OutProfile = TEXT("Living watershed");
            OutDescription = TEXT("Trace the watershed and recover a sample from the living valley.");
        }
        else if (Biome == TEXT("desert"))
        {
            OutType = 2;
            OutProfile = TEXT("Terraced desert");
            OutDescription = TEXT("Survey the terraced dunes and recover a sediment sample.");
        }
        else if (Biome == TEXT("gas"))
        {
            OutType = 3;
            OutProfile = TEXT("Storm atmosphere");
            OutDescription = TEXT("Survey the storm atmosphere from the orbital habitat.");
        }
        else if (Biome == TEXT("ice"))
        {
            OutType = 4;
            OutProfile = TEXT("Glacial ridges");
            OutDescription = TEXT("Record the glacial ridges and recover a crystalline sample.");
        }
        else if (Biome == TEXT("volcanic"))
        {
            OutType = 5;
            OutProfile = TEXT("Volcanic massif");
            OutDescription = TEXT("Survey the volcanic massif and recover a basalt sample.");
        }
        else return false;
        return true;
    }

    FString TitleForPhase(const FString& Phase)
    {
        if (Phase == TEXT("arrival")) return TEXT("Reach the survey site on foot");
        if (Phase == TEXT("scan")) return TEXT("Scan the environment \u00b7 B");
        if (Phase == TEXT("sample")) return TEXT("Collect a field sample \u00b7 Y or Shift+B");
        if (Phase == TEXT("complete")) return TEXT("Field survey complete");
        return FString();
    }
}

USPFieldSurveyComponent::USPFieldSurveyComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USPFieldSurveyComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeSurveys(bAutoLoad);
}

void USPFieldSurveyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bAutoSave && State) SaveSurveys();
    Super::EndPlay(EndPlayReason);
}

bool USPFieldSurveyComponent::LoadDefinitions()
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Worlds.json")))) return false;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
    const TArray<TSharedPtr<FJsonValue>>* Worlds = nullptr;
    if (!Root->TryGetArrayField(TEXT("worlds"), Worlds) || !Worlds) return false;

    TMap<FString, FDefinition> NewDefinitions;
    for (const TSharedPtr<FJsonValue>& Value : *Worlds)
    {
        const TSharedPtr<FJsonObject> World = Value->AsObject();
        if (!World.IsValid()) return false;
        FString Id, Name, Biome;
        if (!World->TryGetStringField(TEXT("id"), Id) ||
            !World->TryGetStringField(TEXT("name"), Name) ||
            !World->TryGetStringField(TEXT("biome"), Biome) ||
            Id.IsEmpty() || Name.IsEmpty() || NewDefinitions.Contains(Id)) return false;
        FDefinition Def;
        Def.Name = Name;
        if (!ProfileForBiome(Biome, Def.Type, Def.Profile, Def.Description)) return false;
        NewDefinitions.Add(Id, MoveTemp(Def));
    }
    if (NewDefinitions.Num() < 19) return false;
    Definitions = MoveTemp(NewDefinitions);
    return true;
}

FString USPFieldSurveyComponent::CanonicalWorldId(const FString& WorldId) const
{
    FString Key = WorldId.TrimStartAndEnd().ToLower();
    if (Key.StartsWith(TEXT("sp-"))) Key.RightChopInline(3);
    if (Definitions.Contains(Key)) return Key;
    Key.ReplaceInline(TEXT(" "), TEXT("-"));
    if (Definitions.Contains(Key)) return Key;
    return FString();
}

FSPFieldSurveyRecord* USPFieldSurveyComponent::FindRecord(const FString& WorldId)
{
    return State ? State->Records.FindByPredicate([&](const FSPFieldSurveyRecord& Item) { return Item.WorldId == WorldId; }) : nullptr;
}

const FSPFieldSurveyRecord* USPFieldSurveyComponent::FindRecord(const FString& WorldId) const
{
    return State ? State->Records.FindByPredicate([&](const FSPFieldSurveyRecord& Item) { return Item.WorldId == WorldId; }) : nullptr;
}

FSPFieldSurveyRecord* USPFieldSurveyComponent::EnsureRecord(const FString& WorldId)
{
    if (!State || !Definitions.Contains(WorldId)) return nullptr;
    if (FSPFieldSurveyRecord* Found = FindRecord(WorldId)) return Found;
    FSPFieldSurveyRecord& Added = State->Records.AddDefaulted_GetRef();
    Added.WorldId = WorldId;
    return &Added;
}

bool USPFieldSurveyComponent::ValidateState(const USPSFieldSurveySaveGame* Candidate) const
{
    if (!Candidate || Candidate->Version != 1 || Candidate->Records.Num() > Definitions.Num()) return false;
    TSet<FString> Seen;
    for (const FSPFieldSurveyRecord& Record : Candidate->Records)
    {
        const FDefinition* Def = Definitions.Find(Record.WorldId);
        if (!Def || Seen.Contains(Record.WorldId)) return false;
        Seen.Add(Record.WorldId);
        const bool bArrival = Record.Phase == TEXT("arrival");
        const bool bScan = Record.Phase == TEXT("scan");
        const bool bSample = Record.Phase == TEXT("sample");
        const bool bEnd = Record.Phase == TEXT("complete");
        if (!(bArrival || bScan || bSample || bEnd) || Record.bComplete != bEnd ||
            (Record.bBeaconEmitted && !Record.bScanned) ||
            ((bArrival || bScan) && Record.bBeaconEmitted) ||
            ((bSample || bEnd) && !Record.bBeaconEmitted) ||
            (bSample && Def->Type == 3)) return false;
        const int32 ExpectedJournalCount = bArrival ? 0 : bEnd ? 2 : 1;
        if (Record.Journal.Num() != ExpectedJournalCount) return false;
        for (int32 Index = 0; Index < Record.Journal.Num(); ++Index)
        {
            if (Record.Journal[Index].WorldId != Record.WorldId || Record.Journal[Index].Sequence != Index + 1) return false;
        }
    }
    return true;
}

bool USPFieldSurveyComponent::InitializeSurveys(bool bLoadExisting)
{
    Definitions.Empty();
    SurveySites.Empty();
    PendingPulses.Empty();
    CurrentWorldId.Empty();
    bHasPlayerContext = false;
    bPlayerWalking = false;
    bPlayerBridgeWalking = false;
    if (!LoadDefinitions()) return false;
    State = NewObject<USPSFieldSurveySaveGame>(this);
    if (bLoadExisting && UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
    {
        USPSFieldSurveySaveGame* Loaded = Cast<USPSFieldSurveySaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
        if (!ValidateState(Loaded)) return false;
        State = Loaded;
    }
    return true;
}

bool USPFieldSurveyComponent::SaveSurveys()
{
    return ValidateState(State) && !SaveSlot.IsEmpty() && UGameplayStatics::SaveGameToSlot(State, SaveSlot, 0);
}

bool USPFieldSurveyComponent::ActivateWorld(const FString& WorldId, FVector SurveySiteWorldLocation)
{
    const FString Key = CanonicalWorldId(WorldId);
    if (Key.IsEmpty() || !State || SurveySiteWorldLocation.ContainsNaN()) return false;
    if (!EnsureRecord(Key)) return false;
    SurveySites.Add(Key, SurveySiteWorldLocation);
    CurrentWorldId = Key;
    bHasPlayerContext = false;
    bPlayerWalking = false;
    bPlayerBridgeWalking = false;
    BroadcastStatus(Key);
    return true;
}

bool USPFieldSurveyComponent::IsNearCurrentSite() const
{
    const FVector* Site = SurveySites.Find(CurrentWorldId);
    return Site && bHasPlayerContext && bPlayerWalking && !bPlayerBridgeWalking &&
        FVector::DistSquared(PlayerLocation, *Site) < ArrivalRadiusCm * ArrivalRadiusCm;
}

void USPFieldSurveyComponent::AddJournal(FSPFieldSurveyRecord& Record, const FString& Text)
{
    FSPFieldSurveyJournalEntry& Entry = Record.Journal.AddDefaulted_GetRef();
    Entry.WorldId = Record.WorldId;
    Entry.Text = Text;
    Entry.Sequence = Record.Journal.Num();
}

void USPFieldSurveyComponent::Complete(FSPFieldSurveyRecord& Record)
{
    Record.Phase = TEXT("complete");
    Record.bComplete = true;
    AddJournal(Record, CompletionJournal);
}

void USPFieldSurveyComponent::ResolveScan(FSPFieldSurveyRecord& Record)
{
    if (Record.Phase != TEXT("scan") || !Record.bScanned || Record.bBeaconEmitted) return;
    const FDefinition* Def = Definitions.Find(Record.WorldId);
    const FVector* Site = SurveySites.Find(Record.WorldId);
    if (!Def || !Site) return;

    const FString WorldId = Record.WorldId;
    Record.bBeaconEmitted = true;
    FSPFieldBeaconPulse Pulse;
    Pulse.WorldId = WorldId;
    Pulse.Position = *Site;
    Pulse.Preset = Def->Type == 4 ? TEXT("frost") : TEXT("arcane");
    PendingPulses.Add(Pulse);
    if (PendingPulses.Num() > MaxPendingPulses) PendingPulses.RemoveAt(0, PendingPulses.Num() - MaxPendingPulses);

    if (Def->Type == 3) Complete(Record);
    else Record.Phase = TEXT("sample");
    OnBeaconPulse.Broadcast(Pulse);
    BroadcastStatus(WorldId);
    if (bAutoSave) SaveSurveys();
}

void USPFieldSurveyComponent::BroadcastStatus(const FString& WorldId)
{
    FSPFieldSurveyStatus Status;
    if (GetStatus(WorldId, Status)) OnStatusChanged.Broadcast(Status);
}

bool USPFieldSurveyComponent::UpdatePlayerContext(FVector PlayerWorldLocation, bool bWalking, bool bBridgeWalk)
{
    if (CurrentWorldId.IsEmpty() || PlayerWorldLocation.ContainsNaN()) return false;
    PlayerLocation = PlayerWorldLocation;
    bHasPlayerContext = true;
    bPlayerWalking = bWalking;
    bPlayerBridgeWalking = bBridgeWalk;
    FSPFieldSurveyRecord* Record = FindRecord(CurrentWorldId);
    if (!Record || Record->Phase != TEXT("arrival") || !IsNearCurrentSite()) return false;
    const FDefinition* Def = Definitions.Find(CurrentWorldId);
    if (!Def) return false;
    AddJournal(*Record, Def->Description);
    Record->Phase = TEXT("scan");
    if (Record->bScanned) ResolveScan(*Record);
    else
    {
        BroadcastStatus(CurrentWorldId);
        if (bAutoSave) SaveSurveys();
    }
    return true;
}

bool USPFieldSurveyComponent::NotifyWorldScanned(const FString& WorldId, bool bScanSucceeded)
{
    if (!bScanSucceeded) return false;
    const FString Key = CanonicalWorldId(WorldId);
    FSPFieldSurveyRecord* Record = EnsureRecord(Key);
    if (!Record || Record->bScanned) return false;
    Record->bScanned = true;
    if (CurrentWorldId == Key && Record->Phase == TEXT("scan")) ResolveScan(*Record);
    else if (bAutoSave) SaveSurveys();
    return true;
}

bool USPFieldSurveyComponent::NotifyFieldSample(const FString& WorldId, const FString& Source, bool bCollectionSucceeded)
{
    if (!bCollectionSucceeded || Source != TEXT("surface-sampler")) return false;
    const FString Key = CanonicalWorldId(WorldId);
    if (Key.IsEmpty() || Key != CurrentWorldId || !IsNearCurrentSite()) return false;
    FSPFieldSurveyRecord* Record = FindRecord(Key);
    if (!Record || Record->Phase != TEXT("sample")) return false;
    Complete(*Record);
    BroadcastStatus(Key);
    if (bAutoSave) SaveSurveys();
    return true;
}

bool USPFieldSurveyComponent::GetStatus(const FString& WorldId, FSPFieldSurveyStatus& OutStatus) const
{
    const FString Key = CanonicalWorldId(WorldId);
    const FDefinition* Def = Definitions.Find(Key);
    const FSPFieldSurveyRecord* Record = FindRecord(Key);
    if (!Def || !Record) return false;
    OutStatus.WorldId = Key;
    OutStatus.Body = Def->Name;
    OutStatus.Profile = Def->Profile;
    OutStatus.Description = Def->Description;
    OutStatus.Phase = Record->Phase;
    OutStatus.Title = TitleForPhase(Record->Phase);
    OutStatus.bComplete = Record->bComplete;
    OutStatus.Journal = Record->Journal;
    const FVector* Site = SurveySites.Find(Key);
    OutStatus.DistanceMeters = Site && Key == CurrentWorldId && bHasPlayerContext ? FVector::Dist(PlayerLocation, *Site) / 100.0 : -1.0;
    return true;
}

bool USPFieldSurveyComponent::GetCurrentStatus(FSPFieldSurveyStatus& OutStatus) const
{
    return !CurrentWorldId.IsEmpty() && GetStatus(CurrentWorldId, OutStatus);
}

TArray<FSPFieldSurveyJournalEntry> USPFieldSurveyComponent::GetJournal(const FString& WorldId) const
{
    const FSPFieldSurveyRecord* Record = FindRecord(CanonicalWorldId(WorldId));
    return Record ? Record->Journal : TArray<FSPFieldSurveyJournalEntry>();
}

TArray<FSPFieldBeaconPulse> USPFieldSurveyComponent::TakePendingBeaconPulses()
{
    TArray<FSPFieldBeaconPulse> Result = MoveTemp(PendingPulses);
    PendingPulses.Reset();
    return Result;
}
