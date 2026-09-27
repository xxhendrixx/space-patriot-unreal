#include "SPArchitecture.h"

#include "Algo/Reverse.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float ArchitectureUnitsPerMeter = 100.0f;
    constexpr float RoomHeight = 2.8f;
    constexpr float BulkheadThickness = 0.16f;
    constexpr int32 MaxRooms = 64;
    constexpr int32 MaxDoors = 128;
    constexpr int32 MaxFixtures = 256;
    constexpr int32 MaxStructureInstances = 2048;

    float ArchitectureNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, float Default = 0.0f)
    {
        double Value = Default;
        return Object.IsValid() && Object->TryGetNumberField(Key, Value) && FMath::IsFinite(Value)
            ? static_cast<float>(Value) : Default;
    }

    FString ArchitectureString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
    {
        FString Value;
        if (Object.IsValid()) Object->TryGetStringField(Key, Value);
        return Value;
    }

    bool Bool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, bool Default = false)
    {
        bool Value = Default;
        if (Object.IsValid()) Object->TryGetBoolField(Key, Value);
        return Value;
    }

    const TArray<TSharedPtr<FJsonValue>>* ArchitectureArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        return Object.IsValid() && Object->TryGetArrayField(Key, Values) ? Values : nullptr;
    }

    bool IsInside(const FSPArchitectureRoom& Room, float X, float Z, float Margin = 0.0f)
    {
        return X >= Room.MinMeters.X + Margin && X <= Room.MaxMeters.X - Margin &&
            Z >= Room.MinMeters.Y + Margin && Z <= Room.MaxMeters.Y - Margin;
    }

    void SortUnique(TArray<float>& Values)
    {
        Values.Sort();
        for (int32 Index = Values.Num() - 1; Index > 0; --Index)
        {
            if (FMath::IsNearlyEqual(Values[Index], Values[Index - 1], 0.001f)) Values.RemoveAt(Index);
        }
    }
}

ASPArchitecture::ASPArchitecture()
{
    PrimaryActorTick.bCanEverTick = true;
    ArchitectureRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArchitectureRoot"));
    RootComponent = ArchitectureRoot;
    Floors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Floors"));
    Ceilings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ceilings"));
    Bulkheads = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Bulkheads"));
    Fixtures = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SolidFixtures"));
    DecorativeFixtures = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DecorativeFixtures"));
    LiftPlatform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LiftPlatform"));
    for (UInstancedStaticMeshComponent* Component : {Floors.Get(), Ceilings.Get(), Bulkheads.Get(), Fixtures.Get(), DecorativeFixtures.Get()})
    {
        Component->SetupAttachment(ArchitectureRoot);
        Component->SetCollisionEnabled(Component == DecorativeFixtures ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
        Component->SetCollisionResponseToAllChannels(Component == DecorativeFixtures ? ECR_Ignore : ECR_Block);
    }
    LiftPlatform->SetupAttachment(ArchitectureRoot);
    LiftPlatform->SetMobility(EComponentMobility::Movable);
    LiftPlatform->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        for (UInstancedStaticMeshComponent* Component : {Floors.Get(), Ceilings.Get(), Bulkheads.Get(), Fixtures.Get(), DecorativeFixtures.Get()}) Component->SetStaticMesh(Cube.Object);
        LiftPlatform->SetStaticMesh(Cube.Object);
    }
    LiftPlatform->SetRelativeScale3D(FVector(2.16f, 2.26f, 0.14f));
    LiftPlatform->SetVisibility(false);
}

void ASPArchitecture::BeginPlay()
{
    Super::BeginPlay();
    if (bLoadOnBeginPlay) RebuildFromSource();
}

void ASPArchitecture::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AdvanceLift(DeltaSeconds);
}

void ASPArchitecture::AdvanceLift(float DeltaSeconds)
{
    if (!bLiftMoving || !bLiftPowered || !Decks.IsValidIndex(LiftTargetDeck)) return;
    const float Target = Decks[LiftTargetDeck].FloorMeters;
    LiftHeightMeters = FMath::FInterpConstantTo(LiftHeightMeters, Target, FMath::Clamp(DeltaSeconds, 0.0f, 60.0f), LiftSpeedMetersPerSecond);
    if (!LiftStopsMeters.IsEmpty())
    {
        LiftPlatform->SetRelativeLocation(FVector(LiftStopsMeters[0].X * ArchitectureUnitsPerMeter,
            -LiftStopsMeters[0].Y * ArchitectureUnitsPerMeter, (LiftHeightMeters - 0.07f) * ArchitectureUnitsPerMeter));
    }
    if (FMath::IsNearlyEqual(LiftHeightMeters, Target, 0.002f))
    {
        LiftHeightMeters = Target;
        LiftDeck = LiftTargetDeck;
        bLiftMoving = false;
        OnLiftArrived.Broadcast(LiftDeck, Decks[LiftDeck].Name);
    }
}

