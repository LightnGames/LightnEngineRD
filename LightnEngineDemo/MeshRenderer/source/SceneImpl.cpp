#include <MeshRenderer/impl/SceneImpl.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <DebugRenderer/DebugRendererSystem.h>
#include <MaterialSystem/MaterialSystem.h>
#include <MaterialSystem/impl/PipelineStateSystem.h>
#include <GfxCore/impl/ViewSystemImpl.h>
#include <Core/Application.h>
#include <MaterialSystem/impl/MaterialSystemImpl.h>

void Scene::initialize() {
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

		desc._sizeInByte = MESHLET_INSTANCE_INFO_COUNT_MAX * sizeof(u32);
		_meshletInstanceInfoOffsetBuffer.initialize(desc);
		_meshletInstanceInfoOffsetBuffer.setDebugName("Meshlet Instance Offsets");

		desc._sizeInByte = PRIMITIVE_INSTANCING_INFO_COUNT_MAX * sizeof(u32);
		_primitiveInstancingOffsetBuffer.initialize(desc);
		_primitiveInstancingOffsetBuffer.setDebugName("Primitive Instancing Offsets");

		desc._sizeInByte = gpu::SHADER_SET_COUNT_MAX * sizeof(u32);
		_indirectArgumentOffsetBuffer.initialize(desc);
		_indirectArgumentOffsetBuffer.setDebugName("Indirect Argument Offsets");

#if ENABLE_MULTI_INDIRECT_DRAW
		_multiDrawIndirectArgumentOffsetBuffer.initialize(desc);
		_multiDrawIndirectArgumentOffsetBuffer.setDebugName("Multi Draw Indirect Argument Offsets");
#endif

		desc._sizeInByte = GetConstantBufferAligned(sizeof(SceneCullingInfo));
		desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		_sceneCullingConstantBuffer.initialize(desc);
		_sceneCullingConstantBuffer.setDebugName("Culling Constant");
	}

	// descriptors
	{
		DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
		_meshInstanceHandles = allocator->allocateDescriptors(3);
		_meshletInstanceInfoOffsetSrv = allocator->allocateDescriptors(1);

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

		// meshlet instance offset
		{
			desc._format = FORMAT_R32_TYPELESS;
			desc._buffer._flags = BUFFER_SRV_FLAG_RAW;
			desc._buffer._numElements = MESHLET_INSTANCE_INFO_COUNT_MAX;
			desc._buffer._structureByteStride = 0;
			device->createShaderResourceView(_meshletInstanceInfoOffsetBuffer.getResource(), &desc, _meshletInstanceInfoOffsetSrv._cpuHandle);
		}

		// indirect argument offset
		{
			_indirectArgumentOffsetSrv = allocator->allocateDescriptors(1);

			ShaderResourceViewDesc desc = {};
			desc._format = FORMAT_UNKNOWN;
			desc._viewDimension = SRV_DIMENSION_BUFFER;
			desc._buffer._firstElement = 0;
			desc._buffer._flags = BUFFER_SRV_FLAG_NONE;
			desc._buffer._numElements = gpu::SHADER_SET_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(u32);
			device->createShaderResourceView(_indirectArgumentOffsetBuffer.getResource(), &desc, _indirectArgumentOffsetSrv._cpuHandle);

#if ENABLE_MULTI_INDIRECT_DRAW
			_multiDrawIndirectArgumentOffsetSrv = allocator->allocateDescriptors(1);
			device->createShaderResourceView(_multiDrawIndirectArgumentOffsetBuffer.getResource(), &desc, _multiDrawIndirectArgumentOffsetSrv._cpuHandle);
#endif
		}

		// scene constant
		{
			_cullingSceneConstantHandle = allocator->allocateDescriptors(1);
			device->createConstantBufferView(_sceneCullingConstantBuffer.getConstantBufferViewDesc(), _cullingSceneConstantHandle._cpuHandle);
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
	}
}

