#include "SPPlayLoopWidget.h"

#include "SPStoryCampaignComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"

namespace
{
    UTextBlock* AddLine(UWidgetTree* Tree, UVerticalBox* Stack, const TCHAR* Name, int32 Size,
        const FLinearColor& Color, float BottomPadding)
    {
        UTextBlock* Line = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
        Line->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), Size));
        Line->SetColorAndOpacity(FSlateColor(Color));
        Line->SetAutoWrapText(true);
        Stack->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
        return Line;
    }
}

void USPPlayLoopWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;

    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RouteCanvas"));
    WidgetTree->RootWidget = Canvas;
    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RouteFrame"));
    Frame->SetBrushColor(FLinearColor(0.018f, 0.026f, 0.027f, 0.91f));
    Frame->SetPadding(FMargin(14.0f, 11.0f, 14.0f, 10.0f));
    UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame);
    FrameSlot->SetAnchors(FAnchors(0.0f, 0.0f));
    FrameSlot->SetPosition(FVector2D(18.0f, 18.0f));
    FrameSlot->SetSize(FVector2D(520.0f, 163.0f));

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RouteStack"));
    Frame->SetContent(Stack);
    WorldText = AddLine(WidgetTree, Stack, TEXT("RouteWorld"), 20,
        FLinearColor(0.84f, 0.78f, 0.57f), 5.0f);
    ActionText = AddLine(WidgetTree, Stack, TEXT("RouteAction"), 17,
        FLinearColor(0.78f, 0.88f, 0.79f), 4.0f);
    DetailText = AddLine(WidgetTree, Stack, TEXT("RouteDetail"), 14,
        FLinearColor(0.60f, 0.71f, 0.66f), 5.0f);
    AddLine(WidgetTree, Stack, TEXT("JournalHint"), 13,
        FLinearColor(0.75f, 0.68f, 0.47f), 0.0f)->SetText(FText::FromString(TEXT("I  CASE JOURNAL")));

    JournalFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CampaignJournalFrame"));
    JournalFrame->SetBrushColor(FLinearColor(0.012f, 0.019f, 0.022f, 0.97f));
    JournalFrame->SetPadding(FMargin(25.0f, 20.0f));
    UCanvasPanelSlot* JournalSlot = Canvas->AddChildToCanvas(JournalFrame);
    JournalSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    JournalSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    JournalSlot->SetPosition(FVector2D::ZeroVector);
    JournalSlot->SetSize(FVector2D(760.0f, 560.0f));

    UVerticalBox* JournalStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CampaignJournalStack"));
    JournalFrame->SetContent(JournalStack);
    AddLine(WidgetTree, JournalStack, TEXT("CampaignJournalTitle"), 25,
        FLinearColor(0.90f, 0.79f, 0.55f), 7.0f)->SetText(FText::FromString(TEXT("THE LONG DEBT  /  CASE JOURNAL")));
    AddLine(WidgetTree, JournalStack, TEXT("CampaignJournalHelp"), 14,
        FLinearColor(0.58f, 0.72f, 0.67f), 16.0f)->SetText(FText::FromString(TEXT("I CLOSE  |  Cases update as you travel and act. Use M at a port board for dialogue.")));
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CampaignJournalScroll"));
    JournalStack->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    JournalBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CampaignJournalBody"));
    JournalBody->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17));
    JournalBody->SetColorAndOpacity(FSlateColor(FLinearColor(0.79f, 0.86f, 0.79f)));
    JournalBody->SetAutoWrapText(true);
    JournalBody->SetText(FText::FromString(TEXT("Loading case records...")));
    Scroll->AddChild(JournalBody);
    JournalFrame->SetVisibility(ESlateVisibility::Collapsed);
    // The persistent route HUD is on screen in both the ground and ship pawn.
    // Leave hit testing to the journal's scroll area when it is open.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void USPPlayLoopWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!IsListeningForInputAction(TEXT("SPJournal")))
    {
        FOnInputAction Toggle;
        Toggle.BindDynamic(this, &USPPlayLoopWidget::ToggleJournal);
        ListenForInputAction(TEXT("SPJournal"), IE_Pressed, true, Toggle);
    }
}

void USPPlayLoopWidget::NativeDestruct()
{
    StopListeningForAllInputActions();
    Super::NativeDestruct();
}

void USPPlayLoopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bJournalOpen || !GetWorld() || GetWorld()->GetTimeSeconds() < NextJournalRefreshSeconds) return;
    RefreshJournal();
    NextJournalRefreshSeconds = GetWorld()->GetTimeSeconds() + 0.5f;
}

