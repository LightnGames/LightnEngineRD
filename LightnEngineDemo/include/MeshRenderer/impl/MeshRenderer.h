#pragma once
#include <Core/System.h>
#include <GfxCore/impl/GpuResourceImpl.h>
#include <GfxCore/impl/DescriptorHeap.h>

struct ViewInfo;
class IndirectArgumentResource;
class VramShaderSet;
class InstancingResource;
class GpuCullingResource;

struct RenderContext {
	CommandList* _commandList = nullptr;
	ViewInfo* _viewInfo = nullptr;
	VramShaderSet* _vramShaderSets = nullptr;
	IndirectArgumentResource* _indirectArgumentResource = nullptr;
	InstancingResource* _primitiveInstancingResource = nullptr;
	GpuCullingResource* _gpuCullingResource = nullptr;
	PipelineStateGroup** _primInstancingPipelineStates = nullptr;
	PipelineStateGroup** _pipelineStates = nullptr;
	GpuDescriptorHandle _meshInstanceHandle;
	GpuDescriptorHandle _meshHandle;
	GpuDescriptorHandle _vertexResourceDescriptors;
	GpuDescriptorHandle _debugFixedViewCbv;
	bool _collectResult = false;
};

struct MultiIndirectRenderContext {
	CommandList* _commandList = nullptr;
	ViewInfo* _viewInfo = nullptr;
	GpuCullingResource* _gpuCullingResource = nullptr;
	IndirectArgumentResource* _indirectArgumentResource = nullptr;
	VramShaderSet* _vramShaderSets = nullptr;
	PipelineStateGroup** _pipelineStates = nullptr;
	VertexBufferView* _vertexBufferViews = nullptr;
	IndexBufferView* _indexBufferView = nullptr;
	u32 _numVertexBufferView = 0;
	const u32* _indirectArgmentOffsets = nullptr;
	const u32* _indirectArgmentCounts = nullptr;
	GpuDescriptorHandle _meshInstanceHandle;
};

struct ComputeLodContext {
	CommandList* _commandList = nullptr;
	ViewInfo* _viewInfo = nullptr;
	GpuCullingResource* _gpuCullingResource = nullptr;
	u32 _meshInstanceCountMax = 0;
	GpuDescriptorHandle _meshInstanceHandle;
	GpuDescriptorHandle _meshHandle;
	GpuDescriptorHandle _sceneConstantCbv;
};

struct GpuCullingContext {
	CommandList* _commandList = nullptr;
	ViewInfo* _viewInfo = nullptr;
	GpuCullingResource* _gpuCullingResource = nullptr;
	IndirectArgumentResource* _indirectArgumentResource = nullptr;
	IndirectArgumentResource* _instancingIndirectArgumentResource = nullptr;
	u32 _meshInstanceCountMax = 0;
	GpuDescriptorHandle _indirectArgumentOffsetSrv;
	GpuDescriptorHandle _sceneConstantCbv;
	GpuDescriptorHandle _meshInstanceHandle;
	GpuDescriptorHandle _meshHandle;
	GpuDescriptorHandle _subMeshDrawInfoHandle;
	GpuDescriptorHandle _meshletInstanceInfoOffsetSrv;
	GpuDescriptorHandle _meshletInstanceInfoCountUav;
	GpuDescriptorHandle _meshletInstanceInfoUav;
	GpuDescriptorHandle _cullingViewCbv;
	GpuDescriptorHandle _materialInstanceIndexSrv;
	InstancingResource* _primitiveInstancingResource = nullptr;
	const char* _scopeName = nullptr;
};

struct BuildIndirectArgumentContext {
	CommandList* _commandList = nullptr;
	IndirectArgumentResource* _indirectArgumentResource = nullptr;
	GpuDescriptorHandle _meshletInstanceOffsetSrv;
	GpuDescriptorHandle _meshletInstanceCountSrv;
	GpuDescriptorHandle _indirectArgumentUav;
	GpuDescriptorHandle _subMeshSrv;
};

struct BuildHizContext {
	CommandList* _commandList = nullptr;
	ViewInfo* _viewInfo = nullptr;
	GpuCullingResource* _gpuCullingResource = nullptr;
};

class MeshRenderer {
public:
	void initialize();
	void terminate();
	void render(RenderContext& context);

	void computeLod(ComputeLodContext& context);
	void depthPrePassCulling(GpuCullingContext& context);
	void mainCulling(GpuCullingContext& context);
	void buildIndirectArgument(BuildIndirectArgumentContext& context);
	void buildHiz(BuildHizContext& context);

#if ENABLE_MULTI_INDIRECT_DRAW
	void multiDrawRender(MultiIndirectRenderContext& context);
	void multiDrawDepthPrePassCulling(GpuCullingContext& context);
	void multiDrawMainCulling(GpuCullingContext& context);
#endif

private:
	void gpuCulling(GpuCullingContext& context, PipelineState* pipelineState);

private:
	RootSignature* _gpuCullingRootSignature = nullptr;
	PipelineState* _gpuCullingPassPipelineState = nullptr;
	PipelineState* _gpuOcclusionCullingPipelineState = nullptr;
	PipelineState* _gpuCullingPipelineState = nullptr;

	PipelineState* _computeLodPipelineState = nullptr;
	RootSignature* _computeLodRootSignature = nullptr;
	PipelineState* _buildHizPipelineState = nullptr;
	RootSignature* _buildHizRootSignature = nullptr;
	PipelineState* _debugMeshletBoundsPipelineState = nullptr;
	RootSignature* _debugMeshletBoundsRootSignature = nullptr;
	PipelineState* _buildIndirectArgumentPipelineState = nullptr;
	RootSignature* _buildIndirectArgumentRootSignature = nullptr;
#if ENABLE_MULTI_INDIRECT_DRAW
	PipelineState* _multiDrawCullingPipelineState = nullptr;
	PipelineState* _multiDrawOcclusionCullingPipelineState = nullptr;
#endif
};