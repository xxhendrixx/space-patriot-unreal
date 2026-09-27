#include "SPWorldSurface.h"

#include "ProceduralMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"

namespace
{
    constexpr float PlanetRadiusMeters = 18000.0f;
    constexpr float WorldSurfaceUnitsPerMeter = 100.0f;
    constexpr float LocalRegionMeters = 3000.0f;

    // This is PlanetEngineSurface.ToSourceNormal with Unity (X,Y-up,Z) mapped
    // to Unreal (X,Y,Z-up). Cross-product handedness changes under that swap.
    const FVector SourceUp = FVector(0.62, 0.69, 0.37).GetSafeNormal();
    const FVector SourceRight = FVector(0.69, -0.62, 0.0).GetSafeNormal();
    const FVector SourceForward = FVector::CrossProduct(SourceRight, SourceUp).GetSafeNormal();

    float Smooth01(float Value)
    {
        const float T = FMath::Clamp(Value, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    float SmoothStep(float Low, float High, float Value)
    {
        return Smooth01((Value - Low) / (High - Low));
    }

    uint32 SourceHash(uint32 X)
    {
        X ^= X >> 16;
        X *= 0x7feb352dU;
        X ^= X >> 15;
        X *= 0x846ca68bU;
        return X ^ (X >> 16);
    }

    float SourceRandom01(uint32& State)
    {
        State += 0x6d2b79f5U;
        uint32 T = (State ^ (State >> 15)) * (State | 1U);
        T ^= T + ((T ^ (T >> 7)) * (T | 61U));
        return static_cast<float>(T ^ (T >> 14)) / 4294967296.0f;
    }

    FVector SourceNormal(const FVector& UnrealRadial)
    {
        return (SourceRight * UnrealRadial.X + SourceUp * UnrealRadial.Z + SourceForward * UnrealRadial.Y).GetSafeNormal();
    }

    const TArray<uint8>& NoiseCube()
    {
        static const TArray<uint8> Data = []()
        {
            TArray<uint8> Result;
            Result.SetNumUninitialized(64 * 64 * 64);
            for (int32 Index = 0; Index < Result.Num(); ++Index)
            {
                Result[Index] = static_cast<uint8>(SourceHash(static_cast<uint32>(Index) ^ 41827U) >> 24);
            }
            return Result;
        }();
        return Data;
    }

    float NoiseAt(int32 X, int32 Y, int32 Z)
    {
        return NoiseCube()[(X & 63) + ((Y & 63) << 6) + ((Z & 63) << 12)] / 255.0f;
    }

    void CalculateSmoothNormals(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, TArray<FVector>& Normals)
    {
        Normals.Init(FVector::ZeroVector, Vertices.Num());
        for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
        {
            const int32 A = Triangles[Index];
            const int32 B = Triangles[Index + 1];
            const int32 C = Triangles[Index + 2];
            const FVector Face = FVector::CrossProduct(Vertices[B] - Vertices[A], Vertices[C] - Vertices[A]);
            Normals[A] += Face;
            Normals[B] += Face;
            Normals[C] += Face;
        }
        for (FVector& Normal : Normals) Normal.Normalize();
    }
}

bool ASPWorldSurface::FAtlas::Load(const FString& Path, bool bSquareTerrain)
{
    Width = Height = 0;
    Values.Reset();
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 16) return false;

    FMemoryReader Reader(Bytes, true);
    Reader << Width;
    if (bSquareTerrain)
    {
        Reader << SizeMeters;
        Height = Width;
    }
    else
    {
        Reader << Height;
        SizeMeters = 0.0f;
    }
    Reader << Seed;
    float Reserved = 0.0f;
    Reader << Reserved;
    if (Width < 2 || Height < 2 || Width > 4096 || Height > 4096 ||
        Bytes.Num() != 16 + static_cast<int64>(Width) * Height * 16 ||
        (bSquareTerrain && SizeMeters <= 0.0f))
    {
        Width = Height = 0;
        return false;
    }

    Values.SetNumUninitialized(Width * Height);
    for (FVector4f& Value : Values)
    {
        Reader << Value.X;
        Reader << Value.Y;
        Reader << Value.Z;
        Reader << Value.W;
    }
    return !Reader.IsError();
}

FVector4f ASPWorldSurface::FAtlas::SampleClamped(float X, float Y) const
{
    if (Values.IsEmpty()) return FVector4f(0.0f, 0.2f, 0.5f, 0.8f);
    const float GX = FMath::Clamp(X, 0.0f, Width - 1.001f);
    const float GY = FMath::Clamp(Y, 0.0f, Height - 1.001f);
    const int32 X0 = FMath::FloorToInt(GX);
    const int32 Y0 = FMath::FloorToInt(GY);
    const float FX = GX - X0;
    const float FY = GY - Y0;
    return FMath::Lerp(
        FMath::Lerp(Values[Y0 * Width + X0], Values[Y0 * Width + X0 + 1], FX),
        FMath::Lerp(Values[(Y0 + 1) * Width + X0], Values[(Y0 + 1) * Width + X0 + 1], FX), FY);
}

FVector4f ASPWorldSurface::FAtlas::SampleClimate(const FVector& Radial) const
{
    if (Values.IsEmpty()) return FVector4f(0.2f, 0.5f, 0.8f, 0.0f);
    const FVector Source = SourceNormal(Radial);
    const float Longitude = FMath::Atan2(Source.Y, Source.X) / (2.0f * PI) + 0.5f;
    const float Latitude = FMath::Asin(FMath::Clamp(Source.Z, -1.0, 1.0)) / PI + 0.5f;
    const float X = FMath::Fmod(Longitude * Width + Width, static_cast<float>(Width));
    const float Y = FMath::Clamp(Latitude * (Height - 1), 0.0f, Height - 1.001f);
    const int32 X0 = FMath::FloorToInt(X);
    const int32 X1 = (X0 + 1) % Width;
    const int32 Y0 = FMath::FloorToInt(Y);
    const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
    return FMath::Lerp(
        FMath::Lerp(Values[Y0 * Width + X0], Values[Y0 * Width + X1], X - X0),
        FMath::Lerp(Values[Y1 * Width + X0], Values[Y1 * Width + X1], X - X0), Y - Y0);
}

ASPWorldSurface::ASPWorldSurface()
{
    PrimaryActorTick.bCanEverTick = true;
    PlanetMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Source Planet"));
    SetRootComponent(PlanetMesh);
    PlanetMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    PlanetMesh->SetCollisionObjectType(ECC_WorldStatic);
    // Keep a coarse floor while the higher-detail patch moves, and have
    // collision ready before a pawn can fall through on the first frame.
    PlanetMesh->bUseAsyncCooking = false;
    DetailMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Streamed Worldworks Detail"));
    DetailMesh->SetupAttachment(PlanetMesh);
    DetailMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    DetailMesh->SetCollisionObjectType(ECC_WorldStatic);
    // Async cooking leaves the previous patch active while its replacement
    // cooks. A moving character can walk off that stale collision body.
    DetailMesh->bUseAsyncCooking = false;
}

void ASPWorldSurface::BeginPlay()
{
    Super::BeginPlay();
    RebuildSurface();
}

void ASPWorldSurface::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    StreamElapsed += DeltaSeconds;
    if (StreamElapsed >= FMath::Max(0.1f, StreamIntervalSeconds))
    {
        StreamElapsed = 0.0f;
        UpdateDetailMesh(false);
    }
}