bool ASPArchitecture::RebuildFromSource()
{
    return LoadInteriorFamily(InteriorFamily);
}

bool ASPArchitecture::LoadInteriorFamily(int32 Family)
{
    LastBuildError.Empty();
    FString Json;
    const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Architecture/DeckPlans.json"));
    if (!FFileHelper::LoadFileToString(Json, *SourcePath))
    {
        LastBuildError = FString::Printf(TEXT("Deck catalog missing: %s"), *SourcePath);
        return false;
    }
    if (!ParseSource(Json, Family)) return false;
    InteriorFamily = Family;
    return BuildGeometry();
}

bool ASPArchitecture::ParseSource(const FString& JsonText, int32 Family)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        LastBuildError = TEXT("Invalid DeckPlans JSON");
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Plans = ArchitectureArray(Root, TEXT("plans"));
    if (!Plans || Plans->Num() > 32)
    {
        LastBuildError = TEXT("Deck catalog has no bounded plans array");
        return false;
    }
    TSharedPtr<FJsonObject> Plan;
    for (const TSharedPtr<FJsonValue>& Value : *Plans)
    {
        const TSharedPtr<FJsonObject> Candidate = Value->AsObject();
        if (Candidate.IsValid() && FMath::RoundToInt(ArchitectureNumber(Candidate, TEXT("family"), -1)) == Family) { Plan = Candidate; break; }
    }
    if (!Plan.IsValid())
    {
        LastBuildError = FString::Printf(TEXT("No deck plan for interior family %d"), Family);
        return false;
    }
    const auto* DeckValues = ArchitectureArray(Plan, TEXT("decks"));
    const auto* RoomValues = ArchitectureArray(Plan, TEXT("rooms"));
    const auto* DoorValues = ArchitectureArray(Plan, TEXT("doors"));
    const auto* FixtureValues = ArchitectureArray(Plan, TEXT("fixtures"));
    const auto* LiftValues = ArchitectureArray(Plan, TEXT("lifts"));
    if (!DeckValues || !RoomValues || !DoorValues || !FixtureValues || !LiftValues ||
        DeckValues->IsEmpty() || DeckValues->Num() > 8 || RoomValues->Num() > MaxRooms ||
        DoorValues->Num() > MaxDoors || FixtureValues->Num() > MaxFixtures || LiftValues->Num() > 8)
    {
        LastBuildError = TEXT("Deck plan violates ArchitectureWorks geometry limits");
        return false;
    }

    TArray<FSPArchitectureDeck> ParsedDecks;
    TArray<FSPArchitectureRoom> ParsedRooms;
    TArray<FSPArchitectureDoor> ParsedDoors;
    TArray<FFixture> ParsedFixtures;
    TArray<FVector> ParsedLiftStops;
    TSet<FString> Ids;
    for (const TSharedPtr<FJsonValue>& Value : *DeckValues)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid()) { LastBuildError = TEXT("Invalid deck row"); return false; }
        FSPArchitectureDeck Deck;
        Deck.Index = FMath::RoundToInt(ArchitectureNumber(Object, TEXT("index"), -1));
        Deck.Name = ArchitectureString(Object, TEXT("name"));
        Deck.FloorMeters = ArchitectureNumber(Object, TEXT("y"));
        if (Deck.Index != ParsedDecks.Num() || Deck.Name.IsEmpty()) { LastBuildError = TEXT("Nonsequential or unnamed deck"); return false; }
        ParsedDecks.Add(MoveTemp(Deck));
    }
    for (const TSharedPtr<FJsonValue>& Value : *RoomValues)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid()) { LastBuildError = TEXT("Invalid room row"); return false; }
        FSPArchitectureRoom Room;
        Room.Id = ArchitectureString(Object, TEXT("id")); Room.Name = ArchitectureString(Object, TEXT("name"));
        Room.Deck = FMath::RoundToInt(ArchitectureNumber(Object, TEXT("deck"), -1));
        Room.FloorMeters = ArchitectureNumber(Object, TEXT("y"));
        Room.MinMeters = FVector2D(ArchitectureNumber(Object, TEXT("x0")), ArchitectureNumber(Object, TEXT("z0")));
        Room.MaxMeters = FVector2D(ArchitectureNumber(Object, TEXT("x1")), ArchitectureNumber(Object, TEXT("z1")));
        Room.bCorridor = Room.Id.StartsWith(TEXT("corridor"));
        if (Room.Id.IsEmpty() || Room.Name.IsEmpty() || Ids.Contains(Room.Id) || !ParsedDecks.IsValidIndex(Room.Deck) ||
            Room.MaxMeters.X <= Room.MinMeters.X || Room.MaxMeters.Y <= Room.MinMeters.Y ||
            FMath::Abs(Room.FloorMeters - ParsedDecks[Room.Deck].FloorMeters) > 0.01f)
        {
            LastBuildError = FString::Printf(TEXT("Invalid or duplicate room: %s"), *Room.Id);
            return false;
        }
        Ids.Add(Room.Id);
        ParsedRooms.Add(MoveTemp(Room));
    }
    for (const TSharedPtr<FJsonValue>& Value : *DoorValues)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid()) { LastBuildError = TEXT("Invalid door row"); return false; }
        FSPArchitectureDoor Door;
        Door.Id = ArchitectureString(Object, TEXT("id")); Door.Name = ArchitectureString(Object, TEXT("name"));
        Door.Deck = FMath::RoundToInt(ArchitectureNumber(Object, TEXT("deck"), -1));
        Door.FloorMeters = ArchitectureNumber(Object, TEXT("y"));
        Door.CenterMeters = FVector2D(ArchitectureNumber(Object, TEXT("x")), ArchitectureNumber(Object, TEXT("z")));
        Door.WidthMeters = ArchitectureNumber(Object, TEXT("width"), 1.0f);
        Door.bFixedX = ArchitectureString(Object, TEXT("axis")) == TEXT("x");
        Door.bOpen = true; // The Unity DeckWalk source starts every bulkhead open.
        if (Door.Id.IsEmpty() || Ids.Contains(Door.Id) || !ParsedDecks.IsValidIndex(Door.Deck) ||
            Door.WidthMeters < 0.55f || Door.WidthMeters > 6.0f ||
            FMath::Abs(Door.FloorMeters - ParsedDecks[Door.Deck].FloorMeters) > 0.01f)
        {
            LastBuildError = FString::Printf(TEXT("Invalid or duplicate door: %s"), *Door.Id);
            return false;
        }
        Ids.Add(Door.Id);
        ParsedDoors.Add(MoveTemp(Door));
    }
    for (const TSharedPtr<FJsonValue>& Value : *FixtureValues)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid()) { LastBuildError = TEXT("Invalid fixture row"); return false; }
        FFixture Fixture;
        Fixture.Id = ArchitectureString(Object, TEXT("id")); Fixture.Type = ArchitectureString(Object, TEXT("type"));
        Fixture.Deck = FMath::RoundToInt(ArchitectureNumber(Object, TEXT("deck"), -1));
        Fixture.X = ArchitectureNumber(Object, TEXT("x")); Fixture.Z = ArchitectureNumber(Object, TEXT("z"));
        Fixture.Y = ArchitectureNumber(Object, TEXT("y")); Fixture.W = ArchitectureNumber(Object, TEXT("w"));
        Fixture.D = ArchitectureNumber(Object, TEXT("d")); Fixture.H = ArchitectureNumber(Object, TEXT("h"));
        Fixture.bSolid = Bool(Object, TEXT("solid"));
        if (Fixture.Id.IsEmpty() || Ids.Contains(Fixture.Id) || !ParsedDecks.IsValidIndex(Fixture.Deck) ||
            Fixture.W <= 0 || Fixture.D <= 0 || Fixture.H <= 0)
        {
            LastBuildError = FString::Printf(TEXT("Invalid or duplicate fixture: %s"), *Fixture.Id);
            return false;
        }
        Ids.Add(Fixture.Id);
        ParsedFixtures.Add(MoveTemp(Fixture));
    }
    for (const TSharedPtr<FJsonValue>& Value : *LiftValues)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid()) { LastBuildError = TEXT("Invalid lift stop"); return false; }
        const int32 Deck = FMath::RoundToInt(ArchitectureNumber(Object, TEXT("deck"), -1));
        if (!ParsedDecks.IsValidIndex(Deck)) { LastBuildError = TEXT("Lift stop references missing deck"); return false; }
        ParsedLiftStops.Add(FVector(ArchitectureNumber(Object, TEXT("x")), ArchitectureNumber(Object, TEXT("z")), ArchitectureNumber(Object, TEXT("y"))));
    }
    if (ParsedDecks.Num() > 1 && ParsedLiftStops.Num() != ParsedDecks.Num())
    {
        LastBuildError = TEXT("Multi-deck source has no stop for every deck");
        return false;
    }

    Decks = MoveTemp(ParsedDecks);
    Rooms = MoveTemp(ParsedRooms);
    Doors = MoveTemp(ParsedDoors);
    SourceFixtures = MoveTemp(ParsedFixtures);
    LiftStopsMeters = MoveTemp(ParsedLiftStops);
    FixtureCount = SourceFixtures.Num();
    DoorLookup.Empty();
    for (int32 Index = 0; Index < Doors.Num(); ++Index) DoorLookup.Add(Doors[Index].Id, Index);
    LiftDeck = LiftTargetDeck = 0;
    LiftHeightMeters = Decks[0].FloorMeters;
    bLiftMoving = false;
    return true;
}

