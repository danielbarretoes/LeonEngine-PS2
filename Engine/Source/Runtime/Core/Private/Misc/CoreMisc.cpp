#include "Misc/CoreMisc.h"

#include "Containers/Array.h"

namespace
{
	/** The live self-registering handlers, in registration order (UE: FSelfRegisteringExec::GetRegisteredExecs). */
	TArray<FSelfRegisteringExec*>& GetRegisteredExecs()
	{
		static TArray<FSelfRegisteringExec*> RegisteredExecs;
		return RegisteredExecs;
	}
} // namespace

FExec::~FExec()
{
}

FSelfRegisteringExec::FSelfRegisteringExec()
{
	GetRegisteredExecs().Add(this);
}

FSelfRegisteringExec::~FSelfRegisteringExec()
{
	GetRegisteredExecs().RemoveSingle(this);
}

bool FSelfRegisteringExec::StaticExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// A copy: a handler may register or unregister others while it runs.
	const TArray<FSelfRegisteringExec*> Execs = GetRegisteredExecs();
	for (FSelfRegisteringExec* Exec : Execs)
	{
		if (Exec->Exec(InWorld, Cmd, Ar))
		{
			return true;
		}
	}
	return false;
}

FStaticSelfRegisteringExec::FStaticSelfRegisteringExec(
	bool (*InStaticExecFunc)(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar))
	: StaticExecFunc(InStaticExecFunc)
{
}

bool FStaticSelfRegisteringExec::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return StaticExecFunc(InWorld, Cmd, Ar);
}
