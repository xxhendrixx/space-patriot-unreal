#include "SPCombatSimulationComponent.h"

#include "Algo/Sort.h"

namespace
{
constexpr int32 WeaponCount = 5;
constexpr int32 MaxContacts = 256;
constexpr int32 MaxProjectiles = 64;
constexpr int32 MaxEvents = 128;

bool IsWeapon(ESPCombatWeapon Weapon)
{
    return static_cast<uint8>(Weapon) < WeaponCount;
}

bool IsGroundWeapon(ESPCombatWeapon Weapon)
{
    return Weapon == ESPCombatWeapon::Rifle || Weapon == ESPCombatWeapon::Sidearm;
}

bool InRange(float Value, float Minimum, float Maximum)
{
    return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
}

bool ValidVector(const FVector& Value)
{
    return !Value.ContainsNaN();
}
}

USPCombatSimulationComponent::USPCombatSimulationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ResetCombat();
}

FSPCombatWeaponSpec USPCombatSimulationComponent::GetWeaponSpec(ESPCombatWeapon Weapon)
{
    FSPCombatWeaponSpec Spec;
    Spec.Weapon = Weapon;
    switch (Weapon)
    {
    case ESPCombatWeapon::Kinetic: Spec.Name = TEXT("K-28 TWIN AUTOCANNON"); Spec.SpeedMetersPerSecond = 1650.0f; Spec.Damage = 14.0f; Spec.IntervalSeconds = 0.1f; Spec.MagazineCapacity = 120; Spec.ReserveCapacity = 960; Spec.ReloadSeconds = 2.7f; Spec.RangeMeters = 3200.0f; Spec.Heat01 = 0.012f; break;
    case ESPCombatWeapon::Laser: Spec.Name = TEXT("L-9 PULSE ARRAY"); Spec.SpeedMetersPerSecond = 0.0f; Spec.Damage = 19.0f; Spec.IntervalSeconds = 0.2f; Spec.MagazineCapacity = 100; Spec.ReserveCapacity = 0; Spec.ReloadSeconds = 0.0f; Spec.RangeMeters = 2400.0f; Spec.Heat01 = 0.075f; break;
    case ESPCombatWeapon::Missile: Spec.Name = TEXT("M-6 IR SEEKER"); Spec.SpeedMetersPerSecond = 320.0f; Spec.Damage = 105.0f; Spec.IntervalSeconds = 1.0f; Spec.MagazineCapacity = 6; Spec.ReserveCapacity = 12; Spec.ReloadSeconds = 4.5f; Spec.RangeMeters = 5000.0f; Spec.Heat01 = 0.12f; break;
    case ESPCombatWeapon::Rifle: Spec.Name = TEXT("AR-30 SERVICE RIFLE"); Spec.SpeedMetersPerSecond = 820.0f; Spec.Damage = 22.0f; Spec.IntervalSeconds = 0.12f; Spec.MagazineCapacity = 30; Spec.ReserveCapacity = 180; Spec.ReloadSeconds = 1.9f; Spec.RangeMeters = 800.0f; Spec.Heat01 = 0.018f; break;
    case ESPCombatWeapon::Sidearm: Spec.Name = TEXT("P-12 SIDEARM"); Spec.SpeedMetersPerSecond = 550.0f; Spec.Damage = 29.0f; Spec.IntervalSeconds = 0.28f; Spec.MagazineCapacity = 12; Spec.ReserveCapacity = 72; Spec.ReloadSeconds = 1.4f; Spec.RangeMeters = 400.0f; Spec.Heat01 = 0.015f; break;
    default: Spec.Name = TEXT("INVALID"); break;
    }
    return Spec;
}

void USPCombatSimulationComponent::ResetCombat()
{
    Ammo.Reset();
    for (int32 Index = 0; Index < WeaponCount; ++Index)
    {
        const ESPCombatWeapon Weapon = static_cast<ESPCombatWeapon>(Index);
        const FSPCombatWeaponSpec Spec = GetWeaponSpec(Weapon);
        FSPCombatAmmo Entry;
        Entry.Weapon = Weapon;
        Entry.Magazine = Spec.MagazineCapacity;
        Entry.Reserve = Spec.ReserveCapacity;
        Ammo.Add(Entry);
    }
    Contacts.Reset(); Projectiles.Reset(); PendingEvents.Reset();
    ShipWeapon = ESPCombatWeapon::Kinetic; GroundWeapon = ESPCombatWeapon::Rifle;
    ReloadWeapon = ESPCombatWeapon::Kinetic; TargetId.Reset();
    TimeSeconds = CooldownSeconds = ReloadSeconds = Heat01 = Lock01 = 0.0f;
    Capacitor = Shield = Hull = Suit = 100.0f;
    LastDamageSeconds = -100.0f;
    Kills = Serial = 0;
    bArmed = bOverheated = bDisabled = bSectorCleared = false;
}

ESPCombatWeapon USPCombatSimulationComponent::ActiveWeapon(const FSPCombatStepInput& Input) const
{
    return Input.bTurretSeat ? ESPCombatWeapon::Laser : Input.bOnFoot ? GroundWeapon : ShipWeapon;
}

