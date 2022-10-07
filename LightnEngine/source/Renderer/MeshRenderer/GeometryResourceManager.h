#pragma once
#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Core/Math.h>
#include <Core/VirturalArray.h>

namespace ltn {
namespace gpu {
struct GeometryGlobalOffsetInfo {
	u32 _vertexOffset;
	u32 _indexOffset;
};
}
class MeshGeometry;
using VertexPosition = Float3;
using VertexNormalTangent = u32;
using VertexTexCoord = u32;
using VertexIndex = u32;
struct GeometryAllocationInfo {
	VirtualArray::AllocationInfo _vertexAllocationInfo;
	VirtualArray::AllocationInfo _indexAllocationInfo;
};

class GeometryResourceManager {
public:
	static constexpr u32 VERTEX_COUNT_MAX = 1024 * 256;
	static constexpr u32 INDEX_COUNT_MAX = 1024 * 512;

	struct MeshLodStreamRange {
		u16 _beginLevel = 0xffff;
		u16 _endLevel = 0;
	};

	void initialize();
	void terminate();
	void update();

	void loadLodMesh(const MeshGeometry* mesh, u32 beginLodLevel, u32 endLodLevel);
	void unloadLodMesh(const MeshGeometry* mesh, u32 beginLodLevel, u32 endLodLevel);
	void updateMeshLodStreamRange(u32 meshIndex, u32 beginLodLevel, u32 endLodLevel);
	const MeshLodStreamRange* getMeshLodStreamRanges() const {return _meshStreamLodRanges; }

	rhi::VertexBufferView getPositionVertexBufferView() const;
	rhi::VertexBufferView getNormalTangentVertexBufferView() const;
	rhi::VertexBufferView getTexcoordVertexBufferView() const;
	rhi::IndexBufferView getIndexBufferView() const;
	GpuBuffer* getGeometryGlobalOffsetGpuBuffer() { return &_geometryGlobalOffsetGpuBuffer; }
	GpuBuffer* getPositionVertexGpuBuffer() { return &_vertexPositionGpuBuffer; }
	GpuBuffer* getNormalTangentVertexGpuBuffer() { return &_vertexNormalTangentGpuBuffer; }
	GpuBuffer* getTexcoordVertexGpuBuffer() { return &_vertexTexcoordGpuBuffer; }
	GpuBuffer* getIndexGpuBuffer() { return &_indexGpuBuffer; }

	rhi::GpuDescriptorHandle getGeometryGlobalOffsetGpuSrv() const { return _geometryGlobalOffsetSrv._gpuHandle; }
	rhi::GpuDescriptorHandle getMeshLodStreamRangeGpuSrv() const { return _meshLodStreamRangeSrv._gpuHandle; }
	rhi::GpuDescriptorHandle getVertexResourceGpuSrv() const { return _vertexResourceSrv._firstHandle._gpuHandle; }

	static GeometryResourceManager* Get();

private:
	rhi::VertexBufferView _vertexBufferView;
	rhi::IndexBufferView _indexBufferView;
	GpuBuffer _vertexPositionGpuBuffer;
	GpuBuffer _vertexNormalTangentGpuBuffer;
	GpuBuffer _vertexTexcoordGpuBuffer;
	GpuBuffer _indexGpuBuffer;
	GpuBuffer _geometryGlobalOffsetGpuBuffer;
	GpuBuffer _meshLodStreamRangeGpuBuffer;
	VirtualArray _vertexPositionAllocations;
	VirtualArray _indexAllocations;
	DescriptorHandle _geometryGlobalOffsetSrv;
	DescriptorHandle _meshLodStreamRangeSrv;
	DescriptorHandles _vertexResourceSrv;

	// num lod meshes
	GeometryAllocationInfo* _geometryAllocationInfos = nullptr;

	// num meshes
	MeshLodStreamRange* _meshStreamLodRanges = nullptr;
};
}