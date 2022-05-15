#pragma once
#include <RendererScene/Mesh.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Core/Math.h>
namespace ltn {
using VertexPosition = Float3;

struct GpuMesh {
	u32 _stateFlags = 0;
	u32 _lodMeshOffset = 0;
	u32 _lodMeshCount = 0;
	u32 _streamedLodLevel = 0;
};

struct GpuLodMesh {
	u32 _vertexOffset = 0;
	u32 _vertexIndexOffset = 0;
	u32 _primitiveOffset = 0;
	u32 _subMeshOffset = 0;
	u32 _subMeshCount = 0;
};

struct GpuSubMesh {
	u32 _indexCount = 0;
	u32 _indexOffset = 0;
};

class GpuMeshResourceManager {
public:
	static constexpr u32 VERTEX_COUNT_MAX = 1024 * 1024;

	void initialize();
	void terminate();
	void update();

private:
	GpuBuffer _meshGpuBuffer;
	GpuBuffer _lodMeshGpuBuffer;
	GpuBuffer _subMeshGpuBuffer;

	GpuBuffer _vertexPositionGpuBuffer;
	GpuBuffer _indexBuffer;
};
}