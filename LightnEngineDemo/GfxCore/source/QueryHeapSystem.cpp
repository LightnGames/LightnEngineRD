#include <GfxCore/impl/QueryHeapSystem.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>

QueryHeapSystem _queryHeapSystem;

void QueryHeapSystem::initialize() {
	Device* device = GraphicsSystemImpl::Get()->getDevice();
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(u64) * TICK_COUNT_MAX * BACK_BUFFER_COUNT;
		desc._initialState = RESOURCE_STATE_COPY_DEST;
		desc._heapType = HEAP_TYPE_READBACK;
		desc._device = device;
		_timeStampBuffer.initialize(desc);
		_timeStampBuffer.setDebugName("Time Stamp");
	}

	{
		QueryHeapDesc desc = {};
		desc._count = TICK_COUNT_MAX * BACK_BUFFER_COUNT;
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
	_gpuPerf.reset();
	_cpuPerf.reset();
}

void QueryHeapSystem::requestTimeStamp(CommandList* commandList, u32 frameIndex) {
	u32 queryFrameOffset = frameIndex * TICK_COUNT_MAX;
	commandList->resolveQueryData(_queryHeap, QUERY_TYPE_TIMESTAMP, queryFrameOffset, _gpuPerf._currentFrameMarkerCount, _timeStampBuffer.getResource(), queryFrameOffset * sizeof(u64));

	MemoryRange range = { queryFrameOffset , TICK_COUNT_MAX };
	u64* mapPtr = _timeStampBuffer.map<u64>(&range);
	memcpy(_gpuPerf._ticks, mapPtr, sizeof(u64) * TICK_COUNT_MAX);
	_timeStampBuffer.unmap();
}

void QueryHeapSystem::setGpuFrequency(CommandQueue* commandQueue) {
	commandQueue->getTimestampFrequency(&_gpuPerf._frequency);
}

void QueryHeapSystem::setCpuFrequency() {
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	_cpuPerf._frequency = static_cast<u64>(freq.QuadPart);
}

void QueryHeapSystem::debugDrawTimeStamps() {
	DebugGui::Start("Perf");
	if (DebugGui::BeginTabBar("TabBar")) {
		constexpr char format[] = "%s / %-4.2f ms";
		if (DebugGui::BeginTabItem("GPU")) {
			DebugGui::Text(format, "GPU", getCurrentGpuFrameTime());
			debugDrawGpuPerf();
			DebugGui::EndTabItem();
		}
		if (DebugGui::BeginTabItem("CPU")) {
			DebugGui::Text(format, "CPU", getCurrentCpuFrameTime());
			debugDrawCpuPerf();
			DebugGui::EndTabItem();
		}
		DebugGui::EndTabBar();
	}
	DebugGui::End();
}

void QueryHeapSystem::debugDrawCpuPerf() {
	if (_cpuPerf._currentFrameMarkerCount == 0) {
		return;
	}

	DebugGui::Columns(2, "tree", true);
	_cpuPerf.debugDrawTree(0);
	DebugGui::Columns(1);
}

void QueryHeapSystem::debugDrawGpuPerf() {
	if (_gpuPerf._currentFrameMarkerCount == 0) {
		return;
	}

	DebugGui::Columns(2, "tree", true);
	_gpuPerf.debugDrawTree(0);
	DebugGui::Columns(1);
}

u32 QueryHeapSystem::pushGpuMarker(CommandList* commandList, const char* markerName) {
	u32 currentTickIndex = _gpuPerf.pushTick(markerName);
	u32 currentMarkerIndex = _gpuPerf.pushMarker(currentTickIndex);

	u32 frameOffset = GraphicsSystemImpl::Get()->getFrameIndex() * TICK_COUNT_MAX;
	commandList->endQuery(_queryHeap, QUERY_TYPE_TIMESTAMP, frameOffset + currentMarkerIndex);
	return currentTickIndex;
}

void QueryHeapSystem::popGpuMarker(CommandList* commandList, u32 tickIndex) {
	u32 currentMarkerIndex = _gpuPerf.popMarker(tickIndex);
	u32 frameOffset = GraphicsSystemImpl::Get()->getFrameIndex() * TICK_COUNT_MAX;
	commandList->endQuery(_queryHeap, QUERY_TYPE_TIMESTAMP, frameOffset + currentMarkerIndex);
}

u32 QueryHeapSystem::pushCpuMarker(const char * markerName) {
	u32 currentTickIndex = _cpuPerf.pushTick(markerName);
	u32 currentMarkerIndex = _cpuPerf.pushMarker(currentTickIndex);

	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	_cpuPerf._ticks[currentMarkerIndex] = static_cast<u64>(counter.QuadPart);

	return currentTickIndex;
}

void QueryHeapSystem::popCpuMarker(u32 tickIndex) {
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	u32 currentMarkerIndex = _cpuPerf.popMarker(tickIndex);
	_cpuPerf._ticks[currentMarkerIndex] = static_cast<u64>(counter.QuadPart);
}

f32 QueryHeapSystem::getCurrentCpuFrameTime() const {
	if (_cpuPerf._currentFrameMarkerCount == 0) {
		return 0.0f;
	}
	const PerfInfo::TickInfo& tickInfo = _cpuPerf._tickInfos[0];
	f32 freq = 1000.0f / _cpuPerf._frequency; // ms
	u64 delta = _cpuPerf._ticks[tickInfo._endMarkerIndex] - _cpuPerf._ticks[tickInfo._beginMarkerIndex];
	return delta * freq;
}

