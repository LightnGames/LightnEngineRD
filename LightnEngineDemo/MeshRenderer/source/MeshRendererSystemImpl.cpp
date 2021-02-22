#include <MeshRenderer/impl/MeshRendererSystemImpl.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <GfxCore/impl/ViewSystemImpl.h>
#include <GfxCore/impl/QueryHeapSystem.h>
#include <DebugRenderer/impl/DebugRendererSystemImpl.h>
#include <MaterialSystem/impl/MaterialSystemImpl.h>
#include <TextureSystem/impl/TextureSystemImpl.h>

MeshRendererSystemImpl _meshRendererSystem;

MeshRendererSystem* MeshRendererSystem::Get() {
	return &_meshRendererSystem;
}

MeshRendererSystemImpl* MeshRendererSystemImpl::Get() {
	return &_meshRendererSystem;
}

#if ENABLE_MESH_SHADER
void MeshRendererSystemImpl::renderMeshShader(CommandList* commandList, ViewInfo* viewInfo) {
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	GpuDescriptorHandle meshInstanceHandle = _scene.getMeshInstanceHandles()._gpuHandle;
	GpuDescriptorHandle meshHandle = _resourceManager.getMeshHandle()._gpuHandle;
	GpuDescriptorHandle indirectArgumentOffsetHandle = _scene.getIndirectArgumentOffsetSrv()._gpuHandle;
	u32 meshInstanceCountMax = _scene.getMeshInstanceCountMax();
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	DescriptorHandle textureDescriptors = TextureSystemImpl::Get()->getDescriptors();
	DescriptorHandle vertexResourceDescriptors = _resourceManager.getVertexHandle();
	u32 pipelineStateResourceCount = materialSystem->getShaderSetCount();

	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::RED, "Mesh Shader Pass");
	queryHeapSystem->setCurrentMarkerName("Mesh Shader Pass");
	queryHeapSystem->setMarker(commandList);

	_view.resetResultBuffers(commandList);

	bool isFixedCullingView = _cullingDebugType & CULLING_DEBUG_TYPE_FIXED_VIEW;
	if (!isFixedCullingView) {
		setFixedDebugView(commandList, viewInfo);
	}

	// Lod level 計算
	if (!isFixedCullingView) {
		computeLod(commandList, viewInfo);
	}

	// メッシュレットバウンディング　デバッグ表示
	if (_debugDrawMeshletBounds) {
		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::YELLOW, "Debug Meshlet Bounds");

		DebugRendererSystemImpl* debugSystem = DebugRendererSystemImpl::Get();
		commandList->setComputeRootSignature(_debugMeshletBoundsRootSignature);
		commandList->setPipelineState(_debugMeshletBoundsPipelineState);

		commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_SCENE_INFO, _cullingSceneConstantHandle._gpuHandle);
		commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_MESH, meshHandle);
		commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_MESH_INSTANCE, meshInstanceHandle);
		commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_LOD_LEVEL, _view.getCurrentLodLevelSrv()._gpuHandle);
		commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_INDIRECT_ARGUMENT, debugSystem->getLineGpuUavHandle()._gpuHandle);

		u32 dispatchCount = RoundUp(meshInstanceCountMax, 128u);
		commandList->dispatch(dispatchCount, 1, 1);

		queryHeapSystem->setCurrentMarkerName("Debug Meshlet Bounds");
		queryHeapSystem->setMarker(commandList);
	}

	// デプスプリパス用　GPUカリング
	if (!isFixedCullingView) {
		bool isPassedCulling = _cullingDebugType & CULLING_DEBUG_TYPE_PASS_MESH_CULLING;
		PipelineState* pipelineState = isPassedCulling ? _gpuCullingPassPipelineState : _gpuCullingPipelineState;
		depthPrePassCulling(commandList, viewInfo, pipelineState);
	}

	buildIndirectArgument(commandList);

	// デプスプリパス
	{
		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::YELLOW, "Depth Prepass");
		commandList->setViewports(1, &viewInfo->_viewPort);
		commandList->setScissorRects(1, &viewInfo->_scissorRect);

		for (u32 pipelineStateIndex = 0; pipelineStateIndex < pipelineStateResourceCount; ++pipelineStateIndex) {
			ShaderSetImpl* shaderSet = materialSystem->getShaderSet(pipelineStateIndex);
			if (!materialSystem->isEnabledShaderSet(shaderSet)) {
				continue;
			}

			VramShaderSet* vramShaderSet = _scene.getVramShaderSetSystem()->getShaderSet(pipelineStateIndex);
			u32 commandCountMax = vramShaderSet->getTotalRefCount();
			if (commandCountMax == 0) {
				continue;
			}

			PipelineStateGroup* pipelineState = shaderSet->getDepthPipelineStateGroup();

			// インスタンシング描画
			{
				u32 indirectArgumentOffset = pipelineStateIndex * Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX;
				u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::DispatchMeshIndirectArgument);

				u32 countBufferOffset = pipelineStateIndex * sizeof(u32);

				commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
				commandList->setPipelineState(pipelineState->getPipelineState());
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VIEW_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT, _debugFixedViewConstantHandle._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH, meshHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESHLET_INFO, _view.getMeshletInstanceInfoSrv()._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH_INSTANCE, meshInstanceHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VERTEX_RESOURCES, vertexResourceDescriptors._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_TEXTURES, textureDescriptors._gpuHandle);
				_view.setDrawCurrentLodDescriptorTable(commandList);
				_view.render(commandList, pipelineState->getCommandSignature(), Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX, indirectArgumentOffsetSizeInByte, countBufferOffset);
			}

			// 単品描画
			{
				u32 indirectArgumentOffset = _scene.getIndirectArgumentOffset(pipelineStateIndex) + (VramShaderSetSystem::SHADER_SET_COUNT_MAX * Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX);
				u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::DispatchMeshIndirectArgument);
				LTN_ASSERT(indirectArgumentOffset + commandCountMax <= GraphicsView::INDIRECT_ARGUMENT_COUNT_MAX);

				u32 countBufferOffset = (pipelineStateIndex + VramShaderSetSystem::SHADER_SET_COUNT_MAX) * sizeof(u32);
				commandList->setPipelineState(pipelineState->getPipelineState());
				_view.render(commandList, pipelineState->getCommandSignature(), commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);
			}
		}

		queryHeapSystem->setCurrentMarkerName("Depth Prepass");
		queryHeapSystem->setMarker(commandList);
	}

	// build hiz
	if (!isFixedCullingView) {
		buildHiz(commandList, viewInfo);
	}

	// GPUカリング
	if (!isFixedCullingView) {
		bool isPassedCulling = _cullingDebugType & CULLING_DEBUG_TYPE_PASS_MESH_CULLING;
		PipelineState* pipelineState = isPassedCulling ? _gpuCullingPassPipelineState : _gpuOcclusionCullingPipelineState;
		mainCulling(commandList, viewInfo, pipelineState);
	}

	buildIndirectArgument(commandList);

	// 描画
	{
		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Main Pass");
		commandList->setViewports(1, &viewInfo->_viewPort);
		commandList->setScissorRects(1, &viewInfo->_scissorRect);

		_view.resourceBarriersHizTextureToSrv(commandList);

		for (u32 pipelineStateIndex = 0; pipelineStateIndex < pipelineStateResourceCount; ++pipelineStateIndex) {
			ShaderSetImpl* shaderSet = materialSystem->getShaderSet(pipelineStateIndex);
			if (!materialSystem->isEnabledShaderSet(shaderSet)) {
				continue;
			}

			VramShaderSet* vramShaderSet = _scene.getVramShaderSetSystem()->getShaderSet(pipelineStateIndex);
			u32 commandCountMax = vramShaderSet->getTotalRefCount();
			if (commandCountMax == 0) {
				continue;
			}

			DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Shader %d", pipelineStateIndex);

			PipelineStateGroup* pipelineState = nullptr;

			switch (_debugPrimitiveType) {
			case DEBUG_PRIMITIVE_TYPE_DEFAULT:
				pipelineState = shaderSet->getPipelineStateGroup();
				break;
			case DEBUG_PRIMITIVE_TYPE_MESHLET:
				pipelineState = shaderSet->getDebugMeshletPipelineStateGroup();
				break;
			case DEBUG_PRIMITIVE_TYPE_LODLEVEL:
				pipelineState = shaderSet->getDebugLodLevelPipelineStateGroup();
				break;
			case DEBUG_PRIMITIVE_TYPE_OCCLUSION:
				pipelineState = shaderSet->getDebugOcclusionPipelineStateGroup();
				break;
			case DEBUG_PRIMITIVE_TYPE_DEPTH:
				pipelineState = shaderSet->getDebugDepthPipelineStateGroup();
				break;
			case DEBUG_PRIMITIVE_TYPE_TEXCOORDS:
				pipelineState = shaderSet->getDebugTexcoordsPipelineStateGroup();
				break;
			case DEBUG_PRIMITIVE_TYPE_WIREFRAME:
				pipelineState = shaderSet->getDebugWireFramePipelineStateGroup();
				break;
			}

			if (_cullingDebugType & CULLING_DEBUG_TYPE_PASS_MESHLET_CULLING) {
				pipelineState = shaderSet->getDebugCullingPassPipelineStateGroup();
			}

			// インスタンシング描画
			{
				u32 countBufferOffset = pipelineStateIndex * sizeof(u32);
				u32 indirectArgumentOffset = pipelineStateIndex * Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX;
				u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::DispatchMeshIndirectArgument);

				commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
				commandList->setPipelineState(pipelineState->getPipelineState());
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VIEW_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT, _debugFixedViewConstantHandle._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH, meshHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESHLET_INFO, _view.getMeshletInstanceInfoSrv()._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH_INSTANCE, meshInstanceHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VERTEX_RESOURCES, vertexResourceDescriptors._gpuHandle);
				commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_TEXTURES, textureDescriptors._gpuHandle);
				_view.setDrawResultDescriptorTable(commandList);
				_view.setDrawCurrentLodDescriptorTable(commandList);
				_view.render(commandList, pipelineState->getCommandSignature(), Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX, indirectArgumentOffsetSizeInByte, countBufferOffset);
			}

			// 単品描画
			{
				u32 indirectArgumentOffset = _scene.getIndirectArgumentOffset(pipelineStateIndex) + (VramShaderSetSystem::SHADER_SET_COUNT_MAX * Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX);
				u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::DispatchMeshIndirectArgument);
				LTN_ASSERT(indirectArgumentOffset + commandCountMax <= GraphicsView::INDIRECT_ARGUMENT_COUNT_MAX);

				u32 countBufferOffset = (pipelineStateIndex + VramShaderSetSystem::SHADER_SET_COUNT_MAX) * sizeof(u32);
				commandList->setPipelineState(pipelineState->getPipelineState());
				_view.render(commandList, pipelineState->getCommandSignature(), commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);
			}
		}

		_view.resourceBarriersHizSrvToTexture(commandList);
}

	_view.readbackCullingResultBuffer(commandList);

	queryHeapSystem->setCurrentMarkerName("Main Pass");
	queryHeapSystem->setMarker(commandList);
}
#endif

