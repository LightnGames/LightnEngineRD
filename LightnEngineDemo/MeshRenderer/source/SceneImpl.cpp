#include <MeshRenderer/impl/SceneImpl.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <DebugRenderer/DebugRendererSystem.h>
#include <MaterialSystem/MaterialSystem.h>
#include <MaterialSystem/impl/PipelineStateSystem.h>
#include <GfxCore/impl/ViewSystemImpl.h>
#include <Core/Application.h>

void Scene::initialize() {
	_vramShaderSetSystem.initialize();

	Device* device = GraphicsSystemImpl::Get()->getDevice();
	_gpuMeshInstances.initialize(MESH_INSTANCE_COUNT_MAX);
	_gpuLodMeshInstances.initialize(LOD_MESH_INSTANCE_COUNT_MAX);
	_gpuSubMeshInstances.initialize(SUB_MESH_INSTANCE_COUNT_MAX);

	// buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = MESH_INSTANCE_COUNT_MAX * sizeof(gpu::MeshInstance);
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._device = device;
		_meshInstanceBuffer.initialize(desc);
		_meshInstanceBuffer.setDebugName("Mesh Instance");

		desc._sizeInByte = LOD_MESH_INSTANCE_COUNT_MAX * sizeof(gpu::LodMeshInstance);
		_lodMeshInstanceBuffer.initialize(desc);
		_lodMeshInstanceBuffer.setDebugName("Lod Mesh Instance");

		desc._sizeInByte = SUB_MESH_INSTANCE_COUNT_MAX * sizeof(gpu::SubMeshInstance);
		_subMeshInstanceBuffer.initialize(desc);
		_subMeshInstanceBuffer.setDebugName("Sub Mesh Instance");

		desc._sizeInByte = PACKED_SUB_MESH_COUNT_MAX * VramShaderSetSystem::SHADER_SET_COUNT_MAX * sizeof(u32);
		_packedMeshletOffsetBuffer.initialize(desc);
		_packedMeshletOffsetBuffer.setDebugName("Packed Meshlet Offsets");
	}

	// descriptors
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		_meshInstanceHandles = allocator->allocateDescriptors(3);
		_packedMeshletOffsetHandle = allocator->allocateDescriptors(1);

		u64 incrimentSize = static_cast<u64>(allocator->getIncrimentSize());

		ShaderResourceViewDesc desc = {};
		desc._format = FORMAT_UNKNOWN;
		desc._viewDimension = SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._flags = BUFFER_SRV_FLAG_NONE;

		// mesh instance
		{
			CpuDescriptorHandle handle = _meshInstanceHandles._cpuHandle;

			desc._buffer._numElements = MESH_INSTANCE_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::MeshInstance);
			device->createShaderResourceView(_meshInstanceBuffer.getResource(), &desc, handle + incrimentSize * 0);

			desc._buffer._numElements = LOD_MESH_INSTANCE_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::LodMeshInstance);
			device->createShaderResourceView(_lodMeshInstanceBuffer.getResource(), &desc, handle + incrimentSize * 1);

			desc._buffer._numElements = SUB_MESH_INSTANCE_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::SubMeshInstance);
			device->createShaderResourceView(_subMeshInstanceBuffer.getResource(), &desc, handle + incrimentSize * 2);
		}

		// packed meshlet offset
		{
			desc._buffer._numElements = PACKED_SUB_MESH_COUNT_MAX * VramShaderSetSystem::SHADER_SET_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(u32);
			device->createShaderResourceView(_packedMeshletOffsetBuffer.getResource(), &desc, _packedMeshletOffsetHandle._cpuHandle);
		}
	}


	// default shader set
	{
		ShaderSetDesc desc = {};
		desc._filePath = "common/default_mesh.sseto";
		_defaultShaderSet = MaterialSystem::Get()->createShaderSet(desc);
	}

	// default material
	{
		MaterialDesc desc = {};
		desc._filePath = "common/default_mesh.mto";
		_defaultMaterial = MaterialSystem::Get()->createMaterial(desc);
		_vramShaderSetSystem.addRefCountMaterial(_defaultMaterial);
	}
}

void Scene::update() {
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	u32 meshInstanceCount = _gpuMeshInstances.getArrayCountMax();
	for (u32 meshInstanceIndex = 0; meshInstanceIndex < meshInstanceCount; ++meshInstanceIndex) {
		if (_meshInstanceUpdateFlags[meshInstanceIndex] & MESH_INSTANCE_UPDATE_WORLD_MATRIX) {
			updateMeshInstanceBounds(meshInstanceIndex);
		}
	}

	u32 subMeshInstanceCount = _gpuSubMeshInstances.getArrayCountMax();
	for (u32 subMeshInstanceIndex = 0; subMeshInstanceIndex < subMeshInstanceCount; ++subMeshInstanceIndex) {
		if (_subMeshInstanceUpdateFlags[subMeshInstanceIndex] & SUB_MESH_INSTANCE_UPDATE_MATERIAL) {
			SubMeshInstance& subMeshInstance = _subMeshInstances[subMeshInstanceIndex];
			if (subMeshInstance.getPrevMaterial() != nullptr) {
				_vramShaderSetSystem.removeRefCountMaterial(subMeshInstance.getPrevMaterial());
			}

			gpu::SubMeshInstance& gpuSubMeshInstance = _gpuSubMeshInstances[subMeshInstanceIndex];
			Material* material = subMeshInstance.getMaterial();
			_vramShaderSetSystem.addRefCountMaterial(material);
			subMeshInstance.setPrevMaterial(material);
			gpuSubMeshInstance._materialIndex = _vramShaderSetSystem.getIndexVramMaterial(material);
			gpuSubMeshInstance._shaderSetIndex = _vramShaderSetSystem.getShaderSetIndex(material);

			u32 offset = sizeof(gpu::SubMeshInstance) * subMeshInstanceIndex;
			gpu::SubMeshInstance* mapSubMeshInstance = vramUpdater->enqueueUpdate<gpu::SubMeshInstance>(&_subMeshInstanceBuffer, offset);
			*mapSubMeshInstance = gpuSubMeshInstance;
		}
	}

	constexpr u32 ARRAY_COUNT = PACKED_SUB_MESH_COUNT_MAX * VramShaderSetSystem::SHADER_SET_COUNT_MAX;
	u32* packedSubMeshOffsets = vramUpdater->enqueueUpdate<u32>(&_packedMeshletOffsetBuffer, 0, ARRAY_COUNT);
	u32 packedMeshletCounts[ARRAY_COUNT] = {};
	for (u32 meshInstanceIndex = 0; meshInstanceIndex < meshInstanceCount; ++meshInstanceIndex) {
		const Mesh* mesh = _meshInstances[meshInstanceIndex].getMesh();
		const gpu::LodMesh* lodMeshes = mesh->getGpuLodMesh();
		const gpu::SubMesh* subMeshes = mesh->getGpuSubMesh();
		gpu::SubMeshInstance* subMeshInstances = _meshInstances[meshInstanceIndex].getGpuSubMeshInstance(0);
		u32 subMeshCounter = 0;
		u32 lodCount = mesh->getGpuMesh()->_lodMeshCount;
		for (u32 lodIndex = 0; lodIndex < lodCount; ++lodIndex) {
			u32 subMeshCount = lodMeshes[lodIndex]._subMeshCount;
			for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
				u32 meshletCount = subMeshes[subMeshCounter]._meshletCount;
				if (meshletCount < PACKED_SUB_MESH_COUNT_MAX) {
					u32 shaderSetOffset = subMeshInstances[subMeshCounter]._shaderSetIndex * PACKED_SUB_MESH_COUNT_MAX;
					packedMeshletCounts[shaderSetOffset + meshletCount]++;
				}
				++subMeshCounter;
			}
		}
	}

	packedSubMeshOffsets[0] = 0;
	for (u32 packedIndex = 1; packedIndex < ARRAY_COUNT; ++packedIndex) {
		u32 prevIndex = packedIndex - 1;
		packedSubMeshOffsets[packedIndex] = packedSubMeshOffsets[prevIndex] + packedMeshletCounts[prevIndex];
	}

	_vramShaderSetSystem.update();
}

