#include "GlobalVideoMemoryAllocator.h"

namespace ltn {
namespace {
GlobalVideoMemoryAllocator g_videoMemoryAllocator;
}
void GlobalVideoMemoryAllocator::initialize(const rhi::VideoMemoryAllocatorDesc& desc) {
	_allocator.initialize(desc);
}

void GlobalVideoMemoryAllocator::terminate() {
	_allocator.terminate();
}

GlobalVideoMemoryAllocator* GlobalVideoMemoryAllocator::Get() {
	return &g_videoMemoryAllocator;
}
}