#if ENABLE_MULTI_INDIRECT_DRAW
void MeshRendererSystemImpl::renderMultiIndirect(CommandList* commandList, ViewInfo* viewInfo) {
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	GpuDescriptorHandle meshInstanceHandle = _scene.getMeshInstanceHandles()._gpuHandle;
	GpuDescriptorHandle meshHandle = _resourceManager.getMeshHandle()._gpuHandle;
	GpuDescriptorHandle indirectArgumentOffsetHandle = _scene.getIndirectArgumentOffsetSrv()._gpuHandle;
	u32 meshInstanceCountMax = _scene.getMeshInstanceCountMax();
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	DescriptorHandle textureDescriptors = TextureSystemImpl::Get()->getDescriptors();
	DescriptorHandle vertexResourceDescriptors = _resourceManager.getVertexHandle();
	u32 pipelineStateResourceCount = materialSystem->getShaderSetCount();

	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::RED, "Multi Draw Indirect Shader Pass");
	queryHeapSystem->setCurrentMarkerName("Multi Draw Indirect Shader Pass");
	queryHeapSystem->setMarker(commandList);

	GpuBuffer* vertexPositionBuffer = _resourceManager.getPositionVertexBuffer();
	GpuBuffer* vertexTexcoordBuffer = _resourceManager.getTexcoordVertexBuffer();
	GpuBuffer* indexBuffer = _resourceManager.getClassicIndexBuffer();

	VertexBufferView vertexBufferViews[2] = {};
	vertexBufferViews[0]._bufferLocation = vertexPositionBuffer->getGpuVirtualAddress();
	vertexBufferViews[0]._sizeInBytes = vertexPositionBuffer->getSizeInByte();
	vertexBufferViews[0]._strideInBytes = sizeof(Vector3);

	vertexBufferViews[1]._bufferLocation = vertexTexcoordBuffer->getGpuVirtualAddress();
	vertexBufferViews[1]._sizeInBytes = vertexTexcoordBuffer->getSizeInByte();
	vertexBufferViews[1]._strideInBytes = sizeof(u32);

	IndexBufferView indexBufferView = {};
	indexBufferView._bufferLocation = indexBuffer->getGpuVirtualAddress();
	indexBufferView._sizeInBytes = indexBuffer->getSizeInByte();
	indexBufferView._format = FORMAT_R32_UINT;

	_view.resetResultBuffers(commandList);

	bool isFixedCullingView = _cullingDebugType & CULLING_DEBUG_TYPE_FIXED_VIEW;
	if (!isFixedCullingView) {
		setFixedDebugView(commandList, viewInfo);
	}

	// Lod level 計算
	if (!isFixedCullingView) {
		ComputeLodContext context = {};
		context._commandList = commandList;
		context._graphicsView = &_view;
		context._viewInfo = viewInfo;
		context._meshHandle = _resourceManager.getMeshHandle()._gpuHandle;
		context._meshInstanceHandle = _scene.getMeshInstanceHandles()._gpuHandle;
		context._meshInstanceCountMax = _scene.getMeshInstanceCountMax();
		context._sceneConstantCbv = _scene.getSceneCbv()._gpuHandle;
		_meshRenderer.computeLod(context);
	}

	// デプスプリパス用　GPUカリング
	if (!isFixedCullingView) {
		GpuCullingContext context = {};
		context._commandList = commandList;
		context._graphicsView = &_view;
		context._viewInfo = viewInfo;
		context._cullingViewCbv = viewInfo->_depthPrePassCbvHandle._gpuHandle;
		context._meshHandle = _resourceManager.getMeshHandle()._gpuHandle;
		context._meshInstanceHandle = _scene.getMeshInstanceHandles()._gpuHandle;
		context._meshInstanceCountMax = _scene.getMeshInstanceCountMax();
		context._sceneConstantCbv = _scene.getSceneCbv()._gpuHandle;
		context._indirectArgumentOffsetSrv = _scene.getMultiDrawIndirectArgumentOffsetSrv()._gpuHandle;
		context._subMeshDrawInfoHandle = _resourceManager.getSubMeshDrawInfoSrvHandle()._gpuHandle;
		context._meshletInstanceInfoUav = _view.getMeshletInstanceInfoUav()._gpuHandle;
		context._meshletInstanceInfoOffsetSrv = _scene.getMeshletInstanceOffsetSrv()._gpuHandle;
		context._meshletInstanceInfoCountUav = _view.getMeshletInstanceCountUav()._gpuHandle;
		context._scopeName = "Depth Pre Pass Culling";
		_meshRenderer.multiDrawDepthPrePassCulling(context);
	}

	// デプスプリパス
	{
		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::YELLOW, "Depth Prepass");
		commandList->setViewports(1, &viewInfo->_viewPort);
		commandList->setScissorRects(1, &viewInfo->_scissorRect);

		viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_DEPTH_WRITE);

		for (u32 pipelineStateIndex = 0; pipelineStateIndex < pipelineStateResourceCount; ++pipelineStateIndex) {
			ShaderSetImpl* shaderSet = materialSystem->getShaderSet(pipelineStateIndex);
			if (!materialSystem->isEnabledShaderSet(shaderSet)) {
				continue;
			}

			VramShaderSet* vramShaderSet = _scene.getVramShaderSetSystem()->getShaderSet(pipelineStateIndex);
			u32 commandCountMax = vramShaderSet->getTotalRefCount();
			if (commandCountMax == 0) {
				continue;
			}

			u32 indirectArgumentOffset = _scene.getMultiDrawIndirectArgumentOffset(pipelineStateIndex);
			u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::StarndardMeshIndirectArguments);
			LTN_ASSERT(indirectArgumentOffset + commandCountMax <= GraphicsView::INDIRECT_ARGUMENT_COUNT_MAX);

			u32 countBufferOffset = pipelineStateIndex * sizeof(u32);
			ClassicShaderSet* classicShaderSet = shaderSet->getClassicShaderSet();

			commandList->setGraphicsRootSignature(classicShaderSet->_rootSignature);
			commandList->setPipelineState(classicShaderSet->_depthPipelineState);
			commandList->setVertexBuffers(0, LTN_COUNTOF(vertexBufferViews), vertexBufferViews);
			commandList->setIndexBuffer(&indexBufferView);
			commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_SCENE_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MESH_INSTANCE, meshInstanceHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_TEXTURES, textureDescriptors._gpuHandle);
			_view.render(commandList, classicShaderSet->_multiDrawCommandSignature, commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);
		}

		viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		queryHeapSystem->setCurrentMarkerName("Depth Prepass");
		queryHeapSystem->setMarker(commandList);
	}

	// build hiz
	if (!isFixedCullingView) {
		BuildHizContext context = {};
		context._commandList = commandList;
		context._graphicsView = &_view;
		context._viewInfo = viewInfo;
		_meshRenderer.buildHiz(context);
	}

	// GPUカリング
	if (!isFixedCullingView) {
		GpuCullingContext context = {};
		context._commandList = commandList;
		context._graphicsView = &_view;
		context._viewInfo = viewInfo;
		context._cullingViewCbv = viewInfo->_cbvHandle._gpuHandle;
		context._meshHandle = _resourceManager.getMeshHandle()._gpuHandle;
		context._meshInstanceHandle = _scene.getMeshInstanceHandles()._gpuHandle;
		context._meshInstanceCountMax = _scene.getMeshInstanceCountMax();
		context._sceneConstantCbv = _scene.getSceneCbv()._gpuHandle;
		context._indirectArgumentOffsetSrv = _scene.getMultiDrawIndirectArgumentOffsetSrv()._gpuHandle;
		context._subMeshDrawInfoHandle = _resourceManager.getSubMeshDrawInfoSrvHandle()._gpuHandle;
		context._meshletInstanceInfoUav = _view.getMeshletInstanceInfoUav()._gpuHandle;
		context._meshletInstanceInfoOffsetSrv = _scene.getMeshletInstanceOffsetSrv()._gpuHandle;
		context._meshletInstanceInfoCountUav = _view.getMeshletInstanceCountUav()._gpuHandle;
		context._scopeName = "Main Culling";
		_meshRenderer.multiDrawMainCulling(context);
	}

	// 描画
	{
		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Main Pass");
		commandList->setViewports(1, &viewInfo->_viewPort);
		commandList->setScissorRects(1, &viewInfo->_scissorRect);

		viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_DEPTH_WRITE);
		_view.resourceBarriersHizTextureToSrv(commandList);

		for (u32 pipelineStateIndex = 0; pipelineStateIndex < pipelineStateResourceCount; ++pipelineStateIndex) {
			ShaderSetImpl* shaderSet = materialSystem->getShaderSet(pipelineStateIndex);
			if (!materialSystem->isEnabledShaderSet(shaderSet)) {
				continue;
			}

			VramShaderSet* vramShaderSet = _scene.getVramShaderSetSystem()->getShaderSet(pipelineStateIndex);
			u32 commandCountMax = vramShaderSet->getTotalRefCount();
			if (commandCountMax == 0) {
				continue;
			}

			DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Shader %d", pipelineStateIndex);

			u32 indirectArgumentOffset = _scene.getMultiDrawIndirectArgumentOffset(pipelineStateIndex);
			u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::StarndardMeshIndirectArguments);
			LTN_ASSERT(indirectArgumentOffset + commandCountMax <= GraphicsView::INDIRECT_ARGUMENT_COUNT_MAX);

			u32 countBufferOffset = pipelineStateIndex * sizeof(u32);
			ClassicShaderSet* classicShaderSet = shaderSet->getClassicShaderSet();

			PipelineState* pipelineState = classicShaderSet->_pipelineState;
			switch (_debugPrimitiveType) {
			case DEBUG_PRIMITIVE_TYPE_DEFAULT:
				pipelineState = classicShaderSet->_pipelineState;
				break;
			case DEBUG_PRIMITIVE_TYPE_MESHLET:
				pipelineState = classicShaderSet->_debugPipelineState;
				break;
			case DEBUG_PRIMITIVE_TYPE_LODLEVEL:
				break;
			case DEBUG_PRIMITIVE_TYPE_OCCLUSION:
				break;
			case DEBUG_PRIMITIVE_TYPE_DEPTH:
				break;
			case DEBUG_PRIMITIVE_TYPE_TEXCOORDS:
				break;
			}

			commandList->setGraphicsRootSignature(classicShaderSet->_rootSignature);
			commandList->setPipelineState(pipelineState);
			commandList->setVertexBuffers(0, LTN_COUNTOF(vertexBufferViews), vertexBufferViews);
			commandList->setIndexBuffer(&indexBufferView);
			commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_SCENE_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MESH_INSTANCE, meshInstanceHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_TEXTURES, textureDescriptors._gpuHandle);
			_view.render(commandList, classicShaderSet->_multiDrawCommandSignature, commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);
		}

		_view.resourceBarriersHizSrvToTexture(commandList);
		viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	_view.readbackCullingResultBuffer(commandList);

	queryHeapSystem->setCurrentMarkerName("Main Pass");
	queryHeapSystem->setMarker(commandList);
}
#endif