void ASPArchitecture::ClearGeometry()
{
    for (UInstancedStaticMeshComponent* Component : {Floors.Get(), Ceilings.Get(), Bulkheads.Get(), Fixtures.Get(), DecorativeFixtures.Get()}) Component->ClearInstances();
    for (UStaticMeshComponent* Mesh : DoorMeshes) if (Mesh) Mesh->DestroyComponent();
    DoorMeshes.Empty();
    StructureInstances = 0;
}

void ASPArchitecture::AddBox(UInstancedStaticMeshComponent* Component, float X, float Y, float Z, float Width, float Height, float Depth)
{
    if (!Component || Width <= 0.001f || Height <= 0.001f || Depth <= 0.001f || StructureInstances >= MaxStructureInstances) return;
    const FVector Location(X * ArchitectureUnitsPerMeter, -Z * ArchitectureUnitsPerMeter, Y * ArchitectureUnitsPerMeter);
    // Engine cube is 100 cm on each axis; source widths are metres.
    Component->AddInstance(FTransform(FQuat::Identity, Location, FVector(Width, Depth, Height)));
    ++StructureInstances;
}

void ASPArchitecture::AddWallWithOpenings(bool bFixedX, float Fixed, float A, float B, int32 Deck, float FloorY)
{
    if (B - A < 0.1f) return;
    TArray<float> Cuts{A, B};
    for (const FSPArchitectureDoor& Door : Doors)
    {
        if (Door.Deck != Deck || Door.bFixedX != bFixedX || FMath::Abs((bFixedX ? Door.CenterMeters.X : Door.CenterMeters.Y) - Fixed) > 0.12f) continue;
        const float Center = bFixedX ? Door.CenterMeters.Y : Door.CenterMeters.X;
        if (Center - Door.WidthMeters * 0.5f < A || Center + Door.WidthMeters * 0.5f > B) continue;
        Cuts.Add(Center - Door.WidthMeters * 0.5f);
        Cuts.Add(Center + Door.WidthMeters * 0.5f);
    }
    SortUnique(Cuts);
    for (int32 Index = 0; Index + 1 < Cuts.Num(); ++Index)
    {
        const float Mid = (Cuts[Index] + Cuts[Index + 1]) * 0.5f;
        bool bDoorGap = false;
        for (const FSPArchitectureDoor& Door : Doors)
        {
            if (Door.Deck != Deck || Door.bFixedX != bFixedX || FMath::Abs((bFixedX ? Door.CenterMeters.X : Door.CenterMeters.Y) - Fixed) > 0.12f) continue;
            const float Center = bFixedX ? Door.CenterMeters.Y : Door.CenterMeters.X;
            if (FMath::Abs(Mid - Center) < Door.WidthMeters * 0.5f - 0.001f) { bDoorGap = true; break; }
        }
        const float Length = Cuts[Index + 1] - Cuts[Index];
        const float LowerHeight = bDoorGap ? 0.0f : RoomHeight;
        const float LintelHeight = bDoorGap ? 0.55f : 0.0f;
        if (LowerHeight > 0)
        {
            if (bFixedX) AddBox(Bulkheads, Fixed, FloorY + LowerHeight * 0.5f, Mid, BulkheadThickness, LowerHeight, Length);
            else AddBox(Bulkheads, Mid, FloorY + LowerHeight * 0.5f, Fixed, Length, LowerHeight, BulkheadThickness);
        }
        if (LintelHeight > 0)
        {
            if (bFixedX) AddBox(Bulkheads, Fixed, FloorY + RoomHeight - LintelHeight * 0.5f, Mid, BulkheadThickness, LintelHeight, Length);
            else AddBox(Bulkheads, Mid, FloorY + RoomHeight - LintelHeight * 0.5f, Fixed, Length, LintelHeight, BulkheadThickness);
        }
    }
}

