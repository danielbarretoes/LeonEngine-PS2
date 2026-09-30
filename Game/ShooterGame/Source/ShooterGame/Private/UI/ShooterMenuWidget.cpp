#include "UI/ShooterMenuWidget.h"

#include "AudioDevice.h"
#include "Blueprint/WidgetTree.h"
#include "CanvasTypes.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Framework/Application/NavigationConfig.h"
#include "GameFramework/HUD.h"
#include "ShooterPersistentUser.h"
#include "ShooterPlayerController.h"

namespace
{
	/** The engine's fonts by size (UEngine's Small 14, Medium 20, Large 32). */
	constexpr int32 TitleFontSize = 32;
	constexpr int32 LineFontSize = 20;
	constexpr int32 HintFontSize = 14;

	/** CS 1.6's (Steam 2003) look: olive panels, light text, an amber title. */
	FButtonStyle MakeLineStyle()
	{
		FButtonStyle Style;
		Style.Normal.TintColor = FLinearColor(0.17f, 0.19f, 0.15f, 0.95f);
		Style.Hovered.TintColor = FLinearColor(0.42f, 0.47f, 0.33f, 1.0f);
		Style.Pressed.TintColor = FLinearColor(0.58f, 0.52f, 0.24f, 1.0f);
		Style.Disabled.TintColor = FLinearColor(0.12f, 0.12f, 0.12f, 0.6f);
		Style.NormalPadding = FMargin(10.0f, 3.0f, 10.0f, 3.0f);
		Style.PressedPadding = FMargin(10.0f, 4.0f, 10.0f, 2.0f);
		return Style;
	}

	/** The menus' canvas batch: in front of the HUD's (FCanvas draws the lower keys last). */
	constexpr int32 MenuDepthSortKey = -1;

	/** Left or right (the arrows, the D-pad): -1, +1, else 0. */
	int32 GetStepFromKey(const FKey& Key)
	{
		if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
		{
			return -1;
		}
		if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
		{
			return 1;
		}
		return 0;
	}
} // namespace

const FLinearColor UShooterMenuWidget::PanelColor(0.07f, 0.08f, 0.06f, 0.88f);
const FLinearColor UShooterMenuWidget::TextColor(0.86f, 0.88f, 0.82f, 1.0f);
const FLinearColor UShooterMenuWidget::TitleColor(0.98f, 0.76f, 0.22f, 1.0f);
const FLinearColor UShooterMenuWidget::HintColor(0.62f, 0.66f, 0.58f, 1.0f);

// UShooterMenuButton

UShooterMenuButton::UShooterMenuButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetStyle(MakeLineStyle());
}

void UShooterMenuButton::Build(UWidgetTree& Tree, const FString& InLabel, bool bIsOption, int32 FontSize)
{
	UHorizontalBox* Row = Tree.ConstructWidget<UHorizontalBox>();
	SetContent(Row);
	Label = Tree.ConstructWidget<UTextBlock>();
	Label->SetFont(FSlateFontInfo(nullptr, FontSize));
	Label->SetText(FText::FromString(InLabel));
	Label->SetColorAndOpacity(UShooterMenuWidget::TextColor);
	Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	if (bIsOption)
	{
		Value = Tree.ConstructWidget<UTextBlock>();
		Value->SetFont(FSlateFontInfo(nullptr, FontSize));
		Value->SetColorAndOpacity(UShooterMenuWidget::TitleColor);
		Value->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Value);
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ValueSlot->SetVerticalAlignment(VAlign_Center);
		OnClicked.AddUObject(this, &UShooterMenuButton::OnOptionClicked);
	}
}

void UShooterMenuButton::SetValueText(const FString& Text)
{
	if (Value != nullptr)
	{
		Value->SetText(FText::FromString(FString::Printf(TEXT("<  %s  >"), *Text)));
	}
}

void UShooterMenuButton::OnOptionClicked()
{
	OnValueStep.Broadcast(1);
}

