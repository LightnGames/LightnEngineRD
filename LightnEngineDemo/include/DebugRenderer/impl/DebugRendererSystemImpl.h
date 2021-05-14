#pragma once
#include <DebugRenderer/DebugRendererSystem.h>
#include <GfxCore/impl/GpuResourceImpl.h>
struct CommandList;
struct PipelineState;
struct RootSignature;
struct ViewInfo;

struct LineInstance{
	Float3 _startPosition;
	Float3 _endPosition;
	Color4 _color;
};

class LTN_DEBUG_RENDERER_API DebugRendererSystemImpl : public DebugRendererSystem {
public:
	static const u32 LINE_INSTANCE_COUNT_MAX = 1024 * 1024 * 8;
	void initialize();
	void update();
	void processDeletion();
	void terminate();
	void resetGpuCounter(CommandList* commandList);
	void render(CommandList* commandList, const ViewInfo* viewInfo);
	DescriptorHandle getLineGpuUavHandle() const { return _lineInstanceGpuUav; }

	virtual void drawLine(Vector3 startPosition, Vector3 endPosition, Color4 color) override;
	virtual void drawAabb(Vector3 boundsMin, Vector3 boundsMax, Color4 color = Color4::RED) override;
	virtual void drawFrustum(Matrix4 view, Matrix4 projection, Color4 color) override;

	static DebugRendererSystemImpl* Get();

private:
	LinerAllocater<LineInstance> _lineInstances;
	PipelineState* _pipelineState = nullptr;
	RootSignature* _rootSignature = nullptr;
	GpuBuffer _lineInstanceBuffer;
	GpuBuffer _lineInstanceGpuBuffer;
	GpuBuffer _lineInstanceIndirectArgumentBuffer;
	DescriptorHandle _lineInstanceHandle;
	DescriptorHandle _lineInstanceGpuUav;
	DescriptorHandle _lineInstanceGpuSrv;
	CommandSignature* _commandSignature = nullptr;
};