void Scene::processDeletion() {
	_vramShaderSetSystem.processDeletion();
	u32 meshInstanceCount = _gpuMeshInstances.getInstanceCount();
	for (u32 meshInstanceIndex = 0; meshInstanceIndex < meshInstanceCount; ++meshInstanceIndex) {
		if (_meshInstanceStateFlags[meshInstanceIndex] & MESH_INSTANCE_FLAG_REQUEST_DESTROY) {
			deleteMeshInstance(meshInstanceIndex);
			_meshInstanceStateFlags[meshInstanceIndex] = MESH_INSTANCE_FLAG_NONE;
		}
	}

	for (u32 meshInstanceIndex = 0; meshInstanceIndex < meshInstanceCount; ++meshInstanceIndex) {
		if (_meshInstanceUpdateFlags[meshInstanceIndex] & MESH_INSTANCE_UPDATE_WORLD_MATRIX) {
			_meshInstanceUpdateFlags[meshInstanceIndex] &= ~MESH_INSTANCE_UPDATE_WORLD_MATRIX;
		}
	}

	u32 subMeshInstanceCount = _gpuSubMeshInstances.getArrayCountMax();
	for (u32 subMeshInstanceIndex = 0; subMeshInstanceIndex < subMeshInstanceCount; ++subMeshInstanceIndex) {
		if (_subMeshInstanceUpdateFlags[subMeshInstanceIndex] & SUB_MESH_INSTANCE_UPDATE_MATERIAL) {
			_subMeshInstanceUpdateFlags[subMeshInstanceIndex] &= ~SUB_MESH_INSTANCE_UPDATE_MATERIAL;
		}
	}
}

void Scene::terminate() {
	_defaultShaderSet->requestToDelete();
	_defaultMaterial->requestToDelete();
	_vramShaderSetSystem.removeRefCountMaterial(_defaultMaterial);
	_vramShaderSetSystem.terminate();
	_meshInstanceBuffer.terminate();
	_lodMeshInstanceBuffer.terminate();
	_subMeshInstanceBuffer.terminate();
	_packedMeshletOffsetBuffer.terminate();

	LTN_ASSERT(_gpuMeshInstances.getInstanceCount() == 0);
	LTN_ASSERT(_gpuLodMeshInstances.getInstanceCount() == 0);
	LTN_ASSERT(_gpuSubMeshInstances.getInstanceCount() == 0);

	_gpuMeshInstances.terminate();
	_gpuLodMeshInstances.terminate();
	_gpuSubMeshInstances.terminate();

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocator->discardDescriptor(_meshInstanceHandles);
	allocator->discardDescriptor(_packedMeshletOffsetHandle);
}

void Scene::updateMeshInstanceBounds(u32 meshInstanceIndex) {
	MeshInstanceImpl& meshInstance = _meshInstances[meshInstanceIndex];
	gpu::MeshInstance& gpuMeshInstance = _gpuMeshInstances[meshInstanceIndex];
	const MeshInfo* meshInfo = meshInstance.getMesh()->getMeshInfo();
	Matrix4 matrixWorld = meshInstance.getWorldMatrix();
	Vector3 worldScale = matrixWorld.getScale();

	AABB meshInstanceLocalBounds(meshInfo->_boundsMin, meshInfo->_boundsMax);
	AABB meshInstanceBounds = meshInstanceLocalBounds.getTransformedAabb(matrixWorld);
	Vector3 boundsCenter = (meshInstanceBounds._min + meshInstanceBounds._max) / 2.0f;
	f32 boundsRadius = Vector3::length(meshInstanceBounds._max - boundsCenter);
	gpuMeshInstance._aabbMin = meshInstanceBounds._min.getFloat3();
	gpuMeshInstance._aabbMax = meshInstanceBounds._max.getFloat3();
	gpuMeshInstance._stateFlags = 1;
	gpuMeshInstance._boundsRadius = boundsRadius;
	gpuMeshInstance._worldScale = Max(worldScale._x, Max(worldScale._y, worldScale._z));
	gpuMeshInstance._matrixWorld = matrixWorld.transpose();

	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	gpu::MeshInstance* mapMeshInstance = vramUpdater->enqueueUpdate<gpu::MeshInstance>(&_meshInstanceBuffer, sizeof(gpu::MeshInstance) * meshInstanceIndex);
	*mapMeshInstance = gpuMeshInstance;
}

