#pragma once

#include <Core/Math.h>
#include <Core/LinerAllocator.h>
#include <Renderer/RenderCore/GpuBuffer.h>
#include <Renderer/RenderCore/DescriptorAllocator.h>

namespace ltn {
class GpuTexture;
struct LineInstance {
	Float3 _startPosition;
	Float3 _endPosition;
	Color4 _color;
};

struct BoxInstance {
	Float3x4 _matrixWorld;
	Color4 _color;
};

class DebugRenderer {
public:
	static const u32 LINE_INSTANCE_CPU_COUNT_MAX = 1024 * 16;
	static const u32 BOX_INSTANCE_CPU_COUNT_MAX = 1024 * 8;

	struct RenderDesc {
		rhi::CommandList* _commandList = nullptr;
		rhi::GpuDescriptorHandle _viewCbv;
		rhi::GpuDescriptorHandle _viewDepthSrv;
		GpuTexture* _viewDepthGpuTexture = nullptr;
	};

	void initialize();
	void update();
	void terminate();

	void render(const RenderDesc& desc);

	void drawLine(Vector3 startPosition, Vector3 endPosition, Color color = Color::Red());
	void drawBox(Matrix4 matrix, Color color = Color::Red());
	void drawAabb(Vector3 boundsMin, Vector3 boundsMax, Color color = Color::Red());
	void drawFrustum(Matrix4 view, Matrix4 projection, Color color = Color::Red());

	static DebugRenderer* Get();

private:
	LinerAllocater<LineInstance> _lineInstances;
	LinerAllocater<BoxInstance> _boxInstances;
	rhi::RootSignature _rootSignature;
	rhi::PipelineState _debugLinePipelineState;
	rhi::PipelineState _debugBoxPipelineState;
	GpuBuffer _lineInstanceGpuBuffer;
	GpuBuffer _boxInstanceGpuBuffer;
	DescriptorHandle _lineInstanceSrv;
	DescriptorHandle _boxInstanceSrv;
};
}