f32 QueryHeapSystem::getCurrentGpuFrameTime() const {
	if (_gpuPerf._currentFrameMarkerCount == 0) {
		return 0.0f;
	}
	const PerfInfo::TickInfo& tickInfo = _gpuPerf._tickInfos[0];
	f32 freq = 1000.0f / _gpuPerf._frequency; // ms
	u64 delta = _gpuPerf._ticks[tickInfo._endMarkerIndex] - _gpuPerf._ticks[tickInfo._beginMarkerIndex];
	return delta * freq;
}

QueryHeapSystem* QueryHeapSystem::Get() {
	return &_queryHeapSystem;
}

u32 PerfInfo::pushMarker(u32 tickIndex) {
	u32 currentNestLevel = _currentNestLevel++;
	u32 currentMarkerIndex = _currentFrameMarkerCount++;
	LTN_ASSERT(currentNestLevel < NEST_LEVEL_COUNT_MAX);
	LTN_ASSERT(currentMarkerIndex < TICK_COUNT_MAX);
	_currentNestMarkerIndices[currentNestLevel] = currentMarkerIndex;

	PerfInfo::TickInfo& info = _tickInfos[tickIndex];
	info._beginMarkerIndex = currentMarkerIndex;

	return currentMarkerIndex;
}

u32 PerfInfo::pushTick(const char * markerName) {
	u32 currentTickIndex = _currentTickCount++;
	LTN_ASSERT(currentTickIndex < TIME_STAMP_COUNT_MAX);
	sprintf_s(_markerNames[currentTickIndex], "%s", markerName);
	return currentTickIndex;
}

u32 PerfInfo::popMarker(u32 tickIndex) {
	u32 currentNestLevel = --_currentNestLevel;
	u32 currentMarkerIndex = _currentFrameMarkerCount++;

	PerfInfo::TickInfo& info = _tickInfos[tickIndex];
	info._endMarkerIndex = currentMarkerIndex;

	_markerNestLevelIndices[tickIndex] = currentNestLevel;
	_markerParentIndices[tickIndex] = currentNestLevel > 0 ? _currentNestMarkerIndices[currentNestLevel - 1] : 0;
	return currentMarkerIndex;
}

void PerfInfo::reset() {
	_currentFrameMarkerCount = 0;
	_currentTickCount = 0;
}

void PerfInfo::debugDrawTree(u32 tickIndex) {
	u32 currentMarkerNestLevel = _markerNestLevelIndices[tickIndex];
	u32 childNestLevel = currentMarkerNestLevel + 1;
	u32 childNestCount = 0;
	u32 childNestStartIndex = 0;
	for (u32 n = tickIndex + 1; n < _currentTickCount; ++n) {
		if (_markerNestLevelIndices[n] != childNestLevel) {
			break;
		}
		if (childNestStartIndex == 0) {
			childNestStartIndex = n;
		}
		childNestCount += (_markerNestLevelIndices[n] == childNestLevel) ? 1 : 0;
	}

	Vector2 offset(12, 0);
	Vector2 size(150, 15);
	f32 rectScale = 0.2f;
	f32 freq = 1000.0f / _frequency; // ms
	const TickInfo& tickInfo = _tickInfos[tickIndex];
	u64 startDelta = _ticks[tickInfo._beginMarkerIndex] - _ticks[0];
	u64 endDelta = _ticks[tickInfo._endMarkerIndex] - _ticks[0];
	f32 startTime = startDelta * freq;
	f32 endTime = endDelta * freq;
	f32 frameTime = (endDelta - startDelta) * freq;
	Vector2 currenCursortScreenPos = Vector2(DebugGui::GetColumnWidth(), DebugGui::GetCursorScreenPos()._y);
	Vector2 currentScreenPos = currenCursortScreenPos + offset;
	Vector2 startSize(size._x * startTime * rectScale, size._y);
	Vector2 endSize(size._x * frameTime * rectScale, size._y);
	Vector2 startOrigin = currenCursortScreenPos + offset;
	Vector2 endOrigin = startOrigin + Vector2(startSize._x, 0);
	bool open2 = DebugGui::TreeNode(static_cast<s32>(tickIndex), "%-24s %-4.2f ms", _markerNames[tickIndex], frameTime);
	if (!open2) {
		DebugGui::NextColumn();
		DebugGui::AddRectFilled(startOrigin, startOrigin + startSize, Color4::DEEP_GREEN, DebugGui::DrawCornerFlags_None);
		DebugGui::AddRectFilled(endOrigin, endOrigin + endSize, Color4::DEEP_RED, DebugGui::DrawCornerFlags_None);
		DebugGui::NextColumn();
	}

	if (open2 && (childNestCount > 0)) {
		u32 nextMarkerNestLevel = _markerNestLevelIndices[tickIndex + 1];
		if (nextMarkerNestLevel > currentMarkerNestLevel) {
			debugDrawTree(tickIndex + 1);
		}
	}

	if (open2) {
		DebugGui::TreePop();
	}

	u32 parentMarkerIndex = _markerParentIndices[tickIndex];
	u32 nextMarkerIndex = 0;
	for (u32 n = tickIndex + 1; n < _currentTickCount; ++n) {
		u32 searchParentMarkerIndex = _markerParentIndices[n];
		if (_markerNestLevelIndices[n] == currentMarkerNestLevel && parentMarkerIndex == searchParentMarkerIndex) {
			nextMarkerIndex = n;
			break;
		}
	}

	if (nextMarkerIndex > 0) {
		debugDrawTree(nextMarkerIndex);
	}
}
