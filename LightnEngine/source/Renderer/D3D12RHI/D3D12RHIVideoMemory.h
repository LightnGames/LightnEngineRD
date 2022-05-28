#pragma once
#include <Renderer/RHI/RhiVideoMemoryBase.h>

namespace D3D12MA {
class Allocator;
class Allocation;
}

namespace ltn {
namespace rhi {
class VideoMemoryAllocation :public VideoMemoryAllocationBase {
public:
	void terminate()  override;
	virtual	bool isAllocated() const  override;

	D3D12MA::Allocation* _allocation = nullptr;
};

class VideoMemoryAllocator :public VideoMemoryAllocatorBase {
public:
	void initialize(const VideoMemoryAllocatorDesc& desc)  override;
	void terminate()  override;

	void createResource(ResourceDesc desc, ResourceStates initialState, VideoMemoryAllocation* allocation, Resource* resource)  override;

	D3D12MA::Allocator* _allocator = nullptr;
};
}
}