#if ENABLE_CLASSIC_VERTEX
void MeshRendererSystemImpl::renderClassicVertex(CommandList* commandList, const ViewInfo* viewInfo) {
	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::RED, "Classic Shader Pass");

	GpuBuffer* vertexPositionBuffer = _resourceManager.getPositionVertexBuffer();
	GpuBuffer* vertexTexcoordBuffer = _resourceManager.getTexcoordVertexBuffer();
	GpuBuffer* indexBuffer = _resourceManager.getClassicIndexBuffer();

	ResourceTransitionBarrier toVertexBarriers[3] = {};
	toVertexBarriers[0] = vertexPositionBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	toVertexBarriers[1] = vertexTexcoordBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	toVertexBarriers[2] = indexBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_INDEX_BUFFER);
	commandList->transitionBarriers(toVertexBarriers, LTN_COUNTOF(toVertexBarriers));

	VertexBufferView vertexBufferViews[2] = {};
	vertexBufferViews[0]._bufferLocation = vertexPositionBuffer->getGpuVirtualAddress();
	vertexBufferViews[0]._sizeInBytes = vertexPositionBuffer->getSizeInByte();
	vertexBufferViews[0]._strideInBytes = sizeof(Vector3);

	vertexBufferViews[1]._bufferLocation = vertexTexcoordBuffer->getGpuVirtualAddress();
	vertexBufferViews[1]._sizeInBytes = vertexTexcoordBuffer->getSizeInByte();
	vertexBufferViews[1]._strideInBytes = sizeof(u32);

	IndexBufferView indexBufferView = {};
	indexBufferView._bufferLocation = indexBuffer->getGpuVirtualAddress();
	indexBufferView._sizeInBytes = indexBuffer->getSizeInByte();
	indexBufferView._format = FORMAT_R32_UINT;

	commandList->setViewports(1, &viewInfo->_viewPort);
	commandList->setScissorRects(1, &viewInfo->_scissorRect);

	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	DescriptorHandle textureDescriptors = TextureSystemImpl::Get()->getDescriptors();
	GpuDescriptorHandle meshInstanceHandle = _scene.getMeshInstanceHandles()._gpuHandle;
	u32 meshInstanceCount = _scene.getMeshInstanceCountMax();
	for (u32 meshInstanceIndex = 0; meshInstanceIndex < meshInstanceCount; ++meshInstanceIndex) {
		MeshInstanceImpl* meshInstance = _scene.getMeshInstance(meshInstanceIndex);
		if (!meshInstance->isEnable()) {
			continue;
		}

		const MeshInfo* meshInfo = meshInstance->getMesh()->getMeshInfo();
		u32 lodLevel = 0;
		u32 lodMeshVertexOffset = meshInstance->getMesh()->getGpuLodMesh(lodLevel)->_vertexOffset;
		u32 lodSubMeshLocalOffset = meshInfo->_subMeshOffsets[lodLevel];
		u32 subMeshCount = meshInstance->getMesh()->getGpuLodMesh()->_subMeshCount;
		for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
			const SubMeshInfo* subMeshInfo = meshInstance->getMesh()->getSubMeshInfo(subMeshIndex) + lodSubMeshLocalOffset;
			SubMeshInstance* subMeshInstance = meshInstance->getSubMeshInstance(subMeshIndex) + lodSubMeshLocalOffset;
			Material* material = subMeshInstance->getMaterial();
			ShaderSetImpl* shaderSet = static_cast<MaterialImpl*>(material)->getShaderSet();
			ClassicShaderSet* classicShaderSet = shaderSet->getClassicShaderSet();
			u32 shaderSetIndex = materialSystem->getShaderSetIndex(shaderSet);
			VramShaderSet* vramShaderSet = _scene.getVramShaderSetSystem()->getShaderSet(shaderSetIndex);
			commandList->setGraphicsRootSignature(classicShaderSet->_rootSignature);
			commandList->setPipelineState(classicShaderSet->_pipelineState);
			commandList->setVertexBuffers(0, LTN_COUNTOF(vertexBufferViews), vertexBufferViews);
			commandList->setIndexBuffer(&indexBufferView);
			commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_SCENE_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MESH_INSTANCE, meshInstanceHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_TEXTURES, textureDescriptors._gpuHandle);

			u32 instanceInfo[2] = {};
			instanceInfo[0] = meshInstanceIndex;
			instanceInfo[1] = meshInstance->getGpuSubMeshInstance(subMeshIndex)->_materialIndex;
			commandList->setGraphicsRoot32BitConstants(ROOT_CLASSIC_MESH_INFO, LTN_COUNTOF(instanceInfo), instanceInfo, 0);

			u32 vertexOffset = lodMeshVertexOffset;
			u32 indexOffset = subMeshInfo->_classiciIndexOffset;
			commandList->drawIndexedInstanced(subMeshInfo->_classicIndexCount, 1, indexOffset, vertexOffset, 0);
		}
	}

	ResourceTransitionBarrier toNonPixelShaderResourceBarriers[3] = {};
	toNonPixelShaderResourceBarriers[0] = vertexPositionBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toNonPixelShaderResourceBarriers[1] = vertexTexcoordBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toNonPixelShaderResourceBarriers[2] = indexBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarriers(toNonPixelShaderResourceBarriers, LTN_COUNTOF(toNonPixelShaderResourceBarriers));
}

