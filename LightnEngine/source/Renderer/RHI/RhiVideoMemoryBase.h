#pragma once
#include <Renderer/RHI/RhiVideoMemoryDef.h>

namespace ltn {
namespace rhi {
class VideoMemoryAllocationBase {
public:
	virtual void terminate() = 0;
	virtual bool isAllocated() const = 0;
};

class VideoMemoryAllocatorBase {
public:
	virtual void initialize(const VideoMemoryAllocatorDesc& desc) = 0;
	virtual void terminate() = 0;

	virtual void createResource(ResourceDesc desc, ResourceStates initialState, const ClearValue* optimizedClearValue, VideoMemoryAllocation* allocation, Resource* resource) = 0;
	virtual void createAliasingResource(ResourceDesc desc, u64 allocationLocalOffset, ResourceStates initialState, const ClearValue* optimizedClearValue, VideoMemoryAllocation* allocation, Resource* resource) = 0;
	virtual void allocateMemory(u32 sizeInByte, VideoMemoryAllocation* allocation) = 0;
};
}
}