FSPCombatAmmo* USPCombatSimulationComponent::FindAmmo(ESPCombatWeapon Weapon)
{
    return Ammo.FindByPredicate([Weapon](const FSPCombatAmmo& Entry) { return Entry.Weapon == Weapon; });
}

const FSPCombatAmmo* USPCombatSimulationComponent::FindAmmo(ESPCombatWeapon Weapon) const
{
    return Ammo.FindByPredicate([Weapon](const FSPCombatAmmo& Entry) { return Entry.Weapon == Weapon; });
}

FSPCombatContact* USPCombatSimulationComponent::FindContact(const FString& Id)
{
    return Contacts.FindByPredicate([&Id](const FSPCombatContact& Contact) { return Contact.Id == Id && Contact.Hull > 0.0f; });
}

const FSPCombatContact* USPCombatSimulationComponent::FindContact(const FString& Id) const
{
    return Contacts.FindByPredicate([&Id](const FSPCombatContact& Contact) { return Contact.Id == Id && Contact.Hull > 0.0f; });
}

bool USPCombatSimulationComponent::SelectWeapon(ESPCombatWeapon Weapon)
{
    if (!IsWeapon(Weapon)) return false;
    if (IsGroundWeapon(Weapon)) GroundWeapon = Weapon;
    else ShipWeapon = Weapon;
    ReloadSeconds = 0.0f;
    Lock01 = 0.0f;
    return true;
}

bool USPCombatSimulationComponent::SetArmed(bool bActive, const FSPCombatStepInput& Input)
{
    if (!bActive || bDisabled) { bArmed = false; return false; }
    const bool bPersonal = Input.bOnFoot && !Input.bTurretSeat;
    if (!bPersonal && (!Input.bSCMMode || !Input.bShipPowerOn))
    {
        bArmed = false;
        return false;
    }
    bArmed = true;
    return true;
}

void USPCombatSimulationComponent::SetTargetId(const FString& NewTargetId)
{
    if (TargetId != NewTargetId)
    {
        TargetId = NewTargetId;
        Lock01 = 0.0f;
    }
}

FString USPCombatSimulationComponent::CycleTarget(const FSPCombatStepInput& Input)
{
    TArray<const FSPCombatContact*> Live;
    for (const FSPCombatContact& Contact : Contacts)
        if (Contact.Hull > 0.0f && Contact.bHostile) Live.Add(&Contact);
    Live.Sort([&Input](const FSPCombatContact& A, const FSPCombatContact& B)
    {
        return FVector::DistSquared(A.PositionMeters, Input.PlayerPositionMeters) < FVector::DistSquared(B.PositionMeters, Input.PlayerPositionMeters);
    });
    if (Live.IsEmpty()) { SetTargetId(TEXT("")); return TargetId; }
    int32 CurrentIndex = INDEX_NONE;
    for (int32 Index = 0; Index < Live.Num(); ++Index)
        if (Live[Index]->Id == TargetId) { CurrentIndex = Index; break; }
    SetTargetId(Live[(CurrentIndex + 1) % Live.Num()]->Id);
    return TargetId;
}

void USPCombatSimulationComponent::SetContacts(const TArray<FSPCombatContact>& NewContacts)
{
    Contacts.Reset();
    for (const FSPCombatContact& Contact : NewContacts) AddContact(Contact);
    bSectorCleared = false;
    if (!FindContact(TargetId)) SetTargetId(TEXT(""));
}

bool USPCombatSimulationComponent::AddContact(const FSPCombatContact& Contact)
{
    if (Contacts.Num() >= MaxContacts || Contact.Id.IsEmpty() || Contacts.ContainsByPredicate([&Contact](const FSPCombatContact& Existing) { return Existing.Id == Contact.Id; }) ||
        !ValidVector(Contact.PositionMeters) || !ValidVector(Contact.VelocityMetersPerSecond) || !InRange(Contact.Hull, 0.0f, 100000.0f) || !InRange(Contact.Shield, 0.0f, 100000.0f) || !InRange(Contact.RadiusMeters, 0.01f, 100000.0f)) return false;
    FSPCombatContact Copy = Contact;
    if (Copy.AnchorMeters.IsNearlyZero() && !Copy.PositionMeters.IsNearlyZero()) Copy.AnchorMeters = Copy.PositionMeters;
    Contacts.Add(MoveTemp(Copy));
    bSectorCleared = false;
    return true;
}

