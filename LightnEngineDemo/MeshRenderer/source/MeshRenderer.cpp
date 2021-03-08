#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <MeshRenderer/GpuStruct.h>
#include <MaterialSystem/impl/PipelineStateSystem.h>
#include <MaterialSystem/impl/MaterialSystemImpl.h>
#include <TextureSystem/impl/TextureSystemImpl.h>

#include <GfxCore/impl/ViewSystemImpl.h>
#include <MeshRenderer/impl/MeshRenderer.h>
#include <MeshRenderer/impl/SceneImpl.h>
#include <MeshRenderer/impl/VramShaderSetSystem.h>
#include <GfxCore/impl/QueryHeapSystem.h>

void MeshRenderer::initialize()
{
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
		meshDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);

		DescriptorRange meshInstanceDescriptorRange = {};
		meshInstanceDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 3, 4);

		DescriptorRange indirectArgumentOffsetRange = {};
		indirectArgumentOffsetRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);

		DescriptorRange lodLevelSrvRange = {};
		lodLevelSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);

		DescriptorRange subMeshInfoSrvRange = {};
		subMeshInfoSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 9);

		DescriptorRange meshletInstanceInfoOffsetSrvRange = {};
		meshletInstanceInfoOffsetSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 10);

		DescriptorRange materialInstanceIndexSrvRange = {};
		materialInstanceIndexSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 11);

		DescriptorRange primitiveInstancingInfoOffsetSrvRange = {};
		primitiveInstancingInfoOffsetSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 12);

		DescriptorRange hizSrvRange = {};
		hizSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, gpu::HIERACHICAL_DEPTH_COUNT, 13);

		DescriptorRange indirectArgumentUavRange = {};
		indirectArgumentUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);

		DescriptorRange meshletInstanceInfoUavRange = {};
		meshletInstanceInfoUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

		DescriptorRange meshletInstanceInfoCountUavRange = {};
		meshletInstanceInfoCountUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);

		DescriptorRange primitiveInstancingInfoUavRange = {};
		primitiveInstancingInfoUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);

		DescriptorRange primitiveInstancingInfoCountUavRange = {};
		primitiveInstancingInfoCountUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);

		DescriptorRange cullingResultUavRange = {};
		cullingResultUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 1, 6);

		// culling root signature
		{
			_gpuCullingRootSignature = allocator->allocateRootSignature();

			RootParameter rootParameters[ROOT_PARAM_GPU_COUNT] = {};
			rootParameters[ROOT_PARAM_GPU_CULLING_SCENE_INFO].initializeDescriptorTable(1, &sceneCullingConstantRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_CULLING_VIEW_INFO].initializeDescriptorTable(1, &cullingViewInfoConstantRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_VIEW_INFO].initializeDescriptorTable(1, &viewInfoConstantRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_INDIRECT_ARGUMENT_OFFSETS].initializeDescriptorTable(1, &indirectArgumentOffsetRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_INDIRECT_ARGUMENTS].initializeDescriptorTable(1, &indirectArgumentUavRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_CULLING_RESULT].initializeDescriptorTable(1, &cullingResultUavRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_LOD_LEVEL].initializeDescriptorTable(1, &lodLevelSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_SUB_MESH_DRAW_INFO].initializeDescriptorTable(1, &subMeshInfoSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MESHLET_INSTANCE_OFFSET].initializeDescriptorTable(1, &meshletInstanceInfoOffsetSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MESHLET_INSTANCE_INFO].initializeDescriptorTable(1, &meshletInstanceInfoUavRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MESHLET_INSTANCE_COUNT].initializeDescriptorTable(1, &meshletInstanceInfoCountUavRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_MATERIAL_INSTANCE_INDEX].initializeDescriptorTable(1, &materialInstanceIndexSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_PRIMITIVE_INSTANCING_OFFSETS].initializeDescriptorTable(1, &primitiveInstancingInfoOffsetSrvRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_PRIMITIVE_INSTANCING_INFOS].initializeDescriptorTable(1, &primitiveInstancingInfoUavRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_PRIMITIVE_INSTANCING_COUNTS].initializeDescriptorTable(1, &primitiveInstancingInfoCountUavRange, SHADER_VISIBILITY_ALL);
			rootParameters[ROOT_PARAM_GPU_HIZ].initializeDescriptorTable(1, &hizSrvRange, SHADER_VISIBILITY_ALL);

			RootSignatureDesc rootSignatureDesc = {};
			rootSignatureDesc._device = device;
			rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
			rootSignatureDesc._parameters = rootParameters;
			_gpuCullingRootSignature->iniaitlize(rootSignatureDesc);
			_gpuCullingRootSignature->setDebugName("Gpu Culling");
		}

		// pipeline state
		{
			_gpuOcclusionCullingPipelineState = allocator->allocatePipelineState();
			_gpuCullingPipelineState = allocator->allocatePipelineState();

			ComputePipelineStateDesc pipelineStateDesc = {};
			pipelineStateDesc._device = device;
			pipelineStateDesc._rootSignature = _gpuCullingRootSignature;

			// フラスタム　＋　オクルージョンカリング
			{
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/mesh_shader_gpu_driven/mesh_culling_frustum_occlusion.cso");

				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_gpuOcclusionCullingPipelineState->iniaitlize(pipelineStateDesc);
				_gpuOcclusionCullingPipelineState->setDebugName("mesh_shader_gpu_driven/mesh_culling_frustum_occlusion.cso");

				computeShader->terminate();
			}

			// フラスタムカリングのみ
			{
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/mesh_shader_gpu_driven/mesh_culling_frustum.cso");

				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_gpuCullingPipelineState->iniaitlize(pipelineStateDesc);
				_gpuCullingPipelineState->setDebugName("mesh_shader_gpu_driven/mesh_culling_frustum.cso");

				computeShader->terminate();
			}

			// カリングオフ
			{
				_gpuCullingPassPipelineState = allocator->allocatePipelineState();
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/mesh_shader_gpu_driven/mesh_culling_pass.cso");
				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_gpuCullingPassPipelineState->iniaitlize(pipelineStateDesc);
				_gpuCullingPassPipelineState->setDebugName("mesh_shader_gpu_driven/mesh_culling_pass.cso");

				computeShader->terminate();
			}

#if ENABLE_MULTI_INDIRECT_DRAW
			_multiDrawOcclusionCullingPipelineState = allocator->allocatePipelineState();
			{
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/standard_gpu_driven/mesh_culling_frustum_occlusion.cso");

				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_multiDrawOcclusionCullingPipelineState->iniaitlize(pipelineStateDesc);
				_multiDrawOcclusionCullingPipelineState->setDebugName("standard_gpu_driven/mesh_culling_frustum_occlusion.cso");

				computeShader->terminate();
			}

			_multiDrawCullingPipelineState = allocator->allocatePipelineState();
			{
				ShaderBlob* computeShader = allocator->allocateShaderBlob();
				computeShader->initialize("L:/LightnEngine/resource/common/shader/standard_gpu_driven/mesh_culling_frustum.cso");
				pipelineStateDesc._cs = computeShader->getShaderByteCode();
				_multiDrawCullingPipelineState->iniaitlize(pipelineStateDesc);
				_multiDrawCullingPipelineState->setDebugName("standard_gpu_driven/mesh_culling_frustum.cso");

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
		_buildIndirectArgumentRootSignature->setDebugName("Build Indirect Argument");

		ShaderBlob* computeShader = allocator->allocateShaderBlob();
		computeShader->initialize("L:/LightnEngine/resource/common/shader/build_indirect_argument.cso");

		ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._cs = computeShader->getShaderByteCode();
		pipelineStateDesc._rootSignature = _buildIndirectArgumentRootSignature;
		_buildIndirectArgumentPipelineState->iniaitlize(pipelineStateDesc);
		_buildIndirectArgumentPipelineState->setDebugName("build_indirect_argument.cso");

		computeShader->terminate();
	}

	// build indirect argument primitive instancing
	{
		_buildIndirectArgumentPrimitiveInstancingPipelineState = allocator->allocatePipelineState();
		_buildIndirectArgumentPrimitiveInstancingRootSignature = allocator->allocateRootSignature();

		DescriptorRange batchedSubMeshInfoOffsetSrvRange = {};
		batchedSubMeshInfoOffsetSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		DescriptorRange batchedSubMeshInfoCountSrvRange = {};
		batchedSubMeshInfoCountSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		DescriptorRange indirectArgumentUavRange = {};
		indirectArgumentUavRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 2, 0);

		RootParameter rootParameters[BuildIndirectArgumentPrimitiveInstancingRootParameters::ROOT_PARAM_COUNT] = {};
		rootParameters[BuildIndirectArgumentPrimitiveInstancingRootParameters::BATCHED_SUBMESH_OFFSET].initializeDescriptorTable(1, &batchedSubMeshInfoOffsetSrvRange, SHADER_VISIBILITY_ALL);
		rootParameters[BuildIndirectArgumentPrimitiveInstancingRootParameters::BATCHED_SUBMESH_COUNT].initializeDescriptorTable(1, &batchedSubMeshInfoCountSrvRange, SHADER_VISIBILITY_ALL);
		rootParameters[BuildIndirectArgumentPrimitiveInstancingRootParameters::INDIRECT_ARGUMENT].initializeDescriptorTable(1, &indirectArgumentUavRange, SHADER_VISIBILITY_ALL);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_buildIndirectArgumentPrimitiveInstancingRootSignature->iniaitlize(rootSignatureDesc);
		_buildIndirectArgumentPrimitiveInstancingRootSignature->setDebugName("Build Indirect Argument Primitive Instancing");

		ShaderBlob* computeShader = allocator->allocateShaderBlob();
		computeShader->initialize("L:/LightnEngine/resource/common/shader/build_indirect_argument_primitive_instancing.cso");

		ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._cs = computeShader->getShaderByteCode();
		pipelineStateDesc._rootSignature = _buildIndirectArgumentRootSignature;
		_buildIndirectArgumentPrimitiveInstancingPipelineState->iniaitlize(pipelineStateDesc);
		_buildIndirectArgumentPrimitiveInstancingPipelineState->setDebugName("build_indirect_argument_primitive_instancing.cso");

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
		_computeLodRootSignature->setDebugName("Compute Lod");

		ShaderBlob* computeShader = allocator->allocateShaderBlob();
		computeShader->initialize("L:/LightnEngine/resource/common/shader/compute_lod.cso");

		ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._cs = computeShader->getShaderByteCode();
		pipelineStateDesc._rootSignature = _computeLodRootSignature;
		_computeLodPipelineState->iniaitlize(pipelineStateDesc);
		_computeLodPipelineState->setDebugName("compute_lod.cso");

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
		_debugMeshletBoundsRootSignature->setDebugName("Debug Meshlet Bounds");

		ShaderBlob* computeShader = allocator->allocateShaderBlob();
		computeShader->initialize("L:/LightnEngine/resource/common/shader/debug/debug_draw_meshlet_bounds.cso");

		ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._cs = computeShader->getShaderByteCode();
		pipelineStateDesc._rootSignature = _debugMeshletBoundsRootSignature;
		_debugMeshletBoundsPipelineState->iniaitlize(pipelineStateDesc);
		_debugMeshletBoundsPipelineState->setDebugName("debug/debug_draw_meshlet_bounds.cso");

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
		outpuDepthRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, gpu::HIERACHICAL_DEPTH_COUNT / 2, 0);

		RootParameter rootParameters[ROOT_PARAM_HIZ_COUNT] = {};
		rootParameters[ROOT_PARAM_HIZ_INFO].initializeDescriptorTable(1, &buildHizInfoConstantRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_HIZ_INPUT_DEPTH].initializeDescriptorTable(1, &inputDepthRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_PARAM_HIZ_OUTPUT_DEPTH].initializeDescriptorTable(1, &outpuDepthRange, SHADER_VISIBILITY_ALL);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		_buildHizRootSignature->iniaitlize(rootSignatureDesc);
		_buildHizRootSignature->setDebugName("Build Hiz");

		ShaderBlob* computeShader = allocator->allocateShaderBlob();
		computeShader->initialize("L:/LightnEngine/resource/common/shader/build_hierarchical_depth.cso");

		ComputePipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._cs = computeShader->getShaderByteCode();
		pipelineStateDesc._rootSignature = _buildHizRootSignature;
		_buildHizPipelineState->iniaitlize(pipelineStateDesc);
		_buildHizPipelineState->setDebugName("build_hierarchical_depth.cso");

		computeShader->terminate();
	}
}

