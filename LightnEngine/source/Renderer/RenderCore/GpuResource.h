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

	rhi::Resource* getResource() { return &_resource; }
	rhi::ResourceStates getResourceState() const { return _currentState; }

protected:
	rhi::ResourceDesc _desc;
	rhi::ResourceStates _currentState;
	rhi::Resource _resource;
	rhi::VideoMemoryAllocation _allocation;
};
}