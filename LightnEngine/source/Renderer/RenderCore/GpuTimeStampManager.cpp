#include "GpuTimeStampManager.h"
#include <Core/Memory.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/CommandListPool.h>

namespace ltn {
namespace {
GpuTimeStampManager g_gpuTimeStampManager;
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

	// GPU é¸îgêîéÊìæ
	u64 frequency = 0.0;
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

void GpuTimeStampManager::readbackTimeStamps(u32 frameIndex, u32 timerCount) {
	_commandQueue.waitForFence(_fenceValue);
	
	u32 timeStampCount = timerCount * 2;
	u64 timeStampOffset = TIME_STAMP_CAPACITY * frameIndex;
	u64 timeStampOffsetInByte = sizeof(u64) * timeStampOffset;
	u64 timeStampSizeInByte = sizeof(u64) * timeStampCount;

	// INFO: D3D12 ReadBackBuffer ÇÕèÌÇ… Map Ç∑ÇÈÇ±Ç∆ÇÕîÒêÑèßÇ»ÇÃÇ≈ Map/Unmap ÇµÇƒÇ¢Ç‹Ç∑ÅB
	rhi::MemoryRange range(timeStampOffsetInByte, timeStampSizeInByte);
	u64* mapPtr = _timeStampBuffer.map<u64>(&range);
	memcpy(_gpuTimeStamps, mapPtr, timeStampSizeInByte);
	_timeStampBuffer.unmap();

	u64 completedFenceValue = _commandQueue.getCompletedValue();
	rhi::CommandList* commandList = _commandListPool.allocateCommandList(completedFenceValue);
	commandList->reset();

	rhi::Resource* resource = _timeStampBuffer.getResource();
	commandList->resolveQueryData(&_queryHeap, rhi::QUERY_TYPE_TIMESTAMP, timeStampOffset, timeStampCount, resource, timeStampOffsetInByte);
	_commandQueue.executeCommandLists(1, &commandList);

	_fenceValue = commandList->_fenceValue;
}

void GpuTimeStampManager::writeGpuMarker(rhi::CommandList* commandList, u32 timeStampIndex) {
	commandList->endQuery(&_queryHeap, rhi::QUERY_TYPE_TIMESTAMP, timeStampIndex);
}

f32 GpuTimeStampManager::getCurrentGpuFrameTime() const {
    return f32();
}

void GpuTimerManager::initialize() {
}

void GpuTimerManager::terminate() {
}

void GpuTimerManager::update(u32 frameIndex) {
	GpuTimeStampManager::Get()->update(frameIndex, _gpuTimerCount[_frameIndex]);
	_frameIndex = frameIndex;
}

u32 GpuTimerManager::pushGpuTimer(rhi::CommandList* commandList, const GpuTimerAdditionalInfo& info) {
	u32 timerIndex = ++_gpuTimerCount[_frameIndex];
	return timerIndex;
}
void GpuTimerManager::popGpuTimer(u32 timerIndex) {
}
}