void Scene::update() {
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	u32 meshInstanceCount = _gpuMeshInstances.getArrayCountMax();
	for (u32 meshInstanceIndex = 0; meshInstanceIndex < meshInstanceCount; ++meshInstanceIndex) {
		if (_meshInstanceUpdateFlags[meshInstanceIndex] & MESH_INSTANCE_UPDATE_WORLD_MATRIX) {
			updateMeshInstanceBounds(meshInstanceIndex);
		}
	}

	bool isUpdatedInstancingOffset = false;
	u32 subMeshInstanceCount = _gpuSubMeshInstances.getArrayCountMax();
	for (u32 subMeshInstanceIndex = 0; subMeshInstanceIndex < subMeshInstanceCount; ++subMeshInstanceIndex) {
		if (_subMeshInstanceUpdateFlags[subMeshInstanceIndex] & SUB_MESH_INSTANCE_UPDATE_MATERIAL) {
			SubMeshInstance& subMeshInstance = _subMeshInstances[subMeshInstanceIndex];
			Material* material = subMeshInstance.getMaterial();
			subMeshInstance.setPrevMaterial(material);

			gpu::SubMeshInstance& gpuSubMeshInstance = _gpuSubMeshInstances[subMeshInstanceIndex];
			gpuSubMeshInstance._materialIndex = materialSystem->getMaterialIndex(material);
			gpuSubMeshInstance._shaderSetIndex = materialSystem->getShaderSetIndex(static_cast<MaterialImpl*>(material)->getShaderSet());

			u32 offset = sizeof(gpu::SubMeshInstance) * subMeshInstanceIndex;
			gpu::SubMeshInstance* mapSubMeshInstance = vramUpdater->enqueueUpdate<gpu::SubMeshInstance>(&_subMeshInstanceBuffer, offset);
			*mapSubMeshInstance = gpuSubMeshInstance;

			isUpdatedInstancingOffset = true;
		}
	}

	// メッシュレット　インスタンシング情報のオフセットを計算
	if(isUpdatedInstancingOffset) {
		memset(_indirectArgumentCounts, 0, sizeof(u32) * gpu::SHADER_SET_COUNT_MAX);
		memset(_indirectArgumentInstancingCounts, 0, sizeof(u32) * gpu::SHADER_SET_COUNT_MAX);
		memset(_multiDrawIndirectArgumentCounts, 0, sizeof(u32) * gpu::SHADER_SET_COUNT_MAX);
		memset(_indirectArgumentPrimitiveInstancingCounts, 0, sizeof(u32) * gpu::SHADER_SET_COUNT_MAX);

		u32 primitiveInstancingCounts[MESHLET_INSTANCE_INFO_COUNT_MAX] = {};
		u32 meshletInstancingCounts[MESHLET_INSTANCE_INFO_COUNT_MAX] = {};
		for (u32 meshInstanceIndex = 0; meshInstanceIndex < meshInstanceCount; ++meshInstanceIndex) {
			const Mesh* mesh = _meshInstances[meshInstanceIndex].getMesh();
			const MeshInfo* meshInfo = mesh->getMeshInfo();
			const gpu::LodMesh* lodMeshes = mesh->getGpuLodMesh();
			const gpu::SubMesh* subMeshes = mesh->getGpuSubMesh();
			const gpu::Meshlet* meshlets = mesh->getGpuMeshlet();
			const gpu::SubMeshInstance* subMeshInstances = _meshInstances[meshInstanceIndex].getGpuSubMeshInstance(0);
			u32 subMeshStartOffset = meshInfo->_subMeshStartIndex;
			u32 subMeshCount = meshInfo->_totalSubMeshCount;
			for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
				const gpu::SubMesh& subMesh = subMeshes[subMeshIndex];
				u32 meshletCount = subMesh._meshletCount - 1;
				u32 shaderSetIndex = subMeshInstances[subMeshIndex]._shaderSetIndex;
				if (meshletCount < MESHLET_INSTANCE_MESHLET_COUNT_MAX) {
					if (meshletCount == 1) {
						const gpu::Meshlet& meshlet = meshlets[subMesh._meshletOffset];
						u32 threadCount = max(meshlet._primitiveCount, meshlet._vertexCount);
						u32 shaderSetOffset = shaderSetIndex * PRIMITIVE_INSTANCING_PRIMITIVE_COUNT_MAX;
						++primitiveInstancingCounts[shaderSetOffset + threadCount];
					} else {
					}
					u32 shaderSetOffset = shaderSetIndex * MESHLET_INSTANCE_MESHLET_COUNT_MAX;
					++meshletInstancingCounts[shaderSetOffset + meshletCount];
					++_indirectArgumentInstancingCounts[shaderSetIndex];
				} else {
					++_indirectArgumentCounts[shaderSetIndex];
				}
				++_multiDrawIndirectArgumentCounts[shaderSetIndex];
			}
		}

		// primitive instancing offsets
		{
			u32* mapOffsets = vramUpdater->enqueueUpdate<u32>(&_primitiveInstancingOffsetBuffer, 0, PRIMITIVE_INSTANCING_INFO_COUNT_MAX);
			memset(mapOffsets, 0, sizeof(u32) * PRIMITIVE_INSTANCING_INFO_COUNT_MAX);
			for (u32 i = 1; i < PRIMITIVE_INSTANCING_INFO_COUNT_MAX; ++i) {
				u32 prevIndex = i - 1;
				mapOffsets[i] = mapOffsets[prevIndex] + primitiveInstancingCounts[prevIndex];
			}
		}

		// meshlet instancing offsets
		{
			u32* mapOffsets = vramUpdater->enqueueUpdate<u32>(&_meshletInstanceInfoOffsetBuffer, 0, MESHLET_INSTANCE_INFO_COUNT_MAX);
			memset(mapOffsets, 0, sizeof(u32) * MESHLET_INSTANCE_INFO_COUNT_MAX);
			for (u32 i = 1; i < MESHLET_INSTANCE_INFO_COUNT_MAX; ++i) {
				u32 prevIndex = i - 1;
				mapOffsets[i] = mapOffsets[prevIndex] + meshletInstancingCounts[prevIndex];
			}
		}

		// mesh shader indirect argument offset
		{
			for (u32 shaderSetIndex = 1; shaderSetIndex < gpu::SHADER_SET_COUNT_MAX; ++shaderSetIndex) {
				u32 prevShaderIndex = shaderSetIndex - 1;
				_indirectArgumentOffsets[shaderSetIndex] = _indirectArgumentOffsets[prevShaderIndex] + _indirectArgumentCounts[prevShaderIndex];
			}

			u32* mapIndirectArgumentOffsets = vramUpdater->enqueueUpdate<u32>(&_indirectArgumentOffsetBuffer, 0, gpu::SHADER_SET_COUNT_MAX);
			memcpy(mapIndirectArgumentOffsets, _indirectArgumentOffsets, sizeof(u32) * gpu::SHADER_SET_COUNT_MAX);
		}

#if ENABLE_MULTI_INDIRECT_DRAW
		// multi draw indirect argument offset
		{
			for (u32 shaderSetIndex = 1; shaderSetIndex < gpu::SHADER_SET_COUNT_MAX; ++shaderSetIndex) {
				u32 prevShaderIndex = shaderSetIndex - 1;
				_multiDrawIndirectArgumentOffsets[shaderSetIndex] = _multiDrawIndirectArgumentOffsets[prevShaderIndex] + _multiDrawIndirectArgumentCounts[prevShaderIndex];
			}

			u32* mapIndirectArgumentOffsets = vramUpdater->enqueueUpdate<u32>(&_multiDrawIndirectArgumentOffsetBuffer, 0, gpu::SHADER_SET_COUNT_MAX);
			memcpy(mapIndirectArgumentOffsets, _multiDrawIndirectArgumentOffsets, sizeof(u32) * gpu::SHADER_SET_COUNT_MAX);
		}
#endif
	}

	SceneCullingInfo* cullingInfo = vramUpdater->enqueueUpdate<SceneCullingInfo>(&_sceneCullingConstantBuffer, 0, 1);
	cullingInfo->_meshInstanceCountMax = getMeshInstanceCountMax();
}