void MeshRenderer::terminate()
{
	_gpuCullingPassPipelineState->terminate();
	_gpuOcclusionCullingPipelineState->terminate();
	_gpuCullingRootSignature->terminate();
	_gpuCullingPipelineState->terminate();
	_computeLodPipelineState->terminate();
	_computeLodRootSignature->terminate();
	_buildHizPipelineState->terminate();
	_buildHizRootSignature->terminate();
	_debugMeshletBoundsPipelineState->terminate();
	_debugMeshletBoundsRootSignature->terminate();
	_buildIndirectArgumentPipelineState->terminate();
	_buildIndirectArgumentRootSignature->terminate();
	_buildIndirectArgumentPrimitiveInstancingPipelineState->terminate();
	_buildIndirectArgumentPrimitiveInstancingRootSignature->terminate();

#if ENABLE_MULTI_INDIRECT_DRAW
	_multiDrawCullingPipelineState->terminate();
	_multiDrawOcclusionCullingPipelineState->terminate();
#endif
}

void MeshRenderer::render(RenderContext& context) {
	CommandList* commandList = context._commandList;
	ViewInfo* viewInfo = context._viewInfo;
	GraphicsView* graphicsView = context._graphicsView;
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	DescriptorHandle textureDescriptors = TextureSystemImpl::Get()->getDescriptors();
	u32 shaderSetCount = materialSystem->getShaderSetCount();

	for (u32 pipelineStateIndex = 0; pipelineStateIndex < shaderSetCount; ++pipelineStateIndex) {
		PipelineStateGroup* pipelineState = context._pipelineStates[pipelineStateIndex];
		if (pipelineState == nullptr) {
			continue;
		}

		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Shader %d", pipelineStateIndex);

		// インスタンシング描画
		u32 commandInstancingCountMax = context._indirectArgmentInstancingCounts[pipelineStateIndex];
		if (commandInstancingCountMax > 0) {
			VramShaderSet* vramShaderSet = &context._vramShaderSets[pipelineStateIndex];
			u32 countBufferOffset = pipelineStateIndex * sizeof(u32);
			u32 indirectArgumentOffset = pipelineStateIndex * Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX;
			u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::DispatchMeshIndirectArgument);

			commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
			commandList->setPipelineState(pipelineState->getPipelineState());
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VIEW_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT, context._debugFixedViewCbv);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MATERIALS, vramShaderSet->getMaterialParametersSrv()._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH, context._meshHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESHLET_INFO, graphicsView->getMeshletInstanceInfoSrv()._gpuHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_MESH_INSTANCE, context._meshInstanceHandle);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_VERTEX_RESOURCES, context._vertexResourceDescriptors);
			commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_TEXTURES, textureDescriptors._gpuHandle);
			
			if (context._collectResult) {
				graphicsView->setDrawResultDescriptorTable(commandList);
			}
			graphicsView->setDrawCurrentLodDescriptorTable(commandList);
			graphicsView->render(commandList, pipelineState->getCommandSignature(), Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX, indirectArgumentOffsetSizeInByte, countBufferOffset);
		}

		// 単品描画
		u32 commandCountMax = context._indirectArgmentCounts[pipelineStateIndex];
		if (commandCountMax > 0) {
			u32 indirectArgumentOffset = context._indirectArgmentOffsets[pipelineStateIndex] + (gpu::SHADER_SET_COUNT_MAX * Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX);
			u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::DispatchMeshIndirectArgument);
			LTN_ASSERT(indirectArgumentOffset + commandCountMax <= GraphicsView::INDIRECT_ARGUMENT_COUNT_MAX);

			u32 countBufferOffset = (pipelineStateIndex + gpu::SHADER_SET_COUNT_MAX) * sizeof(u32);
			commandList->setPipelineState(pipelineState->getPipelineState());
			graphicsView->render(commandList, pipelineState->getCommandSignature(), commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);
		}
	}
}

