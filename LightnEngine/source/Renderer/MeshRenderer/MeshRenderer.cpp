#include "MeshRenderer.h"
#include <Core/Utility.h>
#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/AssetReloader/PipelineStateReloader.h>
#include <Renderer/MeshRenderer/GeometryResourceManager.h>
#include <Renderer/MeshRenderer/GpuMeshResourceManager.h>
#include <Renderer/MeshRenderer/GpuMeshInstanceManager.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/GpuTextureManager.h>
#include <Renderer/RenderCore/RendererUtility.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/VramUpdater.h>

namespace ltn {
namespace {
MeshRenderer g_meshRenderer;
}

void MeshRenderer::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();

	// GPU カリング
	{
		rhi::DescriptorRange cullingInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		rhi::DescriptorRange viewInfoCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
		rhi::DescriptorRange meshSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);
		rhi::DescriptorRange meshInstanceSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 3, 4);
		rhi::DescriptorRange indirectArgumentOffsetSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);
		rhi::DescriptorRange lodLevelSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);
		rhi::DescriptorRange materialInstanceIndexSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 10);
		rhi::DescriptorRange hizSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, gpu::HIERACHICAL_DEPTH_COUNT, 13);
		rhi::DescriptorRange indirectArgumentUavRange(rhi::DESCRIPTOR_RANGE_TYPE_UAV, 3, 0);
		rhi::DescriptorRange cullingResultUavRange(rhi::DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);

		rhi::RootParameter rootParameters[GpuCullingRootParam::COUNT] = {};
		rootParameters[GpuCullingRootParam::CULLING_INFO].initializeDescriptorTable(1, &cullingInfoCbvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::VIEW_INFO].initializeDescriptorTable(1, &viewInfoCbvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::MESH].initializeDescriptorTable(1, &meshSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::INDIRECT_ARGUMENT_OFFSETS].initializeDescriptorTable(1, &indirectArgumentOffsetSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::INDIRECT_ARGUMENTS].initializeDescriptorTable(1, &indirectArgumentUavRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::LOD_LEVEL].initializeDescriptorTable(1, &lodLevelSrvRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::CULLING_RESULT].initializeDescriptorTable(1, &cullingResultUavRange, rhi::SHADER_VISIBILITY_ALL);
		rootParameters[GpuCullingRootParam::HIZ].initializeDescriptorTable(1, &hizSrvRange, rhi::SHADER_VISIBILITY_ALL);

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

	// コマンドシグネチャ
	{
		rhi::IndirectArgumentDesc argumentDescs[1] = {};
		argumentDescs[0]._type = rhi::INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		rhi::CommandSignatureDesc desc = {};
		desc._device = device;
		desc._byteStride = sizeof(rhi::DrawIndexedArguments);
		desc._argumentDescs = argumentDescs;
		desc._numArgumentDescs = LTN_COUNTOF(argumentDescs);
		_commandSignature.initialize(desc);
	}

	// GPU リソース初期化
	{
		rhi::Device* device = DeviceManager::Get()->getDevice();
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

		desc._flags = rhi::RESOURCE_FLAG_NONE;
		desc._initialState = rhi::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._sizeInByte = rhi::GetConstantBufferAligned(sizeof(gpu::CullingInfo));
		_cullingInfoGpuBuffer.initialize(desc);
		_cullingInfoGpuBuffer.setName("CullingInfo");
	}

	// Descriptor
	{
		DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
		_cullingInfoCbv = descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->allocate();
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

		// CBV
		{
			rhi::ConstantBufferViewDesc desc = {};
			desc._bufferLocation = _cullingInfoGpuBuffer.getGpuVirtualAddress();
			desc._sizeInBytes = _cullingInfoGpuBuffer.getSizeInByte();
			device->createConstantBufferView(desc, _cullingInfoCbv._cpuHandle);
		}
	}
}

void MeshRenderer::terminate() {
	DescriptorAllocatorGroup* descriptorAllocatorGroup = DescriptorAllocatorGroup::Get();
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_cullingInfoCbv);
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_indirectArgumentUav);
	descriptorAllocatorGroup->getSrvCbvUavGpuAllocator()->free(_indirectArgumentSubInfoSrv);
	descriptorAllocatorGroup->getSrvCbvUavCpuAllocator()->free(_indirectArgumentCountCpuUav);

	PipelineStateReloader* pipelineStateReloader = PipelineStateReloader::Get();
	pipelineStateReloader->unregisterPipelineState(&_gpuCullingPipelineState);

	_gpuCullingPipelineState.terminate();
	_gpuCullingRootSignature.terminate();
	_commandSignature.terminate();
	_indirectArgumentGpuBuffer.terminate();
	_indirectArgumentCountGpuBuffer.terminate();
	_indirectArgumentSubInfoGpuBuffer.terminate();
	_cullingInfoGpuBuffer.terminate();
}

void MeshRenderer::update() {
	VramUpdater* vramUpdater = VramUpdater::Get();
	gpu::CullingInfo* cullingInfo = vramUpdater->enqueueUpdate<gpu::CullingInfo>(&_cullingInfoGpuBuffer);
	cullingInfo->_meshInstanceReserveCount = GpuMeshInstanceManager::Get()->getMeshInstanceReserveCount();
}

