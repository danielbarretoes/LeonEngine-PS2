# PakFile: .lpak files mounted as a platform file in the IPlatformFile chain, and the writer LeonPak uses (Unreal:
# Runtime/PakFile). Every platform: the PS2 builds it for TestPAL (and, with the Engine port, a pak on cdrom0:).
leon_module(PakFile
	PUBLIC_DEPENDENCIES Core
)