FReply UShooterMenuButton::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const int32 Step = GetStepFromKey(InKeyEvent.GetKey());
	if (Value != nullptr && Step != 0 && GetIsEnabled())
	{
		UShooterMenuWidget::PlayMenuSoundStatic(false);
		OnValueStep.Broadcast(Step);
		return FReply::Handled();
	}
	return Super::OnKeyDown(MyGeometry, InKeyEvent);
}

// UShooterMenuWidget

UShooterMenuWidget::UShooterMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A menu takes the keys even before a line has the focus (UUserWidget::bIsFocusable).
	bIsFocusable = true;
}

void UShooterMenuWidget::NativeOnInitialized()
{
	// Canvas > [Shade], Border (the panel) > VerticalBox > Title, Switcher (the pages), Hint.
	Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	Shade = WidgetTree->ConstructWidget<UBorder>();
	Shade->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Shade->SetVisibility(ESlateVisibility::Collapsed);
	ShadeSlot = Root->AddChildToCanvas(Shade);
	Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(18.0f, 12.0f, 18.0f, 12.0f));
	PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAutoSize(true);
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Box);
	Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetFont(FSlateFontInfo(nullptr, TitleFontSize));
	Title->SetColorAndOpacity(TitleColor);
	Title->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	Title->SetShadowOffset(FVector2D(2.0f, 2.0f));
	Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	// The panel is at least LineWidth wide: the lines fill it.
	UImage* Width = WidgetTree->ConstructWidget<UImage>();
	Width->SetDesiredSizeOverride(FVector2D(LineWidth, 0.0f));
	Width->SetColorAndOpacity(FLinearColor::Transparent);
	Box->AddChildToVerticalBox(Width);
	Pages = WidgetTree->ConstructWidget<UWidgetSwitcher>();
	Box->AddChildToVerticalBox(Pages);
	Hint = WidgetTree->ConstructWidget<UTextBlock>();
	Hint->SetFont(FSlateFontInfo(nullptr, HintFontSize));
	Hint->SetColorAndOpacity(HintColor);
	Box->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	BuildPages();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UShooterMenuWidget::NativePaint(FPaintContext& Ctx)
{
	const FVector2D Frame(static_cast<float>(Ctx.GetWidth()), static_cast<float>(Ctx.GetHeight()));
	const FVector2D Size = Panel->GetDesiredSize();
	PanelSlot->SetPosition(FVector2D(FMath::RoundToFloat((Frame.X - Size.X) * 0.5f),
		FMath::RoundToFloat(FMath::Max(0.0f, (Frame.Y - Size.Y) * 0.5f))));
	ShadeSlot->SetPosition(FVector2D::ZeroVector);
	ShadeSlot->SetSize(Frame);
	// In front of the HUD's own drawing (its batch, key 0: the canvas draws a lower key later), texts included.
	FCanvas& Canvas = Ctx.GetCanvas();
	Canvas.PushDepthSortKey(MenuDepthSortKey);
	Super::NativePaint(Ctx);
	Canvas.PopDepthSortKey();
}

FReply UShooterMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FReply Own = HandleMenuKey(InKeyEvent);
	if (Own.IsEventHandled())
	{
		return Own;
	}
	const FKey& Key = InKeyEvent.GetKey();
	if (FNavigationConfig::GetNavigationActionFromKey(Key) == EUINavigationAction::Back)
	{
		if (!InKeyEvent.IsRepeat())
		{
			PlayMenuSoundStatic(false);
			OnBack();
		}
		return FReply::Handled();
	}
	// The navigation keys move the focus (the user widget's rule); every other key stays in the menu.
	if (FNavigationConfig::GetNavigationDirectionFromKey(Key) != EUINavigation::Invalid)
	{
		PlayMenuSoundStatic(false);
		return FReply::Unhandled();
	}
	(void)InGeometry;
	return FReply::Handled();
}

