#include "SPPortTerminalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void USPPortTerminalWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;

    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PortCanvas"));
    WidgetTree->RootWidget = Canvas;

    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PortFrame"));
    Frame->SetBrushColor(FLinearColor(0.018f, 0.024f, 0.023f, 0.93f));
    Frame->SetPadding(FMargin(16.0f, 12.0f));
    UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame);
    FrameSlot->SetAnchors(FAnchors(1.0f, 0.0f));
    FrameSlot->SetAlignment(FVector2D(1.0f, 0.0f));
    FrameSlot->SetPosition(FVector2D(-18.0f, 18.0f));
    FrameSlot->SetSize(FVector2D(520.0f, 250.0f));

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PortStack"));
    Frame->SetContent(Stack);
    HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PortHeader"));
    HeaderText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 20));
    HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.77f, 0.55f)));
    Stack->AddChildToVerticalBox(HeaderText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

    BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PortBody"));
    BodyText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 15));
    BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.76f, 0.87f, 0.78f)));
    BodyText->SetAutoWrapText(true);
    Stack->AddChildToVerticalBox(BodyText);
    SetVisibility(ESlateVisibility::Collapsed);
}

void USPPortTerminalWidget::SetReadout(const FString& WorldName, const FString& Message)
{
    if (HeaderText) HeaderText->SetText(FText::FromString(FString::Printf(
        TEXT("PORT SERVICES  /  %s"), *WorldName.ToUpper())));
    if (BodyText) BodyText->SetText(FText::FromString(Message));
}