void Scene::processDeletion() {
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
	_meshInstanceBuffer.terminate();
	_lodMeshInstanceBuffer.terminate();
	_subMeshInstanceBuffer.terminate();
	_meshletInstanceInfoOffsetBuffer.terminate();
	_primitiveInstancingOffsetBuffer.terminate();
	_indirectArgumentOffsetBuffer.terminate();
	_sceneCullingConstantBuffer.terminate();

	LTN_ASSERT(_gpuMeshInstances.getInstanceCount() == 0);
	LTN_ASSERT(_gpuLodMeshInstances.getInstanceCount() == 0);
	LTN_ASSERT(_gpuSubMeshInstances.getInstanceCount() == 0);

	_gpuMeshInstances.terminate();
	_gpuLodMeshInstances.terminate();
	_gpuSubMeshInstances.terminate();

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocator->discardDescriptor(_meshInstanceHandles);
	allocator->discardDescriptor(_meshletInstanceInfoOffsetSrv);
	allocator->discardDescriptor(_indirectArgumentOffsetSrv);
	allocator->discardDescriptor(_cullingSceneConstantHandle);

#if ENABLE_MULTI_INDIRECT_DRAW
	_multiDrawIndirectArgumentOffsetBuffer.terminate();
	allocator->discardDescriptor(_multiDrawIndirectArgumentOffsetSrv);
#endif
}

