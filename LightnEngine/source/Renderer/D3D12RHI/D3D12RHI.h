#pragma once
#include <Core/Utility.h>
#include <Renderer/RHI/RhiBase.h>
#include <d3d12.h>
#include <dxgi1_4.h>

namespace ltn {
namespace rhi {
struct ShaderBlob :public ShaderBlobBase {
	void initialize(const char* filePath) override;
	void terminate() override;
	ShaderByteCode getShaderByteCode() const override;

	ID3DBlob* _blob = nullptr;
};

struct HardwareFactory :public HardwareFactoryBase {
	void initialize(const HardwareFactoryDesc& desc) override;
	void terminate() override;

	IDXGIFactory4* _factory = nullptr;
};

struct HardwareAdapter :public HardwareAdapterBase {
	void initialize(const HardwareAdapterDesc& desc) override;
	void terminate() override;
	QueryVideoMemoryInfo queryVideoMemoryInfo() override;

	IDXGIAdapter3* _adapter = nullptr;
};

struct Device :public DeviceBase {
	void initialize(const DeviceDesc& desc) override;
	void terminate() override;
	u32 getDescriptorHandleIncrementSize(DescriptorHeapType type) const override;
	void createRenderTargetView(Resource* resource, const RenderTargetViewDesc* desc, CpuDescriptorHandle destDescriptor) override;
	void createDepthStencilView(Resource* resource, CpuDescriptorHandle destDescriptor) override;
	void createCommittedResource(HeapType heapType, HeapFlags heapFlags, const ResourceDesc& desc,
		ResourceStates initialResourceState, const ClearValue* optimizedClearValue, Resource* dstResource) override;
	void createConstantBufferView(const ConstantBufferViewDesc& desc, CpuDescriptorHandle destDescriptor) override;
	void createShaderResourceView(Resource* resource, const ShaderResourceViewDesc* desc, CpuDescriptorHandle destDescriptor) override;
	void createUnorderedAccessView(Resource* resource, Resource* counterResource, const UnorderedAccessViewDesc* desc, CpuDescriptorHandle destDescriptor) override;
	void getCopyableFootprints(const ResourceDesc* resourceDesc, u32 firstSubresource, u32 numSubresources, u64 baseOffset, PlacedSubresourceFootprint* layouts, u32* numRows, u64* rowSizeInBytes, u64* totalBytes) override;
	u8 getFormatPlaneCount(Format format) override;
	ResourceAllocationInfo getResourceAllocationInfo(u32 visibleMask, u32 numResourceDescs, const ResourceDesc* resourceDescs) override;

	ID3D12Device2* _device = nullptr;
};

struct SwapChain :public SwapChainBase {
	void initialize(const SwapChainDesc& desc) override;
	void terminate() override;

	u32 getCurrentBackBufferIndex() override;
	void present(u32 syncInterval, u32 flags) override;
	void getBackBuffer(Resource& resource, u32 index) override;

	IDXGISwapChain3* _swapChain = nullptr;
};

struct CommandQueue :public CommandQueueBase {
	void initialize(const CommandQueueDesc& desc) override;
	void terminate() override;
	void executeCommandLists(u32 count, CommandList** commandLists) override;
	void getTimestampFrequency(u64* frequency) override;

	bool isFenceComplete(u64 fenceValue);
	void waitForIdle();
	void waitForFence(u64 fenceValue);
	u64 incrimentFence();
	u64 getCompletedValue() const;
	u64 getNextFenceValue() const { return _nextFenceValue; }

	ID3D12CommandQueue* _commandQueue = nullptr;
	ID3D12Fence* _fence = nullptr;
	HANDLE _fenceEvent = nullptr;
	u64 _nextFenceValue = 0;
	u64 _lastCompletedFenceValue = 0;
};

struct DescriptorHeap :public DescriptorHeapBase {
	void initialize(const DescriptorHeapDesc& desc) override;
	void terminate() override;
	CpuDescriptorHandle getCPUDescriptorHandleForHeapStart() const override;
	GpuDescriptorHandle getGPUDescriptorHandleForHeapStart() const override;
	void setName(const char* name) override;

	ID3D12DescriptorHeap* _descriptorHeap = nullptr;
};

struct CommandList :public CommandListBase {
	void initialize(const CommandListDesc& desc) override;
	void terminate() override;

