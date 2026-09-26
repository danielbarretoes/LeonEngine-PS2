#include "AudioDevice.h"

// The PS2's FAudioDevice until the SPU2 backend (audsrv, Docs/PLANS/ps2-engine.md E5): always silent, as a desktop
// device opened with bInSilent. The engine treats it like any silent device: the Play* calls do nothing.

struct FAudioDevice::FImpl
{
};

FAudioDevice::FAudioDevice() = default;

FAudioDevice::~FAudioDevice() = default;

bool FAudioDevice::Initialize(bool bInSilent)
{
	(void)bInSilent;
	bSilent = true;
	bInitialized = true;
	return true;
}

void FAudioDevice::Shutdown()
{
	bInitialized = false;
}

void FAudioDevice::Tick()
{
}

void FAudioDevice::SetMasterVolume(float Volume01)
{
	MasterVolume = Volume01;
}

void FAudioDevice::SetListener(const FVector&, const FVector&, const FVector&)
{
}

void FAudioDevice::PlaySound2D(const FSoundWavePCM&, float)
{
}

void FAudioDevice::PlaySoundAtLocation(const FSoundWavePCM&, const FVector&, float)
{
}

void FAudioDevice::SetUiSound(EUISound, const FSoundWavePCM&)
{
}

bool FAudioDevice::HasUiSound(EUISound) const
{
	return false;
}

void FAudioDevice::PlayUiSound(EUISound, float)
{
}

void FAudioDevice::PlayMusic(const FSoundWavePCM&, float)
{
}

void FAudioDevice::StopMusic()
{
}

bool FAudioDevice::IsMusicPlaying() const
{
	return false;
}
