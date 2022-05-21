#pragma once
#include <Renderer/RHI/RhiVideoMemory.h>

namespace ltn {
class GlobalVideoMemoryAllocator{
public:
	void initialize(const rhi::VideoMemoryAllocatorDesc& desc);
	void terminate();

	rhi::VideoMemoryAllocator* getAllocator() { return &_allocator; }

	static GlobalVideoMemoryAllocator* Get();
private:
	rhi::VideoMemoryAllocator _allocator;
};
}