bool USPCombatSimulationComponent::StartSortie(bool bGround, const FSPCombatStepInput& Input)
{
    if (!Input.bAuthority || !ValidVector(Input.PlayerPositionMeters)) return false;
    Contacts.Reset(); Projectiles.Reset(); PendingEvents.Reset();
    const FVector Aim = Input.AimDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
    const FVector Right = Input.PlayerRight.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
    const FVector Up = Input.PlayerUp.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    for (int32 Index = 0; Index < (bGround ? 4 : 5); ++Index)
    {
        const float Distance = bGround ? 26.0f + Index * 12.0f : 380.0f + Index * 170.0f;
        const float Side = (Index == 0 ? 0.0f : (Index % 2 ? 1.0f : -1.0f) * (Index / 2 + 1.0f)) * (bGround ? 8.0f : 110.0f);
        FSPCombatContact Contact;
        Contact.Id = FString::Printf(TEXT("drone-%d"), ++Serial);
        Contact.Name = bGround ? FString::Printf(TEXT("SENTRY %02d"), Index + 1) : FString::Printf(TEXT("RAIDER %02d"), Index + 1);
        Contact.PositionMeters = Input.PlayerPositionMeters + Aim * Distance + Right * Side;
        Contact.AnchorMeters = Contact.PositionMeters;
        Contact.Forward = -Aim; Contact.Up = Up;
        Contact.Hull = bGround ? 65.0f : 100.0f;
        Contact.Shield = bGround ? 0.0f : 40.0f;
        Contact.RadiusMeters = bGround ? 1.5f : 14.0f;
        Contact.Phase = Index * 1.7f;
        Contact.FireInSeconds = 4.0f + Index * 0.8f;
        Contact.bAIShip = !bGround; Contact.bAISentry = bGround;
        Contacts.Add(Contact);
    }
    bDisabled = false; bOverheated = false; bArmed = true; bSectorCleared = false;
    Heat01 = CooldownSeconds = ReloadSeconds = 0.0f;
    Hull = Shield = Suit = 100.0f;
    TargetId.Reset(); Lock01 = 0.0f;
    CycleTarget(Input);
    return true;
}

bool USPCombatSimulationComponent::RequestReload(const FSPCombatStepInput& Input)
{
    const ESPCombatWeapon Weapon = ActiveWeapon(Input);
    if (Weapon == ESPCombatWeapon::Laser || ReloadSeconds > 0.0f) return false;
    FSPCombatAmmo* Entry = FindAmmo(Weapon);
    const FSPCombatWeaponSpec Spec = GetWeaponSpec(Weapon);
    if (!Entry || Entry->Magazine >= Spec.MagazineCapacity || Entry->Reserve <= 0) return false;
    ReloadWeapon = Weapon;
    ReloadSeconds = Spec.ReloadSeconds;
    return true;
}

void USPCombatSimulationComponent::AddEvent(ESPCombatEventType Type, ESPCombatWeapon Weapon, const FString& SourceId, const FString& HitTargetId, const FVector& Position, float Damage)
{
    FSPCombatEvent Event;
    Event.Type = Type; Event.Weapon = Weapon; Event.SourceId = SourceId; Event.TargetId = HitTargetId;
    Event.PositionMeters = Position; Event.Damage = Damage;
    PendingEvents.Add(MoveTemp(Event));
    if (PendingEvents.Num() > MaxEvents) PendingEvents.RemoveAt(0, PendingEvents.Num() - MaxEvents);
}

bool USPCombatSimulationComponent::SegmentSphereHit(const FVector& Start, const FVector& End, const FVector& Center, float Radius, double& OutT)
{
    const FVector Segment = End - Start;
    const FVector Offset = Start - Center;
    const double C = FVector::DotProduct(Offset, Offset) - static_cast<double>(Radius) * Radius;
    if (C <= 0.0) { OutT = 0.0; return true; }
    const double A = FVector::DotProduct(Segment, Segment);
    if (A < 1e-12) return false;
    const double B = 2.0 * FVector::DotProduct(Offset, Segment);
    const double Discriminant = B * B - 4.0 * A * C;
    if (Discriminant < 0.0) return false;
    const double T = (-B - FMath::Sqrt(Discriminant)) / (2.0 * A);
    if (T < 0.0 || T > 1.0) return false;
    OutT = T;
    return true;
}

void USPCombatSimulationComponent::ApplyContactDamage(FSPCombatContact& Contact, float Amount, ESPCombatWeapon Weapon, const FVector& Position, const FString& OwnerId)
{
    if (Contact.Hull <= 0.0f || Amount <= 0.0f) return;
    const float Absorbed = FMath::Min(Contact.Shield, Amount);
    Contact.Shield -= Absorbed;
    Contact.Hull = FMath::Max(0.0f, Contact.Hull - (Amount - Absorbed));
    AddEvent(ESPCombatEventType::Impact, Weapon, OwnerId, Contact.Id, Position, Amount);
    if (Contact.Hull <= 0.0f)
    {
        Contact.VelocityMetersPerSecond = FVector::ZeroVector;
        Contact.FireInSeconds = TNumericLimits<float>::Max();
        ++Kills;
        AddEvent(ESPCombatEventType::Destroyed, Weapon, OwnerId, Contact.Id, Position);
        if (Contact.Id == TargetId) SetTargetId(TEXT(""));
    }
}

