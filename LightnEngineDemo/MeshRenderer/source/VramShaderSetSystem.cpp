#include <MeshRenderer/impl/VramShaderSetSystem.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <MeshRenderer/GpuStruct.h>
#include <MaterialSystem/impl/PipelineStateSystem.h>
#include <MaterialSystem/impl/MaterialSystemImpl.h>
#include <TextureSystem/impl/TextureSystemImpl.h>

void VramShaderSetSystem::initialize() {
}

void VramShaderSetSystem::update() {
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	MaterialImpl* materials = materialSystem->getMaterial();
	const u8* materialUpdateFlags = materialSystem->getMaterialUpdateFlags();
	const u8* materialStateFlags = materialSystem->getMaterialStateFlags();
	u32 materialCount = materialSystem->getMaterialCount();
	const u8* shaderSetStateFlags = materialSystem->getShaderSetStateFlags();
	u32 shaderSetCount = materialSystem->getShaderSetCount();
	for (u32 shaderSetIndex = 0; shaderSetIndex < shaderSetCount; ++shaderSetIndex) {
		if (shaderSetStateFlags[shaderSetIndex] & SHADER_SET_STATE_FLAG_CREATED) {
			_shaderSets[shaderSetIndex].initialize();
		}
	}

	for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (materialStateFlags[materialIndex] & MATERIAL_STATE_FLAG_CREATED) {
			MaterialImpl* material = &materials[materialIndex];
			u32 shaderSetIndex = materialSystem->getShaderSetIndex(material->getShaderSet());

			VramShaderSet& shaderSet = _shaderSets[shaderSetIndex];
			u32 materialInstanceIndex = shaderSet.findMaterialInstance(material);
			if (materialInstanceIndex == gpu::INVALID_INDEX) {
				materialInstanceIndex = shaderSet.addMaterialInstance(material);

				u32 materialIndex = materialSystem->getMaterialIndex(material);
				MaterialMapKey& mapKey = _materialMapKeys[materialIndex];
				mapKey._materialInstanceIndex = materialInstanceIndex;
				mapKey._vramShaderSetIndex = shaderSetIndex;
			}
		}
	}

	for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (materialUpdateFlags[materialIndex] & MATERIAL_UPDATE_FLAG_UPDATE_PARAMS) {
			const MaterialMapKey& mapKey = _materialMapKeys[materialIndex];
			_shaderSets[mapKey._vramShaderSetIndex].updateMaterialParameter(mapKey._materialInstanceIndex);
		}
	}
}

void VramShaderSetSystem::processDeletion() {
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	MaterialImpl* materials = materialSystem->getMaterial();
	const u8* materialStateFlags = materialSystem->getMaterialStateFlags();
	u32 materialCount = materialSystem->getMaterialCount();
	const u8* shaderSetStateFlags = materialSystem->getShaderSetStateFlags();
	u32 shaderSetCount = materialSystem->getShaderSetCount();
	for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (materialStateFlags[materialIndex] & MATERIAL_STATE_FLAG_REQEST_DESTROY) {
			MaterialImpl* material = &materials[materialIndex];
			const MaterialMapKey& mapKey = _materialMapKeys[materialIndex];
			u32 shaderSetIndex = static_cast<u32>(mapKey._vramShaderSetIndex);
			u32 materialInstanceIndex = static_cast<u32>(mapKey._materialInstanceIndex);
			_shaderSets[shaderSetIndex].removeMaterialInstance(materialInstanceIndex);
		}
	}

	for (u32 shaderSetIndex = 0; shaderSetIndex < shaderSetCount; ++shaderSetIndex) {
		if (shaderSetStateFlags[shaderSetIndex] & SHADER_SET_STATE_FLAG_REQEST_DESTROY) {
			_shaderSets[shaderSetIndex].terminate();
			_shaderSets[shaderSetIndex] = VramShaderSet();
		}
	}
}

void VramShaderSetSystem::terminate() {
}

u32 VramShaderSetSystem::getShaderSetIndex(const Material* material) {
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	return materialSystem->getShaderSetIndex(static_cast<const MaterialImpl*>(material)->getShaderSet());
}

void VramShaderSet::initialize() {
	Device* device = GraphicsSystemImpl::Get()->getDevice();

	// buffers
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._sizeInByte = MATERIAL_INSTANCE_COUNT_MAX * sizeof(MaterialParameter);
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_parameterBuffer.initialize(desc);
		_parameterBuffer.setDebugName("Material Parameters");
	}

	// descriptors
	{
		DescriptorHeapAllocator* allocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		_materialParameterSrv = allocater->allocateDescriptors(1);

		ShaderResourceViewDesc desc = {};
		desc._format = FORMAT_UNKNOWN;
		desc._viewDimension = SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._flags = BUFFER_SRV_FLAG_NONE;
		desc._buffer._numElements = MATERIAL_INSTANCE_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(MaterialParameter);
		device->createShaderResourceView(_parameterBuffer.getResource(), &desc, _materialParameterSrv._cpuHandle);
	}

	_materialInstances.initialize(MATERIAL_INSTANCE_COUNT_MAX);
}

void VramShaderSet::terminate() {
	DescriptorHeapAllocator* allocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocater->discardDescriptor(_materialParameterSrv);
	_parameterBuffer.terminate();

	LTN_ASSERT(_materialInstances.getInstanceCount() == 0);
	_materialInstances.terminate();
}

void VramShaderSet::updateMaterialParameter(u32 materialInstanceIndex) {
	u32 offset = sizeof(MaterialParameter) * materialInstanceIndex;
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	MaterialImpl* material = _materialInstances[materialInstanceIndex];
	TempShaderParam* srcParam = material->getShaderParam();
	MaterialParameter* mapParam = vramUpdater->enqueueUpdate<MaterialParameter>(&_parameterBuffer, offset);
	u32 textureIndex = 0;
	if (srcParam->_albedoTexture != nullptr) {
		textureIndex = TextureSystemImpl::Get()->getTextureIndex(srcParam->_albedoTexture);
	}
	mapParam->_baseColor = srcParam->_color;
	mapParam->_albedoTextureIndex = textureIndex;
}

void VramShaderSet::removeMaterialInstance(u32 materialInstanceIndex) {
	_materialInstances.discard(materialInstanceIndex);
	_materialInstances[materialInstanceIndex] = nullptr;
}

u32 VramShaderSet::addMaterialInstance(MaterialImpl* material) {
	u32 materialInstanceIndex = _materialInstances.request();
	_materialInstances[materialInstanceIndex] = material;
	return materialInstanceIndex;
}

u32 VramShaderSet::findMaterialInstance(Material* material) const {
	u32 materialInstanceCount = _materialInstances.getArrayCountMax();
	u32 findIndex = gpu::INVALID_INDEX;
	for (u32 materialInstanceIndex = 0; materialInstanceIndex < materialInstanceCount; ++materialInstanceIndex) {
		if (_materialInstances[materialInstanceIndex] == material) {
			findIndex = materialInstanceIndex;
			break;
		}
	}
	return findIndex;
}
