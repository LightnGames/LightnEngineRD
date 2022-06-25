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
	static constexpr u32 VERTEX_COUNT_MAX = 1024 * 128;
	static constexpr u32 INDEX_COUNT_MAX = 1024 * 256;

	void initialize();
	void terminate();
	void update();

	rhi::VertexBufferView getPositionVertexBufferView() const;
	rhi::VertexBufferView getNormalTangentVertexBufferView() const;
	rhi::VertexBufferView getTexcoordVertexBufferView() const;
	rhi::IndexBufferView getIndexBufferView() const;
	rhi::GpuDescriptorHandle getGeometryGlobalOffsetGpuSrv() const { return _geometryGlobalOffsetSrv._gpuHandle; }

	static GeometryResourceManager* Get();

private:
	void loadLodMesh(const MeshGeometry* mesh, u32 beginLodLevel, u32 endLodLevel);

private:
	rhi::VertexBufferView _vertexBufferView;
	rhi::IndexBufferView _indexBufferView;
	GpuBuffer _vertexPositionGpuBuffer;
	GpuBuffer _vertexNormalTangentGpuBuffer;
	GpuBuffer _vertexTexcoordGpuBuffer;
	GpuBuffer _indexGpuBuffer;
	GpuBuffer _geometryGlobalOffsetGpuBuffer;
	VirtualArray _vertexPositionAllocations;
	VirtualArray _indexAllocations;
	DescriptorHandle _geometryGlobalOffsetSrv;

	struct LodStreamRange {
		u16 _beginLevel = 0;
		u16 _endLevel = 0;
	};

	// num lod meshes
	GeometryAllocationInfo* _geometryAllocationInfos = nullptr;

	// num meshes
	LodStreamRange* _streamedLodRanges = nullptr;
};
}