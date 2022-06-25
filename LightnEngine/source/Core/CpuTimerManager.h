#pragma once
#include <Core/Type.h>
#include <Core/Math.h>
namespace ltn {
// CPU タイマー管理
class CpuTimerManager {
public:
	static constexpr u32 BUFFERING_COUNT = 2;
	static constexpr u32 NEST_COUNT_MAX = 16;
	static constexpr u32 CPU_TIMER_CAPACITY = 256;

	struct CpuTimerAdditionalInfo {
		char _name[64];
		Color4 _color;
	};

	void initialize();
	void terminate();
	void update();

	u32 pushCpuTimer();
	void popCpuTimer(u32 timerIndex);

	void writeCpuTimerInfo(u32 timerIndex, const char* format, va_list va, const Color4& color);

	u32 getTimerCount() const { return _cpuTimerCounts[_frameIndex]; }
	const CpuTimerAdditionalInfo* getGpuTimerAdditionalInfo(u32 timerIndex) {
		return &_cpuTimerAdditionalInfos[_frameIndex * CPU_TIMER_CAPACITY + timerIndex];
	}

	static CpuTimerManager* Get();

private:
	CpuTimerAdditionalInfo* _cpuTimerAdditionalInfos = nullptr;
	u32 _cpuTimerCounts[BUFFERING_COUNT] = {};
	u32 _frameIndex = 0;
};

// CPU タイムスタンプ管理
class CpuTimeStampManager {
public:
	static constexpr u32 TIME_STAMP_CAPACITY = CpuTimerManager::CPU_TIMER_CAPACITY * 2; // begin end

	void initialize();
	void terminate();
	void update(u32 frameIndex);

	void writeCpuTimeStamp(u32 timeStampIndex);

	f64 getCpuTickDelta() const { return _cpuTickDelta; }
	f64 getCpuTickDeltaInMilliseconds() const { return 1000.0 * _cpuTickDelta; }
	const u64* getCpuTimeStamps() const { return _prevFrameCpuTimeStamps; }

	static CpuTimeStampManager* Get();

private:
	u64* _currentFrameCpuTimeStamps = nullptr;
	u64* _prevFrameCpuTimeStamps = nullptr;
	u64* _cpuTimeStamps = nullptr;
	f64 _cpuTickDelta = 0;
};

// スコープ内の CPU 処理時間を記録してログに出すクラス
class CpuScopedPerf {
public:
	CpuScopedPerf(const char* name);
	~CpuScopedPerf();

private:
	u64 _beginTick = 0;
	const char* _name = nullptr;
};
}