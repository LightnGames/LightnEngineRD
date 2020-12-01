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
	GpuDescriptorHandle indirectArgumentOffsetHandle = _scene.getVramShaderSetSystem()->getOffsetHandle()._gpuHandle;
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
		depthPrePassCulling(commandList, viewInfo, _gpuCullingPipelineState);
	}

	// build indirect arguments
	{
		commandList->setGraphicsRootSignature(_buildIndirectArgumentRootSignature);
		commandList->setPipelineState(_buildIndirectArgumentPipelineState);

		commandList->setComputeRootDescriptorTable(BuildIndirectArgumentRootParameters::BATCHED_SUBMESH_OFFSET, _scene.getBatchedSubMeshInfoOffsetSrv()._gpuHandle);
		commandList->setComputeRootDescriptorTable(BuildIndirectArgumentRootParameters::BATCHED_SUBMESH_COUNT, _view.getBatchedSubMeshInfoCountSrv()._gpuHandle);
		commandList->setComputeRootDescriptorTable(BuildIndirectArgumentRootParameters::INDIRECT_ARGUMENT, _view.getIndirectArgumentUav()._gpuHandle);

		u32 dispatchCount = RoundUp(Scene::PACKED_SUB_MESH_COUNT_MAX * VramShaderSetSystem::SHADER_SET_COUNT_MAX, 128u);
		commandList->dispatch(dispatchCount, 1, 1);
	}

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

			u32 indirectArgumentOffsetSizeInByte = pipelineStateIndex * 64 * sizeof(gpu::DispatchMeshIndirectArgument);
			u32 countBufferOffset = pipelineStateIndex * sizeof(u32);
			PipelineStateGroup* pipelineState = shaderSet->getDepthPipelineStateGroup();

			commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
			commandList->setPipelineState(pipelineState->getPipelineState());
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VIEW_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT, _debugFixedViewConstantHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH, meshHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH_INSTANCE, meshInstanceHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VERTEX_RESOURCES, vertexResourceDescriptors._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_TEXTURES, textureDescriptors._gpuHandle);
			_view.setDrawCurrentLodDescriptorTable(commandList);
			_view.render(commandList, pipelineState->getCommandSignature(), 64, indirectArgumentOffsetSizeInByte, countBufferOffset);

			//VramShaderSet* vramShaderSet = _scene.getVramShaderSetSystem()->getShaderSet(pipelineStateIndex);
			//u32 commandCountMax = vramShaderSet->getTotalRefCount();
			//if (commandCountMax == 0) {
			//	continue;
			//}

			//u32 indirectArgumentOffset = _scene.getVramShaderSetSystem()->getIndirectArgumentOffset(pipelineStateIndex);
			//u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::DispatchMeshIndirectArgument);
			//LTN_ASSERT(indirectArgumentOffset + commandCountMax <= GraphicsView::INDIRECT_ARGUMENT_COUNT_MAX);

			//u32 countBufferOffset = pipelineStateIndex * sizeof(u32);
			//PipelineStateGroup* pipelineState = shaderSet->getDepthPipelineStateGroup();

			//commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
			//commandList->setPipelineState(pipelineState->getPipelineState());
			//commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VIEW_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			//commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT, _debugFixedViewConstantHandle._gpuHandle);
			//commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
			//commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH, meshHandle);
			//commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH_INSTANCE, meshInstanceHandle);
			//commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VERTEX_RESOURCES, vertexResourceDescriptors._gpuHandle);
			//commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_TEXTURES, textureDescriptors._gpuHandle);
			//_view.setDrawCurrentLodDescriptorTable(commandList);
			//_view.render(commandList, pipelineState->getCommandSignature(), commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);
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
		PipelineState* mainCullingPipelineState = isPassedCulling ? _gpuCullingPassPipelineState : _gpuOcclusionCullingPipelineState;
		mainCulling(commandList, viewInfo, mainCullingPipelineState);
	}


	// 描画
	{
		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Main Pass");
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

			DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Shader %d", pipelineStateIndex);

			u32 indirectArgumentOffsetSizeInByte = pipelineStateIndex * 64 * sizeof(gpu::DispatchMeshIndirectArgument);
			u32 countBufferOffset = pipelineStateIndex * sizeof(u32);
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
			}

			if (_cullingDebugType & CULLING_DEBUG_TYPE_PASS_MESHLET_CULLING) {
				pipelineState = shaderSet->getDebugCullingPassPipelineStateGroup();
			}

			commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
			commandList->setPipelineState(pipelineState->getPipelineState());
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VIEW_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT, _debugFixedViewConstantHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH, meshHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH_INSTANCE, meshInstanceHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VERTEX_RESOURCES, vertexResourceDescriptors._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_TEXTURES, textureDescriptors._gpuHandle);
			_view.setDrawResultDescriptorTable(commandList);
			_view.setDrawCurrentLodDescriptorTable(commandList);
			_view.render(commandList, pipelineState->getCommandSignature(), commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);

			/*u32 indirectArgumentOffset = _scene.getVramShaderSetSystem()->getIndirectArgumentOffset(pipelineStateIndex);
			u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::DispatchMeshIndirectArgument);
			LTN_ASSERT(indirectArgumentOffset + commandCountMax <= GraphicsView::INDIRECT_ARGUMENT_COUNT_MAX);

			u32 countBufferOffset = pipelineStateIndex * sizeof(u32);
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
			}

			if (_cullingDebugType & CULLING_DEBUG_TYPE_PASS_MESHLET_CULLING) {
				pipelineState = shaderSet->getDebugCullingPassPipelineStateGroup();
			}

			commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
			commandList->setPipelineState(pipelineState->getPipelineState());
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VIEW_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT, _debugFixedViewConstantHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH, meshHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH_INSTANCE, meshInstanceHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VERTEX_RESOURCES, vertexResourceDescriptors._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_TEXTURES, textureDescriptors._gpuHandle);
			_view.setDrawResultDescriptorTable(commandList);
			_view.setDrawCurrentLodDescriptorTable(commandList);
			_view.render(commandList, pipelineState->getCommandSignature(), commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);*/
		}
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
	GpuDescriptorHandle indirectArgumentOffsetHandle = _scene.getVramShaderSetSystem()->getOffsetHandle()._gpuHandle;
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
		computeLod(commandList, viewInfo);
	}

	// デプスプリパス用　GPUカリング
	if (!isFixedCullingView) {
		depthPrePassCulling(commandList, viewInfo, _multiDrawCullingPipelineState);
	}

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

			u32 indirectArgumentOffset = _scene.getVramShaderSetSystem()->getIndirectArgumentOffset(pipelineStateIndex);
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

		queryHeapSystem->setCurrentMarkerName("Depth Prepass");
		queryHeapSystem->setMarker(commandList);
	}

	// build hiz
	if (!isFixedCullingView) {
		buildHiz(commandList, viewInfo);
	}

	// GPUカリング
	if (!isFixedCullingView) {
		mainCulling(commandList, viewInfo, _multiDrawOcclusionCullingPipelineState);
	}


	// 描画
	{
		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Main Pass");
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

			DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Shader %d", pipelineStateIndex);

			u32 indirectArgumentOffset = _scene.getVramShaderSetSystem()->getIndirectArgumentOffset(pipelineStateIndex);
			u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::StarndardMeshIndirectArguments);
			LTN_ASSERT(indirectArgumentOffset + commandCountMax <= GraphicsView::INDIRECT_ARGUMENT_COUNT_MAX);

			u32 countBufferOffset = pipelineStateIndex * sizeof(u32);
			ClassicShaderSet* classicShaderSet = shaderSet->getClassicShaderSet();

			commandList->setGraphicsRootSignature(classicShaderSet->_rootSignature);
			commandList->setPipelineState(classicShaderSet->_pipelineState);
			commandList->setVertexBuffers(0, LTN_COUNTOF(vertexBufferViews), vertexBufferViews);
			commandList->setIndexBuffer(&indexBufferView);
			commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MATERIALS, vramShaderSet->_materialParameterSrv._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_SCENE_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MESH_INSTANCE, meshInstanceHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_TEXTURES, textureDescriptors._gpuHandle);
			_view.render(commandList, classicShaderSet->_multiDrawCommandSignature, commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);
		}
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

