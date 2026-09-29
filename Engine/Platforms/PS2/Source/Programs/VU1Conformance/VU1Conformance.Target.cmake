# Runs fixed vertex batches through VU1's microprograms without their XGKICK, reads the GIF packets back from VU1's
# memory and compares them with what the C++ emitter linked in the same ELF sends, within plan D2's tolerances; the EE
# log ends with "VU1Conformance: PASSED" or "VU1Conformance: FAILED". Then it draws the batches on the screen, VU1's on
# the left and the emitter's on the right. RunPCSX2.ps1 -Program VU1Conformance runs it; it needs no staged config.
leon_target(VU1Conformance TYPE Program
	PLATFORMS PS2
)