void MeshRendererSystemImpl::setFixedDebugView(CommandList* commandList, ViewInfo* viewInfo) {
	ResourceTransitionBarrier constantToCopy[2] = {};
	constantToCopy[0] = _debugFixedViewConstantBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_COPY_DEST);
	constantToCopy[1] = viewInfo->_viewInfoBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_COPY_SOURCE);
	commandList->transitionBarriers(constantToCopy, LTN_COUNTOF(constantToCopy));
	commandList->copyResource(_debugFixedViewConstantBuffer.getResource(), viewInfo->_viewInfoBuffer.getResource());

	ResourceTransitionBarrier copyToConstant[2] = {};
	copyToConstant[0] = _debugFixedViewConstantBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	copyToConstant[1] = viewInfo->_viewInfoBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	commandList->transitionBarriers(copyToConstant, LTN_COUNTOF(copyToConstant));

	_debugFixedViewMatrix = viewInfo->_viewMatrix;
	_debugFixedProjectionMatrix = viewInfo->_projectionMatrix;
}
#endif

void MeshRendererSystemImpl::initialize() {
	_scene.initialize();
	_resourceManager.initialize();
	_meshRenderer.initialize();

	Device* device = GraphicsSystemImpl::Get()->getDevice();
	GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();

	_view.initialize(ViewSystemImpl::Get()->getView());

	DescriptorHeapAllocator* descriptorHeapAllocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();

	// デバッグビュー固定用定数バッファ
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = GetConstantBufferAligned(sizeof(ViewConstant));
		desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._device = device;
		_debugFixedViewConstantBuffer.initialize(desc);
		_debugFixedViewConstantBuffer.setDebugName("Debug Fixed View Info");
	}

	{
		_debugFixedViewConstantHandle = descriptorHeapAllocater->allocateDescriptors(1);
		device->createConstantBufferView(_debugFixedViewConstantBuffer.getConstantBufferViewDesc(), _debugFixedViewConstantHandle._cpuHandle);
	}
}

