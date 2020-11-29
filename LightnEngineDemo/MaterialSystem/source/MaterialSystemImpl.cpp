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
			_shaderSetFileHashes[shaderSetIndex] = 0;
			_shaderSets[shaderSetIndex].terminate();
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
	return static_cast<u32>(-1);
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

	if (findIndex == static_cast<u32>(-1)) {
		findIndex = _shaderSets.request();
		ShaderSetImpl& shaderSet = _shaderSets[findIndex];
		shaderSet.initialize(desc);
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

void ShaderSetImpl::initialize(const ShaderSetDesc& desc) {
	_shaderParams.initialize(MATERIAL_COUNT_MAX);

	// アセット実パスに変換
	char meshFilePath[FILE_PATH_COUNT_MAX] = {};
	sprintf_s(meshFilePath, "%s/%s", RESOURCE_FOLDER_PATH, desc._filePath);

	// IO読み取り初期化
	std::ifstream fin(meshFilePath, std::ios::in | std::ios::binary);
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

	Device* device = GraphicsSystemImpl::Get()->getDevice();
	RootSignatureDesc rootSignatureDescFurstumCulling = {};
	RootSignatureDesc rootSignatureDescFurstumOcclusionCulling = {};
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

	DescriptorRange batchedSubMeshInfoRange = {};
	batchedSubMeshInfoRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 9);

	DescriptorRange vertexDescriptorRange = {};
	vertexDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 5, 9);

	DescriptorRange currentLodLevelRange = {};
	currentLodLevelRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 15);

	DescriptorRange hizRange = {};
	hizRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 8, 16);

	DescriptorRange textureDescriptorRange = {};
	textureDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 128, 9);

	DescriptorRange cullingResultDescriptorRange = {};
	cullingResultDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// メッシュレット　フラスタムカリングのみ
	{
		constexpr u32 ROOT_FRUSTUM_COUNT = ROOT_DEFAULT_MESH_COUNT - 2;

		RootParameter rootParameters[ROOT_FRUSTUM_COUNT] = {};
		rootParameters[ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT].initializeDescriptorTable(1, &cullingViewCbvRange, SHADER_VISIBILITY_AMPLIFICATION);
		rootParameters[ROOT_DEFAULT_MESH_VIEW_CONSTANT].initializeDescriptorTable(1, &viewCbvRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MATERIALS].initializeDescriptorTable(1, &materialDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_BATCHED_INDEX].initializeConstant(2, 1, SHADER_VISIBILITY_AMPLIFICATION);
		rootParameters[ROOT_DEFAULT_MESH_BATCHED_INFO].initializeDescriptorTable(1, &batchedSubMeshInfoRange, SHADER_VISIBILITY_AMPLIFICATION);
		rootParameters[ROOT_DEFAULT_MESH_VERTEX_RESOURCES].initializeDescriptorTable(1, &vertexDescriptorRange, SHADER_VISIBILITY_MESH);
		rootParameters[ROOT_DEFAULT_MESH_TEXTURES].initializeDescriptorTable(1, &textureDescriptorRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ROOT_DEFAULT_MESH_LOD_LEVEL].initializeDescriptorTable(1, &currentLodLevelRange, SHADER_VISIBILITY_MESH);

		rootSignatureDescFurstumCulling._device = device;
		rootSignatureDescFurstumCulling._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDescFurstumCulling._parameters = rootParameters;
	}

	// メッシュレット　フラスタム + オクルージョンカリング
	{
		RootParameter rootParameters[ROOT_DEFAULT_MESH_COUNT] = {};
		rootParameters[ROOT_DEFAULT_MESH_CULLING_VIEW_CONSTANT].initializeDescriptorTable(1, &cullingViewCbvRange, SHADER_VISIBILITY_AMPLIFICATION);
		rootParameters[ROOT_DEFAULT_MESH_VIEW_CONSTANT].initializeDescriptorTable(1, &viewCbvRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MATERIALS].initializeDescriptorTable(1, &materialDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_BATCHED_INDEX].initializeConstant(2, 1, SHADER_VISIBILITY_AMPLIFICATION);
		rootParameters[ROOT_DEFAULT_MESH_BATCHED_INFO].initializeDescriptorTable(1, &batchedSubMeshInfoRange, SHADER_VISIBILITY_AMPLIFICATION);
		rootParameters[ROOT_DEFAULT_MESH_VERTEX_RESOURCES].initializeDescriptorTable(1, &vertexDescriptorRange, SHADER_VISIBILITY_MESH);
		rootParameters[ROOT_DEFAULT_MESH_TEXTURES].initializeDescriptorTable(1, &textureDescriptorRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ROOT_DEFAULT_MESH_CULLING_RESULT].initializeDescriptorTable(1, &cullingResultDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ROOT_DEFAULT_MESH_LOD_LEVEL].initializeDescriptorTable(1, &currentLodLevelRange, SHADER_VISIBILITY_MESH);
		rootParameters[ROOT_DEFAULT_MESH_HIZ].initializeDescriptorTable(1, &hizRange, SHADER_VISIBILITY_AMPLIFICATION);

		rootSignatureDescFurstumOcclusionCulling._device = device;
		rootSignatureDescFurstumOcclusionCulling._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDescFurstumOcclusionCulling._parameters = rootParameters; 
	}

	PipelineStateSystem* pipelineStateSystem = PipelineStateSystem::Get();
	PipelineStateGroupDesc pipelineStateDesc = {};
	pipelineStateDesc._meshShaderFilePath = meshShaderPath;
	pipelineStateDesc._pixelShaderFilePath = pixelShaderPath;
	pipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;

	// GPUカリング無効デバッグ用
	pipelineStateDesc._amplificationShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\meshlet\\meshlet_culling_pass.aso";
	_debugCullingPassPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// フラスタム＋オクルージョンカリング
	pipelineStateDesc._amplificationShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\meshlet\\meshlet_culling_frustum_occlusion.aso";
	_pipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// メッシュレットデバッグ用
	pipelineStateDesc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_meshlet.pso";
	_debugMeshletPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// LodLevel デバッグ用
	pipelineStateDesc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_lod.pso";
	_debugLodLevelPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// Depth デバッグ用
	pipelineStateDesc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_depth.pso";
	_debugDepthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// Texcoords デバッグ用
	pipelineStateDesc._pixelShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_texcoords.pso";
	_debugTexcoordsPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

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
	_debugOcclusionPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

	// デプスオンリー (フラスタムカリングのみ)
	pipelineStateDesc._amplificationShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\meshlet\\meshlet_culling_frustum.aso";
	pipelineStateDesc._pixelShaderFilePath = "";
	pipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;
	_depthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumCulling);


	// classic
	{
		Device* device = GraphicsSystemImpl::Get()->getDevice();
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();

		char vertexShaderName[FILE_PATH_COUNT_MAX] = {};
		memcpy(vertexShaderName, meshShaderName, meshShaderNameLength - 3);
		memcpy(vertexShaderName + meshShaderNameLength - 3, "vso", 3);

		char vertexShaderPath[FILE_PATH_COUNT_MAX] = {};
		sprintf_s(vertexShaderPath, "%s/%s", RESOURCE_FOLDER_PATH, vertexShaderName);

		ShaderBlob* vertexShader = allocator->allocateShaderBlob();
		ShaderBlob* pixelShader = allocator->allocateShaderBlob();
		vertexShader->initialize(vertexShaderPath);
		pixelShader->initialize(pixelShaderPath);

		_classicShaderSet._depthPipelineState = allocator->allocatePipelineState();
		_classicShaderSet._pipelineState = allocator->allocatePipelineState();
		_classicShaderSet._rootSignature = allocator->allocateRootSignature();

		DescriptorRange cbvDescriptorRange = {};
		cbvDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		DescriptorRange meshInstanceDescriptorRange = {};
		meshInstanceDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		DescriptorRange materialDescriptorRange = {};
		materialDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		DescriptorRange textureDescriptorRange = {};
		textureDescriptorRange.initialize(DESCRIPTOR_RANGE_TYPE_SRV, 128, 9);

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
		_classicShaderSet._rootSignature->iniaitlize(rootSignatureDesc);

		InputElementDesc inputElements[2] = {};
		inputElements[0]._inputSlot = 0;
		inputElements[0]._format = FORMAT_R32G32B32_FLOAT;
		inputElements[0]._semanticName = "POSITION";

		inputElements[1]._inputSlot = 1;
		inputElements[1]._format = FORMAT_R32_UINT;
		inputElements[1]._semanticName = "PACKED_TEX";

		GraphicsPipelineStateDesc pipelineStateDesc = {};
		pipelineStateDesc._device = device;
		pipelineStateDesc._vs = vertexShader->getShaderByteCode();
		pipelineStateDesc._ps = pixelShader->getShaderByteCode();
		pipelineStateDesc._numRenderTarget = 1;
		pipelineStateDesc._rtvFormats[0] = FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc._topologyType = PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateDesc._rootSignature = _classicShaderSet._rootSignature;
		pipelineStateDesc._sampleDesc._count = 1;
		pipelineStateDesc._dsvFormat = FORMAT_D32_FLOAT;
		pipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;
		pipelineStateDesc._inputElements = inputElements;
		pipelineStateDesc._inputElementCount = LTN_COUNTOF(inputElements);
		_classicShaderSet._pipelineState->iniaitlize(pipelineStateDesc);

		pipelineStateDesc._ps = ShaderByteCode();
		_classicShaderSet._depthPipelineState->iniaitlize(pipelineStateDesc);

		vertexShader->terminate();
		pixelShader->terminate();
	}

#if ENABLE_MULTI_INDIRECT_DRAW
	{
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();
		_classicShaderSet._multiDrawCommandSignature = allocator->allocateCommandSignature();

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
		desc._rootSignature = _classicShaderSet._rootSignature;
		_classicShaderSet._multiDrawCommandSignature->initialize(desc);
	}
#endif
}

void ShaderSetImpl::terminate() {
	_pipelineStateGroup->requestToDestroy();
	_depthPipelineStateGroup->requestToDestroy();
	_debugCullingPassPipelineStateGroup->requestToDestroy();
	_debugMeshletPipelineStateGroup->requestToDestroy();
	_debugLodLevelPipelineStateGroup->requestToDestroy();
	_debugOcclusionPipelineStateGroup->requestToDestroy();
	_debugDepthPipelineStateGroup->requestToDestroy();
	_debugTexcoordsPipelineStateGroup->requestToDestroy();
	_classicShaderSet._pipelineState->terminate();
	_classicShaderSet._depthPipelineState->terminate();
	_classicShaderSet._rootSignature->terminate();
#if ENABLE_MULTI_INDIRECT_DRAW
	_classicShaderSet._multiDrawCommandSignature->terminate();
#endif
	_shaderParams.terminate();
}
