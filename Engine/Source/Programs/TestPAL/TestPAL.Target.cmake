# Runs the automation tests of Core, CoreUObject, Json, Projects and PakFile on every platform, without Catch2 (UE:
# Programs/TestPAL). On the PS2 the result is read from the EE console (PCSX2 log): "TestPAL: PASSED" /
# "TestPAL: FAILED".
leon_target(TestPAL TYPE Program
	COLLECT_AUTOMATION_TESTS
)