bool ASPWorldSurface::LoadWorldProfile()
{
    const FString DataRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"));
    FString WorldText;
    if (!FFileHelper::LoadFileToString(WorldText, *FPaths::Combine(DataRoot, TEXT("Worlds.json")))) return false;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(WorldText), Root) || !Root.IsValid()) return false;
    const TArray<TSharedPtr<FJsonValue>>* Worlds = nullptr;
    if (!Root->TryGetArrayField(TEXT("worlds"), Worlds) || !Worlds) return false;

    bool bFound = false;
    for (const TSharedPtr<FJsonValue>& Entry : *Worlds)
    {
        const TSharedPtr<FJsonObject> Object = Entry->AsObject();
        if (!Object.IsValid()) continue;
        FString Candidate;
        Object->TryGetStringField(TEXT("id"), Candidate);
        if (Candidate != WorldId) continue;
        Object->TryGetStringField(TEXT("biome"), ResolvedBiome);
        double SeedNumber = 0.0;
        Object->TryGetNumberField(TEXT("seed"), SeedNumber);
        WorldSeed = static_cast<uint32>(static_cast<uint64>(SeedNumber));
        double RadiusNumber = 1.0;
        Object->TryGetNumberField(TEXT("radius"), RadiusNumber);
        WorldRadiusRatio = static_cast<float>(RadiusNumber);
        bFound = true;
        break;
    }
    if (!bFound) return false;

    const FString FieldRoot = FPaths::Combine(DataRoot, TEXT("Worldworks"));
    if (ResolvedBiome == TEXT("gas")) TerrainField = FAtlas();
    else TerrainField.Load(FPaths::Combine(FieldRoot, WorldId + TEXT(".bytes")), true);
    ClimateField.Load(FPaths::Combine(FieldRoot, TEXT("Climate"), WorldId + TEXT(".bytes")), false);
    if ((TerrainField.Width > 0 && TerrainField.Seed != WorldSeed) ||
        (ClimateField.Width > 0 && ClimateField.Seed != WorldSeed))
    {
        UE_LOG(LogTemp, Error, TEXT("Worldworks seed mismatch for %s"), *WorldId);
        return false;
    }
    SourceTerrainResolution = TerrainField.Width;
    SourceClimateWidth = ClimateField.Width;
    uint32 RandomState = WorldSeed;
    SourceRandom01(RandomState); // Source consumes one draw for body position.
    TerrainAmplitude = ResolvedBiome == TEXT("gas") ? 0.0f : 0.0025f + SourceRandom01(RandomState) * 0.002f;
    TerrainFrequency = 0.75f + SourceRandom01(RandomState) * 0.75f;
    TerrainBase = ResolvedBiome == TEXT("temperate") ? 0.53f : 0.44f;
    NoiseOffset = FVector(WorldSeed % 57U, (WorldSeed >> 8) % 59U, (WorldSeed >> 16) % 61U);
    return ClimateField.Width > 0 && (ResolvedBiome == TEXT("gas") || TerrainField.Width > 0);
}

