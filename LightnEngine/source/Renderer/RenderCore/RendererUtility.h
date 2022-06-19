#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RHI/RhiMarker.h>
#include <Renderer/RenderCore/GpuResource.h>
#include <Renderer/RenderCore/GpuTimeStampManager.h>
namespace ltn {
struct ScopedBarrierDesc {
	ScopedBarrierDesc(GpuResource* resource, rhi::ResourceStates stateAfter)
		:_resources(resource), _stateAfter(stateAfter) {
	}
	GpuResource* _resources = nullptr;
	rhi::ResourceStates _stateAfter;
};

// 定義された行でリソースバリアを発行し、破棄時にリソースステートを戻すクラス
class ScopedBarrier {
public:
	static constexpr u32 SCOPED_BARRIER_COUNT_MAX = 8;

	ScopedBarrier(rhi::CommandList* commandList, ScopedBarrierDesc* barrierDescs, u32 barrierCount) {
		LTN_ASSERT(barrierCount < SCOPED_BARRIER_COUNT_MAX);
		_commandList = commandList;
		_barrierCount = barrierCount;
		_barrierDescs = barrierDescs;

		for (u32 i = 0; i < barrierCount; ++i) {
			_beforeResourceStates[i] = barrierDescs[i]._resources->getResourceState();
		}

		rhi::ResourceTransitionBarrier barriers[SCOPED_BARRIER_COUNT_MAX];
		for (u32 i = 0; i < barrierCount; ++i) {
			ScopedBarrierDesc& desc = barrierDescs[i];
			barriers[i] = desc._resources->getTransitionBarrier(desc._stateAfter);
		}

		for (u32 i = 0; i < barrierCount; ++i) {
			ScopedBarrierDesc& desc = barrierDescs[i];
			desc._resources->setResourceState(desc._stateAfter);
		}

		commandList->transitionBarriers(barriers, barrierCount);
	}

	~ScopedBarrier() {
		rhi::ResourceTransitionBarrier barriers[SCOPED_BARRIER_COUNT_MAX];
		for (u32 i = 0; i < _barrierCount; ++i) {
			barriers[i] = _barrierDescs[i]._resources->getTransitionBarrier(_beforeResourceStates[i]);
		}

		for (u32 i = 0; i < _barrierCount; ++i) {
			_barrierDescs[i]._resources->setResourceState(_beforeResourceStates[i]);
		}

		_commandList->transitionBarriers(barriers, _barrierCount);
	}

private:
	rhi::ResourceStates _beforeResourceStates[SCOPED_BARRIER_COUNT_MAX] = {};
	rhi::CommandList* _commandList = nullptr;
	ScopedBarrierDesc* _barrierDescs = nullptr;
	u32 _barrierCount = 0;
};

class GpuScopedTimer {
public:
	GpuScopedTimer(rhi::CommandList* commandList, const Color4& color, const char* format, ...) {
		_commandList = commandList;
		va_list va;
		va_start(va, format);
		vsprintf_s(_name, format, va);
		//_gpuMarker.setEvent(commandList, color, name, va);
		va_end(va);

		rhi::BeginMarker(commandList, color, _name);
		//QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		//_markerIndex = queryHeapSystem->pushGpuMarker(_commandList, _name);
	}
	~GpuScopedTimer() {
		//QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		//queryHeapSystem->popGpuMarker(_commandList, _markerIndex);
	}
private:
	//u32 _markerIndex = 0;
	rhi::CommandList* _commandList = nullptr;
	char _name[64] = {};
};

class CpuScopedTimer {
public:
	CpuScopedTimer(const char* name, ...) {
		va_list va;
		va_start(va, name);
		vsprintf_s(_name, name, va);
		va_end(va);

		//QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		//_cpuTickIndex = queryHeapSystem->pushCpuMarker(_name);
	}
	~CpuScopedTimer() {
		//QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		//queryHeapSystem->popCpuMarker(_cpuTickIndex);
	}
private:
	//u32 _cpuTickIndex = 0;
	char _name[64] = {};
};

class CpuGpuScopedTimer {
public:
	CpuGpuScopedTimer(rhi::CommandList* commandList, const Color4& color, const char* name, ...) {
		_commandList = commandList;
		va_list va;
		va_start(va, name);
		vsprintf_s(_name, name, va);
		//_gpuMarker.setEvent(commandList, color, name, va);
		va_end(va);

		//QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		//_gpuTickIndex = queryHeapSystem->pushGpuMarker(_commandList, _name);
		//_cpuTickIndex = queryHeapSystem->pushCpuMarker(_name);
		_gpuTimeStampIndex = GpuTimeStampManager::Get()->pushGpuMarker(commandList);
		rhi::BeginMarker(commandList, color, _name);
	}
	~CpuGpuScopedTimer() {
		//QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		//queryHeapSystem->popGpuMarker(_commandList, _gpuTickIndex);
		//queryHeapSystem->popCpuMarker(_cpuTickIndex);
		rhi::EndMarker(_commandList);
	}
private:
	rhi::CommandList* _commandList = nullptr;
	u32 _gpuTimeStampIndex = 0;
	u32 _cpuTickIndex = 0;
	char _name[64] = {};
};

#define DEBUG_MARKER_CPU_SCOPED_TIMER(...) CpuScopedTimer __DEBUG_SCOPED_EVENT__(__VA_ARGS__)
#define DEBUG_MARKER_GPU_SCOPED_TIMER(...) GpuScopedTimer __DEBUG_SCOPED_EVENT__(__VA_ARGS__)
#define DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(...) CpuGpuScopedTimer __DEBUG_SCOPED_EVENT__(__VA_ARGS__)
}