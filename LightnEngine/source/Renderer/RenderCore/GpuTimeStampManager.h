#pragma once
#include <Core/Type.h>
#include <Core/Math.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/CommandListPool.h>

namespace ltn {
class GpuTimerManager {
public:
	static constexpr u32 NEST_COUNT_MAX = 16;
	static constexpr u32 GPU_TIMER_CAPACITY = 128;

	struct GpuTimerAdditionalInfo {
		char _name[64];
		Color4 _color;
	};

	void initialize();
	void terminate();
	void update(u32 frameIndex);

	u32 pushGpuTimer(rhi::CommandList* commandList, const GpuTimerAdditionalInfo& info);
	void popGpuTimer(u32 timerIndex);

private:
	GpuTimerAdditionalInfo* _gpuTimerAdditionalInfos = nullptr;
	u32 _gpuTimerCount[rhi::BACK_BUFFER_COUNT] = {};
	u32 _frameIndex = 0;
};

class GpuTimeStampManager {
public:
	static constexpr u32 TIME_STAMP_CAPACITY = GpuTimerManager::GPU_TIMER_CAPACITY * 2; // begin end

	void initialize();
	void terminate();
	void update(u32 frameIndex, u32 timerCount);

	void readbackTimeStamps(u32 frameIndex, u32 timerCount);
	void writeGpuMarker(rhi::CommandList* commandList, u32 timeStampIndex);

	f32 getCurrentGpuFrameTime() const;

	static GpuTimeStampManager* Get();

private:
	GpuBuffer _timeStampBuffer;
	CommandListPool _commandListPool;
	rhi::QueryHeap _queryHeap;
	rhi::CommandQueue _commandQueue;

	u64 _fenceValue = 0;
	u64* _gpuTimeStamps = nullptr;
	f64 _gpuTickDelta = 0;
};
}