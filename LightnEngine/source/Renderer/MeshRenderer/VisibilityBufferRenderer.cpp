#include <Renderer/RenderCore/DeviceManager.h>
#include <Renderer/RenderCore/VramUpdater.h>
#include <Renderer/RenderCore/GlobalVideoMemoryAllocator.h>
#include <Renderer/RenderCore/RendererUtility.h>
#include <Renderer/RenderCore/LodStreamingManager.h>
#include <Renderer/RenderCore/FrameResourceAllocator.h>
#include <Renderer/MeshRenderer/GpuMaterialManager.h>
#include <Renderer/MeshRenderer/GeometryResourceManager.h>
#include <Renderer/MeshRenderer/GpuMeshResourceManager.h>
#include <Renderer/MeshRenderer/GpuMeshInstanceManager.h>
#include <Renderer/MeshRenderer/GpuTextureManager.h>
#include <Renderer/MeshRenderer/IndirectArgumentResource.h>
#include <Renderer/AssetReloader/PipelineStateReloader.h>
#include <RendererScene/View.h>
#include <Application/Application.h>
#include "VisibilityBufferRenderer.h"

namespace ltn {
namespace {
VisiblityBufferRenderer g_visibilityBufferRenderer;
}

void VisiblityBufferRenderer::initialize() {
	rhi::Device* device = DeviceManager::Get()->getDevice();
	Application* app = ApplicationSysytem::Get()->getApplication();

	u32 screenWidth = app->getScreenWidth();
	u32 screenHeight = app->getScreenHeight();
	u32 shaderRangeWidth = u64(screenWidth / SHADER_RANGE_TILE_SIZE) + 1;
	u32 shaderRangeHeight = u64(screenHeight / SHADER_RANGE_TILE_SIZE) + 1;
	_shadingQuadCount = shaderRangeWidth * shaderRangeHeight;

	// Constant buffer
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._allocator = GlobalVideoMemoryAllocator::Get()->getAllocator();
		desc._sizeInByte = rhi::GetConstantBufferAligned(sizeof(BuildShaderIdConstant));
		desc._initialState = rhi::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		_buildShaderIdConstantBuffer.initialize(desc);
		_buildShaderIdConstantBuffer.setName("VisibilityBufferBuildShaderIdConstant");

		desc._sizeInByte = rhi::GetConstantBufferAligned(sizeof(ShadingConstant));
		_shadingConstantBuffer.initialize(desc);
		_shadingConstantBuffer.setName("VisibilityBufferShadingConstant");

		VramUpdater* vramUpdater = VramUpdater::Get();
		{
			BuildShaderIdConstant* constant = vramUpdater->enqueueUpdate<BuildShaderIdConstant>(&_buildShaderIdConstantBuffer, 0, 1);
			constant->_resolution[0] = shaderRangeWidth;
			constant->_resolution[1] = shaderRangeHeight;
		}

		{
			ShadingConstant* constant = vramUpdater->enqueueUpdate<ShadingConstant>(&_shadingConstantBuffer, 0, 1);
			constant->_quadNdcSize[0] = f32(SHADER_RANGE_TILE_SIZE) / screenWidth;
			constant->_quadNdcSize[1] = f32(SHADER_RANGE_TILE_SIZE) / screenHeight;
			constant->_shaderRangeResolution[0] = shaderRangeWidth;
			constant->_shaderRangeResolution[1] = shaderRangeHeight;
		}
	}

	// デスクリプター
	{
		// GPU visible
		{
			DescriptorAllocatorGroup* allocator = DescriptorAllocatorGroup::Get();
			_buildShaderIdCbv = allocator->allocateSrvCbvUavGpu();
			_shadingCbv = allocator->allocateSrvCbvUavGpu();
		}

		// CBV
		{
			rhi::ConstantBufferViewDesc desc;
			desc._bufferLocation = _buildShaderIdConstantBuffer.getGpuVirtualAddress();
			desc._sizeInBytes = _buildShaderIdConstantBuffer.getSizeInByte();
			device->createConstantBufferView(desc, _buildShaderIdCbv._cpuHandle);

			desc._bufferLocation = _shadingConstantBuffer.getGpuVirtualAddress();
			desc._sizeInBytes = _shadingConstantBuffer.getSizeInByte();
			device->createConstantBufferView(desc, _shadingCbv._cpuHandle);
		}
	}

