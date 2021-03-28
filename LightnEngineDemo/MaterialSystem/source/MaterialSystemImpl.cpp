#include <MaterialSystem/impl/MaterialSystemImpl.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <GfxCore/impl/GraphicsApiInterface.h>
#include <MeshRenderer/GpuStruct.h>
#include <TextureSystem/impl/TextureSystemImpl.h>
#include <fstream>

MaterialSystemImpl _materialSystem;

void MaterialSystemImpl::initialize() {
	_materials.initialize(MATERIAL_COUNT_MAX);
	_shaderSets.initialize(SHADER_SET_COUNT_MAX);
	PipelineStateSystem::Get()->initialize();
}

void MaterialSystemImpl::update() {
	PipelineStateSystem::Get()->update();
}

void MaterialSystemImpl::processDeletion() {
	u32 materialCount = _materials.getArrayCountMax();
	for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (_materialStateFlags[materialIndex] & MATERIAL_STATE_FLAG_CREATED) {
			_materialStateFlags[materialIndex] = MATERIAL_STATE_FLAG_ENABLED;
		}

		if (_materialStateFlags[materialIndex] & MATERIAL_STATE_FLAG_REQEST_DESTROY) {
			MaterialImpl& material = _materials[materialIndex];
			ShaderSetImpl* shaderSet = material.getShaderSet();
			u32 shaderParamIndex = shaderSet->_shaderParams.getArrayIndex(material.getShaderParam());
			shaderSet->_shaderParams.discard(shaderParamIndex);

			_materialStateFlags[materialIndex] = MATERIAL_STATE_FLAG_NONE;
			_materialFileHashes[materialIndex] = 0;
			_materials.discard(materialIndex);
		}
	}

	for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (_materialUpdateFlags[materialIndex] & MATERIAL_UPDATE_FLAG_UPDATE_PARAMS) {
			_materialUpdateFlags[materialIndex] &= ~MATERIAL_UPDATE_FLAG_UPDATE_PARAMS;
		}
	}

	u32 shaderSetCount = _shaderSets.getArrayCountMax();
	for (u32 shaderSetIndex = 0; shaderSetIndex < shaderSetCount; ++shaderSetIndex) {
		if (_shaderSetStateFlags[shaderSetIndex] & SHADER_SET_STATE_FLAG_CREATED) {
			_shaderSetStateFlags[shaderSetIndex] = SHADER_SET_STATE_FLAG_ENABLED;
		}

		if (_shaderSetStateFlags[shaderSetIndex] & SHADER_SET_STATE_FLAG_REQEST_DESTROY) {
			_shaderSetStateFlags[shaderSetIndex] = SHADER_SET_STATE_FLAG_NONE;
			_shaderSets[shaderSetIndex].terminate();
			_pipelineStateSets[TYPE_MESH_SHADER].requestDelete(shaderSetIndex);
			_pipelineStateSets[TYPE_MESH_SHADER_PRIM_INSTANCING].requestDelete(shaderSetIndex);
			_pipelineStateSets[TYPE_CLASSIC].requestDelete(shaderSetIndex);

			_shaderSetFileHashes[shaderSetIndex] = 0;
			_shaderSets[shaderSetIndex] = ShaderSetImpl();

			_shaderSets.discard(shaderSetIndex);
		}
	}

	PipelineStateSystem::Get()->processDeletion();
}

void MaterialSystemImpl::terminate() {
	processDeletion();
	LTN_ASSERT(_materials.getInstanceCount() == 0);
	_materials.terminate();
	_shaderSets.terminate();
	PipelineStateSystem::Get()->terminate();
}

u32 MaterialSystemImpl::findShaderSetIndex(u64 fileHash) {
	u32 pipelineStateCount = _shaderSets.getArrayCountMax();
	for (u32 shaderSetIndex = 0; shaderSetIndex < pipelineStateCount; ++shaderSetIndex) {
		if (_shaderSetFileHashes[shaderSetIndex] == fileHash) {
			return shaderSetIndex;
		}
	}
	return gpu::INVALID_INDEX;
}

