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
	GpuDescriptorHandle meshInstanceHandle = _scene.getMeshInstanceSrv();
	GpuDescriptorHandle meshHandle = _resourceManager.getMeshSrv();
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	DescriptorHandle textureDescriptors = TextureSystemImpl::Get()->getDescriptors();

	DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4::RED, "Mesh Shader Pass");

	_gpuCullingResource.resetResultBuffers(commandList);

	bool isFixedCullingView = _cullingDebugType & CULLING_DEBUG_TYPE_FIXED_VIEW;
	if (!isFixedCullingView) {
		setFixedDebugView(commandList, viewInfo);
	}

	// Lod level 計算
	if (!isFixedCullingView) {
		ComputeLodContext context = {};
		context._commandList = commandList;
		context._gpuCullingResource = &_gpuCullingResource;
		context._viewInfo = viewInfo;
		context._meshHandle = _resourceManager.getMeshSrv();
		context._meshInstanceHandle = _scene.getMeshInstanceSrv();
		context._meshInstanceCountMax = _scene.getMeshInstanceCountMax();
		context._sceneConstantCbv = _scene.getSceneCbv();
		_meshRenderer.computeLod(context);
	}

	// メッシュレットバウンディング　デバッグ表示
	if (_debugDrawMeshletBounds) {
		//DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::YELLOW, "Debug Meshlet Bounds");

		//DebugRendererSystemImpl* debugSystem = DebugRendererSystemImpl::Get();
		//commandList->setComputeRootSignature(_debugMeshletBoundsRootSignature);
		//commandList->setPipelineState(_debugMeshletBoundsPipelineState);

		//commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_SCENE_INFO, _cullingSceneConstantHandle._gpuHandle);
		//commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_MESH, meshHandle);
		//commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_MESH_INSTANCE, meshInstanceHandle);
		//commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_LOD_LEVEL, _view.getCurrentLodLevelSrv()._gpuHandle);
		//commandList->setComputeRootDescriptorTable(ROOT_PARAM_DEBUG_MESHLET_INDIRECT_ARGUMENT, debugSystem->getLineGpuUavHandle()._gpuHandle);

		//u32 dispatchCount = RoundUp(meshInstanceCountMax, 128u);
		//commandList->dispatch(dispatchCount, 1, 1);

		//queryHeapSystem->setCurrentMarkerName("Debug Meshlet Bounds");
		//queryHeapSystem->setMarker(commandList);
	}

	// デプスプリパス用　GPUカリング
	if (!isFixedCullingView) {
		GpuCullingContext context = {};
		context._commandList = commandList;
		context._instancingResource = &_primitiveInstancingResource;
		context._gpuCullingResource = &_gpuCullingResource;
		context._cullingViewCbv = viewInfo->_depthPrePassCbvHandle._gpuHandle;
		context._meshHandle = _resourceManager.getMeshSrv();
		context._meshInstanceSrv = _scene.getMeshInstanceSrv();
		context._meshInstanceCountMax = _scene.getMeshInstanceCountMax();
		context._sceneConstantCbv = _scene.getSceneCbv();
		context._indirectArgumentOffsetSrv = _primitiveInstancingResource.getInfoOffsetSrv();
		context._subMeshDrawInfoSrv = _resourceManager.getSubMeshDrawInfoSrv();
		context._materialInstanceIndexSrv = _vramShaderSetSystem.getMaterialInstanceIndexSrv()._gpuHandle;
		context._scopeName = "Depth Pre Pass Culling";
		_meshRenderer.depthPrePassCulling(context);
	}

	// Build indirect argument
	{
		BuildIndirectArgumentContext context = {};
		context._commandList = commandList;
		context._indirectArgumentResource = &_indirectArgumentResource;
		context._primIndirectArgumentResource = &_primIndirectArgumentResource;
		context._meshletInstanceCountSrv = _primitiveInstancingResource.getInfoCountSrv();
		context._meshletInstanceOffsetSrv = _primitiveInstancingResource.getInfoOffsetSrv();
		context._subMeshSrv = _resourceManager.getSubMeshSrv();
		context._buildResource = &_buildIndirectArgumentResource;
		_meshRenderer.buildIndirectArgument(context);
	}

	// デプスプリパス
	{
		DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4::YELLOW, "Depth Prepass");
		commandList->setViewports(1, &viewInfo->_viewPort);
		commandList->setScissorRects(1, &viewInfo->_scissorRect);

		PipelineStateSet* pipelineStateSet = materialSystem->getPipelineStateSet(MaterialSystemImpl::TYPE_MESH_SHADER);
		PipelineStateSet* primPipelineStateSet = materialSystem->getPipelineStateSet(MaterialSystemImpl::TYPE_MESH_SHADER_PRIM_INSTANCING);
		RenderContext context = {};
		context._commandList = commandList;
		context._viewInfo = viewInfo;
		context._indirectArgumentResource = &_indirectArgumentResource;
		context._primIndirectArgumentResource = &_primIndirectArgumentResource;
		context._debugFixedViewCbv = _debugFixedViewConstantHandle._gpuHandle;
		context._vramShaderSets = _vramShaderSetSystem.getShaderSet(0);
		context._meshSrv = _resourceManager.getMeshSrv();
		context._vertexResourceDescriptors = _resourceManager.getVertexSrv();
		context._pipelineStates = pipelineStateSet->_depthPipelineStateGroups;
		context._primInstancingPipelineStates = primPipelineStateSet->_depthPipelineStateGroups;
		context._primCommandSignatures = primPipelineStateSet->_commandSignatures;
		context._commandSignatures = pipelineStateSet->_commandSignatures;
		context._instancingResource = &_primitiveInstancingResource;
		context._gpuCullingResource = &_gpuCullingResource;
		context._scene = &_scene;
		_meshRenderer.render(context);
	}

	// build hiz
	if (!isFixedCullingView) {
		BuildHizContext context = {};
		context._commandList = commandList;
		context._gpuCullingResource = &_gpuCullingResource;
		context._viewInfo = viewInfo;
		_meshRenderer.buildHiz(context);
	}

	// GPUカリング
	if (!isFixedCullingView) {
		GpuCullingContext context = {};
		context._commandList = commandList;
		context._instancingResource = &_primitiveInstancingResource;
		context._gpuCullingResource = &_gpuCullingResource;
		context._cullingViewCbv = viewInfo->_cbvHandle._gpuHandle;
		context._meshHandle = _resourceManager.getMeshSrv();
		context._meshInstanceSrv = _scene.getMeshInstanceSrv();
		context._meshInstanceCountMax = _scene.getMeshInstanceCountMax();
		context._sceneConstantCbv = _scene.getSceneCbv();
		context._indirectArgumentOffsetSrv = _primitiveInstancingResource.getInfoOffsetSrv();
		context._subMeshDrawInfoSrv = _resourceManager.getSubMeshDrawInfoSrv();
		context._materialInstanceIndexSrv = _vramShaderSetSystem.getMaterialInstanceIndexSrv()._gpuHandle;
		context._scopeName = "Main Culling";
		_meshRenderer.mainCulling(context);
	}

	// Build indirect argument
	{
		BuildIndirectArgumentContext context = {};
		context._commandList = commandList;
		context._indirectArgumentResource = &_indirectArgumentResource;
		context._primIndirectArgumentResource = &_primIndirectArgumentResource;
		context._meshletInstanceCountSrv = _primitiveInstancingResource.getInfoCountSrv();
		context._meshletInstanceOffsetSrv = _primitiveInstancingResource.getInfoOffsetSrv();
		context._subMeshSrv = _resourceManager.getSubMeshSrv();
		context._buildResource = &_buildIndirectArgumentResource;
		_meshRenderer.buildIndirectArgument(context);
	}

	// 描画
	{
		DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Main Pass");
		commandList->setViewports(1, &viewInfo->_viewPort);
		commandList->setScissorRects(1, &viewInfo->_scissorRect);

		PipelineStateSet* pipelineStateSet = materialSystem->getPipelineStateSet(MaterialSystemImpl::TYPE_MESH_SHADER);
		PipelineStateSet* primPipelineStateSet = materialSystem->getPipelineStateSet(MaterialSystemImpl::TYPE_MESH_SHADER_PRIM_INSTANCING);
		PipelineStateGroup** pipelineStates = nullptr;
		PipelineStateGroup** msPipelineStates = nullptr;
		switch (_debugPrimitiveType) {
		case DEBUG_PRIMITIVE_TYPE_DEFAULT:
			pipelineStates = pipelineStateSet->_pipelineStateGroups;
			msPipelineStates = primPipelineStateSet->_pipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_MESHLET:
			pipelineStates = pipelineStateSet->_debugMeshletPipelineStateGroups;
			msPipelineStates = primPipelineStateSet->_debugMeshletPipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_LODLEVEL:
			pipelineStates = pipelineStateSet->_debugLodLevelPipelineStateGroups;
			msPipelineStates = primPipelineStateSet->_debugLodLevelPipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_OCCLUSION:
			pipelineStates = pipelineStateSet->_debugOcclusionPipelineStateGroups;
			msPipelineStates = primPipelineStateSet->_debugOcclusionPipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_DEPTH:
			pipelineStates = pipelineStateSet->_depthPipelineStateGroups;
			msPipelineStates = primPipelineStateSet->_debugDepthPipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_TEXCOORDS:
			pipelineStates = pipelineStateSet->_debugTexcoordsPipelineStateGroups;
			msPipelineStates = primPipelineStateSet->_debugTexcoordsPipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_WIREFRAME:
			pipelineStates = pipelineStateSet->_debugWireFramePipelineStateGroups;
			msPipelineStates = primPipelineStateSet->_debugWireFramePipelineStateGroups;
			break;
		}

		if (_cullingDebugType & CULLING_DEBUG_TYPE_PASS_MESHLET_CULLING) {
			pipelineStates = pipelineStateSet->_debugCullingPassPipelineStateGroups;
			msPipelineStates = primPipelineStateSet->_debugCullingPassPipelineStateGroups;
		}

		LTN_ASSERT(pipelineStates != nullptr);
		_gpuCullingResource.resourceBarriersHizTextureToSrv(commandList);

		RenderContext context = {};
		context._commandList = commandList;
		context._viewInfo = viewInfo;
		context._indirectArgumentResource = &_indirectArgumentResource;
		context._primIndirectArgumentResource = &_primIndirectArgumentResource;
		context._instancingResource = &_primitiveInstancingResource;
		context._gpuCullingResource = &_gpuCullingResource;
		context._debugFixedViewCbv = _debugFixedViewConstantHandle._gpuHandle;
		context._vramShaderSets = _vramShaderSetSystem.getShaderSet(0);
		context._meshSrv = _resourceManager.getMeshSrv();
		context._vertexResourceDescriptors = _resourceManager.getVertexSrv();
		context._pipelineStates = pipelineStates;
		context._primInstancingPipelineStates = msPipelineStates;
		context._commandSignatures = pipelineStateSet->_commandSignatures;
		context._primCommandSignatures = primPipelineStateSet->_commandSignatures;
		context._scene = &_scene;
		context._collectResult = true;
		_meshRenderer.render(context);

		_gpuCullingResource.resourceBarriersHizSrvToTexture(commandList);
	}

	{
		DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Readback Culling Result");
		_gpuCullingResource.readbackCullingResultBuffer(commandList);
	}
}
#endif