	// Shader
	{
		// root signature
		{
			rhi::DescriptorRange constantCbvRange(rhi::DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			rhi::DescriptorRange triangleIdSrvRange(rhi::DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			rhi::DescriptorRange shaderRangeUavRange(rhi::DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);

			rhi::RootParameter rootParameters[VisibilityBufferBuildShaderIdRootParam::COUNT] = {};
			rootParameters[VisibilityBufferBuildShaderIdRootParam::CONSTANT].initializeDescriptorTable(1, &constantCbvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[VisibilityBufferBuildShaderIdRootParam::TRIANGLE_ID].initializeDescriptorTable(1, &triangleIdSrvRange, rhi::SHADER_VISIBILITY_PIXEL);
			rootParameters[VisibilityBufferBuildShaderIdRootParam::SHADER_RANGE].initializeDescriptorTable(1, &shaderRangeUavRange, rhi::SHADER_VISIBILITY_PIXEL);

			rhi::RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;
			rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
			rootSignatureDesc._parameters = rootParameters;
			_buildShaderIdRootSignature.iniaitlize(rootSignatureDesc);
			_buildShaderIdRootSignature.setName("VisibilityBufferBuildShaderIDRootSignature");
		}

		// pipeline state
		{
			AssetPath vertexShaderPath("EngineComponent\\Shader\\ScreenTriangle.vso");
			rhi::ShaderBlob vertexShader;
			vertexShader.initialize(vertexShaderPath.get());

			AssetPath pixelShaderPath("EngineComponent\\Shader\\VisibilityBuffer\\BuildShaderId.pso");
			rhi::ShaderBlob pixelShader;
			pixelShader.initialize(pixelShaderPath.get());

			rhi::GraphicsPipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._vs = vertexShader.getShaderByteCode();
			pipelineStateDesc._ps = pixelShader.getShaderByteCode();
			pipelineStateDesc._dsvFormat = rhi::FORMAT_D16_UNORM;
			pipelineStateDesc._depthComparisonFunc = rhi::COMPARISON_FUNC_ALWAYS;
			pipelineStateDesc._topologyType = rhi::PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateDesc._rootSignature = &_buildShaderIdRootSignature;
			pipelineStateDesc._sampleDesc._count = 1;
			_buildShaderIdPipelineState.iniaitlize(pipelineStateDesc);
			_buildShaderIdPipelineState.setName("BuildShaderIdPSO");

			vertexShader.terminate();
			pixelShader.terminate();
		}
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
}

void VisiblityBufferRenderer::terminate() {
	_shadingConstantBuffer.terminate();
	_buildShaderIdConstantBuffer.terminate();
	_buildShaderIdPipelineState.terminate();
	_buildShaderIdRootSignature.terminate();

	_commandSignature.terminate();

	{
		DescriptorAllocatorGroup* allocator = DescriptorAllocatorGroup::Get();
		allocator->freeSrvCbvUavGpu(_buildShaderIdCbv);
		allocator->freeSrvCbvUavGpu(_shadingCbv);
	}
}

void VisiblityBufferRenderer::buildShaderId(const BuildShaderIdDesc& desc) {
	rhi::CommandList* commandList = desc._commandList;
	VisibilityBufferFrameResource* frameResource = desc._frameResource;
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "Build Shader Id");

	ScopedBarrierDesc barriers[] = {
		ScopedBarrierDesc(frameResource->_triangleIdTexture, rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(frameResource->_triangleShaderIdTexture, rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(frameResource->_shaderRangeBuffer[0], rhi::RESOURCE_STATE_UNORDERED_ACCESS),
		ScopedBarrierDesc(frameResource->_shaderRangeBuffer[1], rhi::RESOURCE_STATE_UNORDERED_ACCESS),
	};
	ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));

	// clear shader range min
	{
		u32 clearValues[4] = { UINT32_MAX };
		rhi::GpuDescriptorHandle gpuHandle = frameResource->_shaderRangeUav.get(0)._gpuHandle;
		rhi::CpuDescriptorHandle cpuHandle = frameResource->_shaderRangeCpuUav.get(0)._cpuHandle;
		commandList->clearUnorderedAccessViewUint(gpuHandle, cpuHandle, frameResource->_shaderRangeBuffer[0]->getResource(), clearValues, 0, nullptr);
	}

	// clear shader range max
	{
		u32 clearValues[4] = { 0 };
		rhi::GpuDescriptorHandle gpuHandle = frameResource->_shaderRangeUav.get(1)._gpuHandle;
		rhi::CpuDescriptorHandle cpuHandle = frameResource->_shaderRangeCpuUav.get(1)._cpuHandle;
		commandList->clearUnorderedAccessViewUint(gpuHandle, cpuHandle, frameResource->_shaderRangeBuffer[1]->getResource(), clearValues, 0, nullptr);
	}

	commandList->setPrimitiveTopology(rhi::PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->setRenderTargets(0, nullptr, &frameResource->_shaderIdDsv._cpuHandle);
	commandList->setGraphicsRootSignature(&_buildShaderIdRootSignature);
	commandList->setPipelineState(&_buildShaderIdPipelineState);
	commandList->setGraphicsRootDescriptorTable(VisibilityBufferBuildShaderIdRootParam::CONSTANT, _buildShaderIdCbv._gpuHandle);
	commandList->setGraphicsRootDescriptorTable(VisibilityBufferBuildShaderIdRootParam::TRIANGLE_ID, frameResource->_shaderIdSrv._gpuHandle);
	commandList->setGraphicsRootDescriptorTable(VisibilityBufferBuildShaderIdRootParam::SHADER_RANGE, frameResource->_shaderRangeUav._firstHandle._gpuHandle);
	commandList->drawInstanced(3, 1, 0, 0);
}

void VisiblityBufferRenderer::shadingPass(const ShadingPassDesc& desc) {
	rhi::CommandList* commandList = desc._commandList;
	VisibilityBufferFrameResource* frameResource = desc._frameResource;
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "Shading Pass");

	GeometryResourceManager* geometryResourceManager = GeometryResourceManager::Get();
	LodStreamingManager* lodStreamingManager = LodStreamingManager::Get();
	ScopedBarrierDesc barriers[] = {
		ScopedBarrierDesc(frameResource->_shaderIdDepth, rhi::RESOURCE_STATE_DEPTH_READ),
		ScopedBarrierDesc(frameResource->_triangleIdTexture, rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(frameResource->_baryCentricsGpuTexture, rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(geometryResourceManager->getPositionVertexGpuBuffer(), rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(geometryResourceManager->getTexcoordVertexGpuBuffer(), rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(geometryResourceManager->getNormalTangentVertexGpuBuffer(), rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(geometryResourceManager->getIndexGpuBuffer(), rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(geometryResourceManager->getGeometryGlobalOffsetGpuBuffer(), rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(lodStreamingManager->getMeshInstanceLodLevelGpuBuffer(), rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		ScopedBarrierDesc(desc._viewDepthGpuTexture, rhi::RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
	};
	ScopedBarrier scopedBarriers(commandList, barriers, LTN_COUNTOF(barriers));

	rhi::GpuDescriptorHandle vertexResourceSrv = geometryResourceManager->getVertexResourceGpuSrv();
	rhi::GpuDescriptorHandle geometryGlobalOffsetSrv = geometryResourceManager->getGeometryGlobalOffsetGpuSrv();
	rhi::GpuDescriptorHandle meshInstanceLodLevelSrv = lodStreamingManager->getMeshInstanceLodLevelGpuSrv();
	rhi::GpuDescriptorHandle meshInstanceScreenPersentageSrv = lodStreamingManager->getMeshInstanceScreenPersentageGpuSrv();
	rhi::GpuDescriptorHandle meshLodStreamedLevelSrv = geometryResourceManager->getMeshLodStreamRangeGpuSrv();
	rhi::GpuDescriptorHandle materialScreenPersentageSrv = lodStreamingManager->getMaterialScreenPersentageGpuSrv();
	rhi::GpuDescriptorHandle meshInstanceSrv = GpuMeshInstanceManager::Get()->getMeshInstanceGpuSrv();
	rhi::GpuDescriptorHandle meshSrv = GpuMeshResourceManager::Get()->getMeshGpuSrv();
	rhi::GpuDescriptorHandle textureSrv = GpuTextureManager::Get()->getTextureGpuSrv();
	rhi::GpuDescriptorHandle materialParameterSrv = GpuMaterialManager::Get()->getParameterGpuSrv();
	rhi::GpuDescriptorHandle materialParameterOffsetSrv = GpuMaterialManager::Get()->getParameterOffsetGpuSrv();

	rhi::CpuDescriptorHandle rtv = desc._viewRtv;
	f32 clearColor[4] = {};
	commandList->setRenderTargets(1, &rtv, &frameResource->_shaderIdDsv._cpuHandle);
	commandList->clearRenderTargetView(rtv, clearColor);
	commandList->setPrimitiveTopology(rhi::PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (u32 i = 0; i < desc._pipelineStateCount; ++i) {
		if (desc._enabledFlags[i] == 0) {
			continue;
		}

		DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "Shading Shader %d", i);

		commandList->setGraphicsRootSignature(&desc._rootSignatures[i]);
		commandList->setPipelineState(&desc._pipelineStates[i]);

		commandList->setGraphicsRoot32BitConstants(ShadingRootParam::PIPELINE_SET_INFO, 1, &i, 0);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::VIEW_INFO, desc._viewCbv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::VIEW_DEPTH, desc._viewDepthSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::SHADING_INFO, _shadingCbv._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::PIPELINE_SET_RANGE, frameResource->_shaderRangeSrv._firstHandle._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::TRIANGLE_ATTRIBUTE, frameResource->_triangleIdSrv._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::BARY_CENTRICS, frameResource->_baryCentricsSrv._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::VERTEX_RESOURCE, vertexResourceSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MESH_INSTANCE, meshInstanceSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MESH, meshSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MESH_INSTANCE_LOD_LEVEL, meshInstanceLodLevelSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MESH_INSTANCE_SCREEN_PERSENTAGE, meshInstanceScreenPersentageSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MESH_LOD_STREAMED_LEVEL, meshLodStreamedLevelSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MATERIAL_SCREEN_PERSENTAGE, materialScreenPersentageSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::GEOMETRY_GLOBAL_OFFSET, geometryGlobalOffsetSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::TEXTURE, textureSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MATERIAL_PARAMETER, materialParameterSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MATERIAL_PARAMETER_OFFSET, materialParameterOffsetSrv);
		commandList->setGraphicsRoot32BitConstants(ShadingRootParam::DEBUG_TYPE, 1, &desc._debugVisualizeType, 0);

		commandList->drawInstanced(6, _shadingQuadCount, 0, 0);
	}
}

void VisiblityBufferRenderer::clearShaderId(const ClearShaderIdDesc& desc) {
	rhi::CommandList* commandList = desc._commandList;
	VisibilityBufferFrameResource* frameResource = desc._frameResource;
	{
		f32 clearColor[4] = {};
		commandList->clearRenderTargetView(frameResource->_triangleIdRtv.get(0)._cpuHandle, clearColor);
	}

	{
		f32 clearColor[4] = { UINT8_MAX };
		commandList->clearRenderTargetView(frameResource->_triangleIdRtv.get(1)._cpuHandle, clearColor);
	}
	commandList->clearDepthStencilView(frameResource->_shaderIdDsv._cpuHandle, rhi::CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void VisiblityBufferRenderer::geometryPass(const GeometryPassDesc& desc) {
	GeometryResourceManager* geometryResourceManager = GeometryResourceManager::Get();
	GpuMeshInstanceManager* gpuMeshInstanceManager = GpuMeshInstanceManager::Get();
	LodStreamingManager* lodStreamingManager = LodStreamingManager::Get();
	VisibilityBufferFrameResource* frameResource = desc._frameResource;

	rhi::IndexBufferView indexBufferView = geometryResourceManager->getIndexBufferView();
	rhi::VertexBufferView vertexBufferViews[4];
	vertexBufferViews[0] = geometryResourceManager->getPositionVertexBufferView();
	vertexBufferViews[1] = geometryResourceManager->getNormalTangentVertexBufferView();
	vertexBufferViews[2] = geometryResourceManager->getTexcoordVertexBufferView();
	vertexBufferViews[3] = gpuMeshInstanceManager->getSubMeshInstanceIndexVertexBufferView();

	IndirectArgumentResource* indirectArgumentResource = desc._indirectArgumentResource;
	rhi::GpuDescriptorHandle indirectArgumentSubInfoSrv = indirectArgumentResource->_indirectArgumentSubInfoSrv._gpuHandle;
	rhi::GpuDescriptorHandle meshInstanceSrv = GpuMeshInstanceManager::Get()->getMeshInstanceGpuSrv();
	const u32* indirectArgumentCounts = gpuMeshInstanceManager->getPipelineSetSubMeshInstanceCounts();
	const u32* indirectArgumentOffsets = gpuMeshInstanceManager->getPipelineSetSubMeshInstanceOffsets();
	rhi::CommandList* commandList = desc._commandList;
	rhi::Resource* indirectArgumentBuffer = indirectArgumentResource->_indirectArgumentGpuBuffer->getResource();
	rhi::Resource* indirectArgumentCountBuffer = indirectArgumentResource->_indirectArgumentCountGpuBuffer->getResource();
	DEBUG_MARKER_CPU_GPU_SCOPED_TIMER(commandList, Color4(), "GeometryPass");

	rhi::CpuDescriptorHandle dsv = desc._viewDsv;
	commandList->setRenderTargets(3, frameResource->_triangleIdRtv._firstHandle._cpuHandle, &dsv);
	commandList->clearDepthStencilView(dsv, rhi::CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList->setPrimitiveTopology(rhi::PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (u32 i = 0; i < desc._pipelineStateCount; ++i) {
		if (desc._enabledFlags[i] == 0) {
			continue;
		}

		commandList->setGraphicsRootSignature(&desc._rootSignatures[i]);
		commandList->setPipelineState(&desc._pipelineStates[i]);

		commandList->setGraphicsRootDescriptorTable(GeometryRootParam::VIEW_INFO, desc._viewCbv);
		commandList->setGraphicsRootDescriptorTable(GeometryRootParam::MESH_INSTANCE, meshInstanceSrv);
		commandList->setGraphicsRootDescriptorTable(GeometryRootParam::INDIRECT_ARGUMENT_SUB_INFO, indirectArgumentSubInfoSrv);

		commandList->setVertexBuffers(0, LTN_COUNTOF(vertexBufferViews), vertexBufferViews);
		commandList->setIndexBuffer(&indexBufferView);

		u32 count = MeshGeometryScene::SUB_MESH_GEOMETRY_CAPACITY;
		u32 offset = sizeof(gpu::IndirectArgument) * i;
		commandList->executeIndirect(&_commandSignature, count, indirectArgumentBuffer, offset, indirectArgumentCountBuffer, 0);
	}
}

VisiblityBufferRenderer* VisiblityBufferRenderer::Get() {
	return &g_visibilityBufferRenderer;
}

void VisibilityBufferFrameResource::setUpFrameResource(rhi::CommandList* commandList) {
	FrameBufferAllocator* frameBufferAllocator = FrameBufferAllocator::Get();
	rhi::Device* device = DeviceManager::Get()->getDevice();
	Application* app = ApplicationSysytem::Get()->getApplication();

	u32 screenWidth = app->getScreenWidth();
	u32 screenHeight = app->getScreenHeight();
	u32 shaderRangeWidth = u64(screenWidth / VisiblityBufferRenderer::SHADER_RANGE_TILE_SIZE) + 1;
	u32 shaderRangeHeight = u64(screenHeight / VisiblityBufferRenderer::SHADER_RANGE_TILE_SIZE) + 1;
	u32 shadingQuadCount = shaderRangeWidth * shaderRangeHeight;

	// Texture
	{
		FrameBufferAllocator::TextureCreatationDesc desc = {};
		desc._format = rhi::FORMAT_R32G32_UINT;
		desc._width = u64(screenWidth);
		desc._height = u64(screenHeight);
		desc._flags = rhi::RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		desc._initialState = rhi::RESOURCE_STATE_RENDER_TARGET;
		{
			rhi::ClearValue optimizedClearValue = {};
			optimizedClearValue._format = desc._format;
			optimizedClearValue._color[0] = 0.0f;
			desc._optimizedClearValue = &optimizedClearValue;

			_triangleIdTexture = frameBufferAllocator->createGpuTexture(desc);
			_triangleIdTexture->setName("VisiblityBufferTriangleId");
		}

		{
			desc._format = rhi::FORMAT_R8_UINT;
			rhi::ClearValue optimizedClearValue = {};
			optimizedClearValue._format = desc._format;
			optimizedClearValue._color[0] = f32(UINT8_MAX);

			desc._optimizedClearValue = &optimizedClearValue;
			_triangleShaderIdTexture = frameBufferAllocator->createGpuTexture(desc);
			_triangleShaderIdTexture->setName("VisiblityBufferTriangleShaderId");
		}

		{
			desc._format = rhi::FORMAT_R16G16_UNORM;
			desc._optimizedClearValue = nullptr;
			_baryCentricsGpuTexture = frameBufferAllocator->createGpuTexture(desc);
			_baryCentricsGpuTexture->setName("VisiblityBufferBaryCentrics");
		}
	}

	// Shader range
	{
		FrameBufferAllocator::BufferCreatationDesc desc = {};
		desc._sizeInByte = shadingQuadCount * sizeof(u32);
		desc._flags = rhi::RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._initialState = rhi::RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_shaderRangeBuffer[0] = frameBufferAllocator->createGpuBuffer(desc);
		_shaderRangeBuffer[1] = frameBufferAllocator->createGpuBuffer(desc);
		_shaderRangeBuffer[0]->setName("VisiblityBufferShaderRangeMin");
		_shaderRangeBuffer[1]->setName("VisiblityBufferShaderRangeMax");
	}

	// Shader id depth
	{
		rhi::ClearValue depthOptimizedClearValue = {};
		depthOptimizedClearValue._format = rhi::FORMAT_D16_UNORM;
		depthOptimizedClearValue._depthStencil._depth = 1.0f;
		depthOptimizedClearValue._depthStencil._stencil = 0;

		FrameBufferAllocator::TextureCreatationDesc desc = {};
		desc._optimizedClearValue = &depthOptimizedClearValue;
		desc._format = rhi::FORMAT_D16_UNORM;
		desc._width = u64(screenWidth);
		desc._height = u64(screenHeight);
		desc._flags = rhi::RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		desc._initialState = rhi::RESOURCE_STATE_DEPTH_WRITE;
		_shaderIdDepth = frameBufferAllocator->createGpuTexture(desc);
		_shaderIdDepth->setName("VisiblityBufferShaderIdDepth");
	}

	// デスクリプター
	{
		FrameDescriptorAllocator* descriptorAllocator = FrameDescriptorAllocator::Get();

		_shaderIdDsv = descriptorAllocator->allocateDsvGpu();
		_triangleIdRtv = descriptorAllocator->allocateRtvGpu(3);
		_triangleIdSrv = descriptorAllocator->allocateSrvCbvUavGpu();
		_shaderRangeSrv = descriptorAllocator->allocateSrvCbvUavGpu(2);
		_shaderIdSrv = descriptorAllocator->allocateSrvCbvUavGpu();
		_shaderRangeUav = descriptorAllocator->allocateSrvCbvUavGpu(2);
		_shaderRangeCpuUav = descriptorAllocator->allocateSrvCbvUavCpu(2);
		_baryCentricsSrv = descriptorAllocator->allocateSrvCbvUavGpu();

		// RTV
		{
			device->createRenderTargetView(_triangleIdTexture->getResource(), _triangleIdRtv.get(0)._cpuHandle);
			device->createRenderTargetView(_triangleShaderIdTexture->getResource(), _triangleIdRtv.get(1)._cpuHandle);
			device->createRenderTargetView(_baryCentricsGpuTexture->getResource(), _triangleIdRtv.get(2)._cpuHandle);
		}

		// DSV
		{
			device->createDepthStencilView(_shaderIdDepth->getResource(), _shaderIdDsv._cpuHandle);
		}

		// SRV
		{
			rhi::ShaderResourceViewDesc desc = {};
			desc._format = rhi::FORMAT_R32_TYPELESS;
			desc._viewDimension = rhi::SRV_DIMENSION_BUFFER;
			desc._buffer._firstElement = 0;
			desc._buffer._flags = rhi::BUFFER_SRV_FLAG_RAW;
			desc._buffer._numElements = _shaderRangeBuffer[0]->getU32ElementCount();
			device->createShaderResourceView(_shaderRangeBuffer[0]->getResource(), &desc, _shaderRangeSrv.get(0)._cpuHandle);
			device->createShaderResourceView(_shaderRangeBuffer[1]->getResource(), &desc, _shaderRangeSrv.get(1)._cpuHandle);

			device->createShaderResourceView(_triangleIdTexture->getResource(), nullptr, _triangleIdSrv._cpuHandle);
			device->createShaderResourceView(_triangleShaderIdTexture->getResource(), nullptr, _shaderIdSrv._cpuHandle);
			device->createShaderResourceView(_baryCentricsGpuTexture->getResource(), nullptr, _baryCentricsSrv._cpuHandle);
		}

		// UAV
		{
			rhi::UnorderedAccessViewDesc desc = {};
			desc._format = rhi::FORMAT_R32_TYPELESS;
			desc._viewDimension = rhi::UAV_DIMENSION_BUFFER;
			desc._buffer._firstElement = 0;
			desc._buffer._flags = rhi::BUFFER_UAV_FLAG_RAW;
			desc._buffer._numElements = _shaderRangeBuffer[0]->getU32ElementCount();
			device->createUnorderedAccessView(_shaderRangeBuffer[0]->getResource(), nullptr, &desc, _shaderRangeUav.get(0)._cpuHandle);
			device->createUnorderedAccessView(_shaderRangeBuffer[1]->getResource(), nullptr, &desc, _shaderRangeUav.get(1)._cpuHandle);

			device->createUnorderedAccessView(_shaderRangeBuffer[0]->getResource(), nullptr, &desc, _shaderRangeCpuUav.get(0)._cpuHandle);
			device->createUnorderedAccessView(_shaderRangeBuffer[1]->getResource(), nullptr, &desc, _shaderRangeCpuUav.get(1)._cpuHandle);
		}
	}
}
};