void MeshRenderer::computeLod(ComputeLodContext& context) {
	CommandList* commandList = context._commandList;
	ViewInfo* viewInfo = context._viewInfo;
	GraphicsView* graphicsView = context._graphicsView;
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::BLUE, "Compute Lod");

	commandList->setComputeRootSignature(_computeLodRootSignature);
	commandList->setPipelineState(_computeLodPipelineState);
	graphicsView->setComputeLodResource(commandList);
	graphicsView->resourceBarriersComputeLodToUAV(commandList);

	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_SCENE_INFO, context._sceneConstantCbv);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_VIEW_INFO, viewInfo->_cbvHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_MESH, context._meshHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_MESH_INSTANCE, context._meshInstanceHandle);

	u32 dispatchCount = RoundUp(context._meshInstanceCountMax, 128u);
	commandList->dispatch(dispatchCount, 1, 1);
	graphicsView->resetResourceComputeLodBarriers(commandList);

	queryHeapSystem->setCurrentMarkerName("Compute Lod");
	queryHeapSystem->setMarker(commandList);
}

void MeshRenderer::depthPrePassCulling(GpuCullingContext& context) {
	gpuCulling(context, _gpuCullingPipelineState);
}

void MeshRenderer::buildIndirectArgument(BuildIndirectArgumentContext& context) {
	CommandList* commandList = context._commandList;
	GraphicsView* graphicsView = context._graphicsView;
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_GREEN, "Build Indirect Argument");

	graphicsView->resourceBarriersBuildIndirectArgument(commandList);
	commandList->setComputeRootSignature(_buildIndirectArgumentRootSignature);
	commandList->setPipelineState(_buildIndirectArgumentPipelineState);

	commandList->setComputeRootDescriptorTable(BuildIndirectArgumentRootParameters::BATCHED_SUBMESH_OFFSET, context._meshletInstanceOffsetSrv);
	commandList->setComputeRootDescriptorTable(BuildIndirectArgumentRootParameters::BATCHED_SUBMESH_COUNT, context._meshletInstanceCountSrv);
	commandList->setComputeRootDescriptorTable(BuildIndirectArgumentRootParameters::INDIRECT_ARGUMENT, context._indirectArgumentUav);

	u32 dispatchCount = RoundUp(Scene::MESHLET_INSTANCE_MESHLET_COUNT_MAX * gpu::SHADER_SET_COUNT_MAX, 128u);
	commandList->dispatch(dispatchCount, 1, 1);
	graphicsView->resourceBarriersResetBuildIndirectArgument(commandList);

	queryHeapSystem->setCurrentMarkerName("Build Indirect Argument");
	queryHeapSystem->setMarker(commandList);
}

