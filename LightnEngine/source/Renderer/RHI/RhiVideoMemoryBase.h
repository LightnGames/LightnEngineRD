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

	virtual void createResource(ResourceDesc desc, ResourceStates initialState, VideoMemoryAllocation * allocation, Resource * resource) = 0;
};
}
}