#pragma once
#include <MeshRenderer/MeshRendererSystem.h>
#include <MeshRenderer/GpuStruct.h>
#include <MeshRenderer/impl/SceneImpl.h>
#include <MeshRenderer/impl/MeshResourceManager.h>
#include <MeshRenderer/impl/MeshRenderer.h>

struct MeshRenderDesc {
	CommandList* _commandList = nullptr;
};

class MeshRendererSystemImpl :public MeshRendererSystem {
public:
	virtual void initialize() override;
	virtual void terminate() override;
	virtual void update() override;
	virtual void render(CommandList* commandList, ViewInfo* viewInfo) override;
	virtual void processDeletion() override;

	virtual Mesh* allocateMesh(const MeshDesc& desc) override;
	virtual Mesh* createMesh(const MeshDesc& desc) override;
	virtual MeshInstance* createMeshInstance(const MeshInstanceDesc& desc) override;
	virtual Mesh* findMesh(u64 filePathHash) override;

	static MeshRendererSystemImpl* Get();
private:
#if ENABLE_MESH_SHADER
	void renderMeshShader(CommandList* commandList, ViewInfo* viewInfo);
#endif
#if ENABLE_MULTI_INDIRECT_DRAW
	void renderMultiIndirect(CommandList* commandList, ViewInfo* viewInfo);
#endif
#if ENABLE_CLASSIC_VERTEX
	void renderClassicVertex(CommandList* commandList, const ViewInfo* viewInfo);
#endif
private:
	void computeLod(CommandList* commandList, ViewInfo* viewInfo);
	void depthPrePassCulling(CommandList* commandList, ViewInfo* viewInfo, PipelineState* pipelineState);
	void buildIndirectArgument(CommandList* commandList);
	void mainCulling(CommandList* commandList, ViewInfo* viewInfo, PipelineState* pipelineState);
	void buildHiz(CommandList* commandList, ViewInfo* viewInfo);
	void mainCulling(CommandList* commandList, ViewInfo* viewInfo);
	void setFixedDebugView(CommandList * commandList, ViewInfo* viewInfo);

private:
	bool _visible = true;
	Scene _scene;
	MeshResourceManager _resourceManager;
	PipelineState* _gpuCullingPassPipelineState = nullptr;
	PipelineState* _gpuOcclusionCullingPipelineState = nullptr;
	RootSignature* _gpuOcclusionCullingRootSignature = nullptr;
	PipelineState* _gpuCullingPipelineState = nullptr;
	RootSignature* _gpuCullingRootSignature = nullptr;
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

	s32 _initializedFixedView = BACK_BUFFER_COUNT;
	GpuBuffer _debugFixedViewConstantBuffer;
	DescriptorHandle _debugFixedViewConstantHandle;
	GpuBuffer _sceneCullingConstantBuffer;
	DescriptorHandle _cullingSceneConstantHandle;
	GraphicsView _view;

	enum GeometoryType {
		GEOMETORY_MODE_MESH_SHADER = 0,
		GEOMETORY_MODE_MULTI_INDIRECT,
		GEOMETORY_MODE_CLASSIC_VERTEX,
	};

	enum DebugPrimitiveType {
		DEBUG_PRIMITIVE_TYPE_DEFAULT = 0,
		DEBUG_PRIMITIVE_TYPE_MESHLET,
		DEBUG_PRIMITIVE_TYPE_LODLEVEL,
		DEBUG_PRIMITIVE_TYPE_OCCLUSION,
		DEBUG_PRIMITIVE_TYPE_DEPTH,
		DEBUG_PRIMITIVE_TYPE_TEXCOORDS,
	};

	enum CullingDebugMenuType {
		CULLING_DEBUG_TYPE_NONE = 0,
		CULLING_DEBUG_TYPE_FIXED_VIEW = 1 << 0,
		CULLING_DEBUG_TYPE_PASS_MESH_CULLING = 1 << 1,
		CULLING_DEBUG_TYPE_PASS_MESHLET_CULLING = 1 << 2
	};

	void setDebugCullingFlag(u8 type, bool flag) {
		_cullingDebugType = flag ? _cullingDebugType | type : _cullingDebugType & ~type;
	}

	bool _debugDrawMeshletBounds = false;
	GeometoryType _geometoryType;
	DebugPrimitiveType _debugPrimitiveType;
	u8 _cullingDebugType = CULLING_DEBUG_TYPE_NONE;
	Matrix4 _debugFixedViewMatrix;
	Matrix4 _debugFixedProjectionMatrix;
};