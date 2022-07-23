#pragma once

#include <Renderer/RenderCore/GpuResource.h>
namespace ltn {
class GpuAliasingMemoryAllocation;
struct GpuBufferDesc {
	// �A���P�[�^�[�ւ̃|�C���^��L���ɂ����GPU���\�[�X�̓��P�[�^�[����m�ۂ���܂�
	rhi::Device* _device = nullptr;
	rhi::ResourceStates _initialState;
	rhi::ResourceFlags _flags = rhi::RESOURCE_FLAG_NONE;
	rhi::HeapType _heapType = rhi::HEAP_TYPE_DEFAULT;
	rhi::VideoMemoryAllocator* _allocator = nullptr;
	GpuAliasingMemoryAllocation* _aliasingMemoryAllocation = nullptr;
	u32 _sizeInByte = 0;
	u32 _offsetSizeInByte = 0;
};

struct GpuDynamicBufferDesc {
	rhi::Device* _device = nullptr;
	u32 _sizeInByte = 0;
};

// �������G�C���A�V���O�p�@�������̈�
class GpuAliasingMemoryAllocation {
public:
	void initialize(rhi::VideoMemoryAllocator* allocator, u32 sizeInByte);
	void terminate();

	rhi::VideoMemoryAllocation* getAllocation() { return &_allocation; }

private:
	rhi::VideoMemoryAllocation _allocation;
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