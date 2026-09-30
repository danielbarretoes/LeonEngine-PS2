#include "Components/TableView.h"

#include "Engine/Font.h"

namespace
{

	/** Text as a number when the whole of it is one (a sign, digits, one point); false otherwise. */
	bool ParseNumber(const FString& Text, float& OutValue)
	{
		const TCHAR* Cursor = *Text;
		while (*Cursor == ' ')
		{
			++Cursor;
		}
		const TCHAR* Start = Cursor;
		if (*Cursor == '-' || *Cursor == '+')
		{
			++Cursor;
		}
		bool bDigits = false;
		bool bPoint = false;
		for (; *Cursor != '\0'; ++Cursor)
		{
			if (*Cursor >= '0' && *Cursor <= '9')
			{
				bDigits = true;
			}
			else if (*Cursor == '.' && !bPoint)
			{
				bPoint = true;
			}
			else
			{
				break;
			}
		}
		while (*Cursor == ' ')
		{
			++Cursor;
		}
		if (!bDigits || *Cursor != '\0')
		{
			return false;
		}
		OutValue = FCString::Atof(Start);
		return true;
	}

	/** -1, 0 or 1: numbers by value, else the text case-insensitively, then exactly. */
	int32 CompareCells(const FString& A, const FString& B)
	{
		float NumberA = 0.0f;
		float NumberB = 0.0f;
		if (ParseNumber(A, NumberA) && ParseNumber(B, NumberB))
		{
			return NumberA < NumberB ? -1 : NumberA > NumberB ? 1 : 0;
		}
		const int32 Folded = FCString::Stricmp(*A, *B);
		if (Folded != 0)
		{
			return Folded < 0 ? -1 : 1;
		}
		const int32 Exact = FCString::Strcmp(*A, *B);
		return Exact < 0 ? -1 : Exact > 0 ? 1 : 0;
	}

	const FString& EmptyCell()
	{
		static const FString Empty;
		return Empty;
	}

} // namespace

UTableView::UTableView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTableView::AddColumn(const FTableViewColumn& Column)
{
	Columns.Add(Column);
	float Width = 0.0f;
	float Height = 0.0f;
	FPaintContext::MeasureText(Font, Column.DefaultText.ToString(), Width, Height);
	HeaderWidths.Add(Width);
	for (FRow& Row : Rows)
	{
		Row.Cells.AddDefaulted();
		Row.CellWidths.Add(0.0f);
	}
}

void UTableView::ClearColumns()
{
	Columns.Reset();
	HeaderWidths.Reset();
	Rows.Reset();
}