bool ASPArchitecture::BuildGeometry()
{
    ClearGeometry();
    Floors->SetMaterial(0, FloorMaterial);
    Ceilings->SetMaterial(0, FloorMaterial);
    Bulkheads->SetMaterial(0, BulkheadMaterial);
    Fixtures->SetMaterial(0, FixtureMaterial);
    DecorativeFixtures->SetMaterial(0, FixtureMaterial);
    LiftPlatform->SetMaterial(0, FixtureMaterial);

    // ArchitectureWorks' union-of-rooms principle avoids duplicate coplanar
    // slabs where the source central passage overlaps adjoining rooms.
    for (const FSPArchitectureDeck& Deck : Decks)
    {
        TArray<float> Xs, Zs;
        for (const FSPArchitectureRoom& Room : Rooms) if (Room.Deck == Deck.Index)
        {
            Xs.Add(Room.MinMeters.X); Xs.Add(Room.MaxMeters.X);
            Zs.Add(Room.MinMeters.Y); Zs.Add(Room.MaxMeters.Y);
        }
        if (!LiftStopsMeters.IsEmpty())
        {
            const FVector Stop = LiftStopsMeters[0];
            Xs.Add(Stop.X - 1.08f); Xs.Add(Stop.X + 1.08f);
            Zs.Add(Stop.Y - 1.13f); Zs.Add(Stop.Y + 1.13f);
        }
        SortUnique(Xs); SortUnique(Zs);
        for (int32 Zi = 0; Zi + 1 < Zs.Num(); ++Zi)
        {
            const float MidZ = (Zs[Zi] + Zs[Zi + 1]) * 0.5f;
            float RunStart = 0.0f;
            bool bInRun = false;
            for (int32 Xi = 0; Xi < Xs.Num(); ++Xi)
            {
                bool bOccupied = false;
                if (Xi + 1 < Xs.Num())
                {
                    const float MidX = (Xs[Xi] + Xs[Xi + 1]) * 0.5f;
                    for (const FSPArchitectureRoom& Room : Rooms) if (Room.Deck == Deck.Index && IsInside(Room, MidX, MidZ)) { bOccupied = true; break; }
                    if (bOccupied && !LiftStopsMeters.IsEmpty())
                    {
                        const FVector Stop = LiftStopsMeters[0];
                        if (FMath::Abs(MidX - Stop.X) < 1.08f && FMath::Abs(MidZ - Stop.Y) < 1.13f) bOccupied = false;
                    }
                }
                if (bOccupied && !bInRun) { RunStart = Xs[Xi]; bInRun = true; }
                if (!bOccupied && bInRun)
                {
                    const float Width = Xs[Xi] - RunStart;
                    const float CenterX = (Xs[Xi] + RunStart) * 0.5f;
                    const float CenterZ = (Zs[Zi] + Zs[Zi + 1]) * 0.5f;
                    const float Depth = Zs[Zi + 1] - Zs[Zi];
                    AddBox(Floors, CenterX, Deck.FloorMeters - 0.09f, CenterZ, Width, 0.18f, Depth);
                    AddBox(Ceilings, CenterX, Deck.FloorMeters + RoomHeight + 0.045f, CenterZ, Width, 0.09f, Depth);
                    bInRun = false;
                }
            }
        }

        // The two corridor bulkheads span gaps between room modules; every
        // measured source door cuts its own passage and lintel from this wall.
        for (int32 Side : {-1, 1})
        {
            float MinZ = TNumericLimits<float>::Max(), MaxZ = -TNumericLimits<float>::Max();
            float FixedX = Side < 0 ? -1.22f : 1.22f;
            for (const FSPArchitectureRoom& Room : Rooms) if (Room.Deck == Deck.Index && !Room.bCorridor &&
                (Side < 0 ? Room.MaxMeters.X <= -1.19f : Room.MinMeters.X >= 1.19f))
            {
                MinZ = FMath::Min(MinZ, Room.MinMeters.Y); MaxZ = FMath::Max(MaxZ, Room.MaxMeters.Y);
            }
            if (MaxZ > MinZ) AddWallWithOpenings(true, FixedX, MinZ, MaxZ, Deck.Index, Deck.FloorMeters);
        }

        for (const FSPArchitectureRoom& Room : Rooms) if (Room.Deck == Deck.Index && !Room.bCorridor)
        {
            const bool bSideRoom = Room.MinMeters.X <= -1.19f && Room.MaxMeters.X <= -1.19f ||
                Room.MinMeters.X >= 1.19f && Room.MaxMeters.X >= 1.19f;
            if (bSideRoom)
            {
                const float Outer = Room.MaxMeters.X < 0 ? Room.MinMeters.X : Room.MaxMeters.X;
                AddWallWithOpenings(true, Outer, Room.MinMeters.Y, Room.MaxMeters.Y, Deck.Index, Deck.FloorMeters);
                AddWallWithOpenings(false, Room.MinMeters.Y, Room.MinMeters.X, Room.MaxMeters.X, Deck.Index, Deck.FloorMeters);
                AddWallWithOpenings(false, Room.MaxMeters.Y, Room.MinMeters.X, Room.MaxMeters.X, Deck.Index, Deck.FloorMeters);
            }
            else if (Room.Id == TEXT("bridge"))
            {
                AddWallWithOpenings(true, Room.MinMeters.X, Room.MinMeters.Y, Room.MaxMeters.Y, Deck.Index, Deck.FloorMeters);
                AddWallWithOpenings(true, Room.MaxMeters.X, Room.MinMeters.Y, Room.MaxMeters.Y, Deck.Index, Deck.FloorMeters);
                AddWallWithOpenings(false, Room.MinMeters.Y, Room.MinMeters.X, Room.MaxMeters.X, Deck.Index, Deck.FloorMeters);
            }
        }
        for (const FSPArchitectureRoom& Room : Rooms) if (Room.Deck == Deck.Index && Room.bCorridor)
        {
            AddWallWithOpenings(false, Room.MaxMeters.Y - 2.0f, Room.MinMeters.X, Room.MaxMeters.X, Deck.Index, Deck.FloorMeters);
        }
    }

    for (const FFixture& Fixture : SourceFixtures)
    {
        AddBox(Fixture.bSolid ? Fixtures.Get() : DecorativeFixtures.Get(), Fixture.X,
            Fixture.Y + Fixture.H * 0.5f, Fixture.Z, Fixture.W, Fixture.H, Fixture.D);
    }
    DoorMeshes.Reserve(Doors.Num());
    for (int32 Index = 0; Index < Doors.Num(); ++Index)
    {
        UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Door_%d"), Index));
        Mesh->SetStaticMesh(Bulkheads->GetStaticMesh());
        Mesh->SetMaterial(0, DoorMaterial ? DoorMaterial : BulkheadMaterial);
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetCollisionResponseToAllChannels(ECR_Block);
        Mesh->SetupAttachment(ArchitectureRoot);
        Mesh->RegisterComponent();
        DoorMeshes.Add(Mesh);
        UpdateDoorMesh(Index);
    }
    LiftPlatform->SetVisibility(!LiftStopsMeters.IsEmpty());
    if (!LiftStopsMeters.IsEmpty())
    {
        LiftPlatform->SetRelativeLocation(FVector(LiftStopsMeters[0].X * ArchitectureUnitsPerMeter,
            -LiftStopsMeters[0].Y * ArchitectureUnitsPerMeter, (LiftHeightMeters - 0.07f) * ArchitectureUnitsPerMeter));
    }
    if (StructureInstances >= MaxStructureInstances)
    {
        LastBuildError = TEXT("Architecture instance budget exceeded");
        return false;
    }
    return true;
}