void UShooterMenuWidget::SetShown(bool bShown)
{
	SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShown)
	{
		ShowPage(GetPage());
	}
	else
	{
		SetFocusedWidget(nullptr);
	}
}

int32 UShooterMenuWidget::GetPage() const
{
	return Pages != nullptr ? Pages->GetActiveWidgetIndex() : 0;
}

void UShooterMenuWidget::ShowPage(int32 Page)
{
	if (Pages == nullptr)
	{
		return;
	}
	Pages->SetActiveWidgetIndex(Page);
	if (UShooterMenuButton* First = GetFirstButton(GetPage()))
	{
		First->SetKeyboardFocus();
	}
}

UShooterMenuButton* UShooterMenuWidget::GetFirstButton(int32 Page) const
{
	const UPanelWidget* Box = Pages != nullptr ? Cast<UPanelWidget>(Pages->GetWidgetAtIndex(Page)) : nullptr;
	if (Box == nullptr)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < Box->GetChildrenCount(); ++Index)
	{
		UShooterMenuButton* Button = Cast<UShooterMenuButton>(Box->GetChildAt(Index));
		if (Button != nullptr && Button->IsVisible() && Button->GetIsEnabled())
		{
			return Button;
		}
	}
	return nullptr;
}

AShooterPlayerController* UShooterMenuWidget::GetShooterPlayerController() const
{
	const AHUD* HUD = GetOwningHUD();
	return HUD != nullptr ? Cast<AShooterPlayerController>(HUD->PlayerOwner) : nullptr;
}

UVerticalBox* UShooterMenuWidget::AddPage(int32& OutPage)
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();
	OutPage = Pages->GetChildrenCount();
	Pages->AddChild(Page);
	return Page;
}

UShooterMenuButton* UShooterMenuWidget::AddMenuButton(UVerticalBox& Page, const FString& InLabel, bool bIsOption)
{
	UShooterMenuButton* Button = WidgetTree->ConstructWidget<UShooterMenuButton>();
	Button->Build(*WidgetTree, InLabel, bIsOption, LineFontSize);
	Page.AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 2.0f));
	return Button;
}

UTextBlock* UShooterMenuWidget::AddTextLine(
	UVerticalBox& Page, const FString& Text, int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	Line->SetFont(FSlateFontInfo(nullptr, FontSize));
	Line->SetText(FText::FromString(Text));
	Line->SetColorAndOpacity(Color);
	Line->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	Page.AddChildToVerticalBox(Line)->SetPadding(FMargin(4.0f, 2.0f, 4.0f, 2.0f));
	return Line;
}

void UShooterMenuWidget::SetTitle(const FString& Text)
{
	Title->SetText(FText::FromString(Text));
}

void UShooterMenuWidget::SetHint(const FString& Text)
{
	Hint->SetText(FText::FromString(Text));
}

void UShooterMenuWidget::PlayMenuSoundStatic(bool bConfirm)
{
	if (GEngine != nullptr)
	{
		GEngine->GetAudioDevice().PlayUiSound(bConfirm ? EUISound::Confirm : EUISound::Click);
	}
}

void UShooterMenuWidget::BuildOptionsPage()
{
	UVerticalBox* Page = AddPage(OptionsPageIndex);
	OptionsPage.Sensitivity = AddMenuButton(*Page, TEXT("Aim sensitivity"), true);
	OptionsPage.Sensitivity->OnValueStep.AddLambda(
		[this](int32 Step) { OptionsPage.StepSensitivity(GetShooterPlayerController(), Step); });
	OptionsPage.InvertY = AddMenuButton(*Page, TEXT("Invert Y axis"), true);
	OptionsPage.InvertY->OnValueStep.AddLambda(
		[this](int32) { OptionsPage.StepInvertY(GetShooterPlayerController()); });
	OptionsPage.Volume = AddMenuButton(*Page, TEXT("Volume"), true);
	OptionsPage.Volume->OnValueStep.AddLambda(
		[this](int32 Step) { OptionsPage.StepVolume(GetShooterPlayerController(), Step); });
	OptionsPage.Crouch = AddMenuButton(*Page, TEXT("Crouch key"), true);
	OptionsPage.Crouch->OnValueStep.AddLambda([this](int32) { OptionsPage.StepCrouch(GetShooterPlayerController()); });
	OptionsPage.Back = AddMenuButton(*Page, TEXT("Back"));
	OptionsPage.Back->OnClicked.AddLambda([this]() { LeaveOptionsPage(); });
}