bool ASPWorldSurface::RebuildSurface()
{
    // Placed map components may retain their old serialized async-cook value
    // even after this class's constructor default changes.
    PlanetMesh->bUseAsyncCooking = false;
    DetailMesh->bUseAsyncCooking = false;
    PlanetMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    PlanetMesh->SetCollisionObjectType(ECC_WorldStatic);
    DetailMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    DetailMesh->SetCollisionObjectType(ECC_WorldStatic);
    if (!LoadWorldProfile())
    {
        UE_LOG(LogTemp, Error, TEXT("Worldworks profile or source field unavailable for %s"), *WorldId);
        PlanetMesh->ClearAllMeshSections();
        DetailMesh->ClearAllMeshSections();
        PlanetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        DetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return false;
    }
    if (!SurfaceMaterial)
    {
        SurfaceMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SpacePatriot/Materials/M_WorldVertex.M_WorldVertex"));
    }
    if (SurfaceMaterial)
    {
        PlanetMesh->SetMaterial(0, SurfaceMaterial);
    }
    // Preserve the inexpensive vertex-color globe while allowing triplanar
    // PBR only on the small, streamed patch nearest the player.
    DetailMesh->SetMaterial(0, DetailMaterial ? DetailMaterial.Get() : SurfaceMaterial.Get());
    BuildPlanetMesh();
    DetailLOD = -1;
    UpdateDetailMesh(true);
    return true;
}

bool ASPWorldSurface::ActivateWorld(const FString& NewWorldId)
{
    if (NewWorldId.IsEmpty()) return false;
    const FString Previous = WorldId;
    WorldId = NewWorldId;
    if (RebuildSurface()) return true;
    WorldId = Previous;
    RebuildSurface();
    return false;
}

