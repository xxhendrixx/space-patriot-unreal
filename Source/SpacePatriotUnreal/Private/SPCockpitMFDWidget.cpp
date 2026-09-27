#include "SPCockpitMFDWidget.h"

#include "SPFlightPawn.h"
#include "SPVesselSystemsComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace
{
    UTextBlock* MakeLabel(UWidgetTree* Tree, const TCHAR* Name, int32 Size, FLinearColor Color)
    {
        UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
        Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), Size));
        Label->SetColorAndOpacity(FSlateColor(Color));
        Label->SetText(FText::GetEmpty());
        return Label;
    }

    UButton* MakeButton(UWidgetTree* Tree, UHorizontalBox* Row, const TCHAR* Name, const TCHAR* Caption)
    {
        UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), FName(Name));
        // UButton has no public setter for this construction-time property.
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        Button->IsFocusable = false;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        Button->SetBackgroundColor(FLinearColor(0.12f, 0.17f, 0.18f, 1.0f));
        UTextBlock* Label = MakeLabel(Tree, *FString::Printf(TEXT("%sLabel"), Name), 15,
            FLinearColor(0.72f, 0.81f, 0.73f, 1.0f));
        Label->SetText(FText::FromString(Caption));
        CastChecked<UButtonSlot>(Button->AddChild(Label))->SetPadding(FMargin(9.0f, 4.0f));
        UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Button);
        Slot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
        return Button;
    }
}

void USPCockpitMFDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (WidgetTree->RootWidget) return;

    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MFDRoot"));
    WidgetTree->RootWidget = Canvas;
    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MFDFrame"));
    Frame->SetBrushColor(FLinearColor(0.012f, 0.026f, 0.028f, 0.94f));
    Frame->SetPadding(FMargin(16.0f, 12.0f, 15.0f, 10.0f));
    FrameSlot = Canvas->AddChildToCanvas(Frame);

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MFDStack"));
    Frame->SetContent(Stack);
    HeaderText = MakeLabel(WidgetTree, TEXT("MFDHeader"), 21, FLinearColor(0.65f, 0.75f, 0.62f));
    Stack->AddChildToVerticalBox(HeaderText)->SetPadding(FMargin(1.0f, 0.0f, 0.0f, 6.0f));

    UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MFDHeaderRule"));
    Rule->SetBrushColor(FLinearColor(0.28f, 0.39f, 0.32f, 0.85f));
    Stack->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 9.0f));

    BodyText = MakeLabel(WidgetTree, TEXT("MFDBody"), 19, FLinearColor(0.70f, 0.84f, 0.64f));
    BodyText->SetAutoWrapText(true);
    UVerticalBoxSlot* BodySlot = Stack->AddChildToVerticalBox(BodyText);
    BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    BodySlot->SetPadding(FMargin(1.0f, 0.0f, 0.0f, 8.0f));

    UHorizontalBox* Controls = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MFDControls"));
    Stack->AddChildToVerticalBox(Controls)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 5.0f));
    MakeButton(WidgetTree, Controls, TEXT("MFDPrevious"), TEXT("<"))->OnClicked.AddDynamic(this, &USPCockpitMFDWidget::OnPreviousPage);
    MakeButton(WidgetTree, Controls, TEXT("MFDNext"), TEXT(">"))->OnClicked.AddDynamic(this, &USPCockpitMFDWidget::OnNextPage);
    MakeButton(WidgetTree, Controls, TEXT("MFDTravel"), TEXT("TRAVEL"))->OnClicked.AddDynamic(this, &USPCockpitMFDWidget::OnTravelPreset);
    MakeButton(WidgetTree, Controls, TEXT("MFDCombat"), TEXT("COMBAT"))->OnClicked.AddDynamic(this, &USPCockpitMFDWidget::OnCombatPreset);
    MakeButton(WidgetTree, Controls, TEXT("MFDDim"), TEXT("DIM"))->OnClicked.AddDynamic(this, &USPCockpitMFDWidget::OnDim);

    HintText = MakeLabel(WidgetTree, TEXT("MFDHint"), 13, FLinearColor(0.47f, 0.58f, 0.53f));
    Stack->AddChildToVerticalBox(HintText);
    SetPresentation(false, false);
}

void USPCockpitMFDWidget::SetShip(ASPFlightPawn* InShip)
{
    Ship = InShip;
}

void USPCockpitMFDWidget::SetReadout(const FText& Readout, float Brightness)
{
    if (!HeaderText || !BodyText) return;
    FString Header;
    FString Body;
    if (!Readout.ToString().Split(TEXT("\n"), &Header, &Body))
    {
        Header = Readout.ToString();
    }
    HeaderText->SetText(FText::FromString(Header));
    BodyText->SetText(FText::FromString(Body));
    const float Gain = FMath::Clamp(Brightness, 0.15f, 1.0f);
    HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.75f, 0.62f, 1.0f) * Gain));
    BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.84f, 0.64f, 1.0f) * Gain));
}

void USPCockpitMFDWidget::SetPresentation(bool bCockpit, bool bPointerActive)
{
    if (!FrameSlot || !HintText) return;
    if (bCockpit)
    {
        FrameSlot->SetAnchors(FAnchors(0.5f, 1.0f));
        FrameSlot->SetAlignment(FVector2D(0.5f, 1.0f));
        FrameSlot->SetPosition(FVector2D(0.0f, -12.0f));
        FrameSlot->SetSize(FVector2D(600.0f, 270.0f));
    }
    else
    {
        FrameSlot->SetAnchors(FAnchors(0.0f, 1.0f));
        FrameSlot->SetAlignment(FVector2D(0.0f, 1.0f));
        FrameSlot->SetPosition(FVector2D(18.0f, -18.0f));
        FrameSlot->SetSize(FVector2D(600.0f, 270.0f));
    }
    HintText->SetText(FText::FromString(bPointerActive
        ? TEXT("TAB FLIGHT  /  F1 TRAVEL  F2 COMBAT  F3 PAGE  F4 DIM")
        : TEXT("TAB POINTER  /  F1 TRAVEL  F2 COMBAT  F3 PAGE  F4 DIM")));
}

void USPCockpitMFDWidget::OnPreviousPage()
{
    if (Ship.IsValid()) Ship->CycleMFDPage(-1);
}

void USPCockpitMFDWidget::OnNextPage()
{
    if (Ship.IsValid()) Ship->CycleMFDPage(1);
}

void USPCockpitMFDWidget::OnTravelPreset()
{
    if (!Ship.IsValid()) return;
    if (USPVesselSystemsComponent* Systems = Ship->FindComponentByClass<USPVesselSystemsComponent>())
    {
        Systems->ApplyPreset(ESPVesselProfile::Travel);
        Ship->RefreshMFD();
    }
}

void USPCockpitMFDWidget::OnCombatPreset()
{
    if (!Ship.IsValid()) return;
    if (USPVesselSystemsComponent* Systems = Ship->FindComponentByClass<USPVesselSystemsComponent>())
    {
        Systems->ApplyPreset(ESPVesselProfile::Combat);
        Ship->RefreshMFD();
    }
}

void USPCockpitMFDWidget::OnDim()
{
    if (Ship.IsValid()) Ship->AdjustMFDBrightness(-0.2f);
}
