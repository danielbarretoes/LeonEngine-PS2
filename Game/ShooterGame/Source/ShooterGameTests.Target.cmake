# ShooterGame's automation tests (Source/ShooterGame/Private/Tests): the engine's test runner (LeonAutomationTests's
# main) linked with the game module, collecting that module's tests only (the engine's run in LeonAutomationTests).
# Game/ShooterGame/Binaries/Win64/ShooterGameTests.exe [-automation=<filter>]; RunTests.bat builds and runs it.
leon_target(ShooterGameTests TYPE Program
	PLATFORMS Win64
	LAUNCH_MODULE LeonAutomationTests
	# The Renderer gives the test worlds their scene, as in LeonAutomationTests.
	EXTRA_MODULE_NAMES ShooterGame Renderer
	COLLECT_AUTOMATION_TESTS
	AUTOMATION_TEST_MODULES ShooterGame
)