void ASPArchitecture::UpdateDoorMesh(int32 DoorIndex)
{
    if (!Doors.IsValidIndex(DoorIndex) || !DoorMeshes.IsValidIndex(DoorIndex) || !DoorMeshes[DoorIndex]) return;
    const FSPArchitectureDoor& Door = Doors[DoorIndex];
    const float Slide = Door.bOpen ? Door.WidthMeters - 0.10f : 0.0f;
    const float X = Door.CenterMeters.X + (Door.bFixedX ? 0.0f : Slide);
    const float Z = Door.CenterMeters.Y + (Door.bFixedX ? Slide : 0.0f);
    DoorMeshes[DoorIndex]->SetRelativeLocation(FVector(X * ArchitectureUnitsPerMeter, -Z * ArchitectureUnitsPerMeter,
        (Door.FloorMeters + 1.1f) * ArchitectureUnitsPerMeter));
    DoorMeshes[DoorIndex]->SetRelativeScale3D(Door.bFixedX
        ? FVector(0.075f, FMath::Max(0.55f, Door.WidthMeters - 0.12f), 2.2f)
        : FVector(FMath::Max(0.55f, Door.WidthMeters - 0.12f), 0.075f, 2.2f));
}

bool ASPArchitecture::SetDoorOpen(const FString& DoorId, bool bOpen)
{
    const int32* Index = DoorLookup.Find(DoorId);
    if (!Index || !Doors.IsValidIndex(*Index)) return false;
    if (Doors[*Index].bOpen == bOpen) return true;
    Doors[*Index].bOpen = bOpen;
    UpdateDoorMesh(*Index);
    OnDoorChanged.Broadcast(DoorId, bOpen);
    return true;
}

