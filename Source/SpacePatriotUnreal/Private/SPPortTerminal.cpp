#include "SPPortTerminal.h"

#include "SpacePatriotBlueprintBases.h"
#include "SPFlightPawn.h"
#include "SPHyperjumpRouteComponent.h"
#include "SPPlayLoopDirector.h"
#include "SPTravelNavigationComponent.h"
#include "SPWorldSurface.h"
#include "SPWorldDressing.h"
#include "SPSocietySimulationComponent.h"
#include "SPStoryCampaignComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Math/RotationMatrix.h"

namespace
{
    const FSPFreightContract* SealedContract(const USPSocietySimulationComponent* Society,
        const FString& Good)
    {
        if (!Society || !Society->State) return nullptr;
        return Society->State->FreightContracts.FindByPredicate([&](const FSPFreightContract& Contract)
        {
            return Contract.Good == Good &&
                (Contract.Status == TEXT("awaiting loading") || Contract.Status == TEXT("in transit"));
        });
    }
}

ASPPortTerminal::ASPPortTerminal()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    TerminalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalMesh"));
    SetRootComponent(TerminalMesh);
    TerminalMesh->SetRelativeScale3D(FVector(1.1, 0.65, 1.55));
    TerminalMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    // This mesh is installed locally from the exact UE 5.8 template checksum
    // in Data/ExternalRuntimeContentManifest.json, and is not stored in Git.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Placeholder(
        TEXT("/Game/LevelPrototyping/Meshes/SM_ChamferCube.SM_ChamferCube"));
    if (Placeholder.Succeeded()) TerminalMesh->SetStaticMesh(Placeholder.Object);

    TerminalLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TerminalLabel"));
    TerminalLabel->SetupAttachment(TerminalMesh);
    TerminalLabel->SetRelativeLocation(FVector(0.0, -40.0, 120.0));
    TerminalLabel->SetRelativeRotation(FRotator(0.0, 0.0, 0.0));
    TerminalLabel->SetHorizontalAlignment(EHTA_Center);
    TerminalLabel->SetWorldSize(19.0f);
    TerminalLabel->SetText(FText::FromString(
        TEXT("F FREIGHT / M CASE / H NEXT\n3 GOOD  4 BUY  5 LOAD  6 UNLOAD  7 SELL")));
    TerminalLabel->SetTextRenderColor(FColor(185, 221, 202));
}

void ASPPortTerminal::BeginPlay()
{
    Super::BeginPlay();
    bRemotePortProxy = bRemotePortProxy || ActorHasTag(TEXT("SP_RemotePortProxy"));
    if (bRemotePortProxy)
    {
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
    }
    if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
    {
        EnableInput(Controller);
        if (InputComponent)
        {
            FInputKeyBinding& Binding = InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ASPPortTerminal::OnInteractPressed);
            Binding.bConsumeInput = false;
            InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ASPPortTerminal::OnMissionPressed).bConsumeInput = false;
            InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ASPPortTerminal::OnCycleMissionPressed).bConsumeInput = false;
            InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ASPPortTerminal::OnMissionChoiceOne).bConsumeInput = false;
            InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ASPPortTerminal::OnMissionChoiceTwo).bConsumeInput = false;
            InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ASPPortTerminal::OnCycleCargoGood).bConsumeInput = false;
            InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ASPPortTerminal::OnBuyCargo).bConsumeInput = false;
            InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ASPPortTerminal::OnLoadCargo).bConsumeInput = false;
            InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ASPPortTerminal::OnUnloadCargo).bConsumeInput = false;
            InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ASPPortTerminal::OnSellCargo).bConsumeInput = false;
        }
    }
}

void ASPPortTerminal::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bRemotePortProxy) UpdateRemoteProxy();
    const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    const bool bNearby = Pawn && !Pawn->IsA(ASPFlightPawn::StaticClass()) &&
        FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractionRadiusCm);
    if (TerminalLabel) TerminalLabel->SetVisibility(bNearby && !IsHidden());
}