void Scene::terminateDefaultResources() {
	_defaultShaderSet->requestToDelete();
	_defaultMaterial->requestToDelete();
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

	MaterialSystemImpl* materialSystem = MaterialSystemImpl::Get();
	u32 subMeshCount = meshInfo->_totalSubMeshCount;
	for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
		const SubMeshInfo* info = mesh->getSubMeshInfo(subMeshIndex);
		const gpu::SubMesh* subMesh = mesh->getGpuSubMesh(subMeshIndex);
		gpu::SubMeshInstance& subMeshInstance = _gpuSubMeshInstances[subMeshInstanceIndex + subMeshIndex];
		subMeshInstance._materialIndex = materialSystem->getMaterialIndex(_defaultMaterial);
		subMeshInstance._shaderSetIndex = materialSystem->getShaderSetIndex(static_cast<MaterialImpl*>(_defaultMaterial)->getShaderSet());
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

void MeshInstance::requestToDelete() {
	*_stateFlags |= MESH_INSTANCE_FLAG_REQUEST_DESTROY;
}

void MeshInstance::setWorldMatrix(const Matrix4& matrixWorld) {
	_matrixWorld = matrixWorld;
	*_updateFlags |= MESH_INSTANCE_UPDATE_WORLD_MATRIX;
}

void MeshInstance::setMaterialSlotIndex(Material* material, u32 slotIndex) {
	const gpu::SubMesh* subMeshes = _mesh->getGpuSubMesh();
	u32 subMeshCount = _mesh->getMeshInfo()->_totalSubMeshCount;
	for (u32 subMeshIndex = 0; subMeshIndex < subMeshCount; ++subMeshIndex) {
		if (subMeshes[subMeshIndex]._materialSlotIndex == slotIndex) {
			_subMeshInstances[subMeshIndex].setMaterial(material);
		}
	}
}

void MeshInstance::setMaterial(Material* material, u64 slotNameHash) {
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

		desc._sizeInByte = sizeof(u32) * INDIRECT_ARGUMENT_COUNTER_COUNT;
		_countBuffer.initialize(desc);
		_countBuffer.setDebugName("Indirect Argument Count");
	}

	// meshlet instance info buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(gpu::MeshletInstanceInfo) * Scene::SUB_MESH_INSTANCE_COUNT_MAX;
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._device = device;
		_meshletInstanceInfoBuffer.initialize(desc);
		_meshletInstanceInfoBuffer.setDebugName("Meshlet Instance Infos");

		desc._sizeInByte = sizeof(u32) * Scene::MESHLET_INSTANCE_INFO_COUNT_MAX;
		_meshletInstanceInfoCountBuffer.initialize(desc);
		_meshletInstanceInfoCountBuffer.setDebugName("Meshlet Instance Info Counts");
	}

	// primitive instancing buffers
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(gpu::MeshletInstanceInfo) * Scene::SUB_MESH_INSTANCE_COUNT_MAX;
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		desc._device = device;
		_primitiveInstancingInfoBuffer.initialize(desc);
		_primitiveInstancingInfoBuffer.setDebugName("Primitive Instancing Infos");

		desc._sizeInByte = sizeof(u32) * Scene::PRIMITIVE_INSTANCING_INFO_COUNT_MAX;
		_primitiveInstancingCountBuffer.initialize(desc);
		_primitiveInstancingCountBuffer.setDebugName("Primitive Instancing Counts");
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
	{
		GpuBufferDesc desc = {};
		desc._sizeInByte = sizeof(gpu::CullingResult) * BACK_BUFFER_COUNT;
		desc._initialState = RESOURCE_STATE_COPY_DEST;
		desc._heapType = HEAP_TYPE_READBACK;
		desc._device = device;
		_cullingResultReadbackBuffer.initialize(desc);
		_cullingResultReadbackBuffer.setDebugName("Culling Result Readback");
	}


	// passed meshlet info srv
	{
		_meshletInstanceInfoSrv = allocator->allocateDescriptors(1);

		ShaderResourceViewDesc desc = {};
		desc._format = FORMAT_UNKNOWN;
		desc._viewDimension = SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._numElements = Scene::SUB_MESH_INSTANCE_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(gpu::MeshletInstanceInfo);
		device->createShaderResourceView(_meshletInstanceInfoBuffer.getResource(), &desc, _meshletInstanceInfoSrv._cpuHandle);
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
		{
			_indirectArgumentUavHandle = allocator->allocateDescriptors(2);
			u32 incrimentSize = allocator->getIncrimentSize();
			CpuDescriptorHandle indirectArgumentHandle = _indirectArgumentUavHandle._cpuHandle;
			CpuDescriptorHandle countHandle = indirectArgumentHandle + incrimentSize;

			UnorderedAccessViewDesc desc = {};
			desc._format = FORMAT_UNKNOWN;
			desc._viewDimension = UAV_DIMENSION_BUFFER;
			desc._buffer._firstElement = 0;
			desc._buffer._numElements = INDIRECT_ARGUMENT_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::DispatchMeshIndirectArgument);
			device->createUnorderedAccessView(_indirectArgumentBuffer.getResource(), nullptr, &desc, indirectArgumentHandle);
			
			desc._buffer._numElements = INDIRECT_ARGUMENT_COUNTER_COUNT;
			desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
			desc._buffer._structureByteStride = 0;
			desc._format = FORMAT_R32_TYPELESS;
			device->createUnorderedAccessView(_countBuffer.getResource(), nullptr, &desc, countHandle);

			// カウントバッファをAPIの機能でクリアするためにCPU Only Descriptorを作成
			_countCpuUavHandle = cpuAllocator->allocateDescriptors(1);
			device->createUnorderedAccessView(_countBuffer.getResource(), nullptr, &desc, _countCpuUavHandle._cpuHandle);
		}

		// meshlet instance 
		{
			_meshletInstanceInfoUav = allocator->allocateDescriptors(1);
			UnorderedAccessViewDesc desc = {};
			desc._format = FORMAT_UNKNOWN;
			desc._viewDimension = UAV_DIMENSION_BUFFER;
			desc._buffer._firstElement = 0;
			desc._buffer._numElements = Scene::SUB_MESH_INSTANCE_COUNT_MAX;
			desc._buffer._structureByteStride = sizeof(gpu::MeshletInstanceInfo);
			device->createUnorderedAccessView(_meshletInstanceInfoBuffer.getResource(), nullptr, &desc, _meshletInstanceInfoUav._cpuHandle);
		}

		// meshlet instance count
		{
			_meshletInstanceInfoCountUav = allocator->allocateDescriptors(1);
			_meshletInstanceInfoCountCpuUav = cpuAllocator->allocateDescriptors(1);

			UnorderedAccessViewDesc desc = {};
			desc._format = FORMAT_R32_TYPELESS;
			desc._viewDimension = UAV_DIMENSION_BUFFER;
			desc._buffer._numElements = Scene::MESHLET_INSTANCE_INFO_COUNT_MAX;
			desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
			device->createUnorderedAccessView(_meshletInstanceInfoCountBuffer.getResource(), nullptr, &desc, _meshletInstanceInfoCountUav._cpuHandle);
			device->createUnorderedAccessView(_meshletInstanceInfoCountBuffer.getResource(), nullptr, &desc, _meshletInstanceInfoCountCpuUav._cpuHandle);
		}

		// primitive instancing count
		{
			_primitiveInstancingCountUav = allocator->allocateDescriptors(1);
			_primitiveInstancingCountCpuUav = cpuAllocator->allocateDescriptors(1);

			UnorderedAccessViewDesc desc = {};
			desc._format = FORMAT_R32_TYPELESS;
			desc._viewDimension = UAV_DIMENSION_BUFFER;
			desc._buffer._numElements = Scene::PRIMITIVE_INSTANCING_INFO_COUNT_MAX;
			desc._buffer._flags = BUFFER_UAV_FLAG_RAW;
			device->createUnorderedAccessView(_primitiveInstancingCountBuffer.getResource(), nullptr, &desc, _primitiveInstancingCountUav._cpuHandle);
			device->createUnorderedAccessView(_primitiveInstancingCountBuffer.getResource(), nullptr, &desc, _primitiveInstancingCountCpuUav._cpuHandle);
		}
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
		desc._buffer._numElements = Scene::MESH_INSTANCE_COUNT_MAX;
		desc._buffer._flags = BUFFER_SRV_FLAG_RAW;
		device->createShaderResourceView(_currentLodLevelBuffer.getResource(), &desc, _currentLodLevelSrv._cpuHandle);
	}

	// meshlet instance info count srv
	{
		_meshletInstanceInfoCountSrv = allocator->allocateDescriptors(1);
		ShaderResourceViewDesc desc = {};
		desc._format = FORMAT_R32_TYPELESS;
		desc._viewDimension = SRV_DIMENSION_BUFFER;
		desc._buffer._numElements = Scene::MESHLET_INSTANCE_INFO_COUNT_MAX;
		desc._buffer._flags = BUFFER_SRV_FLAG_RAW;
		device->createShaderResourceView(_meshletInstanceInfoCountBuffer.getResource(), &desc, _meshletInstanceInfoCountSrv._cpuHandle);
	}

	// build hiz textures
	{
		_hizDepthTextureUav = allocator->allocateDescriptors(gpu::HIERACHICAL_DEPTH_COUNT);
		_hizDepthTextureSrv = allocator->allocateDescriptors(gpu::HIERACHICAL_DEPTH_COUNT);

		u32 incrimentSize = allocator->getIncrimentSize();
		Application* app = ApplicationSystem::Get()->getApplication();
		u32 powScale = 1;
		for (u32 i = 0; i < gpu::HIERACHICAL_DEPTH_COUNT; ++i) {
			powScale *= 2;
			u32 screenWidth = app->getScreenWidth() / powScale;
			u32 screenHeight = app->getScreenHeight() / powScale;
			screenWidth += screenWidth % 2;
			screenHeight += screenHeight % 2;

			GpuTextureDesc desc = {};
			desc._device = device;
			desc._format = FORMAT_R32_FLOAT;
			desc._width = screenWidth;
			desc._height = screenHeight;
			desc._flags = RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			desc._initialState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			_hizDepthTextures[i].initialize(desc);

			char debugName[64];
			sprintf_s(debugName, "Hiz [%-2d] %-4d x %-4d", i, screenWidth, screenHeight);
			_hizDepthTextures[i].setDebugName(debugName);
		}

		for (u32 i = 0; i < gpu::HIERACHICAL_DEPTH_COUNT; ++i) {
			CpuDescriptorHandle uavHandle = _hizDepthTextureUav._cpuHandle + static_cast<u64>(incrimentSize) * i;
			UnorderedAccessViewDesc uavDesc = {};
			uavDesc._format = FORMAT_R32_FLOAT;
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
	}

	{
		GpuBuffer& buffer = _hizInfoConstantBuffer[1];
		GpuBufferDesc desc = {};
		desc._sizeInByte = GetConstantBufferAligned(sizeof(HizInfoConstant));
		desc._initialState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		desc._device = device;
		buffer.initialize(desc);
		buffer.setDebugName("Hiz Info Constant 1");
	}

	// build hiz cbv descriptors
	{
		for (u32 i = 0; i < LTN_COUNTOF(_hizInfoConstantBuffer); ++i) {
			_hizInfoConstantCbv[i] = allocator->allocateDescriptors(1);
			device->createConstantBufferView(_hizInfoConstantBuffer[i].getConstantBufferViewDesc(), _hizInfoConstantCbv[i]._cpuHandle);
		}
	}
}