#if ENABLE_MULTI_INDIRECT_DRAW
void MeshRendererSystemImpl::renderMultiIndirect(CommandList* commandList, ViewInfo* viewInfo) {
	GpuDescriptorHandle meshInstanceHandle = _scene.getMeshInstanceSrv();
	GpuDescriptorHandle meshHandle = _resourceManager.getMeshSrv();
	u32 meshInstanceCountMax = _scene.getMeshInstanceCountMax();
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	DescriptorHandle textureDescriptors = TextureSystemImpl::Get()->getDescriptors();
	u32 pipelineStateResourceCount = materialSystem->getShaderSetCount();

	DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4::RED, "Multi Draw Pass");

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

	_gpuCullingResource.resetResultBuffers(commandList);

	bool isFixedCullingView = _cullingDebugType & CULLING_DEBUG_TYPE_FIXED_VIEW;
	if (!isFixedCullingView) {
		setFixedDebugView(commandList, viewInfo);
	}

	// Lod level 計算
	if (!isFixedCullingView) {
		ComputeLodContext context = {};
		context._commandList = commandList;
		context._gpuCullingResource = &_gpuCullingResource;
		context._viewInfo = viewInfo;
		context._meshHandle = _resourceManager.getMeshSrv();
		context._meshInstanceHandle = _scene.getMeshInstanceSrv();
		context._meshInstanceCountMax = _scene.getMeshInstanceCountMax();
		context._sceneConstantCbv = _scene.getSceneCbv();
		_meshRenderer.computeLod(context);
	}

	// デプスプリパス用　GPUカリング
	if (!isFixedCullingView) {
		MultiDrawGpuCullingContext context = {};
		context._commandList = commandList;
		context._indirectArgumentResource = &_multiDrawIndirectArgumentResource;
		context._gpuCullingResource = &_gpuCullingResource;
		context._cullingViewCbv = viewInfo->_depthPrePassCbvHandle._gpuHandle;
		context._meshSrv = _resourceManager.getMeshSrv();
		context._meshInstanceSrv = _scene.getMeshInstanceSrv();
		context._meshInstanceCountMax = _scene.getMeshInstanceCountMax();
		context._sceneConstantCbv = _scene.getSceneCbv();
		context._indirectArgumentOffsetSrv = _multiDrawInstancingResource.getIndirectArgumentOffsetSrv();
		context._subMeshDrawInfoHandle = _resourceManager.getSubMeshDrawInfoSrv();
		context._materialInstanceIndexSrv = _vramShaderSetSystem.getMaterialInstanceIndexSrv()._gpuHandle;
		context._scopeName = "Depth Pre Pass Culling";
		_meshRenderer.multiDrawDepthPrePassCulling(context);
	}

	// デプスプリパス
	{
		DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4::YELLOW, "Depth Prepass");

		PipelineStateSet* pipelineStateSet = materialSystem->getPipelineStateSet(MaterialSystemImpl::TYPE_CLASSIC);
		MultiIndirectRenderContext context = {};
		context._commandList = commandList;
		context._viewInfo = viewInfo;
		context._indirectArgumentResource = &_multiDrawIndirectArgumentResource;
		context._gpuCullingResource = &_gpuCullingResource;
		context._vramShaderSets = _vramShaderSetSystem.getShaderSet(0);
		context._indirectArgmentOffsets = _multiDrawInstancingResource.getIndirectArgumentOffsets();
		context._indirectArgmentCounts = _multiDrawInstancingResource.getIndirectArgumentCounts();
		context._meshInstanceWorldMatrixSrv = _scene.getMeshInstanceWorldMatrixSrv();
		context._pipelineStates = pipelineStateSet->_depthPipelineStateGroups;
		context._commandSignatures = pipelineStateSet->_commandSignatures;
		context._vertexBufferViews = vertexBufferViews;
		context._indexBufferView = &indexBufferView;
		context._numVertexBufferView = LTN_COUNTOF(vertexBufferViews);
		_meshRenderer.multiDrawRender(context);
	}

	// build hiz
	if (!isFixedCullingView) {
		BuildHizContext context = {};
		context._commandList = commandList;
		context._gpuCullingResource = &_gpuCullingResource;
		context._viewInfo = viewInfo;
		_meshRenderer.buildHiz(context);
	}

	// GPUカリング
	if (!isFixedCullingView) {
		MultiDrawGpuCullingContext context = {};
		context._commandList = commandList;
		context._indirectArgumentResource = &_multiDrawIndirectArgumentResource;
		context._gpuCullingResource = &_gpuCullingResource;
		context._cullingViewCbv = viewInfo->_cbvHandle._gpuHandle;
		context._meshSrv = _resourceManager.getMeshSrv();
		context._meshInstanceSrv = _scene.getMeshInstanceSrv();
		context._meshInstanceCountMax = _scene.getMeshInstanceCountMax();
		context._sceneConstantCbv = _scene.getSceneCbv();
		context._indirectArgumentOffsetSrv = _multiDrawInstancingResource.getIndirectArgumentOffsetSrv();
		context._subMeshDrawInfoHandle = _resourceManager.getSubMeshDrawInfoSrv();
		context._materialInstanceIndexSrv = _vramShaderSetSystem.getMaterialInstanceIndexSrv()._gpuHandle;
		context._scopeName = "Main Culling";
		_meshRenderer.multiDrawMainCulling(context);
	}

	// 描画
	{
		DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Main Pass");

		PipelineStateSet* pipelineStateSet = materialSystem->getPipelineStateSet(MaterialSystemImpl::TYPE_CLASSIC);
		PipelineStateGroup** pipelineStates = nullptr;
		switch (_debugPrimitiveType) {
		case DEBUG_PRIMITIVE_TYPE_DEFAULT:
			pipelineStates = pipelineStateSet->_pipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_MESHLET:
			pipelineStates = pipelineStateSet->_debugMeshletPipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_LODLEVEL:
			pipelineStates = pipelineStateSet->_debugLodLevelPipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_OCCLUSION:
			pipelineStates = pipelineStateSet->_debugOcclusionPipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_DEPTH:
			pipelineStates = pipelineStateSet->_debugDepthPipelineStateGroups;
			break;
		case DEBUG_PRIMITIVE_TYPE_TEXCOORDS:
			pipelineStates = pipelineStateSet->_debugTexcoordsPipelineStateGroups;
			break;
		}

		MultiIndirectRenderContext context = {};
		context._commandList = commandList;
		context._viewInfo = viewInfo;
		context._indirectArgumentResource = &_multiDrawIndirectArgumentResource;
		context._gpuCullingResource = &_gpuCullingResource;
		context._vramShaderSets = _vramShaderSetSystem.getShaderSet(0);
		context._indirectArgmentOffsets = _multiDrawInstancingResource.getIndirectArgumentOffsets();
		context._indirectArgmentCounts = _multiDrawInstancingResource.getIndirectArgumentCounts();
		context._meshInstanceWorldMatrixSrv = _scene.getMeshInstanceWorldMatrixSrv();
		context._pipelineStates = pipelineStateSet->_pipelineStateGroups;
		context._commandSignatures = pipelineStateSet->_commandSignatures;
		context._vertexBufferViews = vertexBufferViews;
		context._indexBufferView = &indexBufferView;
		context._numVertexBufferView = LTN_COUNTOF(vertexBufferViews);
		_meshRenderer.multiDrawRender(context);
	}

	_gpuCullingResource.readbackCullingResultBuffer(commandList);
}
#endif

