#include <MeshRenderer/impl/GlobalDistanceField.h>
#include <GfxCore/impl/GraphicsSystemImpl.h>
#include <MeshRenderer/impl/SceneImpl.h>
#include <DebugRenderer/DebugRendererSystem.h>

void GlobalDistanceField::initialize() {
	for (u32 i = 0; i < LAYER_COUNT_MAX; ++i) {
		_layers[i].initialize(i);
	}
}

void GlobalDistanceField::terminate() {
	for (u32 i = 0; i < LAYER_COUNT_MAX; ++i) {
		_layers[i].terminate();
	}
}

void GlobalDistanceField::update() {
	//for (u32 i = 0; i < LAYER_COUNT_MAX; ++i) {
	//	_layers[i].update();
	//}
}

void GlobalDistanceField::addMeshInstanceSdfGlobal(const AABB& worldBounds, u32 meshInstanceIndex) {
	for (u32 i = 0; i < LAYER_COUNT_MAX; ++i) {
		_layers[i].addMeshInstanceSdfGlobal(worldBounds, meshInstanceIndex);
	}
}

void GlobalDistanceField::removeMeshInstanceSdfGlobal(const AABB& worldBounds, u32 meshInstanceIndex) {
	for (u32 i = 0; i < LAYER_COUNT_MAX; ++i) {
		_layers[i].removeMeshInstanceSdfGlobal(worldBounds, meshInstanceIndex);
	}
}

void GlobalDistanceField::debugDrawGlobalSdfCells() const {
	for (u32 i = 0; i < LAYER_COUNT_MAX; ++i) {
		_layers[i].debugDrawGlobalSdfCells();
	}
}

void DistanceFieldLayer::initialize(u32 layerLevel) {
	_sdfGlobalMeshInstanceIndicesArray.initialize(SDF_GLOBAL_MESH_INDEX_ARRAY_COUNT_MAX);
	_cellSize = static_cast<f32>(layerLevel + 1) * SDF_GLOBAL_CELL_SIZE;
	_globalCellHalfExtent = SDF_GLOBAL_WIDTH * _cellSize * 0.5f;

	Device* device = GraphicsSystemImpl::Get()->getDevice();
	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();

	// buffer
	{
		GpuBufferDesc desc = {};
		desc._device = device;
		desc._sizeInByte = sizeof(u32) * Scene::MESH_INSTANCE_COUNT_MAX;
		desc._initialState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		_sdfGlobalMeshInstanceCountBuffer.initialize(desc);
		_sdfGlobalMeshInstanceCountBuffer.setDebugName("SDF Mesh Instance Count");

		_sdfGlobalMeshInstanceIndexBuffer.initialize(desc);
		_sdfGlobalMeshInstanceIndexBuffer.setDebugName("SDF Mesh Instance Index");

		_sdfGlobalMeshInstanceOffsetBuffer.initialize(desc);
		_sdfGlobalMeshInstanceOffsetBuffer.setDebugName("SDF Mesh Instance Offset");
	}

	// descriptor
	{
		_sdfGlobalMeshInstanceCountSrv = allocator->allocateDescriptors(1);
		_sdfGlobalMeshInstanceIndexSrv = allocator->allocateDescriptors(1);
		_sdfGlobalMeshInstanceOffsetSrv = allocator->allocateDescriptors(1);

		ShaderResourceViewDesc desc = {};
		desc._format = FORMAT_UNKNOWN;
		desc._viewDimension = SRV_DIMENSION_BUFFER;
		desc._buffer._firstElement = 0;
		desc._buffer._flags = BUFFER_SRV_FLAG_NONE;
		desc._buffer._numElements = Scene::MESH_INSTANCE_COUNT_MAX;
		desc._buffer._structureByteStride = sizeof(u32);
		device->createShaderResourceView(_sdfGlobalMeshInstanceIndexBuffer.getResource(), &desc, _sdfGlobalMeshInstanceIndexSrv._cpuHandle);
		device->createShaderResourceView(_sdfGlobalMeshInstanceCountBuffer.getResource(), &desc, _sdfGlobalMeshInstanceCountSrv._cpuHandle);
		device->createShaderResourceView(_sdfGlobalMeshInstanceOffsetBuffer.getResource(), &desc, _sdfGlobalMeshInstanceOffsetSrv._cpuHandle);
	}

	for (u32 i = 0; i < SDF_GLOBAL_CELL_COUNT; ++i) {
		_sdfGlobalOffsets[i] = gpu::INVALID_INDEX;
	}
}

