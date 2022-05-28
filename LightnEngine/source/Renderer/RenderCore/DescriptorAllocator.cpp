#include "DescriptorAllocator.h"

namespace ltn {
namespace {
DescriptorAllocatorGroup g_descriptorAllocatorGroup;
}
void DescriptorAllocator::initialize(const rhi::DescriptorHeapDesc& desc) {
	_descriptorHeap.initialize(desc);

	VirtualArray::Desc handleDesc = {};
	handleDesc._size = desc._numDescriptors;
	_allocationInfo.initialize(handleDesc);

	_incrementSize = desc._device->getDescriptorHandleIncrementSize(desc._type);
	_cpuHandleStart = _descriptorHeap.getCPUDescriptorHandleForHeapStart();

	if (desc._flags == rhi::DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
		_gpuHandleStart = _descriptorHeap.getGPUDescriptorHandleForHeapStart();
	}
}

void DescriptorAllocator::terminate() {
	_descriptorHeap.terminate();
	_allocationInfo.terminate();
}

DescriptorHandle DescriptorAllocator::allocate() {
	return allocate(1)._firstHandle;
}

DescriptorHandles DescriptorAllocator::allocate(u32 count) {
	VirtualArray::AllocationInfo allocationInfo = _allocationInfo.allocation(count);
	u32 offset = static_cast<u32>(allocationInfo._offset) * _incrementSize;
	DescriptorHandles descriptors = {};
	descriptors._descriptorCount = count;
	descriptors._incrementSize = _incrementSize;
	descriptors._firstHandle._allocationInfo = allocationInfo;
	descriptors._firstHandle._cpuHandle = _cpuHandleStart + offset;
	descriptors._firstHandle._gpuHandle = _gpuHandleStart + offset;
	return descriptors;
}

void DescriptorAllocator::free(DescriptorHandle& descriptorHandle) {
	_allocationInfo.freeAllocation(descriptorHandle._allocationInfo);
}

void DescriptorAllocator::free(DescriptorHandles& descriptorHandles) {
	_allocationInfo.freeAllocation(descriptorHandles._firstHandle._allocationInfo);
}

void DescriptorAllocator::setName(const char* name) {
	_descriptorHeap.setName(name);
}

void DescriptorAllocatorGroup::initialize(const Desc& desc) {
	rhi::DescriptorHeapDesc allocatorDesc = {};
	allocatorDesc._device = desc._device;
	allocatorDesc._numDescriptors = desc._rtvCount;
	allocatorDesc._type = rhi::DESCRIPTOR_HEAP_TYPE_RTV;
	_rtvAllocator.initialize(allocatorDesc);
	_rtvAllocator.setName("DescriptorHeapRtv");

	allocatorDesc._numDescriptors = desc._dsvCount;
	allocatorDesc._type = rhi::DESCRIPTOR_HEAP_TYPE_DSV;
	_dsvAllocator.initialize(allocatorDesc);
	_dsvAllocator.setName("DescriptorHeapDsv");

	allocatorDesc._numDescriptors = desc._srvCbvUavCpuCount;
	allocatorDesc._type = rhi::DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	_srvCbvUavCpuAllocator.initialize(allocatorDesc);
	_srvCbvUavCpuAllocator.setName("DescriptorHeapSrvCbvUavCPU");

	allocatorDesc._numDescriptors = desc._srvCbvUavGpuCount;
	allocatorDesc._flags = rhi::DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	_srvCbvUavGpuAllocator.initialize(allocatorDesc);
	_srvCbvUavGpuAllocator.setName("DescriptorHeapSrvCbvUavGPU");
}

void DescriptorAllocatorGroup::terminate() {
	_rtvAllocator.terminate();
	_dsvAllocator.terminate();
	_srvCbvUavCpuAllocator.terminate();
	_srvCbvUavGpuAllocator.terminate();
}

DescriptorAllocatorGroup* DescriptorAllocatorGroup::Get() {
	return &g_descriptorAllocatorGroup;
}
}
