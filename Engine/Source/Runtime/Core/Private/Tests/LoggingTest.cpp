#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/OutputDeviceHelper.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogCoreTest, Log, All);

namespace
{
	struct FRecordingDevice : public FOutputDevice
	{
		int32 NumMessages = 0;
		ELogVerbosity::Type LastVerbosity = ELogVerbosity::NoLogging;
		FString LastMessage;
		FName LastCategory;

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			++NumMessages;
			LastVerbosity = Verbosity;
			LastMessage = V;
			LastCategory = Category;
		}
	};

	struct FListener : public TSharedFromThis<FListener>
	{
		int32 Sum = 0;

		void OnValue(int32 Value)
		{
			Sum += Value;
		}

		void OnValueWithPayload(int32 Value, int32 Scale)
		{
			Sum += Value * Scale;
		}

		int32 Twice(int32 Value) const
		{
			return Value * 2;
		}
	};

	int32 StaticTriple(int32 Value)
	{
		return Value * 3;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoggingTest, "System.Core.Logging.UE_LOG",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLoggingTest::RunTest(const FString& Parameters)
{
	FRecordingDevice Device;
	GLog->AddOutputDevice(&Device);

	UE_LOG(LogCoreTest, Display, TEXT("value %d"), 7);
	TestEqual("Message", Device.LastMessage, TEXT("value 7"));
	TestEqual("Category", Device.LastCategory, FName("LogCoreTest"));
	TestTrue("Verbosity", Device.LastVerbosity == ELogVerbosity::Display);

	const int32 Before = Device.NumMessages;
	UE_LOG(LogCoreTest, Verbose, TEXT("filtered at run time"));
	TestEqual("Verbose is suppressed by default", Device.NumMessages, Before);

	LogCoreTest.SetVerbosity(ELogVerbosity::VeryVerbose);
	UE_LOG(LogCoreTest, Verbose, TEXT("now visible"));
	TestEqual("SetVerbosity", Device.NumMessages, Before + 1);
	LogCoreTest.ResetToDefault();

	UE_CLOG(false, LogCoreTest, Display, TEXT("never"));
	TestEqual("UE_CLOG", Device.NumMessages, Before + 1);

	GLog->RemoveOutputDevice(&Device);

	TestNotNull("Categories register", FLogCategoryBase::FindCategory(FName("LogCoreTest")));
	TestTrue("ParseLogVerbosityFromString", ParseLogVerbosityFromString("warning") == ELogVerbosity::Warning);
	TestEqual("FormatLogLine", FOutputDeviceHelper::FormatLogLine(ELogVerbosity::Warning, FName("LogX"), TEXT("m")),
		TEXT("LogX: Warning: m"));
	TestEqual("FormatLogLine omits Log",
		FOutputDeviceHelper::FormatLogLine(ELogVerbosity::Log, FName("LogX"), TEXT("m")), TEXT("LogX: m"));

	// An error logged during a test fails it unless expected.
	AddExpectedError(TEXT("expected failure"), 1);
	UE_LOG(LogCoreTest, Error, TEXT("an expected failure"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDelegateTest, "System.Core.Delegates.Delegate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDelegateTest::RunTest(const FString& Parameters)
{
	DECLARE_DELEGATE_RetVal_OneParam(int32, FIntFunc, int32);

	FIntFunc Lambda = FIntFunc::CreateLambda([](int32 Value) { return Value * 2; });
	TestEqual("Lambda", Lambda.Execute(21), 42);

	FIntFunc Static = FIntFunc::CreateStatic(&StaticTriple);
	TestEqual("Static", Static.Execute(5), 15);

	const FListener ConstListener;
	FIntFunc Raw = FIntFunc::CreateRaw(&ConstListener, &FListener::Twice);
	TestEqual("Raw const member", Raw.Execute(4), 8);
	TestTrue("IsBoundToObject", Raw.IsBoundToObject(&ConstListener));

	FIntFunc Copy = Raw;
	TestTrue("Copies keep the handle", Copy.GetHandle() == Raw.GetHandle());
	Copy.Unbind();
	TestFalse("Unbind", Copy.IsBound());

	DECLARE_DELEGATE_OneParam(FVoidFunc, int32);
	FVoidFunc Unbound;
	TestFalse("ExecuteIfBound on unbound", Unbound.ExecuteIfBound(1));

	TSharedPtr<FListener> Shared = MakeShared<FListener>();
	FVoidFunc SP = FVoidFunc::CreateSP(Shared, &FListener::OnValue);
	TestTrue("ExecuteIfBound on SP", SP.ExecuteIfBound(3));
	TestEqual("SP call", Shared->Sum, 3);
	Shared.Reset();
	TestFalse("SP binding expires with the object", SP.IsBound());

	FVoidFunc Payload = FVoidFunc::CreateLambda(
		[](int32 Value, int32 Extra, FString Tag)
		{
			(void)Value;
			(void)Extra;
			(void)Tag;
		},
		1, FString("tag"));
	TestTrue("Payload binding", Payload.ExecuteIfBound(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMulticastDelegateTest, "System.Core.Delegates.Multicast",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMulticastDelegateTest::RunTest(const FString& Parameters)
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnValue, int32);
	FOnValue OnValue;
	TestFalse("Empty", OnValue.IsBound());

	TSharedRef<FListener> Listener = MakeShared<FListener>();
	FListener Raw;
	OnValue.AddSP(Listener, &FListener::OnValue);
	const FDelegateHandle RawHandle = OnValue.AddRaw(&Raw, &FListener::OnValueWithPayload, 10);
	TArray<int32> Order;
	OnValue.AddLambda([&Order](int32) { Order.Add(3); });
	OnValue.AddLambda([&Order](int32) { Order.Add(4); });

	OnValue.Broadcast(3);
	TestEqual("SP", Listener->Sum, 3);
	TestEqual("Raw with payload", Raw.Sum, 30);
	TestTrue("Broadcast order is latest first (UE4)", Order.Num() == 2 && Order[0] == 4 && Order[1] == 3);

	TestTrue("Remove", OnValue.Remove(RawHandle));
	OnValue.Broadcast(1);
	TestEqual("Removed binding not called", Raw.Sum, 30);
	TestEqual("Remaining binding called", Listener->Sum, 4);

	// Removing during a broadcast is safe.
	FOnValue SelfRemoving;
	FDelegateHandle SelfHandle;
	int32 Calls = 0;
	SelfHandle = SelfRemoving.AddLambda(
		[&](int32)
		{
			++Calls;
			SelfRemoving.Remove(SelfHandle);
		});
	SelfRemoving.Broadcast(0);
	SelfRemoving.Broadcast(0);
	TestEqual("Self-removal", Calls, 1);

	OnValue.AddRaw(&Raw, &FListener::OnValue);
	TestEqual("RemoveAll(object)", OnValue.RemoveAll(&Raw), 1);
	OnValue.Clear();
	TestFalse("Clear", OnValue.IsBound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTickerTest, "System.Core.Containers.Ticker",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTickerTest::RunTest(const FString& Parameters)
{
	FTicker Ticker;
	int32 Ticks = 0;
	Ticker.AddTicker(TEXT("Counter"), 0.0f, [&Ticks](float) { return ++Ticks < 3; });
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		Ticker.Tick(0.016f);
	}
	TestEqual("Unregisters after returning false", Ticks, 3);

	int32 Delayed = 0;
	Ticker.AddTicker(FTickerDelegate::CreateLambda(
						 [&Delayed](float)
						 {
							 ++Delayed;
							 return true;
						 }),
		0.1f);
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		Ticker.Tick(0.025f);
	}
	TestEqual("Delay", Delayed, 2);

	int32 RemovedCalls = 0;
	FDelegateHandle Handle;
	Handle = Ticker.AddTicker(FTickerDelegate::CreateLambda(
		[&](float)
		{
			++RemovedCalls;
			Ticker.RemoveTicker(Handle);
			return true;
		}));
	Ticker.Tick(0.016f);
	Ticker.Tick(0.016f);
	TestEqual("RemoveTicker during Tick", RemovedCalls, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