#if ENABLE_CLASSIC_VERTEX
void MeshRendererSystemImpl::renderClassicVertex(CommandList* commandList, ViewInfo* viewInfo) {
	DEBUG_MARKER_CPU_GPU_SCOPED_EVENT(commandList, Color4::RED, "Classic Shader Pass");

	GpuBuffer* vertexPositionBuffer = _resourceManager.getPositionVertexBuffer();
	GpuBuffer* vertexTexcoordBuffer = _resourceManager.getTexcoordVertexBuffer();
	GpuBuffer* indexBuffer = _resourceManager.getClassicIndexBuffer();

	ResourceTransitionBarrier toVertexBarriers[4] = {};
	toVertexBarriers[0] = vertexPositionBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	toVertexBarriers[1] = vertexTexcoordBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	toVertexBarriers[2] = indexBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_INDEX_BUFFER);
	toVertexBarriers[3] = viewInfo->_depthTexture.getAndUpdateTransitionBarrier(RESOURCE_STATE_DEPTH_WRITE);
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
	GpuDescriptorHandle meshInstanceHandle = _scene.getMeshInstanceSrv();
	PipelineStateGroup** pipelineStates = materialSystem->getPipelineStateSet(MaterialSystemImpl::TYPE_CLASSIC)->_pipelineStateGroups;
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
			u32 shaderSetIndex = materialSystem->getShaderSetIndex(shaderSet);
			PipelineStateGroup* pipelineState = pipelineStates[shaderSetIndex];
			VramShaderSet* vramShaderSet = _vramShaderSetSystem.getShaderSet(shaderSetIndex);
			commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
			commandList->setPipelineState(pipelineState->getPipelineState());
			commandList->setVertexBuffers(0, LTN_COUNTOF(vertexBufferViews), vertexBufferViews);
			commandList->setIndexBuffer(&indexBufferView);
			commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->setGraphicsRootDescriptorTable(ClassicMeshRootParam::MATERIALS, vramShaderSet->getMaterialParametersSrv()._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ClassicMeshRootParam::SCENE_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ClassicMeshRootParam::MESH_INSTANCE, meshInstanceHandle);
			commandList->setGraphicsRootDescriptorTable(ClassicMeshRootParam::TEXTURES, textureDescriptors._gpuHandle);

			u32 instanceInfo[2] = {};
			instanceInfo[0] = meshInstanceIndex;
			instanceInfo[1] = meshInstance->getGpuSubMeshInstance(subMeshIndex)->_materialIndex;
			commandList->setGraphicsRoot32BitConstants(ClassicMeshRootParam::MESH_INFO, LTN_COUNTOF(instanceInfo), instanceInfo, 0);

			u32 vertexOffset = lodMeshVertexOffset;
			u32 indexOffset = subMeshInfo->_classiciIndexOffset;
			commandList->drawIndexedInstanced(subMeshInfo->_classicIndexCount, 1, indexOffset, vertexOffset, 0);
		}
	}

	ResourceTransitionBarrier toNonPixelShaderResourceBarriers[4] = {};
	toNonPixelShaderResourceBarriers[0] = vertexPositionBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toNonPixelShaderResourceBarriers[1] = vertexTexcoordBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toNonPixelShaderResourceBarriers[2] = indexBuffer->getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	toNonPixelShaderResourceBarriers[3] = viewInfo->_depthTexture.getAndUpdateTransitionBarrier(RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
void MeshRendererSystemImpl::debugDrawGpuCullingResult() {
	constexpr char FORMAT1[] = "%7.3f%% ( %-6s/ %-6s)";
	constexpr char FORMAT2[] = "%-12s";
	char t[128];
	const gpu::GpuCullingResult* cullingResult = _gpuCullingResource.getGpuCullingResult();

	if (DebugGui::BeginTabBar("CullingResultTabBar")) {
		if (DebugGui::BeginTabItem("Summary")) {
			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingMeshInstanceCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passOcclusionCullingMeshInstanceCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassSummaryCullingMeshInstancePersentage(cullingResult);
				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Mesh Instance");
			}

			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingSubMeshInstanceCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passOcclusionCullingSubMeshInstanceCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassSummaryCullingSubMeshInstancePersentage(cullingResult);
				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Sub Mesh Instance");
			}

			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingMeshletInstanceCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passOcclusionCullingMeshletInstanceCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassSummaryCullingMeshletInstancePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Meshlet Instance");
			}

			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingTriangleCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passOcclusionCullingTriangleCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassSummaryCullingTrianglePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Triangle");
			}
			DebugGui::EndTabItem();
		}
		if (DebugGui::BeginTabItem("Frustum")) {
			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingMeshInstanceCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passFrustumCullingMeshInstanceCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassFrustumCullingMeshInstancePersentage(cullingResult);
				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Mesh Instance");
			}

			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingSubMeshInstanceCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passFrustumCullingSubMeshInstanceCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassFrustumCullingSubMeshInstancePersentage(cullingResult);
				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Sub Mesh Instance");
			}

			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingMeshletInstanceCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passFrustumCullingMeshletInstanceCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassFrustumCullingMeshletInstancePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Meshlet Instance");
			}

			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingTriangleCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passFrustumCullingTriangleCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassFrustumCullingTrianglePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Triangle");
			}

			DebugGui::EndTabItem();
		}
		if (DebugGui::BeginTabItem("Occlusion")) {
			{
				ThreeDigiets testOcclusionCullingCount(cullingResult->_testOcclusionCullingMeshInstanceCount);
				ThreeDigiets passOcclusionCullingCount(cullingResult->_passOcclusionCullingMeshInstanceCount);
				f32 passOcclusionCullingPersentage = CullingResult::getPassOcclusionCullingMeshInstancePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passOcclusionCullingPersentage, passOcclusionCullingCount.get(), testOcclusionCullingCount.get());
				DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Mesh Instance");
			}

			{
				ThreeDigiets testOcclusionCullingCount(cullingResult->_testOcclusionCullingSubMeshInstanceCount);
				ThreeDigiets passOcclusionCullingCount(cullingResult->_passOcclusionCullingSubMeshInstanceCount);
				f32 passOcclusionCullingPersentage = CullingResult::getPassOcclusionCullingSubMeshInstancePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passOcclusionCullingPersentage, passOcclusionCullingCount.get(), testOcclusionCullingCount.get());
				DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Sub Mesh Instance");
			}

			{
				ThreeDigiets testOcclusionCullingCount(cullingResult->_testOcclusionCullingMeshletInstanceCount);
				ThreeDigiets passOcclusionCullingCount(cullingResult->_passOcclusionCullingMeshletInstanceCount);
				f32 passOcclusionCullingPersentage = CullingResult::getPassOcclusionCullingMeshletInstancePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passOcclusionCullingPersentage, passOcclusionCullingCount.get(), testOcclusionCullingCount.get());
				DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Meshlet Instance");
			}

			{
				ThreeDigiets testOcclusionCullingCount(cullingResult->_testOcclusionCullingTriangleCount);
				ThreeDigiets passOcclusionCullingCount(cullingResult->_passOcclusionCullingTriangleCount);
				f32 passOcclusionCullingPersentage = CullingResult::getPassOcclusionCullingTrianglePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passOcclusionCullingPersentage, passOcclusionCullingCount.get(), testOcclusionCullingCount.get());
				DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Triangle");
			}

			DebugGui::EndTabItem();
		}
		DebugGui::EndTabBar();
	}
}
void MeshRendererSystemImpl::debugDrawAmplificationCullingResult() {
	constexpr char FORMAT1[] = "%7.3f%% ( %-6s/ %-6s)";
	constexpr char FORMAT2[] = "%-12s";
	char t[128];
	const gpu::AmplificationCullingResult* cullingResult = _gpuCullingResource.getAmplificationCullingResult();

	if (DebugGui::BeginTabBar("CullingResultTabBar")) {
		if (DebugGui::BeginTabItem("Summary")) {
			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingMeshletInstanceCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passOcclusionCullingMeshletInstanceCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassSummaryCullingMeshletInstancePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Meshlet Instance");
			}

			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingTriangleCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passOcclusionCullingTriangleCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassSummaryCullingTrianglePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Triangle");
			}

			DebugGui::EndTabItem();
		}
		if (DebugGui::BeginTabItem("Frustum")) {
			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingMeshletInstanceCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passFrustumCullingMeshletInstanceCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassFrustumCullingMeshletInstancePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Meshlet Instance");
			}

			{
				ThreeDigiets testFrustumCullingCount(cullingResult->_testFrustumCullingTriangleCount);
				ThreeDigiets passFrustumCullingCount(cullingResult->_passFrustumCullingTriangleCount);
				f32 passFrustumCullingPersentage = CullingResult::getPassFrustumCullingTrianglePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passFrustumCullingPersentage, passFrustumCullingCount.get(), testFrustumCullingCount.get());
				DebugGui::ProgressBar(passFrustumCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Triangle");
			}

			DebugGui::EndTabItem();
		}
		if (DebugGui::BeginTabItem("Occlusion")) {
			{
				ThreeDigiets testOcclusionCullingCount(cullingResult->_testOcclusionCullingMeshletInstanceCount);
				ThreeDigiets passOcclusionCullingCount(cullingResult->_passOcclusionCullingMeshletInstanceCount);
				f32 passOcclusionCullingPersentage = CullingResult::getPassOcclusionCullingMeshletInstancePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passOcclusionCullingPersentage, passOcclusionCullingCount.get(), testOcclusionCullingCount.get());
				DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Meshlet Instance");
			}

			{
				ThreeDigiets testOcclusionCullingCount(cullingResult->_testOcclusionCullingTriangleCount);
				ThreeDigiets passOcclusionCullingCount(cullingResult->_passOcclusionCullingTriangleCount);
				f32 passOcclusionCullingPersentage = CullingResult::getPassOcclusionCullingTrianglePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passOcclusionCullingPersentage, passOcclusionCullingCount.get(), testOcclusionCullingCount.get());
				DebugGui::ProgressBar(passOcclusionCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Triangle");
			}

			DebugGui::EndTabItem();
		}
		if (DebugGui::BeginTabItem("Backface")) {
			{
				ThreeDigiets testBackfaceCullingCount(cullingResult->_testBackfaceCullingMeshletInstanceCount);
				ThreeDigiets passBackfaceCullingCount(cullingResult->_passBackfaceCullingMeshletInstanceCount);
				f32 passBackfaceCullingPersentage = CullingResult::getPassBackfaceCullingMeshletInstancePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passBackfaceCullingPersentage, passBackfaceCullingCount.get(), testBackfaceCullingCount.get());
				DebugGui::ProgressBar(passBackfaceCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Meshlet Instance");
			}

			{
				ThreeDigiets testBackfaceCullingCount(cullingResult->_testBackfaceCullingTriangleCount);
				ThreeDigiets passBackfaceCullingCount(cullingResult->_passBackfaceCullingTriangleCount);
				f32 passBackfaceCullingPersentage = CullingResult::getPassBackfaceCullingTrianglePersentage(cullingResult);

				sprintf_s(t, LTN_COUNTOF(t), FORMAT1, passBackfaceCullingPersentage, passBackfaceCullingCount.get(), testBackfaceCullingCount.get());
				DebugGui::ProgressBar(passBackfaceCullingPersentage / 100.0f, Vector2(0, 0), t);
				DebugGui::SameLine(0.0f, DebugGui::GetItemInnerSpacing()._x);
				DebugGui::Text(FORMAT2, "Triangle");
			}

			DebugGui::EndTabItem();
		}
		DebugGui::EndTabBar();
	}
}
#endif

