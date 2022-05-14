#pragma once
#include <Renderer/RHI/RhiDef.h>
namespace ltn {
namespace rhi {
class VideoMemoryAllocation;

struct VideoMemoryAllocatorDesc {
	rhi::Device* _device = nullptr;
	rhi::HardwareAdapter* _hardwareAdapter = nullptr;
};
}
}