void GraphicsView::terminate() {
	_indirectArgumentBuffer.terminate();
	_countBuffer.terminate();
	_cullingResultBuffer.terminate();
	_meshletInstanceInfoBuffer.terminate();
	_meshletInstanceInfoCountBuffer.terminate();
	_cullingViewConstantBuffer.terminate();
	_currentLodLevelBuffer.terminate();
	_cullingResultReadbackBuffer.terminate();
	_primitiveInstancingInfoBuffer.terminate();
	_primitiveInstancingCountBuffer.terminate();

	for (u32 i = 0; i < LTN_COUNTOF(_hizInfoConstantBuffer); ++i) {
		_hizInfoConstantBuffer[i].terminate();
	}

	for (u32 i = 0; i < gpu::HIERACHICAL_DEPTH_COUNT; ++i) {
		_hizDepthTextures[i].terminate();
	}

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocator->discardDescriptor(_indirectArgumentUavHandle);
	allocator->discardDescriptor(_cullingResultUavHandle);
	allocator->discardDescriptor(_cullingViewInfoCbvHandle);
	allocator->discardDescriptor(_currentLodLevelUav);
	allocator->discardDescriptor(_currentLodLevelSrv);
	allocator->discardDescriptor(_meshletInstanceInfoSrv);
	allocator->discardDescriptor(_meshletInstanceInfoCountSrv);
	allocator->discardDescriptor(_meshletInstanceInfoCountUav);
	allocator->discardDescriptor(_hizDepthTextureUav);
	allocator->discardDescriptor(_hizDepthTextureSrv);
	allocator->discardDescriptor(_meshletInstanceInfoUav);
	allocator->discardDescriptor(_primitiveInstancingCountUav);
	for (u32 i = 0; i < LTN_COUNTOF(_hizInfoConstantBuffer); ++i) {
		allocator->discardDescriptor(_hizInfoConstantCbv[i]);
	}

	DescriptorHeapAllocator* cpuAllocator = GraphicsSystemImpl::Get()->getSrvCbvUavCpuDescriptorAllocator();
	cpuAllocator->discardDescriptor(_countCpuUavHandle);
	cpuAllocator->discardDescriptor(_cullingResultCpuUavHandle);
	cpuAllocator->discardDescriptor(_meshletInstanceInfoCountCpuUav);
	cpuAllocator->discardDescriptor(_primitiveInstancingCountCpuUav);
}

