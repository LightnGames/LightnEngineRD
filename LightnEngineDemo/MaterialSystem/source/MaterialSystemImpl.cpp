#include <MaterialSystem/impl/MaterialSystemImpl.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <GfxCore/impl/GraphicsApiInterface.h>
#include <MeshRenderer/GpuStruct.h>
#include <TextureSystem/impl/TextureSystemImpl.h>
#include <TextureSystem/impl/TextureStreamingSystem.h>
#include <fstream>

MaterialSystemImpl _materialSystem;

void MaterialSystemImpl::initialize() {
	_materials.initialize(MATERIAL_COUNT_MAX);
	_shaderSets.initialize(SHADER_SET_COUNT_MAX);
	PipelineStateSystem::Get()->initialize();

	Device* device = GraphicsSystemImpl::Get()->getDevice();

	// Material screen area feedback buffers
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._sizeInByte = MATERIAL_COUNT_MAX * sizeof(u32);
		desc._initialState = RESOURCE_STATE_UNORDERED_ACCESS;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		_screenAreaFeedbackBuffer.initialize(desc);
		_screenAreaFeedbackBuffer.setDebugName("Material Lod Level Feedback");

		desc._sizeInByte = MATERIAL_COUNT_MAX * sizeof(u32) * BACK_BUFFER_COUNT;
		desc._initialState = RESOURCE_STATE_COPY_DEST;
		desc._heapType = HEAP_TYPE_READBACK;
		desc._flags = RESOURCE_FLAG_NONE;
		_screenAreaReadbackBuffer.initialize(desc);
		_screenAreaReadbackBuffer.setDebugName("Material Lod Level Readback");
	}

	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		DescriptorHeapAllocator* cpuAllocator = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();
		_screenAreaFeedbackUav = allocator->allocateDescriptors(1);
		_screenAreaFeedbackCpuUav = cpuAllocator->allocateDescriptors(1);

		UnorderedAccessViewDesc desc = {};
		desc._format = FORMAT_R32_TYPELESS;
		desc._viewDimension = UAV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._numElements = MATERIAL_COUNT_MAX;
		desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
		device->createUnorderedAccessView(_screenAreaFeedbackBuffer.getResource(), nullptr, &desc, _screenAreaFeedbackUav._cpuHandle);
		device->createUnorderedAccessView(_screenAreaFeedbackBuffer.getResource(), nullptr, &desc, _screenAreaFeedbackCpuUav._cpuHandle);
	}
}

void MaterialSystemImpl::terminate() {
	processDeletion();
	LTN_ASSERT(_materials.getInstanceCount() == 0);
	_materials.terminate();
	_shaderSets.terminate();
	PipelineStateSystem::Get()->terminate();

	_screenAreaFeedbackBuffer.terminate();
	_screenAreaReadbackBuffer.terminate();

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	DescriptorHeapAllocator* cpuAllocator = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();
	allocator->discardDescriptor(_screenAreaFeedbackUav);
	cpuAllocator->discardDescriptor(_screenAreaFeedbackCpuUav);
}

void MaterialSystemImpl::update() {
	PipelineStateSystem::Get()->update();

	// Screen area �����[�h�o�b�N
	{
		u32 frameIndex = GraphicsSystemImpl::Get()->getFrameIndex();
		u64 startOffset = static_cast<u64>(frameIndex) * MATERIAL_COUNT_MAX;
		MemoryRange range(startOffset, MATERIAL_COUNT_MAX);
		u32* mapPtr = _screenAreaReadbackBuffer.map<u32>(&range);
		memcpy(_screenAreas, mapPtr, sizeof(u32) * MATERIAL_COUNT_MAX);
		_screenAreaReadbackBuffer.unmap();
	}

	// �e�N�X�`���X�g���[�~���O�V�X�e���Ƀ}�e���A���������Ă���e�N�X�`���ɃX�g���[�~���O���x���𑗐M
	TextureStreamingSystem* textureStreamingSystem = TextureStreamingSystem::Get();
	textureStreamingSystem->resetStreamingLevels();
	u32 materialCount = _materials.getResarveCount();
	for (u32 i = 0; i < materialCount; ++i) {
		MaterialImpl* material = &_materials[i];
		u16 screenAreaUint = min(_screenAreas[i], UINT16_MAX);
		if (screenAreaUint == UINT16_MAX) {
			continue;
		}

		f32 screenArea = static_cast<f32>(screenAreaUint) / UINT16_MAX;
		u16 foundIndices[16];
		u16 foundCount = _materials[i].findParameterCount(Material::ShaderValiableType::TEXTURE, foundIndices);
		for (u32 j = 0; j < foundCount; ++j) {
			const u32* textureIndex = _materials[i].getParameterFromIndex<u32>(foundIndices[j]);
			textureStreamingSystem->updateStreamingLevel(*textureIndex, screenArea);
		}
	}
}