void ASPPortTerminal::UpdateRemoteProxy()
{
    ASPWorldSurface* Surface = nullptr;
    ASPFlightPawn* LandedShip = nullptr;
    for (TActorIterator<ASPWorldSurface> It(GetWorld()); It; ++It)
    {
        Surface = *It;
        break;
    }
    for (TActorIterator<ASPFlightPawn> It(GetWorld()); It; ++It)
    {
        if (!It->bFlying)
        {
            LandedShip = *It;
            break;
        }
    }
    bool bVisible = false;
    if (Surface && Surface->WorldId != TEXT("earth") && LandedShip)
    {
        const FVector Up = LandedShip->GetActorUpVector().GetSafeNormal();
        // Put the board beyond the port-side hatch. The ground trace makes it
        // usable after landing anywhere, including terrain without a city mesh.
        const FVector Candidate = LandedShip->GetActorLocation()
            - LandedShip->GetActorRightVector() * 2500.0f
            - LandedShip->GetActorForwardVector() * 300.0f;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(SpacePatriotRemotePort), false, this);
        Params.AddIgnoredActor(LandedShip);
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Candidate + Up * 700.0f,
            Candidate - Up * 5000.0f, ECC_Visibility, Params) &&
            FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), Up) >= 0.55f)
        {
            const FVector Location = Hit.ImpactPoint + Up * 78.0f;
            const FRotator Facing = FRotationMatrix::MakeFromZX(Up,
                LandedShip->GetActorForwardVector()).Rotator();
            SetActorLocationAndRotation(Location, Facing, false, nullptr, ETeleportType::TeleportPhysics);
            bVisible = true;
        }
    }
    SetActorHiddenInGame(!bVisible);
    SetActorEnableCollision(bVisible);
}

void ASPPortTerminal::OnInteractPressed()
{
    TryInteract();
}

void ASPPortTerminal::OnMissionPressed()
{
    TryMissionInteract();
}

void ASPPortTerminal::OnCycleMissionPressed()
{
    TryCycleMission();
}

void ASPPortTerminal::OnMissionChoiceOne()
{
    TryMissionChoice(1);
}

void ASPPortTerminal::OnMissionChoiceTwo()
{
    TryMissionChoice(2);
}

void ASPPortTerminal::OnCycleCargoGood() { TryCycleCargoGood(); }
void ASPPortTerminal::OnBuyCargo() { TryBuyCargo(); }
void ASPPortTerminal::OnLoadCargo() { TryLoadCargo(); }
void ASPPortTerminal::OnUnloadCargo() { TryUnloadCargo(); }
void ASPPortTerminal::OnSellCargo() { TrySellCargo(); }

void ASPPortTerminal::Report(const FString& Message, bool bSuccess, float DurationSeconds)
{
    LastMessage = Message;
    UE_LOG(LogTemp, Display, TEXT("Space Patriot port terminal: %s"), *Message);
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, DurationSeconds,
        bSuccess ? FColor(175, 235, 191) : FColor(237, 189, 126), Message);
}

bool ASPPortTerminal::CanPlayerUseTerminal() const
{
    if (IsHidden() || !GetActorEnableCollision() || !GetWorld()) return false;
    const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    return Pawn && !Pawn->IsA(ASPFlightPawn::StaticClass()) &&
        FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractionRadiusCm);
}