u32 MaterialSystemImpl::getShaderSetIndex(const ShaderSetImpl* shaderSet) const {
	return _shaderSets.getArrayIndex(shaderSet);
}

u32 MaterialSystemImpl::getMaterialIndex(const Material* material) const {
	return _materials.getArrayIndex(static_cast<const MaterialImpl*>(material));
}

bool MaterialSystemImpl::isEnabledShaderSet(const ShaderSetImpl* shaderSet) const {
	return !(_shaderSetStateFlags[getShaderSetIndex(shaderSet)] & SHADER_SET_STATE_FLAG_NONE);
}

ShaderSet* MaterialSystemImpl::createShaderSet(const ShaderSetDesc& desc) {
	u64 fileHash = StrHash(desc._filePath);
	u32 findIndex = findShaderSetIndex(fileHash);

	if (findIndex == gpu::INVALID_INDEX) {
		findIndex = _shaderSets.request();

		ShaderSetImplDesc implDesc = {};
		implDesc._debugCullingPassPipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER]._debugCullingPassPipelineStateGroups[findIndex];
		implDesc._debugDepthPipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER]._debugDepthPipelineStateGroups[findIndex];
		implDesc._debugLodLevelPipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER]._debugLodLevelPipelineStateGroups[findIndex];
		implDesc._debugMeshletPipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER]._debugMeshletPipelineStateGroups[findIndex];
		implDesc._debugOcclusionPipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER]._debugOcclusionPipelineStateGroups[findIndex];
		implDesc._debugTexcoordsPipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER]._debugTexcoordsPipelineStateGroups[findIndex];
		implDesc._debugWireFramePipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER]._debugWireFramePipelineStateGroups[findIndex];
		implDesc._depthPipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER]._depthPipelineStateGroups[findIndex];
		implDesc._pipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER]._pipelineStateGroups[findIndex];
		implDesc._primitiveInstancingPipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER_PRIM_INSTANCING]._pipelineStateGroups[findIndex];
		implDesc._primitiveInstancingDepthPipelineStateGroup = &_pipelineStateSets[TYPE_MESH_SHADER_PRIM_INSTANCING]._depthPipelineStateGroups[findIndex];
		implDesc._classicPipelineStateGroup = &_pipelineStateSets[TYPE_CLASSIC]._pipelineStateGroups[findIndex];
		implDesc._classicDepthPipelineStateGroup = &_pipelineStateSets[TYPE_CLASSIC]._depthPipelineStateGroups[findIndex];
		implDesc._commandSignature = &_pipelineStateSets[TYPE_MESH_SHADER]._commandSignatures[findIndex];
		implDesc._msCommandSignature = &_pipelineStateSets[TYPE_MESH_SHADER_PRIM_INSTANCING]._commandSignatures[findIndex];
		implDesc._multiDrawCommandSignature = &_pipelineStateSets[TYPE_CLASSIC]._commandSignatures[findIndex];

		ShaderSetImpl& shaderSet = _shaderSets[findIndex];
		shaderSet.initialize(desc, implDesc);
		shaderSet.setStateFlags(&_shaderSetStateFlags[findIndex]);
		_shaderSetFileHashes[findIndex] = fileHash;
		_shaderSetStateFlags[findIndex] |= SHADER_SET_STATE_FLAG_CREATED;
	}

	ShaderSetImpl* shaderSet = &_shaderSets[findIndex];
	return shaderSet;
}

