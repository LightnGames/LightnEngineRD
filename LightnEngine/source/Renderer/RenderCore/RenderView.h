#pragma once
#include <Core/Math.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>
namespace ltn {
namespace gpu {
	struct View {
		Float3 _cameraPosition;
		Float4x4 _viewMatrix;
		Float4x4 _projectionMatrix;
		Float4x4 _viewProjectionMatrix;
	};
}

class RenderViewScene {
public:
	void initialize();
	void terminate();
	void update();

	static RenderViewScene* Get();

private:
	GpuBuffer _viewConstantBuffer;
	DescriptorHandles _viewDescriptors;
};
}