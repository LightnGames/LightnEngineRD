#pragma once
#include <RendererScene/Mesh.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Core/Math.h>
#include <Core/VirturalArray.h>
namespace ltn {
using VertexPosition = Float3;

// 以下の定義は Compute Shader で利用するものと同一にします
namespace gpu {
struct Mesh {
	u32 _stateFlags = 0;
	u32 _lodMeshOffset = 0;
	u32 _lodMeshCount = 0;
	u32 _streamedLodLevel = 0;
};

struct LodMesh {
	u32 _vertexOffset = 0;
	u32 _vertexIndexOffset = 0;
	u32 _primitiveOffset = 0;
	u32 _subMeshOffset = 0;
	u32 _subMeshCount = 0;
};

struct SubMesh {
	u32 _indexCount = 0;
	u32 _indexOffset = 0;
};
}
// ---------------------------------------------------------

// メッシュ情報の Gpu データを管理するクラス
class GpuMeshResourceManager {
public:
	static constexpr u32 VERTEX_COUNT_MAX = 1024 * 1024;
	static constexpr u32 INDEX_COUNT_MAX = 1024 * 1024 * 4;

	void initialize();
	void terminate();
	void update();

	static GpuMeshResourceManager* Get();

private:
	GpuBuffer _meshGpuBuffer;
	GpuBuffer _lodMeshGpuBuffer;
	GpuBuffer _subMeshGpuBuffer;

	GpuBuffer _vertexPositionGpuBuffer;
	GpuBuffer _indexBuffer;
};
}