void GraphicsView::update() {
	DescriptorHeapAllocator* descriptorHeapAllocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	u32 incrimentSize = descriptorHeapAllocater->getIncrimentSize();

	ResourceDesc firstDesc = _hizDepthTextures[0].getResourceDesc();
	f32 aspectRate = firstDesc._width / static_cast<f32>(firstDesc._height);

	DebugWindow::StartWindow("Depth Texture");
	DebugGui::Columns(1, nullptr, true);
	for (u32 i = 0; i < gpu::HIERACHICAL_DEPTH_COUNT; ++i) {
		ResourceDesc desc = _hizDepthTextures[i].getResourceDesc();
		DebugGui::Text("[%d] width:%-4d height:%-4d", i, desc._width, desc._height);
		DebugGui::Image(_hizDepthTextureSrv._gpuHandle + incrimentSize * i, Vector2(200 * aspectRate, 200),
			Vector2(0, 0), Vector2(1, 1), Color4::WHITE, Color4::BLACK, DebugGui::COLOR_CHANNEL_FILTER_R, Vector2(0.0f, 1), DebugGui::TEXTURE_SAMPLE_TYPE_POINT);
		DebugGui::NextColumn();
	}
	DebugGui::Columns(1);
	DebugWindow::End();

	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	CullingViewInfo* cullingInfo = vramUpdater->enqueueUpdate<CullingViewInfo>(&_cullingViewConstantBuffer, 0);
	*reinterpret_cast<u64*>(cullingInfo->_meshletInfoGpuAddress) = _meshletInstanceInfoBuffer.getGpuVirtualAddress();

	Application* app = ApplicationSystem::Get()->getApplication();
	{
		HizInfoConstant* mapConstant = vramUpdater->enqueueUpdate<HizInfoConstant>(&_hizInfoConstantBuffer[0], 0);
		mapConstant->_inputDepthWidth = app->getScreenWidth();
		mapConstant->_inputDepthHeight = app->getScreenHeight();
		mapConstant->_nearClip = _viewInfo->_nearClip;
		mapConstant->_farClip = _viewInfo->_farClip;
		mapConstant->_inputBitDepth = UINT32_MAX;
	}

	{
		ResourceDesc hizLevel3Desc = getHizTextureResourceDesc(3);
		HizInfoConstant* mapConstant = vramUpdater->enqueueUpdate<HizInfoConstant>(&_hizInfoConstantBuffer[1], 0);
		mapConstant->_inputDepthWidth = static_cast<u32>(hizLevel3Desc._width);
		mapConstant->_inputDepthHeight = static_cast<u32>(hizLevel3Desc._height);
		mapConstant->_nearClip = _viewInfo->_nearClip;
		mapConstant->_farClip = _viewInfo->_farClip;
		mapConstant->_inputBitDepth = UINT16_MAX;
	}
}