bool ASPPortTerminal::ShowMissionNode(USPStoryCampaignComponent* Campaign, const FSPStoryQuestView& Quest)
{
    FSPStoryNodeView Node;
    if (!Campaign || !Campaign->GetCurrentNode(Quest.Id, Node)) return false;
    FString Message = FString::Printf(TEXT("%s: %s"), *Quest.Title, *Node.Title);
    if (!Node.Text.IsEmpty()) Message += TEXT("  ") + Node.Text;
    if (Node.Type == TEXT("objective"))
    {
        Message += FString::Printf(TEXT("  Target: %s on %s."), *Node.ObjectiveKind, *Node.WorldId);
        if (Node.ObjectiveKind == TEXT("delivery"))
            Message += FString::Printf(TEXT(" Bring %d %s and press F at the port board."), Node.Units, *Node.Good);
        else if (Node.ObjectiveKind == TEXT("terminal"))
            Message += TEXT(" Press F at that world's port board.");
    }
    else if (Node.Type == TEXT("dialogue"))
    {
        if (Node.Choices.Num() == 1) Message += TEXT("  Press M to continue.");
        else for (int32 Index = 0; Index < FMath::Min(Node.Choices.Num(), 2); ++Index)
            Message += FString::Printf(TEXT("  %d: %s"), Index + 1, *Node.Choices[Index].Text);
    }
    else if (Node.Type == TEXT("end")) Message += TEXT("  Case closed. Press M to open another case.");
    Report(Message, true, 16.0f);
    return true;
}

bool ASPPortTerminal::FindServices(FString& WorldId, FString& CityId,
    USPSocietySimulationComponent*& Society, USPStoryCampaignComponent*& Campaign) const
{
    WorldId.Empty();
    CityId.Empty();
    Society = nullptr;
    Campaign = nullptr;
    for (TActorIterator<ASPWorldRuntime> It(GetWorld()); It; ++It)
    {
        const ASPWorldRuntime* Runtime = *It;
        if (!Runtime || !Runtime->Systems) continue;
        WorldId = Runtime->Systems->ActiveWorldId;
        Society = Runtime->FindComponentByClass<USPSocietySimulationComponent>();
        Campaign = Runtime->FindComponentByClass<USPStoryCampaignComponent>();
        break;
    }
    if (WorldId.IsEmpty() || !Society || !Society->State) return false;
    // The nearby field board uses the exact settlement ledger selected for
    // this landing cell. The modular yard is still a temporary stand-in, not
    // a claim that the catalogued city has been fully rendered here.
    for (TActorIterator<ASPWorldDressing> It(GetWorld()); It; ++It)
    {
        const ASPWorldDressing* Dressing = *It;
        if (!Dressing || Dressing->ActiveWorldId != WorldId ||
            Dressing->ActiveServiceSettlementId.IsEmpty()) continue;
        FSPSocietySettlement Settlement;
        if (Society->GetSettlement(Dressing->ActiveServiceSettlementId, Settlement) &&
            Settlement.WorldId == WorldId)
        {
            CityId = Settlement.Id;
            return true;
        }
    }
    const TArray<FSPSocietySettlement> Cities = Society->GetSettlementsForWorld(WorldId);
    const FSPSocietySettlement* Primary = Cities.FindByPredicate(
        [](const FSPSocietySettlement& City) { return City.bPrimary; });
    if (Primary) CityId = Primary->Id;
    else if (!Cities.IsEmpty()) CityId = Cities[0].Id;
    return !CityId.IsEmpty();
}

bool ASPPortTerminal::FindCargoServices(FString& WorldId, FString& CityId,
    USPSocietySimulationComponent*& Society)
{
    if (!CanPlayerUseTerminal()) return false;
    USPStoryCampaignComponent* Campaign = nullptr;
    if (FindServices(WorldId, CityId, Society, Campaign)) return true;
    Report(TEXT("Dock ledger unavailable; cargo was not changed."), false);
    return false;
}

void ASPPortTerminal::ReportCargoStatus(USPSocietySimulationComponent* Society,
    const FString& WorldId, const FString& CityId, const FString& Action, bool bSuccess)
{
    if (!Society || !Society->State) return;
    FSPMarketRecord Market;
    Society->GetMarket(WorldId, Market);
    const int32 Stock = SelectedCargoGood == TEXT("ore") ? FMath::FloorToInt(Market.Ore) :
        SelectedCargoGood == TEXT("crystal") ? FMath::FloorToInt(Market.Crystal) :
        FMath::FloorToInt(Market.Organics);
    const FString Message = FString::Printf(
        TEXT("%s | %s: buy %d / sell %d cr, stock %d, dock %d, ship %d, hold %d/%d, wallet %d cr. [3 good 4 buy 5 load 6 unload 7 sell]"),
        *Action, *SelectedCargoGood,
        Society->GetPrice(WorldId, SelectedCargoGood, true),
        Society->GetPrice(WorldId, SelectedCargoGood, false), Stock,
        Society->GetStagedCargo(CityId, SelectedCargoGood),
        Society->GetHoldCargo(SelectedCargoGood), Society->GetHoldUsed(),
        Society->GetHoldCapacity(), Society->GetPlayerCredits());
    Report(Message, bSuccess, 11.0f);
}

