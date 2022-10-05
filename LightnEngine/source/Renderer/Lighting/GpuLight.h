#pragma once
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
namespace gpu {
struct DirectionalLight {
	f32 _intensity;
	Float3 _color;
	Float3 _direction;
};
}
class GpuLightScene {
public:
	void initialize();
	void terminate();
	void update();

	rhi::GpuDescriptorHandle getDirectionalLightGpuSrv() const { return _directionalLightSrv._gpuHandle; }

	static GpuLightScene* Get();
private:
	GpuBuffer _directionalLightGpuBuffer;
	DescriptorHandle _directionalLightSrv;
};
}