void USPCombatSimulationComponent::SpawnProjectile(ESPCombatWeapon Weapon, const FString& OwnerId, const FString& ShotTargetId, const FVector& Origin, const FVector& Direction, const FVector& InheritedVelocity)
{
    const FSPCombatWeaponSpec Spec = GetWeaponSpec(Weapon);
    FSPCombatProjectile Projectile;
    Projectile.Id = ++Serial; Projectile.Weapon = Weapon; Projectile.OwnerId = OwnerId; Projectile.TargetId = ShotTargetId;
    Projectile.PositionMeters = Projectile.PreviousMeters = Origin;
    Projectile.Direction = Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
    Projectile.VelocityMetersPerSecond = Projectile.Direction * Spec.SpeedMetersPerSecond + InheritedVelocity;
    Projectile.Damage = Spec.Damage;
    Projectile.TimeToLiveSeconds = Spec.SpeedMetersPerSecond > 0.0f ? Spec.RangeMeters / Spec.SpeedMetersPerSecond : 0.12f;
    Projectiles.Add(MoveTemp(Projectile));
    if (Projectiles.Num() > MaxProjectiles) Projectiles.RemoveAt(0, Projectiles.Num() - MaxProjectiles);
}

bool USPCombatSimulationComponent::TryFire(const FSPCombatStepInput& Input)
{
    const ESPCombatWeapon Weapon = ActiveWeapon(Input);
    const FSPCombatWeaponSpec Spec = GetWeaponSpec(Weapon);
    FSPCombatAmmo* Entry = FindAmmo(Weapon);
    const bool bPersonal = Input.bOnFoot && !Input.bTurretSeat;
    if (!bArmed || bDisabled || bOverheated || ReloadSeconds > 0.0f || CooldownSeconds > 0.0f ||
        (!bPersonal && (!Input.bSCMMode || !Input.bShipPowerOn || Input.WeaponFactor < 0.06f || Input.bInstrumentsOpen))) return false;
    if (Weapon == ESPCombatWeapon::Missile && (!FindContact(TargetId) || Lock01 < 1.0f)) return false;
    if (Weapon == ESPCombatWeapon::Laser && Capacitor < 12.0f) return false;
    if (Weapon != ESPCombatWeapon::Laser && (!Entry || Entry->Magazine <= 0)) { RequestReload(Input); return false; }

    if (Weapon == ESPCombatWeapon::Laser) Capacitor -= 12.0f;
    else --Entry->Magazine;
    CooldownSeconds = Spec.IntervalSeconds;
    Heat01 = FMath::Clamp(Heat01 + Spec.Heat01, 0.0f, 1.0f);
    if (Heat01 >= 1.0f) bOverheated = true;
    const FVector Direction = Input.AimDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
    const FVector Origin = Input.PlayerPositionMeters + Direction * (Input.bOnFoot ? 0.04f : FMath::Max(0.0f, Input.MuzzleOffsetMeters));
    AddEvent(ESPCombatEventType::Fired, Weapon, TEXT("local"), TargetId, Origin);
    if (!Input.bAuthority) return true; // Client prediction never owns damage.

    if (Weapon == ESPCombatWeapon::Laser)
    {
        const FVector End = Origin + Direction * Spec.RangeMeters;
        FSPCombatContact* Hit = nullptr;
        double NearestT = 2.0;
        for (FSPCombatContact& Contact : Contacts)
        {
            if (Contact.Hull <= 0.0f) continue;
            double T = 0.0;
            if (SegmentSphereHit(Origin, End, Contact.PositionMeters, Contact.RadiusMeters, T) && T < NearestT)
            {
                Hit = &Contact; NearestT = T;
            }
        }
        if (Hit) ApplyContactDamage(*Hit, Spec.Damage, Weapon, FMath::Lerp(Origin, End, NearestT), TEXT("local"));
    }
    else SpawnProjectile(Weapon, TEXT("local"), TargetId, Origin, Direction, Weapon == ESPCombatWeapon::Missile ? FVector::ZeroVector : Input.PlayerVelocityMetersPerSecond);
    return true;
}

float USPCombatSimulationComponent::HeatSignature(const FSPCombatContact& Contact)
{
    if (Contact.Hull <= 0.0f) return 0.0f;
    return FMath::Clamp(0.22f + Contact.Thrust01 * 0.9f + Contact.VelocityMetersPerSecond.Size() / 1000.0f * 0.8f + (Contact.bAIShip ? 0.2f : 0.1f), 0.0f, 2.0f);
}