void Scene::deleteMeshInstance(u32 meshInstanceIndex) {
	const MeshInstanceImpl& meshInstanceInfo = _meshInstances[meshInstanceIndex];
	const MeshInfo* meshInfo = meshInstanceInfo.getMesh()->getMeshInfo();

	const gpu::MeshInstance& meshInstance = _gpuMeshInstances[meshInstanceIndex];
	const gpu::LodMeshInstance& lodMeshInstance = _gpuLodMeshInstances[meshInstance._lodMeshInstanceOffset];
	const gpu::SubMeshInstance& subMeshInstance = _gpuSubMeshInstances[lodMeshInstance._subMeshInstanceOffset];
	u32 subMeshCount = meshInfo->_totalSubMeshCount;
	for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
		SubMeshInstance& subMeshInstance = _subMeshInstances[lodMeshInstance._subMeshInstanceOffset + subMeshIndex];
		Material* material = subMeshInstance.getMaterial();
		_vramShaderSetSystem.removeRefCountMaterial(material);
	}
	_gpuSubMeshInstances.discard(&_gpuSubMeshInstances[lodMeshInstance._subMeshInstanceOffset], meshInfo->_totalSubMeshCount);
	_gpuLodMeshInstances.discard(&_gpuLodMeshInstances[meshInstance._lodMeshInstanceOffset], meshInfo->_totalLodMeshCount);
	_gpuMeshInstances.discard(&_gpuMeshInstances[meshInstanceIndex], 1);
}

void Scene::debugDrawMeshletBounds() {
	//for (u32 meshletIndex = 0; meshletIndex < MESHLET_INSTANCE_COUNT_MAX; ++meshletIndex) {
	//	const gpu::MeshletInstance& meshlet = _gpuMeshletInstances[meshletIndex];
	//	if (abs(meshlet._normal._x + meshlet._normal._y + meshlet._normal._z) < 0.5f) {
	//		continue;
	//	}

	//	Vector3 boundsMin = Vector3(meshlet._aabbMin);
	//	Vector3 boundsMax = Vector3(meshlet._aabbMax);
	//	Vector3 boundsCenter = (boundsMin + boundsMax) / 2.0f;
	//	DebugRendererSystem::Get()->drawAabb(boundsMin, boundsMax);
	//	DebugRendererSystem::Get()->drawLine(boundsCenter, boundsCenter + Vector3(meshlet._normal), Color4::GREEN);
	//}
}

void Scene::debugDrawMeshInstanceBounds() {
	for (u32 meshInstanceIndex = 0; meshInstanceIndex < MESH_INSTANCE_COUNT_MAX; ++meshInstanceIndex) {
		if (_meshInstanceStateFlags[meshInstanceIndex] == MESH_INSTANCE_FLAG_NONE) {
			continue;
		}

		const gpu::MeshInstance& meshInstance = _gpuMeshInstances[meshInstanceIndex];
		Vector3 boundsMin = Vector3(meshInstance._aabbMin);
		Vector3 boundsMax = Vector3(meshInstance._aabbMax);
		DebugRendererSystem::Get()->drawAabb(boundsMin, boundsMax, Color4::BLUE);
	}
}

void Scene::debugDrawGui() {
	u32 meshInstanceCount = _gpuMeshInstances.getArrayCountMax();
	DebugGui::Text("Total:%3d", meshInstanceCount);
	DebugGui::Columns(2, "tree", true);
	constexpr f32 MESH_NAME_WIDTH = 320;
	DebugGui::SetColumnWidth(0, MESH_NAME_WIDTH);
	for (u32 x = 0; x < meshInstanceCount; x++) {
		if (_meshInstanceStateFlags[x] == MESH_INSTANCE_FLAG_NONE) {
			DebugGui::TextDisabled("Disabled");
			DebugGui::NextColumn();
			DebugGui::TextDisabled("Disabled");
			DebugGui::NextColumn();
			continue;
		}
		MeshInstanceImpl& meshInstance = _meshInstances[x];
		gpu::MeshInstance& gpuMeshInstance = _gpuMeshInstances[x];
		const MeshInfo* meshInfo = meshInstance.getMesh()->getMeshInfo();
		const SubMeshInfo* subMeshInfos = meshInstance.getMesh()->getSubMeshInfo();
		const DebugMeshInfo* debugMeshInfo = meshInstance.getMesh()->getDebugMeshInfo();
		bool open1 = DebugGui::TreeNode(static_cast<s32>(x), "%-3d: %-20s", x, debugMeshInfo->_filePath);
		Color4 lodCountTextColor = open1 ? Color4::GREEN : Color4::WHITE;
		DebugGui::NextColumn();
		DebugGui::TextColored(lodCountTextColor, "Lod Count:%-2d", meshInfo->_totalLodMeshCount);
		DebugGui::NextColumn();
		if (!open1) {
			continue;
		}

		// mesh info
		DebugGui::Separator();
		u32 subMeshIndex = 0;
		for (u32 y = 0; y < meshInfo->_totalLodMeshCount; y++) {
			bool open2 = DebugGui::TreeNode(static_cast<s32>(y), "Lod %2d", y);
			u32 subMeshCount = meshInstance.getMesh()->getGpuLodMesh(y)->_subMeshCount;
			Color4 subMeshCountTextColor = open2 ? Color4::GREEN : Color4::WHITE;
			DebugGui::NextColumn();
			DebugGui::TextColored(subMeshCountTextColor, "Sub Mesh Count:%2d", subMeshCount);
			DebugGui::NextColumn();
			if (!open2) {
				continue;
			}

			DebugGui::Separator();
			for (u32 z = 0; z < subMeshCount; z++) {
				bool open3 = DebugGui::TreeNode(static_cast<s32>(z), "Sub Mesh %2d", z);
				Color4 meshletCountTextColor = open3 ? Color4::GREEN : Color4::WHITE;
				u32 meshletCount = subMeshInfos[z]._meshletCount;
				DebugGui::NextColumn();
				DebugGui::TextColored(meshletCountTextColor, "Meshlet Count:%2d", meshletCount);
				DebugGui::NextColumn();

				if (!open3) {
					continue;
				}

				DebugGui::Separator();
				for (u32 w = 0; w < meshletCount; w++) {
					DebugGui::Text("Meshlet %2d", w);
				}
				//メッシュレット情報
				DebugGui::TreePop();
			}
			subMeshIndex += subMeshCount;
			DebugGui::TreePop();
		}

		constexpr char boundsFormat[] = "%-14s:[%5.1f][%5.1f][%5.1f]";
		Matrix4 matrixWorld = meshInstance.getWorldMatrix();
		DebugGui::Text(boundsFormat, "World Matrix 0", matrixWorld.m[0][0], matrixWorld.m[0][1], matrixWorld.m[0][2]);
		DebugGui::Text(boundsFormat, "World Matrix 1", matrixWorld.m[1][0], matrixWorld.m[1][1], matrixWorld.m[1][2]);
		DebugGui::Text(boundsFormat, "World Matrix 2", matrixWorld.m[2][0], matrixWorld.m[2][1], matrixWorld.m[2][2]);
		DebugGui::Text(boundsFormat, "World Matrix 3", matrixWorld.m[3][0], matrixWorld.m[3][1], matrixWorld.m[3][2]);

		DebugGui::Text(boundsFormat, "Bounds Min", gpuMeshInstance._aabbMin._x, gpuMeshInstance._aabbMin._y, gpuMeshInstance._aabbMin._z);
		DebugGui::Text(boundsFormat, "Bounds Max", gpuMeshInstance._aabbMax._x, gpuMeshInstance._aabbMax._y, gpuMeshInstance._aabbMax._z);

		DebugGui::TreePop();
	}
	DebugGui::Columns(1);

	DebugGui::EndTabItem();
}