void DistanceFieldLayer::terminate() {
	_sdfGlobalMeshInstanceIndexBuffer.terminate();
	_sdfGlobalMeshInstanceCountBuffer.terminate();
	_sdfGlobalMeshInstanceOffsetBuffer.terminate();

	LTN_ASSERT(_sdfGlobalMeshInstanceIndicesArray.getInstanceCount() == 0);

	_sdfGlobalMeshInstanceIndicesArray.terminate();

	for (u32 i = 0; i < SDF_GLOBAL_CELL_COUNT; ++i) {
		LTN_ASSERT(_sdfGlobalMeshInstanceCounts[i] == 0);
	}

	DescriptorHeapAllocator* allocator = GraphicsSystemImpl::Get()->getSrvCbvUavGpuDescriptorAllocator();
	allocator->discardDescriptor(_sdfGlobalMeshInstanceIndexSrv);
	allocator->discardDescriptor(_sdfGlobalMeshInstanceCountSrv);
	allocator->discardDescriptor(_sdfGlobalMeshInstanceOffsetSrv);

}

void DistanceFieldLayer::addMeshInstanceSdfGlobal(const AABB& worldBounds, u32 meshInstanceIndex) {
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	s32 globalSdfMin[3];
	globalSdfMin[0] = static_cast<s32>((worldBounds._min._x + _globalCellHalfExtent) / _cellSize);
	globalSdfMin[1] = static_cast<s32>((worldBounds._min._y + _globalCellHalfExtent) / _cellSize);
	globalSdfMin[2] = static_cast<s32>((worldBounds._min._z + _globalCellHalfExtent) / _cellSize);

	s32 globalSdfMax[3];
	globalSdfMax[0] = static_cast<s32>((worldBounds._max._x + _globalCellHalfExtent) / _cellSize) + 1;
	globalSdfMax[1] = static_cast<s32>((worldBounds._max._y + _globalCellHalfExtent) / _cellSize) + 1;
	globalSdfMax[2] = static_cast<s32>((worldBounds._max._z + _globalCellHalfExtent) / _cellSize) + 1;
	for (s32 z = globalSdfMin[2]; z < globalSdfMax[2]; ++z) {
		if (z < 0 || z >= SDF_GLOBAL_WIDTH) {
			continue;
		}
		u32 depthOffset = z * SDF_GLOBAL_WIDTH * SDF_GLOBAL_WIDTH;
		for (s32 y = globalSdfMin[1]; y < globalSdfMax[1]; ++y) {
			if (y < 0 || y >= SDF_GLOBAL_WIDTH) {
				continue;
			}
			u32 heightOffset = y * SDF_GLOBAL_WIDTH;
			for (s32 x = globalSdfMin[0]; x < globalSdfMax[0]; ++x) {
				if (x < 0 || x >= SDF_GLOBAL_WIDTH) {
					continue;
				}

				u32 offset = depthOffset + heightOffset + x;
				u32 currentSdfOffset = gpu::INVALID_INDEX;
				if (_sdfGlobalMeshInstanceCounts[offset] > 0) {
					currentSdfOffset = _sdfGlobalOffsets[offset];
					_sdfGlobalMeshInstanceIndicesArray.discard(currentSdfOffset, _sdfGlobalMeshInstanceCounts[offset]);
				}
				u32 currentCountIndex = _sdfGlobalMeshInstanceCounts[offset]++;
				_sdfGlobalOffsets[offset] = _sdfGlobalMeshInstanceIndicesArray.request(_sdfGlobalMeshInstanceCounts[offset]);

				if (currentSdfOffset != gpu::INVALID_INDEX) {
					if (currentCountIndex > 0) {
						u32 dstOffset = _sdfGlobalOffsets[offset];
						u32 srcOffset = currentSdfOffset;
						memcpy(&_sdfGlobalMeshInstanceIndices[dstOffset], &_sdfGlobalMeshInstanceIndices[srcOffset], sizeof(u32) * currentCountIndex);
					}
				}

				u32 indexOffset = _sdfGlobalOffsets[offset] + currentCountIndex;
				LTN_ASSERT(indexOffset < SDF_GLOBAL_MESH_INDEX_ARRAY_COUNT_MAX);
				_sdfGlobalMeshInstanceIndices[indexOffset] = meshInstanceIndex;

				u32 dstOffset = _sdfGlobalOffsets[offset];
				u32 copyCount = _sdfGlobalMeshInstanceCounts[offset];
				u32* mapSdfGlobalMeshIndices = vramUpdater->enqueueUpdate<u32>(&_sdfGlobalMeshInstanceIndexBuffer, sizeof(u32) * dstOffset, copyCount);
				u32* mapSdfGlobalMeshOffsets = vramUpdater->enqueueUpdate<u32>(&_sdfGlobalMeshInstanceOffsetBuffer, sizeof(u32) * offset);
				u32* mapSdfGlobalMeshCounts = vramUpdater->enqueueUpdate<u32>(&_sdfGlobalMeshInstanceCountBuffer, sizeof(u32) * offset);
				memcpy(mapSdfGlobalMeshIndices, &_sdfGlobalMeshInstanceIndices[dstOffset], sizeof(u32) * copyCount);
				memcpy(mapSdfGlobalMeshOffsets, &_sdfGlobalOffsets[offset], sizeof(u32));
				memcpy(mapSdfGlobalMeshCounts, &_sdfGlobalMeshInstanceCounts[offset], sizeof(u32));
			}
		}
	}
}