void USPCombatSimulationComponent::AdvanceEnemies(float DeltaSeconds, const FSPCombatStepInput& Input)
{
    for (FSPCombatContact& Contact : Contacts)
    {
        if (Contact.Hull <= 0.0f || (!Contact.bAIShip && !Contact.bAISentry)) continue;
        if (Contact.bAIShip)
        {
            const FVector Before = Contact.PositionMeters;
            Contact.PositionMeters = Contact.AnchorMeters + Input.PlayerRight.GetSafeNormal() * FMath::Sin(TimeSeconds * 0.19f + Contact.Phase) * 65.0f +
                Contact.Up.GetSafeNormal() * FMath::Cos(TimeSeconds * 0.23f + Contact.Phase) * 18.0f;
            Contact.VelocityMetersPerSecond = (Contact.PositionMeters - Before) / FMath::Max(DeltaSeconds, 1e-6f);
            Contact.Forward = (Input.PlayerPositionMeters - Contact.PositionMeters).GetSafeNormal();
        }
        Contact.FireInSeconds -= DeltaSeconds;
        const float Distance = FVector::Dist(Input.PlayerPositionMeters, Contact.PositionMeters);
        if (Contact.FireInSeconds <= 0.0f && Distance < (Contact.bAIShip ? 1800.0f : 150.0f) && !bDisabled)
        {
            Contact.FireInSeconds = Contact.bAIShip ? 2.8f : 2.0f;
            const FVector Direction = (Input.PlayerPositionMeters - Contact.PositionMeters).GetSafeNormal();
            FSPCombatProjectile Projectile;
            Projectile.Id = ++Serial; Projectile.OwnerId = TEXT("hostile"); Projectile.TargetId = TEXT("local");
            Projectile.Weapon = ESPCombatWeapon::Kinetic;
            Projectile.PositionMeters = Contact.PositionMeters + Direction * Contact.RadiusMeters * 1.2f;
            Projectile.PreviousMeters = Projectile.PositionMeters;
            Projectile.Direction = Direction;
            Projectile.VelocityMetersPerSecond = Direction * (Contact.bAIShip ? 230.0f : 55.0f);
            Projectile.Damage = Contact.bAIShip ? 7.0f : 5.0f;
            Projectile.TimeToLiveSeconds = 8.0f;
            Projectiles.Add(Projectile);
            AddEvent(ESPCombatEventType::Fired, ESPCombatWeapon::Kinetic, Contact.Id, TEXT("local"), Projectile.PositionMeters);
        }
    }
    if (Projectiles.Num() > MaxProjectiles) Projectiles.RemoveAt(0, Projectiles.Num() - MaxProjectiles);
}

void USPCombatSimulationComponent::AdvanceProjectiles(float DeltaSeconds, const FSPCombatStepInput& Input)
{
    for (FSPCombatProjectile& Projectile : Projectiles)
    {
        Projectile.PreviousMeters = Projectile.PositionMeters;
        Projectile.AgeSeconds += DeltaSeconds;
        if (Input.bAuthority && Projectile.Weapon == ESPCombatWeapon::Missile)
        {
            FSPCombatContact* Best = nullptr;
            float BestScore = 0.0f;
            for (FSPCombatContact& Contact : Contacts)
            {
                if (Contact.Hull <= 0.0f || Contact.bCivilian) continue;
                const FVector Delta = Contact.PositionMeters - Projectile.PositionMeters;
                const float Distance = Delta.Size();
                if (Distance > 5000.0f || Distance < UE_SMALL_NUMBER || FVector::DotProduct(Delta / Distance, Projectile.Direction) < FMath::Cos(PI * 0.23f)) continue;
                const float Signature = HeatSignature(Contact);
                if (Signature <= 0.1f) continue;
                const float Kilometres = Distance / 1000.0f;
                const float Score = Signature * (Contact.Id == Projectile.TargetId ? 1.5f : 1.0f) / (Kilometres * Kilometres + 0.015f);
                if (Score > BestScore) { Best = &Contact; BestScore = Score; }
            }
            if (Best)
            {
                Projectile.TargetId = Best->Id;
                const float Lead = FMath::Clamp(FVector::Dist(Best->PositionMeters, Projectile.PositionMeters) / 320.0f, 0.0f, 3.0f);
                const FVector Desired = (Best->PositionMeters + Best->VelocityMetersPerSecond * Lead - Projectile.PositionMeters).GetSafeNormal();
                const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Projectile.Direction, Desired), -1.0, 1.0));
                if (Angle > 1e-5)
                {
                    const FQuat Rotation = FQuat::FindBetweenNormals(Projectile.Direction, Desired);
                    Projectile.Direction = FQuat::Slerp(FQuat::Identity, Rotation, FMath::Min(1.0, DeltaSeconds * 1.4 / Angle)).RotateVector(Projectile.Direction).GetSafeNormal();
                }
                else Projectile.Direction = Desired;
            }
            Projectile.VelocityMetersPerSecond = Projectile.Direction * 320.0f;
        }
        Projectile.PositionMeters += Projectile.VelocityMetersPerSecond * DeltaSeconds;
        Projectile.TimeToLiveSeconds -= DeltaSeconds;
        if (!Input.bAuthority) continue;

        if (Projectile.OwnerId == TEXT("hostile"))
        {
            double T = 0.0;
            if (!bDisabled && SegmentSphereHit(Projectile.PreviousMeters, Projectile.PositionMeters, Input.PlayerPositionMeters, Input.bOnFoot ? 0.8f : 11.0f, T))
            {
                const FVector Impact = FMath::Lerp(Projectile.PreviousMeters, Projectile.PositionMeters, T);
                ApplyPlayerDamage(Projectile.Damage, Input.bOnFoot);
                AddEvent(ESPCombatEventType::Impact, Projectile.Weapon, TEXT("hostile"), TEXT("local"), Impact, Projectile.Damage);
                Projectile.TimeToLiveSeconds = 0.0f;
            }
            continue;
        }
        FSPCombatContact* Hit = nullptr;
        double NearestT = 2.0;
        for (FSPCombatContact& Contact : Contacts)
        {
            if (Contact.Hull <= 0.0f || Contact.Id == Projectile.OwnerId) continue;
            double T = 0.0;
            if (SegmentSphereHit(Projectile.PreviousMeters, Projectile.PositionMeters, Contact.PositionMeters, Contact.RadiusMeters, T) && T < NearestT)
            {
                NearestT = T; Hit = &Contact;
            }
        }
        if (Hit)
        {
            ApplyContactDamage(*Hit, Projectile.Damage, Projectile.Weapon, FMath::Lerp(Projectile.PreviousMeters, Projectile.PositionMeters, NearestT), Projectile.OwnerId);
            Projectile.TimeToLiveSeconds = 0.0f;
        }
    }
    Projectiles.RemoveAll([](const FSPCombatProjectile& Projectile) { return Projectile.TimeToLiveSeconds <= 0.0f; });
    if (Projectiles.Num() > MaxProjectiles) Projectiles.RemoveAt(0, Projectiles.Num() - MaxProjectiles);
}