FVector ASPWorldSurface::GetPlanetCenterLocal() const
{
    return FVector(0.0, 0.0, -(PlanetRadiusMeters + 3.0f) * WorldSurfaceUnitsPerMeter);
}

FVector ASPWorldSurface::GetRadialAtLocal(const FVector& LocalPoint) const
{
    return (LocalPoint - GetPlanetCenterLocal()).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
}

FVector ASPWorldSurface::GetFocusLocal() const
{
    const AActor* Target = FocusActor.Get();
    if (!Target && GetWorld())
    {
        if (const APlayerController* PC = GetWorld()->GetFirstPlayerController()) Target = PC->GetPawn();
    }
    return Target ? GetActorTransform().InverseTransformPosition(Target->GetActorLocation()) : FVector::ZeroVector;
}

float ASPWorldSurface::PeriodicNoise(const FVector& P)
{
    const int32 IX = FMath::FloorToInt(P.X);
    const int32 IY = FMath::FloorToInt(P.Y);
    const int32 IZ = FMath::FloorToInt(P.Z);
    const float X = Smooth01(static_cast<float>(P.X - IX));
    const float Y = Smooth01(static_cast<float>(P.Y - IY));
    const float Z = Smooth01(static_cast<float>(P.Z - IZ));
    const float A = FMath::Lerp(
        FMath::Lerp(NoiseAt(IX, IY, IZ), NoiseAt(IX + 1, IY, IZ), X),
        FMath::Lerp(NoiseAt(IX, IY + 1, IZ), NoiseAt(IX + 1, IY + 1, IZ), X), Y);
    const float B = FMath::Lerp(
        FMath::Lerp(NoiseAt(IX, IY, IZ + 1), NoiseAt(IX + 1, IY, IZ + 1), X),
        FMath::Lerp(NoiseAt(IX, IY + 1, IZ + 1), NoiseAt(IX + 1, IY + 1, IZ + 1), X), Y);
    return FMath::Lerp(A, B, Z);
}

float ASPWorldSurface::GlobeHeightMeters(const FVector& Radial) const
{
    if (ResolvedBiome == TEXT("gas")) return 0.0f;
    const FVector Source = SourceNormal(Radial);
    const float Q = PeriodicNoise(Source * (12.0f * TerrainFrequency) + NoiseOffset) * 2.0f - 1.0f;
    const float Regional =
        0.58f * PeriodicNoise(Source * (3.5f * TerrainFrequency) + NoiseOffset) +
        0.26f * (1.0f - Q * Q) +
        0.14f * PeriodicNoise(Source * (42.0f * TerrainFrequency) + NoiseOffset) +
        0.02f * PeriodicNoise(Source * (135.0f * TerrainFrequency) + NoiseOffset);
    // Matches PlanetEngineSurface.SourceHeightMeters after cancelling radiusKm.
    return PlanetRadiusMeters * TerrainAmplitude * (Regional - TerrainBase - 0.02f);
}

float ASPWorldSurface::SurfaceHeightMeters(const FVector& Radial) const
{
    const float Globe = GlobeHeightMeters(Radial);
    if (TerrainField.Width == 0 || Radial.Z < 0.985f) return Globe;
    const FVector Nominal = GetPlanetCenterLocal() + Radial * PlanetRadiusMeters * WorldSurfaceUnitsPerMeter;
    const float X = static_cast<float>(Nominal.X / WorldSurfaceUnitsPerMeter);
    const float Y = static_cast<float>(Nominal.Y / WorldSurfaceUnitsPerMeter);
    const float Radius = FMath::Sqrt(X * X + Y * Y);
    if (Radius >= LocalRegionMeters) return Globe;
    const float GridX = (X / TerrainField.SizeMeters + 0.5f) * (TerrainField.Width - 1);
    const float GridY = (Y / TerrainField.SizeMeters + 0.5f) * (TerrainField.Height - 1);
    const float Relief = FMath::Clamp(TerrainField.SampleClamped(GridX, GridY).X, -16.0f, 55.0f);
    const float OuterFade = 1.0f - SmoothStep(2400.0f, 3000.0f, Radius);
    const float FlatX = FMath::Max(0.0f, FMath::Abs(X - 120.0f) - 315.0f);
    const float FlatY = FMath::Max(0.0f, FMath::Abs(Y) - 260.0f);
    const float Basin = SmoothStep(0.0f, 230.0f, FMath::Sqrt(FlatX * FlatX + FlatY * FlatY));
    // The original port protects the landing apron and fades the authored
    // local Worldworks relief into the globe over the final 600 metres.
    return FMath::Lerp(-3.0f, Globe + Relief * 0.62f * OuterFade, Basin);
}

