#include "Blueprint/PaintContext.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CanvasTypes.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TableView.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "Misc/AutomationTest.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// UMG's new widgets and its input: the horizontal box, the switcher, the table, the image's texture, buttons, focus
// and navigation from the keyboard, the pad and the mouse.

namespace
{

	/** Paints a user widget over a 640 x 448 canvas (its hit-test grid follows) and returns the canvas's runs. */
	void PaintWidget(UUserWidget& Widget, TArray<FCanvasVertex>* OutVertices = nullptr,
		TArray<FCanvasPrimitiveRun>* OutRuns = nullptr)
	{
		FCanvas Canvas(640, 448);
		FPaintContext Ctx(Canvas);
		Widget.NativePaint(Ctx);
		if (OutVertices != nullptr && OutRuns != nullptr)
		{
			Canvas.GetPrimitives(*OutVertices, *OutRuns);
		}
	}

	/** A button holding a label, in Box. */
	UButton* AddButton(UWidgetTree& Tree, UPanelWidget& Box, const TCHAR* Label)
	{
		UButton* Button = Tree.ConstructWidget<UButton>();
		UTextBlock* Text = Tree.ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Label));
		(void)Button->SetContent(Text);
		(void)Box.AddChild(Button);
		return Button;
	}

	FReply PressKey(UUserWidget& Widget, const FKey& Key)
	{
		const FReply Down = Widget.ProcessKeyDownEvent(FKeyEvent(Key));
		(void)Widget.ProcessKeyUpEvent(FKeyEvent(Key));
		return Down;
	}

	/**
	 * A menu: a canvas holding, at (100, 100), a vertical box of three buttons and a horizontal box of two more below
	 * them (Left and Right).
	 */
	struct FTestMenu
	{
		TStrongObjectPtr<UUserWidget> Widget;
		UButton* Buttons[5] = {};
		UHorizontalBox* Row = nullptr;

		FTestMenu()
			: Widget(NewObject<UUserWidget>())
		{
			(void)Widget->Initialize();
			UWidgetTree& Tree = *Widget->WidgetTree;
			UCanvasPanel* Root = Tree.ConstructWidget<UCanvasPanel>();
			Tree.RootWidget = Root;
			UVerticalBox* Column = Tree.ConstructWidget<UVerticalBox>();
			UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(Column);
			ColumnSlot->SetPosition(FVector2D(100.0f, 100.0f));
			ColumnSlot->SetAutoSize(true);
			Buttons[0] = AddButton(Tree, *Column, TEXT("Play"));
			Buttons[1] = AddButton(Tree, *Column, TEXT("Options"));
			Buttons[2] = AddButton(Tree, *Column, TEXT("Quit"));
			Row = Tree.ConstructWidget<UHorizontalBox>();
			(void)Column->AddChildToVerticalBox(Row);
			Buttons[3] = AddButton(Tree, *Row, TEXT("Left"));
			Buttons[4] = AddButton(Tree, *Row, TEXT("Right"));
			Widget->bIsFocusable = true;
			PaintWidget(*Widget);
		}
	};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUMGFocusNavigationTest, "System.UMG.Focus.Navigation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FUMGFocusNavigationTest::RunTest(const FString& Parameters)
{
	// A menu takes the first arrow or d-pad press to focus its first button; the arrows, the d-pad and Tab then move
	// the focus to the nearest button that way (none past the edge); an explicit rule wins; a disabled or hidden
	// button is skipped; keys that do not navigate go back to the game.
	FTestMenu Menu;
	UUserWidget& Widget = *Menu.Widget;
	UButton** Buttons = Menu.Buttons;
	TestEqual("Five buttons in the grid", Widget.GetHittestGrid().Num(), 5);
	TestTrue("Nothing focused", Widget.GetFocusedWidget() == nullptr);
	TestTrue("The first press focuses the first button",
		PressKey(Widget, EKeys::Down).IsEventHandled() && Buttons[0]->HasKeyboardFocus());
	(void)PressKey(Widget, EKeys::Down);
	TestTrue("Down", Buttons[1]->HasKeyboardFocus());
	(void)PressKey(Widget, EKeys::Gamepad_DPad_Down);
	TestTrue("The d-pad's down", Buttons[2]->HasKeyboardFocus());
	// Down from Quit: the row's button whose centre is nearer Quit's across.
	const float QuitX = Buttons[2]->GetCachedGeometry().GetAbsoluteCenter().X;
	const bool bLeftNearer = FMath::Abs(Buttons[3]->GetCachedGeometry().GetAbsoluteCenter().X - QuitX) <=
		FMath::Abs(Buttons[4]->GetCachedGeometry().GetAbsoluteCenter().X - QuitX);
	(void)PressKey(Widget, EKeys::Down);
	TestTrue("Down to the row's nearer button", Buttons[bLeftNearer ? 3 : 4]->HasKeyboardFocus());
	(void)PressKey(Widget, EKeys::Left);
	TestTrue("Left", Buttons[3]->HasKeyboardFocus());
	(void)PressKey(Widget, EKeys::Right);
	TestTrue("Right", Buttons[4]->HasKeyboardFocus());
	TestTrue("Past the edge: kept, handled",
		PressKey(Widget, EKeys::Right).IsEventHandled() && Buttons[4]->HasKeyboardFocus());
	(void)PressKey(Widget, EKeys::Gamepad_DPad_Left);
	TestTrue("The d-pad's left", Buttons[3]->HasKeyboardFocus());
	(void)PressKey(Widget, EKeys::Up);
	TestTrue("Up", Buttons[2]->HasKeyboardFocus());
	(void)PressKey(Widget, EKeys::Tab);
	TestTrue("Tab: the next in paint order", Buttons[3]->HasKeyboardFocus());
	TestFalse("A key that does not navigate goes to the game", PressKey(Widget, EKeys::W).IsEventHandled());

	// An explicit rule: up from Left goes to Play.
	Buttons[3]->SetNavigationRuleExplicit(EUINavigation::Up, Buttons[0]);
	(void)PressKey(Widget, EKeys::Up);
	TestTrue("An explicit rule", Buttons[0]->HasKeyboardFocus());
	// Options disabled, Quit collapsed: down from Play goes to the row.
	Buttons[1]->SetIsEnabled(false);
	Buttons[2]->SetVisibility(ESlateVisibility::Collapsed);
	PaintWidget(Widget);
	(void)PressKey(Widget, EKeys::Down);
	TestTrue("Disabled and collapsed skipped", Buttons[3]->HasKeyboardFocus() || Buttons[4]->HasKeyboardFocus());

	// A user widget that is not a menu takes no key while nothing inside has the focus.
	TStrongObjectPtr<UUserWidget> Hud(NewObject<UUserWidget>());
	(void)Hud->Initialize();
	TestFalse("A HUD without focus takes no key", Hud->ProcessKeyDownEvent(FKeyEvent(EKeys::Down)).IsEventHandled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUMGButtonActivationTest, "System.UMG.Button.Activation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FUMGButtonActivationTest::RunTest(const FString& Parameters)
{
	// Accept (Enter, Space, the pad's Cross) presses the focused button and its release clicks; the mouse hovers,
	// presses and clicks over the button, and a release elsewhere does not click; a disabled button does nothing;
	// the HUD routes the player's keys and mouse to its widgets first.
	FTestMenu Menu;
	UUserWidget& Widget = *Menu.Widget;
	UButton& Play = *Menu.Buttons[0];
	UButton& Quit = *Menu.Buttons[2];
	int32 Clicks = 0;
	int32 Presses = 0;
	int32 Hovers = 0;
	Play.OnClicked.AddLambda([&Clicks]() { ++Clicks; });
	Play.OnPressed.AddLambda([&Presses]() { ++Presses; });
	Play.OnHovered.AddLambda([&Hovers]() { ++Hovers; });
	Play.SetKeyboardFocus();

	TestTrue("Accept pressed", Widget.ProcessKeyDownEvent(FKeyEvent(EKeys::Enter)).IsEventHandled());
	TestTrue("Held", Play.IsPressed() && Presses == 1 && Clicks == 0);
	(void)Widget.ProcessKeyUpEvent(FKeyEvent(EKeys::Enter));
	TestTrue("Released: one click", !Play.IsPressed() && Clicks == 1);
	(void)PressKey(Widget, EKeys::Gamepad_FaceButton_Bottom);
	TestEqual("The pad's Cross clicks", Clicks, 2);
	(void)PressKey(Widget, EKeys::SpaceBar);
	TestEqual("Space clicks", Clicks, 3);
	(void)PressKey(Widget, EKeys::Gamepad_FaceButton_Right);
	TestEqual("Back does not click", Clicks, 3);

	// The mouse over Play: hover, press, release over it.
	const FVector2D OverPlay = Play.GetCachedGeometry().GetAbsoluteCenter();
	const FVector2D Away(5.0f, 5.0f);
	TestTrue("Hover",
		Widget.ProcessMouseMoveEvent(FPointerEvent(OverPlay, FKey())).IsEventHandled() && Play.IsHovered() &&
			Hovers == 1);
	(void)Widget.ProcessMouseButtonDownEvent(FPointerEvent(OverPlay, EKeys::LeftMouseButton));
	TestTrue("Mouse pressed", Play.IsPressed());
	(void)Widget.ProcessMouseButtonUpEvent(FPointerEvent(OverPlay, EKeys::LeftMouseButton));
	TestEqual("Mouse click", Clicks, 4);
	(void)Widget.ProcessMouseButtonDownEvent(FPointerEvent(OverPlay, EKeys::LeftMouseButton));
	(void)Widget.ProcessMouseButtonUpEvent(FPointerEvent(Away, EKeys::LeftMouseButton));
	TestTrue("Released away: no click", Clicks == 4 && !Play.IsPressed());
	TestFalse("Nothing under the mouse", Widget.ProcessMouseMoveEvent(FPointerEvent(Away, FKey())).IsEventHandled());
	TestFalse("Unhovered", Play.IsHovered());
	// Clicking Quit gives it the focus.
	(void)Widget.ProcessMouseButtonDownEvent(
		FPointerEvent(Quit.GetCachedGeometry().GetAbsoluteCenter(), EKeys::LeftMouseButton));
	TestTrue("A click focuses", Quit.HasKeyboardFocus());
	(void)Widget.ProcessMouseButtonUpEvent(
		FPointerEvent(Quit.GetCachedGeometry().GetAbsoluteCenter(), EKeys::LeftMouseButton));
	// Disabled: no press, no click.
	Play.SetIsEnabled(false);
	Play.SetKeyboardFocus();
	(void)PressKey(Widget, EKeys::Enter);
	TestEqual("Disabled: no click", Clicks, 4);

	// Through the HUD: its widgets see the keys before the game.
	FScopedTestWorld TestWorld;
	AHUD* HUD = TestWorld->SpawnActor<AHUD>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull("A HUD", HUD))
	{
		return false;
	}
	UUserWidget* HudWidget = HUD->AddWidget<UUserWidget>();
	UButton* HudButton = HudWidget->WidgetTree->ConstructWidget<UButton>();
	UCanvasPanel* Root = HudWidget->WidgetTree->ConstructWidget<UCanvasPanel>();
	HudWidget->WidgetTree->RootWidget = Root;
	UCanvasPanelSlot* ButtonSlot = Root->AddChildToCanvas(HudButton);
	ButtonSlot->SetPosition(FVector2D(10.0f, 10.0f));
	ButtonSlot->SetSize(FVector2D(100.0f, 20.0f));
	int32 HudClicks = 0;
	HudButton->OnClicked.AddLambda([&HudClicks]() { ++HudClicks; });
	FCanvas Canvas(640, 448);
	HUD->Paint(Canvas);
	TestFalse("No focus: the game gets Enter", HUD->InputKey(EKeys::Enter, IE_Pressed));
	HudButton->SetKeyboardFocus();
	TestTrue("Focused: the HUD takes Enter", HUD->InputKey(EKeys::Enter, IE_Pressed));
	(void)HUD->InputKey(EKeys::Enter, IE_Released);
	HUD->InputMouseMove(FVector2D(50.0f, 20.0f));
	TestTrue("Hovered through the HUD", HudButton->IsHovered());
	TestTrue("The mouse's button", HUD->InputKey(EKeys::LeftMouseButton, IE_Pressed));
	(void)HUD->InputKey(EKeys::LeftMouseButton, IE_Released);
	TestEqual("Two clicks through the HUD", HudClicks, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUMGPanelsTest, "System.UMG.Panels.HorizontalBoxAndSwitcher",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FUMGPanelsTest::RunTest(const FString& Parameters)
{
	// A horizontal box: automatic slots at their widths, fill slots sharing the rest by value, vertical alignment; a
	// switcher shows its active child alone and asks for the largest.
	TStrongObjectPtr<UWidgetTree> Tree(NewObject<UWidgetTree>());
	UHorizontalBox* Box = Tree->ConstructWidget<UHorizontalBox>();
	UImage* Fixed = Tree->ConstructWidget<UImage>();
	Fixed->SetDesiredSizeOverride(FVector2D(40.0f, 10.0f));
	UImage* One = Tree->ConstructWidget<UImage>();
	One->SetDesiredSizeOverride(FVector2D(5.0f, 30.0f));
	UImage* Two = Tree->ConstructWidget<UImage>();
	Two->SetDesiredSizeOverride(FVector2D(5.0f, 5.0f));
	UHorizontalBoxSlot* FixedSlot = Box->AddChildToHorizontalBox(Fixed);
	FixedSlot->SetPadding(FMargin(4.0f, 0.0f));
	FixedSlot->SetVerticalAlignment(VAlign_Center);
	Box->AddChildToHorizontalBox(One)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	FSlateChildSize Double(ESlateSizeRule::Fill);
	Double.Value = 2.0f;
	Box->AddChildToHorizontalBox(Two)->SetSize(Double);
	TestTrue("Desired: the widths added, the tallest", Box->GetDesiredSize().Equals(FVector2D(58.0f, 30.0f)));
	FCanvas Canvas(640, 448);
	FPaintContext Ctx(Canvas);
	Box->Paint(Ctx, FVector2D(0.0f, 0.0f), FVector2D(200.0f, 30.0f));
	TestTrue("Automatic: its width, centred",
		Fixed->GetCachedGeometry().GetAbsolutePosition().Equals(FVector2D(4.0f, 10.0f)) &&
			Fixed->GetCachedGeometry().GetLocalSize().Equals(FVector2D(40.0f, 10.0f)));
	// 200 - 48 = 152 left: 1/3 and 2/3.
	TestTrue("Fill: a third",
		One->GetCachedGeometry().GetAbsolutePosition().X == 48.0f &&
			FMath::IsNearlyEqual(One->GetCachedGeometry().GetLocalSize().X, 152.0f / 3.0f, 1.0e-3f));
	TestTrue("Fill: two thirds",
		FMath::IsNearlyEqual(Two->GetCachedGeometry().GetLocalSize().X, 152.0f * 2.0f / 3.0f, 1.0e-3f));

	UWidgetSwitcher* Switcher = Tree->ConstructWidget<UWidgetSwitcher>();
	UImage* PageA = Tree->ConstructWidget<UImage>();
	PageA->SetDesiredSizeOverride(FVector2D(10.0f, 50.0f));
	UImage* PageB = Tree->ConstructWidget<UImage>();
	PageB->SetDesiredSizeOverride(FVector2D(60.0f, 20.0f));
	(void)Switcher->AddChild(PageA);
	(void)Switcher->AddChild(PageB);
	TestTrue("The largest of its pages", Switcher->GetDesiredSize().Equals(FVector2D(60.0f, 50.0f)));
	TestTrue("The first page by default", Switcher->GetActiveWidget() == PageA && Switcher->GetNumWidgets() == 2);
	Switcher->SetActiveWidget(PageB);
	TestEqual("SetActiveWidget", Switcher->GetActiveWidgetIndex(), 1);
	Switcher->SetActiveWidgetIndex(7);
	TestEqual("Clamped", Switcher->GetActiveWidgetIndex(), 1);
	FCanvas SwitcherCanvas(640, 448);
	FPaintContext SwitcherCtx(SwitcherCanvas);
	Switcher->Paint(SwitcherCtx, FVector2D(0.0f, 0.0f), FVector2D(60.0f, 50.0f));
	TArray<FCanvasVertex> Vertices;
	TArray<FCanvasPrimitiveRun> Runs;
	SwitcherCanvas.GetPrimitives(Vertices, Runs);
	TestTrue("Only the active page painted", Runs.Num() == 1 && Runs[0].NumVertices == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUMGImageTextureTest, "System.UMG.Image.Texture",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FUMGImageTextureTest::RunTest(const FString& Parameters)
{
	// An image with a texture brush draws its UV region tinted by the brush and the image's colour (alpha blended);
	// matching the size takes the texture's; without a texture it is its tint.
	TStrongObjectPtr<UWidgetTree> Tree(NewObject<UWidgetTree>());
	UTexture2D* Texture = UTexture2D::CreateTransient(32, 16);
	UImage* Image = Tree->ConstructWidget<UImage>();
	Image->SetBrushFromTexture(Texture, true);
	TestTrue("The texture's size", Image->GetDesiredSize().Equals(FVector2D(32.0f, 16.0f)));
	FSlateBrush Brush = Image->GetBrush();
	Brush.SetUVRegion(FBox2D(FVector2D(0.5f, 0.0f), FVector2D(1.0f, 0.5f)));
	Brush.TintColor = FLinearColor(1.0f, 0.5f, 1.0f, 1.0f);
	Image->SetBrush(Brush);
	Image->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));
	FCanvas Canvas(640, 448);
	FPaintContext Ctx(Canvas);
	Image->Paint(Ctx, FVector2D(10.0f, 20.0f), FVector2D(16.0f, 8.0f));
	UImage* Plain = Tree->ConstructWidget<UImage>();
	Plain->SetColorAndOpacity(FLinearColor(0.0f, 1.0f, 0.0f, 0.25f));
	TestTrue("UE's default brush size", Plain->GetDesiredSize().Equals(FVector2D(32.0f, 32.0f)));
	Plain->Paint(Ctx, FVector2D(0.0f, 0.0f), FVector2D(4.0f, 4.0f));
	TArray<FCanvasVertex> Vertices;
	TArray<FCanvasPrimitiveRun> Runs;
	Canvas.GetPrimitives(Vertices, Runs);
	if (!TestTrue("A textured tile, then a flat one",
			Runs.Num() == 2 && Runs[0].Texture == Texture && Runs[1].Texture == nullptr))
	{
		return false;
	}
	TestTrue("One to one with its region: nearest", Runs[0].bNearest);
	const FCanvasVertex& TopLeft = Vertices[0];
	const FCanvasVertex& BottomRight = Vertices[1];
	TestTrue(
		"The region's UVs", TopLeft.U == 0.5f && TopLeft.V == 0.0f && BottomRight.U == 1.0f && BottomRight.V == 0.5f);
	TestTrue("Tinted, half transparent", TopLeft.G == 0.5f && TopLeft.A == 0.5f);
	TestTrue("The plain image's tint", Vertices[2].G == 1.0f && Vertices[2].A == 0.25f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUMGTableViewTest, "System.UMG.TableView.SortAndLayout",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FUMGTableViewTest::RunTest(const FString& Parameters)
{
	// A scoreboard's table: a fill column for the name and fixed right-aligned ones for the numbers; rows sorted by a
	// column (numbers as numbers, ties in the order added), the highlight staying on its row; the header and every row
	// a font line tall; a cell's text where its column's alignment puts it; a tile a row and a label a cell.
	TStrongObjectPtr<UWidgetTree> Tree(NewObject<UWidgetTree>());
	UTableView* Table = Tree->ConstructWidget<UTableView>();
	Table->AddColumn(FTableViewColumn(TEXT("Name"), FText::FromString(TEXT("Name"))));
	Table->AddColumn(FTableViewColumn(TEXT("Kills"), FText::FromString(TEXT("K")), 40.0f, HAlign_Right));
	Table->AddColumn(FTableViewColumn(TEXT("Deaths"), FText::FromString(TEXT("D")), 40.0f, HAlign_Center));
	const TCHAR* Names[4] = {TEXT("Bot Ana"), TEXT("Player"), TEXT("Bot Zoe"), TEXT("Bot Luis")};
	const TCHAR* Kills[4] = {TEXT("3"), TEXT("12"), TEXT("3"), TEXT("9")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const int32 Row = Table->AddRow();
		Table->SetCellText(Row, 0, Names[Index]);
		Table->SetCellText(Row, 1, Kills[Index]);
		Table->SetCellText(Row, 2, FString::FromInt(Index));
	}
	Table->SetHighlightedRow(1);
	Table->SetSortMode(TEXT("Kills"), EColumnSortMode::Descending);
	TestEqual("12 first (numbers, not text)", Table->GetCellText(0, 0), FString(TEXT("Player")));
	TestEqual("Then 9", Table->GetCellText(1, 0), FString(TEXT("Bot Luis")));
	TestTrue("Ties in the order added",
		Table->GetCellText(2, 0) == TEXT("Bot Ana") && Table->GetCellText(3, 0) == TEXT("Bot Zoe"));
	TestEqual("The highlight moved with its row", Table->GetHighlightedRow(), 0);
	Table->SetSortMode(TEXT("Name"), EColumnSortMode::Ascending);
	TestTrue("By name", Table->GetCellText(0, 0) == TEXT("Bot Ana") && Table->GetCellText(3, 0) == TEXT("Player"));
	TestEqual("Still the player's", Table->GetHighlightedRow(), 3);
	Table->SetCellText(3, 0, TEXT("Aaron"));
	Table->SortRows();
	TestTrue(
		"Sorted again after a change", Table->GetCellText(0, 0) == TEXT("Aaron") && Table->GetHighlightedRow() == 0);

	const UFont* Font = UEngine::GetSmallFont();
	if (!TestNotNull("The small font", Font))
	{
		return false;
	}
	const float RowHeight = Font->GetLineHeight() + 2.0f;
	TestEqual("A row: a line and the padding", Table->GetRowHeight(), RowHeight);
	TestEqual("The header and four rows", Table->GetDesiredSize().Y, 5.0f * RowHeight);
	float X = 0.0f;
	float Width = 0.0f;
	Table->GetColumnLayout(300.0f, 0, X, Width);
	TestTrue("The name fills what the numbers leave", X == 0.0f && Width == 220.0f);
	Table->GetColumnLayout(300.0f, 1, X, Width);
	TestTrue("Kills after it, 40 wide", X == 220.0f && Width == 40.0f);
	const float KillsWidth = float(Font->GetStringSize(*Table->GetCellText(1, 1)));
	TestEqual("A right-aligned cell ends inside its padding", Table->GetCellTextPosition(300.0f, 1, 1).X,
		FMath::RoundToFloat(220.0f + 4.0f + 32.0f - KillsWidth));
	TestEqual("Row 1's text below the header and row 0", Table->GetCellTextPosition(300.0f, 1, 0).Y,
		(2.0f * RowHeight) + 1.0f);

	FCanvas Canvas(640, 448);
	FPaintContext Ctx(Canvas);
	Table->Paint(Ctx, FVector2D(10.0f, 10.0f), FVector2D(300.0f, 5.0f * RowHeight));
	TArray<FCanvasVertex> Vertices;
	TArray<FCanvasPrimitiveRun> Runs;
	Canvas.GetPrimitives(Vertices, Runs);
	int32 Tiles = 0;
	for (const FCanvasPrimitiveRun& Run : Runs)
	{
		Tiles += Run.Texture == nullptr ? Run.NumVertices / 2 : 0;
	}
	TestEqual("A tile for the header and each row", Tiles, 5);
	TestTrue("The highlighted row's tile",
		Vertices[2].R == Table->HighlightBackgroundColor.R && Vertices[2].Y == 10.0f + RowHeight);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