void USPCombatSimulationComponent::StepCombat(float DeltaSeconds, const FSPCombatStepInput& Input)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f) return;
    // Run at a fixed 30-60 Hz cadence. Clamping prevents a paused Editor from simulating a giant frame.
    DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f);
    TimeSeconds += DeltaSeconds;
    CooldownSeconds = FMath::Max(0.0f, CooldownSeconds - DeltaSeconds);
    Heat01 = FMath::Max(0.0f, Heat01 - DeltaSeconds * 0.13f * FMath::Max(0.0f, Input.CoolerFactor));
    if (Heat01 < 0.35f) bOverheated = false;
    Capacitor = FMath::Min(100.0f, Capacitor + DeltaSeconds * 17.0f * FMath::Max(0.0f, Input.WeaponFactor));
    if (TimeSeconds - LastDamageSeconds > 6.0f && !bDisabled && Input.bSCMMode)
        Shield = FMath::Min(100.0f, Shield + DeltaSeconds * 5.0f * FMath::Max(0.0f, Input.ShieldFactor));
    if (ReloadSeconds > 0.0f)
    {
        ReloadSeconds -= DeltaSeconds;
        if (ReloadSeconds <= 0.0f)
        {
            ReloadSeconds = 0.0f;
            if (FSPCombatAmmo* Entry = FindAmmo(ReloadWeapon))
            {
                const int32 Count = FMath::Min(GetWeaponSpec(ReloadWeapon).MagazineCapacity - Entry->Magazine, Entry->Reserve);
                Entry->Magazine += Count; Entry->Reserve -= Count;
                AddEvent(ESPCombatEventType::Reloaded, ReloadWeapon, TEXT("local"), TEXT(""), Input.PlayerPositionMeters);
            }
        }
    }
    const FSPCombatContact* Target = FindContact(TargetId);
    if (Target)
    {
        const FVector ToTarget = Target->PositionMeters - Input.PlayerPositionMeters;
        const float Distance = ToTarget.Size();
        const bool bLockable = Distance > UE_SMALL_NUMBER && Distance < 5000.0f &&
            FVector::DotProduct(ToTarget / Distance, Input.AimDirection.GetSafeNormal()) > 0.97f && HeatSignature(*Target) > 0.1f;
        Lock01 = FMath::Clamp(Lock01 + DeltaSeconds * (bLockable ? 0.65f : -1.5f), 0.0f, 1.0f);
    }
    else Lock01 = 0.0f;
    if (Input.bTrigger) TryFire(Input);
    if (Input.bAuthority)
    {
        AdvanceEnemies(DeltaSeconds, Input);
        AdvanceProjectiles(DeltaSeconds, Input);
        if (!bSectorCleared && !Contacts.IsEmpty() && Contacts.ContainsByPredicate([](const FSPCombatContact& Contact) { return Contact.bHostile; }) &&
            !Contacts.ContainsByPredicate([](const FSPCombatContact& Contact) { return Contact.bHostile && Contact.Hull > 0.0f; }))
        {
            bSectorCleared = true;
            AddEvent(ESPCombatEventType::SectorClear, ESPCombatWeapon::Kinetic, TEXT("system"), TEXT(""), Input.PlayerPositionMeters);
        }
    }
    else AdvanceProjectiles(DeltaSeconds, Input);
}

void USPCombatSimulationComponent::ApplyPlayerDamage(float Amount, bool bOnFoot)
{
    if (bDisabled || !FMath::IsFinite(Amount) || Amount <= 0.0f) return;
    LastDamageSeconds = TimeSeconds;
    if (bOnFoot) Suit = FMath::Max(0.0f, Suit - Amount);
    else
    {
        const float Absorbed = FMath::Min(Shield, Amount);
        Shield -= Absorbed;
        Hull = FMath::Max(0.0f, Hull - Amount + Absorbed);
    }
    if (Suit <= 0.0f || Hull <= 0.0f)
    {
        bDisabled = true; bArmed = false;
        AddEvent(ESPCombatEventType::PlayerDisabled, ESPCombatWeapon::Kinetic, TEXT("system"), TEXT("local"), FVector::ZeroVector);
    }
}