MeshInstance* Scene::createMeshInstance(const Mesh* mesh) {
	const MeshInfo* meshInfo = mesh->getMeshInfo();
	u32 meshInstanceIndex = _gpuMeshInstances.request(1);
	u32 lodMeshInstanceIndex = _gpuLodMeshInstances.request(meshInfo->_totalLodMeshCount);
	u32 subMeshInstanceIndex = _gpuSubMeshInstances.request(meshInfo->_totalSubMeshCount);

	// mesh instance 情報を初期化
	u32 lodMeshCount = meshInfo->_totalLodMeshCount;
	for (u32 lodMeshIndex = 0; lodMeshIndex < lodMeshCount; ++lodMeshIndex) {
		f32 threshhold = 1.0f - ((lodMeshIndex + 1) / static_cast<f32>(lodMeshCount));
		gpu::LodMeshInstance& lodMeshInstance = _gpuLodMeshInstances[lodMeshInstanceIndex + lodMeshIndex];
		lodMeshInstance._subMeshInstanceOffset = subMeshInstanceIndex + meshInfo->_subMeshOffsets[lodMeshIndex];
		lodMeshInstance._threshhold = threshhold;
	}

	u32 subMeshCount = meshInfo->_totalSubMeshCount;
	for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
		_vramShaderSetSystem.addRefCountMaterial(_defaultMaterial);
		const SubMeshInfo* info = mesh->getSubMeshInfo(subMeshIndex);
		const gpu::SubMesh* subMesh = mesh->getGpuSubMesh(subMeshIndex);
		gpu::SubMeshInstance& subMeshInstance = _gpuSubMeshInstances[subMeshInstanceIndex + subMeshIndex];
		subMeshInstance._materialIndex = _vramShaderSetSystem.getIndexVramMaterial(_defaultMaterial);
		subMeshInstance._shaderSetIndex = _vramShaderSetSystem.getShaderSetIndex(_defaultMaterial);
	}

	gpu::MeshInstance& gpuMeshInstance = _gpuMeshInstances[meshInstanceIndex];
	gpuMeshInstance._lodMeshInstanceOffset = lodMeshInstanceIndex;
	gpuMeshInstance._meshIndex = meshInfo->_meshIndex;
	gpuMeshInstance._aabbMin = meshInfo->_boundsMin.getFloat3();
	gpuMeshInstance._aabbMax = meshInfo->_boundsMax.getFloat3();

	// mesh instance 情報をVramにアップロード
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	u32 lodMeshInstanceOffset = sizeof(gpu::LodMeshInstance) * lodMeshInstanceIndex;
	gpu::LodMeshInstance* lodMeshInstance = vramUpdater->enqueueUpdate<gpu::LodMeshInstance>(&_lodMeshInstanceBuffer, lodMeshInstanceOffset, lodMeshCount);
	memcpy(lodMeshInstance, &_gpuLodMeshInstances[lodMeshInstanceIndex], sizeof(gpu::LodMeshInstance) * lodMeshCount);

	u32 subMeshInstanceOffset = sizeof(gpu::SubMeshInstance) * subMeshInstanceIndex;
	gpu::SubMeshInstance* subMeshInstance = vramUpdater->enqueueUpdate<gpu::SubMeshInstance>(&_subMeshInstanceBuffer, subMeshInstanceOffset, subMeshCount);
	memcpy(subMeshInstance, &_gpuSubMeshInstances[subMeshInstanceIndex], sizeof(gpu::SubMeshInstance) * subMeshCount);

	u32 meshInstanceOffset = sizeof(gpu::MeshInstance) * meshInstanceIndex;
	gpu::MeshInstance* mapMeshInstance = vramUpdater->enqueueUpdate<gpu::MeshInstance>(&_meshInstanceBuffer, meshInstanceOffset);
	*mapMeshInstance = gpuMeshInstance;

	MeshInstanceImpl* meshInstance = &_meshInstances[meshInstanceIndex];
	meshInstance->setMesh(mesh);
	meshInstance->setGpuMeshInstance(&_gpuMeshInstances[meshInstanceIndex]);
	meshInstance->setGpuLodMeshInstances(&_gpuLodMeshInstances[lodMeshInstanceIndex]);
	meshInstance->setGpuSubMeshInstances(&_gpuSubMeshInstances[subMeshInstanceIndex]);
	meshInstance->setSubMeshInstance(&_subMeshInstances[subMeshInstanceIndex]);
	meshInstance->setUpdateFlags(&_meshInstanceUpdateFlags[meshInstanceIndex]);
	meshInstance->setStateFlags(&_meshInstanceStateFlags[meshInstanceIndex]);
	meshInstance->setWorldMatrix(Matrix4::Identity);
	meshInstance->setEnabled();

	for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
		u32 index = subMeshInstanceIndex + subMeshIndex;
		SubMeshInstanceImpl& subMeshInstance = _subMeshInstances[index];
		subMeshInstance.setUpdateFlags(&_subMeshInstanceUpdateFlags[index]);
		subMeshInstance.setMaterial(_defaultMaterial);
		subMeshInstance.setPrevMaterial(_defaultMaterial);
	}

	return meshInstance;
}

u32 Scene::getSubMeshInstanceRefCount(const PipelineStateGroup* pipelineState) {
	u32 index = PipelineStateSystem::Get()->getGroupIndex(pipelineState);
	return _vramShaderSetSystem.getMaterialInstanceTotalRefCount(index);
}

void MeshInstance::requestToDelete(){
	*_stateFlags |= MESH_INSTANCE_FLAG_REQUEST_DESTROY;
}

void MeshInstance::setWorldMatrix(const Matrix4 & matrixWorld){
	_matrixWorld = matrixWorld;
	*_updateFlags |= MESH_INSTANCE_UPDATE_WORLD_MATRIX;
}