bool ASPArchitecture::IsDoorOpen(const FString& DoorId) const
{
    const int32* Index = DoorLookup.Find(DoorId);
    return Index && Doors.IsValidIndex(*Index) && Doors[*Index].bOpen;
}

bool ASPArchitecture::RequestLiftToDeck(int32 DeckIndex)
{
    if (!bLiftPowered || bLiftMoving || !Decks.IsValidIndex(DeckIndex) || LiftStopsMeters.Num() != Decks.Num()) return false;
    if (DeckIndex == LiftDeck) return true;
    LiftTargetDeck = DeckIndex;
    bLiftMoving = true;
    return true;
}

void ASPArchitecture::SetLiftPower(bool bPowered)
{
    bLiftPowered = bPowered;
}

int32 ASPArchitecture::FindDeckAtHeight(float HeightMeters) const
{
    for (const FSPArchitectureDeck& Deck : Decks)
    {
        if (FMath::Abs(HeightMeters - Deck.FloorMeters) < 0.22f) return Deck.Index;
    }
    return INDEX_NONE;
}

int32 ASPArchitecture::FindRoomIndex(float X, float Z, int32 Deck, bool bPreferNonCorridor) const
{
    int32 Corridor = INDEX_NONE;
    int32 Other = INDEX_NONE;
    for (int32 Index = 0; Index < Rooms.Num(); ++Index)
    {
        const FSPArchitectureRoom& Room = Rooms[Index];
        if (Room.Deck != Deck || !IsInside(Room, X, Z)) continue;
        if (Room.bCorridor)
        {
            if (!bPreferNonCorridor) return Index;
            Corridor = Index;
        }
        else
        {
            if (bPreferNonCorridor) return Index;
            Other = Index;
        }
    }
    return bPreferNonCorridor ? Corridor : Other;
}

