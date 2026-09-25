#pragma once

// Console command handlers that register themselves (UE: Misc/CoreMisc.h; only the exec part).

#include "CoreTypes.h"
#include "Misc/Exec.h"

class FOutputDevice;
class UWorld;

/**
 * An FExec that adds itself to a global list while it exists; StaticExec offers a command to each of them (UE:
 * FSelfRegisteringExec). Subsystems without a UObject owner use it to publish console commands.
 */
class CORE_API FSelfRegisteringExec : public FExec
{
public:
	FSelfRegisteringExec();
	virtual ~FSelfRegisteringExec() override;

	FSelfRegisteringExec(const FSelfRegisteringExec&) = delete;
	FSelfRegisteringExec& operator=(const FSelfRegisteringExec&) = delete;

	/** Offers Cmd to every registered handler, newest last; true when one of them handled it (UE). */
	static bool StaticExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);
};

/** Registers a plain function as a self-registering exec handler (UE: FStaticSelfRegisteringExec). */
class CORE_API FStaticSelfRegisteringExec : public FSelfRegisteringExec
{
public:
	explicit FStaticSelfRegisteringExec(bool (*InStaticExecFunc)(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar));

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

private:
	bool (*StaticExecFunc)(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);
};