FVector4f ASPWorldSurface::ClimateChannels(const FVector& Radial) const
{
    FVector4f Result = ClimateField.SampleClimate(Radial);
    if (TerrainField.Width > 0 && Radial.Z > 0.985f)
    {
        const FVector Nominal = GetPlanetCenterLocal() + Radial * PlanetRadiusMeters * WorldSurfaceUnitsPerMeter;
        const float X = static_cast<float>(Nominal.X / WorldSurfaceUnitsPerMeter);
        const float Y = static_cast<float>(Nominal.Y / WorldSurfaceUnitsPerMeter);
        const float Radius = FMath::Sqrt(X * X + Y * Y);
        if (Radius < LocalRegionMeters)
        {
            const FVector4f Local = TerrainField.SampleClamped(
                (X / TerrainField.SizeMeters + 0.5f) * (TerrainField.Width - 1),
                (Y / TerrainField.SizeMeters + 0.5f) * (TerrainField.Height - 1));
            const float Blend = 1.0f - SmoothStep(2400.0f, 3000.0f, Radius);
            Result.X = FMath::Lerp(Result.X, Local.Y, Blend);
            Result.Y = FMath::Lerp(Result.Y, Local.Z, Blend);
            Result.Z = FMath::Lerp(Result.Z, Local.W, Blend);
        }
    }
    return Result;
}

FLinearColor ASPWorldSurface::SurfaceColor(const FVector& Radial, float HeightMeters) const
{
    const FVector4f C = ClimateChannels(Radial);
    FLinearColor Base(0.36f, 0.35f, 0.32f, 1.0f);
    if (ResolvedBiome == TEXT("temperate"))
    {
        Base = FLinearColor(0.28f, 0.30f, 0.23f);
        Base = FMath::Lerp(Base, FLinearColor(0.18f, 0.28f, 0.19f), FMath::Clamp(C.X * (0.40f + C.W * 0.45f), 0.0f, 0.85f));
    }
    else if (ResolvedBiome == TEXT("desert")) Base = FMath::Lerp(FLinearColor(0.40f, 0.30f, 0.23f), FLinearColor(0.51f, 0.37f, 0.23f), C.Y);
    else if (ResolvedBiome == TEXT("ice")) Base = FMath::Lerp(FLinearColor(0.46f, 0.54f, 0.56f), FLinearColor(0.68f, 0.72f, 0.70f), 1.0f - C.Y);
    else if (ResolvedBiome == TEXT("volcanic")) Base = FMath::Lerp(FLinearColor(0.19f, 0.17f, 0.16f), FLinearColor(0.38f, 0.23f, 0.18f), C.Y * 0.6f);
    else if (ResolvedBiome == TEXT("gas")) Base = FMath::Lerp(FLinearColor(0.50f, 0.43f, 0.39f), FLinearColor(0.68f, 0.58f, 0.45f), C.Y);
    if (ResolvedBiome != TEXT("gas"))
    {
        Base = FMath::Lerp(Base, FLinearColor(0.34f, 0.34f, 0.33f), FMath::Clamp(C.Z * 0.52f, 0.0f, 0.55f));
        if (HeightMeters < -4.0f && ResolvedBiome == TEXT("temperate"))
            Base = FMath::Lerp(Base, FLinearColor(0.13f, 0.23f, 0.27f), 0.75f);
    }
    Base.A = 1.0f;
    return Base;
}

