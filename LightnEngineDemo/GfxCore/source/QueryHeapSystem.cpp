#include <GfxCore/impl/QueryHeapSystem.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>

QueryHeapSystem _queryHeapSystem;

void QueryHeapSystem::initialize() {
	Device* device = GraphicsSystemImpl::Get()->getDevice();
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(u64) * GPU_TIME_STAMP_COUNT_MAX * BACK_BUFFER_COUNT;
		desc._initialState = RESOURCE_STATE_COPY_DEST;
		desc._heapType = HEAP_TYPE_READBACK;
		desc._device = device;
		_timeStampBuffer.initialize(desc);
		_timeStampBuffer.setDebugName("Time Stamp");
		_mapedTimeStampPtr = _timeStampBuffer.map<u64>();
	}

	{
		QueryHeapDesc desc = {};
		desc._count = GPU_TIME_STAMP_COUNT_MAX * BACK_BUFFER_COUNT;
		desc._type = QUERY_HEAP_TYPE_TIMESTAMP;
		desc._device = device;
		_queryHeap = GraphicsApiInstanceAllocator::Get()->allocateQueryHeap();
		_queryHeap->initialize(desc);
	}
}

void QueryHeapSystem::terminate() {
	_timeStampBuffer.terminate();
	_queryHeap->terminate();
	_mapedTimeStampPtr = nullptr;
	_currentTimeStamps = nullptr;
}

void QueryHeapSystem::update() {
	_currentFrameMarkerCount = 0;
}

void QueryHeapSystem::requestTimeStamp(CommandList* commandList, u32 frameIndex) {
	u32 queryFrameOffset = frameIndex * GPU_TIME_STAMP_COUNT_MAX;
	commandList->resolveQueryData(_queryHeap, QUERY_TYPE_TIMESTAMP, queryFrameOffset, _currentFrameMarkerCount, _timeStampBuffer.getResource(), queryFrameOffset * sizeof(u64));
	_currentTimeStamps = _mapedTimeStampPtr + queryFrameOffset;
}

void QueryHeapSystem::setGpuFrequency(CommandQueue* commandQueue) {
	commandQueue->getTimestampFrequency(&_currentFrameFrequency);
}

void QueryHeapSystem::debugDrawTimeStamps() {
	DebugWindow::StartWindow("GPU Time Stamp");
	Vector2 offset(230, 0);
	Vector2 size(200, 15);
	f32 rectScale = 0.3f;
	if (_currentFrameMarkerCount > 0) {
		f32 freq = 1000.0f / _currentFrameFrequency; // ms
		for (u32 markerIndex = 1; markerIndex < _currentFrameMarkerCount; ++markerIndex) {
			u64 startDelta = _currentTimeStamps[markerIndex - 1] -_currentTimeStamps[0];
			u64 timeStampDelta = _currentTimeStamps[markerIndex] - _currentTimeStamps[markerIndex - 1];
			f32 startTime = startDelta * freq;
			f32 frameTime = timeStampDelta * freq;
			Vector2 currentOffset(size._x * startTime * rectScale, 0);
			Vector2 currentScreenPos = DebugGui::GetCursorScreenPos() + offset;
			Vector2 currentScreenOffsetPos = DebugGui::GetCursorScreenPos() + offset + currentOffset;
			Vector2 currentSize(size._x * frameTime * rectScale, size._y);
			DebugGui::Text("%-24s %-4.2f ms", _debugMarkerNames[markerIndex], frameTime);
			DebugGui::AddRectFilled(currentScreenPos, currentScreenOffsetPos + currentSize, Color4::DEEP_GREEN, DebugGui::DrawCornerFlags_None);
			DebugGui::AddRectFilled(currentScreenOffsetPos, currentScreenOffsetPos + currentSize, Color4::DEEP_RED, DebugGui::DrawCornerFlags_None);
		}
	}
	DebugWindow::End();
}

void QueryHeapSystem::setMarker(CommandList* commandList) {
	u32 frameOffset = GraphicsSystemImpl::Get()->getFrameIndex() * GPU_TIME_STAMP_COUNT_MAX;
	u32 currentFrameIndex = _currentFrameMarkerCount++;
	commandList->endQuery(_queryHeap, QUERY_TYPE_TIMESTAMP, frameOffset + currentFrameIndex);
}

void QueryHeapSystem::setCurrentMarkerName(const char* markerName) {
	sprintf_s(_debugMarkerNames[_currentFrameMarkerCount], "%s", markerName);
}

QueryHeapSystem* QueryHeapSystem::Get() {
	return &_queryHeapSystem;
}