void MeshInstance::setMaterialSlotIndex(Material * material, u32 slotIndex){
	const gpu::SubMesh* subMeshes = _mesh->getGpuSubMesh();
	u32 subMeshCount = _mesh->getMeshInfo()->_totalSubMeshCount;
	for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
		if (subMeshes[subMeshIndex]._materialSlotIndex == slotIndex) {
			_subMeshInstances[subMeshIndex].setMaterial(material);
		}
	}
}

void MeshInstance::setMaterial(Material * material, u64 slotNameHash){
	u32 slotIndex = getMaterialSlotIndex(slotNameHash);
	LTN_ASSERT(slotIndex != static_cast<u32>(-1));
	setMaterialSlotIndex(material, slotIndex);
}

u32 MeshInstance::getMaterialSlotIndex(u64 slotNameHash) const {
	const u32 materialSlotCount = getMesh()->getMeshInfo()->_materialSlotCount;
	const u64* materialSlotNameHashes = getMesh()->getMeshInfo()->_materialSlotNameHashes;
	u32 slotIndex = static_cast<u32>(-1);
	for (u32 materialIndex = 0; materialIndex < materialSlotCount; ++materialIndex) {
		if (materialSlotNameHashes[materialIndex] == slotNameHash) {
			slotIndex = materialIndex;
			break;
		}
	}
	return slotIndex;
}

void SubMeshInstanceImpl::setMaterial(Material* material) {
	if (_material == material) {
		return;
	}

	_material = material;
	*_updateFlags |= SUB_MESH_INSTANCE_UPDATE_MATERIAL;
}

void SubMeshInstanceImpl::setDefaultMaterial(Material* material) {
	_material = material;
}

