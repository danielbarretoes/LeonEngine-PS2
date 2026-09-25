#include "DynamicRHI.h"
#include "Windows/WindowsHWrapper.h"

#include <dxgi1_4.h>

bool GetOpenGLPlatformGPUMemoryStats(FRHIGPUMemoryStats& OutStats)
{
	IDXGIFactory4* Factory = nullptr;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&Factory))) || Factory == nullptr)
	{
		return false;
	}

	IDXGIAdapter1* Adapter1 = nullptr;
	if (FAILED(Factory->EnumAdapters1(0, &Adapter1)) || Adapter1 == nullptr)
	{
		Factory->Release();
		return false;
	}

	IDXGIAdapter3* Adapter3 = nullptr;
	const HRESULT QueryResult = Adapter1->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&Adapter3));
	Adapter1->Release();
	Factory->Release();
	if (FAILED(QueryResult) || Adapter3 == nullptr)
	{
		return false;
	}

	DXGI_QUERY_VIDEO_MEMORY_INFO Info{};
	const HRESULT MemoryResult = Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &Info);
	Adapter3->Release();
	if (FAILED(MemoryResult))
	{
		return false;
	}

	OutStats.bValid = true;
	OutStats.bReportsUsage = true;
	OutStats.UsedBytes = static_cast<uint64>(Info.CurrentUsage);
	OutStats.BudgetBytes = static_cast<uint64>(Info.Budget);
	return true;
}