int32 UTableView::FindColumn(FName ColumnId) const
{
	for (int32 Index = 0; Index < Columns.Num(); ++Index)
	{
		if (Columns[Index].ColumnId == ColumnId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UTableView::AddRow()
{
	FRow& Row = Rows.AddDefaulted_GetRef();
	Row.Cells.SetNum(Columns.Num());
	Row.CellWidths.SetNumZeroed(Columns.Num());
	Row.Serial = NextSerial++;
	return Rows.Num() - 1;
}

void UTableView::MeasureCell(FRow& Row, int32 Column) const
{
	float Height = 0.0f;
	FPaintContext::MeasureText(Font, Row.Cells[Column], Row.CellWidths[Column], Height);
}

void UTableView::SetCellText(int32 Row, int32 Column, const FString& Text)
{
	if (!Rows.IsValidIndex(Row) || !Columns.IsValidIndex(Column))
	{
		return;
	}
	FRow& Changed = Rows[Row];
	if (Changed.Cells[Column] != Text)
	{
		Changed.Cells[Column] = Text;
		MeasureCell(Changed, Column);
	}
}

const FString& UTableView::GetCellText(int32 Row, int32 Column) const
{
	return Rows.IsValidIndex(Row) && Columns.IsValidIndex(Column) ? Rows[Row].Cells[Column] : EmptyCell();
}

void UTableView::SetRowColor(int32 Row, const FLinearColor& Color)
{
	if (Rows.IsValidIndex(Row))
	{
		Rows[Row].Color = Color;
	}
}

const FLinearColor& UTableView::GetRowColor(int32 Row) const
{
	return Rows.IsValidIndex(Row) ? Rows[Row].Color : FLinearColor::White;
}

void UTableView::ClearRows()
{
	Rows.Reset();
	NextSerial = 0;
}

void UTableView::SetSortMode(FName ColumnId, EColumnSortMode InSortMode)
{
	SortColumnId = ColumnId;
	SortMode = InSortMode;
	SortRows();
}

void UTableView::SortRows()
{
	const int32 Column = FindColumn(SortColumnId);
	if (Column == INDEX_NONE || SortMode == EColumnSortMode::None)
	{
		// Unsorted: the order the rows were added.
		Rows.Sort([](const FRow& A, const FRow& B) { return A.Serial < B.Serial; });
		return;
	}
	const bool bDescending = SortMode == EColumnSortMode::Descending;
	Rows.Sort(
		[Column, bDescending](const FRow& A, const FRow& B)
		{
			const int32 Order = CompareCells(A.Cells[Column], B.Cells[Column]);
			if (Order != 0)
			{
				return bDescending ? Order > 0 : Order < 0;
			}
			return A.Serial < B.Serial;
		});
}

void UTableView::SetHighlightedRow(int32 Row)
{
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		Rows[Index].bHighlighted = Index == Row;
	}
}

int32 UTableView::GetHighlightedRow() const
{
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (Rows[Index].bHighlighted)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UTableView::SetFont(const FSlateFontInfo& InFont)
{
	if (InFont == Font)
	{
		return;
	}
	Font = InFont;
	float Height = 0.0f;
	for (int32 Column = 0; Column < Columns.Num(); ++Column)
	{
		FPaintContext::MeasureText(Font, Columns[Column].DefaultText.ToString(), HeaderWidths[Column], Height);
	}
	for (FRow& Row : Rows)
	{
		for (int32 Column = 0; Column < Columns.Num(); ++Column)
		{
			MeasureCell(Row, Column);
		}
	}
}

float UTableView::GetRowHeight() const
{
	const UFont* RowFont = Font.GetFont();
	const float Line = RowFont != nullptr ? RowFont->GetLineHeight() : 0.0f;
	return Line + CellPadding.Top + CellPadding.Bottom;
}

void UTableView::GetColumnLayout(float Width, int32 Column, float& OutX, float& OutWidth) const
{
	float FixedWidth = 0.0f;
	float FillShares = 0.0f;
	for (const FTableViewColumn& Each : Columns)
	{
		if (Each.ManualWidth > 0.0f)
		{
			FixedWidth += Each.ManualWidth;
		}
		else
		{
			FillShares += FMath::Max(0.0f, Each.FillWidth);
		}
	}
	const float FillWidth = FMath::Max(0.0f, Width - FixedWidth);
	OutX = 0.0f;
	OutWidth = 0.0f;
	for (int32 Index = 0; Index <= Column && Index < Columns.Num(); ++Index)
	{
		OutX += OutWidth;
		const FTableViewColumn& Each = Columns[Index];
		OutWidth = Each.ManualWidth > 0.0f
			? Each.ManualWidth
			: (FillShares > 0.0f ? FillWidth * FMath::Max(0.0f, Each.FillWidth) / FillShares : 0.0f);
	}
}

float UTableView::AlignText(float X, float Width, float TextWidth, EHorizontalAlignment Align) const
{
	const float Inner = Width - CellPadding.Left - CellPadding.Right;
	switch (Align)
	{
		case HAlign_Center:
			return X + CellPadding.Left + ((Inner - TextWidth) * 0.5f);
		case HAlign_Right:
			return X + CellPadding.Left + Inner - TextWidth;
		default:
			return X + CellPadding.Left;
	}
}

FVector2D UTableView::GetCellTextPosition(float Width, int32 Row, int32 Column) const
{
	float ColumnX = 0.0f;
	float ColumnWidth = 0.0f;
	GetColumnLayout(Width, Column, ColumnX, ColumnWidth);
	const float TextWidth =
		Rows.IsValidIndex(Row) && Columns.IsValidIndex(Column) ? Rows[Row].CellWidths[Column] : 0.0f;
	const float X = AlignText(
		ColumnX, ColumnWidth, TextWidth, Columns.IsValidIndex(Column) ? Columns[Column].HAlignCell : HAlign_Left);
	const float Y = (float(Row + (bShowHeader ? 1 : 0)) * GetRowHeight()) + CellPadding.Top;
	return FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y));
}

FVector2D UTableView::ComputeDesiredSize() const
{
	float Width = 0.0f;
	for (int32 Column = 0; Column < Columns.Num(); ++Column)
	{
		if (Columns[Column].ManualWidth > 0.0f)
		{
			Width += Columns[Column].ManualWidth;
			continue;
		}
		float Widest = HeaderWidths[Column];
		for (const FRow& Row : Rows)
		{
			Widest = FMath::Max(Widest, Row.CellWidths[Column]);
		}
		Width += Widest + CellPadding.Left + CellPadding.Right;
	}
	return FVector2D(Width, float(Rows.Num() + (bShowHeader ? 1 : 0)) * GetRowHeight());
}

void UTableView::OnPaint(FPaintContext& Ctx, const FVector2D& Position, const FVector2D& Size) const
{
	const float RowHeight = GetRowHeight();
	// The columns' left edges and widths once (a scoreboard's handful fit inline).
	TArray<float, TInlineAllocator<16>> ColumnX;
	TArray<float, TInlineAllocator<16>> ColumnWidth;
	for (int32 Column = 0; Column < Columns.Num(); ++Column)
	{
		float X = 0.0f;
		float Width = 0.0f;
		GetColumnLayout(Size.X, Column, X, Width);
		ColumnX.Add(Position.X + X);
		ColumnWidth.Add(Width);
	}
	float Y = Position.Y;
	if (bShowHeader)
	{
		Ctx.DrawRect(Position.X, Y, Size.X, RowHeight, HeaderBackgroundColor);
		for (int32 Column = 0; Column < Columns.Num(); ++Column)
		{
			const float TextX =
				AlignText(ColumnX[Column], ColumnWidth[Column], HeaderWidths[Column], Columns[Column].HAlignHeader);
			Ctx.DrawText(Font, Columns[Column].DefaultText.ToString(), FMath::RoundToFloat(TextX),
				FMath::RoundToFloat(Y + CellPadding.Top), HeaderTextColor);
		}
		Y += RowHeight;
	}
	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		const FRow& Row = Rows[RowIndex];
		const FLinearColor& Background = Row.bHighlighted ? HighlightBackgroundColor
			: (RowIndex % 2) == 0                         ? RowBackgroundColor
														  : AlternateRowBackgroundColor;
		if (Background.A > 0.0f)
		{
			Ctx.DrawRect(Position.X, Y, Size.X, RowHeight, Background);
		}
		for (int32 Column = 0; Column < Columns.Num(); ++Column)
		{
			if (Row.Cells[Column].IsEmpty())
			{
				continue;
			}
			const float TextX =
				AlignText(ColumnX[Column], ColumnWidth[Column], Row.CellWidths[Column], Columns[Column].HAlignCell);
			Ctx.DrawText(Font, Row.Cells[Column], FMath::RoundToFloat(TextX), FMath::RoundToFloat(Y + CellPadding.Top),
				Row.Color);
		}
		Y += RowHeight;
	}
}