bool ASPPortTerminal::TryCycleCargoGood()
{
    FString WorldId, CityId;
    USPSocietySimulationComponent* Society = nullptr;
    if (!FindCargoServices(WorldId, CityId, Society)) return false;
    SelectedCargoGood = SelectedCargoGood == TEXT("ore") ? TEXT("organics") :
        SelectedCargoGood == TEXT("organics") ? TEXT("crystal") : TEXT("ore");
    ReportCargoStatus(Society, WorldId, CityId, TEXT("Dock market"), true);
    return true;
}

bool ASPPortTerminal::TryBuyCargo()
{
    FString WorldId, CityId;
    USPSocietySimulationComponent* Society = nullptr;
    if (!FindCargoServices(WorldId, CityId, Society)) return false;
    const int32 Staged = Society->GetStagedCargo(CityId, TEXT("ore")) +
        Society->GetStagedCargo(CityId, TEXT("organics")) +
        Society->GetStagedCargo(CityId, TEXT("crystal"));
    if (Society->GetHoldUsed() + Staged >= Society->GetHoldCapacity())
    {
        ReportCargoStatus(Society, WorldId, CityId, TEXT("Purchase blocked: ship and dock allocation full"), false);
        return false;
    }
    if (!Society->BuyAtSettlement(CityId, SelectedCargoGood, 1))
    {
        ReportCargoStatus(Society, WorldId, CityId, TEXT("Purchase blocked: check credits or stock"), false);
        return false;
    }
    if (Society->bAutoSave) Society->SaveSociety();
    ReportCargoStatus(Society, WorldId, CityId, TEXT("Bought one unit into dock staging"), true);
    return true;
}

bool ASPPortTerminal::TryLoadCargo()
{
    FString WorldId, CityId;
    USPSocietySimulationComponent* Society = nullptr;
    if (!FindCargoServices(WorldId, CityId, Society)) return false;
    const FSPFreightContract* Contract = SealedContract(Society, SelectedCargoGood);
    if (Contract && Contract->Status == TEXT("awaiting loading") && Contract->FromWorldId == WorldId)
    {
        ReportCargoStatus(Society, WorldId, CityId,
            TEXT("Sealed shipment: press F to load its full manifest"), false);
        return false;
    }
    if (!Society->TransferCargo(CityId, SelectedCargoGood, 1, true))
    {
        ReportCargoStatus(Society, WorldId, CityId,
            TEXT("Load blocked: no dock cargo or ship hold is full"), false);
        return false;
    }
    if (Society->bAutoSave) Society->SaveSociety();
    ReportCargoStatus(Society, WorldId, CityId, TEXT("Loaded one unit into ship hold"), true);
    return true;
}

bool ASPPortTerminal::TryUnloadCargo()
{
    FString WorldId, CityId;
    USPSocietySimulationComponent* Society = nullptr;
    if (!FindCargoServices(WorldId, CityId, Society)) return false;
    const FSPFreightContract* Contract = SealedContract(Society, SelectedCargoGood);
    if (Contract && Contract->Status == TEXT("in transit") && Contract->ToWorldId != WorldId)
    {
        ReportCargoStatus(Society, WorldId, CityId,
            FString::Printf(TEXT("Sealed shipment must reach %s"), *Contract->ToWorldId), false);
        return false;
    }
    if (!Society->TransferCargo(CityId, SelectedCargoGood, 1, false))
    {
        ReportCargoStatus(Society, WorldId, CityId, TEXT("Unload blocked: no matching cargo aboard"), false);
        return false;
    }
    if (Society->bAutoSave) Society->SaveSociety();
    ReportCargoStatus(Society, WorldId, CityId, TEXT("Unloaded one unit onto dock"), true);
    return true;
}