void GraphicsView::initialize(const ViewInfo* viewInfo) {
	_viewInfo = viewInfo;
	Device* device = GraphicsSystemImpl::Get()->getDevice();
	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	DescriptorHeapAllocator* cpuAllocator = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();

	// indirect argument buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(gpu::DispatchMeshIndirectArgument) * INDIRECT_ARGUMENT_COUNT_MAX;
		desc._initialState = RESOURCE_STATE_INDIRECT_ARGUMENT;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._device = device;
		_indirectArgumentBuffer.initialize(desc);
		_indirectArgumentBuffer.setDebugName("Indirect Argument");

		desc._sizeInByte = sizeof(u32) * INDIRECT_ARGUMENT_COUNT_MAX;
		_countBuffer.initialize(desc);
		_countBuffer.setDebugName("Indirect Argument Count");

		desc._initialState = RESOURCE_STATE_UNORDERED_ACCESS;
		desc._sizeInByte = Scene::SUB_MESH_INSTANCE_COUNT_MAX * sizeof(gpu::PackedMeshletInfo);
		_packedMeshletBuffer.initialize(desc);
		_packedMeshletBuffer.setDebugName("Packed Meshlets");

		desc._sizeInByte = Scene::PACKED_SUB_MESH_COUNT_MAX * VramShaderSetSystem::SHADER_SET_COUNT_MAX * sizeof(u32);
		_packedMeshletCountBuffer.initialize(desc);
		_packedMeshletCountBuffer.setDebugName("Packed Meshlet Counts");
	}

	// srv buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(gpu::BatchedMeshletInfo) * INDIRECT_ARGUMENT_COUNT_MAX;
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._device = device;
		_batchedMeshletInfoBuffer.initialize(desc);
		_batchedMeshletInfoBuffer.setDebugName("Passed Meshlet Infos");
	}

	// culling result buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(gpu::CullingResult);
		desc._initialState = RESOURCE_STATE_UNORDERED_ACCESS;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._device = device;
		_cullingResultBuffer.initialize(desc);
		_cullingResultBuffer.setDebugName("Culling Result");
	}

	// current lod level buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(u32) * Scene::MESH_INSTANCE_COUNT_MAX;
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._device = device;
		_currentLodLevelBuffer.initialize(desc);
		_currentLodLevelBuffer.setDebugName("Current Lod Levels");
	}

	// culling result readback buffers
	for (u32 frameIndex = 0; frameIndex < BACK_BUFFER_COUNT; ++frameIndex) {
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(gpu::CullingResult);
		desc._initialState = RESOURCE_STATE_COPY_DEST;
		desc._heapType = HEAP_TYPE_READBACK;
		desc._device = device;
		_cullingResultReadbackBuffer[frameIndex].initialize(desc);
		_cullingResultReadbackBuffer[frameIndex].setDebugName("Culling Result Readback");
	}

	// constant buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = GetConstantBufferAligned(sizeof(CullingViewInfo));
		desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._device = device;
		_cullingViewConstantBuffer.initialize(desc);
		_cullingViewConstantBuffer.setDebugName("Culling View Constant");
	}

	// cbv descriptors
	{
		_cullingViewInfoCbvHandle = allocator->allocateDescriptors(1);
		device->createConstantBufferView(_cullingViewConstantBuffer.getConstantBufferViewDesc(), _cullingViewInfoCbvHandle._cpuHandle);
	}

	// uav descriptors
	{
		_indirectArgumentUavHandle = allocator->allocateDescriptors(5);
		u64 incrimentSize = static_cast<u64>(allocator->getIncrimentSize());
		CpuDescriptorHandle indirectArgumentHandle = _indirectArgumentUavHandle._cpuHandle;
		CpuDescriptorHandle countHandle = indirectArgumentHandle + incrimentSize;
		CpuDescriptorHandle meshletInfoHandle = indirectArgumentHandle + incrimentSize * 2;
		CpuDescriptorHandle packedMeshletHandle = indirectArgumentHandle + incrimentSize * 3;
		CpuDescriptorHandle packedMeshletCountHandle = indirectArgumentHandle + incrimentSize * 4;

		UnorderedAccessViewDesc desc = {};
		desc._format = FORMAT_UNKNOWN;
		desc._viewDimension = UAV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._numElements = INDIRECT_ARGUMENT_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(gpu::DispatchMeshIndirectArgument);
		device->createUnorderedAccessView(_indirectArgumentBuffer.getResource(), nullptr, &desc, indirectArgumentHandle);

		desc._buffer._structureByteStride = sizeof(gpu::BatchedMeshletInfo);
		device->createUnorderedAccessView(_batchedMeshletInfoBuffer.getResource(), nullptr, &desc, meshletInfoHandle);

		desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
		desc._buffer._structureByteStride = 0;
		desc._format = FORMAT_R32_TYPELESS;
		device->createUnorderedAccessView(_countBuffer.getResource(), nullptr, &desc, countHandle);

		// カウントバッファをAPIの機能でクリアするためにCPU Only Descriptorを作成
		_countCpuUavHandle = cpuAllocator->allocateDescriptors(1);
		device->createUnorderedAccessView(_countBuffer.getResource(), nullptr, &desc, _countCpuUavHandle._cpuHandle);

		desc._format = FORMAT_UNKNOWN;
		desc._buffer._numElements = Scene::SUB_MESH_INSTANCE_COUNT_MAX;
		desc._buffer._flags = BUFFER_UAV_FLAG_NONE;
		desc._buffer._structureByteStride = sizeof(gpu::PackedMeshletInfo);
		device->createUnorderedAccessView(_packedMeshletBuffer.getResource(), nullptr, &desc, packedMeshletHandle);

		desc._format = FORMAT_R32_TYPELESS;
		desc._buffer._numElements = Scene::PACKED_SUB_MESH_COUNT_MAX * VramShaderSetSystem::SHADER_SET_COUNT_MAX;
		desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
		desc._buffer._structureByteStride = 0;
		device->createUnorderedAccessView(_packedMeshletCountBuffer.getResource(), nullptr, &desc, packedMeshletCountHandle);

		_packedMeshletCountCpuHandle = cpuAllocator->allocateDescriptors(1);
		device->createUnorderedAccessView(_packedMeshletCountBuffer.getResource(), nullptr, &desc, _packedMeshletCountCpuHandle._cpuHandle);
	}

	// culling result uav descriptors
	{
		_cullingResultUavHandle = allocator->allocateDescriptors(1);
		_cullingResultCpuUavHandle = cpuAllocator->allocateDescriptors(1);

		UnorderedAccessViewDesc desc = {};
		desc._format = FORMAT_R32_TYPELESS;
		desc._viewDimension = UAV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._numElements = sizeof(gpu::CullingResult) / sizeof(u32);
		desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
		device->createUnorderedAccessView(_cullingResultBuffer.getResource(), nullptr, &desc, _cullingResultUavHandle._cpuHandle);
		device->createUnorderedAccessView(_cullingResultBuffer.getResource(), nullptr, &desc, _cullingResultCpuUavHandle._cpuHandle);
	}

	// current lod level uav descriptors
	{
		_currentLodLevelUav = allocator->allocateDescriptors(1);

		UnorderedAccessViewDesc desc = {};
		desc._format = FORMAT_R32_TYPELESS;
		desc._viewDimension = UAV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._numElements = Scene::MESH_INSTANCE_COUNT_MAX;
		desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
		device->createUnorderedAccessView(_currentLodLevelBuffer.getResource(), nullptr, &desc, _currentLodLevelUav._cpuHandle);
	}

	// current lod level srv descriptors
	{
		_currentLodLevelSrv = allocator->allocateDescriptors(1);
		ShaderResourceViewDesc desc = {};
		desc._format = FORMAT_R32_TYPELESS;
		desc._viewDimension = SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._numElements = Scene::MESH_INSTANCE_COUNT_MAX;
		desc._buffer._flags = BUFFER_SRV_FLAG_RAW;
		device->createShaderResourceView(_currentLodLevelBuffer.getResource(), &desc, _currentLodLevelSrv._cpuHandle);
	}

	// packed meshlets
	{
		_packedMeshletSrv = allocator->allocateDescriptors(1);
		ShaderResourceViewDesc desc = {};
		desc._format = FORMAT_UNKNOWN;
		desc._viewDimension = SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._numElements = Scene::SUB_MESH_INSTANCE_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(gpu::PackedMeshletInfo);
		device->createShaderResourceView(_packedMeshletBuffer.getResource(), &desc, _packedMeshletSrv._cpuHandle);
	}

	// build hiz textures
	{
		_hizDepthTextureUav = allocator->allocateDescriptors(HIERACHICAL_DEPTH_COUNT);
		_hizDepthTextureSrv = allocator->allocateDescriptors(HIERACHICAL_DEPTH_COUNT);

		u32 incrimentSize = allocator->getIncrimentSize();
		Application* app = ApplicationSystem::Get()->getApplication();
		u32 powScale = 1;
		for (u32 i = 0; i < HIERACHICAL_DEPTH_COUNT; ++i) {
			powScale *= 2;
			u32 screenWidth = app->getScreenWidth() / powScale;
			u32 screenHeight = app->getScreenHeight() / powScale;
			screenWidth += screenWidth % 2;
			screenHeight += screenHeight % 2;

			GpuTextureDesc desc = {};
			desc._device = device;
			desc._format = FORMAT_R32_UINT;
			desc._width = screenWidth;
			desc._height = screenHeight;
			desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			desc._initialState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			_hizDepthTextures[i].initialize(desc);

			char debugName[64];
			sprintf_s(debugName, "Hiz [%-2d] %-4d x %-4d", i, screenWidth, screenHeight);
			_hizDepthTextures[i].setDebugName(debugName);
		}

		for (u32 i = 0; i < HIERACHICAL_DEPTH_COUNT; ++i) {
			CpuDescriptorHandle uavHandle = _hizDepthTextureUav._cpuHandle + static_cast<u64>(incrimentSize) * i;
			UnorderedAccessViewDesc uavDesc = {};
			uavDesc._format = FORMAT_R32_UINT;
			uavDesc._viewDimension = UAV_DIMENSION_TEXTURE2D;
			uavDesc._texture2D._mipSlice = 0;
			uavDesc._texture2D._planeSlice = 0;
			device->createUnorderedAccessView(_hizDepthTextures[i].getResource(), nullptr, &uavDesc, uavHandle);

			CpuDescriptorHandle srvHandle = _hizDepthTextureSrv._cpuHandle + static_cast<u64>(incrimentSize) * i;
			device->createShaderResourceView(_hizDepthTextures[i].getResource(), nullptr, srvHandle);
		}
	}

	// build hiz constant buffers
	{
		GpuBuffer& buffer = _hizInfoConstantBuffer[0];
		GpuBufferDesc desc = {};
		desc._sizeInByte = GetConstantBufferAligned(sizeof(HizInfoConstant));
		desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._device = device;
		buffer.initialize(desc);
		buffer.setDebugName("Hiz Info Constant 0");

		Application* app = ApplicationSystem::Get()->getApplication();
		VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
		HizInfoConstant* mapConstant = vramUpdater->enqueueUpdate<HizInfoConstant>(&buffer, 0);
		mapConstant->_inputDepthWidth = app->getScreenWidth();
		mapConstant->_inputDepthHeight = app->getScreenHeight();
	}

	{
		GpuBuffer& buffer = _hizInfoConstantBuffer[1];
		GpuBufferDesc desc = {};
		desc._sizeInByte = GetConstantBufferAligned(sizeof(HizInfoConstant));
		desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._device = device;
		buffer.initialize(desc);
		buffer.setDebugName("Hiz Info Constant 1");

		ResourceDesc hizLevel3Desc = getHizTextureResourceDesc(3);
		VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
		HizInfoConstant* mapConstant = vramUpdater->enqueueUpdate<HizInfoConstant>(&buffer, 0);
		mapConstant->_inputDepthWidth = static_cast<u32>(hizLevel3Desc._width);
		mapConstant->_inputDepthHeight = static_cast<u32>(hizLevel3Desc._height);
	}

	// build hiz cbv descriptors
	{
		_hizInfoConstantCbv[0] = allocator->allocateDescriptors(1);
		_hizInfoConstantCbv[1] = allocator->allocateDescriptors(1);
		device->createConstantBufferView(_hizInfoConstantBuffer[0].getConstantBufferViewDesc(), _hizInfoConstantCbv[0]._cpuHandle);
		device->createConstantBufferView(_hizInfoConstantBuffer[1].getConstantBufferViewDesc(), _hizInfoConstantCbv[1]._cpuHandle);
	}
}