void MaterialSystemImpl::processDeletion() {
	u32 materialCount = _materials.getResarveCount();
	for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (_materialStateFlags[materialIndex] & MATERIAL_STATE_FLAG_CREATED) {
			_materialStateFlags[materialIndex] = MATERIAL_STATE_FLAG_ENABLED;
		}

		if (_materialStateFlags[materialIndex] & MATERIAL_STATE_FLAG_REQEST_DESTROY) {
			MaterialImpl& material = _materials[materialIndex];
			ShaderSetImpl* shaderSet = material.getShaderSet();
			u32 shaderParamIndex = static_cast<u32>(material.getShaderSetStateFlags() - shaderSet->_shaderParamStateFlags);
			shaderSet->_shaderParamInstances.discard(shaderParamIndex);

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

	u32 shaderSetCount = _shaderSets.getResarveCount();
	for (u32 shaderSetIndex = 0; shaderSetIndex < shaderSetCount; ++shaderSetIndex) {
		if (_shaderSetStateFlags[shaderSetIndex] & SHADER_SET_STATE_FLAG_CREATED) {
			_shaderSetStateFlags[shaderSetIndex] = SHADER_SET_STATE_FLAG_ENABLED;
		}

		if (_shaderSetStateFlags[shaderSetIndex] & SHADER_SET_STATE_FLAG_REQEST_DESTROY) {
			_shaderSetStateFlags[shaderSetIndex] = SHADER_SET_STATE_FLAG_NONE;
			_shaderSets[shaderSetIndex].terminate();
			_pipelineStateSets[TYPE_AS_MESH_SHADER].requestDelete(shaderSetIndex);
			_pipelineStateSets[TYPE_MESH_SHADER].requestDelete(shaderSetIndex);
			_pipelineStateSets[TYPE_CLASSIC].requestDelete(shaderSetIndex);

			_shaderSetFileHashes[shaderSetIndex] = 0;
			_shaderSets[shaderSetIndex] = ShaderSetImpl();

			_shaderSets.discard(shaderSetIndex);
		}
	}

	PipelineStateSystem::Get()->processDeletion();
}

u32 MaterialSystemImpl::findShaderSetIndex(u64 fileHash) {
	u32 pipelineStateCount = _shaderSets.getResarveCount();
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

		PipelineStateSet& meshShaderPipelineStateSet = _pipelineStateSets[TYPE_AS_MESH_SHADER];
		PipelineStateSet& primInstancingPipelineStateSet = _pipelineStateSets[TYPE_MESH_SHADER];
		PipelineStateSet& classicPipelineStateSet = _pipelineStateSets[TYPE_CLASSIC];
		PipelineStateSet& shadingPipelineStateSet = _pipelineStateSets[TYPE_SHADING];

		ShaderSetImplDesc implDesc = {};
		implDesc._debugCullingPassPipelineStateGroup = &meshShaderPipelineStateSet._debugCullingPassPipelineStateGroups[findIndex];
		implDesc._debugDepthPipelineStateGroup = &meshShaderPipelineStateSet._debugDepthPipelineStateGroups[findIndex];
		implDesc._debugLodLevelPipelineStateGroup = &meshShaderPipelineStateSet._debugLodLevelPipelineStateGroups[findIndex];
		implDesc._debugMeshletPipelineStateGroup = &meshShaderPipelineStateSet._debugMeshletPipelineStateGroups[findIndex];
		implDesc._debugOcclusionPipelineStateGroup = &meshShaderPipelineStateSet._debugOcclusionPipelineStateGroups[findIndex];
		implDesc._debugTexcoordsPipelineStateGroup = &meshShaderPipelineStateSet._debugTexcoordsPipelineStateGroups[findIndex];
		implDesc._debugWireFramePipelineStateGroup = &meshShaderPipelineStateSet._debugWireFramePipelineStateGroups[findIndex];
		implDesc._commandSignature = &meshShaderPipelineStateSet._commandSignatures[findIndex];

		implDesc._debugPrimCullingPassPipelineStateGroup = &primInstancingPipelineStateSet._debugCullingPassPipelineStateGroups[findIndex];
		implDesc._debugPrimDepthPipelineStateGroup = &primInstancingPipelineStateSet._debugDepthPipelineStateGroups[findIndex];
		implDesc._debugPrimLodLevelPipelineStateGroup = &primInstancingPipelineStateSet._debugLodLevelPipelineStateGroups[findIndex];
		implDesc._debugPrimMeshletPipelineStateGroup = &primInstancingPipelineStateSet._debugMeshletPipelineStateGroups[findIndex];
		implDesc._debugPrimOcclusionPipelineStateGroup = &primInstancingPipelineStateSet._debugOcclusionPipelineStateGroups[findIndex];
		implDesc._debugPrimTexcoordsPipelineStateGroup = &primInstancingPipelineStateSet._debugTexcoordsPipelineStateGroups[findIndex];
		implDesc._debugPrimWireFramePipelineStateGroup = &primInstancingPipelineStateSet._debugWireFramePipelineStateGroups[findIndex];
		implDesc._msCommandSignature = &primInstancingPipelineStateSet._commandSignatures[findIndex];

		implDesc._depthPipelineStateGroup = &meshShaderPipelineStateSet._depthPipelineStateGroups[findIndex];
		implDesc._pipelineStateGroup = &meshShaderPipelineStateSet._pipelineStateGroups[findIndex];
		implDesc._primPipelineStateGroup = &primInstancingPipelineStateSet._pipelineStateGroups[findIndex];
		implDesc._primDepthPipelineStateGroup = &primInstancingPipelineStateSet._depthPipelineStateGroups[findIndex];
		implDesc._classicPipelineStateGroup = &classicPipelineStateSet._pipelineStateGroups[findIndex];
		implDesc._classicDepthPipelineStateGroup = &classicPipelineStateSet._depthPipelineStateGroups[findIndex];
		implDesc._multiDrawCommandSignature = &classicPipelineStateSet._commandSignatures[findIndex];

		implDesc._shadingPipelineStateGroup = &shadingPipelineStateSet._pipelineStateGroups[findIndex];

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
	// �A�Z�b�g���p�X�ɕϊ�
	char meshFilePath[FILE_PATH_COUNT_MAX] = {};
	sprintf_s(meshFilePath, "%s%s", RESOURCE_FOLDER_PATH, desc._filePath);

	// IO�ǂݎ�菉����
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
	u32 shaderSetMaterialIndex = shaderSet->_shaderParamInstances.request();

	u32 materialIndex = _materials.request();
	MaterialImpl* material = &_materials[materialIndex];
	material->setShaderSetStateFlags(&shaderSet->_shaderParamStateFlags[shaderSetMaterialIndex]);
	material->setParameterDataPtr(&shaderSet->_parameterDatas[shaderSet->_parameterSizeInByte*shaderSetMaterialIndex]);
	material->setShaderSet(shaderSet);
	material->setStateFlags(&_materialStateFlags[materialIndex]);
	material->setUpdateFlags(&_materialUpdateFlags[materialIndex]);
	_materialFileHashes[materialIndex] = StrHash(desc._filePath);
	_materialStateFlags[materialIndex] |= MATERIAL_STATE_FLAG_CREATED;
	_materialUpdateFlags[materialIndex] |= MATERIAL_UPDATE_FLAG_UPDATE_PARAMS;

	Texture* texture = TextureSystemImpl::Get()->findTexture(textureFileHashes[0]);
	material->setParameter<Color4>(StrHash32("BaseColor"), Color4::WHITE);
	material->setTexture(StrHash32("BaseColorRoughness"), texture);

	return material;
}

Material* MaterialSystemImpl::findMaterial(u64 filePathHash) {
	u32 materialCount = _materials.getResarveCount();
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

void MaterialImpl::setTexture(u32 nameHash, Texture* texture) {
	setParameter<u32>(nameHash, TextureSystemImpl::Get()->getTextureIndex(texture));
}

const u8* MaterialImpl::getParameterRawFromIndex(u32 index) const
{
	return &_parameterData[_shaderSet->_parameterByteOffset[index]];
}

const u8* MaterialImpl::getParameterRaw(u32 nameHash) const {
	u16 findIndex = findParameter(nameHash);
	if (findIndex == INVALID_PARAMETER_INDEX) {
		return nullptr;
	}

	return getParameterRawFromIndex(findIndex);
}

void MaterialImpl::setParameterRaw(u32 nameHash, const void* dataPtr) {
	u16 findIndex = findParameter(nameHash);
	LTN_ASSERT(findIndex != INVALID_PARAMETER_INDEX);

	u16 copySizeInByte = PARAM_TYPE_SIZE_IN_BYTE[_shaderSet->_parameterTypes[findIndex]];
	u16 offset = _shaderSet->_parameterByteOffset[findIndex];
	u8* basePtr = &_parameterData[offset];
	memcpy(basePtr, dataPtr, copySizeInByte);

	*_updateFlags |= MATERIAL_UPDATE_FLAG_UPDATE_PARAMS;
}

u16 MaterialImpl::findParameter(u32 nameHash) const {
	u16 parameterCount = _shaderSet->_parameterCount;
	u32* parameterNameHashes = _shaderSet->_parameterNameHashes;
	for (u16 parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex) {
		if (nameHash == parameterNameHashes[parameterIndex]) {
			return parameterIndex;
		}
	}

	return INVALID_PARAMETER_INDEX;
}

u16 MaterialImpl::findParameterCount(u16 typeIndex, u16* outTypeIndices) const
{
	u16 parameterCount = _shaderSet->_parameterCount;
	u16* parameterTypes = _shaderSet->_parameterTypes;
	u16 foundCount = 0;
	for (u16 parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex) {
		if (typeIndex == parameterTypes[parameterIndex]) {
			if (outTypeIndices != nullptr) {
				outTypeIndices[foundCount] = parameterIndex;
			}
			foundCount++;
		}
	}

	return foundCount;
}

void ShaderSetImpl::requestToDelete() {
	*_stateFlags |= SHADER_SET_STATE_FLAG_REQEST_DESTROY;
}

void ShaderSetImpl::setTexture(Texture* texture, u64 parameterNameHash) {
}

void ShaderSetImpl::initialize(const ShaderSetDesc& desc, ShaderSetImplDesc& implDesc) {
	_shaderParamInstances.initialize(MATERIAL_COUNT_MAX);

	u32 meshShaderNameLength = 0;
	u32 pixelShaderNameLength = 0;
	char meshShaderName[FILE_PATH_COUNT_MAX] = {};
	char pixelShaderName[FILE_PATH_COUNT_MAX] = {};

	// �V�F�[�_�[�p�X
	{
		char shaderSetFilePath[FILE_PATH_COUNT_MAX] = {};
		sprintf_s(shaderSetFilePath, "%s%s", RESOURCE_FOLDER_PATH, desc._filePath);

		std::ifstream fin(shaderSetFilePath, std::ios::in | std::ios::binary);
		fin.exceptions(std::ios::badbit);
		LTN_ASSERT(!fin.fail());

		fin.read(reinterpret_cast<char*>(&meshShaderNameLength), sizeof(u32));
		fin.read(reinterpret_cast<char*>(meshShaderName), meshShaderNameLength);
		fin.read(reinterpret_cast<char*>(&pixelShaderNameLength), sizeof(u32));
		fin.read(reinterpret_cast<char*>(pixelShaderName), pixelShaderNameLength);

		fin.close();
	}

	char meshShaderPath[FILE_PATH_COUNT_MAX] = {};
	char pixelShaderPath_[FILE_PATH_COUNT_MAX] = {};
	sprintf_s(meshShaderPath, "%s%s", RESOURCE_FOLDER_PATH, meshShaderName);
	sprintf_s(pixelShaderPath_, "%s%s", RESOURCE_FOLDER_PATH, pixelShaderName);
#if ENABLE_VISIBILITY_BUFFER
	const char* pixelShaderPath = "L://LightnEngine//resource//common//shader//visibility_buffer//geometry_pass.pso";
#else
	const char* pixelShaderPath = pixelShaderPath_;
#endif

	// �V�F�[�_�[�p�����[�^�[
	{
		char shaderSetFilePath[FILE_PATH_COUNT_MAX] = {};
		sprintf_s(shaderSetFilePath, "%s%s", RESOURCE_FOLDER_PATH, pixelShaderName);

		// �V�F�[�_�[�p�X���� Info �p�X�ɕϊ�
		char* str = shaderSetFilePath;
		while (*str != '.') {
			str++;
		}

		// .vso -> .vsInfo
		// .pso -> .psinfo
		memcpy(str + 3, "info\0", 6);

		std::ifstream fin(shaderSetFilePath, std::ios::in | std::ios::binary);
		fin.exceptions(std::ios::badbit);
		LTN_ASSERT(!fin.fail());

		u16 parameterSizeInByte = 0;
		u16 parameterCount = 0;
		fin.read(reinterpret_cast<char*>(&parameterCount), sizeof(u16));
		for (u32 parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex) {
			u32& nameHash = _parameterNameHashes[parameterIndex];
			u16& type = _parameterTypes[parameterIndex];
			fin.read(reinterpret_cast<char*>(&nameHash), sizeof(u32));
			fin.read(reinterpret_cast<char*>(&type), sizeof(u16));
			_parameterByteOffset[parameterIndex] = parameterSizeInByte;
			parameterSizeInByte += Material::PARAM_TYPE_SIZE_IN_BYTE[type];
		}

		_parameterSizeInByte = parameterSizeInByte;
		_parameterCount = parameterCount;

		fin.close();
	}

	_parameterDatas = new u8[_parameterSizeInByte * MATERIAL_COUNT_MAX];

	constexpr u32 GLOBAL_SDF_LAYER_COUNT = 4;
	constexpr u32 TEXTURE_BASE_REGISTER = 36;
	constexpr u32 MESH_INSTANCE_INV_BOUNDS_BASE_REGISTER = 164;
	constexpr u32 SDF_GROUP_BASE_REGISTER = MESH_INSTANCE_INV_BOUNDS_BASE_REGISTER + 1;
	Device* device = GraphicsSystemImpl::Get()->getDevice();
	DescriptorRange cullingViewCbvRange(DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	DescriptorRange viewCbvRange(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	DescriptorRange materialDescriptorRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	DescriptorRange meshDescriptorRange(DESCRIPTOR_RANGE_TYPE_SRV, 5, 1);
	DescriptorRange meshInstanceDescriptorRange(DESCRIPTOR_RANGE_TYPE_SRV, 4, 6);
	DescriptorRange meshletInfoSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 10);
	DescriptorRange vertexDescriptorRange(DESCRIPTOR_RANGE_TYPE_SRV, 5, 11);
	DescriptorRange currentLodLevelRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 16);
	DescriptorRange meshletInstancePrimitiveInfoRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 17);
	DescriptorRange meshletInstanceMeshInstanceIndexRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 18);
	DescriptorRange meshletInstanceWorldMatrixRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 19);
	DescriptorRange hizRange(DESCRIPTOR_RANGE_TYPE_SRV, 8, 20);
	DescriptorRange textureDescriptorRange(DESCRIPTOR_RANGE_TYPE_SRV, 128, TEXTURE_BASE_REGISTER);
	DescriptorRange sdfMeshInstanceInvBoundsSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, MESH_INSTANCE_INV_BOUNDS_BASE_REGISTER);
	DescriptorRange sdfMeshInstanceIndexOffsetSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, GLOBAL_SDF_LAYER_COUNT, SDF_GROUP_BASE_REGISTER + GLOBAL_SDF_LAYER_COUNT * 0);
	DescriptorRange sdfMeshInstanceIndicesSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, GLOBAL_SDF_LAYER_COUNT, SDF_GROUP_BASE_REGISTER + GLOBAL_SDF_LAYER_COUNT * 1);
	DescriptorRange sdfMeshInstanceCountSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, GLOBAL_SDF_LAYER_COUNT, SDF_GROUP_BASE_REGISTER + GLOBAL_SDF_LAYER_COUNT * 2);
	DescriptorRange sdfTextureSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 256, SDF_GROUP_BASE_REGISTER + GLOBAL_SDF_LAYER_COUNT * 3);
	DescriptorRange cullingResultDescriptorRange(DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	constexpr u32 ROOT_FRUSTUM_COUNT = DefaultMeshRootParam::ROOT_DEFAULT_MESH_COUNT - 2;
	RootParameter furstumCullingRootParameters[ROOT_FRUSTUM_COUNT] = {};
	RootParameter furstumOcclusionCullingRootParameters[DefaultMeshRootParam::ROOT_DEFAULT_MESH_COUNT] = {};

	RootSignatureDesc rootSignatureDescFurstumCulling = {};
	RootSignatureDesc rootSignatureDescFurstumOcclusionCulling = {};

	// ���b�V�����b�g�@�t���X�^���J�����O�̂�
	{
		RootParameter* rootParameters = furstumCullingRootParameters;
		rootParameters[DefaultMeshRootParam::CULLING_VIEW_CONSTANT].initializeDescriptorTable(1, &cullingViewCbvRange, SHADER_VISIBILITY_AMPLIFICATION);
		rootParameters[DefaultMeshRootParam::VIEW_CONSTANT].initializeDescriptorTable(1, &viewCbvRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::MATERIALS].initializeDescriptorTable(1, &materialDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::MESH].initializeDescriptorTable(1, &meshDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::INDIRECT_CONSTANT].initializeConstant(2, 4, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::MESHLET_INFO].initializeDescriptorTable(1, &meshletInfoSrvRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::VERTEX_RESOURCES].initializeDescriptorTable(1, &vertexDescriptorRange, SHADER_VISIBILITY_MESH);
		rootParameters[DefaultMeshRootParam::TEXTURES].initializeDescriptorTable(1, &textureDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::LOD_LEVEL].initializeDescriptorTable(1, &currentLodLevelRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::MESHLET_PRIMITIVE_INFO].initializeDescriptorTable(1, &meshletInstancePrimitiveInfoRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::MESHLET_MESH_INSTANCE_INDEX].initializeDescriptorTable(1, &meshletInstanceMeshInstanceIndexRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::MESH_INSTANCE_WORLD_MATRIX].initializeDescriptorTable(1, &meshletInstanceWorldMatrixRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::SDF_MESH_INSTANCE_INV_BOUNDS].initializeDescriptorTable(1, &sdfMeshInstanceInvBoundsSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[DefaultMeshRootParam::SDF_MESH_INSTANCE_OFFSET].initializeDescriptorTable(1, &sdfMeshInstanceIndexOffsetSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[DefaultMeshRootParam::SDF_MESH_INSTANCE_INDEX].initializeDescriptorTable(1, &sdfMeshInstanceIndicesSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[DefaultMeshRootParam::SDF_MESH_INSTANCE_COUNT].initializeDescriptorTable(1, &sdfMeshInstanceCountSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[DefaultMeshRootParam::SDF_MESH_TEXTURE].initializeDescriptorTable(1, &sdfTextureSrvRange, SHADER_VISIBILITY_PIXEL);

		rootSignatureDescFurstumCulling._device = device;
		rootSignatureDescFurstumCulling._numParameters = LTN_COUNTOF(furstumCullingRootParameters);
		rootSignatureDescFurstumCulling._parameters = rootParameters;
		memcpy(furstumOcclusionCullingRootParameters, rootParameters, sizeof(RootParameter)*ROOT_FRUSTUM_COUNT);
	}

	// ���b�V�����b�g�@�t���X�^�� + �I�N���[�W�����J�����O
	{
		RootParameter* rootParameters = furstumOcclusionCullingRootParameters;
		rootParameters[DefaultMeshRootParam::CULLING_RESULT].initializeDescriptorTable(1, &cullingResultDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[DefaultMeshRootParam::HIZ].initializeDescriptorTable(1, &hizRange, SHADER_VISIBILITY_ALL);

		rootSignatureDescFurstumOcclusionCulling._device = device;
		rootSignatureDescFurstumOcclusionCulling._numParameters = LTN_COUNTOF(furstumOcclusionCullingRootParameters);
		rootSignatureDescFurstumOcclusionCulling._parameters = rootParameters; 
	}

	PipelineStateSystem* pipelineStateSystem = PipelineStateSystem::Get();
	RenderTargetBlendDesc debugOcclusionBlendDesc = {};
	debugOcclusionBlendDesc._blendEnable = true;
	debugOcclusionBlendDesc._srcBlend = BLEND_SRC_ALPHA;
	debugOcclusionBlendDesc._destBlend = BLEND_INV_SRC_ALPHA;
	debugOcclusionBlendDesc._blendOp = BLEND_OP_ADD;
	debugOcclusionBlendDesc._srcBlendAlpha = BLEND_ONE;
	debugOcclusionBlendDesc._destBlendAlpha = BLEND_ZERO;
	debugOcclusionBlendDesc._blendOpAlpha = BLEND_OP_ADD;

	constexpr char msPrimitiveInstancingFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\standard_mesh\\default_mesh_primitive_instancing.mso";
	constexpr char asMeshletCullingFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\meshlet\\meshlet_culling_pass.aso";
	constexpr char asMeshletCullingFrustumOcclusionFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\meshlet\\meshlet_culling_frustum_occlusion.aso";
	constexpr char asMeshletCullingFrustumFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\meshlet\\meshlet_culling_frustum.aso";
	constexpr char psDebugMeshletFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_meshlet.pso";
	constexpr char psDebugLodFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_lod.pso";
	constexpr char psDebugDepthFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_depth.pso";
	constexpr char psDebugTexCoordFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_texcoords.pso";
	constexpr char psDebugWireFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_wireframe.pso";
	constexpr char psDebugOcclusionFilePath[] = "L:\\LightnEngine\\resource\\common\\shader\\debug\\debug_occlusion_culling.pso";

#if ENABLE_VISIBILITY_BUFFER
	Format rtvFormats[2];
	rtvFormats[0] = FORMAT_R32G32_UINT;
	rtvFormats[1] = FORMAT_R8_UINT;
#else
	Format rtvFormats[1];
	rtvFormats[0] = FORMAT_R8G8B8A8_UNORM;
#endif

	MeshShaderPipelineStateGroupDesc sharedMeshShaderPipelineStateDesc = {};
	sharedMeshShaderPipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;
	sharedMeshShaderPipelineStateDesc._rtvCount = LTN_COUNTOF(rtvFormats);
	sharedMeshShaderPipelineStateDesc._rtvFormats = rtvFormats;

	// ���b�V���V�F�[�_�[�̂�
	{
		MeshShaderPipelineStateGroupDesc pipelineStateDesc = sharedMeshShaderPipelineStateDesc;
		RootSignatureDesc rootSignatureDesc = rootSignatureDescFurstumCulling;
		rootSignatureDesc._parameters = furstumCullingRootParameters;

		// Depth Only
		pipelineStateDesc._meshShaderFilePath = msPrimitiveInstancingFilePath;
		*implDesc._primDepthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDesc);

		// �t���X�^�� �{ �I�N���[�W�����J�����O
		pipelineStateDesc._pixelShaderFilePath = pixelShaderPath;
		*implDesc._primPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDesc);

		// ���b�V�����b�g�f�o�b�O�p
		pipelineStateDesc._pixelShaderFilePath = psDebugMeshletFilePath;
		*implDesc._debugPrimMeshletPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDesc);

		// LodLevel �f�o�b�O�p
		pipelineStateDesc._pixelShaderFilePath = psDebugLodFilePath;
		*implDesc._debugPrimLodLevelPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDesc);

		// Depth �f�o�b�O�p
		pipelineStateDesc._pixelShaderFilePath = psDebugDepthFilePath;
		*implDesc._debugPrimDepthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDesc);

		// TexCoords �f�o�b�O�p
		pipelineStateDesc._pixelShaderFilePath = psDebugTexCoordFilePath;
		*implDesc._debugPrimTexcoordsPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDesc);

		// ���C���[�t���[���\��
		{
			MeshShaderPipelineStateGroupDesc desc = pipelineStateDesc;
			desc._fillMode = FILL_MODE_WIREFRAME;
			desc._pixelShaderFilePath = psDebugWireFilePath;
			*implDesc._debugPrimWireFramePipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(desc, rootSignatureDesc);
		}

		// �I�N���[�W�����J�����O����
#if ENABLE_VISIBILITY_BUFFER
		* implDesc._debugOcclusionPipelineStateGroup = nullptr;
#else
		pipelineStateDesc._pixelShaderFilePath = psDebugOcclusionFilePath;
		pipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_ALWAYS;
		pipelineStateDesc._blendDesc._renderTarget[0] = debugOcclusionBlendDesc;
		*implDesc._debugPrimOcclusionPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDesc);
