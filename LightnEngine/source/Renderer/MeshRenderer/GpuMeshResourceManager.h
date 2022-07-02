#pragma once
#include <RendererScene/MeshGeometry.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
#include <Core/Math.h>
#include <Core/VirturalArray.h>

namespace ltn {
// 以下の定義は Compute Shader で利用するものと同一にします
namespace gpu {
struct Mesh {
	u32 _lodMeshOffset = 0;
	u32 _lodMeshCount = 0;
};

struct LodMesh {
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
	void initialize();
	void terminate();
	void update();

	rhi::GpuDescriptorHandle getMeshGpuSrv() const { return _meshSrv._firstHandle._gpuHandle; }

	static GpuMeshResourceManager* Get();

private:
	GpuBuffer _meshGpuBuffer;
	GpuBuffer _lodMeshGpuBuffer;
	GpuBuffer _subMeshGpuBuffer;
	DescriptorHandles _meshSrv;
};
}