bool ASPPortTerminal::TrySellCargo()
{
    FString WorldId, CityId;
    USPSocietySimulationComponent* Society = nullptr;
    if (!FindCargoServices(WorldId, CityId, Society)) return false;
    const FSPFreightContract* Contract = SealedContract(Society, SelectedCargoGood);
    if (Contract && ((Contract->Status == TEXT("awaiting loading") && Contract->FromWorldId == WorldId) ||
        (Contract->Status == TEXT("in transit") && Contract->ToWorldId == WorldId)))
    {
        ReportCargoStatus(Society, WorldId, CityId, TEXT("Sealed shipment: press F for freight"), false);
        return false;
    }
    if (!Society->SellAtSettlement(CityId, SelectedCargoGood, 1))
    {
        ReportCargoStatus(Society, WorldId, CityId, TEXT("Sale blocked: unload cargo at this dock first"), false);
        return false;
    }
    if (Society->bAutoSave) Society->SaveSociety();
    ReportCargoStatus(Society, WorldId, CityId, TEXT("Sold one staged unit"), true);
    return true;
}

bool ASPPortTerminal::AdvanceTerminalObjective(USPStoryCampaignComponent* Campaign, const FString& WorldId)
{
    if (!Campaign || !Campaign->State) return false;
    for (const FSPStoryQuestView& Quest : Campaign->GetQuestSummaries())
    {
        if (Quest.Status != TEXT("active")) continue;
        FSPStoryNodeView Node;
        if (!Campaign->GetCurrentNode(Quest.Id, Node) || Node.Type != TEXT("objective") ||
            Node.WorldId != WorldId) continue;
        if (Node.ObjectiveKind == TEXT("terminal"))
        {
            if (!Campaign->SignalGameplay(TEXT("terminal"), WorldId)) continue;
            Report(FString::Printf(TEXT("%s: terminal record logged. Press M for the next lead."), *Quest.Title), true);
            return true;
        }
        if (Node.ObjectiveKind == TEXT("delivery") &&
            Campaign->DeliverObjective(Quest.Id, WorldId, true))
        {
            Report(FString::Printf(TEXT("%s: %d %s delivered. Press M for the next lead."),
                *Quest.Title, Node.Units, *Node.Good), true);
            return true;
        }
    }
    return false;
}

