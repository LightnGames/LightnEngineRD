#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RHI/RhiVideoMemory.h>

namespace ltn {
class GpuResource {
public:
	void terminate() {
		_resource.terminate();
		if (_allocation.isAllocated()) {
			_allocation.terminate();
		}
	}

	rhi::ResourceTransitionBarrier getTransitionBarrier(rhi::ResourceStates stateAfter) {
		rhi::ResourceTransitionBarrier barrier = {};
		barrier._resource = &_resource;
		barrier._stateAfter = stateAfter;
		barrier._stateBefore = _currentState;
		return barrier;
	}

	void setResourceState(rhi::ResourceStates stateAfter) {
		_currentState = stateAfter;
	}

	void setName(const char* format, ...) {
		constexpr u32 SET_NAME_LENGTH_COUNT_MAX = 64;
		char nameBuffer[SET_NAME_LENGTH_COUNT_MAX] = {};
		va_list va;
		va_start(va, format);
		vsprintf_s(nameBuffer, format, va);
		va_end(va);

		_resource.setName(nameBuffer);
	}

	u32 getU32ElementCount() const { return _sizeInByte / sizeof(u32); }

	rhi::Resource* getResource() { return &_resource; }
	rhi::ResourceStates getResourceState() const { return _currentState; }
	rhi::ResourceDesc getResourceDesc() const { return _desc; }
	u32 getSizeInByte() const { return _sizeInByte; }
	u64 getGpuVirtualAddress() const { return _resource.getGpuVirtualAddress(); }

protected:
	u32 _sizeInByte = 0;
	rhi::ResourceDesc _desc;
	rhi::ResourceStates _currentState = rhi::RESOURCE_STATE_COMMON;
	rhi::Resource _resource;
	rhi::VideoMemoryAllocation _allocation;
};
}