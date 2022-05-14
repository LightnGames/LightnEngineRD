#pragma once
#include <Renderer/RHI/RhiDef.h>
#include <Renderer/RHI/RhiVideoMemoryDef.h>

namespace D3D12MA {
class Allocator;
class Allocation;
}

namespace ltn {
namespace rhi {
class VideoMemoryAllocationD3D12 {
public:
	virtual void terminate() = 0;
	virtual	bool isAllocated() const = 0;

	D3D12MA::Allocation* _allocation = nullptr;
};

class VideoMemoryAllocatorD3D12 {
public:
	virtual void initialize(const VideoMemoryAllocatorDesc& desc) = 0;
	virtual void terminate() = 0;

	virtual void createResource(ResourceDesc desc, ResourceStates initialState, VideoMemoryAllocation* allocation, Resource* resource) = 0;

	D3D12MA::Allocator* _allocator = nullptr;
};
}
}