void USPPlayLoopWidget::ToggleJournal()
{
    bJournalOpen = !bJournalOpen;
    if (JournalFrame) JournalFrame->SetVisibility(bJournalOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bJournalOpen)
    {
        RefreshJournal();
        if (GetWorld()) NextJournalRefreshSeconds = GetWorld()->GetTimeSeconds() + 0.5f;
    }
}

void USPPlayLoopWidget::RefreshJournal()
{
    if (!IsValid(Campaign) && GetWorld())
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            Campaign = It->FindComponentByClass<USPStoryCampaignComponent>();
            if (Campaign) break;
        }
    }
    if (!JournalBody) return;
    const FString NewText = FormatJournal(Campaign);
    if (JournalBody->GetText().ToString() != NewText) JournalBody->SetText(FText::FromString(NewText));
}

FString USPPlayLoopWidget::FormatJournal(const USPStoryCampaignComponent* Source)
{
    if (!IsValid(Source) || !Source->State)
        return TEXT("Case records are unavailable. Visit a staffed port when services return.");

    const TArray<FSPStoryQuestView> Quests = Source->GetQuestSummaries();
    FString Text;
    int32 ActiveCount = 0;
    int32 CompletedCount = 0;
    for (const FSPStoryQuestView& Quest : Quests)
    {
        if (Quest.Status == TEXT("completed")) ++CompletedCount;
        if (Quest.Status != TEXT("active")) continue;
        if (ActiveCount++ == 0) Text += TEXT("ACTIVE CASES\n\n");
        FSPStoryNodeView Node;
        Text += FString::Printf(TEXT("%s\n"), *Quest.Title.ToUpper());
        if (Source->GetCurrentNode(Quest.Id, Node))
        {
            if (Node.Type == TEXT("objective"))
            {
                Text += FString::Printf(TEXT("%s  [%d / %d]\n"), *Node.Title,
                    Node.Progress, FMath::Max(1, Node.Goal));
                if (!Node.WorldId.IsEmpty()) Text += FString::Printf(TEXT("Destination: %s\n"), *Node.WorldId);
                if (Node.ObjectiveKind == TEXT("delivery"))
                    Text += FString::Printf(TEXT("Cargo required: %d %s. Deliver at that world's port board.\n"),
                        Node.Units, *Node.Good);
            }
            else if (Node.Type == TEXT("dialogue"))
            {
                if (!Node.Speaker.IsEmpty()) Text += Node.Speaker + TEXT(": ");
                Text += Node.Text + TEXT("\n");
                for (int32 Index = 0; Index < Node.Choices.Num(); ++Index)
                    Text += FString::Printf(TEXT("%d. %s\n"), Index + 1, *Node.Choices[Index].Text);
                Text += TEXT("Speak at a port board (M) to continue.\n");
            }
        }
        Text += TEXT("\n");
    }
    if (ActiveCount == 0) Text += TEXT("NO ACTIVE CASE\nOpen a case at a nearby port board with M.\n\n");

    int32 OpenCount = 0;
    for (const FSPStoryQuestView& Quest : Quests)
    {
        if (Quest.Status != TEXT("available") || !Quest.bUnlocked) continue;
        if (OpenCount++ == 0) Text += TEXT("OPEN CASES\n");
        Text += FString::Printf(TEXT("  %s\n"), *Quest.Title);
    }
    if (OpenCount > 0) Text += TEXT("Select a case at a port board with H, then open it with M.\n\n");
    Text += FString::Printf(TEXT("COMPLETED CASES  %d / %d\n"), CompletedCount, Quests.Num());
    for (const FSPStoryQuestView& Quest : Quests)
        if (Quest.Status == TEXT("completed")) Text += FString::Printf(TEXT("  %s\n"), *Quest.Title);

    const TArray<FSPStoryJournalEntry> Entries = Source->GetJournal();
    if (!Entries.IsEmpty())
    {
        Text += TEXT("\nRECENT RECORDS\n");
        for (int32 Index = Entries.Num() - 1; Index >= FMath::Max(0, Entries.Num() - 4); --Index)
            Text += FString::Printf(TEXT("\n%02d  %s\n"), Entries[Index].Sequence, *Entries[Index].Text);
    }
    return Text;
}

void USPPlayLoopWidget::SetStatus(const FString& World, const FString& Action, const FString& Detail)
{
    if (WorldText) WorldText->SetText(FText::FromString(World));
    if (ActionText) ActionText->SetText(FText::FromString(Action));
    if (DetailText) DetailText->SetText(FText::FromString(Detail));
}
