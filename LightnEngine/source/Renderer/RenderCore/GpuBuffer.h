#pragma once

#include <Renderer/RenderCore/GpuResource.h>
namespace ltn {
struct GpuBufferDesc {
	// アロケーターへのポインタを有効にするとGPUリソースはロケーターから確保されます
	rhi::VideoMemoryAllocator* _allocator = nullptr;
	rhi::Device* _device = nullptr;
	rhi::ResourceStates _initialState;
	u32 _sizeInByte = 0;
	rhi::ResourceFlags _flags = rhi::RESOURCE_FLAG_NONE;
	rhi::HeapType _heapType = rhi::HEAP_TYPE_DEFAULT;
};

struct GpuDynamicBufferDesc {
	rhi::Device* _device = nullptr;
	u32 _sizeInByte = 0;
};

class GpuBuffer :public GpuResource {
public:
	void initialize(const GpuBufferDesc& desc);
	void initialize(const GpuDynamicBufferDesc& desc);

	template<class T>
	T* map(const rhi::MemoryRange* range = nullptr) {
		return reinterpret_cast<T*>(_resource.map(range));
	}

	void unmap(const rhi::MemoryRange* range = nullptr) {
		_resource.unmap(range);
	}
};
}