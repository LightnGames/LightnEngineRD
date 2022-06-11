#pragma once
#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Core/Math.h>
#include <Core/VirturalArray.h>

namespace ltn {
class Mesh;
using VertexPosition = Float3;
using VertexNormalTangent = u32;
using VertexTexCoords = u32;
using VertexIndex = u32;
struct MeshGeometryInfo {
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
	rhi::IndexBufferView getIndexBufferView() const;

	static GeometryResourceManager* Get();

private:
	void loadLodMesh(const Mesh* mesh, u32 beginLodLevel, u32 endLodLevel);

private:
	rhi::VertexBufferView _vertexBufferView;
	rhi::IndexBufferView _indexBufferView;
	GpuBuffer _vertexPositionGpuBuffer;
	GpuBuffer _indexGpuBuffer;
	VirtualArray _vertexPositionAllocations;
	VirtualArray _indexAllocations;

	MeshGeometryInfo* _meshInfos = nullptr;
};
}