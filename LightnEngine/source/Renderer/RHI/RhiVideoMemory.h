#pragma once

#include <Renderer/RHI/RhiConfig.h>
#include <Renderer/RHI/Rhi.h>
#if RHI_D3D12
#include <Renderer/D3D12RHI/D3D12RHIVideoMemory.h>
#endif

namespace ltn {
namespace rhi {
class VideoMemoryAllocation :public RhiBase(VideoMemoryAllocation) {
public:
	void terminate() override;
	bool isAllocated() const override;
};

class VideoMemoryAllocator :public RhiBase(VideoMemoryAllocator) {
public:
	void initialize(const VideoMemoryAllocatorDesc& desc) override;
	void terminate() override;

	void createResource(ResourceDesc desc, ResourceStates initialState, VideoMemoryAllocation * allocation, Resource * resource) override;
};
}
}