void MeshRenderer::culling(const CullingDesc& desc) {
	rhi::CommandList* commandList = desc._commandList;
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "GpuCulling");

	ScopedBarrierDesc barriers[] = {
		ScopedBarrierDesc(&_indirectArgumentGpuBuffer, rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(&_indirectArgumentCountGpuBuffer, rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(&_indirectArgumentSubInfoGpuBuffer, rhi::RESOURCE_STATE_UNORDERED_ACCESS)
	};
	ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));

	// Indirect Argument カウントバッファをクリア
	{
		u32 clearValues[4] = {};
		rhi::GpuDescriptorHandle gpuDescriptor = _indirectArgumentUav.get(1)._gpuHandle;
		rhi::CpuDescriptorHandle cpuDescriptor = _indirectArgumentCountCpuUav._cpuHandle;
		rhi::Resource* resource = _indirectArgumentCountGpuBuffer.getResource();
		commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, resource, clearValues, 0, nullptr);
	}

	commandList->setComputeRootSignature(&_gpuCullingRootSignature);
	commandList->setPipelineState(&_gpuCullingPipelineState);
	//gpuCullingResource->resourceBarriersHizTextureToSrv(commandList);

	rhi::GpuDescriptorHandle meshSrv = GpuMeshResourceManager::Get()->getMeshGpuSrv();
	rhi::GpuDescriptorHandle meshInstanceSrv = GpuMeshInstanceManager::Get()->getMeshInstanceGpuSrv();
	rhi::GpuDescriptorHandle indirectArgumentOffsetSrv = GpuMeshInstanceManager::Get()->getSubMeshInstanceOffsetsGpuSrv();

	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::CULLING_INFO, _cullingInfoCbv._gpuHandle);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::VIEW_INFO, desc._viewCbv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::MESH, meshSrv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::MESH_INSTANCE, meshInstanceSrv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::INDIRECT_ARGUMENT_OFFSETS, indirectArgumentOffsetSrv);
	commandList->setComputeRootDescriptorTable(GpuCullingRootParam::INDIRECT_ARGUMENTS, _indirectArgumentUav._firstHandle._gpuHandle);

	u32 meshInstanceReserveCount = desc._meshInstanceReserveCount;
	u32 dispatchCount = RoundDivUp(meshInstanceReserveCount, 128u);
	commandList->dispatch(dispatchCount, 1, 1);
	//gpuCullingResource->resourceBarriersHizSrvToTexture(commandList);
}

void MeshRenderer::render(const RenderDesc& desc) {
	GeometryResourceManager* geometryResourceManager = GeometryResourceManager::Get();
	GpuMeshInstanceManager* gpuMeshInstanceManager = GpuMeshInstanceManager::Get();

	rhi::IndexBufferView indexBufferView = geometryResourceManager->getIndexBufferView();
	rhi::VertexBufferView vertexBufferViews[2];
	vertexBufferViews[0] = geometryResourceManager->getPositionVertexBufferView();
	vertexBufferViews[1] = gpuMeshInstanceManager->getMeshInstanceIndexVertexBufferView();

	rhi::GpuDescriptorHandle textureSrv = GpuTextureManager::Get()->getTextureGpuSrv();
	rhi::GpuDescriptorHandle materialParameterSrv = GpuMaterialManager::Get()->getParameterGpuSrv();
	rhi::GpuDescriptorHandle meshInstanceSrv = GpuMeshInstanceManager::Get()->getMeshInstanceGpuSrv();
	const u32* indirectArgumentCounts = gpuMeshInstanceManager->getSubMeshInstanceCounts();
	const u32* indirectArgumentOffsets = gpuMeshInstanceManager->getSubMeshInstanceOffsets();
	rhi::CommandList* commandList = desc._commandList;
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "Render");

	commandList->setPrimitiveTopology(rhi::PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (u32 i = 0; i < desc._pipelineStateCount; ++i) {
		if (desc._enabledFlags[i] == 0) {
			continue;
		}

		commandList->setGraphicsRootSignature(&desc._rootSignatures[i]);
		commandList->setPipelineState(&desc._pipelineStates[i]);

		commandList->setGraphicsRootDescriptorTable(DefaultRootParam::VIEW_INFO, desc._viewCbv);
		commandList->setGraphicsRootDescriptorTable(DefaultRootParam::MESH_INSTANCE, meshInstanceSrv);
		commandList->setGraphicsRootDescriptorTable(DefaultRootParam::MATERIAL_PARAMETER, materialParameterSrv);
		commandList->setGraphicsRootDescriptorTable(DefaultRootParam::INDIRECT_ARGUMENT_SUB_INFO, _indirectArgumentSubInfoSrv._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(DefaultRootParam::TEXTURE, textureSrv);

		commandList->setVertexBuffers(0, LTN_COUNTOF(vertexBufferViews), vertexBufferViews);
		commandList->setIndexBuffer(&indexBufferView);

		u32 count = indirectArgumentCounts[i];
		u32 offset = sizeof(gpu::IndirectArgument) * indirectArgumentOffsets[i];
		commandList->executeIndirect(&_commandSignature, count, _indirectArgumentGpuBuffer.getResource(), offset, _indirectArgumentCountGpuBuffer.getResource(), 0);
	}
}

MeshRenderer* MeshRenderer::Get() {
	return &g_meshRenderer;
}
}
