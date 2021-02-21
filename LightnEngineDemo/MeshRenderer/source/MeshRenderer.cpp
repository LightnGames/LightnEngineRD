#include <MeshRenderer/impl/MeshRenderer.h>
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
			//_shaderSets[shaderSetIndex].initialize();
		}

		if (shaderSetStateFlags[shaderSetIndex] & SHADER_SET_STATE_FLAG_REQEST_DESTROY) {
			//_shaderSets[shaderSetIndex].terminate();
		}
	}

	for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (materialUpdateFlags[materialIndex] & MATERIAL_UPDATE_FLAG_UPDATE_PARAMS) {
			const MaterialMapKey& mapKey = _materialMapKeys[materialIndex];
			VramShaderSet& shaderSet = _shaderSets[mapKey._vramShaderSetIndex];
			MaterialImpl* material = &materials[materialIndex];
			if (shaderSet._materialInstances[mapKey._materialInstanceIndex] != material) {
				continue;
			}

			u32 offset = sizeof(MaterialParameter) * mapKey._materialInstanceIndex;
			TempShaderParam* srcParam = material->getShaderParam();
			MaterialParameter* mapParam = vramUpdater->enqueueUpdate<MaterialParameter>(&shaderSet._parameterBuffer, offset);
			u32 textureIndex = 0;
			if (srcParam->_albedoTexture != nullptr) {
				textureIndex = TextureSystemImpl::Get()->getTextureIndex(srcParam->_albedoTexture);
			}
			mapParam->_baseColor = srcParam->_color;
			mapParam->_albedoTextureIndex = textureIndex;
		}
	}

	for (u32 materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (materialUpdateFlags[materialIndex] & MATERIAL_STATE_FLAG_CREATED) {
			continue;
		}

		if (materialStateFlags[materialIndex] & MATERIAL_STATE_FLAG_REQEST_DESTROY) {
			continue;
		}
	}
}

void VramShaderSetSystem::processDeletion() {
	//MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	//const u8* shaderSetStateFlags = materialSystem->getShaderSetStateFlags();
	//u32 shaderSetCount = materialSystem->getShaderSetCount();
	//for (u32 shaderSetIndex = 0; shaderSetIndex < shaderSetCount; ++shaderSetIndex) {
	//	if (shaderSetStateFlags[shaderSetIndex] & SHADER_SET_STATE_FLAG_REQEST_DESTROY) {
	//		_shaderSets[shaderSetIndex].terminate();
	//	}
	//}
}

void VramShaderSetSystem::terminate() {
}

u32 VramShaderSetSystem::getMaterialInstanceTotalRefCount(u32 pipelineStateIndex) {
	u32 totalCount = 0;
	const VramShaderSet& shaderSet = _shaderSets[pipelineStateIndex];
	u32 materialInstanceCount = shaderSet._materialInstances.getArrayCountMax();
	for (u32 materialInstanceIndex = 0; materialInstanceIndex < materialInstanceCount; ++materialInstanceIndex) {
		totalCount += shaderSet._materialRefCounts[materialInstanceIndex];
	}
	return totalCount;
}

u32 VramShaderSetSystem::getIndexVramMaterial(const Material* material) {
	u32 shaderSetIndex = getShaderSetIndex(material);

	VramShaderSet& shaderSet = _shaderSets[shaderSetIndex];
	u32 materialInstanceCount = shaderSet._materialInstances.getArrayCountMax();
	for (u32 materialInstanceIndex = 0; materialInstanceIndex < materialInstanceCount; ++materialInstanceIndex) {
		if (shaderSet._materialInstances[materialInstanceIndex] == material) {
			return materialInstanceIndex;
			break;
		}
	}

	return static_cast<u32>(-1);
}

u32 VramShaderSetSystem::getShaderSetIndex(const Material* material) {
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	return materialSystem->getShaderSetIndex(static_cast<const MaterialImpl*>(material)->getShaderSet());
}

void VramShaderSetSystem::addRefCountMaterial(Material* material) {
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	MaterialImpl* materialImpl = static_cast<MaterialImpl*>(material);
	ShaderSetImpl* shaderSetImpl = materialImpl->getShaderSet();
	u32 shaderSetIndex = materialSystem->getShaderSetIndex(shaderSetImpl);

	VramShaderSet& shaderSet = _shaderSets[shaderSetIndex];
	if (getMaterialInstanceTotalRefCount(shaderSetIndex) == 0) {
		shaderSet.initialize();
	}

	u32 materialInstanceCount = shaderSet._materialInstances.getArrayCountMax();
	u32 findIndex = static_cast<u32>(-1);
	for (u32 materialInstanceIndex = 0; materialInstanceIndex < materialInstanceCount; ++materialInstanceIndex) {
		if (shaderSet._materialInstances[materialInstanceIndex] == material) {
			findIndex = materialInstanceIndex;
			break;
		}
	}

	if (findIndex == static_cast<u32>(-1)) {
		findIndex = shaderSet._materialInstances.request();
		shaderSet._materialInstances[findIndex] = material;

		u32 materialIndex = materialSystem->getMaterialIndex(material);
		MaterialMapKey& mapKey = _materialMapKeys[materialIndex];
		mapKey._materialInstanceIndex = findIndex;
		mapKey._vramShaderSetIndex = shaderSetIndex;

		VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
		u32 offset = sizeof(MaterialParameter) * mapKey._materialInstanceIndex;
		TempShaderParam* srcParam = materialImpl->getShaderParam();
		MaterialParameter* mapParam = vramUpdater->enqueueUpdate<MaterialParameter>(&shaderSet._parameterBuffer, offset);
		u32 textureIndex = 0;
		if (srcParam->_albedoTexture != nullptr) {
			textureIndex = TextureSystemImpl::Get()->getTextureIndex(srcParam->_albedoTexture);
		}
		mapParam->_baseColor = srcParam->_color;
		mapParam->_albedoTextureIndex = textureIndex;
	}

	++shaderSet._materialRefCounts[findIndex];
	++shaderSet._totalRefCount;
}

void VramShaderSetSystem::removeRefCountMaterial(const Material* material) {
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	u32 shaderSetIndex = materialSystem->getShaderSetIndex(static_cast<const MaterialImpl*>(material)->getShaderSet());

	VramShaderSet& shaderSet = _shaderSets[shaderSetIndex];
	u32 findIndex = getIndexVramMaterial(material);

	LTN_ASSERT(findIndex != static_cast<u32>(-1));
	--shaderSet._materialRefCounts[findIndex];
	--shaderSet._totalRefCount;

	if (getMaterialInstanceTotalRefCount(shaderSetIndex) == 0) {
		shaderSet.terminate();
		shaderSet = VramShaderSet();

		u32 materialIndex = materialSystem->getMaterialIndex(material);
		_materialMapKeys[materialIndex] = MaterialMapKey();
	}
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
	_materialInstances.terminate();
}