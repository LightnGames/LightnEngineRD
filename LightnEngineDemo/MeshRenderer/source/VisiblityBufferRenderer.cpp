#include <MeshRenderer/impl/VisiblityBufferRenderer.h>
#include <GfxCore/impl/ViewSystemImpl.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <GfxCore/impl/QueryHeapSystem.h>
#include <MaterialSystem/impl/MaterialSystemImpl.h>
#include <MaterialSystem/impl/PipelineStateSystem.h>
#include <MeshRenderer/impl/VramShaderSetSystem.h>
#include <TextureSystem/impl/TextureSystemImpl.h>

void VisiblityBufferRenderer::initialize() {
	GraphicsSystemImpl* graphicsSystem = GraphicsSystemImpl::Get();
	Device* device = graphicsSystem->getDevice();
	ViewInfo* viewInfo = ViewSystemImpl::Get()->getView();
	ViewPort viewPort = viewInfo->_viewPort;

	u32 shaderRangeWidth = static_cast<u64>(viewPort._width / SHADER_RANGE_TILE_SIZE) + 1;
	u32 shaderRangeHeight = static_cast<u64>(viewPort._height / SHADER_RANGE_TILE_SIZE) + 1;
	_shadingQuadCount = shaderRangeWidth * shaderRangeHeight;

	// Texture
	{
		GpuTextureDesc desc = {};
		desc._device = device;
		desc._format = FORMAT_R32G32_UINT;
		desc._width = static_cast<u64>(viewPort._width);
		desc._height = static_cast<u64>(viewPort._height);
		desc._flags = RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		desc._initialState = RESOURCE_STATE_RENDER_TARGET;
		_triangleIdTexture.initialize(desc);
		_triangleIdTexture.setDebugName("Visiblity Buffer Triangle ID");

		desc._format = FORMAT_R8_UINT;
		ClearValue optimizedClearValue = {};
		optimizedClearValue._format = desc._format;
		optimizedClearValue._color[0] = static_cast<f32>(UINT8_MAX);

		desc._optimizedClearValue = &optimizedClearValue;
		_triangleShaderIdTexture.initialize(desc);
		_triangleShaderIdTexture.setDebugName("Visiblity Buffer Triangle Shader ID");
	}

	// Shader range
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._sizeInByte = _shadingQuadCount * sizeof(u32);
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_shaderRangeBuffer[0].initialize(desc);
		_shaderRangeBuffer[1].initialize(desc);
		_shaderRangeBuffer[0].setDebugName("Visiblity Buffer Shader Range Min");
		_shaderRangeBuffer[1].setDebugName("Visiblity Buffer Shader Range Max");
	}

	// Shader id depth
	{
		GpuTextureDesc desc = {};
		desc._device = device;
		desc._format = FORMAT_D16_UNORM;
		desc._width = static_cast<u64>(viewPort._width);
		desc._height = static_cast<u64>(viewPort._height);
		desc._flags = RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		desc._initialState = RESOURCE_STATE_DEPTH_WRITE;
		_shaderIdDepth.initialize(desc);
		_shaderIdDepth.setDebugName("Visiblity Buffer Shader ID Depth");
	}

	// Constant buffer
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._sizeInByte = GetConstantBufferAligned(sizeof(BuildShaderIdConstant));
		desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		_buildShaderIdConstantBuffer.initialize(desc);
		_buildShaderIdConstantBuffer.setDebugName("Visibility Buffer Build Shader ID Constant");

		desc._sizeInByte = GetConstantBufferAligned(sizeof(ShadingConstant));
		_shadingConstantBuffer.initialize(desc);
		_shadingConstantBuffer.setDebugName("Visibility Buffer Shading Constant");

		VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
		{
			BuildShaderIdConstant* constant = vramUpdater->enqueueUpdate<BuildShaderIdConstant>(&_buildShaderIdConstantBuffer, 0, 1);
			constant->_resolution[0] = shaderRangeWidth;
			constant->_resolution[1] = shaderRangeHeight;
		}

		{
			ShadingConstant* constant = vramUpdater->enqueueUpdate<ShadingConstant>(&_shadingConstantBuffer, 0, 1);
			constant->_quadNdcSize[0] = static_cast<f32>(SHADER_RANGE_TILE_SIZE) / viewPort._width;
			constant->_quadNdcSize[1] = static_cast<f32>(SHADER_RANGE_TILE_SIZE) / viewPort._height;
			constant->_shaderRangeResolution[0] = shaderRangeWidth;
			constant->_shaderRangeResolution[1] = shaderRangeHeight;
		}
	}

	// RTV
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getRtvGpuDescriptorAllocator();
		u64 incremenetSize = allocator->getIncrimentSize();
		_triangleIdRtv = allocator->allocateDescriptors(2);
		device->createRenderTargetView(_triangleIdTexture.getResource(), _triangleIdRtv._cpuHandle);
		device->createRenderTargetView(_triangleShaderIdTexture.getResource(), _triangleIdRtv._cpuHandle + incremenetSize);
	}

	// DSV
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getDsvGpuDescriptorAllocator();
		_shaderIdDsv = allocator->allocateDescriptors(1);
		device->createDepthStencilView(_shaderIdDepth.getResource(), _shaderIdDsv._cpuHandle);
	}

	// SRV & UAV & CBV
	{
		// GPU visible
		{
			DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
			_triangleIdSrv = allocator->allocateDescriptors(1);
			_shaderRangeSrv = allocator->allocateDescriptors(2);
			_shaderIdSrv = allocator->allocateDescriptors(1);
			_shaderRangeUav = allocator->allocateDescriptors(2);
			_buildShaderIdCbv = allocator->allocateDescriptors(1);
			_shadingCbv = allocator->allocateDescriptors(1);
		}

		// CPU visible
		{
			DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();
			_shaderRangeCpuUav = allocator->allocateDescriptors(2);
		}

		device->createShaderResourceView(_triangleIdTexture.getResource(), nullptr, _triangleIdSrv._cpuHandle);
		device->createShaderResourceView(_triangleShaderIdTexture.getResource(), nullptr, _shaderIdSrv._cpuHandle);

		u32 cpuIncSize = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator()->getIncrimentSize();
		u32 gpuIncSize = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator()->getIncrimentSize();
		u32 shaderRangeNumElements = _shaderRangeBuffer[0].getSizeInByte() / sizeof(u32);
		{
			ShaderResourceViewDesc desc = {};
			desc._format = FORMAT_R32_TYPELESS;
			desc._viewDimension = SRV_DIMENSION_BUFFER;
			desc._buffer._firstElement = 0;
			desc._buffer._flags = BUFFER_SRV_FLAG_RAW;
			desc._buffer._numElements = shaderRangeNumElements;
			device->createShaderResourceView(_shaderRangeBuffer[0].getResource(), &desc, _shaderRangeSrv._cpuHandle);
			device->createShaderResourceView(_shaderRangeBuffer[1].getResource(), &desc, _shaderRangeSrv._cpuHandle + gpuIncSize);
		}

		{
			UnorderedAccessViewDesc desc = {};
			desc._format = FORMAT_R32_TYPELESS;
			desc._viewDimension = UAV_DIMENSION_BUFFER;
			desc._buffer._firstElement = 0;
			desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
			desc._buffer._numElements = shaderRangeNumElements;
			device->createUnorderedAccessView(_shaderRangeBuffer[0].getResource(), nullptr, &desc, _shaderRangeUav._cpuHandle);
			device->createUnorderedAccessView(_shaderRangeBuffer[1].getResource(), nullptr, &desc, _shaderRangeUav._cpuHandle + gpuIncSize);

			device->createUnorderedAccessView(_shaderRangeBuffer[0].getResource(), nullptr, &desc, _shaderRangeCpuUav._cpuHandle);
			device->createUnorderedAccessView(_shaderRangeBuffer[1].getResource(), nullptr, &desc, _shaderRangeCpuUav._cpuHandle + cpuIncSize);
		}

		{
			device->createConstantBufferView(_buildShaderIdConstantBuffer.getConstantBufferViewDesc(), _buildShaderIdCbv._cpuHandle);
			device->createConstantBufferView(_shadingConstantBuffer.getConstantBufferViewDesc(), _shadingCbv._cpuHandle);
		}
	}

	// Shader
	{
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();

		// root signature
		{
			DescriptorRange constantCbvRange(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			DescriptorRange triangleIdSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			DescriptorRange shaderRangeUavRange(DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);

			_buildShaderIdRootSignature = allocator->allocateRootSignature();

			RootParameter rootParameters[VisibilityBufferBuildShaderIdRootParam::COUNT] = {};
			rootParameters[VisibilityBufferBuildShaderIdRootParam::CONSTANT].initializeDescriptorTable(1, &constantCbvRange, SHADER_VISIBILITY_PIXEL);
			rootParameters[VisibilityBufferBuildShaderIdRootParam::TRIANGLE_ID].initializeDescriptorTable(1, &triangleIdSrvRange, SHADER_VISIBILITY_PIXEL);
			rootParameters[VisibilityBufferBuildShaderIdRootParam::SHADER_RANGE].initializeDescriptorTable(1, &shaderRangeUavRange, SHADER_VISIBILITY_PIXEL);

			RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;
			rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
			rootSignatureDesc._parameters = rootParameters;
			_buildShaderIdRootSignature->iniaitlize(rootSignatureDesc);
			_buildShaderIdRootSignature->setDebugName("Visibility Buffer Build Shader ID Root Signature");
		}

		// pipeline state
		{
			ShaderBlob* vertexShader = allocator->allocateShaderBlob();
			ShaderBlob* pixelShader = allocator->allocateShaderBlob();
			vertexShader->initialize("L:/LightnEngine/resource/common/shader/standard_mesh/screen_triangle.vso");
			pixelShader->initialize("L:/LightnEngine/resource/common/shader/visibility_buffer/build_shader_id.pso");

			_buildShaderIdPipelineState = allocator->allocatePipelineState();

			GraphicsPipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._vs = vertexShader->getShaderByteCode();
			pipelineStateDesc._ps = pixelShader->getShaderByteCode();
			pipelineStateDesc._dsvFormat = FORMAT_D16_UNORM;
			pipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_ALWAYS;
			pipelineStateDesc._topologyType = PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineStateDesc._rootSignature = _buildShaderIdRootSignature;
			pipelineStateDesc._sampleDesc._count = 1;
			_buildShaderIdPipelineState->iniaitlize(pipelineStateDesc);
			_buildShaderIdPipelineState->setDebugName("L:/LightnEngine/work/common/shader/visibility_buffer/build_shader_id.pso");

			vertexShader->terminate();
			pixelShader->terminate();
		}
	}
}