#endif
	}

	// ���b�V���V�F�[�_�[ + �����V�F�[�_�[
	{
		MeshShaderPipelineStateGroupDesc pipelineStateDesc = sharedMeshShaderPipelineStateDesc;
		pipelineStateDesc._meshShaderFilePath = meshShaderPath;

		// GPU �J�����O�����f�o�b�O�p
		pipelineStateDesc._amplificationShaderFilePath = asMeshletCullingFilePath;
		pipelineStateDesc._pixelShaderFilePath = pixelShaderPath;
		*implDesc._debugCullingPassPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

		// �t���X�^�� �{ �I�N���[�W�����J�����O
		pipelineStateDesc._amplificationShaderFilePath = asMeshletCullingFrustumOcclusionFilePath;
		*implDesc._pipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

		// ���b�V�����b�g�f�o�b�O�p
		pipelineStateDesc._pixelShaderFilePath = psDebugMeshletFilePath;
		*implDesc._debugMeshletPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

		// LodLevel �f�o�b�O�p
		pipelineStateDesc._pixelShaderFilePath = psDebugLodFilePath;
		*implDesc._debugLodLevelPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

		// Depth �f�o�b�O�p
		pipelineStateDesc._pixelShaderFilePath = psDebugDepthFilePath;
		*implDesc._debugDepthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

		// TexCoords �f�o�b�O�p
		pipelineStateDesc._pixelShaderFilePath = psDebugTexCoordFilePath;
		*implDesc._debugTexcoordsPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

		// ���C���[�t���[���\��
		{
			MeshShaderPipelineStateGroupDesc desc = pipelineStateDesc;
			desc._fillMode = FILL_MODE_WIREFRAME;
			desc._pixelShaderFilePath = psDebugWireFilePath;
			*implDesc._debugWireFramePipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(desc, rootSignatureDescFurstumOcclusionCulling);
		}

		// �f�v�X�I�����[ (�t���X�^���J�����O�̂�)
		pipelineStateDesc._amplificationShaderFilePath = asMeshletCullingFrustumFilePath;
		pipelineStateDesc._pixelShaderFilePath = nullptr;
		*implDesc._depthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);

		// �I�N���[�W�����J�����O����
#if ENABLE_VISIBILITY_BUFFER
			* implDesc._debugOcclusionPipelineStateGroup = nullptr;
#else
		pipelineStateDesc._pixelShaderFilePath = psDebugOcclusionFilePath;
		pipelineStateDesc._depthComparisonFunc = COMPARISON_FUNC_ALWAYS;
		pipelineStateDesc._blendDesc._renderTarget[0] = debugOcclusionBlendDesc;
		*implDesc._debugOcclusionPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(pipelineStateDesc, rootSignatureDescFurstumOcclusionCulling);
#endif
	}

#if ENABLE_VISIBILITY_BUFFER
	// shading
	{
		DescriptorRange constantCbvRange(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		DescriptorRange shaderRangeSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		DescriptorRange triangleAttributeSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 29);
		DescriptorRange primitiveIndicesSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 30);
		DescriptorRange vertexPositionSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 31);
		DescriptorRange meshInstanceWorldMatricesSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 32);
		DescriptorRange meshesSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 2, 33);
		DescriptorRange currentLodLevelSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 35);

		RootParameter rootParameters[ShadingRootParam::COUNT] = {};
		rootParameters[ShadingRootParam::CONSTANT].initializeDescriptorTable(1, &constantCbvRange, SHADER_VISIBILITY_VERTEX);
		rootParameters[ShadingRootParam::ROOT_CONSTANT].initializeConstant(1, 1, SHADER_VISIBILITY_VERTEX);
		rootParameters[ShadingRootParam::SHADER_RANGE].initializeDescriptorTable(1, &shaderRangeSrvRange, SHADER_VISIBILITY_VERTEX);

		rootParameters[ShadingRootParam::VIEW_CONSTANT].initializeDescriptorTable(1, &viewCbvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ShadingRootParam::MATERIALS].initializeDescriptorTable(1, &materialDescriptorRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ShadingRootParam::MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceDescriptorRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ShadingRootParam::TEXTURES].initializeDescriptorTable(1, &textureDescriptorRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ShadingRootParam::TRIANGLE_ATTRIBUTE].initializeDescriptorTable(1, &triangleAttributeSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ShadingRootParam::PRIMITIVE_INDICES].initializeDescriptorTable(1, &primitiveIndicesSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ShadingRootParam::VERTEX_POSITION].initializeDescriptorTable(1, &vertexPositionSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ShadingRootParam::MESH_INSTANCE_WORLD_MATRICES].initializeDescriptorTable(1, &meshInstanceWorldMatricesSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ShadingRootParam::MESHES].initializeDescriptorTable(1, &meshesSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ShadingRootParam::LOD_LEVELS].initializeDescriptorTable(1, &currentLodLevelSrvRange, SHADER_VISIBILITY_PIXEL);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;

		Format rtvFormat = FORMAT_R8G8B8A8_UNORM;
		ClassicPipelineStateGroupDesc desc = {};
		desc._vertexShaderFilePath = "L:\\LightnEngine\\resource\\common\\shader\\visibility_buffer\\shading_quad.vso";
		desc._pixelShaderFilePath = pixelShaderPath_;
		desc._depthComparisonFunc = COMPARISON_FUNC_EQUAL;
		desc._rtvCount = 1;
		desc._rtvFormats = &rtvFormat;
		desc._dsvFormat = FORMAT_D16_UNORM;
		desc._depthWriteMask = DEPTH_WRITE_MASK_ZERO;
		*implDesc._shadingPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(desc, rootSignatureDesc);
	}
