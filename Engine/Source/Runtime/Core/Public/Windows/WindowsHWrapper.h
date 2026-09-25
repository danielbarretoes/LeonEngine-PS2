#pragma once

// Includes <Windows.h> without losing Core's TEXT (Windows.h defines its own). UE: Windows/WindowsHWrapper.h.
#pragma push_macro("TEXT")
#undef TEXT
#include <Windows.h>
#pragma pop_macro("TEXT")
