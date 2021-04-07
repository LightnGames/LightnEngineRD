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
}

void QueryHeapSystem::update() {
	// カウンタをリセット　初期値として先頭に0を詰めておく
	_currentFrameGpuMarkerCount = 1;
	_currentFrameCpuMarkerCount = 1;
	_currentGpuTimeStamps[0] = 0;
	_currentCpuTimeStamps[0] = 0;
}

void QueryHeapSystem::requestTimeStamp(CommandList* commandList, u32 frameIndex) {
	u32 queryFrameOffset = frameIndex * GPU_TIME_STAMP_COUNT_MAX;
	commandList->resolveQueryData(_queryHeap, QUERY_TYPE_TIMESTAMP, queryFrameOffset, _currentFrameGpuMarkerCount, _timeStampBuffer.getResource(), queryFrameOffset * sizeof(u64));

	MemoryRange range = { queryFrameOffset , GPU_TIME_STAMP_COUNT_MAX };
	u64* mapPtr = _timeStampBuffer.map<u64>(&range);
	memcpy(_currentGpuTimeStamps, mapPtr, sizeof(u64) * GPU_TIME_STAMP_COUNT_MAX);
	_timeStampBuffer.unmap();
}

void QueryHeapSystem::setGpuFrequency(CommandQueue* commandQueue) {
	commandQueue->getTimestampFrequency(&_gpuFrequency);
}

void QueryHeapSystem::setCpuFrequency() {
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	_cpuFrequency = static_cast<u64>(freq.QuadPart);
}

void QueryHeapSystem::debugDrawTimeStamps() {
	if (DebugGui::BeginTabBar("Perf")) {
		if (DebugGui::BeginTabItem("GPU")) {
			debugDrawGpuPerf();
		}
		if (DebugGui::BeginTabItem("CPU")) {
			debugDrawGpuPerf();
		}
		DebugGui::EndTabBar();
	}
}

void QueryHeapSystem::debugDrawCpuPerf() {
	if (_currentFrameCpuMarkerCount == 0) {
		return;
	}

	Vector2 offset(230, 0);
	Vector2 size(200, 15);
	f32 rectScale = 0.3f;
	f32 freq = 1000.0f / _cpuFrequency; // ms
	for (u32 markerIndex = 1; markerIndex < _currentFrameCpuMarkerCount; ++markerIndex) {
		u64 startDelta = _currentCpuTimeStamps[markerIndex - 1] - _currentCpuTimeStamps[0];
		u64 timeStampDelta = _currentCpuTimeStamps[markerIndex] - _currentCpuTimeStamps[markerIndex - 1];
		f32 startTime = startDelta * freq;
		f32 frameTime = timeStampDelta * freq;
		Vector2 currentOffset(size._x * startTime * rectScale, 0);
		Vector2 currentScreenPos = DebugGui::GetCursorScreenPos() + offset;
		Vector2 currentScreenOffsetPos = DebugGui::GetCursorScreenPos() + offset + currentOffset;
		Vector2 currentSize(size._x * frameTime * rectScale, size._y);
		DebugGui::Text("%-24s %-4.2f ms", _debugCpuMarkerNames[markerIndex], frameTime);
		DebugGui::AddRectFilled(currentScreenPos, currentScreenOffsetPos + currentSize, Color4::DEEP_GREEN, DebugGui::DrawCornerFlags_None);
		DebugGui::AddRectFilled(currentScreenOffsetPos, currentScreenOffsetPos + currentSize, Color4::DEEP_RED, DebugGui::DrawCornerFlags_None);
	}
}

void QueryHeapSystem::debugDrawGpuPerf() {
	if (_currentFrameGpuMarkerCount == 0) {
		return;
	}

	Vector2 offset(230, 0);
	Vector2 size(200, 15);
	f32 rectScale = 0.3f;
	f32 freq = 1000.0f / _gpuFrequency; // ms
	for (u32 markerIndex = 1; markerIndex < _currentFrameGpuMarkerCount; ++markerIndex) {
		u64 startDelta = _currentGpuTimeStamps[markerIndex - 1] - _currentGpuTimeStamps[0];
		u64 timeStampDelta = _currentGpuTimeStamps[markerIndex] - _currentGpuTimeStamps[markerIndex - 1];
		f32 startTime = startDelta * freq;
		f32 frameTime = timeStampDelta * freq;
		Vector2 currentOffset(size._x * startTime * rectScale, 0);
		Vector2 currentScreenPos = DebugGui::GetCursorScreenPos() + offset;
		Vector2 currentScreenOffsetPos = DebugGui::GetCursorScreenPos() + offset + currentOffset;
		Vector2 currentSize(size._x * frameTime * rectScale, size._y);
		DebugGui::Text("%-24s %-4.2f ms", _debugGpuMarkerNames[markerIndex], frameTime);
		DebugGui::AddRectFilled(currentScreenPos, currentScreenOffsetPos + currentSize, Color4::DEEP_GREEN, DebugGui::DrawCornerFlags_None);
		DebugGui::AddRectFilled(currentScreenOffsetPos, currentScreenOffsetPos + currentSize, Color4::DEEP_RED, DebugGui::DrawCornerFlags_None);
	}
}

void QueryHeapSystem::setGpuMarker(CommandList* commandList, const char* markerName) {
	u32 frameOffset = GraphicsSystemImpl::Get()->getFrameIndex() * GPU_TIME_STAMP_COUNT_MAX;
	u32 currentFrameIndex = _currentFrameGpuMarkerCount++;
	commandList->endQuery(_queryHeap, QUERY_TYPE_TIMESTAMP, frameOffset + currentFrameIndex);
	if (markerName != nullptr) {
		sprintf_s(_debugGpuMarkerNames[_currentFrameGpuMarkerCount], "%s", markerName);
	}
}

void QueryHeapSystem::setCpuMarker(const char* markerName) {
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	u32 currentFrameIndex = _currentFrameCpuMarkerCount++;
	_currentCpuTimeStamps[currentFrameIndex] = static_cast<u64>(counter.QuadPart);
	sprintf_s(_debugCpuMarkerNames[currentFrameIndex], "%s", markerName);
}

QueryHeapSystem* QueryHeapSystem::Get() {
	return &_queryHeapSystem;
}