void MeshRendererSystemImpl::computeLod(CommandList* commandList, ViewInfo* viewInfo) {
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	GpuDescriptorHandle meshInstanceHandle = _scene.getMeshInstanceHandles()._gpuHandle;
	GpuDescriptorHandle meshHandle = _resourceManager.getMeshHandle()._gpuHandle;
	u32 meshInstanceCountMax = _scene.getMeshInstanceCountMax();

	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::BLUE, "Compute Lod");

	commandList->setComputeRootSignature(_computeLodRootSignature);
	commandList->setPipelineState(_computeLodPipelineState);
	_view.setComputeLodResource(commandList);
	_view.resourceBarriersComputeLodToUAV(commandList);

	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_SCENE_INFO, _cullingSceneConstantHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_VIEW_INFO, viewInfo->_cbvHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_MESH, meshHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_MESH_INSTANCE, meshInstanceHandle);

	u32 dispatchCount = RoundUp(meshInstanceCountMax, 128u);
	commandList->dispatch(dispatchCount, 1, 1);

	_view.resetResourceComputeLodBarriers(commandList);

	queryHeapSystem->setCurrentMarkerName("Compute Lod");
	queryHeapSystem->setMarker(commandList);
}

void MeshRendererSystemImpl::depthPrePassCulling(CommandList* commandList, ViewInfo* viewInfo, PipelineState* pipelineState) {
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	GpuDescriptorHandle meshInstanceHandle = _scene.getMeshInstanceHandles()._gpuHandle;
	GpuDescriptorHandle meshHandle = _resourceManager.getMeshHandle()._gpuHandle;
	GpuDescriptorHandle subMeshDrawInfoHandle = _resourceManager.getSubMeshDrawInfoSrvHandle()._gpuHandle;
	GpuDescriptorHandle indirectArgumentOffsetHandle = _scene.getVramShaderSetSystem()->getOffsetHandle()._gpuHandle;
	GpuDescriptorHandle batchedSubMeshInfoOffsetSrv = _scene.getBatchedSubMeshInfoOffsetSrv()._gpuHandle;
	u32 meshInstanceCountMax = _scene.getMeshInstanceCountMax();

	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::GREEN, "Depth Prepass Culling");

	_view.resourceBarriersGpuCullingToUAV(commandList);
	_view.resetCountBuffers(commandList);

	commandList->setComputeRootSignature(_gpuCullingRootSignature);
	commandList->setPipelineState(pipelineState);
	_view.setGpuFrustumCullingResources(commandList);

	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_CULLING_SCENE_INFO, _cullingSceneConstantHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_VIEW_INFO, viewInfo->_depthPrePassCbvHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MESH, meshHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MESH_INSTANCE, meshInstanceHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_SUB_MESH_DRAW_INFO, subMeshDrawInfoHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_BATCHED_SUBMESH_INFO, indirectArgumentOffsetHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_BATCHED_SUBMESH_OFFSET, batchedSubMeshInfoOffsetSrv);

	u32 dispatchCount = RoundUp(meshInstanceCountMax, 128u);
	commandList->dispatch(dispatchCount, 1, 1);

	_view.resetResourceGpuCullingBarriers(commandList);

	queryHeapSystem->setCurrentMarkerName("Depth Prepass Culling");
	queryHeapSystem->setMarker(commandList);
}

