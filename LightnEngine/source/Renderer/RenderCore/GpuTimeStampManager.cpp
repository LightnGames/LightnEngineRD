#include "GpuTimeStampManager.h"
#include <Core/Memory.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/CommandListPool.h>

namespace ltn {
namespace {
GpuTimeStampManager g_gpuTimeStampManager;
GpuTimerManager g_gpuTimerManager;
}

GpuTimeStampManager* ltn::GpuTimeStampManager::Get() {
    return &g_gpuTimeStampManager;
}

void GpuTimeStampManager::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();
	{
		rhi::CommandQueueDesc commandQueueDesc = {};
		commandQueueDesc._device = device;
		commandQueueDesc._type = rhi::COMMAND_LIST_TYPE_DIRECT;
		_commandQueue.initialize(commandQueueDesc);
	}

	{
		CommandListPool::Desc commandListPoolDesc = {};
		commandListPoolDesc._device = device;
		commandListPoolDesc._type = rhi::COMMAND_LIST_TYPE_DIRECT;
		_commandListPool.initialize(commandListPoolDesc);
	}

	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(u64) * TIME_STAMP_CAPACITY;
		desc._initialState = rhi::RESOURCE_STATE_COPY_DEST;
		desc._heapType = rhi::HEAP_TYPE_READBACK;
		desc._device = device;
		_timeStampBuffer.initialize(desc);
		_timeStampBuffer.setName("TimeStamp");
	}

	{
		rhi::QueryHeapDesc desc = {};
		desc._device = device;
		desc._count = TIME_STAMP_CAPACITY;
		desc._type = rhi::QUERY_HEAP_TYPE_TIMESTAMP;
		_queryHeap.initialize(desc);
	}

	// GPU 周波数取得
	u64 frequency = 0;
	_commandQueue.getTimestampFrequency(&frequency);
	_gpuTickDelta = 1.0 / f64(frequency);

	_gpuTimeStamps = Memory::allocObjects<u64>(sizeof(u64) * TIME_STAMP_CAPACITY * rhi::BACK_BUFFER_COUNT);
}

void GpuTimeStampManager::terminate() {
	_timeStampBuffer.terminate();
	_queryHeap.terminate();
	_commandQueue.terminate();
	_commandListPool.terminate();

	Memory::freeObjects(_gpuTimeStamps);
}

void GpuTimeStampManager::update(u32 frameIndex, u32 timerCount) {
	readbackTimeStamps(frameIndex, timerCount);
}

void GpuTimeStampManager::readbackTimeStamps(u32 frameIndex, u32 timeStampCount) {
	_commandQueue.waitForFence(_fenceValue);
	
	u64 timeStampOffset = TIME_STAMP_CAPACITY * frameIndex;
	u64 timeStampOffsetInByte = sizeof(u64) * timeStampOffset;
	u64 timeStampSizeInByte = sizeof(u64) * timeStampCount;

	// INFO: D3D12 ReadBackBuffer は常に Map することは非推奨なので Map/Unmap しています。
	rhi::MemoryRange range(timeStampOffsetInByte, timeStampSizeInByte);
	u64* mapPtr = _timeStampBuffer.map<u64>(&range);
	memcpy(_gpuTimeStamps, mapPtr, timeStampSizeInByte);
	_timeStampBuffer.unmap();

	// GPU タイムスタンプリードバックコマンド発行
	{
		u64 completedFenceValue = _commandQueue.getCompletedValue();
		rhi::CommandList* commandList = _commandListPool.allocateCommandList(completedFenceValue);
		commandList->reset();

		rhi::Resource* resource = _timeStampBuffer.getResource();
		commandList->resolveQueryData(&_queryHeap, rhi::QUERY_TYPE_TIMESTAMP, 0, timeStampCount, resource, 0);
		_commandQueue.executeCommandLists(1, &commandList);

		_fenceValue = commandList->_fenceValue;
	}
}

void GpuTimeStampManager::writeGpuMarker(rhi::CommandList* commandList, u32 timeStampIndex) {
	commandList->endQuery(&_queryHeap, rhi::QUERY_TYPE_TIMESTAMP, timeStampIndex);
}

f32 GpuTimeStampManager::getCurrentGpuFrameTime() const {
    return f32();
}

void GpuTimerManager::initialize() {
	_gpuTimerAdditionalInfos = Memory::allocObjects<GpuTimerAdditionalInfo>(GPU_TIMER_CAPACITY * rhi::BACK_BUFFER_COUNT);
	GpuTimeStampManager::Get()->initialize();
}

void GpuTimerManager::terminate() {
	Memory::freeObjects(_gpuTimerAdditionalInfos);
	GpuTimeStampManager::Get()->terminate();
}

void GpuTimerManager::update(u32 frameIndex) {
	u32 gpuTimerCount = _gpuTimerCounts[_frameIndex];
	GpuTimeStampManager::Get()->update(frameIndex, gpuTimerCount * 2);
	_frameIndex = frameIndex;
	_gpuTimerCounts[frameIndex] = 0;
}

u32 GpuTimerManager::pushGpuTimer(rhi::CommandList* commandList) {
	u32 timerIndex = _gpuTimerCounts[_frameIndex]++;
	GpuTimeStampManager::Get()->writeGpuMarker(commandList, timerIndex * 2);
	return timerIndex;
}

void GpuTimerManager::popGpuTimer(rhi::CommandList* commandList, u32 timerIndex) {
	GpuTimeStampManager::Get()->writeGpuMarker(commandList, timerIndex * 2 + 1);
}

void GpuTimerManager::writeGpuTimerInfo(u32 timerIndex, const char* format, va_list va, const Color4& color) {
	GpuTimerAdditionalInfo* infos = _gpuTimerAdditionalInfos + _frameIndex * GPU_TIMER_CAPACITY;
	GpuTimerAdditionalInfo& info = infos[timerIndex];
	info._color = color;
	vsprintf_s(info._name, format, va);
}

GpuTimerManager* GpuTimerManager::Get() {
	return &g_gpuTimerManager;
}
}