void ASPWorldSurface::BuildPlanetMesh()
{
    constexpr int32 Latitudes = 64;
    constexpr int32 Longitudes = 128;
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Colors;
    const int32 Side = Longitudes + 1;
    Vertices.Reserve((Latitudes + 1) * Side);
    Triangles.Reserve(Latitudes * Longitudes * 6);
    for (int32 Row = 0; Row <= Latitudes; ++Row)
    {
        const float Theta = PI * Row / Latitudes;
        for (int32 Column = 0; Column <= Longitudes; ++Column)
        {
            const float Phi = 2.0f * PI * Column / Longitudes;
            const FVector Radial(FMath::Sin(Theta) * FMath::Cos(Phi), FMath::Sin(Theta) * FMath::Sin(Phi), FMath::Cos(Theta));
            const float Height = SurfaceHeightMeters(Radial);
            Vertices.Add(GetPlanetCenterLocal() + Radial * (PlanetRadiusMeters + Height) * WorldSurfaceUnitsPerMeter);
            UV.Add(FVector2D(static_cast<float>(Column) / Longitudes, static_cast<float>(Row) / Latitudes));
            Colors.Add(SurfaceColor(Radial, Height));
        }
    }
    for (int32 Row = 0; Row < Latitudes; ++Row)
    {
        for (int32 Column = 0; Column < Longitudes; ++Column)
        {
            const int32 A = Row * Side + Column;
            const int32 B = (Row + 1) * Side + Column;
            const int32 C = A + 1;
            const int32 D = B + 1;
            Triangles.Append({A, B, C, C, B, D});
        }
    }
    CalculateSmoothNormals(Vertices, Triangles, Normals);
    const bool bSolid = ResolvedBiome != TEXT("gas");
    PlanetMesh->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    PlanetMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV, Colors, TArray<FProcMeshTangent>(), bSolid);
}

void ASPWorldSurface::UpdateDetailMesh(bool bForce)
{
    if (ResolvedBiome == TEXT("gas"))
    {
        DetailMesh->ClearAllMeshSections();
        DetailMesh->SetVisibility(false);
        ApplyCollisionMode(false);
        bDetailSectionCollidable = false;
        DetailVertices = DetailTriangles = 0;
        DetailLOD = -1;
        return;
    }
    const FVector Focus = GetFocusLocal();
    const FVector Radial = GetRadialAtLocal(Focus);
    LastFocusAltitudeMeters = static_cast<float>((Focus - GetPlanetCenterLocal()).Length() / WorldSurfaceUnitsPerMeter) - PlanetRadiusMeters - SurfaceHeightMeters(Radial);
    if (LastFocusAltitudeMeters > MaxDetailAltitudeMeters)
    {
        DetailMesh->SetVisibility(false);
        ApplyCollisionMode(false);
        return;
    }
    const int32 NewLOD = LastFocusAltitudeMeters < 40.0f ? 0 : (LastFocusAltitudeMeters < 400.0f ? 1 : 2);
    const bool bNeedsDetailCollision = bEnableDetailCollision && LastFocusAltitudeMeters < 400.0f;
    const float RecenterMeters = NewLOD == 0 ? 120.0f : 700.0f;
    if (!bForce && NewLOD == DetailLOD && bNeedsDetailCollision == bDetailSectionCollidable &&
        FVector::Dist(Focus, DetailAnchor) < RecenterMeters * WorldSurfaceUnitsPerMeter)
    {
        ApplyCollisionMode(bNeedsDetailCollision);
        DetailMesh->SetVisibility(true);
        return;
    }
    BuildDetailMesh(Focus, NewLOD == 0 ? 96 : (NewLOD == 1 ? 48 : 24));
    DetailAnchor = Focus;
    DetailLOD = NewLOD;
    DetailMesh->SetVisibility(true);
}