bool USPCombatSimulationComponent::Service(bool bLandedOrDocked, bool bWalking)
{
    if (!bLandedOrDocked || bWalking) return false;
    for (FSPCombatAmmo& Entry : Ammo)
    {
        const FSPCombatWeaponSpec Spec = GetWeaponSpec(Entry.Weapon);
        Entry.Magazine = Spec.MagazineCapacity; Entry.Reserve = Spec.ReserveCapacity;
    }
    Shield = Suit = Hull = Capacitor = 100.0f;
    Heat01 = CooldownSeconds = ReloadSeconds = 0.0f;
    bOverheated = bDisabled = false;
    return true;
}

FSPCombatTelemetry USPCombatSimulationComponent::GetTelemetry(const FSPCombatStepInput& Input) const
{
    FSPCombatTelemetry Out;
    Out.ActiveWeapon = ActiveWeapon(Input);
    if (const FSPCombatAmmo* Entry = FindAmmo(Out.ActiveWeapon)) Out.Ammo = *Entry;
    Out.TargetId = TargetId;
    Out.Lock01 = Lock01; Out.Heat01 = Heat01; Out.Capacitor = Capacitor;
    Out.Shield = Shield; Out.Hull = Hull; Out.Suit = Suit;
    Out.ReloadSeconds = ReloadSeconds; Out.bArmed = bArmed; Out.bOverheated = bOverheated;
    Out.bDisabled = bDisabled; Out.Kills = Kills; Out.ActiveProjectiles = Projectiles.Num();
    if (bDisabled) Out.Status = TEXT("FIRE CONTROL DISABLED");
    else if (!bArmed) Out.Status = TEXT("SAFE");
    else if (!(Input.bOnFoot && !Input.bTurretSeat) && !Input.bSCMMode) Out.Status = TEXT("NAV MODE");
    else if (!(Input.bOnFoot && !Input.bTurretSeat) && (!Input.bShipPowerOn || Input.WeaponFactor < 0.06f)) Out.Status = TEXT("WEAPON BUS OFFLINE");
    else if (!(Input.bOnFoot && !Input.bTurretSeat) && Input.bInstrumentsOpen) Out.Status = TEXT("RETURN TO FLIGHT");
    else if (ReloadSeconds > 0.0f) Out.Status = TEXT("RELOADING");
    else if (bOverheated) Out.Status = TEXT("COOLING");
    else if (Out.ActiveWeapon == ESPCombatWeapon::Laser && Capacitor < 12.0f) Out.Status = TEXT("CAPACITOR CHARGING");
    else if (Out.ActiveWeapon != ESPCombatWeapon::Laser && Out.Ammo.Magazine <= 0) Out.Status = TEXT("EMPTY");
    else if (Out.ActiveWeapon == ESPCombatWeapon::Missile && !FindContact(TargetId)) Out.Status = TEXT("SELECT MISSILE TARGET");
    else if (Out.ActiveWeapon == ESPCombatWeapon::Missile && Lock01 < 1.0f) Out.Status = TEXT("SEEKER LOCKING");
    else Out.Status = TEXT("READY");
    return Out;
}

FSPCombatFiringSolution USPCombatSimulationComponent::GetFiringSolution(const FSPCombatStepInput& Input) const
{
    FSPCombatFiringSolution Out;
    const FSPCombatContact* Target = FindContact(TargetId);
    if (!Target) return Out;
    const FSPCombatWeaponSpec Spec = GetWeaponSpec(ActiveWeapon(Input));
    const FVector RelativePosition = Target->PositionMeters - Input.PlayerPositionMeters;
    const FVector RelativeVelocity = Target->VelocityMetersPerSecond - Input.PlayerVelocityMetersPerSecond;
    const double Distance = RelativePosition.Size();
    Out.DistanceMeters = Distance;
    Out.ClosingMetersPerSecond = Distance > UE_SMALL_NUMBER ? -FVector::DotProduct(RelativeVelocity, RelativePosition / Distance) : 0.0f;
    Out.PointMeters = Target->PositionMeters;
    if (Distance <= UE_SMALL_NUMBER || Spec.SpeedMetersPerSecond <= 0.0f)
    {
        Out.TimeSeconds = 0.0f;
        Out.bReachable = Distance <= Spec.RangeMeters;
        return Out;
    }
    const double A = RelativeVelocity.SizeSquared() - FMath::Square(static_cast<double>(Spec.SpeedMetersPerSecond));
    const double B = 2.0 * FVector::DotProduct(RelativePosition, RelativeVelocity);
    const double C = RelativePosition.SizeSquared();
    double Time = -1.0;
    if (FMath::Abs(A) < 1e-10)
    {
        if (B < 0.0) Time = -C / B;
    }
    else
    {
        const double Discriminant = B * B - 4.0 * A * C;
        if (Discriminant >= 0.0)
        {
            const double Root = FMath::Sqrt(Discriminant);
            const double First = (-B - Root) / (2.0 * A);
            const double Second = (-B + Root) / (2.0 * A);
            if (First > 0.0) Time = First;
            if (Second > 0.0 && (Time < 0.0 || Second < Time)) Time = Second;
        }
    }
    if (Time < 0.0) return Out;
    Out.TimeSeconds = Time;
    Out.PointMeters = Target->PositionMeters + RelativeVelocity * Time;
    Out.bReachable = Distance <= Spec.RangeMeters && Time <= Spec.RangeMeters / Spec.SpeedMetersPerSecond;
    return Out;
}