Material* MaterialSystemImpl::createMaterial(const MaterialDesc& desc) {
	// アセット実パスに変換
	char meshFilePath[FILE_PATH_COUNT_MAX] = {};
	sprintf_s(meshFilePath, "%s/%s", RESOURCE_FOLDER_PATH, desc._filePath);

	// IO読み取り初期化
	std::ifstream fin(meshFilePath, std::ios::in | std::ios::binary);
	fin.exceptions(std::ios::badbit);
	LTN_ASSERT(!fin.fail());

	u64 shaderSetFileHash = 0;
	fin.read(reinterpret_cast<char*>(&shaderSetFileHash), sizeof(u64));

	u32 textureCount = 0;
	u64 textureFileHashes[32] = {};
	fin.read(reinterpret_cast<char*>(&textureCount), sizeof(u32));
	for (u32 textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
		fin.read(reinterpret_cast<char*>(&textureFileHashes[textureIndex]), sizeof(u64));
	}

	fin.close();

	u32 shaderSetIndex = findShaderSetIndex(shaderSetFileHash);
	LTN_ASSERT(shaderSetIndex != static_cast<u32>(-1));

	ShaderSetImpl* shaderSet = &_shaderSets[shaderSetIndex];
	u32 shaderSetMaterialIndex = shaderSet->_shaderParams.request();
	TempShaderParam* shaderParam = &shaderSet->_shaderParams[shaderSetMaterialIndex];
	shaderParam->_color = Color4::WHITE;
	shaderParam->_albedoTexture = TextureSystemImpl::Get()->findTexture(textureFileHashes[0]);

	u32 materialIndex = _materials.request();
	MaterialImpl* material = &_materials[materialIndex];
	material->setShaderSetStateFlags(&shaderSet->_shaderParamStateFlags[shaderSetMaterialIndex]);
	material->setShaderParam(shaderParam);
	material->setShaderSet(shaderSet);
	material->setStateFlags(&_materialStateFlags[materialIndex]);
	material->setUpdateFlags(&_materialUpdateFlags[materialIndex]);
	_materialFileHashes[materialIndex] = StrHash(desc._filePath);
	_materialStateFlags[materialIndex] |= MATERIAL_STATE_FLAG_CREATED;
	_materialUpdateFlags[materialIndex] |= MATERIAL_UPDATE_FLAG_UPDATE_PARAMS;
	return material;
}

Material* MaterialSystemImpl::findMaterial(u64 filePathHash) {
	u32 materialCount = _materials.getArrayCountMax();
	for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (_materialFileHashes[materialIndex] == filePathHash) {
			return &_materials[materialIndex];
		}
	}

	return nullptr;
}

MaterialSystemImpl* MaterialSystemImpl::Get() {
	return &_materialSystem;
}

MaterialSystem* MaterialSystem::Get() {
	return &_materialSystem;
}

void MaterialImpl::requestToDelete() {
	_shaderSet->requestToDelete();
	*_stateFlags |= MATERIAL_STATE_FLAG_REQEST_DESTROY;
}

void MaterialImpl::setTexture(Texture* texture, u64 parameterNameHash) {
	_shaderParam->_albedoTexture = texture;
	*_updateFlags |= MATERIAL_UPDATE_FLAG_UPDATE_PARAMS;
}

void ShaderSetImpl::requestToDelete() {
	*_stateFlags |= SHADER_SET_STATE_FLAG_REQEST_DESTROY;
}

void ShaderSetImpl::setTexture(Texture* texture, u64 parameterNameHash) {
}

