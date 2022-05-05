#pragma once
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RenderCore/GpuResource.h>
namespace ltn {
struct ScopedBarrierDesc {
	ScopedBarrierDesc(GpuResource* resource, rhi::ResourceStates stateAfter)
		:_resources(resource), _stateAfter(stateAfter) {}
	GpuResource* _resources = nullptr;
	rhi::ResourceStates _stateAfter;
};

class ScopedBarrier {
public:
	static constexpr u32 SCOPED_BARRIER_COUNT_MAX = 8;
	ScopedBarrier(rhi::CommandList* commandList, ScopedBarrierDesc* barrierDescs, u32 barrierCount) {
		LTN_ASSERT(barrierCount < SCOPED_BARRIER_COUNT_MAX);
		_commandList = commandList;
		_barrierCount = barrierCount;
		_barrierDescs = barrierDescs;

		for (u32 i = 0; i < barrierCount; ++i) {
			_beforeResourceStates[i] = barrierDescs[i]._resources->getResourceState();
		}

		rhi::ResourceTransitionBarrier barriers[16];
		for (u32 i = 0; i < barrierCount; ++i) {
			ScopedBarrierDesc& desc = barrierDescs[i];
			barriers[i] = desc._resources->getTransitionBarrier(desc._stateAfter);
		}

		for (u32 i = 0; i < barrierCount; ++i) {
			ScopedBarrierDesc& desc = barrierDescs[i];
			desc._resources->setResourceState(desc._stateAfter);
		}

		commandList->transitionBarriers(barriers, barrierCount);
	}

	~ScopedBarrier() {
		rhi::ResourceTransitionBarrier barriers[SCOPED_BARRIER_COUNT_MAX];
		for (u32 i = 0; i < _barrierCount; ++i) {
			barriers[i] = _barrierDescs[i]._resources->getTransitionBarrier(_beforeResourceStates[i]);
		}

		for (u32 i = 0; i < _barrierCount; ++i) {
			_barrierDescs[i]._resources->setResourceState(_beforeResourceStates[i]);
		}

		_commandList->transitionBarriers(barriers, _barrierCount);
	}

private:
	rhi::ResourceStates _beforeResourceStates[SCOPED_BARRIER_COUNT_MAX] = {};
	rhi::CommandList* _commandList = nullptr;
	ScopedBarrierDesc* _barrierDescs = nullptr;
	u32 _barrierCount = 0;
};
}