TArray<FSPCombatEvent> USPCombatSimulationComponent::DrainEvents()
{
    TArray<FSPCombatEvent> Result = MoveTemp(PendingEvents);
    PendingEvents.Reset();
    return Result;
}

FSPCombatSnapshot USPCombatSimulationComponent::CaptureState() const
{
    FSPCombatSnapshot Out;
    Out.Ammo = Ammo; Out.Contacts = Contacts; Out.Projectiles = Projectiles;
    Out.ShipWeapon = ShipWeapon; Out.GroundWeapon = GroundWeapon; Out.ReloadWeapon = ReloadWeapon;
    Out.TargetId = TargetId; Out.TimeSeconds = TimeSeconds; Out.CooldownSeconds = CooldownSeconds;
    Out.ReloadSeconds = ReloadSeconds; Out.Heat01 = Heat01; Out.Capacitor = Capacitor;
    Out.Shield = Shield; Out.Hull = Hull; Out.Suit = Suit; Out.LastDamageSeconds = LastDamageSeconds;
    Out.Lock01 = Lock01; Out.Kills = Kills; Out.Serial = Serial;
    Out.bArmed = bArmed; Out.bOverheated = bOverheated; Out.bDisabled = bDisabled; Out.bSectorCleared = bSectorCleared;
    return Out;
}

bool USPCombatSimulationComponent::RestoreState(const FSPCombatSnapshot& State)
{
    if (State.Ammo.Num() != WeaponCount || State.Contacts.Num() > MaxContacts || State.Projectiles.Num() > MaxProjectiles ||
        !IsWeapon(State.ShipWeapon) || IsGroundWeapon(State.ShipWeapon) || !IsGroundWeapon(State.GroundWeapon) || !IsWeapon(State.ReloadWeapon) ||
        !InRange(State.TimeSeconds, 0.0f, 1.0e9f) || !InRange(State.CooldownSeconds, 0.0f, 100.0f) || !InRange(State.ReloadSeconds, 0.0f, 100.0f) ||
        !InRange(State.Heat01, 0.0f, 1.0f) || !InRange(State.Capacitor, 0.0f, 100.0f) || !InRange(State.Shield, 0.0f, 100.0f) ||
        !InRange(State.Hull, 0.0f, 100.0f) || !InRange(State.Suit, 0.0f, 100.0f) || !InRange(State.Lock01, 0.0f, 1.0f) ||
        !FMath::IsFinite(State.LastDamageSeconds) || State.Kills < 0 || State.Serial < 0) return false;
    bool Seen[WeaponCount] = {};
    for (const FSPCombatAmmo& Entry : State.Ammo)
    {
        if (!IsWeapon(Entry.Weapon)) return false;
        const int32 Index = static_cast<int32>(Entry.Weapon);
        const FSPCombatWeaponSpec Spec = GetWeaponSpec(Entry.Weapon);
        if (Seen[Index] || Entry.Magazine < 0 || Entry.Magazine > Spec.MagazineCapacity || Entry.Reserve < 0 || Entry.Reserve > Spec.ReserveCapacity) return false;
        Seen[Index] = true;
    }
    TSet<FString> Ids;
    for (const FSPCombatContact& Contact : State.Contacts)
    {
        if (Contact.Id.IsEmpty() || Ids.Contains(Contact.Id) || !ValidVector(Contact.PositionMeters) || !ValidVector(Contact.AnchorMeters) ||
            !ValidVector(Contact.VelocityMetersPerSecond) || !InRange(Contact.Hull, 0.0f, 100000.0f) ||
            !InRange(Contact.Shield, 0.0f, 100000.0f) || !InRange(Contact.RadiusMeters, 0.01f, 100000.0f)) return false;
        Ids.Add(Contact.Id);
    }
    for (const FSPCombatProjectile& Projectile : State.Projectiles)
        if (!IsWeapon(Projectile.Weapon) || Projectile.Id < 0 || !ValidVector(Projectile.PositionMeters) ||
            !ValidVector(Projectile.VelocityMetersPerSecond) || !InRange(Projectile.Damage, 0.0f, 100000.0f) ||
            !InRange(Projectile.TimeToLiveSeconds, 0.0f, 100000.0f)) return false;

    Ammo = State.Ammo; Contacts = State.Contacts; Projectiles = State.Projectiles;
    ShipWeapon = State.ShipWeapon; GroundWeapon = State.GroundWeapon; ReloadWeapon = State.ReloadWeapon;
    TargetId = State.TargetId; TimeSeconds = State.TimeSeconds; CooldownSeconds = State.CooldownSeconds;
    ReloadSeconds = State.ReloadSeconds; Heat01 = State.Heat01; Capacitor = State.Capacitor;
    Shield = State.Shield; Hull = State.Hull; Suit = State.Suit; LastDamageSeconds = State.LastDamageSeconds;
    Lock01 = State.Lock01; Kills = State.Kills; Serial = State.Serial;
    bArmed = State.bArmed; bOverheated = State.bOverheated; bDisabled = State.bDisabled; bSectorCleared = State.bSectorCleared;
    PendingEvents.Reset();
    return true;
}
