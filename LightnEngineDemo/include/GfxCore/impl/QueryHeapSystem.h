#pragma once
#include <Core/System.h>
#include <GfxCore/GfxModuleSettings.h>
#include <GfxCore/impl/GpuResourceImpl.h>

class LTN_GFX_CORE_API QueryHeapSystem {
public:
	static constexpr u32 NEST_COUNT_MAX = 16;
	static constexpr u32 GPU_TIME_STAMP_COUNT_MAX = 64;
	static constexpr u32 TICK_COUNT_MAX = GPU_TIME_STAMP_COUNT_MAX * 2; // begin end
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

	u32 pushGpuMarker(CommandList* commandList, const char* markerName);
	void popGpuMarker(CommandList* commandList, u32 markerIndex);
	u32 pushCpuMarker(const char* markerName);
	//void popCpuMarker(u32 markerIndex);

	void debugDrawTree(u32 tickIndex);

	f32 getCurrentCpuFrameTime() const;
	f32 getCurrentGpuFrameTime() const;

	static QueryHeapSystem* Get();

private:
	struct TickInfo {
		u16 _beginMarkerIndex = 0;
		u16 _endMarkerIndex = 0;
	};

	GpuBuffer _timeStampBuffer;
	QueryHeap* _queryHeap = nullptr;
	u64 _gpuTicks[TICK_COUNT_MAX] = {};
	u64 _cpuTicks[TICK_COUNT_MAX] = {};
	u64 _gpuFrequency = 0;
	u64 _cpuFrequency = 0;
	u32 _currentFrameCpuMarkerCount = 0;
	u32 _currentFrameGpuMarkerCount = 0;
	u32 _currentTickCount = 0;
	u32 _currentNestGpuLevel = 0;
	u32 _currentNestMarkerIndices[NEST_COUNT_MAX] = {};
	u32 _nestGpuParentIndices[GPU_TIME_STAMP_COUNT_MAX] = {};
	u32 _nestGpuIndices[GPU_TIME_STAMP_COUNT_MAX] = {};
	TickInfo _gpuTickInfos[GPU_TIME_STAMP_COUNT_MAX] = {};
	char _debugGpuMarkerNames[GPU_TIME_STAMP_COUNT_MAX][DEBUG_MARKER_NAME_COUNT_MAX] = {};
	char _debugCpuMarkerNames[GPU_TIME_STAMP_COUNT_MAX][DEBUG_MARKER_NAME_COUNT_MAX] = {};
};

class LTN_GFX_CORE_API GpuScopedEvent {
public:
	GpuScopedEvent(CommandList* commandList, const Color4& color, const char* name, ...) {
		_commandList = commandList;
		va_list va;
		va_start(va, name);
		vsprintf_s(_name, name, va);
		_gpuMarker.setEvent(commandList, color, name, va);
		va_end(va);

		QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		_markerIndex = queryHeapSystem->pushGpuMarker(_commandList, _name);
	}
	~GpuScopedEvent() {
		QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		queryHeapSystem->popGpuMarker(_commandList, _markerIndex);
	}
private:
	DebugMarker::ScopedEvent _gpuMarker;
	u32 _markerIndex = 0;
	CommandList* _commandList = nullptr;
	char _name[DebugMarker::SET_NAME_LENGTH_COUNT_MAX] = {};
};

class LTN_GFX_CORE_API CpuGpuScopedEvent {
public:
	CpuGpuScopedEvent(CommandList* commandList, const Color4& color, const char* name, ...) {
		_commandList = commandList;
		va_list va;
		va_start(va, name);
		vsprintf_s(_name, name, va);
		_gpuMarker.setEvent(commandList, color, name, va);
		va_end(va);

		QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		_tickIndex = queryHeapSystem->pushGpuMarker(_commandList, _name);
		queryHeapSystem->pushCpuMarker(_name);
	}
	~CpuGpuScopedEvent() {
		QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		queryHeapSystem->popGpuMarker(_commandList, _tickIndex);
	}
private:
	DebugMarker::ScopedEvent _gpuMarker;
	CommandList* _commandList = nullptr;
	u32 _tickIndex = 0;
	char _name[DebugMarker::SET_NAME_LENGTH_COUNT_MAX] = {};
};

#define DEBUG_MARKER_GPU_SCOPED_EVENT(...) GpuScopedEvent __DEBUG_SCOPED_EVENT__(__VA_ARGS__)
#define DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(...) CpuGpuScopedEvent __DEBUG_SCOPED_EVENT__(__VA_ARGS__)