void VisiblityBufferRenderer::terminate() {
	_triangleIdTexture.terminate();
	_triangleShaderIdTexture.terminate();
	_shaderIdDepth.terminate();
	_shaderRangeBuffer[0].terminate();
	_shaderRangeBuffer[1].terminate();
	_shadingConstantBuffer.terminate();
	_buildShaderIdConstantBuffer.terminate();
	_buildShaderIdPipelineState->terminate();
	_buildShaderIdRootSignature->terminate();

	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getRtvGpuDescriptorAllocator();
		allocator->discardDescriptor(_triangleIdRtv);
	}

	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getDsvGpuDescriptorAllocator();
		allocator->discardDescriptor(_shaderIdDsv);
	}

	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		allocator->discardDescriptor(_triangleIdSrv);
		allocator->discardDescriptor(_shaderIdSrv);
		allocator->discardDescriptor(_shaderRangeSrv);
		allocator->discardDescriptor(_shaderRangeUav);
		allocator->discardDescriptor(_buildShaderIdCbv);
		allocator->discardDescriptor(_shadingCbv);
	}

	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();
		allocator->discardDescriptor(_shaderRangeCpuUav);
	}
}

void VisiblityBufferRenderer::buildShaderId(const BuildShaderIdContext& context) {
	CommandList* commandList = context._commandList;

	{
		ResourceTransitionBarrier barriers[3] = {};
		barriers[0] = _triangleIdTexture.getAndUpdateTransitionBarrier(RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		barriers[1] = _shaderRangeBuffer[0].getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
		barriers[2] = _shaderRangeBuffer[1].getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->transitionBarriers(barriers, LTN_COUNTOF(barriers));
	}

	// clear shader range min
	{
		u32 clearValues[4] = { UINT32_MAX };
		GpuDescriptorHandle gpuHandle = _shaderRangeUav._gpuHandle;
		CpuDescriptorHandle cpuHandle = _shaderRangeCpuUav._cpuHandle;
		commandList->clearUnorderedAccessViewUint(gpuHandle, cpuHandle, _shaderRangeBuffer[0].getResource(), clearValues, 0, nullptr);
	}

	// clear shader range max
	{
		u32 clearValues[4] = { 0 };
		u32 cpuIncSize = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator()->getIncrimentSize();
		u32 gpuIncSize = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator()->getIncrimentSize();
		GpuDescriptorHandle gpuHandle = _shaderRangeUav._gpuHandle + gpuIncSize;
		CpuDescriptorHandle cpuHandle = _shaderRangeCpuUav._cpuHandle + cpuIncSize;
		commandList->clearUnorderedAccessViewUint(gpuHandle, cpuHandle, _shaderRangeBuffer[1].getResource(), clearValues, 0, nullptr);
	}

	commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->setRenderTargets(0, nullptr, &_shaderIdDsv);
	commandList->setGraphicsRootSignature(_buildShaderIdRootSignature);
	commandList->setPipelineState(_buildShaderIdPipelineState);
	commandList->setGraphicsRootDescriptorTable(VisibilityBufferBuildShaderIdRootParam::CONSTANT, _buildShaderIdCbv._gpuHandle);
	commandList->setGraphicsRootDescriptorTable(VisibilityBufferBuildShaderIdRootParam::TRIANGLE_ID, _shaderIdSrv._gpuHandle);
	commandList->setGraphicsRootDescriptorTable(VisibilityBufferBuildShaderIdRootParam::SHADER_RANGE, _shaderRangeUav._gpuHandle);
	commandList->drawInstanced(3, 1, 0, 0);

	{
		ResourceTransitionBarrier barriers[3] = {};
		barriers[0] = _triangleIdTexture.getAndUpdateTransitionBarrier(RESOURCE_STATE_RENDER_TARGET);
		barriers[1] = _shaderRangeBuffer[0].getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		barriers[2] = _shaderRangeBuffer[1].getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		commandList->transitionBarriers(barriers, LTN_COUNTOF(barriers));
	}
}

void VisiblityBufferRenderer::shading(const ShadingContext& context) {
	CommandList* commandList = context._commandList;
	ViewInfo* viewInfo = context._viewInfo;
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	DescriptorHandle textureDescriptors = TextureSystemImpl::Get()->getDescriptors();
	u32 shaderSetCount = materialSystem->getShaderSetCount();

	{
		ResourceTransitionBarrier barriers[2] = {};
		barriers[0] = _shaderIdDepth.getAndUpdateTransitionBarrier(RESOURCE_STATE_DEPTH_READ);
		barriers[1] = _triangleIdTexture.getAndUpdateTransitionBarrier(RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->transitionBarriers(barriers, LTN_COUNTOF(barriers));
	}

	commandList->setRenderTargets(1, &viewInfo->_hdrRtv, &_shaderIdDsv);
	commandList->setViewports(1, &viewInfo->_viewPort);
	commandList->setScissorRects(1, &viewInfo->_scissorRect);

	for (u32 pipelineStateIndex = 0; pipelineStateIndex < shaderSetCount; ++pipelineStateIndex) {
		PipelineStateGroup* pipelineState = context._pipelineStates[pipelineStateIndex];
		if (pipelineState == nullptr) {
			continue;
		}

		VramShaderSet* vramShaderSet = &context._vramShaderSets[pipelineStateIndex];
		//u32 commandCountMax = context._indirectArgmentCounts[pipelineStateIndex];
		//if (commandCountMax == 0) {
		//	continue;
		//}

		DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Shading Shader %d", pipelineStateIndex);

		commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
		commandList->setPipelineState(pipelineState->getPipelineState());
		commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::VIEW_CONSTANT, viewInfo->_viewInfoCbv._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::CONSTANT, _shadingCbv._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::SHADER_RANGE, _shaderRangeSrv._gpuHandle);
		commandList->setGraphicsRoot32BitConstants(ShadingRootParam::ROOT_CONSTANT, 1, &pipelineStateIndex, 0);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::TRIANGLE_ATTRIBUTE, _triangleIdSrv._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::PRIMITIVE_INDICES, context._primitiveIndicesSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::VERTEX_POSITION, context._vertexPositionSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MESH_INSTANCE_WORLD_MATRICES, context._meshInstanceWorldMatrixSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MESH_INSTANCE, context._meshInstanceSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::MESHES, context._meshesSrv);
		commandList->setGraphicsRootDescriptorTable(ShadingRootParam::LOD_LEVELS, context._currentLodLevelSrv);

		commandList->drawInstanced(6, _shadingQuadCount, 0, 0);
	}

	{
		ResourceTransitionBarrier barriers[2] = {};
		barriers[0] = _shaderIdDepth.getAndUpdateTransitionBarrier(RESOURCE_STATE_DEPTH_WRITE);
		barriers[1] = _triangleIdTexture.getAndUpdateTransitionBarrier(RESOURCE_STATE_RENDER_TARGET);
		commandList->transitionBarriers(barriers, LTN_COUNTOF(barriers));
	}
}
