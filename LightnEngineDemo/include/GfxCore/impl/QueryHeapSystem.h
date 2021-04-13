#pragma once
#include <Core/System.h>
#include <GfxCore/GfxModuleSettings.h>
#include <GfxCore/impl/GpuResourceImpl.h>

class LTN_GFX_CORE_API QueryHeapSystem {
public:
	static constexpr u32 GPU_TIME_STAMP_COUNT_MAX = 64;
	static constexpr u32 DEBUG_MARKER_NAME_COUNT_MAX = 128;

	void initialize();
	void terminate();
	void update();
	void requestTimeStamp(CommandList* commandList, u32 frameIndex);
	void setGpuFrequency(CommandQueue* commandQueue);
	void setCpuFrequency();
	void debugDrawTimeStamps();
	void debugDrawCpuPerf();
	void debugDrawGpuPerf();

	void setGpuMarker(CommandList* commandList, const char* markerName = nullptr);
	void setCpuMarker(const char* markerName);

	f32 getCurrentCpuFrameTime() const;
	f32 getCurrentGpuFrameTime() const;

	static QueryHeapSystem* Get();

private:
	GpuBuffer _timeStampBuffer;
	QueryHeap* _queryHeap = nullptr;
	u64 _currentGpuTimeStamps[GPU_TIME_STAMP_COUNT_MAX] = {};
	u64 _currentCpuTimeStamps[GPU_TIME_STAMP_COUNT_MAX] = {};
	u64 _gpuFrequency = 0;
	u64 _cpuFrequency = 0;
	u32 _currentFrameCpuMarkerCount = 0;
	u32 _currentFrameGpuMarkerCount = 0;
	char _debugGpuMarkerNames[GPU_TIME_STAMP_COUNT_MAX][DEBUG_MARKER_NAME_COUNT_MAX] = {};
	char _debugCpuMarkerNames[GPU_TIME_STAMP_COUNT_MAX][DEBUG_MARKER_NAME_COUNT_MAX] = {};
};

class LTN_GFX_CORE_API CpuGpuScopedEvent {
public:
	CpuGpuScopedEvent(CommandList* commandList, const Color4& color, const char* name, ...) {
		_commandList = commandList;
		va_list va;
		va_start(va, name);
		_gpuMarker.setEvent(commandList, color, name, va);
		va_end(va);

		vsprintf_s(_name, name, va);
	}
	~CpuGpuScopedEvent() {
		QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		queryHeapSystem->setGpuMarker(_commandList, _name);
		queryHeapSystem->setCpuMarker(_name);
	}
private:
	DebugMarker::ScopedEvent _gpuMarker;
	CommandList* _commandList = nullptr;
	char _name[DebugMarker::SET_NAME_LENGTH_COUNT_MAX] = {};
};

#define DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(...) CpuGpuScopedEvent __DEBUG_SCOPED_EVENT__(__VA_ARGS__)