	void reset() override;
	void transitionBarrierSimple(Resource* resource, ResourceStates currentState, ResourceStates nextState) override;
	void transitionBarriers(ResourceTransitionBarrier* barriers, u32 count) override;
	void aliasingBarriers(ResourceAliasingBarrier* barriers, u32 count) override;
	void copyBufferRegion(Resource* dstBuffer, u64 dstOffset, Resource* srcBuffer, u64 srcOffset, u64 numBytes) override;
	void copyTextureRegion(const TextureCopyLocation* dst, u32 dstX, u32 dstY, u32 dstZ, const TextureCopyLocation* src, const Box* srcBox) override;
	void copyResource(Resource* dstResource, Resource* srcResource) override;
	void setDescriptorHeaps(u32 count, DescriptorHeap** descriptorHeaps) override;
	void setViewports(u32 count, const ViewPort* viewPorts) override;
	void setScissorRects(u32 count, const Rect* scissorRects) override;
	void setRenderTargets(u32 count, CpuDescriptorHandle* rtvHandles, CpuDescriptorHandle* dsvHandle) override;
	void setRenderTargets(u32 count, CpuDescriptorHandle rtvHandles, CpuDescriptorHandle* dsvHandle) override;
	void clearRenderTargetView(CpuDescriptorHandle rtvHandle, f32 clearColor[4]) override;
	void clearDepthStencilView(CpuDescriptorHandle depthStencilView, ClearFlags clearFlags, f32 depth, u8 stencil, u32 numRects, const Rect* rects) override;
	void setPipelineState(PipelineState* pipelineState) override;
	void setGraphicsRootSignature(RootSignature* rootSignature) override;
	void setComputeRootSignature(RootSignature* rootSignature) override;
	void setPrimitiveTopology(PrimitiveTopology primitiveTopology) override;
	void setGraphicsRootDescriptorTable(u32 rootParameterIndex, GpuDescriptorHandle baseDescriptor) override;
	void setComputeRootDescriptorTable(u32 rootParameterIndex, GpuDescriptorHandle baseDescriptor) override;
	void setGraphicsRoot32BitConstants(u32 rootParameterIndex, u32 num32BitValuesToSet, const void* srcData, u32 destOffsetIn32BitValues) override;
	void setComputeRoot32BitConstants(u32 rootParameterIndex, u32 num32BitValuesToSet, const void* srcData, u32 destOffsetIn32BitValues) override;
	void drawInstanced(u32 vertexCountPerInstance, u32 instanceCount, u32 startVertexLocation, u32 startInstanceLocation) override;
	void drawIndexedInstanced(u32 indexCountPerInstance, u32 instanceCount, u32 startIndexLocation, s32 baseVertexLocation, u32 startInstanceLocation) override;
	void dispatch(u32 threadGroupCountX, u32 threadGroupCountY, u32 threadGroupCountZ) override;
	void executeIndirect(CommandSignature* commandSignature, u32 maxCommandCount, Resource* argumentBuffer, u64 argumentBufferOffset, Resource* countBuffer, u64 countBufferOffset) override;
	void clearUnorderedAccessViewUint(GpuDescriptorHandle viewGPUHandleInCurrentHeap, CpuDescriptorHandle viewCPUHandle, Resource* resource, const u32 values[4], u32 numRects, const Rect* rects) override;
	void setVertexBuffers(u32 startSlot, u32 numViews, const VertexBufferView* views) override;
	void setIndexBuffer(const IndexBufferView* view) override;
	void endQuery(QueryHeap* queryHeap, QueryType type, u32 index) override;
	void resolveQueryData(QueryHeap* queryHeap, QueryType type, u32 startIndex, u32 numQueries, Resource* destinationBuffer, u64 alignedDestinationBufferOffset) override;

	u64 _fenceValue = INVAILD_FENCE_VALUE;
	ID3D12CommandAllocator* _allocator = nullptr;
	ID3D12GraphicsCommandList* _commandList = nullptr;
};

struct CommandSignature :public CommandSignatureBase {
	void initialize(const CommandSignatureDesc& desc) override;
	void terminate() override;
	void setName(const char* name) override;

	ID3D12CommandSignature* _commandSignature = nullptr;
};

struct Resource :public ResourceBase {
	void initialize(ID3D12Resource* resource) { _resource = resource; }
	void terminate() override;
	void* map(const MemoryRange* range) override;
	void unmap(const MemoryRange* range) override;
	u64 getGpuVirtualAddress() const override;
	void setName(const char* name) override;
	virtual u64 getUniqueMarker() const override { return u64(_resource); }

	ID3D12Resource* _resource = nullptr;
};

struct PipelineState :public PipelineStateBase {
	void iniaitlize(const GraphicsPipelineStateDesc& desc) override;
	void iniaitlize(const ComputePipelineStateDesc& desc) override;
	void terminate() override;
	void setName(const char* name) override;

	ID3D12PipelineState* _pipelineState = nullptr;
};

struct RootSignature :public RootSignatureBase {
	void iniaitlize(const RootSignatureDesc& desc) override;
	void terminate() override;
	void setName(const char* name) override;

	ID3D12RootSignature* _rootSignature = nullptr;
};

struct QueryHeap :public QueryHeapBase {
	void initialize(const QueryHeapDesc& desc) override;
	void terminate() override;

	ID3D12QueryHeap* _queryHeap = nullptr;
};

namespace {
D3D12_CPU_DESCRIPTOR_HANDLE toD3d12(CpuDescriptorHandle handle) {
	D3D12_CPU_DESCRIPTOR_HANDLE result = { handle._ptr };
	return result;
}

D3D12_GPU_DESCRIPTOR_HANDLE toD3d12(GpuDescriptorHandle handle) {
	D3D12_GPU_DESCRIPTOR_HANDLE result = { handle._ptr };
	return result;
}

DXGI_FORMAT toD3d12(Format format) {
	return static_cast<DXGI_FORMAT>(format);
}

D3D12_RESOURCE_STATES toD3d12(ResourceStates states) {
	return static_cast<D3D12_RESOURCE_STATES>(states);
}

D3D12_RESOURCE_DESC toD3d12(ResourceDesc desc) {
	D3D12_RESOURCE_DESC result = {};
	memcpy(&result, &desc, sizeof(result));
	return result;
}

const D3D12_CLEAR_VALUE* toD3d12(const ClearValue* value) {
	return reinterpret_cast<const D3D12_CLEAR_VALUE*>(value);
}
}
}
}