void ShaderSetImpl::initialize(const ShaderSetDesc& desc, ShaderSetImplDesc& implDesc) {
	_shaderParams.initialize(MATERIAL_COUNT_MAX);

	// アセット実パスに変換
	char shaderSetFilePath[FILE_PATH_COUNT_MAX] = {};
	sprintf_s(shaderSetFilePath, "%s/%s", RESOURCE_FOLDER_PATH, desc._filePath);

	// IO読み取り初期化
	std::ifstream fin(shaderSetFilePath, std::ios::in | std::ios::binary);
	fin.exceptions(std::ios::badbit);
	LTN_ASSERT(!fin.fail());

	u32 meshShaderNameLength = 0;
	u32 pixelShaderNameLength = 0;
	char meshShaderName[FILE_PATH_COUNT_MAX] = {};
	char pixelShaderName[FILE_PATH_COUNT_MAX] = {};

	fin.read(reinterpret_cast<char*>(&meshShaderNameLength), sizeof(u32));
	fin.read(reinterpret_cast<char*>(meshShaderName), meshShaderNameLength);
	fin.read(reinterpret_cast<char*>(&pixelShaderNameLength), sizeof(u32));
	fin.read(reinterpret_cast<char*>(pixelShaderName), pixelShaderNameLength);

	fin.close();

	char meshShaderPath[FILE_PATH_COUNT_MAX] = {};
	char pixelShaderPath[FILE_PATH_COUNT_MAX] = {};
	sprintf_s(meshShaderPath, "%s/%s", RESOURCE_FOLDER_PATH, meshShaderName);
	sprintf_s(pixelShaderPath, "%s/%s", RESOURCE_FOLDER_PATH, pixelShaderName);

	constexpr u32 TEXTURE_BASE_REGISTER = 25;
	Device* device = GraphicsSystemImpl::Get()->getDevice();
	DescriptorRange cullingViewCbvRange = {};
	cullingViewCbvRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	DescriptorRange viewCbvRange = {};
	viewCbvRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	DescriptorRange materialDescriptorRange = {};
	materialDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	DescriptorRange meshDescriptorRange = {};
	meshDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 4, 1);

	DescriptorRange meshInstanceDescriptorRange = {};
	meshInstanceDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 4, 5);

	DescriptorRange meshletInfoSrvRange = {};
	meshletInfoSrvRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 9);

	DescriptorRange vertexDescriptorRange = {};
	vertexDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 5, 10);

	DescriptorRange currentLodLevelRange = {};
	currentLodLevelRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 15);

	DescriptorRange hizRange = {};
	hizRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 8, 16);

	DescriptorRange textureDescriptorRange = {};
	textureDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 128, TEXTURE_BASE_REGISTER);

	DescriptorRange cullingResultDescriptorRange = {};
	cullingResultDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	constexpr u32 ROOT_FRUSTUM_COUNT = ROOT_DEFAULT_MESH_COUNT - 2;
	RootParameter furstumCullingRootParameters[ROOT_FRUSTUM_COUNT] = {};
	RootParameter furstumOcclusionCullingRootParameters[ROOT_DEFAULT_MESH_COUNT] = {};

	RootSignatureDesc rootSignatureDescFurstumCulling = {};
	RootSignatureDesc rootSignatureDescFurstumOcclusionCulling = {};

	// メッシュレット　フラスタムカリングのみ
	{
		RootParameter* rootParameters = furstumCullingRootParameters;
		rootParameters[ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT].initializeDescriptorTable(1, &cullingViewCbvRange, SHADER_VISIBILITY_AMPLIFICATION);
		rootParameters[ROOT_DEFAULT_MESH_VIEW_CONSTANT].initializeDescriptorTable(1, &viewCbvRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MATERIALS].initializeDescriptorTable(1, &materialDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_INDIRECT_CONSTANT].initializeConstant(2, 3, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESHLET_INFO].initializeDescriptorTable(1, &meshletInfoSrvRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_VERTEX_RESOURCES].initializeDescriptorTable(1, &vertexDescriptorRange, SHADER_VISIBILITY_MESH);
		rootParameters[ROOT_DEFAULT_MESH_TEXTURES].initializeDescriptorTable(1, &textureDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_LOD_LEVEL].initializeDescriptorTable(1, &currentLodLevelRange, SHADER_VISIBILITY_ALL);

		rootSignatureDescFurstumCulling._device = device;
		rootSignatureDescFurstumCulling._numParameters = LTN_COUNTOF(furstumCullingRootParameters);
		rootSignatureDescFurstumCulling._parameters = rootParameters;
	}

	// メッシュレット　フラスタム + オクルージョンカリング
	{
		RootParameter* rootParameters = furstumOcclusionCullingRootParameters;
		rootParameters[ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT].initializeDescriptorTable(1, &cullingViewCbvRange, SHADER_VISIBILITY_AMPLIFICATION);
		rootParameters[ROOT_DEFAULT_MESH_VIEW_CONSTANT].initializeDescriptorTable(1, &viewCbvRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MATERIALS].initializeDescriptorTable(1, &materialDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_INDIRECT_CONSTANT].initializeConstant(2, 3, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESHLET_INFO].initializeDescriptorTable(1, &meshletInfoSrvRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_VERTEX_RESOURCES].initializeDescriptorTable(1, &vertexDescriptorRange, SHADER_VISIBILITY_MESH);
		rootParameters[ROOT_DEFAULT_MESH_TEXTURES].initializeDescriptorTable(1, &textureDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_CULLING_RESULT].initializeDescriptorTable(1, &cullingResultDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_LOD_LEVEL].initializeDescriptorTable(1, &currentLodLevelRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_HIZ].initializeDescriptorTable(1, &hizRange, SHADER_VISIBILITY_ALL);

		rootSignatureDescFurstumOcclusionCulling._device = device;
		rootSignatureDescFurstumOcclusionCulling._numParameters = LTN_COUNTOF(furstumOcclusionCullingRootParameters);
		rootSignatureDescFurstumOcclusionCulling._parameters = rootParameters; 
	}

	PipelineStateSystem* pipelineStateSystem = PipelineStateSystem::Get();
	MeshShaderPipelineStateGroupDesc pipelineStateDesc = {};
	pipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;

	// メッシュレット　プリミティブインスタンシング
	{
		RootParameter rootParameters[ROOT_FRUSTUM_COUNT] = {};
		memcpy(rootParameters, furstumCullingRootParameters, sizeof(RootParameter)* ROOT_FRUSTUM_COUNT);
		rootParameters[ROOT_DEFAULT_MESH_INDIRECT_CONSTANT].initializeConstant(2, 4, SHADER_VISIBILITY_ALL);

		RootSignatureDesc rootSignatureDesc = rootSignatureDescFurstumCulling;
		rootSignatureDesc._parameters = rootParameters;

		// Depth Only
		pipelineStateDesc._meshShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\standard_mesh\\default_mesh_primitive_instancing.mso";
		*implDesc._primitiveInstancingDepthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDesc);

		// フラスタム ＋ オクルージョンカリング
		pipelineStateDesc._pixelShaderFilePath = pixelShaderPath;
		*implDesc._primitiveInstancingPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDesc);
	}

	pipelineStateDesc._meshShaderFilePath = meshShaderPath;
	pipelineStateDesc._pixelShaderFilePath = pixelShaderPath;

	// GPUカリング無効デバッグ用
	pipelineStateDesc._amplificationShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\meshlet\\meshlet_culling_pass.aso";
	*implDesc._debugCullingPassPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// フラスタム ＋ オクルージョンカリング
	pipelineStateDesc._amplificationShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\meshlet\\meshlet_culling_frustum_occlusion.aso";
	*implDesc._pipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// メッシュレットデバッグ用
	pipelineStateDesc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_meshlet.pso";
	*implDesc._debugMeshletPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// LodLevel デバッグ用
	pipelineStateDesc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_lod.pso";
	*implDesc._debugLodLevelPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// Depth デバッグ用
	pipelineStateDesc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_depth.pso";
	*implDesc._debugDepthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// Texcoords デバッグ用
	pipelineStateDesc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_texcoords.pso";
	*implDesc._debugTexcoordsPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// ワイヤーフレーム表示
	{
		MeshShaderPipelineStateGroupDesc desc = pipelineStateDesc;
		desc._fillMode = FILL_MODE_WIREFRAME;
		desc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_wireframe.pso";
		*implDesc._debugWireFramePipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(desc, rootSignatureDescFurstumOcclusionCulling);
	}

	// デプスオンリー (フラスタムカリングのみ)
	pipelineStateDesc._amplificationShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\meshlet\\meshlet_culling_frustum.aso";
	pipelineStateDesc._pixelShaderFilePath = nullptr;
	*implDesc._depthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumCulling);

	// オクルージョンカリング可視化
	pipelineStateDesc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_occlusion_culling.pso";
	pipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_ALWAYS;

	RenderTargetBlendDesc& blendDesc = pipelineStateDesc._blendDesc._renderTarget[0];
	blendDesc._blendEnable = true;
	blendDesc._srcBlend = BLEND_SRC_ALPHA;
	blendDesc._destBlend = BLEND_INV_SRC_ALPHA;
	blendDesc._blendOp = BLEND_OP_ADD;
	blendDesc._srcBlendAlpha = BLEND_ONE;
	blendDesc._destBlendAlpha = BLEND_ZERO;
	blendDesc._blendOpAlpha = BLEND_OP_ADD;
	*implDesc._debugOcclusionPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// classic
	{
		DescriptorRange cbvDescriptorRange = {};
		cbvDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		DescriptorRange meshInstanceDescriptorRange = {};
		meshInstanceDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		DescriptorRange materialDescriptorRange = {};
		materialDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		DescriptorRange textureDescriptorRange = {};
		textureDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 128, TEXTURE_BASE_REGISTER);

		RootParameter rootParameters[ROOT_CLASSIC_MESH_COUNT] = {};
		rootParameters[ROOT_CLASSIC_MESH_SCENE_CONSTANT].initializeDescriptorTable(1, &cbvDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_CLASSIC_MESH_INFO].initializeConstant(1, 2, SHADER_VISIBILITY_VERTEX);
		rootParameters[ROOT_CLASSIC_MESH_MATERIALS].initializeDescriptorTable(1, &materialDescriptorRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ROOT_CLASSIC_MESH_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_VERTEX);
		rootParameters[ROOT_CLASSIC_MESH_TEXTURES].initializeDescriptorTable(1, &textureDescriptorRange, SHADER_VISIBILITY_PIXEL);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		rootSignatureDesc._flags = ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		char vertexShaderName[FILE_PATH_COUNT_MAX] = {};
		memcpy(vertexShaderName, meshShaderName, meshShaderNameLength - 3);
		memcpy(vertexShaderName + meshShaderNameLength - 3, "vso", 3);

		char vertexShaderPath[FILE_PATH_COUNT_MAX] = {};
		sprintf_s(vertexShaderPath, "%s/%s", RESOURCE_FOLDER_PATH, vertexShaderName);

		ClassicPipelineStateGroupDesc desc = {};
		desc._vertexShaderFilePath = vertexShaderPath;
		desc._pixelShaderFilePath = pixelShaderPath;
		desc._depthComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;
		*implDesc._classicPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(desc, rootSignatureDesc);

		desc._pixelShaderFilePath = nullptr;
		desc._depthComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;
		*implDesc._classicDepthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(desc, rootSignatureDesc);

		// "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_meshlet.pso"
	}

	{
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();
		*implDesc._commandSignature = allocator->allocateCommandSignature();
		*implDesc._msCommandSignature = allocator->allocateCommandSignature();

		IndirectArgumentDesc argumentDescs[2] = {};
		argumentDescs[0]._type = INDIRECT_ARGUMENT_TYPE_CONSTANT;
		argumentDescs[0].Constant._num32BitValuesToSet = 3;
		argumentDescs[0].Constant._rootParameterIndex = ROOT_DEFAULT_MESH_INDIRECT_CONSTANT;
		argumentDescs[1]._type = INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

		CommandSignatureDesc desc = {};
		desc._device = device;
		desc._byteStride = sizeof(gpu::DispatchMeshIndirectArgumentAS);
		desc._argumentDescs = argumentDescs;
		desc._numArgumentDescs = LTN_COUNTOF(argumentDescs);
		desc._rootSignature = (*implDesc._pipelineStateGroup)->getRootSignature();
		(*implDesc._commandSignature)->initialize(desc);

		argumentDescs[0].Constant._num32BitValuesToSet = 4;
		desc._byteStride = sizeof(gpu::DispatchMeshIndirectArgumentMS);
		desc._rootSignature = (*implDesc._primitiveInstancingPipelineStateGroup)->getRootSignature();
		(*implDesc._msCommandSignature)->initialize(desc);
	}