void MeshRendererSystemImpl::mainCulling(CommandList* commandList, ViewInfo* viewInfo, PipelineState* pipelineState) {
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	GpuDescriptorHandle meshInstanceHandle = _scene.getMeshInstanceHandles()._gpuHandle;
	GpuDescriptorHandle meshHandle = _resourceManager.getMeshHandle()._gpuHandle;
	GpuDescriptorHandle indirectArgumentOffsetHandle = _scene.getVramShaderSetSystem()->getOffsetHandle()._gpuHandle;
	GpuDescriptorHandle subMeshDrawInfoHandle = _resourceManager.getSubMeshDrawInfoSrvHandle()._gpuHandle;
	GpuDescriptorHandle batchedSubMeshInfoOffsetSrv = _scene.getBatchedSubMeshInfoOffsetSrv()._gpuHandle;
	u32 meshInstanceCountMax = _scene.getMeshInstanceCountMax();

	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_GREEN, "Main Culling");

	_view.resourceBarriersGpuCullingToUAV(commandList);
	_view.resetCountBuffers(commandList);

	commandList->setComputeRootSignature(_gpuOcclusionCullingRootSignature);
	commandList->setPipelineState(pipelineState);
	_view.setGpuCullingResources(commandList);

	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_CULLING_SCENE_INFO, _cullingSceneConstantHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_VIEW_INFO, viewInfo->_cbvHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MESH, meshHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MESH_INSTANCE, meshInstanceHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_SUB_MESH_DRAW_INFO, subMeshDrawInfoHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_BATCHED_SUBMESH_INFO, indirectArgumentOffsetHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_BATCHED_SUBMESH_OFFSET, batchedSubMeshInfoOffsetSrv);

	u32 dispatchCount = RoundUp(meshInstanceCountMax, 128u);
	commandList->dispatch(dispatchCount, 1, 1);

	_view.resetResourceGpuCullingBarriers(commandList);
	viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_DEPTH_WRITE);

	queryHeapSystem->setCurrentMarkerName("Main Culling");
	queryHeapSystem->setMarker(commandList);
}

