#include "Blueprint/PaintContext.h"
#include "Blueprint/WidgetTree.h"
#include "CanvasTypes.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** The bounds of the canvas's Index-th tile (its 6 vertices, tiles first in a batch). */
	FBox2D GetTileBounds(const TArray<FCanvasVertex>& Vertices, int32 Index)
	{
		FBox2D Bounds(ForceInit);
		for (int32 Vertex = Index * 6; Vertex < (Index + 1) * 6; ++Vertex)
		{
			Bounds += FVector2D(Vertices[Vertex].X, Vertices[Vertex].Y);
		}
		return Bounds;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetTreeLayoutTest, "System.UMG.WidgetTree.LayoutAndPaint",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWidgetTreeLayoutTest::RunTest(const FString& Parameters)
{
	// A canvas holding, at (40, 100), a border (padding 10) around a vertical box of a text, an image 5 px below it
	// and a collapsed image: the sizes add up as UE's, and the paint puts every rectangle where the slots say.
	TStrongObjectPtr<UWidgetTree> Tree(NewObject<UWidgetTree>());
	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>();
	Tree->RootWidget = Root;
	UBorder* Border = Tree->ConstructWidget<UBorder>();
	Border->SetPadding(FMargin(10.0f));
	UCanvasPanelSlot* BorderSlot = Root->AddChildToCanvas(Border);
	BorderSlot->SetPosition(FVector2D(40.0f, 100.0f));
	BorderSlot->SetAutoSize(true);
	UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>();
	TestNotNull("The border's content", Border->SetContent(Box));
	TestNull("A single child", Border->AddChild(Tree->ConstructWidget<UImage>()));
	UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
	Text->SetText(FText::FromString(TEXT("Buy")));
	Box->AddChildToVerticalBox(Text);
	UImage* Image = Tree->ConstructWidget<UImage>();
	Image->SetDesiredSizeOverride(FVector2D(20.0f, 30.0f));
	Box->AddChildToVerticalBox(Image)->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 0.0f));
	UImage* Collapsed = Tree->ConstructWidget<UImage>();
	Collapsed->SetVisibility(ESlateVisibility::Collapsed);
	Box->AddChildToVerticalBox(Collapsed);

	TestTrue("Parents", Text->GetParent() == Box && Box->GetParent() == Border && Border->GetParent() == Root);
	TestEqual("Children", Box->GetChildrenCount(), 3);
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	FCanvas::MeasureText(TEXT("Buy"), HudFontScale, TextWidth, TextHeight);
	const FVector2D BoxSize(FMath::Max(TextWidth, 20.0f), TextHeight + 5.0f + 30.0f);
	TestTrue("The box's size (collapsed takes none)", Box->GetDesiredSize().Equals(BoxSize));
	TestTrue("The border's", Border->GetDesiredSize().Equals(BoxSize + FVector2D(20.0f, 20.0f)));
	TestTrue("The canvas's", Root->GetDesiredSize().Equals(FVector2D(40.0f, 100.0f) + BoxSize + FVector2D(20.0f)));

	FCanvas Canvas(640, 480);
	FPaintContext Ctx(Canvas);
	Root->Paint(Ctx, FVector2D::ZeroVector, FVector2D(640.0f, 480.0f));
	TArray<FCanvasVertex> Vertices;
	Canvas.GetTriangles(Vertices);
	if (!TestTrue("Two tiles and the text", Vertices.Num() > 12))
	{
		return false;
	}
	const FBox2D BorderTile = GetTileBounds(Vertices, 0);
	TestTrue("The border",
		BorderTile.Min.Equals(FVector2D(40.0f, 100.0f)) &&
			BorderTile.Max.Equals(FVector2D(40.0f, 100.0f) + BoxSize + FVector2D(20.0f)));
	const FBox2D ImageTile = GetTileBounds(Vertices, 1);
	TestTrue("The image below the text, as wide as the box",
		ImageTile.Min.Equals(FVector2D(50.0f, 110.0f + TextHeight + 5.0f)) &&
			ImageTile.GetSize().Equals(FVector2D(BoxSize.X, 30.0f)));

	// Moving a child to another panel takes it out of the first; RemoveFromParent frees it.
	UVerticalBox* Other = Tree->ConstructWidget<UVerticalBox>();
	Other->AddChild(Text);
	TestTrue("Moved", Text->GetParent() == Other && !Box->HasChild(Text) && Box->GetChildrenCount() == 2);
	Text->RemoveFromParent();
	TestTrue("Removed", Text->GetParent() == nullptr && Text->Slot == nullptr && Other->GetChildrenCount() == 0);
	Box->ClearChildren();
	TestTrue("Cleared", Box->GetChildrenCount() == 0 && Image->Slot == nullptr);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
