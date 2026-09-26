#include "Components/ContentWidget.h"

UContentWidget::UContentWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPanelSlot* UContentWidget::SetContent(UWidget* Content)
{
	ClearChildren();
	return AddChild(Content);
}
