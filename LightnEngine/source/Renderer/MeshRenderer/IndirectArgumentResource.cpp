#include "IndirectArgumentResource.h"
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>

namespace ltn {
void IndirectArgumentResource::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// GPU ƒŠƒ\[ƒX‰Šú‰»
	{
		GpuBufferDesc desc = {};
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._device = device;
		desc._sizeInByte = INDIRECT_ARGUMENT_CAPACITY * sizeof(gpu::IndirectArgument);
		desc._initialState = rhi::RESOURCE_STATE_INDIRECT_ARGUMENT;
		desc._flags = rhi::RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		_indirectArgumentGpuBuffer.initialize(desc);
		_indirectArgumentGpuBuffer.setName("IndirectArguments");

		desc._sizeInByte = INDIRECT_ARGUMENT_CAPACITY * sizeof(u32);
		_indirectArgumentCountGpuBuffer.initialize(desc);
		_indirectArgumentCountGpuBuffer.setName("IndirectArgumentCounts");

		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._sizeInByte = INDIRECT_ARGUMENT_CAPACITY * sizeof(gpu::IndirectArgumentSubInfo);
		_indirectArgumentSubInfoGpuBuffer.initialize(desc);
		_indirectArgumentSubInfoGpuBuffer.setName("IndirectArgumentSubInfos");
	}

	// Descriptor
	{
		DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
		_indirectArgumentUav = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate(3);
		_indirectArgumentCountCpuUav = descriptorAllocatorGroup->getSrvCbvUavCpuAllocator()->allocate();
		_indirectArgumentSubInfoSrv = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate();

		// UAV
		{
			rhi::UnorderedAccessViewDesc desc = {};
			desc._viewDimension = rhi::UAV_DIMENSION_BUFFER;
			desc._buffer._numElements = _indirectArgumentGpuBuffer.getU32ElementCount();
			desc._format = rhi::FORMAT_R32_TYPELESS;
			desc._buffer._flags = rhi::BUFFER_UAV_FLAG_RAW;
			device->createUnorderedAccessView(_indirectArgumentGpuBuffer.getResource(), nullptr, &desc, _indirectArgumentUav.get(0)._cpuHandle);

			desc._buffer._numElements = _indirectArgumentCountGpuBuffer.getU32ElementCount();
			device->createUnorderedAccessView(_indirectArgumentCountGpuBuffer.getResource(), nullptr, &desc, _indirectArgumentUav.get(1)._cpuHandle);
			device->createUnorderedAccessView(_indirectArgumentCountGpuBuffer.getResource(), nullptr, &desc, _indirectArgumentCountCpuUav._cpuHandle);

			desc._buffer._numElements = _indirectArgumentSubInfoGpuBuffer.getU32ElementCount();
			device->createUnorderedAccessView(_indirectArgumentSubInfoGpuBuffer.getResource(), nullptr, &desc, _indirectArgumentUav.get(2)._cpuHandle);
		}

		// SRV
		{
			rhi::ShaderResourceViewDesc desc = {};
			desc._format = rhi::FORMAT_R32_TYPELESS;
			desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
			desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
			desc._buffer._numElements = _indirectArgumentSubInfoGpuBuffer.getU32ElementCount();
			device->createShaderResourceView(_indirectArgumentSubInfoGpuBuffer.getResource(), &desc, _indirectArgumentSubInfoSrv._cpuHandle);
		}
	}
}
void IndirectArgumentResource::terminate() {
	_indirectArgumentGpuBuffer.terminate();
	_indirectArgumentCountGpuBuffer.terminate();
	_indirectArgumentSubInfoGpuBuffer.terminate();

	{
		DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
		descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_indirectArgumentUav);
		descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_indirectArgumentSubInfoSrv);
		descriptorAllocatorGroup->getSrvCbvUavCpuAllocator()->free(_indirectArgumentCountCpuUav);
	}
}
}
