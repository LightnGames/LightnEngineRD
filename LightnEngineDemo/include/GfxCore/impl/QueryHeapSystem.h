#pragma once
#include <Core/System.h>
#include <GfxCore/GfxModuleSettings.h>
#include <GfxCore/impl/GpuResourceImpl.h>

class LTN_GFX_CORE_API QueryHeapSystem {
public:
	static constexpr u32 GPU_TIME_STAMP_COUNT_MAX = 32;
	static constexpr u32 DEBUG_MARKER_NAME_COUNT_MAX = 128;

	void initialize();
	void terminate();
	void update();
	void requestTimeStamp(CommandList* commandList, u32 frameIndex);
	void setGpuFrequency(CommandQueue* commandQueue);
	void debugDrawTimeStamps();

	void setMarker(CommandList* commandList);
	void setCurrentMarkerName(const char* markerName);

	static QueryHeapSystem* Get();

private:
	GpuBuffer _timeStampBuffer;
	QueryHeap* _queryHeap = nullptr;
	u64* _mapedTimeStampPtr = nullptr;
	u64* _currentTimeStamps = nullptr;
	u64 _currentFrameFrequency = 0;
	u32 _currentFrameMarkerCount = 0;
	char _debugMarkerNames[GPU_TIME_STAMP_COUNT_MAX][DEBUG_MARKER_NAME_COUNT_MAX] = {};
};