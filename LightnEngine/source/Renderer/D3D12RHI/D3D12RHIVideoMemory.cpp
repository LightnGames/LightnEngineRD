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

void VideoMemoryAllocator::createResource(ResourceDesc desc, ResourceStates initialState, const ClearValue* optimizedClearValue, VideoMemoryAllocation* allocation, Resource* resource) {
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC d3d12ResourceDesc = toD3d12(desc);
	HRESULT hr = _allocator->CreateResource(
		&allocationDesc,
		&d3d12ResourceDesc,
		toD3d12(initialState),
		toD3d12(optimizedClearValue),
		&allocation->_allocation,
		IID_PPV_ARGS(&resource->_resource));
}

void VideoMemoryAllocator::createAliasingResource(ResourceDesc desc, u64 allocationLocalOffset, ResourceStates initialState, const ClearValue* optimizedClearValue, VideoMemoryAllocation* allocation, Resource* resource) {
	D3D12_RESOURCE_DESC d3d12ResourceDesc = toD3d12(desc);
	HRESULT hr = _allocator->CreateAliasingResource(
		allocation->_allocation,
		allocationLocalOffset,
		&d3d12ResourceDesc,
		toD3d12(initialState),
		toD3d12(optimizedClearValue),
		IID_PPV_ARGS(&resource->_resource));
}

void VideoMemoryAllocator::allocateMemory(u32 sizeInByte, VideoMemoryAllocation* allocation) {
	D3D12_RESOURCE_ALLOCATION_INFO finalAllocInfo = {};
	finalAllocInfo.Alignment = 65536; // 64kb
	finalAllocInfo.SizeInBytes = sizeInByte;
	LTN_ASSERT((finalAllocInfo.SizeInBytes % finalAllocInfo.Alignment) == 0);

	D3D12MA::ALLOCATION_DESC allocDesc = {};
	allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	D3D12MA::Allocation*& alloc = allocation->_allocation;
	HRESULT hr = _allocator->AllocateMemory(&allocDesc, &finalAllocInfo, &alloc);
	LTN_ASSERT(alloc != NULL && alloc->GetHeap() != NULL);
}

bool VideoMemoryAllocation::isAllocated() const {
	return _allocation != nullptr;
}
}
}