void MeshRenderer::buildIndirectArgumentPrimitiveInstancing(BuildIndirectArgumentPrimitiveInstancingContext& context) {
	CommandList* commandList = context._commandList;
	GraphicsView* graphicsView = context._graphicsView;
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_GREEN, "Build Indirect Argument Primitive");

	graphicsView->resourceBarriersBuildIndirectArgument(commandList);
	commandList->setComputeRootSignature(_buildIndirectArgumentPrimitiveInstancingRootSignature);
	commandList->setPipelineState(_buildIndirectArgumentPrimitiveInstancingPipelineState);

	commandList->setComputeRootDescriptorTable(BuildIndirectArgumentPrimitiveInstancingRootParameters::BATCHED_SUBMESH_OFFSET, context._meshletInstanceOffsetSrv);
	commandList->setComputeRootDescriptorTable(BuildIndirectArgumentPrimitiveInstancingRootParameters::BATCHED_SUBMESH_COUNT, context._meshletInstanceCountSrv);
	commandList->setComputeRootDescriptorTable(BuildIndirectArgumentPrimitiveInstancingRootParameters::INDIRECT_ARGUMENT, context._indirectArgumentUav);

	u32 dispatchCount = RoundUp(PrimitiveInstancingResource::PRIMITIVE_INSTANCING_INFO_COUNT_MAX, 128u);
	commandList->dispatch(dispatchCount, 1, 1);
	graphicsView->resourceBarriersResetBuildIndirectArgument(commandList);

	queryHeapSystem->setCurrentMarkerName("Build Indirect Argument primitive");
	queryHeapSystem->setMarker(commandList);
}