void GraphicsView::terminate() {
	_indirectArgumentBuffer.terminate();
	_countBuffer.terminate();
	_cullingResultBuffer.terminate();
	_batchedMeshletInfoBuffer.terminate();
	_cullingViewConstantBuffer.terminate();
	_currentLodLevelBuffer.terminate();
	_packedMeshletBuffer.terminate();
	_packedMeshletCountBuffer.terminate();

	for (u32 i = 0; i < LTN_COUNTOF(_hizInfoConstantBuffer); ++i) {
		_hizInfoConstantBuffer[i].terminate();
	}

	for (u32 i = 0; i < HIERACHICAL_DEPTH_COUNT; ++i) {
		_hizDepthTextures[i].terminate();
	}

	for (u32 frameIndex = 0; frameIndex < BACK_BUFFER_COUNT; ++frameIndex) {
		_cullingResultReadbackBuffer[frameIndex].terminate();
	}

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocator->discardDescriptor(_indirectArgumentUavHandle);
	allocator->discardDescriptor(_cullingResultUavHandle);
	allocator->discardDescriptor(_cullingViewInfoCbvHandle);
	allocator->discardDescriptor(_currentLodLevelUav);
	allocator->discardDescriptor(_currentLodLevelSrv);
	for (u32 i = 0; i < LTN_COUNTOF(_hizInfoConstantBuffer); ++i) {
		allocator->discardDescriptor(_hizInfoConstantCbv[i]);
	}
	allocator->discardDescriptor(_hizDepthTextureUav);
	allocator->discardDescriptor(_hizDepthTextureSrv);
	allocator->discardDescriptor(_packedMeshletSrv);

	DescriptorHeapAllocator* cpuAllocater = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();
	cpuAllocater->discardDescriptor(_countCpuUavHandle);
	cpuAllocater->discardDescriptor(_cullingResultCpuUavHandle);
	cpuAllocater->discardDescriptor(_packedMeshletCountCpuHandle);
}

void GraphicsView::update() {
	DescriptorHeapAllocator* descriptorHeapAllocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	u32 incrimentSize = descriptorHeapAllocater->getIncrimentSize();

	ResourceDesc firstDesc = _hizDepthTextures[0].getResourceDesc();
	f32 aspectRate = firstDesc._width / static_cast<f32>(firstDesc._height);

	DebugWindow::StartWindow("Depth Texture");
	DebugGui::Columns(1, nullptr, true);
	for (u32 i = 0; i < HIERACHICAL_DEPTH_COUNT; ++i) {
		ResourceDesc desc = _hizDepthTextures[i].getResourceDesc();
		DebugGui::Text("[%d] width:%-4d height:%-4d", i, desc._width, desc._height);
		DebugGui::Image(_hizDepthTextureSrv._gpuHandle + incrimentSize * i, Vector2(200 * aspectRate, 200),
			Vector2(0, 0), Vector2(1, 1), Color4::WHITE, Color4::BLACK, DebugGui::COLOR_CHANNEL_FILTER_R, Vector2(0.95f, 1), DebugGui::TEXTURE_SAMPLE_TYPE_POINT);
		DebugGui::NextColumn();
	}
	DebugGui::Columns(1);
	DebugWindow::End();

	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	CullingViewInfo* cullingInfo = vramUpdater->enqueueUpdate<CullingViewInfo>(&_cullingViewConstantBuffer, 0);
	*reinterpret_cast<u64*>(cullingInfo->_meshletInfoGpuAddress) = _batchedMeshletInfoBuffer.getGpuVirtualAddress();
}

void GraphicsView::setComputeLodResource(CommandList* commandList) {
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_RESULT_LEVEL, _currentLodLevelUav._gpuHandle);
}

void GraphicsView::setGpuCullingResources(CommandList* commandList) {
	setGpuFrustumCullingResources(commandList);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_CULLING_RESULT, _cullingResultUavHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_HIZ, _hizDepthTextureSrv._gpuHandle);
}

void GraphicsView::setGpuFrustumCullingResources(CommandList * commandList){
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_INDIRECT_ARGUMENTS, _indirectArgumentUavHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_CULLING_VIEW_INFO, _cullingViewInfoCbvHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_LOD_LEVEL, _currentLodLevelSrv._gpuHandle);
}

void GraphicsView::setHizResourcesPass0(CommandList* commandList){
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_HIZ_INFO, _hizInfoConstantCbv[0]._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_HIZ_OUTPUT_DEPTH, _hizDepthTextureUav._gpuHandle); // 0 ~ 3
}

void GraphicsView::setHizResourcesPass1(CommandList* commandList) {
	DescriptorHeapAllocator* descriptorHeapAllocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	u64 incrimentSize = static_cast<u64>(descriptorHeapAllocater->getIncrimentSize());

	commandList->setComputeRootDescriptorTable(ROOT_PARAM_HIZ_INFO, _hizInfoConstantCbv[1]._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_HIZ_OUTPUT_DEPTH, _hizDepthTextureUav._gpuHandle + incrimentSize * 4); // 4 ~ 5
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_HIZ_INPUT_DEPTH, _hizDepthTextureSrv._gpuHandle + incrimentSize * 3); // 3
}

