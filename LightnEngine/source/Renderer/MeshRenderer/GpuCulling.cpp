#include "GpuCulling.h"
#include <Core/Utility.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/AssetReloader/PipelineStateReloader.h>
#include <Renderer/MeshRenderer/GeometryResourceManager.h>
#include <Renderer/MeshRenderer/GpuMeshResourceManager.h>
#include <Renderer/MeshRenderer/GpuMeshInstanceManager.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/GpuTextureManager.h>
#include <Renderer/MeshRenderer/IndirectArgumentResource.h>
#include <Renderer/RenderCore/RendererUtility.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/LodStreamingManager.h>

namespace ltn {
namespace {
GpuCulling g_gpuCulling;
}

void GpuCulling::initialize() {
	CpuScopedPerf scopedPerf("GpuCulling");
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// GPU カリング
	{
		rhi::DescriptorRange cullingInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		rhi::DescriptorRange viewInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
		rhi::DescriptorRange meshSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);
		rhi::DescriptorRange meshInstanceSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 3, 4);
		rhi::DescriptorRange indirectArgumentOffsetSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);
		rhi::DescriptorRange geometryGlobalOffsetSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);
		rhi::DescriptorRange lodLevelSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 9);
		rhi::DescriptorRange meshLodStreamRangeSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 10);
		//rhi::DescriptorRange hizSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, gpu::HIERACHICAL_DEPTH_COUNT, 13);
		rhi::DescriptorRange indirectArgumentUavRange(rhi::DESCRIPTOR_RANGE_TYPE_UAV, 3, 0);
		rhi::DescriptorRange cullingResultUavRange(rhi::DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);

		rhi::RootParameter rootParameters[GpuCullingRootParam::COUNT] = {};
		rootParameters[GpuCullingRootParam::CULLING_INFO].initializeDescriptorTable(1, &cullingInfoCbvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::VIEW_INFO].initializeDescriptorTable(1, &viewInfoCbvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::MESH].initializeDescriptorTable(1, &meshSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::INDIRECT_ARGUMENT_OFFSET].initializeDescriptorTable(1, &indirectArgumentOffsetSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::INDIRECT_ARGUMENTS].initializeDescriptorTable(1, &indirectArgumentUavRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::GEOMETRY_GLOBA_OFFSET].initializeDescriptorTable(1, &geometryGlobalOffsetSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::MESH_INSTANCE_LOD_LEVEL].initializeDescriptorTable(1, &lodLevelSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::MESH_LOD_STREAM_RANGE].initializeDescriptorTable(1, &meshLodStreamRangeSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::CULLING_RESULT].initializeDescriptorTable(1, &cullingResultUavRange, rhi::SHADER_VISIBILITY_ALL);
		//rootParameters[GpuCullingRootParam::HIZ].initializeDescriptorTable(1, &hizSrvRange, rhi::SHADER_VISIBILITY_ALL);

		rhi::RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_gpuCullingRootSignature.iniaitlize(rootSignatureDesc);

		AssetPath shaderPath("EngineComponent\\Shader\\MeshRenderer\\MeshCulling.cso");
		rhi::ShaderBlob shader;
		shader.initialize(shaderPath.get());

		rhi::ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._rootSignature = &_gpuCullingRootSignature;
		pipelineStateDesc._cs = shader.getShaderByteCode();
		_gpuCullingPipelineState.iniaitlize(pipelineStateDesc);

		//PipelineStateReloader::ComputePipelineStateRegisterDesc reloaderDesc = {};
		//reloaderDesc._desc = pipelineStateDesc;
		//reloaderDesc._shaderPathHash = shaderPath.get();
		//PipelineStateReloader::Get()->registerPipelineState(&_gpuCullingPipelineState, reloaderDesc);

		shader.terminate();
	}

	{
		rhi::Device* device = DeviceManager::Get()->getDevice();
		GpuBufferDesc desc = {};
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._device = device;
		desc._flags = rhi::RESOURCE_FLAG_NONE;
		desc._initialState = rhi::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._sizeInByte = rhi::GetConstantBufferAligned(sizeof(gpu::CullingInfo));
		_cullingInfoGpuBuffer.initialize(desc);
		_cullingInfoGpuBuffer.setName("CullingInfo");
	}

	{
		DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
		_cullingInfoCbv = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate();

		// CBV
		{
			rhi::ConstantBufferViewDesc desc = {};
			desc._bufferLocation = _cullingInfoGpuBuffer.getGpuVirtualAddress();
			desc._sizeInBytes = _cullingInfoGpuBuffer.getSizeInByte();
			device->createConstantBufferView(desc, _cullingInfoCbv._cpuHandle);
		}
	}
}

