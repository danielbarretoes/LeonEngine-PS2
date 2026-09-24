# miniaudio 0.11.25 — desktop audio backend (AudioMixer).
leon_module(MiniAudio
	PLATFORMS Desktop
	DOWNLOAD_URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz
	DOWNLOAD_SHA256 b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274
	DOWNLOAD_DIR miniaudio-0.11.25
	EXTERNAL_TARGETS LeonThirdParty_MiniAudio
)

function(LeonExternal_MiniAudio)
	add_library(LeonThirdParty_MiniAudio STATIC "${LEON_THIRDPARTY_DIR}/miniaudio.c")
	target_include_directories(LeonThirdParty_MiniAudio SYSTEM PUBLIC "${LEON_THIRDPARTY_DIR}")
	if(MSVC)
		target_compile_options(LeonThirdParty_MiniAudio PRIVATE /W0)
	else()
		target_compile_options(LeonThirdParty_MiniAudio PRIVATE -w)
	endif()
	if(WIN32)
		target_link_libraries(LeonThirdParty_MiniAudio PUBLIC winmm)
	endif()
endfunction()