void GraphicsView::resourceBarriersComputeLodToUAV(CommandList* commandList) {
	_currentLodLevelBuffer.transitionResource(commandList, RESOURCE_STATE_UNORDERED_ACCESS);
}

void GraphicsView::resetResourceComputeLodBarriers(CommandList* commandList) {
	_currentLodLevelBuffer.transitionResource(commandList, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void GraphicsView::resourceBarriersGpuCullingToUAV(CommandList* commandList) {
	// Indirect Argument から UAVへ
	ResourceTransitionBarrier indirectArgumentToUavBarriers[3] = {};
	indirectArgumentToUavBarriers[0] = _indirectArgumentBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	indirectArgumentToUavBarriers[1] = _countBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	indirectArgumentToUavBarriers[2] = _batchedMeshletInfoBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarriers(indirectArgumentToUavBarriers, LTN_COUNTOF(indirectArgumentToUavBarriers));
}

void GraphicsView::resourceBarriersHizSrvToUav(CommandList * commandList){
	ResourceTransitionBarrier srvToUav[HIERACHICAL_DEPTH_COUNT] = {};
	for (u32 i = 0; i < HIERACHICAL_DEPTH_COUNT; ++i) {
		srvToUav[i] = _hizDepthTextures[i].getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	}
	commandList->transitionBarriers(srvToUav, LTN_COUNTOF(srvToUav));
}

void GraphicsView::resourceBarriersHizUavtoSrv(CommandList * commandList){
	ResourceTransitionBarrier uavToSrv[HIERACHICAL_DEPTH_COUNT] = {};
	for (u32 i = 0; i < HIERACHICAL_DEPTH_COUNT; ++i) {
		uavToSrv[i] = _hizDepthTextures[i].getAndUpdateTransitionBarrier(RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	commandList->transitionBarriers(uavToSrv, LTN_COUNTOF(uavToSrv));
}

void GraphicsView::resetResourceGpuCullingBarriers(CommandList* commandList) {
	// UAV から Indirect Argumentへ
	ResourceTransitionBarrier uavToIndirectArgumentBarriers[3] = {};
	uavToIndirectArgumentBarriers[0] = _indirectArgumentBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_INDIRECT_ARGUMENT);
	uavToIndirectArgumentBarriers[1] = _countBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_INDIRECT_ARGUMENT);
	uavToIndirectArgumentBarriers[2] = _batchedMeshletInfoBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarriers(uavToIndirectArgumentBarriers, LTN_COUNTOF(uavToIndirectArgumentBarriers));
}

	// カウントバッファクリア
void GraphicsView::resetCountBuffers(CommandList* commandList) {
	u32 clearValues[4] = {};
	DescriptorHeapAllocator* allocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	u32 incrimentSize = allocater->getIncrimentSize();
	// indirect arguments count
	{
		GpuDescriptorHandle gpuDescriptor = _indirectArgumentUavHandle._gpuHandle + incrimentSize;
		CpuDescriptorHandle cpuDescriptor = _countCpuUavHandle._cpuHandle;
		commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, _countBuffer.getResource(), clearValues, 0, nullptr); 
	}

	// packed meshlet count
	{
		GpuDescriptorHandle gpuDescriptor = _indirectArgumentUavHandle._gpuHandle + incrimentSize * 4;
		CpuDescriptorHandle cpuDescriptor = _packedMeshletCountCpuHandle._cpuHandle;
		commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, _packedMeshletCountBuffer.getResource(), clearValues, 0, nullptr);
	}
}

	// カリング結果バッファクリア
void GraphicsView::resetResultBuffers(CommandList* commandList) {
	u32 clearValues[4] = {};
	GpuDescriptorHandle gpuDescriptor = _cullingResultUavHandle._gpuHandle;
	CpuDescriptorHandle cpuDescriptor = _cullingResultCpuUavHandle._cpuHandle;
	commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, _cullingResultBuffer.getResource(), clearValues, 0, nullptr);
}

void GraphicsView::readbackCullingResultBuffer(CommandList* commandList) {
	// カリング結果をリードバックバッファへコピー 
	u32 frameIndex = GraphicsSystemImpl::Get()->getFrameIndex();
	GpuBuffer& currentReadbackBuffer = _cullingResultReadbackBuffer[frameIndex];
	_cullingResultBuffer.transitionResource(commandList, RESOURCE_STATE_COPY_SOURCE);
	commandList->copyBufferRegion(currentReadbackBuffer.getResource(), 0, _cullingResultBuffer.getResource(), 0, sizeof(gpu::CullingResult));
	_cullingResultBuffer.transitionResource(commandList, RESOURCE_STATE_UNORDERED_ACCESS);

	MemoryRange range = { 0, 1 };
	gpu::CullingResult* currentFrameCullingResultMapPtr = currentReadbackBuffer.map<gpu::CullingResult>(&range);
	memcpy(&_currentFrameCullingResultMapPtr, currentFrameCullingResultMapPtr, sizeof(gpu::CullingResult));
	currentReadbackBuffer.unmap();
}

void GraphicsView::setDrawResultDescriptorTable(CommandList* commandList) {
	commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_CULLING_RESULT, _cullingResultUavHandle._gpuHandle);
	commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_HIZ, _hizDepthTextureSrv._gpuHandle);
}

void GraphicsView::setDrawCurrentLodDescriptorTable(CommandList* commandList) {
	commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_LOD_LEVEL, _currentLodLevelSrv._gpuHandle);
	commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_PACKED_MESHLET_INFO, _packedMeshletSrv._gpuHandle);
}

void GraphicsView::render(CommandList* commandList, CommandSignature* commandSignature, u32 commandCountMax, u32 indirectArgumentOffset, u32 countBufferOffset) {
	Resource* indirectArgumentResource = _indirectArgumentBuffer.getResource();
	Resource* countResource = _countBuffer.getResource();
	commandList->executeIndirect(commandSignature, commandCountMax, indirectArgumentResource, indirectArgumentOffset, countResource, countBufferOffset);
}

ResourceDesc GraphicsView::getHizTextureResourceDesc(u32 level) const {
	return _hizDepthTextures[level].getResourceDesc();
}

const CullingResult* GraphicsView::getCullingResult() const {
	return reinterpret_cast<const CullingResult*>(&_currentFrameCullingResultMapPtr);
}