bool ASPPortTerminal::ProcessFreight(USPSocietySimulationComponent* Society,
    const FString& WorldId, const FString& CityId)
{
    // One contract at a time keeps this first physical job board legible.
    for (const FSPFreightContract& Contract : Society->State->FreightContracts)
    {
        if (Contract.Status != TEXT("in transit") && Contract.Status != TEXT("awaiting loading")) continue;
        if (Contract.Status == TEXT("awaiting loading"))
        {
            if (Contract.FromWorldId != WorldId)
            {
                Report(FString::Printf(TEXT("Freight waits at %s. Return there to load it."), *Contract.FromWorldId), false);
                return false;
            }
            if (!Society->TransferCargo(CityId, Contract.Good, Contract.Units, true))
            {
                Report(TEXT("Cargo hold full. Make room before loading this contract."), false);
                return false;
            }
            if (Society->bAutoSave) Society->SaveSociety();
            Report(FString::Printf(TEXT("Loaded %d %s. Deliver to %s."), Contract.Units,
                *Contract.Good, *Contract.ToWorldId), true);
            return true;
        }
        if (Contract.ToWorldId != WorldId)
        {
            Report(FString::Printf(TEXT("Freight aboard: %d %s for %s. Use your ship's sector drive."),
                Contract.Units, *Contract.Good, *Contract.ToWorldId), false);
            return false;
        }
        // The player may already have unloaded part or all of the shipment
        // with 6. Move only the remainder from the hold before settlement.
        const int32 AlreadyStaged = Society->GetStagedCargo(CityId, Contract.Good);
        const int32 Remaining = FMath::Max(0, Contract.Units - AlreadyStaged);
        if (Society->GetHoldCargo(Contract.Good) < Remaining ||
            (Remaining > 0 && !Society->TransferCargo(CityId, Contract.Good, Remaining, false)))
        {
            Report(FString::Printf(TEXT("Delivery needs %d %s on this dock or aboard the ship."),
                Contract.Units, *Contract.Good), false);
            return false;
        }
        if (!Society->DeliverFreightContract(CityId, Contract.Id))
        {
            // Delivery validation may fail if market state changed; return the
            // cargo to the hold so a failed interaction never consumes it.
            if (Remaining > 0) Society->TransferCargo(CityId, Contract.Good, Remaining, true);
            Report(TEXT("Port ledger rejected delivery. Cargo returned to the hold."), false);
            return false;
        }
        if (Society->bAutoSave) Society->SaveSociety();
        Report(FString::Printf(TEXT("Delivered %d %s from %s. +%d credits."),
            Contract.Units, *Contract.Good, *Contract.FromWorldId, Contract.Reward), true);
        return true;
    }

    const USPTravelNavigationComponent* Navigation = nullptr;
    for (TActorIterator<ASPPlayLoopDirector> It(GetWorld()); It; ++It)
    {
        Navigation = It->HyperjumpRoute ? It->HyperjumpRoute->GetNavigationComponent() : nullptr;
        if (Navigation) break;
    }
    for (const FSPFreightContract& Offer : Society->GetFreightContractsForWorld(WorldId))
    {
        if (Offer.FromWorldId != WorldId || Offer.Status != TEXT("available")) continue;
        FSPTravelWorld Destination;
        // A freight offer must be completable in this land-anywhere slice.
        if (!Navigation || !Navigation->FindWorld(Offer.ToWorldId, Destination) ||
            Destination.Biome == TEXT("gas")) continue;
        if (!Society->AcceptFreightContract(CityId, Offer.Id)) continue;
        const bool bLoaded = Society->TransferCargo(CityId, Offer.Good, Offer.Units, true);
        if (Society->bAutoSave) Society->SaveSociety();
        Report(FString::Printf(TEXT("Accepted %d %s for %s, reward %d credits. %s"),
            Offer.Units, *Offer.Good, *Offer.ToWorldId, Offer.Reward,
            bLoaded ? TEXT("Cargo loaded.") : TEXT("Return when the hold has room to load.")), true);
        return true;
    }
    Report(TEXT("No freight contract is currently available at this port."), false);
    return false;
}

bool ASPPortTerminal::TryInteract()
{
    if (!CanPlayerUseTerminal()) return false;

    FString WorldId, CityId;
    USPSocietySimulationComponent* Society = nullptr;
    USPStoryCampaignComponent* Campaign = nullptr;
    if (!FindServices(WorldId, CityId, Society, Campaign))
    {
        Report(TEXT("Port services are offline. No freight or mission state was changed."), false);
        return false;
    }
    if (AdvanceTerminalObjective(Campaign, WorldId)) return true;
    return ProcessFreight(Society, WorldId, CityId);
}

