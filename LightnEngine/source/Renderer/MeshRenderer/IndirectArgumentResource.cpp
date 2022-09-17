#include "IndirectArgumentResource.h"
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/FrameResourceAllocator.h>
#include <RendererScene/MeshGeometry.h>
#include <RendererScene/Material.h>

namespace ltn {
void IndirectArgumentResource::setUpFrameResource(rhi::CommandList* commandList) {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// GPU リソース初期化
	{
		FrameBufferAllocator* frameBufferAllocator = FrameBufferAllocator::Get();
		FrameBufferAllocator::BufferCreatationDesc desc = {};
		desc._initialState = rhi::RESOURCE_STATE_INDIRECT_ARGUMENT;
		desc._sizeInByte = INDIRECT_ARGUMENT_CAPACITY * sizeof(gpu::IndirectArgument);
		desc._flags = rhi::RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		_indirectArgumentGpuBuffer = frameBufferAllocator->createGpuBuffer(desc);
		_indirectArgumentGpuBuffer->setName("IndirectArguments");

		desc._sizeInByte = PipelineSetScene::PIPELINE_SET_CAPACITY * sizeof(u32);
		_indirectArgumentCountGpuBuffer = frameBufferAllocator->createGpuBuffer(desc);
		_indirectArgumentCountGpuBuffer->setName("IndirectArgumentCounts");

		desc._initialState = rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		desc._sizeInByte = MeshGeometryScene::SUB_MESH_GEOMETRY_CAPACITY * sizeof(u32);
		_subMeshDrawCountGpuBuffer = frameBufferAllocator->createGpuBuffer(desc);
		_subMeshDrawCountGpuBuffer->setName("SubMeshDrawCounts");

		desc._sizeInByte = INDIRECT_ARGUMENT_CAPACITY * sizeof(gpu::IndirectArgumentSubInfo);
		_indirectArgumentSubInfoGpuBuffer = frameBufferAllocator->createGpuBuffer(desc);
		_indirectArgumentSubInfoGpuBuffer->setName("IndirectArgumentSubInfos");
	}

	// Descriptor
	{
		FrameDescriptorAllocator* descriptorAllocator = FrameDescriptorAllocator::Get();
		_indirectArgumentUav = descriptorAllocator->allocateSrvCbvUavGpu(2);
		_subIndirectArgumentUav = descriptorAllocator->allocateSrvCbvUavGpu(2);
		_subMeshDrawCountCpuUav = descriptorAllocator->allocateSrvCbvUavCpu();
		_indirectArgumentCountCpuUav = descriptorAllocator->allocateSrvCbvUavCpu();
		_indirectArgumentSubInfoSrv = descriptorAllocator->allocateSrvCbvUavGpu();
		_subMeshDrawCountSrv = descriptorAllocator->allocateSrvCbvUavGpu();

		// UAV
		{
			rhi::UnorderedAccessViewDesc desc = {};
			desc._viewDimension = rhi::UAV_DIMENSION_BUFFER;
			desc._buffer._numElements = _indirectArgumentGpuBuffer->getU32ElementCount();
			desc._format = rhi::FORMAT_R32_TYPELESS;
			desc._buffer._flags = rhi::BUFFER_UAV_FLAG_RAW;
			desc._buffer._numElements = _subMeshDrawCountGpuBuffer->getU32ElementCount();
			device->createUnorderedAccessView(_subMeshDrawCountGpuBuffer->getResource(), nullptr, &desc, _subIndirectArgumentUav.get(0)._cpuHandle);
			device->createUnorderedAccessView(_subMeshDrawCountGpuBuffer->getResource(), nullptr, &desc, _subMeshDrawCountCpuUav._cpuHandle);

			desc._buffer._numElements = _indirectArgumentSubInfoGpuBuffer->getU32ElementCount();
			device->createUnorderedAccessView(_indirectArgumentSubInfoGpuBuffer->getResource(), nullptr, &desc, _subIndirectArgumentUav.get(1)._cpuHandle);

			desc._buffer._numElements = _indirectArgumentGpuBuffer->getU32ElementCount();
			device->createUnorderedAccessView(_indirectArgumentGpuBuffer->getResource(), nullptr, &desc, _indirectArgumentUav.get(0)._cpuHandle);

			desc._buffer._numElements = _indirectArgumentCountGpuBuffer->getU32ElementCount();
			device->createUnorderedAccessView(_indirectArgumentCountGpuBuffer->getResource(), nullptr, &desc, _indirectArgumentUav.get(1)._cpuHandle);
			device->createUnorderedAccessView(_indirectArgumentCountGpuBuffer->getResource(), nullptr, &desc, _indirectArgumentCountCpuUav._cpuHandle);
		}

		// SRV
		{
			rhi::ShaderResourceViewDesc desc = {};
			desc._format = rhi::FORMAT_R32_TYPELESS;
			desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
			desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
			desc._buffer._numElements = _indirectArgumentSubInfoGpuBuffer->getU32ElementCount();
			device->createShaderResourceView(_indirectArgumentSubInfoGpuBuffer->getResource(), &desc, _indirectArgumentSubInfoSrv._cpuHandle);

			desc._buffer._numElements = _subMeshDrawCountGpuBuffer->getU32ElementCount();
			device->createShaderResourceView(_subMeshDrawCountGpuBuffer->getResource(), &desc,_subMeshDrawCountSrv._cpuHandle);
		}
	}
	
	// エイリアシングバリア
	//rhi::ResourceAliasingBarrier aliasingBarriers[3];
	//aliasingBarriers[0]._resourceAfter = _indirectArgumentGpuBuffer->getResource();
	//aliasingBarriers[1]._resourceAfter = _indirectArgumentCountGpuBuffer->getResource();
	//aliasingBarriers[2]._resourceAfter = _indirectArgumentSubInfoGpuBuffer->getResource();
	//commandList->aliasingBarriers(aliasingBarriers, LTN_COUNTOF(aliasingBarriers));
}
}