void UShooterMenuWidget::ShowOptionsPage()
{
	OptionsPage.bDirty = false;
	OptionsPage.Refresh(GetShooterPlayerController());
	ShowPage(OptionsPageIndex);
}

void UShooterMenuWidget::LeaveOptionsPage()
{
	OptionsPage.SaveIfDirty(GetShooterPlayerController());
	ShowPage(0);
}

// FShooterOptionsPage

void FShooterOptionsPage::StepSensitivity(AShooterPlayerController* Player, int32 Direction)
{
	if (Player == nullptr)
	{
		return;
	}
	UShooterPersistentUser* User = Player->GetPersistentUser();
	const float Next = User->AimSensitivity + (static_cast<float>(Direction) * SensitivityStep);
	// Stepping past an end wraps to the other (a click only steps forward).
	User->AimSensitivity = Next > MaxSensitivity + KINDA_SMALL_NUMBER ? MinSensitivity
		: Next < MinSensitivity - KINDA_SMALL_NUMBER                  ? MaxSensitivity
																	  : Next;
	bDirty = true;
	Player->ApplyPersistentUser();
	Refresh(Player);
}

void FShooterOptionsPage::StepInvertY(AShooterPlayerController* Player)
{
	if (Player == nullptr)
	{
		return;
	}
	UShooterPersistentUser* User = Player->GetPersistentUser();
	User->bInvertedYAxis = !User->bInvertedYAxis;
	bDirty = true;
	Player->ApplyPersistentUser();
	Refresh(Player);
}

void FShooterOptionsPage::StepVolume(AShooterPlayerController* Player, int32 Direction)
{
	if (Player == nullptr)
	{
		return;
	}
	UShooterPersistentUser* User = Player->GetPersistentUser();
	const float Next = User->SoundVolume + (static_cast<float>(Direction) * VolumeStep);
	User->SoundVolume = Next > 1.0f + KINDA_SMALL_NUMBER ? 0.0f
		: Next < -KINDA_SMALL_NUMBER                     ? 1.0f
														 : FMath::Clamp(Next, 0.0f, 1.0f);
	bDirty = true;
	Player->ApplyPersistentUser();
	Refresh(Player);
}

void FShooterOptionsPage::StepCrouch(AShooterPlayerController* Player)
{
	if (Player == nullptr)
	{
		return;
	}
	UShooterPersistentUser* User = Player->GetPersistentUser();
	User->bToggleCrouch = !User->bToggleCrouch;
	bDirty = true;
	Player->ApplyPersistentUser();
	Refresh(Player);
}

void FShooterOptionsPage::Refresh(AShooterPlayerController* Player) const
{
	if (Player == nullptr || Sensitivity == nullptr)
	{
		return;
	}
	const UShooterPersistentUser* User = Player->GetPersistentUser();
	Sensitivity->SetValueText(FString::Printf(TEXT("%.2f"), static_cast<double>(User->AimSensitivity)));
	InvertY->SetValueText(User->bInvertedYAxis ? TEXT("On") : TEXT("Off"));
	Volume->SetValueText(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(User->SoundVolume * 100.0f)));
	Crouch->SetValueText(User->bToggleCrouch ? TEXT("Toggle") : TEXT("Hold"));
}

void FShooterOptionsPage::SaveIfDirty(AShooterPlayerController* Player)
{
	if (bDirty && Player != nullptr)
	{
		Player->SavePersistentUser();
	}
	bDirty = false;
}