FSPArchitectureRoom ASPArchitecture::RoomAtLocalLocation(FVector LocalCentimetres, bool& bFound) const
{
    const int32 Deck = FindDeckAtHeight(LocalCentimetres.Z / ArchitectureUnitsPerMeter);
    const int32 Index = FindRoomIndex(LocalCentimetres.X / ArchitectureUnitsPerMeter,
        -LocalCentimetres.Y / ArchitectureUnitsPerMeter, Deck, true);
    bFound = Rooms.IsValidIndex(Index);
    return bFound ? Rooms[Index] : FSPArchitectureRoom();
}

bool ASPArchitecture::CanOccupyLocalLocation(FVector LocalCentimetres, float RadiusCentimetres) const
{
    const float X = LocalCentimetres.X / ArchitectureUnitsPerMeter;
    const float Z = -LocalCentimetres.Y / ArchitectureUnitsPerMeter;
    const float Y = LocalCentimetres.Z / ArchitectureUnitsPerMeter;
    const float Radius = FMath::Clamp(RadiusCentimetres / ArchitectureUnitsPerMeter, 0.05f, 0.5f);
    const int32 Deck = FindDeckAtHeight(Y);
    if (Deck == INDEX_NONE)
    {
        if (bLiftMoving && !LiftStopsMeters.IsEmpty() && FMath::Abs(X - LiftStopsMeters[0].X) < 0.95f &&
            FMath::Abs(Z - LiftStopsMeters[0].Y) < 1.0f) return true;
        return false;
    }
    for (int32 Sample = 0; Sample < 12; ++Sample)
    {
        const float Angle = Sample * (2.0f * PI / 12.0f);
        if (FindRoomIndex(X + Radius * FMath::Cos(Angle), Z + Radius * FMath::Sin(Angle), Deck, true) == INDEX_NONE) return false;
    }
    for (const FFixture& Fixture : SourceFixtures)
    {
        if (Fixture.Deck == Deck && Fixture.bSolid &&
            FMath::Abs(X - Fixture.X) < Fixture.W * 0.5f + Radius &&
            FMath::Abs(Z - Fixture.Z) < Fixture.D * 0.5f + Radius) return false;
    }
    for (const FSPArchitectureDoor& Door : Doors)
    {
        if (Door.Deck != Deck || Door.bOpen) continue;
        if (Door.bFixedX && FMath::Abs(X - Door.CenterMeters.X) < 0.18f + Radius &&
            FMath::Abs(Z - Door.CenterMeters.Y) < Door.WidthMeters * 0.5f + Radius) return false;
        if (!Door.bFixedX && FMath::Abs(Z - Door.CenterMeters.Y) < 0.18f + Radius &&
            FMath::Abs(X - Door.CenterMeters.X) < Door.WidthMeters * 0.5f + Radius) return false;
    }
    // Side-room bulkheads are solid away from their measured door openings.
    for (const FSPArchitectureRoom& Room : Rooms)
    {
        if (Room.Deck != Deck || Room.bCorridor || Z < Room.MinMeters.Y - Radius || Z > Room.MaxMeters.Y + Radius) continue;
        const bool bPort = FMath::Abs(Room.MaxMeters.X + 1.2f) < 0.08f;
        const bool bStarboard = FMath::Abs(Room.MinMeters.X - 1.2f) < 0.08f;
        if (!bPort && !bStarboard) continue;
        const float Edge = bPort ? Room.MaxMeters.X : Room.MinMeters.X;
        if (FMath::Abs(X - Edge) > 0.08f + Radius) continue;
        bool bInOpening = false;
        for (const FSPArchitectureDoor& Door : Doors)
        {
            if (Door.Deck == Deck && Door.bFixedX && Door.bOpen && FMath::Abs(Door.CenterMeters.X - Edge) < 0.08f &&
                FMath::Abs(Z - Door.CenterMeters.Y) < Door.WidthMeters * 0.5f - Radius) { bInOpening = true; break; }
        }
        if (!bInOpening) return false;
    }
    return true;
}

