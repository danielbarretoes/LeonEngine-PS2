#pragma once

#include "Components/Widget.h"
#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"
#include "TableView.generated.h"

/** The order a column sorts the rows in (UE: EColumnSortMode). */
enum class EColumnSortMode : uint8
{
	None,
	Ascending,
	Descending,
};

/** A table column (UE: SHeaderRow::FColumn, a subset): its id, header, width and the alignment of its cells. */
struct FTableViewColumn
{
	/** UE: ColumnId. */
	FName ColumnId;
	/** The header's text (UE: DefaultText). */
	FText DefaultText;
	/** A fixed width in pixels (UE: ManualWidth); 0: a FillWidth share of what the fixed columns leave. */
	float ManualWidth = 0.0f;
	/** UE: FillWidth. */
	float FillWidth = 1.0f;
	/** Where the cells' and the header's text sit in the column (UE: HAlignCell, HAlignHeader). */
	EHorizontalAlignment HAlignCell = HAlign_Left;
	EHorizontalAlignment HAlignHeader = HAlign_Left;

	FTableViewColumn() = default;
	FTableViewColumn(FName InColumnId, const FText& InDefaultText, float InManualWidth = 0.0f,
		EHorizontalAlignment InHAlign = HAlign_Left)
		: ColumnId(InColumnId)
		, DefaultText(InDefaultText)
		, ManualWidth(InManualWidth)
		, HAlignCell(InHAlign)
		, HAlignHeader(InHAlign)
	{
	}
};

/**
 * A table of text (Leon's UMG counterpart of UE's SListView with an SHeaderRow; UE's UListView makes entry widgets
 * instead): columns with a header, a width and an alignment, rows of cells, one column the rows are sorted by, and a
 * highlighted row (the local player on a scoreboard). Rows keep their identity through sorting (SortRows after
 * filling them): a row index is its place in the current order.
 *
 * Paint is cheap: each cell's text width is measured when the cell changes (the cached layout), and a frame draws a
 * tile a row and a label a cell with no allocation. Text wider than its column is not clipped.
 */
UCLASS()
class UMG_API UTableView : public UWidget
{
	GENERATED_BODY()

public:
	UTableView(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Columns

	/** Adds a column after the others (UE: SHeaderRow::AddColumn). */
	void AddColumn(const FTableViewColumn& Column);
	/** Removes every column and every row (UE: ClearColumns). */
	void ClearColumns();
	[[nodiscard]] int32 GetNumColumns() const
	{
		return Columns.Num();
	}
	[[nodiscard]] const FTableViewColumn& GetColumn(int32 Column) const
	{
		return Columns[Column];
	}
	/** The column with ColumnId, or INDEX_NONE. */
	[[nodiscard]] int32 FindColumn(FName ColumnId) const;

	// Rows

	/** Adds an empty row; returns its index in the current order (the rows are sorted again when a sort is set). */
	int32 AddRow();
	/** Sets a cell's text (the row index of the current order). */
	void SetCellText(int32 Row, int32 Column, const FString& Text);
	[[nodiscard]] const FString& GetCellText(int32 Row, int32 Column) const;
	/** The colour of a row's text. */
	void SetRowColor(int32 Row, const FLinearColor& Color);
	[[nodiscard]] const FLinearColor& GetRowColor(int32 Row) const;
	/** Removes every row, keeping their memory. */
	void ClearRows();
	[[nodiscard]] int32 GetNumRows() const
	{
		return Rows.Num();
	}

	// Sorting and highlight

	/**
	 * Sorts the rows by a column's cells (numbers as numbers, text by characters, ties in the order added) now and on
	 * every SortRows (UE: the header row's sort mode).
	 */
	void SetSortMode(FName ColumnId, EColumnSortMode InSortMode);
	/** Sorts the rows again after they were filled or changed (Leon: UE's list refresh). */
	void SortRows();
	[[nodiscard]] FName GetSortColumn() const
	{
		return SortColumnId;
	}
	[[nodiscard]] EColumnSortMode GetSortMode() const
	{
		return SortMode;
	}
	/** Highlights a row (INDEX_NONE: none); the highlight stays on that row when the rows are sorted. */
	void SetHighlightedRow(int32 Row);
	/** The highlighted row's index in the current order, or INDEX_NONE. */
	[[nodiscard]] int32 GetHighlightedRow() const;

	// Style

	/** The cells' and the header's font (the engine's small font by default). */
	void SetFont(const FSlateFontInfo& InFont);
	[[nodiscard]] const FSlateFontInfo& GetFont() const
	{
		return Font;
	}
	/** Shows the header row (UE: the header row's visibility). */
	void SetShowHeader(bool bInShowHeader)
	{
		bShowHeader = bInShowHeader;
	}
	/** The header's text and background, the rows' two alternating backgrounds and the highlighted row's. */
	FLinearColor HeaderTextColor = FLinearColor(0.85f, 0.85f, 0.85f);
	FLinearColor HeaderBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);
	FLinearColor RowBackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.45f);
	FLinearColor AlternateRowBackgroundColor = FLinearColor(0.06f, 0.06f, 0.06f, 0.45f);
	FLinearColor HighlightBackgroundColor = FLinearColor(0.35f, 0.30f, 0.10f, 0.6f);
	/** The space around a cell's text: Left and Right inside the column, Top and Bottom inside the row. */
	FMargin CellPadding = FMargin(4.0f, 1.0f);

	// Layout (what the last paint used)

	/** A row's height: the font's line plus the cell padding. */
	[[nodiscard]] float GetRowHeight() const;
	/** A column's left edge and width across Width pixels. */
	void GetColumnLayout(float Width, int32 Column, float& OutX, float& OutWidth) const;
	/** Where a cell's text starts, from the table's top-left corner, when the table is Width wide. */
	[[nodiscard]] FVector2D GetCellTextPosition(float Width, int32 Row, int32 Column) const;

protected:
	FVector2D ComputeDesiredSize() const override;
	void OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const override;

private:
	struct FRow
	{
		TArray<FString> Cells;
		/** Each cell's text width in the font (the cached layout). */
		TArray<float> CellWidths;
		FLinearColor Color = FLinearColor::White;
		/** The order the row was added: sorting's tie break. */
		int32 Serial = 0;
		bool bHighlighted = false;
	};

	/** Measures a cell's text (its width, for its alignment). */
	void MeasureCell(FRow& Row, int32 Column) const;
	/** The text's left edge in a column of Width at X, aligned as Align. */
	[[nodiscard]] float AlignText(float X, float Width, float TextWidth, EHorizontalAlignment Align) const;

	UPROPERTY()
	FSlateFontInfo Font;
	TArray<FTableViewColumn> Columns;
	/** Each column's header text width. */
	TArray<float> HeaderWidths;
	TArray<FRow> Rows;
	FName SortColumnId;
	EColumnSortMode SortMode = EColumnSortMode::None;
	int32 NextSerial = 0;
	bool bShowHeader = true;
};