void MeshRendererSystemImpl::initialize() {
	_scene.initialize();
	_resourceManager.initialize();
	_meshRenderer.initialize();
	_vramShaderSetSystem.initialize();
	_primitiveInstancingResource.initialize();

	Device* device = GraphicsSystemImpl::Get()->getDevice();
	GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();

	{
		IndirectArgumentResource::InitializeDesc desc;
		desc._indirectArgumentCount = 1024 * 256;
		desc._indirectArgumentCounterCount = gpu::SHADER_SET_COUNT_MAX;
		desc._strideInByte = sizeof(gpu::DispatchMeshIndirectArgumentMS);
		_indirectArgumentResource.initialize(desc);
	}

	{
		IndirectArgumentResource::InitializeDesc desc;
		desc._indirectArgumentCount = MeshResourceManager::SUB_MESH_COUNT_MAX * gpu::SHADER_SET_COUNT_MAX;
		desc._indirectArgumentCounterCount = gpu::SHADER_SET_COUNT_MAX;
		desc._strideInByte = sizeof(gpu::DispatchMeshIndirectArgumentMS);
		_primIndirectArgumentResource.initialize(desc);
	}

#if ENABLE_MULTI_INDIRECT_DRAW
	_multiDrawInstancingResource.initialize();
	{
		IndirectArgumentResource::InitializeDesc desc;
		desc._indirectArgumentCount = Scene::SUB_MESH_INSTANCE_COUNT_MAX;
		desc._indirectArgumentCounterCount = gpu::SHADER_SET_COUNT_MAX;
		desc._strideInByte = sizeof(gpu::StarndardMeshIndirectArguments);
		_multiDrawIndirectArgumentResource.initialize(desc);
	}
#endif
	_gpuCullingResource.initialize();
	_buildIndirectArgumentResource.initialize();

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
	_scene.terminateDefaultResources();
	_resourceManager.terminateDefaultResources();

	processDeletion();

	_scene.terminate();
	_vramShaderSetSystem.terminate();
	_resourceManager.terminate();
	_meshRenderer.terminate();
	_indirectArgumentResource.terminate();
	_primIndirectArgumentResource.terminate();
	_gpuCullingResource.terminate();
	_buildIndirectArgumentResource.terminate();
	_primitiveInstancingResource.terminate();

#if ENABLE_MULTI_INDIRECT_DRAW
	_multiDrawIndirectArgumentResource.terminate();
	_multiDrawInstancingResource.terminate();
#endif

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
	_gpuCullingResource.update(ViewSystemImpl::Get()->getView());
	_vramShaderSetSystem.update();

	bool isUpdatedGeometryType = false;

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
			s32 _packedMeshletCount = 0;
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
		DebugGui::SliderInt("Packed Meshlet", &debug._packedMeshletCount, 0, 350);
		DebugWindow::End();

		if (debug._drawMeshInstanceBounds) {
			_scene.debugDrawMeshInstanceBounds();
		}

		_packedMeshletCount = static_cast<u32>(debug._packedMeshletCount);
		_debugDrawMeshletBounds = debug._drawMeshletBounds;
		_visible = debug._visible;
		_debugPrimitiveType = debug._primitiveType;

		if (_geometoryType != debug._geometoryMode) {
			isUpdatedGeometryType = true;
		}
		_geometoryType = debug._geometoryMode;
		setDebugCullingFlag(CULLING_DEBUG_TYPE_FIXED_VIEW, debug._fixedCullingView);
		setDebugCullingFlag(CULLING_DEBUG_TYPE_PASS_MESH_CULLING, debug._passMeshInstanceCulling);
		setDebugCullingFlag(CULLING_DEBUG_TYPE_PASS_MESHLET_CULLING, debug._passMeshletInstanceCulling);

		if (debug._fixedCullingView) {
			DebugRendererSystem::Get()->drawFrustum(_debugFixedViewMatrix, _debugFixedProjectionMatrix, Color4::YELLOW);
		}
	}

	{
		DebugWindow::StartWindow("Culling Results");

		if (DebugGui::BeginTabBar("CullingResultTabBar")) {
			if (DebugGui::BeginTabItem("Gpu Culling")) {
				debugDrawGpuCullingResult();
				DebugGui::EndTabItem();
			}

			if (DebugGui::BeginTabItem("Amplification")) {
				debugDrawAmplificationCullingResult();
				DebugGui::EndTabItem();
			}
			DebugGui::EndTabItem();
		}

		DebugWindow::End();
	}

	{
		DebugWindow::StartWindow("Scene Instances");
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

	{
		BuildIndirectArgumentResource::UpdateDesc desc;
		desc._packedMeshletCount = _packedMeshletCount;
		_buildIndirectArgumentResource.update(desc);
	}

	if (_scene.isUpdatedInstancingOffset() || isUpdatedGeometryType) {
		switch (_geometoryType) {
#if ENABLE_MESH_SHADER
		case GEOMETORY_MODE_MESH_SHADER:
		{
			InstancingResource::UpdateDesc desc;
			desc._meshInstances = _scene.getMeshInstance(0);
			desc._countMax = _scene.getMeshInstanceArrayCountMax();
			_primitiveInstancingResource.update(desc);
			break;
		}
#endif
#if ENABLE_MULTI_INDIRECT_DRAW
		case GEOMETORY_MODE_MULTI_INDIRECT:
		{
			MultiDrawInstancingResource::UpdateDesc desc;
			desc._subMeshInstances = _scene.getSubMeshInstances();
			desc._countMax = _scene.getSubMeshInstanceArrayCountMax();
			_multiDrawInstancingResource.update(desc);
			break;
		}
#endif
		}
	}
}

void MeshRendererSystemImpl::render(CommandList* commandList, ViewInfo* viewInfo) {
	if (!_visible) {
		return;
	}

	// レンダーターゲットクリア
	{
		DEBUG_MARKER_GPU_SCOPED_EVENT(commandList, Color4::GREEN, "Setup Draw");

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
	_vramShaderSetSystem.processDeletion();
}

Mesh* MeshRendererSystemImpl::allocateMesh(const MeshDesc& desc) {
	return _resourceManager.allocateMesh(desc);
}

Mesh* MeshRendererSystemImpl::createMesh(const MeshDesc& desc) {
	return _resourceManager.createMesh(desc);
}

void MeshRendererSystemImpl::createMeshInstance(MeshInstance** outMeshInstances, const MeshInstanceDesc& desc) {
	return _scene.allocateMeshInstance(outMeshInstances, desc._meshes, desc._instanceCount);
}

Mesh* MeshRendererSystemImpl::findMesh(u64 filePathHash) {
	return _resourceManager.findMesh(filePathHash);
}