void MeshRendererSystemImpl::buildHiz(CommandList* commandList, ViewInfo* viewInfo) {
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_BLUE, "Build Hiz");

	_view.resourceBarriersHizSrvToUav(commandList);
	viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->setComputeRootSignature(_buildHizRootSignature);
	commandList->setPipelineState(_buildHizPipelineState);

	// pass 0
	{
		commandList->setComputeRootDescriptorTable(ROOT_PARAM_HIZ_INPUT_DEPTH, viewInfo->_depthSrv._gpuHandle);
		_view.setHizResourcesPass0(commandList);

		u32 dispatchWidthCount = RoundUp(static_cast<u32>(viewInfo->_viewPort._width), 16u);
		u32 dispatchHeightCount = RoundUp(static_cast<u32>(viewInfo->_viewPort._height), 16u);
		commandList->dispatch(dispatchWidthCount, dispatchHeightCount, 1);
	}

	// pass 1
	{
		_view.setHizResourcesPass1(commandList);

		ResourceDesc hizLevel3Desc = _view.getHizTextureResourceDesc(3);
		u32 dispatchWidthCount = RoundUp(static_cast<u32>(hizLevel3Desc._width), 16u);
		u32 dispatchHeightCount = RoundUp(static_cast<u32>(hizLevel3Desc._height), 16u);
		commandList->dispatch(dispatchWidthCount, dispatchHeightCount, 1);
	}

	_view.resourceBarriersHizUavtoSrv(commandList);

	queryHeapSystem->setCurrentMarkerName("Build Hiz");
	queryHeapSystem->setMarker(commandList);
}