void GpuCulling::terminate() {
	//PipelineStateReloader* pipelineStateReloader = PipelineStateReloader::Get();
	//pipelineStateReloader->unregisterPipelineState(&_gpuCullingPipelineState);

	_gpuCullingPipelineState.terminate();
	_gpuCullingRootSignature.terminate();

	_cullingInfoGpuBuffer.terminate();

	DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_cullingInfoCbv);
}

void GpuCulling::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	gpu::CullingInfo* cullingInfo = vramUpdater->enqueueUpdate<gpu::CullingInfo>(&_cullingInfoGpuBuffer);
	cullingInfo->_meshInstanceReserveCount = GpuMeshInstanceManager::Get()->getMeshInstanceReserveCount();
}

void GpuCulling::gpuCulling(const CullingDesc& desc) {
	rhi::CommandList* commandList = desc._commandList;
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "GpuCulling");

	IndirectArgumentResource* indirectArgumentResource = desc._indirectArgumentResource;
	LodStreamingManager* lodStreamingManager = LodStreamingManager::Get();
	ScopedBarrierDesc barriers[] = {
		ScopedBarrierDesc(&indirectArgumentResource->_indirectArgumentGpuBuffer, rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(&indirectArgumentResource->_indirectArgumentCountGpuBuffer, rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(&indirectArgumentResource->_indirectArgumentSubInfoGpuBuffer, rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(lodStreamingManager->getMeshInstanceLodLevelGpuBuffer(), rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
	};
	ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));

	// Indirect Argument カウントバッファをクリア
	{
		u32 clearValues[4] = {};
		rhi::GpuDescriptorHandle gpuDescriptor = indirectArgumentResource->_indirectArgumentUav.get(1)._gpuHandle;
		rhi::CpuDescriptorHandle cpuDescriptor = indirectArgumentResource->_indirectArgumentCountCpuUav._cpuHandle;
		rhi::Resource* resource = indirectArgumentResource->_indirectArgumentCountGpuBuffer.getResource();
		commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, resource, clearValues, 0, nullptr);
	}

	commandList->setComputeRootSignature(&_gpuCullingRootSignature);
	commandList->setPipelineState(&_gpuCullingPipelineState);
	//gpuCullingResource->resourceBarriersHizTextureToSrv(commandList);

	rhi::GpuDescriptorHandle geometryGlobalOffsetSrv = GeometryResourceManager::Get()->getGeometryGlobalOffsetGpuSrv();
	rhi::GpuDescriptorHandle meshLodStreamRangeSrv = GeometryResourceManager::Get()->getMeshLodStreamRangeGpuSrv();
	rhi::GpuDescriptorHandle meshSrv = GpuMeshResourceManager::Get()->getMeshGpuSrv();
	rhi::GpuDescriptorHandle meshInstanceSrv = GpuMeshInstanceManager::Get()->getMeshInstanceGpuSrv();
	rhi::GpuDescriptorHandle indirectArgumentOffsetSrv = GpuMeshInstanceManager::Get()->getSubMeshInstanceOffsetsGpuSrv();
	rhi::GpuDescriptorHandle meshInstanceLodLevelSrv = lodStreamingManager->getMeshInstanceLodLevelGpuSrv();

	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::CULLING_INFO, _cullingInfoCbv._gpuHandle);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::VIEW_INFO, desc._viewCbv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::MESH, meshSrv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::MESH_INSTANCE, meshInstanceSrv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::INDIRECT_ARGUMENT_OFFSET, indirectArgumentOffsetSrv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::INDIRECT_ARGUMENTS, indirectArgumentResource->_indirectArgumentUav._firstHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::GEOMETRY_GLOBA_OFFSET, geometryGlobalOffsetSrv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::MESH_INSTANCE_LOD_LEVEL, meshInstanceLodLevelSrv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::MESH_LOD_STREAM_RANGE, meshLodStreamRangeSrv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::CULLING_RESULT, desc._cullingResultUav);

	u32 meshInstanceReserveCount = GpuMeshInstanceManager::Get()->getMeshInstanceReserveCount();
	u32 dispatchCount = RoundDivUp(meshInstanceReserveCount, 128u);
	commandList->dispatch(dispatchCount, 1, 1);
	//gpuCullingResource->resourceBarriersHizSrvToTexture(commandList);
}

GpuCulling* GpuCulling::Get() {
	return &g_gpuCulling;
}
}