#endif

	// classic
	{
		DescriptorRange cbvDescriptorRange(DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		DescriptorRange meshInstanceWordlMatrixSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		DescriptorRange materialDescriptorRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		DescriptorRange meshInstanceSrvRange(DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);

		RootParameter rootParameters[ClassicMeshRootParam::COUNT] = {};
		rootParameters[ClassicMeshRootParam::SCENE_CONSTANT].initializeDescriptorTable(1, &cbvDescriptorRange, SHADER_VISIBILITY_ALL);
		rootParameters[ClassicMeshRootParam::MESH_INFO].initializeConstant(1, 3, SHADER_VISIBILITY_VERTEX);
		rootParameters[ClassicMeshRootParam::MATERIALS].initializeDescriptorTable(1, &materialDescriptorRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ClassicMeshRootParam::MESH_INSTANCE].initializeDescriptorTable(1, &meshInstanceSrvRange, SHADER_VISIBILITY_ALL);
		rootParameters[ClassicMeshRootParam::MESH_INSTANCE_WORLD_MATRIX].initializeDescriptorTable(1, &meshInstanceWordlMatrixSrvRange, SHADER_VISIBILITY_VERTEX);
		rootParameters[ClassicMeshRootParam::TEXTURES].initializeDescriptorTable(1, &textureDescriptorRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ClassicMeshRootParam::SDF_MESH_INSTANCE_INV_BOUNDS].initializeDescriptorTable(1, &sdfMeshInstanceInvBoundsSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ClassicMeshRootParam::SDF_MESH_INSTANCE_OFFSET].initializeDescriptorTable(1, &sdfMeshInstanceIndexOffsetSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ClassicMeshRootParam::SDF_MESH_INSTANCE_INDEX].initializeDescriptorTable(1, &sdfMeshInstanceIndicesSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ClassicMeshRootParam::SDF_MESH_INSTANCE_COUNT].initializeDescriptorTable(1, &sdfMeshInstanceCountSrvRange, SHADER_VISIBILITY_PIXEL);
		rootParameters[ClassicMeshRootParam::SDF_MESH_TEXTURE].initializeDescriptorTable(1, &sdfTextureSrvRange, SHADER_VISIBILITY_PIXEL);

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc._device = device;
		rootSignatureDesc._numParameters = LTN_COUNTOF(rootParameters);
		rootSignatureDesc._parameters = rootParameters;
		rootSignatureDesc._flags = ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		char vertexShaderName[FILE_PATH_COUNT_MAX] = {};
		memcpy(vertexShaderName, meshShaderName, meshShaderNameLength - 3);
		memcpy(vertexShaderName + meshShaderNameLength - 3, "vso", 3);

		char vertexShaderPath[FILE_PATH_COUNT_MAX] = {};
		sprintf_s(vertexShaderPath, "%s%s", RESOURCE_FOLDER_PATH, vertexShaderName);

		InputElementDesc inputElements[2] = {};
		inputElements[0]._inputSlot = 0;
		inputElements[0]._format = FORMAT_R32G32B32_FLOAT;
		inputElements[0]._semanticName = "POSITION";

		inputElements[1]._inputSlot = 1;
		inputElements[1]._format = FORMAT_R32_UINT;
		inputElements[1]._semanticName = "PACKED_TEX";

		ClassicPipelineStateGroupDesc desc = {};
		desc._rtvCount = LTN_COUNTOF(rtvFormats);
		desc._rtvFormats = rtvFormats;
		desc._vertexShaderFilePath = vertexShaderPath;
		desc._pixelShaderFilePath = pixelShaderPath;
		desc._inputElements = inputElements;
		desc._inputElementCount = LTN_COUNTOF(inputElements);
		desc._depthComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;
		desc._dsvFormat = FORMAT_D32_FLOAT;
		*implDesc._classicPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(desc, rootSignatureDesc);

		desc._pixelShaderFilePath = nullptr;
		*implDesc._classicDepthPipelineStateGroup = pipelineStateSystem->createPipelineStateGroup(desc, rootSignatureDesc);
	}

	// �R�}���h�V�O�l�`��
	{
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();
		*implDesc._commandSignature = allocator->allocateCommandSignature();
		*implDesc._msCommandSignature = allocator->allocateCommandSignature();

		IndirectArgumentDesc argumentDescs[2] = {};
		argumentDescs[0]._type = INDIRECT_ARGUMENT_TYPE_CONSTANT;
		argumentDescs[0].Constant._num32BitValuesToSet = 4;
		argumentDescs[0].Constant._rootParameterIndex = DefaultMeshRootParam::INDIRECT_CONSTANT;
		argumentDescs[1]._type = INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

		CommandSignatureDesc desc = {};
		desc._device = device;
		desc._byteStride = sizeof(gpu::DispatchMeshIndirectArgument);

		desc._argumentDescs = argumentDescs;
		desc._numArgumentDescs = LTN_COUNTOF(argumentDescs);
		desc._rootSignature = (*implDesc._pipelineStateGroup)->getRootSignature();
		(*implDesc._commandSignature)->initialize(desc);

		desc._rootSignature = (*implDesc._primPipelineStateGroup)->getRootSignature();
		(*implDesc._msCommandSignature)->initialize(desc);
	}

#if ENABLE_MULTI_INDIRECT_DRAW
	{
		GraphicsApiInstanceAllocator* allocator = GraphicsApiInstanceAllocator::Get();

		IndirectArgumentDesc argumentDescs[2] = {};
		argumentDescs[0]._type = INDIRECT_ARGUMENT_TYPE_CONSTANT;
		argumentDescs[0].Constant._rootParameterIndex = ClassicMeshRootParam::MESH_INFO;
		argumentDescs[0].Constant._num32BitValuesToSet = 3;
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
	_shaderParamInstances.terminate();
	delete[] _parameterDatas;
	_parameterDatas = nullptr;
}

void PipelineStateSet::requestDelete(u32 shaderSetIndex) {
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