void MeshRenderer::mainCulling(GpuCullingContext& context) {
	gpuCulling(context, _gpuOcclusionCullingPipelineState);
}

void MeshRenderer::buildHiz(BuildHizContext& context) {
	CommandList* commandList = context._commandList;
	ViewInfo* viewInfo = context._viewInfo;
	GraphicsView* graphicsView = context._graphicsView;
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_BLUE, "Build Hiz");

	viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->setComputeRootSignature(_buildHizRootSignature);
	commandList->setPipelineState(_buildHizPipelineState);

	// pass 0
	{
		commandList->setComputeRootDescriptorTable(ROOT_PARAM_HIZ_INPUT_DEPTH, viewInfo->_depthSrv._gpuHandle);
		graphicsView->resourceBarriersHizTextureToUav(commandList, 0);
		graphicsView->setHizResourcesPass0(commandList);

		u32 dispatchWidthCount = RoundUp(static_cast<u32>(viewInfo->_viewPort._width), 16u);
		u32 dispatchHeightCount = RoundUp(static_cast<u32>(viewInfo->_viewPort._height), 16u);
		commandList->dispatch(dispatchWidthCount, dispatchHeightCount, 1);

		graphicsView->resourceBarriersHizUavtoSrv(commandList, 0);
	}

	// pass 1
	{
		graphicsView->resourceBarriersHizTextureToUav(commandList, 4);
		graphicsView->setHizResourcesPass1(commandList);

		ResourceDesc hizLevel3Desc = graphicsView->getHizTextureResourceDesc(3);
		u32 dispatchWidthCount = RoundUp(static_cast<u32>(hizLevel3Desc._width), 16u);
		u32 dispatchHeightCount = RoundUp(static_cast<u32>(hizLevel3Desc._height), 16u);
		commandList->dispatch(dispatchWidthCount, dispatchHeightCount, 1);

		graphicsView->resourceBarriersHizUavtoSrv(commandList, 4);
	}

	viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	graphicsView->resourceBarriersHizSrvToTexture(commandList);

	queryHeapSystem->setCurrentMarkerName("Build Hiz");
	queryHeapSystem->setMarker(commandList);
}

