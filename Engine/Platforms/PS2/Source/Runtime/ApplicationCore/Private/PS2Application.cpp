#include "GenericPlatform/GenericApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "PS2InputInterface.h"
#include "PS2Window.h"

/** PS2 application: one GS window and the DualShock on port 0 (UE: F<Platform>Application). */
class FPS2Application final : public GenericApplication
{
public:
	FPS2Application()
	{
		InputInterface.Initialize();
	}

	virtual std::unique_ptr<FGenericWindow> MakeWindow() override
	{
		return std::make_unique<FPS2Window>();
	}

	virtual void PollGameDeviceState() override
	{
		InputInterface.SendControllerEvents();
	}

	virtual IInputInterface* GetInputInterface() override
	{
		return &InputInterface;
	}

private:
	FPS2InputInterface InputInterface;
};

GenericApplication* FPlatformApplicationMisc::CreateApplication()
{
	return new FPS2Application();
}