void MeshRendererSystemImpl::terminate() {
	processDeletion();
	_scene.terminate();
	_resourceManager.terminate();
	_meshRenderer.terminate();
	_view.terminate();

	_debugFixedViewConstantBuffer.terminate();

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocator->discardDescriptor(_debugFixedViewConstantHandle);
}

void MeshRendererSystemImpl::update() {
	if (!GraphicsSystemImpl::Get()->isInitialized()) {
		return;
	}

	_scene.update();
	_resourceManager.update();
	_view.update();

	// メッシュインスタンスデバッグオプション
	{
		struct MeshInstanceDebug {
			bool _visible = true;
			bool _drawMeshInstanceBounds = false;
			bool _drawMeshletBounds = false;
			bool _passMeshInstanceCulling = false;
			bool _passMeshletInstanceCulling = false;
			bool _fixedCullingView = false;
			GeometoryType _geometoryMode = GEOMETORY_MODE_MESH_SHADER;
			DebugPrimitiveType _primitiveType = DEBUG_PRIMITIVE_TYPE_DEFAULT;
		};

		auto debug = DebugWindow::StartWindow<MeshInstanceDebug>("MeshInstanceDebug");
		DebugWindow::Checkbox("visible", &debug._visible);
		DebugWindow::Checkbox("draw mesh instance bounds", &debug._drawMeshInstanceBounds);
		DebugWindow::Checkbox("draw meshlet bounds", &debug._drawMeshletBounds);
		DebugWindow::Checkbox("pass mesh culling", &debug._passMeshInstanceCulling);
		DebugWindow::Checkbox("pass meshlet culling", &debug._passMeshletInstanceCulling);
		DebugWindow::Checkbox("fixed culling view", &debug._fixedCullingView);

		const char* geometoryTypes[] = { "Mesh Shader", "Multi Indirect", "Vertex Shader" };
		DebugGui::Combo("Geometory Type", reinterpret_cast<s32*>(&debug._geometoryMode), geometoryTypes, LTN_COUNTOF(geometoryTypes));

		const char* primitiveTypes[] = { "Default", "Meshlet", "LodLevel", "Occlusion", "Depth", "Texcoords", "Wire Frame" };
		DebugGui::Combo("Primitive Type", reinterpret_cast<s32*>(&debug._primitiveType), primitiveTypes, LTN_COUNTOF(primitiveTypes));
		DebugWindow::End();

		if (debug._drawMeshInstanceBounds) {
			_scene.debugDrawMeshInstanceBounds();
		}

		_debugDrawMeshletBounds = debug._drawMeshletBounds;
		_visible = debug._visible;
		_debugPrimitiveType = debug._primitiveType;
		_geometoryType = debug._geometoryMode;
		setDebugCullingFlag(CULLING_DEBUG_TYPE_FIXED_VIEW, debug._fixedCullingView);
		setDebugCullingFlag(CULLING_DEBUG_TYPE_PASS_MESH_CULLING, debug._passMeshInstanceCulling);
		setDebugCullingFlag(CULLING_DEBUG_TYPE_PASS_MESHLET_CULLING, debug._passMeshletInstanceCulling);

		if (debug._fixedCullingView) {
			DebugRendererSystem::Get()->drawFrustum(_debugFixedViewMatrix, _debugFixedProjectionMatrix, Color4::YELLOW);
		}
	}

	DebugWindow::StartWindow("Scene Meshs");
	constexpr char format3[] = "%-7.3f%% ( %-6d/ %-6d)";
	constexpr char format2[] = "%-12s";
	char t[128];
	const CullingResult* cullingResult = _view.getCullingResult();

	// メッシュインスタンス GPUカリング結果表示
	{
		DebugGui::Text("Mesh Instance");

		u32 testFrustumCullingCount = cullingResult->_testFrustumCullingMeshInstanceCount;
		u32 passFrustumCullingCount = cullingResult->_passFrustumCullingMeshInstanceCount;
		u32 testOcclusionCullingCount = cullingResult->_testOcclusionCullingMeshInstanceCount;
		u32 passOcclusionCullingCount = cullingResult->_passOcclusionCullingMeshInstanceCount;
		f32 passFrustumCullingPersentage = cullingResult->getPassFrustumCullingMeshInstancePersentage();
		f32 passOcclusionCullingPersentage = cullingResult->getPassOcclusionCullingMeshInstancePersentage();

		sprintf_s(t, LTN_COUNTOF(t), format3, passFrustumCullingPersentage, passFrustumCullingCount, testFrustumCullingCount);
		DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Frustum Culling");

		sprintf_s(t, LTN_COUNTOF(t), format3, passOcclusionCullingPersentage, passOcclusionCullingCount, testOcclusionCullingCount);
		DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Occlusion Culling");
	}

	// サブメッシュインスタンス GPUカリング結果表示
	{
		DebugGui::Text("Sub Mesh Instance");

		u32 testFrustumCullingCount = cullingResult->_testFrustumCullingSubMeshInstanceCount;
		u32 passFrustumCullingCount = cullingResult->_passFrustumCullingSubMeshInstanceCount;
		u32 testOcclusionCullingCount = cullingResult->_testOcclusionCullingSubMeshInstanceCount;
		u32 passOcclusionCullingCount = cullingResult->_passOcclusionCullingSubMeshInstanceCount;
		f32 passFrustumCullingPersentage = cullingResult->getPassFrustumCullingSubMeshInstancePersentage();
		f32 passOcclusionCullingPersentage = cullingResult->getPassOcclusionCullingSubMeshInstancePersentage();

		sprintf_s(t, LTN_COUNTOF(t), format3, passFrustumCullingPersentage, passFrustumCullingCount, testFrustumCullingCount);
		DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Frustum Culling");

		sprintf_s(t, LTN_COUNTOF(t), format3, passOcclusionCullingPersentage, passOcclusionCullingCount, testOcclusionCullingCount);
		DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Occlusion Culling");
	}

	// メッシュレットインスタンス GPUカリング結果表示
	{
		DebugGui::Text("Meshlet Instance");

		u32 testFrustumCullingCount = cullingResult->_testFrustumCullingMeshletInstanceCount;
		u32 passFrustumCullingCount = cullingResult->_passFrustumCullingMeshletInstanceCount;
		u32 testBackfaceCullingCount = cullingResult->_testBackfaceCullingMeshletInstanceCount;
		u32 passBackfaceCullingCount = cullingResult->_passBackfaceCullingMeshletInstanceCount;
		u32 testOcclusionCullingCount = cullingResult->_testOcclusionCullingMeshletInstanceCount;
		u32 passOcclusionCullingCount = cullingResult->_passOcclusionCullingMeshletInstanceCount;
		f32 passFrustumCullingPersentage = cullingResult->getPassFrustumCullingMeshletInstancePersentage();
		f32 passBackfaceCullingPersentage = cullingResult->getPassBackfaceCullingMeshletInstancePersentage();
		f32 passOcclusionCullingPersentage = cullingResult->getPassOcclusionCullingMeshletInstancePersentage();

		sprintf_s(t, LTN_COUNTOF(t), format3, passFrustumCullingPersentage, passFrustumCullingCount, testFrustumCullingCount);
		DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Frustum Culling");

		sprintf_s(t, LTN_COUNTOF(t), format3, passBackfaceCullingPersentage, passBackfaceCullingCount, testBackfaceCullingCount);
		DebugGui::ProgressBar(passBackfaceCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Backface Culling");

		sprintf_s(t, LTN_COUNTOF(t), format3, passOcclusionCullingPersentage, passOcclusionCullingCount, testOcclusionCullingCount);
		DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Occlusion Culling");
	}

	// ポリゴン数 GPUカリング結果表示
	{
		DebugGui::Text("Triangle");

		u32 testFrustumCullingCount = cullingResult->_testFrustumCullingTriangleCount;
		u32 passFrustumCullingCount = cullingResult->_passFrustumCullingTriangleCount;
		u32 testBackfaceCullingCount = cullingResult->_testBackfaceCullingTriangleCount;
		u32 passBackfaceCullingCount = cullingResult->_passBackfaceCullingTriangleCount;
		u32 testOcclusionCullingCount = cullingResult->_testOcclusionCullingTriangleCount;
		u32 passOcclusionCullingCount = cullingResult->_passOcclusionCullingTriangleCount;
		f32 passFrustumCullingPersentage = cullingResult->getPassFrustumCullingTrianglePersentage();
		f32 passBackfaceCullingPersentage = cullingResult->getPassBackfaceCullingTrianglePersentage();
		f32 passOcclusionCullingPersentage = cullingResult->getPassOcclusionCullingTrianglePersentage();

		sprintf_s(t, LTN_COUNTOF(t), format3, passFrustumCullingPersentage, passFrustumCullingCount, testFrustumCullingCount);
		DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Frustum Culling");

		sprintf_s(t, LTN_COUNTOF(t), format3, passBackfaceCullingPersentage, passBackfaceCullingCount, testBackfaceCullingCount);
		DebugGui::ProgressBar(passBackfaceCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Backface Culling");

		sprintf_s(t, LTN_COUNTOF(t), format3, passOcclusionCullingPersentage, passOcclusionCullingCount, testOcclusionCullingCount);
		DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
		DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
		DebugGui::Text(format2, "Occlusion Culling");
	}

	if (DebugGui::BeginTabBar("SceneMeshsTabBar")) {
		if (DebugGui::BeginTabItem("Meshes")) {
			_resourceManager.drawDebugGui();
		}
		if (DebugGui::BeginTabItem("Mesh Instances")) {
			_scene.debugDrawGui();
		}
		if (DebugGui::BeginTabItem("Option")) {
			DebugGui::Text("This is the Cucumber tab!\nblah blah blah blah blah");
			DebugGui::EndTabItem();
		}
		DebugGui::EndTabBar();
	}

	DebugWindow::End();
}

void MeshRendererSystemImpl::render(CommandList* commandList, ViewInfo* viewInfo) {
	if (!_visible) {
		return;
	}

	// レンダーターゲットクリア
	{
		QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::GREEN, "Setup Draw");

		f32 clearColor[4] = {};
		DescriptorHandle currentRenderTargetHandle = viewInfo->_hdrRtv;
		viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_DEPTH_WRITE);
		commandList->setRenderTargets(1, &currentRenderTargetHandle, &viewInfo->_depthDsv);
		commandList->clearRenderTargetView(currentRenderTargetHandle, clearColor);
		commandList->clearDepthStencilView(viewInfo->_depthDsv._cpuHandle, CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	switch (_geometoryType) {
#if ENABLE_MESH_SHADER
	case GEOMETORY_MODE_MESH_SHADER:
		renderMeshShader(commandList, viewInfo);
		break;
#endif
#if ENABLE_MULTI_INDIRECT_DRAW
	case GEOMETORY_MODE_MULTI_INDIRECT:
		renderMultiIndirect(commandList, viewInfo);
		break;
#endif
#if ENABLE_CLASSIC_VERTEX
	case GEOMETORY_MODE_CLASSIC_VERTEX:
		renderClassicVertex(commandList, viewInfo);
		break;
#endif
	}
}

void MeshRendererSystemImpl::processDeletion() {
	_scene.processDeletion();
	_resourceManager.processDeletion();
}

Mesh* MeshRendererSystemImpl::allocateMesh(const MeshDesc& desc) {
	return _resourceManager.allocateMesh(desc);
}

Mesh* MeshRendererSystemImpl::createMesh(const MeshDesc& desc) {
	return _resourceManager.createMesh(desc);
}

MeshInstance* MeshRendererSystemImpl::createMeshInstance(const MeshInstanceDesc& desc) {
	return _scene.createMeshInstance(desc.mesh);
}

Mesh* MeshRendererSystemImpl::findMesh(u64 filePathHash) {
	return _resourceManager.findMesh(filePathHash);
}