bool ASPArchitecture::FindRoomRoute(const FString& StartRoomId, const FString& GoalRoomId, TArray<FString>& RoomIds) const
{
    RoomIds.Empty();
    const int32 Start = Rooms.IndexOfByPredicate([&](const FSPArchitectureRoom& Room) { return Room.Id == StartRoomId; });
    const int32 Goal = Rooms.IndexOfByPredicate([&](const FSPArchitectureRoom& Room) { return Room.Id == GoalRoomId; });
    if (Start == INDEX_NONE || Goal == INDEX_NONE) return false;
    TArray<TArray<int32>> Graph;
    Graph.SetNum(Rooms.Num());
    const auto Link = [&](int32 A, int32 B)
    {
        if (A != INDEX_NONE && B != INDEX_NONE && A != B)
        {
            Graph[A].AddUnique(B); Graph[B].AddUnique(A);
        }
    };
    for (int32 A = 0; A < Rooms.Num(); ++A) for (int32 B = A + 1; B < Rooms.Num(); ++B)
    {
        if (Rooms[A].Deck != Rooms[B].Deck) continue;
        const float OverlapX = FMath::Min(Rooms[A].MaxMeters.X, Rooms[B].MaxMeters.X) - FMath::Max(Rooms[A].MinMeters.X, Rooms[B].MinMeters.X);
        const float OverlapZ = FMath::Min(Rooms[A].MaxMeters.Y, Rooms[B].MaxMeters.Y) - FMath::Max(Rooms[A].MinMeters.Y, Rooms[B].MinMeters.Y);
        if (OverlapX > 0.25f && OverlapZ > 0.25f) Link(A, B);
    }
    for (const FSPArchitectureDoor& Door : Doors) if (Door.bOpen)
    {
        const float Epsilon = 0.4f;
        const int32 A = FindRoomIndex(Door.CenterMeters.X - (Door.bFixedX ? Epsilon : 0.0f),
            Door.CenterMeters.Y - (Door.bFixedX ? 0.0f : Epsilon), Door.Deck, true);
        const int32 B = FindRoomIndex(Door.CenterMeters.X + (Door.bFixedX ? Epsilon : 0.0f),
            Door.CenterMeters.Y + (Door.bFixedX ? 0.0f : Epsilon), Door.Deck, true);
        Link(A, B);
    }
    if (bLiftPowered && LiftStopsMeters.Num() == Decks.Num()) for (int32 Deck = 0; Deck + 1 < Decks.Num(); ++Deck)
    {
        const int32 A = FindRoomIndex(LiftStopsMeters[Deck].X, LiftStopsMeters[Deck].Y, Deck, false);
        const int32 B = FindRoomIndex(LiftStopsMeters[Deck + 1].X, LiftStopsMeters[Deck + 1].Y, Deck + 1, false);
        Link(A, B);
    }
    TArray<int32> Previous;
    Previous.Init(INDEX_NONE, Rooms.Num());
    TArray<int32> Queue{Start};
    Previous[Start] = Start;
    for (int32 Head = 0; Head < Queue.Num() && Previous[Goal] == INDEX_NONE; ++Head)
    {
        for (const int32 Next : Graph[Queue[Head]]) if (Previous[Next] == INDEX_NONE)
        {
            Previous[Next] = Queue[Head];
            Queue.Add(Next);
        }
    }
    if (Previous[Goal] == INDEX_NONE) return false;
    for (int32 At = Goal;; At = Previous[At])
    {
        RoomIds.Add(Rooms[At].Id);
        if (At == Start) break;
    }
    Algo::Reverse(RoomIds);
    return true;
}