void DistanceFieldLayer::removeMeshInstanceSdfGlobal(const AABB& worldBounds, u32 meshInstanceIndex) {
	VramBufferUpdater* vramUpdater = GraphicsSystemImpl::Get()->getVramUpdater();
	s32 globalSdfMin[3];
	globalSdfMin[0] = static_cast<s32>((worldBounds._min._x + _globalCellHalfExtent) / _cellSize);
	globalSdfMin[1] = static_cast<s32>((worldBounds._min._y + _globalCellHalfExtent) / _cellSize);
	globalSdfMin[2] = static_cast<s32>((worldBounds._min._z + _globalCellHalfExtent) / _cellSize);

	s32 globalSdfMax[3];
	globalSdfMax[0] = static_cast<s32>((worldBounds._max._x + _globalCellHalfExtent) / _cellSize) + 1;
	globalSdfMax[1] = static_cast<s32>((worldBounds._max._y + _globalCellHalfExtent) / _cellSize) + 1;
	globalSdfMax[2] = static_cast<s32>((worldBounds._max._z + _globalCellHalfExtent) / _cellSize) + 1;
	for (s32 z = globalSdfMin[2]; z < globalSdfMax[2]; ++z) {
		if (z < 0 || z >= SDF_GLOBAL_WIDTH) {
			continue;
		}
		u32 depthOffset = z * SDF_GLOBAL_WIDTH * SDF_GLOBAL_WIDTH;
		for (s32 y = globalSdfMin[1]; y < globalSdfMax[1]; ++y) {
			if (y < 0 || y >= SDF_GLOBAL_WIDTH) {
				continue;
			}
			u32 heightOffset = y * SDF_GLOBAL_WIDTH;
			for (s32 x = globalSdfMin[0]; x < globalSdfMax[0]; ++x) {
				if (x < 0 || x >= SDF_GLOBAL_WIDTH) {
					continue;
				}
				u32 offset = depthOffset + heightOffset + x;

				LTN_ASSERT(_sdfGlobalMeshInstanceCounts[offset] > 0);
				u32 currentSdfOffset = _sdfGlobalOffsets[offset];
				u32 prevCount = _sdfGlobalMeshInstanceCounts[offset];
				u32 currentCount = --_sdfGlobalMeshInstanceCounts[offset];
				_sdfGlobalMeshInstanceIndicesArray.discard(currentSdfOffset, prevCount);

				if (currentCount > 0) {
					_sdfGlobalOffsets[offset] = _sdfGlobalMeshInstanceIndicesArray.request(currentCount);
				}
				else {
					_sdfGlobalOffsets[offset] = gpu::INVALID_INDEX;
				}

				u32* mapSdfGlobalMeshOffsets = vramUpdater->enqueueUpdate<u32>(&_sdfGlobalMeshInstanceOffsetBuffer, sizeof(u32) * offset);
				u32* mapSdfGlobalMeshCounts = vramUpdater->enqueueUpdate<u32>(&_sdfGlobalMeshInstanceCountBuffer, sizeof(u32) * offset);
				memcpy(mapSdfGlobalMeshOffsets, &_sdfGlobalOffsets[offset], sizeof(u32));
				memcpy(mapSdfGlobalMeshCounts, &_sdfGlobalMeshInstanceCounts[offset], sizeof(u32));

				if (currentCount > 0) {
					u32 dstOffset = _sdfGlobalOffsets[offset];
					u32 srcOffset = currentSdfOffset;
					u32 findIndex = gpu::INVALID_INDEX;
					for (u32 i = 0; i < prevCount; ++i) {
						if (_sdfGlobalMeshInstanceIndices[srcOffset + i] == meshInstanceIndex) {
							findIndex = i;
							break;
						}
					}

					LTN_ASSERT(findIndex != gpu::INVALID_INDEX);

					if (findIndex > 0) {
						memcpy(&_sdfGlobalMeshInstanceIndices[dstOffset], &_sdfGlobalMeshInstanceIndices[srcOffset], sizeof(u32) * findIndex);
					}

					u32 backCopyCount = currentCount - findIndex;
					if (backCopyCount > 0) {
						memcpy(&_sdfGlobalMeshInstanceIndices[dstOffset + findIndex], &_sdfGlobalMeshInstanceIndices[srcOffset + findIndex + 1], sizeof(u32) * backCopyCount);
					}

					u32* mapSdfGlobalMeshIndices = vramUpdater->enqueueUpdate<u32>(&_sdfGlobalMeshInstanceIndexBuffer, sizeof(u32) * dstOffset, currentCount);
					memcpy(mapSdfGlobalMeshIndices, &_sdfGlobalMeshInstanceIndices[dstOffset], sizeof(u32) * currentCount);
				}
			}
		}
	}
}