void GraphicsView::setComputeLodResource(CommandList* commandList) {
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_LOD_RESULT_LEVEL, _currentLodLevelUav._gpuHandle);
}

void GraphicsView::setGpuCullingResources(CommandList* commandList) {
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_INDIRECT_ARGUMENTS, _indirectArgumentUavHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_CULLING_VIEW_INFO, _cullingViewInfoCbvHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_LOD_LEVEL, _currentLodLevelSrv._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_CULLING_RESULT, _cullingResultUavHandle._gpuHandle);
	commandList->setComputeRootDescriptorTable(ROOT_PARAM_GPU_HIZ, _hizDepthTextureSrv._gpuHandle);
}

void GraphicsView::setHizResourcesPass0(CommandList* commandList) {
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
	ResourceTransitionBarrier indirectArgumentToUavBarriers[5] = {};
	indirectArgumentToUavBarriers[0] = _indirectArgumentBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	indirectArgumentToUavBarriers[1] = _countBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	indirectArgumentToUavBarriers[2] = _meshletInstanceInfoBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	indirectArgumentToUavBarriers[3] = _meshletInstanceInfoCountBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	indirectArgumentToUavBarriers[4] = _primitiveInstancingCountBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarriers(indirectArgumentToUavBarriers, LTN_COUNTOF(indirectArgumentToUavBarriers));
}

void GraphicsView::resourceBarriersHizTextureToUav(CommandList* commandList, u32 offset) {
	ResourceTransitionBarrier srvToUav[4] = {};
	for (u32 i = 0; i < LTN_COUNTOF(srvToUav); ++i) {
		srvToUav[i] = _hizDepthTextures[i + offset].getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	}
	commandList->transitionBarriers(srvToUav, LTN_COUNTOF(srvToUav));
}

