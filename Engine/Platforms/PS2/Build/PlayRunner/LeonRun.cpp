// LeonRun: boots one EE ELF headless on Play!'s HLE BIOS (no console BIOS needed) and prints what the program writes
// to the IOP's stdout (UE_LOG's EE console). host: is the ELF's folder, as PCSX2's host filesystem. Built inside a
// Play! checkout by BuildPlayRunner.sh; Play! is BSD-2-Clause (Jean-Philip Desjardins).
//   leonrun <file.elf> [seconds]    runs the ELF for that long (20 s by default) or until it exits
#include "DefaultAppConfig.h"
#include "PS2VM.h"
#include "PS2VM_Preferences.h"
#include "StdStream.h"
#include "ee/PS2OS.h"
#include "gs/GSH_Null.h"
#include "iop/IopBios.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unistd.h>

int main(int argc, const char** argv)
{
	if (argc < 2)
	{
		std::printf("leonrun <file.elf> [seconds]\n");
		return 2;
	}
	const fs::path Elf = fs::absolute(argv[1]);
	const int Seconds = argc > 2 ? std::atoi(argv[2]) : 20;
	bool bExited = false;
	CPS2VM VirtualMachine;
	CAppConfig::GetInstance().SetPreferencePath(PREF_PS2_HOST_DIRECTORY, Elf.parent_path());
	VirtualMachine.Initialize();
	VirtualMachine.CreateGSHandler(CGSH_Null::GetFactoryFunction());
	auto Connection = VirtualMachine.m_ee->m_os->OnRequestExit.Connect([&bExited]() { bExited = true; });
	VirtualMachine.m_ee->m_os->BootFromFile(Elf);
	auto* IopBios = dynamic_cast<CIopBios*>(VirtualMachine.m_iop->m_bios.get());
	IopBios->GetIoman()->SetFileStream(Iop::CIoman::FID_STDOUT, new Framework::CStdStream(stdout));
	VirtualMachine.Resume();
	for (int Tick = 0; Tick < Seconds * 10 && !bExited; ++Tick)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	VirtualMachine.Pause();
	std::fflush(stdout);
	std::fprintf(stderr, "leonrun: %s after %d s (EE PC %08X)\n", bExited ? "the program exited" : "still running",
		Seconds, VirtualMachine.m_ee->m_EE.m_State.nPC);
	std::fflush(stderr);
	// Play!'s threads do not all stop on Destroy; the result is out.
	_exit(0);
}
