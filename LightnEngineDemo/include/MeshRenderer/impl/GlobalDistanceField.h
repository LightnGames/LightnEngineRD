#pragma once
#include <GfxCore/impl/GpuResourceImpl.h>

static constexpr u32 LAYER_COUNT_MAX = 2;
static constexpr s32 SDF_GLOBAL_WIDTH = 8;
static constexpr s32 SDF_GLOBAL_HALF_WIDTH = SDF_GLOBAL_WIDTH / 2;
static constexpr s32 SDF_GLOBAL_CELL_COUNT = SDF_GLOBAL_WIDTH * SDF_GLOBAL_WIDTH * SDF_GLOBAL_WIDTH;
static constexpr f32 SDF_GLOBAL_CELL_SIZE = 6.4f;
//static constexpr f32 SDF_GLOBAL_CELL_HALF_SIZE = SDF_GLOBAL_CELL_SIZE * SDF_GLOBAL_WIDTH * 0.5f;
static constexpr s32 SDF_GLOBAL_MESH_INDEX_ARRAY_COUNT_MAX = 1024 * 64;

class DistanceFieldLayer {
public:
	void initialize(u32 layerLevel);
	void terminate();

	void addMeshInstanceSdfGlobal(const AABB& worldBounds, u32 meshInstanceIndex);
	void removeMeshInstanceSdfGlobal(const AABB& worldBounds, u32 meshInstanceIndex);

	void debugDrawGlobalSdfCells() const;

	GpuDescriptorHandle getSdfGlobalMeshInstanceCountSrv() const { return _sdfGlobalMeshInstanceCountSrv._gpuHandle; }
	GpuDescriptorHandle getSdfGlobalMeshInstanceIndexSrv() const { return _sdfGlobalMeshInstanceIndexSrv._gpuHandle; }
	GpuDescriptorHandle getSdfGlobalMeshInstanceOffsetSrv() const { return _sdfGlobalMeshInstanceOffsetSrv._gpuHandle; }

private:
	u32 _layerLevel = 0;
	f32 _cellSize = 0.0f;
	f32 _globalCellHalfExtent = 0.0f;
	u32 _sdfGlobalOffsets[SDF_GLOBAL_CELL_COUNT] = {};
	u32 _sdfGlobalMeshInstanceCounts[SDF_GLOBAL_CELL_COUNT] = {};
	u32 _sdfGlobalMeshInstanceIndices[SDF_GLOBAL_MESH_INDEX_ARRAY_COUNT_MAX] = {};
	MultiDynamicQueueBlockManager _sdfGlobalMeshInstanceIndicesArray;

	GpuBuffer _sdfGlobalMeshInstanceOffsetBuffer;
	GpuBuffer _sdfGlobalMeshInstanceIndexBuffer;
	GpuBuffer _sdfGlobalMeshInstanceCountBuffer;
	DescriptorHandle _sdfGlobalMeshInstanceOffsetSrv;
	DescriptorHandle _sdfGlobalMeshInstanceIndexSrv;
	DescriptorHandle _sdfGlobalMeshInstanceCountSrv;
};

class GlobalDistanceField {
public:
	void initialize();
	void terminate();
	void update();

	void addMeshInstanceSdfGlobal(const AABB& worldBounds, u32 meshInstanceIndex);
	void removeMeshInstanceSdfGlobal(const AABB& worldBounds, u32 meshInstanceIndex);

	GpuDescriptorHandle getSdfGlobalMeshInstanceCountSrv() const { return _layers[0].getSdfGlobalMeshInstanceCountSrv(); }
	GpuDescriptorHandle getSdfGlobalMeshInstanceIndexSrv() const { return _layers[0].getSdfGlobalMeshInstanceIndexSrv(); }
	GpuDescriptorHandle getSdfGlobalMeshInstanceOffsetSrv() const { return _layers[0].getSdfGlobalMeshInstanceOffsetSrv(); }

	void debugDrawGlobalSdfCells() const;

private:
	DistanceFieldLayer _layers[LAYER_COUNT_MAX] = {};
};