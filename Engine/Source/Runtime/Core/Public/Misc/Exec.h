#pragma once

#include "CoreTypes.h"

class FOutputDevice;
class UWorld;

/**
 * Something that runs console commands (UE: FExec). Exec returns true when it handled Cmd; output goes to Ar. The
 * console (P13: UGameViewportClient) offers a command to every exec handler until one takes it.
 */
class CORE_API FExec
{
public:
	virtual ~FExec();

	/** Runs Cmd when it is one of this handler's commands; InWorld is the world the command targets (may be null). */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) = 0;
};
