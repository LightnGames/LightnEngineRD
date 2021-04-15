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
	// カウンタをリセット　初期値として先頭に0を詰めておく
	_currentFrameGpuMarkerCount = 0;
	_currentFrameCpuMarkerCount = 0;
	_currentTickCount = 0;
}

void QueryHeapSystem::requestTimeStamp(CommandList* commandList, u32 frameIndex) {
	u32 queryFrameOffset = frameIndex * TICK_COUNT_MAX;
	commandList->resolveQueryData(_queryHeap, QUERY_TYPE_TIMESTAMP, queryFrameOffset, _currentFrameGpuMarkerCount, _timeStampBuffer.getResource(), queryFrameOffset * sizeof(u64));

	MemoryRange range = { queryFrameOffset , TICK_COUNT_MAX };
	u64* mapPtr = _timeStampBuffer.map<u64>(&range);
	memcpy(_gpuTicks, mapPtr, sizeof(u64) * TICK_COUNT_MAX);
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
	if (_currentFrameCpuMarkerCount == 0) {
		return;
	}

	Vector2 offset(230, 0);
	Vector2 size(200, 15);
	f32 rectScale = 0.3f;
	f32 freq = 1000.0f / _cpuFrequency; // ms
	for (u32 markerIndex = 1; markerIndex < _currentFrameCpuMarkerCount; ++markerIndex) {
		u64 startDelta = _cpuTicks[markerIndex - 1] - _cpuTicks[0];
		u64 timeStampDelta = _cpuTicks[markerIndex] - _cpuTicks[markerIndex - 1];
		f32 startTime = startDelta * freq;
		f32 frameTime = timeStampDelta * freq;
		Vector2 currentOffset(size._x * startTime * rectScale, 0);
		Vector2 currentScreenPos = DebugGui::GetCursorScreenPos() + offset;
		Vector2 currentScreenOffsetPos = DebugGui::GetCursorScreenPos() + offset + currentOffset;
		Vector2 currentSize(size._x * frameTime * rectScale, size._y);
		DebugGui::Text("[%2d] %-24s %-4.2f ms", _nestGpuParentIndices[markerIndex], _debugCpuMarkerNames[markerIndex], frameTime);
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
	u32 popCnt = 0;
	u32 remainingNestCount = 0;

	DebugGui::Columns(2, "tree", true);
	debugDrawTree(0);
	DebugGui::Columns(1);
}

u32 QueryHeapSystem::pushGpuMarker(CommandList* commandList, const char* markerName) {
	u32 currentNestLevel = _currentNestGpuLevel++;
	u32 currentMarkerIndex = _currentFrameGpuMarkerCount++;
	u32 currentTickIndex = _currentTickCount++;
	LTN_ASSERT(currentNestLevel < NEST_COUNT_MAX);
	LTN_ASSERT(currentMarkerIndex < TICK_COUNT_MAX);
	LTN_ASSERT(currentTickIndex < GPU_TIME_STAMP_COUNT_MAX);
	_currentNestMarkerIndices[currentNestLevel] = currentMarkerIndex;

	sprintf_s(_debugGpuMarkerNames[currentTickIndex], "%s", markerName);

	u32 frameOffset = GraphicsSystemImpl::Get()->getFrameIndex() * TICK_COUNT_MAX;
	commandList->endQuery(_queryHeap, QUERY_TYPE_TIMESTAMP, frameOffset + currentMarkerIndex);

	TickInfo& info = _gpuTickInfos[currentTickIndex];
	info._beginMarkerIndex = currentMarkerIndex;

	return currentTickIndex;
}

void QueryHeapSystem::popGpuMarker(CommandList* commandList, u32 tickIndex) {
	u32 currentNestLevel = --_currentNestGpuLevel;
	u32 currentMarkerIndex = _currentFrameGpuMarkerCount++;

	u32 frameOffset = GraphicsSystemImpl::Get()->getFrameIndex() * TICK_COUNT_MAX;
	commandList->endQuery(_queryHeap, QUERY_TYPE_TIMESTAMP, frameOffset + currentMarkerIndex);

	TickInfo& info = _gpuTickInfos[tickIndex];
	info._endMarkerIndex = currentMarkerIndex;

	_nestGpuIndices[tickIndex] = currentNestLevel;
	_nestGpuParentIndices[tickIndex] = currentNestLevel > 0 ? _currentNestMarkerIndices[currentNestLevel - 1] : 0;
}

u32 QueryHeapSystem::pushCpuMarker(const char * markerName) {
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	u32 currentFrameIndex = _currentFrameCpuMarkerCount++;
	_cpuTicks[currentFrameIndex] = static_cast<u64>(counter.QuadPart);
	sprintf_s(_debugCpuMarkerNames[currentFrameIndex], "%s", markerName);

	return currentFrameIndex;
}

void QueryHeapSystem::debugDrawTree(u32 tickIndex) {
	u32 currentMarkerNestLevel = _nestGpuIndices[tickIndex];
	u32 childNestLevel = currentMarkerNestLevel + 1;
	u32 childNestCount = 0;
	u32 childNestStartIndex = 0;
	for (u32 n = tickIndex + 1; n < _currentTickCount; ++n) {
		if (_nestGpuIndices[n] != childNestLevel) {
			break;
		}
		if (childNestStartIndex == 0) {
			childNestStartIndex = n;
		}
		childNestCount += (_nestGpuIndices[n] == childNestLevel) ? 1 : 0;
	}

	Vector2 offset(12, 0);
	Vector2 size(150, 15);
	f32 rectScale = 0.2f;
	f32 freq = 1000.0f / _gpuFrequency; // ms
	const TickInfo& tickInfo = _gpuTickInfos[tickIndex];
	u64 startDelta = _gpuTicks[tickInfo._beginMarkerIndex] - _gpuTicks[0];
	u64 endDelta = _gpuTicks[tickInfo._endMarkerIndex] - _gpuTicks[0];
	f32 startTime = startDelta * freq;
	f32 endTime = endDelta * freq;
	f32 frameTime = (endDelta - startDelta) * freq;
	Vector2 currenCursortScreenPos = Vector2(DebugGui::GetColumnWidth(), DebugGui::GetCursorScreenPos()._y);
	Vector2 currentScreenPos = currenCursortScreenPos + offset;
	Vector2 startSize(size._x * startTime * rectScale, size._y);
	Vector2 endSize(size._x * frameTime * rectScale, size._y);
	Vector2 startOrigin = currenCursortScreenPos + offset;
	Vector2 endOrigin = startOrigin + Vector2(startSize._x, 0);
	bool open2 = DebugGui::TreeNode(static_cast<s32>(tickIndex), "%-24s %-4.2f ms", _debugGpuMarkerNames[tickIndex], frameTime);
	if (!open2) {
		DebugGui::NextColumn();
		DebugGui::AddRectFilled(startOrigin, startOrigin + startSize, Color4::DEEP_GREEN, DebugGui::DrawCornerFlags_None);
		DebugGui::AddRectFilled(endOrigin, endOrigin + endSize, Color4::DEEP_RED, DebugGui::DrawCornerFlags_None);
		DebugGui::NextColumn();
	}

	if (open2 && (childNestCount > 0)) {
		u32 nextMarkerNestLevel = _nestGpuIndices[tickIndex + 1];
		if (nextMarkerNestLevel > currentMarkerNestLevel) {
			debugDrawTree(tickIndex + 1);
		}
	}

	if (open2) {
		DebugGui::TreePop();
	}

	u32 parentMarkerIndex = _nestGpuParentIndices[tickIndex];
	u32 nextMarkerIndex = 0;
	for (u32 n = tickIndex + 1; n < _currentTickCount; ++n) {
		u32 searchParentMarkerIndex = _nestGpuParentIndices[n];
		if (_nestGpuIndices[n] == currentMarkerNestLevel && parentMarkerIndex == searchParentMarkerIndex) {
			nextMarkerIndex = n;
			break;
		}
	}

	if (nextMarkerIndex > 0) {
		debugDrawTree(nextMarkerIndex);
	}
}

f32 QueryHeapSystem::getCurrentCpuFrameTime() const {
	if (_currentFrameCpuMarkerCount == 0) {
		return 0.0f;
	}
	f32 freq = 1000.0f / _cpuFrequency; // ms
	u64 delta = _cpuTicks[_currentFrameCpuMarkerCount - 1] - _cpuTicks[0];
	return delta * freq;
}

f32 QueryHeapSystem::getCurrentGpuFrameTime() const {
	if (_currentFrameGpuMarkerCount == 0) {
		return 0.0f;
	}
	const TickInfo& tickInfo = _gpuTickInfos[0];
	f32 freq = 1000.0f / _gpuFrequency; // ms
	u64 delta = _gpuTicks[tickInfo._endMarkerIndex] - _gpuTicks[tickInfo._beginMarkerIndex];
	return delta * freq;
}

QueryHeapSystem* QueryHeapSystem::Get() {
	return &_queryHeapSystem;
}