void DistanceFieldLayer::debugDrawGlobalSdfCells() const {
	Vector3 cellOffset(0.5f);
	for (s32 z = 0; z < SDF_GLOBAL_WIDTH; ++z) {
		u32 depthOffset = z * SDF_GLOBAL_WIDTH * SDF_GLOBAL_WIDTH;
		for (s32 y = 0; y < SDF_GLOBAL_WIDTH; ++y) {
			u32 heightOffset = y * SDF_GLOBAL_WIDTH;
			for (s32 x = 0; x < SDF_GLOBAL_WIDTH; ++x) {
				u32 offset = depthOffset + heightOffset + x;
				u32 count = _sdfGlobalMeshInstanceCounts[offset];
				if (count == 0) {
					continue;
				}
				Vector3 cellIndex(static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(z));
				Vector3 start = cellIndex * _cellSize + cellOffset - Vector3(_globalCellHalfExtent);
				Vector3 end = cellIndex * _cellSize - cellOffset + Vector3(_cellSize) - Vector3(_globalCellHalfExtent);
				f32 alpha = static_cast<f32>(count) / 100.0f;
				DebugRendererSystem::Get()->drawAabb(start, end, Color4(alpha, _layerLevel * 0.2f, 0.2f, 1.0f));
			}
		}
	}
}