#if ENABLE_MULTI_INDIRECT_DRAW
	{
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();

		IndirectArgumentDesc argumentDescs[2] = {};
		argumentDescs[0]._type = INDIRECT_ARGUMENT_TYPE_CONSTANT;
		argumentDescs[0].Constant._rootParameterIndex = ROOT_CLASSIC_MESH_INFO;
		argumentDescs[0].Constant._num32BitValuesToSet = 2;
		argumentDescs[1]._type = INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		CommandSignatureDesc desc = {};
		desc._device = device;
		desc._byteStride = sizeof(gpu::StarndardMeshIndirectArguments);
		desc._argumentDescs = argumentDescs;
		desc._numArgumentDescs = LTN_COUNTOF(argumentDescs);
		desc._rootSignature = (*implDesc._classicPipelineStateGroup)->getRootSignature();
		(*implDesc._multiDrawCommandSignature) = allocator->allocateCommandSignature();
		(*implDesc._multiDrawCommandSignature)->initialize(desc);
	}
#endif
}

void ShaderSetImpl::terminate() {
	_shaderParams.terminate();
}

void PipelineStateSet::requestDelete(u32 shaderSetIndex){
	if (_pipelineStateGroups[shaderSetIndex]) {
		_pipelineStateGroups[shaderSetIndex]->requestToDestroy();
		_pipelineStateGroups[shaderSetIndex] = nullptr;
	}

	if (_depthPipelineStateGroups[shaderSetIndex]) {
		_depthPipelineStateGroups[shaderSetIndex]->requestToDestroy();
		_depthPipelineStateGroups[shaderSetIndex] = nullptr;
	}

	if (_debugCullingPassPipelineStateGroups[shaderSetIndex]) {
		_debugCullingPassPipelineStateGroups[shaderSetIndex]->requestToDestroy();
		_debugCullingPassPipelineStateGroups[shaderSetIndex] = nullptr;
	}

	if (_debugMeshletPipelineStateGroups[shaderSetIndex]) {
		_debugMeshletPipelineStateGroups[shaderSetIndex]->requestToDestroy();
		_debugMeshletPipelineStateGroups[shaderSetIndex] = nullptr;
	}

	if (_debugLodLevelPipelineStateGroups[shaderSetIndex]) {
		_debugLodLevelPipelineStateGroups[shaderSetIndex]->requestToDestroy();
		_debugLodLevelPipelineStateGroups[shaderSetIndex] = nullptr;
	}

	if (_debugOcclusionPipelineStateGroups[shaderSetIndex]) {
		_debugOcclusionPipelineStateGroups[shaderSetIndex]->requestToDestroy();
		_debugOcclusionPipelineStateGroups[shaderSetIndex] = nullptr;
	}

	if (_debugDepthPipelineStateGroups[shaderSetIndex]) {
		_debugDepthPipelineStateGroups[shaderSetIndex]->requestToDestroy();
		_debugDepthPipelineStateGroups[shaderSetIndex] = nullptr;
	}

	if (_debugTexcoordsPipelineStateGroups[shaderSetIndex]) {
		_debugTexcoordsPipelineStateGroups[shaderSetIndex]->requestToDestroy();
		_debugTexcoordsPipelineStateGroups[shaderSetIndex] = nullptr;
	}

	if (_debugWireFramePipelineStateGroups[shaderSetIndex]) {
		_debugWireFramePipelineStateGroups[shaderSetIndex]->requestToDestroy();
		_debugWireFramePipelineStateGroups[shaderSetIndex] = nullptr;
	}

	if (_commandSignatures[shaderSetIndex]) {
		_commandSignatures[shaderSetIndex]->terminate();
		_commandSignatures[shaderSetIndex] = nullptr;
	}
}
