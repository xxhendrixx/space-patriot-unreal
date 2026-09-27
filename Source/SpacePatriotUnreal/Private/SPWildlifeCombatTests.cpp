#include "SpacePatriotBlueprintBases.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

namespace
{
    struct FScopedWildlifeWorld
    {
        UWorld* World = nullptr;

        FScopedWildlifeWorld()
        {
            if (!GEngine) return;
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            World = UWorld::CreateWorld(EWorldType::Game, false,
                MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("WildlifeCombatTest")),
                GetTransientPackage());
            if (!World)
            {
                GEngine->DestroyWorldContext(Context.World());
                return;
            }
            World->AddToRoot();
            Context.SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
        }

        ~FScopedWildlifeWorld()
        {
            if (!World) return;
            GEngine->ShutdownWorldNetDriver(World);
            World->DestroyWorld(true);
            World->SetPhysicsScene(nullptr);
            GEngine->DestroyWorldContext(World);
            World->RemoveFromRoot();
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPWildlifeProjectileDamageTest,
    "SpacePatriot.Wildlife.ShooterProjectileDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSPWildlifeProjectileDamageTest::RunTest(const FString& Parameters)
{
    FScopedWildlifeWorld Fixture;
    UWorld* World = Fixture.World;
    if (!TestNotNull(TEXT("temporary combat world"), World)) return false;

    ASPWildlifeEncounter* Creature = World->SpawnActor<ASPWildlifeEncounter>();
    AActor* Projectile = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("creature actor"), Creature) ||
        !TestNotNull(TEXT("projectile actor"), Projectile)) return false;

    UBoxComponent* CreatureHitbox = NewObject<UBoxComponent>(Creature, TEXT("CreatureHitbox"));
    Creature->SetRootComponent(CreatureHitbox);
    Creature->AddInstanceComponent(CreatureHitbox);
    CreatureHitbox->SetBoxExtent(FVector(100.0f));
    CreatureHitbox->SetCollisionObjectType(ECC_WorldDynamic);
    CreatureHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CreatureHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    CreatureHitbox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    TestTrue(TEXT("wildlife accepts the configured Shooter projectile channel"),
        Creature->ConfigureProjectileHitbox());
    CreatureHitbox->RegisterComponent();
    TestEqual(TEXT("Shooter projectile is blocked"),
        CreatureHitbox->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Block);
    TestEqual(TEXT("proxy still does not block the walking player"),
        CreatureHitbox->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);

    USphereComponent* BulletHitbox = NewObject<USphereComponent>(Projectile, TEXT("BulletHitbox"));
    Projectile->SetRootComponent(BulletHitbox);
    Projectile->AddInstanceComponent(BulletHitbox);
    BulletHitbox->SetSphereRadius(20.0f);
    BulletHitbox->SetCollisionObjectType(ECC_GameTraceChannel1);
    BulletHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BulletHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    BulletHitbox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    BulletHitbox->RegisterComponent();
    Projectile->SetActorLocation(FVector(-300.0f, 0.0f, 0.0f));

    Creature->CreatureId = TEXT("mars-wild-06");
    Creature->MaxHealth = 70.0f;
    Creature->ResetEncounter();
    FHitResult ProjectileHit;
    Projectile->SetActorLocation(FVector(300.0f, 0.0f, 0.0f), true, &ProjectileHit);
    if (!TestTrue(TEXT("Projectile-channel sweep actually reaches wildlife"),
        ProjectileHit.GetActor() == Creature)) return false;

    TestEqual(TEXT("Unreal ApplyDamage reports effective rifle damage"),
        UGameplayStatics::ApplyDamage(Creature, 25.0f, nullptr, Projectile, nullptr), 25.0f);
    TestEqual(TEXT("Shooter hit removes creature health"), Creature->Health, 45.0f);
    TestFalse(TEXT("nonfatal hit keeps encounter active"), Creature->bDefeated);
    TestEqual(TEXT("second hit is capped at remaining health"),
        UGameplayStatics::ApplyDamage(Creature, 100.0f, nullptr, Projectile, nullptr), 45.0f);
    TestTrue(TEXT("fatal rifle hit defeats wildlife"), Creature->bDefeated);
    TestEqual(TEXT("dead wildlife has no health"), Creature->Health, 0.0f);
    TestFalse(TEXT("defeated wildlife no longer blocks shots"), Creature->GetActorEnableCollision());
    TestEqual(TEXT("repeated shots cannot damage a dead creature"),
        UGameplayStatics::ApplyDamage(Creature, 10.0f, nullptr, Projectile, nullptr), 0.0f);
    Creature->ResetEncounter();
    TestEqual(TEXT("reset restores catalog health"), Creature->Health, 70.0f);
    TestTrue(TEXT("reset restores projectile collision"), Creature->GetActorEnableCollision());
    return true;
}

#endif
