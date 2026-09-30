#pragma once

#include "CoreTypes.h"

/**
 * What a widget did with an input event (UE: FReply): handled stops it there; unhandled passes it on, to the widget's
 * parent and then to the game.
 */
class FReply
{
public:
	/** UE: FReply::Handled. */
	[[nodiscard]] static FReply Handled()
	{
		return FReply(true);
	}
	/** UE: FReply::Unhandled. */
	[[nodiscard]] static FReply Unhandled()
	{
		return FReply(false);
	}
	/** UE: IsEventHandled. */
	[[nodiscard]] bool IsEventHandled() const
	{
		return bHandled;
	}

private:
	explicit FReply(bool bInHandled)
		: bHandled(bInHandled)
	{
	}

	bool bHandled = false;
};