void ASPWorldSurface::ApplyCollisionMode(bool bDetailCollidable)
{
    // The coarse globe and detailed patch can disagree by metres because the
    // globe interpolates 128x64 vertices. If both block pawns, they stand on
    // the coarse invisible collider instead of the visible ground. Keep the
    // globe solid only while the detailed collision patch is unavailable.
    PlanetMesh->SetCollisionEnabled(ResolvedBiome != TEXT("gas") && !bDetailCollidable
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    DetailMesh->SetCollisionEnabled(bDetailCollidable
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ASPWorldSurface::BuildDetailMesh(const FVector& FocusLocal, int32 Segments)
{
    const FVector Radial = GetRadialAtLocal(FocusLocal);
    FVector East = FVector::VectorPlaneProject(FVector::ForwardVector, Radial).GetSafeNormal();
    if (East.IsNearlyZero()) East = FVector::VectorPlaneProject(FVector::RightVector, Radial).GetSafeNormal();
    const FVector North = FVector::CrossProduct(Radial, East).GetSafeNormal();
    const int32 Side = Segments + 1;
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV;
    TArray<FLinearColor> Colors;
    Vertices.Reserve(Side * Side);
    Triangles.Reserve(Segments * Segments * 6);

    for (int32 Y = 0; Y <= Segments; ++Y)
    {
        const float TY = (2.0f * Y / Segments) - 1.0f;
        const float SpreadY = FMath::Sign(TY) * FMath::Pow(FMath::Abs(TY), 2.1f) * PatchHalfSizeMeters * WorldSurfaceUnitsPerMeter;
        for (int32 X = 0; X <= Segments; ++X)
        {
            const float TX = (2.0f * X / Segments) - 1.0f;
            const float SpreadX = FMath::Sign(TX) * FMath::Pow(FMath::Abs(TX), 2.1f) * PatchHalfSizeMeters * WorldSurfaceUnitsPerMeter;
            const FVector PointRadial = (Radial * PlanetRadiusMeters * WorldSurfaceUnitsPerMeter + East * SpreadX + North * SpreadY).GetSafeNormal();
            const float Height = SurfaceHeightMeters(PointRadial);
            Vertices.Add(GetPlanetCenterLocal() + PointRadial * ((PlanetRadiusMeters + Height) * WorldSurfaceUnitsPerMeter + 3.5f));
            UV.Add(FVector2D(static_cast<float>(X) / Segments, static_cast<float>(Y) / Segments));
            Colors.Add(SurfaceColor(PointRadial, Height));
        }
    }
    for (int32 Y = 0; Y < Segments; ++Y)
    {
        for (int32 X = 0; X < Segments; ++X)
        {
            const int32 A = Y * Side + X;
            const int32 B = A + 1;
            const int32 C = A + Side;
            const int32 D = C + 1;
            Triangles.Append({A, B, C, B, D, C});
        }
    }
    CalculateSmoothNormals(Vertices, Triangles, Normals);
    const bool bCollision = bEnableDetailCollision && LastFocusAltitudeMeters < 400.0f;
    DetailMesh->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    DetailMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV, Colors, TArray<FProcMeshTangent>(), bCollision);
    bDetailSectionCollidable = bCollision;
    ApplyCollisionMode(bCollision);
    DetailVertices = Vertices.Num();
    DetailTriangles = Triangles.Num() / 3;
}

FSPWorldSurfaceSample ASPWorldSurface::SampleAtWorldLocation(FVector WorldLocation) const
{
    FSPWorldSurfaceSample Result;
    const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
    const FVector Radial = GetRadialAtLocal(Local);
    Result.ElevationMeters = SurfaceHeightMeters(Radial);
    const FVector4f Climate = ClimateChannels(Radial);
    Result.Moisture = Climate.X;
    Result.Temperature = Climate.Y;
    Result.Rock = Climate.Z;
    Result.Forest = Climate.W;
    Result.Color = SurfaceColor(Radial, Result.ElevationMeters);
    const FVector Nominal = GetPlanetCenterLocal() + Radial * PlanetRadiusMeters * WorldSurfaceUnitsPerMeter;
    Result.bSourceTerrainField = TerrainField.Width > 0 && Radial.Z > 0.985f &&
        FVector2D(Nominal.X, Nominal.Y).Length() < LocalRegionMeters * WorldSurfaceUnitsPerMeter;
    return Result;
}
