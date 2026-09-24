# Runs the automation tests of every module in its closure (<Module>/Private/Tests/**), with Catch2.
# UE: automation tests (IMPLEMENT_SIMPLE_AUTOMATION_TEST) run by the session frontend / -ExecCmds.
leon_target(LeonAutomationTests TYPE Program
	PLATFORMS Desktop
	EXTRA_MODULE_NAMES Core Json Projects RenderCore Renderer UMG PhysicsCore AnimationCore AudioMixer NetCore
		Engine AIModule MeshUtilities Cooker
	ENABLE_PLUGINS JoltPhysics
	COLLECT_AUTOMATION_TESTS
)