void MeshRendererSystemImpl::mainCulling(CommandList* commandList, ViewInfo* viewInfo) {}
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

	Device* device = GraphicsSystemImpl::Get()->getDevice();
	GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();

	// gpu culling
	{
		DescriptorRange sceneCullingConstantRange = {};
		sceneCullingConstantRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		DescriptorRange cullingViewInfoConstantRange = {};
		cullingViewInfoConstantRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

		DescriptorRange viewInfoConstantRange = {};
		viewInfoConstantRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);

		DescriptorRange meshDescriptorRange = {};
		meshDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);

		DescriptorRange meshInstanceDescriptorRange = {};
		meshInstanceDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 3, 3);

		DescriptorRange batchedSubMeshInfoUavRange = {};
		batchedSubMeshInfoUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);

		DescriptorRange cullingResultUavRange = {};
		cullingResultUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);

		DescriptorRange lodLevelSrvRange = {};
		lodLevelSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);

		DescriptorRange batchedSubMeshInfoOffsetSrvRange = {};
		batchedSubMeshInfoOffsetSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);

		DescriptorRange batchedSubMeshInfoSrvRange = {};
		batchedSubMeshInfoSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 9);

		DescriptorRange hizSrvRange = {};
		hizSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, GraphicsView::HIERACHICAL_DEPTH_COUNT, 10);


		// フラスタムカリングのみ
		{
			_gpuCullingPipelineState = allocator->allocatePipelineState();
			_gpuCullingRootSignature = allocator->allocateRootSignature();

			constexpr u32 rootParamCount = ROOT_PARAM_GPU_COUNT - 2;
			RootParameter rootParameters[rootParamCount] = {};
			rootParameters[ROOT_PARAM_GPU_CULLING_SCENE_INFO].initializeDescriptorTable(1, &sceneCullingConstantRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_CULLING_VIEW_INFO].initializeDescriptorTable(1, &cullingViewInfoConstantRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_VIEW_INFO].initializeDescriptorTable(1, &viewInfoConstantRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_BATCHED_SUBMESH_INFO].initializeDescriptorTable(1, &batchedSubMeshInfoUavRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_BATCHED_SUBMESH_OFFSET].initializeDescriptorTable(1, &batchedSubMeshInfoOffsetSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_LOD_LEVEL].initializeDescriptorTable(1, &lodLevelSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_SUB_MESH_DRAW_INFO].initializeDescriptorTable(1, &batchedSubMeshInfoSrvRange, SHADER_VISIBILITY_ALL);

			RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;
			rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
			rootSignatureDesc._parameters = rootParameters;
			_gpuCullingRootSignature->iniaitlize(rootSignatureDesc);

			ComputePipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._rootSignature = _gpuCullingRootSignature;
			{
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/mesh_shader_gpu_driven/mesh_culling_frustum.cso");

				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_gpuCullingPipelineState->iniaitlize(pipelineStateDesc);

				computeShader->terminate();
			}

#if ENABLE_MULTI_INDIRECT_DRAW
			_multiDrawCullingPipelineState = allocator->allocatePipelineState();
			{
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/standard_gpu_driven/mesh_culling_frustum.cso");
				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_multiDrawCullingPipelineState->iniaitlize(pipelineStateDesc);

				computeShader->terminate();
			}
#endif
		}

		// フラスタムカリング ＋ オクルージョンカリング
		{
			_gpuOcclusionCullingPipelineState = allocator->allocatePipelineState();
			_gpuOcclusionCullingRootSignature = allocator->allocateRootSignature();

			RootParameter rootParameters[ROOT_PARAM_GPU_COUNT] = {};
			rootParameters[ROOT_PARAM_GPU_CULLING_SCENE_INFO].initializeDescriptorTable(1, &sceneCullingConstantRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_CULLING_VIEW_INFO].initializeDescriptorTable(1, &cullingViewInfoConstantRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_VIEW_INFO].initializeDescriptorTable(1, &viewInfoConstantRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_BATCHED_SUBMESH_INFO].initializeDescriptorTable(1, &batchedSubMeshInfoUavRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_BATCHED_SUBMESH_OFFSET].initializeDescriptorTable(1, &batchedSubMeshInfoOffsetSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_CULLING_RESULT].initializeDescriptorTable(1, &cullingResultUavRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_LOD_LEVEL].initializeDescriptorTable(1, &lodLevelSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_SUB_MESH_DRAW_INFO].initializeDescriptorTable(1, &batchedSubMeshInfoSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_HIZ].initializeDescriptorTable(1, &hizSrvRange, SHADER_VISIBILITY_ALL);

			RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;
			rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
			rootSignatureDesc._parameters = rootParameters;
			_gpuOcclusionCullingRootSignature->iniaitlize(rootSignatureDesc);

			ComputePipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._rootSignature = _gpuOcclusionCullingRootSignature;
			// standard
			{
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/mesh_shader_gpu_driven/mesh_culling_frustum_occlusion.cso");

				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_gpuOcclusionCullingPipelineState->iniaitlize(pipelineStateDesc);

				computeShader->terminate();
			}

			// カリングオフ
			{
				_gpuCullingPassPipelineState = allocator->allocatePipelineState();
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/mesh_shader_gpu_driven/mesh_culling_pass.cso");
				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_gpuCullingPassPipelineState->iniaitlize(pipelineStateDesc);

				computeShader->terminate();
			}

#if ENABLE_MULTI_INDIRECT_DRAW
			_multiDrawOcclusionCullingPipelineState = allocator->allocatePipelineState();
			{
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/standard_gpu_driven/mesh_culling_frustum_occlusion.cso");

				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_multiDrawOcclusionCullingPipelineState->iniaitlize(pipelineStateDesc);

				computeShader->terminate();
			}
#endif
		}
	}

	// build indirect arguments
	{
		_buildIndirectArgumentPipelineState = allocator->allocatePipelineState();
		_buildIndirectArgumentRootSignature = allocator->allocateRootSignature();

		DescriptorRange batchedSubMeshInfoOffsetSrvRange = {};
		batchedSubMeshInfoOffsetSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		DescriptorRange batchedSubMeshInfoCountSrvRange = {};
		batchedSubMeshInfoCountSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		DescriptorRange indirectArgumentUavRange = {};
		indirectArgumentUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);

		RootParameter rootParameters[BuildIndirectArgumentRootParameters::ROOT_PARAM_COUNT] = {};
		rootParameters[BuildIndirectArgumentRootParameters::BATCHED_SUBMESH_OFFSET].initializeDescriptorTable(1, &batchedSubMeshInfoOffsetSrvRange, SHADER_VISIBILITY_ALL);
		rootParameters[BuildIndirectArgumentRootParameters::BATCHED_SUBMESH_COUNT].initializeDescriptorTable(1, &batchedSubMeshInfoCountSrvRange, SHADER_VISIBILITY_ALL);
		rootParameters[BuildIndirectArgumentRootParameters::INDIRECT_ARGUMENT].initializeDescriptorTable(1, &indirectArgumentUavRange, SHADER_VISIBILITY_ALL);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_buildIndirectArgumentRootSignature->iniaitlize(rootSignatureDesc);

		ShaderBlob* computeShader = allocator->allocateShaderBlob();
		computeShader->initialize("L:/LightnEngine/resource/common/shader/mesh_shader_gpu_driven/build_indirect_arguments.cso");

		ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._cs = computeShader->getShaderByteCode();
		pipelineStateDesc._rootSignature = _buildIndirectArgumentRootSignature;
		_buildIndirectArgumentPipelineState->iniaitlize(pipelineStateDesc);

		computeShader->terminate();
	}

	// gpu compute lod 
	{
		_computeLodPipelineState = allocator->allocatePipelineState();
		_computeLodRootSignature = allocator->allocateRootSignature();

		DescriptorRange sceneCullingConstantRange = {};
		sceneCullingConstantRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		DescriptorRange cullingViewInfoConstantRange = {};
		cullingViewInfoConstantRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

		DescriptorRange meshDescriptorRange = {};
		meshDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

		DescriptorRange meshInstanceDescriptorRange = {};
		meshInstanceDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 2, 3);

		DescriptorRange resultLodLevelDescriptorRange = {};
		resultLodLevelDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		RootParameter rootParameters[ROOT_PARAM_LOD_COUNT] = {};
		rootParameters[ROOT_PARAM_LOD_SCENE_INFO].initializeDescriptorTable(1, &sceneCullingConstantRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_LOD_VIEW_INFO].initializeDescriptorTable(1, &cullingViewInfoConstantRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_LOD_MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_LOD_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_LOD_RESULT_LEVEL].initializeDescriptorTable(1, &resultLodLevelDescriptorRange, SHADER_VISIBILITY_ALL);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_computeLodRootSignature->iniaitlize(rootSignatureDesc);

		ShaderBlob* computeShader = allocator->allocateShaderBlob();
		computeShader->initialize("L:/LightnEngine/resource/common/shader/compute_lod.cso");

		ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._cs = computeShader->getShaderByteCode();
		pipelineStateDesc._rootSignature = _computeLodRootSignature;
		_computeLodPipelineState->iniaitlize(pipelineStateDesc);

		computeShader->terminate();
	}

	// debug meshlet bounds
	{
		_debugMeshletBoundsPipelineState = allocator->allocatePipelineState();
		_debugMeshletBoundsRootSignature = allocator->allocateRootSignature();

		DescriptorRange sceneCullingConstantRange = {};
		sceneCullingConstantRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		DescriptorRange meshDescriptorRange = {};
		meshDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);

		DescriptorRange meshInstanceDescriptorRange = {};
		meshInstanceDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);

		DescriptorRange currentLodLevelRange = {};
		currentLodLevelRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);

		DescriptorRange lineDrawIndirectRange = {};
		lineDrawIndirectRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);

		RootParameter rootParameters[ROOT_PARAM_DEBUG_MESHLET_COUNT] = {};
		rootParameters[ROOT_PARAM_DEBUG_MESHLET_SCENE_INFO].initializeDescriptorTable(1, &sceneCullingConstantRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_DEBUG_MESHLET_MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_DEBUG_MESHLET_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_DEBUG_MESHLET_LOD_LEVEL].initializeDescriptorTable(1, &currentLodLevelRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_DEBUG_MESHLET_INDIRECT_ARGUMENT].initializeDescriptorTable(1, &lineDrawIndirectRange, SHADER_VISIBILITY_ALL);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_debugMeshletBoundsRootSignature->iniaitlize(rootSignatureDesc);

		ShaderBlob* computeShader = allocator->allocateShaderBlob();
		computeShader->initialize("L:/LightnEngine/resource/common/shader/debug/debug_draw_meshlet_bounds.cso");

		ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._cs = computeShader->getShaderByteCode();
		pipelineStateDesc._rootSignature = _debugMeshletBoundsRootSignature;
		_debugMeshletBoundsPipelineState->iniaitlize(pipelineStateDesc);

		computeShader->terminate();
	}

	// build hiz
	{
		_buildHizPipelineState = allocator->allocatePipelineState();
		_buildHizRootSignature = allocator->allocateRootSignature();

		DescriptorRange buildHizInfoConstantRange = {};
		buildHizInfoConstantRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		DescriptorRange inputDepthRange = {};
		inputDepthRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		DescriptorRange outpuDepthRange = {};
		outpuDepthRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, GraphicsView::HIERACHICAL_DEPTH_COUNT / 2, 0);

		RootParameter rootParameters[ROOT_PARAM_HIZ_COUNT] = {};
		rootParameters[ROOT_PARAM_HIZ_INFO].initializeDescriptorTable(1, &buildHizInfoConstantRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_HIZ_INPUT_DEPTH].initializeDescriptorTable(1, &inputDepthRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_HIZ_OUTPUT_DEPTH].initializeDescriptorTable(1, &outpuDepthRange, SHADER_VISIBILITY_ALL);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_buildHizRootSignature->iniaitlize(rootSignatureDesc);

		ShaderBlob* computeShader = allocator->allocateShaderBlob();
		computeShader->initialize("L:/LightnEngine/resource/common/shader/build_hierarchical_depth.cso");

		ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._cs = computeShader->getShaderByteCode();
		pipelineStateDesc._rootSignature = _buildHizRootSignature;
		_buildHizPipelineState->iniaitlize(pipelineStateDesc);

		computeShader->terminate();
	}

	_view.initialize(ViewSystemImpl::Get()->getView());

	DescriptorHeapAllocator* descriptorHeapAllocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();

	// gpu culling constant buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = GetConstantBufferAligned(sizeof(SceneCullingInfo));
		desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._device = device;
		_sceneCullingConstantBuffer.initialize(desc);
		_sceneCullingConstantBuffer.setDebugName("Culling Constant");
	}

	// gpu culling cbv descriptors
	{
		_cullingSceneConstantHandle = descriptorHeapAllocater->allocateDescriptors(1);
		device->createConstantBufferView(_sceneCullingConstantBuffer.getConstantBufferViewDesc(), _cullingSceneConstantHandle._cpuHandle);
	}

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
	_view.terminate();

	_debugFixedViewConstantBuffer.terminate();
	_gpuCullingPassPipelineState->terminate();
	_gpuOcclusionCullingPipelineState->terminate();
	_gpuOcclusionCullingRootSignature->terminate();
	_gpuCullingPipelineState->terminate();
	_gpuCullingRootSignature->terminate();
	_buildIndirectArgumentPipelineState->terminate();
	_buildIndirectArgumentRootSignature->terminate();
	_computeLodPipelineState->terminate();
	_computeLodRootSignature->terminate();
	_buildHizPipelineState->terminate();
	_buildHizRootSignature->terminate();
	_debugMeshletBoundsPipelineState->terminate();
	_debugMeshletBoundsRootSignature->terminate();

	_sceneCullingConstantBuffer.terminate();
