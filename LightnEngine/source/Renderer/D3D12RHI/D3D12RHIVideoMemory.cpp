#include "D3D12RHIVideoMemory.h"
#include <Renderer/RHI/Rhi.h>
#include <Renderer/RHI/RhiVideoMemory.h>
#include <ThiredParty/D3D12MemoryAllocator/D3D12MemAlloc.h>

namespace ltn {
namespace rhi {
void VideoMemoryAllocation::terminate() {
	_allocation->Release();
}

void VideoMemoryAllocator::initialize(const VideoMemoryAllocatorDesc& desc) {
	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = desc._device->_device;
	allocatorDesc.pAdapter = desc._hardwareAdapter->_adapter;
	LTN_SUCCEEDED(D3D12MA::CreateAllocator(&allocatorDesc, &_allocator));
}
void VideoMemoryAllocator::terminate() {
	_allocator->Release();
}
void VideoMemoryAllocator::createResource(ResourceDesc desc, ResourceStates initialState, VideoMemoryAllocation* allocation, Resource* resource) {
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC d3d12ResourceDesc = toD3d12(desc);
	HRESULT hr = _allocator->CreateResource(
		&allocationDesc,
		&d3d12ResourceDesc,
		toD3d12(initialState),
		NULL,
		&allocation->_allocation,
		IID_PPV_ARGS(&resource->_resource));
}
bool VideoMemoryAllocation::isAllocated() const {
	return _allocation != nullptr;
}
}
}
