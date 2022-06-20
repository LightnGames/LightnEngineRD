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

	u32 pushGpuTimer(rhi::CommandList* commandList);
	void popGpuTimer(rhi::CommandList* commandList, u32 timerIndex);

	void writeGpuTimerInfo(u32 timerIndex, const char* format, va_list va, const Color4& color);

	u32 getTimerCount() const { return _gpuTimerCounts[_frameIndex]; }
	const GpuTimerAdditionalInfo* getGpuTimerAdditionalInfo(u32 timerIndex) {
		return &_gpuTimerAdditionalInfos[_frameIndex * GPU_TIMER_CAPACITY + timerIndex];
	}

	static GpuTimerManager* Get();

private:
	GpuTimerAdditionalInfo* _gpuTimerAdditionalInfos = nullptr;
	u32 _gpuTimerCounts[rhi::BACK_BUFFER_COUNT] = {};
	u32 _frameIndex = 0;
};

class GpuTimeStampManager {
public:
	static constexpr u32 TIME_STAMP_CAPACITY = GpuTimerManager::GPU_TIMER_CAPACITY * 2; // begin end

	void initialize();
	void terminate();
	void update(u32 frameIndex, u32 timerCount);

	void readbackTimeStamps(u32 frameIndex, u32 timeStampCount);
	void writeGpuMarker(rhi::CommandList* commandList, u32 timeStampIndex);

	f64 getGpuTickDelta() const { return _gpuTickDelta; }
	f64 getGpuTickDeltaInMilliseconds() const { return 1000.0 * _gpuTickDelta; }
	f32 getCurrentGpuFrameTime() const;
	const u64* getGpuTimeStamps() const { return _gpuTimeStamps; }

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