#if ENABLE_MULTI_INDIRECT_DRAW
	_multiDrawCullingPipelineState->terminate();
	_multiDrawOcclusionCullingPipelineState->terminate();
#endif

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocator->discardDescriptor(_cullingSceneConstantHandle);
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

		const char* primitiveTypes[] = { "Default", "Meshlet", "LodLevel", "Occlusion","Depth","Texcoords","World Normal" };
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

	constexpr char format1[] = "%-20s %-10d";
	DebugWindow::StartWindow("Scene Instances");
	DebugGui::Text(format1, "Mesh", _resourceManager.getMeshCount());
	DebugGui::Text(format1, "LOD Mesh", _resourceManager.getLodMeshCount());
	DebugGui::Text(format1, "Sub Mesh", _resourceManager.getSubMeshCount());
	DebugGui::Text(format1, "Meshlet", _resourceManager.getMeshletCount());

	DebugGui::Text(format1, "Mesh Instance", _scene.getMeshInstanceCount());
	DebugGui::Text(format1, "LOD Mesh Instance", _scene.getLodMeshInstanceCount());
	DebugGui::Text(format1, "Sub Mesh Instance", _scene.getSubMeshInstanceCount());
	DebugWindow::End();

	DebugWindow::StartWindow("Scene Meshs");
	constexpr char format2[] = "%-12s";
	constexpr char format3[] = "%-7.3f%% ( %-6d/ %-6d)";
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

	u32 meshInstanceCountMax = _scene.getMeshInstanceCountMax();
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	SceneCullingInfo* cullingInfo = vramUpdater->enqueueUpdate<SceneCullingInfo>(&_sceneCullingConstantBuffer, 0, 1);
	cullingInfo->_meshInstanceCountMax = meshInstanceCountMax;
}

void MeshRendererSystemImpl::render(CommandList* commandList, ViewInfo* viewInfo) {
	if (!_visible) {
		return;
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