void GraphicsView::resourceBarriersHizUavtoSrv(CommandList* commandList, u32 offset) {
	ResourceTransitionBarrier uavToSrv[4] = {};
	for (u32 i = 0; i < LTN_COUNTOF(uavToSrv); ++i) {
		uavToSrv[i] = _hizDepthTextures[i + offset].getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
	commandList->transitionBarriers(uavToSrv, LTN_COUNTOF(uavToSrv));
}

void GraphicsView::resourceBarriersHizSrvToTexture(CommandList* commandList) {
	ResourceTransitionBarrier srvToTexture[gpu::HIERACHICAL_DEPTH_COUNT] = {};
	for (u32 i = 0; i < gpu::HIERACHICAL_DEPTH_COUNT; ++i) {
		srvToTexture[i] = _hizDepthTextures[i].getAndUpdateTransitionBarrier(RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	commandList->transitionBarriers(srvToTexture, LTN_COUNTOF(srvToTexture));
}

void GraphicsView::resourceBarriersHizTextureToSrv(CommandList* commandList) {
	ResourceTransitionBarrier textureToSrv[gpu::HIERACHICAL_DEPTH_COUNT] = {};
	for (u32 i = 0; i < gpu::HIERACHICAL_DEPTH_COUNT; ++i) {
		textureToSrv[i] = _hizDepthTextures[i].getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
	commandList->transitionBarriers(textureToSrv, LTN_COUNTOF(textureToSrv));
}

void GraphicsView::resetResourceGpuCullingBarriers(CommandList* commandList) {
	// UAV から Indirect Argumentへ
	ResourceTransitionBarrier uavToIndirectArgumentBarriers[5] = {};
	uavToIndirectArgumentBarriers[0] = _indirectArgumentBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_INDIRECT_ARGUMENT);
	uavToIndirectArgumentBarriers[1] = _countBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_INDIRECT_ARGUMENT);
	uavToIndirectArgumentBarriers[2] = _meshletInstanceInfoBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	uavToIndirectArgumentBarriers[3] = _meshletInstanceInfoCountBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	uavToIndirectArgumentBarriers[4] = _primitiveInstancingCountBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->transitionBarriers(uavToIndirectArgumentBarriers, LTN_COUNTOF(uavToIndirectArgumentBarriers));
}

void GraphicsView::resourceBarriersBuildIndirectArgument(CommandList* commandList) {
	ResourceTransitionBarrier uavToIndirectArgumentBarriers[2] = {};
	uavToIndirectArgumentBarriers[0] = _indirectArgumentBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	uavToIndirectArgumentBarriers[1] = _countBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->transitionBarriers(uavToIndirectArgumentBarriers, LTN_COUNTOF(uavToIndirectArgumentBarriers));
}

void GraphicsView::resourceBarriersResetBuildIndirectArgument(CommandList* commandList) {
	ResourceTransitionBarrier uavToIndirectArgumentBarriers[2] = {};
	uavToIndirectArgumentBarriers[0] = _indirectArgumentBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_INDIRECT_ARGUMENT);
	uavToIndirectArgumentBarriers[1] = _countBuffer.getAndUpdateTransitionBarrier(RESOURCE_STATE_INDIRECT_ARGUMENT);
	commandList->transitionBarriers(uavToIndirectArgumentBarriers, LTN_COUNTOF(uavToIndirectArgumentBarriers));
}

// カウントバッファクリア
void GraphicsView::resetIndirectArgumentCountBuffers(CommandList* commandList) {
	u32 clearValues[4] = {};
	DescriptorHeapAllocator* allocater = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	u32 incrimentSize = allocater->getIncrimentSize();
	GpuDescriptorHandle gpuDescriptor = _indirectArgumentUavHandle._gpuHandle + incrimentSize;
	CpuDescriptorHandle cpuDescriptor = _countCpuUavHandle._cpuHandle;
	commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, _countBuffer.getResource(), clearValues, 0, nullptr);
}

void GraphicsView::resetMeshletInstanceInfoCountBuffers(CommandList* commandList) {
	u32 clearValues[4] = {};
	{
		GpuDescriptorHandle gpuDescriptor = _meshletInstanceInfoCountUav._gpuHandle;
		CpuDescriptorHandle cpuDescriptor = _meshletInstanceInfoCountCpuUav._cpuHandle;
		commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, _meshletInstanceInfoCountBuffer.getResource(), clearValues, 0, nullptr);
	}

	{
		GpuDescriptorHandle gpuDescriptor = _primitiveInstancingCountUav._gpuHandle;
		CpuDescriptorHandle cpuDescriptor = _primitiveInstancingCountCpuUav._cpuHandle;
		commandList->clearUnorderedAccessViewUint(gpuDescriptor, cpuDescriptor, _primitiveInstancingCountBuffer.getResource(), clearValues, 0, nullptr);
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
	u32 offset = frameIndex * sizeof(gpu::CullingResult);
	_cullingResultBuffer.transitionResource(commandList, RESOURCE_STATE_COPY_SOURCE);
	commandList->copyBufferRegion(_cullingResultReadbackBuffer.getResource(), offset, _cullingResultBuffer.getResource(), 0, sizeof(gpu::CullingResult));
	_cullingResultBuffer.transitionResource(commandList, RESOURCE_STATE_UNORDERED_ACCESS);

	MemoryRange range = { frameIndex, frameIndex + 1 };
	gpu::CullingResult* mapPtr = _cullingResultReadbackBuffer.map<gpu::CullingResult>(&range);
	memcpy(&_currentFrameCullingResultMapPtr, mapPtr, sizeof(gpu::CullingResult));
	_cullingResultReadbackBuffer.unmap();
}

void GraphicsView::setDrawResultDescriptorTable(CommandList* commandList) {
	commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_CULLING_RESULT, _cullingResultUavHandle._gpuHandle);
	commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_HIZ, _hizDepthTextureSrv._gpuHandle);
}

void GraphicsView::setDrawCurrentLodDescriptorTable(CommandList* commandList) {
	commandList->setGraphicsRootDescriptorTable(ROOT_DEFAULT_MESH_LOD_LEVEL, _currentLodLevelSrv._gpuHandle);
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