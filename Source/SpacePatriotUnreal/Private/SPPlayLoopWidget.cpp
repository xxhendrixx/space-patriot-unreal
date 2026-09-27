#include "SPPlayLoopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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
    FrameSlot->SetSize(FVector2D(520.0f, 142.0f));

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RouteStack"));
    Frame->SetContent(Stack);
    WorldText = AddLine(WidgetTree, Stack, TEXT("RouteWorld"), 20,
        FLinearColor(0.84f, 0.78f, 0.57f), 5.0f);
    ActionText = AddLine(WidgetTree, Stack, TEXT("RouteAction"), 17,
        FLinearColor(0.78f, 0.88f, 0.79f), 4.0f);
    DetailText = AddLine(WidgetTree, Stack, TEXT("RouteDetail"), 14,
        FLinearColor(0.60f, 0.71f, 0.66f), 0.0f);
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USPPlayLoopWidget::SetStatus(const FString& World, const FString& Action, const FString& Detail)
{
    if (WorldText) WorldText->SetText(FText::FromString(World));
    if (ActionText) ActionText->SetText(FText::FromString(Action));
    if (DetailText) DetailText->SetText(FText::FromString(Detail));
}