bool ASPPortTerminal::TryMissionInteract()
{
    if (!CanPlayerUseTerminal()) return false;
    FString WorldId, CityId;
    USPSocietySimulationComponent* Society = nullptr;
    USPStoryCampaignComponent* Campaign = nullptr;
    if (!FindServices(WorldId, CityId, Society, Campaign) || !Campaign || !Campaign->State)
    {
        Report(TEXT("Mission records are unavailable at this port."), false);
        return false;
    }

    const TArray<FSPStoryQuestView> Quests = Campaign->GetQuestSummaries();
    const FSPStoryQuestView* Selected = Quests.FindByPredicate([this](const FSPStoryQuestView& Quest)
    {
        return Quest.Id == SelectedQuestId &&
            (Quest.Status == TEXT("active") || (Quest.Status == TEXT("available") && Quest.bUnlocked));
    });
    if (!Selected) Selected = Quests.FindByPredicate([](const FSPStoryQuestView& Quest)
    {
        return Quest.Status == TEXT("active");
    });
    if (!Selected) Selected = Quests.FindByPredicate([](const FSPStoryQuestView& Quest)
    {
        return Quest.Status == TEXT("available") && Quest.bUnlocked;
    });
    if (!Selected)
    {
        Report(TEXT("No unlocked case is waiting at this port. Complete the current investigation to open the next one."), false);
        return false;
    }
    SelectedQuestId = Selected->Id;
    if (Selected->Status == TEXT("active"))
    {
        FSPStoryNodeView Node;
        if (!Campaign->GetCurrentNode(Selected->Id, Node)) return false;
        if (Node.Type == TEXT("dialogue") && Node.Choices.Num() == 1)
        {
            if (!Campaign->Choose(Selected->Id, Node.Choices[0].Id)) return false;
        }
        return ShowMissionNode(Campaign, *Selected);
    }
    if (!Campaign->StartQuest(Selected->Id)) return false;
    return ShowMissionNode(Campaign, *Selected);
}

bool ASPPortTerminal::TryCycleMission()
{
    if (!CanPlayerUseTerminal()) return false;
    FString WorldId, CityId;
    USPSocietySimulationComponent* Society = nullptr;
    USPStoryCampaignComponent* Campaign = nullptr;
    if (!FindServices(WorldId, CityId, Society, Campaign) || !Campaign || !Campaign->State) return false;
    TArray<FSPStoryQuestView> Candidates;
    for (const FSPStoryQuestView& Quest : Campaign->GetQuestSummaries())
    {
        if (Quest.Status == TEXT("active") || (Quest.Status == TEXT("available") && Quest.bUnlocked))
            Candidates.Add(Quest);
    }
    if (Candidates.IsEmpty())
    {
        Report(TEXT("No open or unlocked cases are on the board."), false);
        return false;
    }
    const int32 CurrentIndex = Candidates.IndexOfByPredicate([this](const FSPStoryQuestView& Quest)
    {
        return Quest.Id == SelectedQuestId;
    });
    const FSPStoryQuestView& Selection = Candidates[(CurrentIndex + 1) % Candidates.Num()];
    SelectedQuestId = Selection.Id;
    if (Selection.Status == TEXT("active")) return ShowMissionNode(Campaign, Selection);
    Report(FString::Printf(TEXT("Case selected: %s. Press M to open it."), *Selection.Title), true, 10.0f);
    return true;
}

bool ASPPortTerminal::TryMissionChoice(int32 ChoiceNumber)
{
    if (!CanPlayerUseTerminal() || ChoiceNumber < 1) return false;
    FString WorldId, CityId;
    USPSocietySimulationComponent* Society = nullptr;
    USPStoryCampaignComponent* Campaign = nullptr;
    if (!FindServices(WorldId, CityId, Society, Campaign) || !Campaign || !Campaign->State) return false;
    const TArray<FSPStoryQuestView> Quests = Campaign->GetQuestSummaries();
    const FSPStoryQuestView* Selected = Quests.FindByPredicate([this](const FSPStoryQuestView& Quest)
    {
        return Quest.Id == SelectedQuestId && Quest.Status == TEXT("active");
    });
    if (!Selected) Selected = Quests.FindByPredicate([](const FSPStoryQuestView& Quest)
    {
        return Quest.Status == TEXT("active");
    });
    if (!Selected) return false;
    FSPStoryNodeView Node;
    if (!Campaign->GetCurrentNode(Selected->Id, Node) || Node.Type != TEXT("dialogue") ||
        !Node.Choices.IsValidIndex(ChoiceNumber - 1)) return false;
    if (!Campaign->Choose(Selected->Id, Node.Choices[ChoiceNumber - 1].Id)) return false;
    return ShowMissionNode(Campaign, *Selected);
}
