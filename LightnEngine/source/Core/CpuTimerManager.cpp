#include "CpuTimerManager.h"
#include <Core/Memory.h>
#include <Core/Utility.h>
#include <Windows.h>
#include <stdio.h>
namespace ltn {
namespace {
CpuTimerManager g_cpuTimerManager;
CpuTimeStampManager g_cpuTimeStampManager;
}

void CpuTimerManager::initialize() {
	_cpuTimerAdditionalInfos = Memory::allocObjects<CpuTimerAdditionalInfo>(CPU_TIMER_CAPACITY * BUFFERING_COUNT);
	CpuTimeStampManager::Get()->initialize();
}

void CpuTimerManager::terminate() {
	Memory::deallocObjects(_cpuTimerAdditionalInfos);
	CpuTimeStampManager::Get()->terminate();
}

void CpuTimerManager::update() {
	CpuTimeStampManager::Get()->update(_frameIndex);
	_frameIndex = (_frameIndex + 1) % BUFFERING_COUNT;
	_cpuTimerCounts[_frameIndex] = 0;
}

u32 CpuTimerManager::pushCpuTimer() {
	u32 timerIndex = _cpuTimerCounts[_frameIndex]++;
	CpuTimeStampManager::Get()->writeCpuTimeStamp(timerIndex * 2);
	return timerIndex;
}

void CpuTimerManager::popCpuTimer(u32 timerIndex) {
	CpuTimeStampManager::Get()->writeCpuTimeStamp(timerIndex * 2 + 1);
}

void CpuTimerManager::writeCpuTimerInfo(u32 timerIndex, const char* format, va_list va, const Color4& color) {
	CpuTimerAdditionalInfo* infos = _cpuTimerAdditionalInfos + _frameIndex * CPU_TIMER_CAPACITY;
	CpuTimerAdditionalInfo& info = infos[timerIndex];
	info._color = color;
	vsprintf_s(info._name, format, va);
}

CpuTimerManager* ltn::CpuTimerManager::Get() {
	return &g_cpuTimerManager;
}

void CpuTimeStampManager::initialize() {
	_cpuTimeStamps = Memory::allocObjects<u64>(TIME_STAMP_CAPACITY * CpuTimerManager::BUFFERING_COUNT);

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	_cpuTickDelta = 1.0 / f64(freq.QuadPart);
	_currentFrameCpuTimeStamps = _cpuTimeStamps;
	_prevFrameCpuTimeStamps = _cpuTimeStamps + TIME_STAMP_CAPACITY;
}

void CpuTimeStampManager::terminate() {
	Memory::deallocObjects(_cpuTimeStamps);
}

void CpuTimeStampManager::update(u32 frameIndex) {
	_prevFrameCpuTimeStamps = _currentFrameCpuTimeStamps;
	_currentFrameCpuTimeStamps = _cpuTimeStamps + TIME_STAMP_CAPACITY * frameIndex;
}

void CpuTimeStampManager::writeCpuTimeStamp(u32 timeStampIndex) {
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	_currentFrameCpuTimeStamps[timeStampIndex] = counter.QuadPart;
}

CpuTimeStampManager* CpuTimeStampManager::Get() {
	return &g_cpuTimeStampManager;
}

CpuScopedPerf::CpuScopedPerf(const char* name) {
	_name = name;
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	_beginTick = counter.QuadPart;
}

CpuScopedPerf::~CpuScopedPerf() {
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	f64 tickInMilliSeconds = CpuTimeStampManager::Get()->getCpuTickDelta() * (counter.QuadPart - _beginTick);
	LTN_INFO("%-30s %3.4f ms", _name, tickInMilliSeconds);
}
}
