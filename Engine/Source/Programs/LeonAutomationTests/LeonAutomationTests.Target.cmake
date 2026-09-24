# Runs the automation tests of every module in its closure (<Module>/Private/Tests/**), with Catch2.
# UE: automation tests (IMPLEMENT_SIMPLE_AUTOMATION_TEST) run by the session frontend / -ExecCmds.
leon_target(LeonAutomationTests TYPE Program
	PLATFORMS Desktop
	COLLECT_AUTOMATION_TESTS
)