void MeshRenderer::multiDrawRender(MultiIndirectRenderContext& context) {
	CommandList* commandList = context._commandList;
	ViewInfo* viewInfo = context._viewInfo;
	GraphicsView* graphicsView = context._graphicsView;
	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	DescriptorHandle textureDescriptors = TextureSystemImpl::Get()->getDescriptors();
	u32 shaderSetCount = materialSystem->getShaderSetCount();

	commandList->setViewports(1, &viewInfo->_viewPort);
	commandList->setScissorRects(1, &viewInfo->_scissorRect);
	viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_DEPTH_WRITE);
	graphicsView->resourceBarriersHizTextureToSrv(commandList);
	for (u32 pipelineStateIndex = 0; pipelineStateIndex < shaderSetCount; ++pipelineStateIndex) {
		PipelineStateGroup* pipelineState = context._pipelineStates[pipelineStateIndex];
		if (pipelineState == nullptr) {
			continue;
		}

		VramShaderSet* vramShaderSet = &context._vramShaderSets[pipelineStateIndex];
		u32 commandCountMax = context._indirectArgmentCounts[pipelineStateIndex];
		if (commandCountMax == 0) {
			continue;
		}

		DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::DEEP_RED, "Shader %d", pipelineStateIndex);

		u32 indirectArgumentOffset = context._indirectArgmentOffsets[pipelineStateIndex];
		u32 indirectArgumentOffsetSizeInByte = indirectArgumentOffset * sizeof(gpu::StarndardMeshIndirectArguments);
		LTN_ASSERT(indirectArgumentOffset + commandCountMax <= GraphicsView::INDIRECT_ARGUMENT_COUNT_MAX);

		u32 countBufferOffset = pipelineStateIndex * sizeof(u32);

		commandList->setGraphicsRootSignature(pipelineState->getRootSignature());
		commandList->setPipelineState(pipelineState->getPipelineState());
		commandList->setVertexBuffers(0, context._numVertexBufferView, context._vertexBufferViews);
		commandList->setIndexBuffer(context._indexBufferView);
		commandList->setPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MATERIALS, vramShaderSet->getMaterialParametersSrv()._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_SCENE_CONSTANT, viewInfo->_cbvHandle._gpuHandle);
		commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_MESH_INSTANCE, context._meshInstanceHandle);
		commandList->setGraphicsRootDescriptorTable(ROOT_CLASSIC_MESH_TEXTURES, textureDescriptors._gpuHandle);
		graphicsView->render(commandList, pipelineState->getCommandSignature(), commandCountMax, indirectArgumentOffsetSizeInByte, countBufferOffset);
	}

	graphicsView->resourceBarriersHizSrvToTexture(commandList);
	viewInfo->_depthTexture.transitionResource(commandList, RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void MeshRenderer::multiDrawDepthPrePassCulling(GpuCullingContext & context) {
	gpuCulling(context, _multiDrawCullingPipelineState);
}

void MeshRenderer::multiDrawMainCulling(GpuCullingContext & context) {
	gpuCulling(context, _multiDrawOcclusionCullingPipelineState);
}

void MeshRenderer::gpuCulling(GpuCullingContext & context, PipelineState * pipelineState) {
	CommandList* commandList = context._commandList;
	ViewInfo* viewInfo = context._viewInfo;
	GraphicsView* graphicsView = context._graphicsView;
	u32 meshInstanceCountMax = context._meshInstanceCountMax;

	QueryHeapSystem* queryHeapSystem = QueryHeapSystem::Get();
	DEBUG_MARKER_SCOPED_EVENT(commandList, Color4::GREEN, context._scopeName);

	// primitive instancing resource -> resourceBarriersGpuCullingToUAV(commandList)
	graphicsView->resourceBarriersGpuCullingToUAV(commandList);
	// primitive instancing resource -> resetMeshletInstanceInfoCountBuffers(commandList)
	graphicsView->resetMeshletInstanceInfoCountBuffers(commandList);
	graphicsView->resetIndirectArgumentCountBuffers(commandList);

	commandList->setComputeRootSignature(_gpuCullingRootSignature);
	commandList->setPipelineState(pipelineState);
	graphicsView->setGpuCullingResources(commandList);

	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_CULLING_SCENE_INFO, context._sceneConstantCbv);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_VIEW_INFO, context._cullingViewCbv);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MESH, context._meshHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MESH_INSTANCE, context._meshInstanceHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_SUB_MESH_DRAW_INFO, context._subMeshDrawInfoHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_INDIRECT_ARGUMENT_OFFSETS, context._indirectArgumentOffsetSrv);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MESHLET_INSTANCE_OFFSET, context._meshletInstanceInfoOffsetSrv);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MESHLET_INSTANCE_INFO, context._meshletInstanceInfoUav);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MESHLET_INSTANCE_COUNT, context._meshletInstanceInfoCountUav);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_MATERIAL_INSTANCE_INDEX, context._materialInstanceIndexSrv);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_PRIMITIVE_INSTANCING_OFFSETS, context._primitiveInstancingInfoOffsetSrv);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_PRIMITIVE_INSTANCING_INFOS, context._primitiveInstancingInfoUav);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_PRIMITIVE_INSTANCING_COUNTS, context._primitiveInstancingInfoCountUav);

	u32 dispatchCount = RoundUp(meshInstanceCountMax, 128u);
	commandList->dispatch(dispatchCount, 1, 1);
	graphicsView->resetResourceGpuCullingBarriers(commandList);
	// primitive instancing resource -> resetResourceGpuCullingBarriers(commandList)

	queryHeapSystem->setCurrentMarkerName(context._scopeName);
	queryHeapSystem->setMarker(commandList);
}
